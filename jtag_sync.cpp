/*
	LPCJTAG, a GDB remote JTAG connector,

	Copyright (C) 2008 Bjorn Bosell (mithris@misantrop.org)

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright
	   notice, this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.
	3. The name of the author may not be used to endorse or promote products
	   derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, 
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
	NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdlib.h>
#include "Serial.h"
#include "jtag.h"
#include "LPC214x.h"
#include "jtag_asm.h"


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
	/*
	IOCLR0 = (BIT_TDI | BIT_TMS | BIT_TCK | BIT_nTRST);
	IODIR0 |= ((1 << 16) | (1 << 17) | (1 << 18) | (1 << 20));
	IODIR0 &= ~(1 << 19);
	FIO0MASK = 0;
*/
	// Enable fast IO on IOport 0.
	SCS |= 1;
	FIO0CLR = (BIT_TDI | BIT_TMS | BIT_TCK | BIT_nTRST);
	FIO0DIR |= ((1 << 16) | (1 << 17) | (1 << 18) | (1 << 20));
	FIO0DIR &= ~(1 << 19);

	
	nTRSR(false);
	nTRSR(true);


	// Reset TAP controller.
	jtag_transaction res(jtag_transaction::eTestReset, NULL, NULL, 0, false);
	Enqueue(&res);

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
	return (IOPIN0 & BIT_TDO)?1:0;
#else
	return (FIO0PIN & BIT_TDO)?1:0;
#endif

}

void
jtag::nTRSR(bool b)
{
	if (b) {
#ifndef USEFIO
		IOSET0 = BIT_nTRST;
#else
		FIO0SET = BIT_nTRST;
#endif
	} else {
#ifndef USEFIO
		IOCLR0 = BIT_nTRST;
#else
		FIO0CLR = BIT_nTRST;
#endif
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


void 
jtag::Enqueue(jtag_transaction *trans)
{
	ProcessTransaction(trans);
}


void
jtag::ProcessTransaction(jtag_transaction *trans)
{
	// We're always starting out on SelectDR
	switch (trans->type()) {
		case jtag_transaction::eShiftDR:
			ShiftDR_asm(trans->m_indata, trans->m_outdata, trans->rti()?1:0,  trans->bits());
			trans->setDone(true);
			return;
		case jtag_transaction::eEnterInstruction:
			EnterInsn_asm(trans->m_indata, trans->m_outdata, trans->rti()?1:0,  trans->bits());
			trans->setDone(true);
			return;
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
		int obit = 0;
		for (int i = 0,n = trans->bits(); i < n; ++i) {

			bool bit = trans->getoutbit(i);

			if (NULL != trans->m_outdata) {
				//int bit = trans->m_tdo;
				unsigned char byte = trans->m_outdata[obit >> 3];

				byte &= ~(1 << (obit & 0x07));
				byte |= (TDO() << (obit & 0x07));
				trans->m_outdata[obit >> 3] = byte;
				obit++;
			}

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

	trans->setDone(true);
}

void 
jtag::EnqueueCommand(unsigned char cmd)
{
	TCK(false);
	TMS(cmd & CMD_BIT_TMS);
	TDI(cmd & CMD_BIT_TDI);
	TCK(true);
	TCK(false);
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



