#include <stdio.h>
#include <stdlib.h>
#include "Serial.h"
#include "jtag.h"
#include "LPC214x.h"

/*                                                              
	Theory, user calls Enqueue(jtag_transaction)

	The jtag transaction is converted into a set of commands,
	the commands are only the bits, TMS/TDI and TDO is shifted into the command.

	When converting a jtag_transaction to a list of commands, each TDO bit needs will
	have to be picked up from the command buffer.

                                                                
*/


#define BIT_TDI 	(1 << 16)
#define BIT_TMS 	(1 << 17)
#define BIT_TCK 	(1 << 18)
#define BIT_TDO 	(1 << 19)
#define BIT_nTRST	(1 << 20)


jtag::jtag() : 
	m_commandHead(0), 
	m_commandTail(0), 
	m_trans_head(NULL), 
	m_trans_tail(NULL), 
	m_Timer(NULL), 
	m_bTimerRunning(false), 
	m_TCKPulse(0), 
	m_trans_cur(NULL)
{
	m_scan_n_length = 5; // Default for ARM9
}


jtag::~jtag()
{

}

void
jtag::initialize()
{
	/*
	NC - 3v
	gnd
	P0.16 - TDI   - O
	P0.17 - TMS   - O
	NC    - NC
	P0.18 - TCK   - O
	P0.19 - TDO   - I
	P0.20 - nTRST - O
	*/

	// Setup GPIO, we'll be using P0.16-20
	
	PINSEL1 &= ~(1023);
	IOCLR0 = (BIT_TDI | BIT_TMS | BIT_TCK | BIT_nTRST);
	IODIR0 |= ((1 << 16) | (1 << 17) | (1 << 18) | (1 << 20));
	IODIR0 &= ~(1 << 19);
	FIO0MASK = 0;

	// Enable Fast io for IO port 0.
	SCS |= 1;

	//return;

	m_Timer = new Timer(0, 60, 1, new jtagCB(this));
	
	nTRSR(false);
	nTRSR(true);
	
	// Reset TAP controller.
	jtag_transaction res(jtag_transaction::eTestReset, NULL, NULL, 0, false);
	Enqueue(&res);

	while(!res.done());
}

#define USEFIO

void	
jtag::TMS(bool b)
{
	if (b) {
#ifndef USEFIO
		IOSET0 = BIT_TMS;
#else
		FIO0SET = BIT_TMS;
#endif
	} else {
#ifndef USEFIO
		IOCLR0 = BIT_TMS;
#else
		FIO0CLR = BIT_TMS;
#endif
	}
}

void	
jtag::TCK(bool b)
{
	if (b) {
#ifndef USEFIO
		IOSET0 = BIT_TCK;
#else
		FIO0SET = BIT_TCK;
#endif
	} else {
#ifndef USEFIO
		IOCLR0 = BIT_TCK;
#else
		FIO0CLR = BIT_TCK;
#endif
	}
}

void	
jtag::TDI(bool b)
{
	if (b) {
#ifndef USEFIO
		IOSET0 = BIT_TDI;
#else
		FIO0SET = BIT_TDI;
#endif
	} else {
#ifndef USEFIO
		IOCLR0 = BIT_TDI;
#else
		FIO0CLR = BIT_TDI;
#endif
	}
}

int		
jtag::TDO()
{
#ifndef USEFIO
	return (FIO0PIN & BIT_TDO)?1:0;
#else
	return (IOPIN0 & BIT_TDO)?1:0;
#endif

}

void
jtag::nTRSR(bool b)
{
	if (b) {
		IOSET0 = BIT_nTRST;
	} else {
		IOCLR0 = BIT_nTRST;
	}
}

void 
jtag::pulseTCK()
{
	TCK(true);
	for (int i = 0; i < 100000; ++i) {
		TCK(true);
	}
	TCK(false);
}

int
jtag::idcode()
{
	int retval;
	EnterInstruction(INSTR_IDCODE);
	ShiftDRBits(NULL, (char *)&retval, 32);
	return retval;
}

void
jtag::EnterInstruction(int n)
{
	//TMS(1);
	//pulseTCK(); // Select DR.

	TMS(1);
	pulseTCK(); // Select IR

	TMS(0);
	pulseTCK(); // Capture IR.

	pulseTCK(); // Shift IR.
	for (int i = 0; i < 5; i++) {
		TDI(n & (1 << i));
		if (i == 4) {
			TMS(1); // Go to exit1
		}
		pulseTCK();
	}

	pulseTCK(); // Update IR
	TMS(1);
	pulseTCK(); // Run-Test/Idle


}

void	
jtag::ShiftDRBits(char *pIn, char *pOut, int nbits)
{
	int ibyte = 0;
	int ibit = 0;

	//TMS(1);
	//pulseTCK(); // Select DR.

	TMS(0);
	pulseTCK(); // Capture DR.
	pulseTCK(); // Shift DR.

	if (pOut) pOut[ibyte] = 0;

	for (int i = 0; i < nbits; i++) {
		if (ibit == 8) {
			ibyte++;
			ibit = 0;
			if (pOut) pOut[ibyte] = 0;
		}
		int val = TDO();
		if (pOut) pOut[ibyte] |= (val << ibit);
		if (pIn) TDI(pIn[ibyte] & (1 << ibit));

		ibit++;

		if (i == (nbits -1)) {
			TMS(1);
		}
		pulseTCK();

	}

	pulseTCK(); // Update DR
	TMS(1);
	pulseTCK(); // Run-Test/Idle

}

void
jtag::SelectScanChain(int chain)
{
	char	data[1];

	EnterInstruction(INSTR_SCAN_N);
	data[0] = chain;

	ShiftDRBits(data, NULL, m_scan_n_length);
}

void
jtag::SetScanNLength(int length)
{
	m_scan_n_length = length;
}


void 
jtag::Enqueue(jtag_transaction *trans)
{
	bool bStart = false;

//-- Protect here!
	if (m_bTimerRunning) {
		m_Timer->Disable();
	}
	trans->m_bDone = false;
	trans->m_tdo = 0;
	if (NULL == m_trans_head) {
		m_trans_head = trans;
		m_trans_tail = trans;
		bStart = true;
	} else {
		m_trans_tail->m_next = trans;
		m_trans_tail = trans;
	}

	m_trans_tail->m_next = NULL;
	if (m_bTimerRunning) {
		m_Timer->Enable();
	}

//-- To here !

	if (bStart && !m_bTimerRunning) {
		//UART_puts("Starting timer\n\r");
		StartTimer();
	} else {
		//UART_puts("NOT Starting timer\n\r");

	}
}


void
jtag::SetSignals(jtag_transaction *trans)
{

}

void 
jtag::EnqueueCommand(unsigned char pins)
{
	unsigned char cmd = m_commandBuffer[m_commandTail >> 1];

	int shift = ((m_commandTail & 1)?4:0);
	unsigned char mask = 0x0f << shift;

	cmd &= ~mask;
	cmd |= ((pins << shift) & mask);

	m_commandBuffer[m_commandTail >> 1] = cmd;
	m_commandTail++;
}

unsigned char
jtag::DequeueCommand()
{
	int shift = ((m_commandHead & 1)?4:0);
	unsigned char cmd = m_commandBuffer[m_commandHead >> 1];

	m_commandHead++;
	return (cmd >> shift) & 0x0f;
}

void
jtag::StartTimer()
{
	m_Timer->Enable();
	m_bTimerRunning = true;
}

void
jtag::StopTimer()
{
	m_Timer->Disable();
	m_bTimerRunning = false;

}

void
jtag::Tick()
{
	// If the command buffer is empty
	if (CommandTail() == CommandHead() && m_TCKPulse == 0) {
		if (NULL != m_trans_cur) {
			m_trans_cur->m_bDone = true;
			TDI(false);
		}

		jtag_transaction *trans = Dequeue();
		m_trans_cur = trans;

		if (NULL != trans) {
			ProcessTransaction(trans);
		} else {
			StopTimer();
			m_bTimerRunning = false;
		}
	}
	if (NULL == m_trans_cur) {
		return;
	}

	switch (m_TCKPulse) {
		case 0: 
		{
			TCK(false);
			char cmd = DequeueCommand();
			if (cmd & CMD_BIT_TDO) {
				CommandPushTDO(TDO());
			}
			TMS(cmd & CMD_BIT_TMS);
			TDI(cmd & CMD_BIT_TDI);
			m_TCKPulse++;
		}
		break;
		case 1:
			TCK(true);
			m_TCKPulse = 0;
			break;
		case 2:
			TCK(false);
			m_TCKPulse = 0;
			break;
		default:
			m_TCKPulse = 0;
	}
}


jtag_transaction *
jtag::Dequeue()
{

	// --- Protect here!
	jtag_transaction *trans = m_trans_head;

	if (NULL != trans) {
		m_trans_head = trans->m_next;
	}

	// See if it's the last one
	if (m_trans_tail == trans) {
		m_trans_tail = NULL;
	}

	return trans;
}

void
jtag::ProcessTransaction(jtag_transaction *trans)
{

	// Reset command buffer
	CommandClear();

	// We're always starting out on SelectDR
	switch (trans->type()) {
		case jtag_transaction::eShiftDR:
			//SelectDR
			EnqueueCommand(0); // No TMS, and no TDI
			// Capture DR
			EnqueueCommand(0); // No TMS, and no TDI
			// Shift DR
			break;
		case jtag_transaction::eEnterInstruction:
			// Select DR
			EnqueueCommand(CMD_BIT_TMS);
			// Select IR
			EnqueueCommand(0); // No TMS, and no TDI
			// Capture IR
			EnqueueCommand(0); // No TMS, and no TDI
			// Shift IR
			break;
		case jtag_transaction::eTestReset:
			// 5 clocks with TMS high should always get you to the reset state.
			EnqueueCommand(CMD_BIT_TMS);
			EnqueueCommand(CMD_BIT_TMS);
			EnqueueCommand(CMD_BIT_TMS);
			EnqueueCommand(CMD_BIT_TMS);
			EnqueueCommand(CMD_BIT_TMS);
			EnqueueCommand(0);
			EnqueueCommand(CMD_BIT_TMS);
			return;
		case jtag_transaction::eRTI:
			// Not supported atm.
			break;
		case jtag_transaction::eReset:
			nTRSR(false);
			for (int i = 0; i < 1000000; ++i) {
				nTRSR(false);
			}
			nTRSR(true);
			EnqueueCommand(0);
			// RTI
			EnqueueCommand(CMD_BIT_TMS);
			// Select-DR
			return;
	}


	if (trans->bits() > 0) {
		for (int i = 0,n = trans->bits(); i < n; ++i) {
			bool bit = trans->getoutbit(i);
			if (i == (n - 1)) {
				EnqueueCommand(CMD_BIT_TMS | (bit?CMD_BIT_TDI:0) | CMD_BIT_TDO);
			} else {
				EnqueueCommand((bit?CMD_BIT_TDI:0) | CMD_BIT_TDO);
			}
		}
	}

	// We're at Exit-1 now.
	EnqueueCommand(CMD_BIT_TMS);
	// Update-IR/DR

	if (trans->rti()) {
		EnqueueCommand(0);
		// RTI
		EnqueueCommand(CMD_BIT_TMS);
		// Select-DR
	} else {
		EnqueueCommand(CMD_BIT_TMS);
		// Select-DR
	}

}

int
jtag::CommandHead()
{
	return m_commandHead;
}

int
jtag::CommandTail()
{
	return m_commandTail;
}

void
jtag::CommandPushTDO(int tdo)
{
	/*
	unsigned char cmd = m_commandBuffer[CommandHead() >> 1];
	int shift = ((CommandHead() & 1)?4:0) + CMD_SHIFT_TDO;
	cmd |= ((tdo>0?1:0) << shift);
	m_commandBuffer[CommandHead() >> 1] = cmd;
	*/


	if (NULL != m_trans_cur->m_outdata) {
		int bit = m_trans_cur->m_tdo;
		unsigned char byte = m_trans_cur->m_outdata[bit >> 3];

		byte &= ~(1 << (bit & 0x07));
		byte |= (tdo << (bit & 0x07));
		m_trans_cur->m_outdata[bit >> 3] = byte;
		m_trans_cur->m_tdo++;
	}
}

void
jtag::CommandClear()
{
	m_commandHead = 0;
	m_commandTail = 0;
}
