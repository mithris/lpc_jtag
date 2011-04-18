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
#include "JTAG_arm9tdmi.h"
#include "ARM.h"
#include "jtag.h"
#include "Serial.h"
#include "string.h"
#include "debug_out.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* I *think* this needs to be properly documented

	When halting the CPU with high vectors turned on, and then continuing again,
	you may end up with an exception which would read to one fucked up address.
	Which in turn will whack the system badly.

	This is my theory.


*/

JTAG_arm9tdmi::JTAG_arm9tdmi()
{
	m_TTB = 0xffffffff;

}
//###########################################################################

JTAG_arm9tdmi::~JTAG_arm9tdmi()
{
}
//###########################################################################

unsigned long
JTAG_arm9tdmi::Execute(unsigned long insn, unsigned long data, bool speed)
{
	unsigned int in[4];
	unsigned int out[4];

	CreateSC1Packet(in, insn, data, speed);

	jtag_transaction *t = getTrans();
	*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)in, (char *)out, 67, true);

	m_device->Enqueue(t);
	while(!t->done());

	relTrans(t);
	//m_device->ShiftDRBits((char *)in, (char *)out, 67);

	return out[0];
}
//###########################################################################


void
JTAG_arm9tdmi::EnterInstruction(int insn)
{
	jtag_transaction *t = getTrans();

	*t = jtag_transaction(jtag_transaction::eEnterInstruction, (char *)&insn, NULL, 4, insn==INSTR_RESTART?true:false);

	m_device->Enqueue(t);

	while(!t->done());
	relTrans(t);

}
//###########################################################################

void
JTAG_arm9tdmi::SelectScanChain(int chain)
{
	EnterInstruction(INSTR_SCAN_N);

	jtag_transaction *t = getTrans();
	*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)&chain, NULL, 5, true);
	m_device->Enqueue(t);

	while(!t->done());
	relTrans(t);
}
//###########################################################################


void
JTAG_arm9tdmi::ReadRegs()
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);


	Execute(STM_ALL_REGS, 0, false);
	Execute(NOP, 0, false);
	Execute(NOP, 0, false); // Apparently, only these three instructions add to the PC, add 4 also from the breakpoint.

	for (int i = 0; i < 16; i++) {
		unsigned long val = Execute(NOP, 0, false);
		m_registers[i].value = val;
		//UART_putHex(val);
		//UART_puts("\n\r");
	}
	Execute(MRS_R1_CPSR, 0, false); 
	Execute(STR_R1_R0, 0, false);
	Execute(NOP, 0, false);
	Execute(NOP, 0, false);
	m_registers[16].value = Execute(NOP, 0, false);

	Execute(MRS_R1_SPSR, 0, false);
	Execute(STR_R1_R0, 0, false);
	Execute(NOP, 0, false);
	Execute(NOP, 0, false);
	m_registers[17].value = Execute(NOP, 0, false);

	// Adjust PC.
	m_registers[15].value -= 6 * 4; // 3 instructions + 4 for breakpoint.
}
//###########################################################################

void 
JTAG_arm9tdmi::CreateSC1Packet(unsigned int *indata, unsigned int opcode, unsigned int data, bool speed)
{
	unsigned int buf[4];
	unsigned int instr = 0;

	// First reverse the instruction.
	for (int i = 0; i < 32; i++) {
		instr |= (((opcode >> i) & 1) << (31 - i));
	}

	buf[0] = data;
	buf[1] = instr << 3;
	buf[2] = instr >> 29;

	if (speed) {
		buf[1] |= 4;
	}

	indata[0] = buf[0];
	indata[1] = buf[1];
	indata[2] = buf[2];
}
//###########################################################################

void 
JTAG_arm9tdmi::ReadMem(char *pDest, int Address, int size)
{
	int	iWords;
	int LDM_Mask;
	int iReads;
	bool bAlignment = ((Address & 3) != 0);
	int *pIntPtr = (int *)pDest;
	int offset = 0;

	// Handle small sizes
	switch (size) {
		case 0: return;
		case 1:
			ReadByte(Address, pDest);
			return;
		case 2:
			if ((Address & 1) == 0) {
				ReadHWord(Address, pDest);
			} else {
				ReadByte(Address, pDest);
				ReadByte(Address+1, &pDest[1]);
			}
			return;
		case 3:
			if ((Address & 1) == 0) {
				ReadHWord(Address, pDest);
				ReadByte(Address + 2, &pDest[2]);
			} else {
				ReadByte(Address, pDest);
				ReadHWord(Address + 1, &pDest[1]);
			}

			return;
	}


	// Check alignment
	if ((Address & 3) != 0) {
		int bytesoff = 4 - (Address & 3);
		for (int i = 0; i < bytesoff; ++i) {
			ReadByte(Address + i, &pDest[i]);
		}

		// Update size.
		size -= bytesoff;
		offset = bytesoff;
	}


	// Align address.
	Address = (Address + 3) & ~3;

	// Calculate number of whole words
	iWords = (size >> 2);

	// Update data pointer
	pIntPtr = (int *)&pDest[offset];

	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
	do {
		if (iWords > 6) {
			LDM_Mask = 0x7e; // To load r1-r7
			iReads = 6;
		}
		else {
			LDM_Mask = 0;
			for (int i = 0; i < iWords; i++) {
				LDM_Mask |= (1 << (i+1));
			}
			iReads = iWords;
		}

		Execute(LDR_R0_PC, 0, false);
		Execute(NOP, 0, false); // Clock 1
		Execute(NOP, 0, false); // Clock 2

		Execute(NOP, Address, false); // Loads data

		// Load LDMIa instruction.
		Execute(LDMIA_R0_XX | LDM_Mask, 0, false);
		Execute(NOP, 0, true); // SYSSPEED high for system speed access.

		// Restart to execute my LDM in system speed.
		EnterInstruction(INSTR_RESTART);
		EnterInstruction(INSTR_INTEST);
		WaitStatus(STATUS_SYSSPEED);
		SelectScanChain(1);
		EnterInstruction(INSTR_INTEST);

		// Load STM instruction.
		Execute(STM_R0_XX | LDM_Mask, 0, false);
		Execute(NOP, 0, false); // Clock 1
		Execute(NOP, 0, false); // Clock 2
		for (int i = 0; i < iReads;  i++) {
			unsigned long val = Execute(NOP, 0, false);
			memcpy(pIntPtr, &val, 4);
			pIntPtr++;
			offset += 4;
		}
		Address += iReads * 4;
		iWords -= iReads;
		size -= iReads * 4;
	} while(iWords > 0);

	// Read the remaining bytes.
	if (size > 0) {
		for (int i = 0; i < size; ++i) {
			ReadByte(Address + i, &pDest[offset + i]);
		}
	}

}
//###########################################################################

void
JTAG_arm9tdmi::ReadHWord(unsigned long Address, void *pData)
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(LDR_R0_PC, 0, false);
	Execute(NOP, Address, false); // Clock 1
	Execute(NOP, Address, false); // Clock 1

	// Load Address.
	Execute(NOP, Address, false);


	Execute(LDRH_R1_R0, 0, false);
	Execute(NOP, 0, true);  // SYSSPEED high for system speed access.

	// Restart to execute my STM in system speed.
	EnterInstruction(INSTR_RESTART);
	EnterInstruction(INSTR_INTEST);
	WaitStatus(STATUS_SYSSPEED);
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(STR_R1_R0, 0, false);
	Execute(NOP, 0, false); // Clock 1
	Execute(NOP, 0, false); // Clock 2
	unsigned long val = Execute(NOP, 0, false); // get data.
	((int *)pData)[0] = val & 0xffff;
	Execute(NOP, 0, false); // Get it out of the system.


}
//###########################################################################
void
JTAG_arm9tdmi::WriteHWord(unsigned long Address, void *pData)
{

	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(LDR_R0_PC, 0, false);
	Execute(NOP, Address, false); // Clock 1
	Execute(NOP, Address, false); // Clock 1
	// Load Address.
	Execute(NOP, Address, false);

	unsigned long data = 0;
	memcpy(&data, pData, 2);

	Execute(LDR_R1_PC, 0, false);
	Execute(NOP, data, false); // Clock 1
	Execute(NOP, data, false); // Clock 1
	// Load Address.
	Execute(NOP, data, false);

	Execute(STRH_R1_R0, 0, false);
	Execute(NOP, 0, true);

	EnterInstruction(INSTR_RESTART);
	EnterInstruction(INSTR_INTEST);
	WaitStatus(STATUS_SYSSPEED);
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);


}



void 
JTAG_arm9tdmi::WriteMem(char *pData, int Address, int size)
{
	int	iWords;
	int LDM_Mask;
	int iReads;
	bool bAlignment = ((Address & 3) != 0);
	int *piData = (int *)pData;
	int offset = 0;

	// Handle small sizes
	switch (size) {
		case 0: return;
		case 1:
			WriteByte(Address, pData);
			return;
		case 2:
			if ((Address & 1) == 0) {
				WriteHWord(Address, pData);
			} else {
				WriteByte(Address, pData);
				WriteByte(Address+1, &pData[1]);
			}
			return;
		case 3:
			if ((Address & 1) == 0) {
				WriteHWord(Address, pData);
				WriteByte(Address + 2, &pData[2]);
			} else {
				WriteByte(Address, pData);
				WriteHWord(Address + 1, &pData[1]);
			}
			
			return;
	}


	// Check alignment
	if ((Address & 3) != 0) {
		int bytesoff = 4 - (Address & 3);
		for (int i = 0; i < bytesoff; ++i) {
			WriteByte(Address + i, &pData[i]);
		}

		// Update size.
		size -= bytesoff;
		offset = bytesoff;
	}

	// Correct the data pointer.
	piData = (int *)&pData[offset];

	// Align address.
	Address = (Address + 3) & ~3;

	// Calculate how many whole words we can write.
	iWords = (size >> 2);

	if (iWords > 0) {
		SelectScanChain(1);
		EnterInstruction(INSTR_INTEST);
		do {
			if (iWords > 6) {
				LDM_Mask = 0x7e; // To load r1-r7
				iReads = 6;
			}
			else {
				LDM_Mask = 0;
				for (int i = 0; i < iWords; i++) {
					LDM_Mask |= (1 << (i+1));
				}
				iReads = iWords;
			}
			Execute(LDR_R0_PC, 0, false);
			Execute(NOP, Address, false); // Clock 1
			Execute(NOP, Address, false); // Clock 1

			// Load Address.
			Execute(NOP, Address, false);

			// Load data.
			// Load LDMIa instruction.
			Execute(LDMIA_R0_XX | LDM_Mask, 0, false);
			Execute(NOP, 0, false); // Clock 1
			Execute(NOP, 0, false); // Clock 1
			for (int i = 0; i < iReads; i++) {
				unsigned long data = 0;
				memcpy(&data, piData, 4);
				piData++;
				Execute(NOP, data, false); // Load Data.
				offset += 4;
			}

			// Load STMIa instruction.
			Execute(STMIA_R0_XX | LDM_Mask, 0, false);
			Execute(NOP, 0, true);  // SYSSPEED high for system speed access.

			// Restart to execute my STM in system speed.
			EnterInstruction(INSTR_RESTART);
			EnterInstruction(INSTR_INTEST);

			WaitStatus(1 << 3);

			SelectScanChain(1);
			EnterInstruction(INSTR_INTEST);

			Address += iReads * 4;
			iWords -= iReads;
			size -= iReads * 4;
		} while(iWords > 0);
	}

	if (size > 0) {
		// Write the last bytes.
		for (int i = 0; i < size; ++i) {
			WriteByte(Address + i, &pData[offset + i]);
		}
	}
}

//###########################################################################



void
JTAG_arm9tdmi::SystemReset()
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
	Execute(MOV_PC_0, 0, false);
	Execute(NOP, 0, true);
	EnterInstruction(INSTR_RESTART);
}



//###########################################################################
void
JTAG_arm9tdmi::Halt()
{
	int status = ReadICEReg(1);
	if (status & 1) {
		// System already halted!.
	} else {
		// Halt system.
		//printf("Setting a breakpoint with all addresspins masked\n");
		
		WriteICEReg(0, CTRL_DBGRQ | CTRL_INTDIS);
		bool ok = WaitStatus(STATUS_DBGACK);
		if (!ok) {
			return;
		}
		WriteICEReg(0, CTRL_INTDIS);
	}

	GetContext();
}
//###########################################################################


int
JTAG_arm9tdmi::ReadICEReg(int iReg)
{
	int data[2];

	SelectScanChain(2); // EICE scan-chain
	EnterInstruction(INSTR_INTEST);

	data[0] = 0;
	data[1] = iReg | (0 << 5); // Read reg <iReg>
	//m_device->ShiftDRBits((char *)data, NULL, 38);
	//m_device->ShiftDRBits(NULL, (char *)data, 38);

	jtag_transaction *t1 = getTrans();
	jtag_transaction *t2 = getTrans();

	*t1 = jtag_transaction(jtag_transaction::eShiftDR, (char *)data, NULL, 38, true);
	*t2 = jtag_transaction(jtag_transaction::eShiftDR, NULL, (char *)data, 38, true);

	m_device->Enqueue(t1);
	m_device->Enqueue(t2);

	while(!t2->done());

	relTrans(t1);
	relTrans(t2);


	return data[0];
}
//###########################################################################

void
JTAG_arm9tdmi::WriteICEReg(int iReg, int indata)
{
	int data[2];

	SelectScanChain(2); // EICE scan-chain
	EnterInstruction(INSTR_INTEST);

	data[1] = iReg | (1 << 5); // Write reg <iReg>
	data[0] = indata;
	//m_device->ShiftDRBits((char *)data, NULL, 38);

	jtag_transaction *t = getTrans();

	*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)data, NULL, 38, true);
	m_device->Enqueue(t);

	while(!t->done());

	relTrans(t);

}
//###########################################################################

void
JTAG_arm9tdmi::Continue()
{

	// Make sure WP0 is configured to break on the breakpoint instruction.
	SetupSWBreakWatchPoint();

	// clear intdis.
	//WriteICEReg(0, 0);
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	// try whacking the IRQ's
	//m_registers[16].value |= 0xc0;

	// remove THUMB bit from CPSR
	m_registers[16].value &= ~(1 << 5);

	// Reload CPSR
	Execute(LDR_R1_PC, 0, false);
	Execute(NOP, 0, false);
	Execute(NOP, 0, false);
	Execute(NOP, m_registers[16].value, false);
	Execute(MSR_R1_CPSR, 0, false);
	Execute(NOP, 0, false);
	Execute(NOP, 0, false);
	Execute(NOP, 0, false);
	Execute(NOP, 0, false);

	// Reload all registers.
	Execute(LDMIA_R0_XX | 0xffff, 0, false);
	Execute(NOP, 0, false);
	Execute(NOP, 0, false);

	if (m_targetWasThumb) {
		Execute(NOP, m_registers[15].value | 1, false);
		for (int i = 1; i < 16; i++) {
			Execute(NOP, m_registers[i].value, false);
		}

		Execute(NOP, 0, false);
		Execute(BX(0), 0, false);
		Execute(NOP, 0, false);
		Execute(NOP, 0, false);
		// Thumb now.
		Execute(THUMB_NOP, 0, false);
		Execute(THUMB_NOP, 0, false);
		Execute(THUMB_MOV_IMM(0, 0x00), 0, false);
		Execute(THUMB_LDR(0, 0), 0, false);
		Execute(THUMB_NOP, 0, false);
		Execute(THUMB_NOP, 0, false);
		//Execute(m_registers[0].value, false);
		Execute(THUMB_NOP, m_registers[0].value, false);
		Execute(THUMB_NOP, 0, false);

		Execute(THUMB_NOP, 0, false);
		Execute(THUMB_B(0x7f6), 0, false);
		Execute(THUMB_NOP, 0, true);

		EnterInstruction(INSTR_RESTART);
	} else {
		for (int i = 0; i < 16; i++) {
			Execute(NOP, m_registers[i].value, false);
		}
		Execute(NOP, 0, false);
		debug_printf("Returning to: 0x%x\r\n",m_registers[15].value);
		// Branch X addresses back.
		Execute(NOP, 0, false);
		//Execute(0xeafffffe, 0, false);
		Execute(0xeafffffd, 0, false); // 2 addresses back. from the 2 nops above.
		Execute(NOP, 0, true);
		EnterInstruction(INSTR_RESTART);
	}

/*
	// Branch X addresses back.
	Execute(NOP, 0, false);
	//Execute(0xeafffffe, 0, false);
	Execute(0xeafffffe, 0, false); // 2 addresses back. lets assume only one of them are from here.
	Execute(NOP, 0, true);
	m_device->EnterInstruction(INSTR_RESTART);
*/
	// clear intdis.
	WriteICEReg(0, 0);
}

//###########################################################################

void 
JTAG_arm9tdmi::KillMMU()
{
	unsigned int iData[3];
	unsigned int iDataOut[3];
	unsigned int C15State;


	// Enter interpreted mode.
	SC15Physical(SC15_PHYS_C15State, 0, false);
	C15State = SC15Physical(SC15_PHYS_C15State, 0, false);
	SC15Physical(SC15_PHYS_C15State, C15State | 1, true);


	// MRC p15, 5, r0, c15, c1, 2
	SC15Interpreted(CP_INSN(1, 5, 0, 15, 1,2));

	
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
	Execute(LDR_R1_PC, 0, false);
	Execute(NOP, 0, true);

	// Restart to execute my LDR in system speed.
	EnterInstruction(INSTR_RESTART);
	EnterInstruction(INSTR_INTEST);
	WaitStatus(STATUS_SYSSPEED);


	SC15Physical(SC15_PHYS_C15State, C15State & 0xfffffffe, true);
	

	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
	Execute(STR_R1_R0, 0, false);
	Execute(NOP, 0, false); // 1
	Execute(NOP, 0, false); // 2
	m_TTB = Execute(NOP, 0, false); // 3 -- value should be here now?!
	Execute(NOP, 0, false); // Get it out of the pipeline.


	// Ok, MMU must die now...
	SC15Physical(SC15_PHYS_Control, 0, false);
	m_CP15_C1 = SC15Physical(SC15_PHYS_Control, 0, false);
	//SC15Physical(SC15_PHYS_Control, Control, true);

	SC15Physical(SC15_PHYS_Control, m_CP15_C1 & (~0x1005), true);
	//SC15Physical(SC15_PHYS_Control, 0xc000007a, true);
	//SC15Physical(SC15_PHYS_Control, m_CP15_C1 & 0xfffffffe, true);


	// Just for checking.
	SC15Physical(SC15_PHYS_Control, 0, false);
	unsigned long Control  = SC15Physical(SC15_PHYS_Control, 0, false);

//0b10000101111010
//0b01000001111110
	//printf("0x0c000243 == 0x%x\n", TranslateAddress(0x0c700243));

}
//###########################################################################

unsigned long
JTAG_arm9tdmi::GetFSR(int cache)
{
	unsigned long retVal;
	unsigned int C15State;


	// Enter interpreted mode.
	SC15Physical(SC15_PHYS_C15State, 0, false);
	C15State = SC15Physical(SC15_PHYS_C15State, 0, false);
	SC15Physical(SC15_PHYS_C15State, C15State | 1, true);

	// MRC p15, 0, r0, c5, c0, <cache>
	SC15Interpreted(CP_INSN(1, 0, 0, 5, 0, cache));


	// Push a LDR instruction and run it in sys-speed
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
	Execute(LDR_R1_PC, 0, false);
	Execute(NOP, 0, true);

	// Restart to execute my LDR in system speed.
	EnterInstruction(INSTR_RESTART);
	EnterInstruction(INSTR_INTEST);
	WaitStatus(STATUS_SYSSPEED);

	// Turn interpreted access off.
	SC15Physical(SC15_PHYS_C15State, C15State & 0xfffffffe, true);


	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
	Execute(STR_R1_R0, 0, false);
	Execute(NOP, 0, false); // 1
	Execute(NOP, 0, false); // 2
	retVal = Execute(NOP, 0, false); // 3 -- value should be here now?!
	Execute(NOP, 0, false); // Get it out of the pipeline.

	return retVal;

}
//###########################################################################

unsigned long 
JTAG_arm9tdmi::SC15Physical(unsigned long reg, unsigned long value, bool bwrite)
{


	// Lets access CP15.C15.State (Physical mode)
	// using SC15.. insane.. alot of 15 here..:D
	// SC15 is 40bit, and looks like this.
	// 39 - R/W bit. (Read:0/Write:1) (i think)
	// 38:33 - Register.
	// 32:1 - Data
	// 1 - Physical(1), Interpreted(0)


	int iData[2];
	int oData[2];


	iData[0] = (value << 1) | 1;
	iData[1] = ((bwrite?1:0) << 7) | (reg << 1) | (value >> 31);
	SelectScanChain(15);
	EnterInstruction(INSTR_INTEST);
	//m_device->ShiftDRBits((char *)iData, (char *)oData, 40);

	jtag_transaction *t = getTrans();

	*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)iData, (char *)oData, 40, false);
	m_device->Enqueue(t);
	while(!t->done());

	relTrans(t);


	return ((oData[0] >> 1) | (oData[1] << 31));

}
//###########################################################################

void 
JTAG_arm9tdmi::SC15Interpreted(unsigned long insn)
{

	// Lets access CP15.C15.State (Physical mode)
	// using SC15.. insane.. alot of 15 here..:D
	// SC15 is 40bit, and looks like this.
	// 39 - R/W bit. (Read:0/Write:1) (i think)
	// 38:33 - Register.
	// 32:1 - Data
	// 1 - Physical(1), Interpreted(0)

	int iData[2];

	iData[0] = (insn << 1);
	iData[1] = (insn >> 31);
	SelectScanChain(15);
	EnterInstruction(INSTR_INTEST);
	//m_device->ShiftDRBits((char *)iData, NULL, 40);


	jtag_transaction *t = getTrans();

	*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)iData, NULL, 40, false);
	m_device->Enqueue(t);

	while(!t->done());

	relTrans(t);
}
//###########################################################################

bool
JTAG_arm9tdmi::WaitStatus(int status)
{
	int s = 0;
	for (int i = 0; i != 15; i++) {
		s = ReadICEReg(1);
		if (s & status) {
			return true;
		}
		//sleep(1);
	}
	return false;
}
//###########################################################################

unsigned long
JTAG_arm9tdmi::TranslateAddress(unsigned long MVA)
{
	//unsigned long *Level1Table = new unsigned long[4096];
	unsigned long Level1Entry[4];

	// Was the MMU in use?
	if (!(m_CP15_C1 & 1)) {
		return MVA;
	}

	// Do we have a TTB?
	if (0xffffffff == m_TTB) {
		return 0xffffffff;
	}

	ReadMem((char *)&Level1Entry[0], m_TTB + ((MVA >> 18) & (~3)), 4 * 4);
	//printf("Level1 0x%x!\n", m_TTB + (MVA >> 20));

	switch(Level1Entry[0] & 3) {
		case 0: // Faulty.
			//printf("Faulty!\n");
			return 0xffffffff;
			break;
		case 1: // Coarse.
			{
				unsigned long Level2Address = (Level1Entry[0] & 0xfffffc00) | ((MVA & 0x000ff000) >> 10);
				unsigned long Level2Entry;
				ReadMem((char *)&Level2Entry, Level2Address, 4);
				//printf("Coarse Level2: 0x%x\n", Level2Entry);
				return Level2Decode(Level2Entry, MVA);
			}
			break;
		case 2: // Section. easy one, me like.
			return ((Level1Entry[0] & 0xfff00000) | (MVA & 0x000fffff));
			//printf("Section!\n");
			break;
		case 3: // Fine.
			{
				unsigned long Level2Address = (Level1Entry[0] & 0xfffff000) | ((MVA & 0x000ffc00) >> 8);
				unsigned long Level2Entry;
				ReadMem((char *)&Level2Entry, Level2Address, 4);
				//printf("Fine Level2: 0x%x\n", Level2Entry);
				return Level2Decode(Level2Entry, MVA);
			}
			break;
	}
	
}
//###########################################################################


unsigned long 
JTAG_arm9tdmi::Level2Decode(unsigned long Level2Descriptor, unsigned long MVA)
{
	switch(Level2Descriptor & 3) {
	case 0: // Fault
		return 0xffffffff;
	case 1: // Large
		return (Level2Descriptor & 0xffff0000) | (MVA & 0x0000ffff);
	case 2: // Small
		return (Level2Descriptor & 0xfffff000) | (MVA & 0x00000fff);
	case 3: // Tiny
		return (Level2Descriptor & 0xfffffc00) | (MVA & 0x000003ff);
	}
}
//###########################################################################

bool
JTAG_arm9tdmi::Halted()
{
	int status = ReadICEReg(1);

	return (status & 1)?true:false;
}
//###########################################################################


void 
JTAG_arm9tdmi::MaskInt()
{
	// Try to disable interrupts.
	unsigned long newCPSR = m_registers[16].value | 0xc0;
	Execute(LDR_R1_PC, 0, false);
	Execute(NOP, 0, false); // Clock 1
	Execute(NOP, 0, false); // Clock 2
	Execute(NOP, newCPSR, false);
	Execute(MSR_R1_CPSR_C, 0, false);

	// This is odd.
	Execute(NOP, 0, false); 
	Execute(NOP, 0, false); 
	Execute(NOP, 0, false); 
	Execute(NOP, 0, false); 

}
//###########################################################################
void 
JTAG_arm9tdmi::UnMaskInt()
{

}
//###########################################################################

void 
JTAG_arm9tdmi::HandleException()
{
	switch (m_registers[ARM_REG_CPSR].value & 0x1f) {
		case ARM_MODE_ABT:
		case ARM_MODE_UDF:
			// This mean it was a crash.
			// And we don't really care about r13-r15 in this processormode.
			m_registers[15].value = m_registers[14].value; // To get the proper PC.
			GetRegsFromMode(m_registers[ARM_REG_SPSR].value);


			break;

		case ARM_MODE_USR:
		case ARM_MODE_FIQ:
		case ARM_MODE_IRQ:
		case ARM_MODE_SVC:
		case ARM_MODE_SYS:
			// Just a normal breakpoint probably, nothing to worry about.
			break;

	}
}
//###########################################################################

void
JTAG_arm9tdmi::GetRegsFromMode(unsigned long cpsr)
{
	// Get back to whichever mode it is, but with IRQ/FIQ disabled.
	unsigned long newCPSR = cpsr | 0xc0;

	Execute(LDR_R1_PC, 0, false);
	Execute(NOP, 0, false); // Clock 1
	Execute(NOP, 0, false); // Clock 2
	Execute(NOP, cpsr, false);
	Execute(MSR_R1_CPSR_C, 0, false);

	// This is odd.
	Execute(NOP, 0, false); 
	Execute(NOP, 0, false); 
	Execute(NOP, 0, false); 
	Execute(NOP, 0, false); 

	Execute(STM_R0_XX | (1 << 13) | (1 << 14), 0, false);
	Execute(NOP, 0, false); // Clock 1
	Execute(NOP, 0, false); // Clock 2

	m_registers[13].value = Execute(NOP, 0, false);
	m_registers[14].value = Execute(NOP, 0, false);

	Execute(NOP, 0, false); // Last clock of STM

}
//###########################################################################

void 
JTAG_arm9tdmi::SetBreakpoint(unsigned long Address)
{

	// Only support one breakpoint for now.
	// SW breakpoints can be handled in some other layer above this.


	unsigned long WP0Control = (1 << 8)/* | (1 << 4)*/ | ((Address & 0x01)?(1 << 1):0);

	WriteICEReg(ARM_ICE_WP0_ADDRESS, Address & ~1);
	WriteICEReg(ARM_ICE_WP0_ADDRESS_MASK, (Address & 1)?0x1:0x3);
	WriteICEReg(ARM_ICE_WP0_DATA, 0);
	WriteICEReg(ARM_ICE_WP0_DATA_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, (Address & 1)?0xfffffffd:0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL, WP0Control);
}
//###########################################################################


void 
JTAG_arm9tdmi::GetContext()
{
	int status = ReadICEReg(1);
	bool bThumb = (status & 0x10)?true:false;

	if (bThumb) {
		m_targetWasThumb = true;
		// Force the core into ARM mode.
		SwitchToARM();
		bool bThumb = (status & 0x10)?true:false;
		ReadRegs();
		m_registers[15].value = m_savedPC - (0x09 << 1); // 4 instructions for halt, and 5 instructions for switching to ARM.
		m_registers[0].value = m_savedR0;
		m_registers[16].value |= (1 << 5); // Set the T bit in CPSR.
	} else {
		m_targetWasThumb = false;

		// Get rid of annoying breakpoint.
		//WriteICEReg(ARM_ICE_WP0_CONTROL, 0);

		// Obtain all registers.
		ReadRegs();

		// Mask interrupts.
		MaskInt();

		// Set vector trapping.
		//WriteICEReg(2, 0x1a | 1);

		// Kill the MMU before we start playing around too much!
		//KillMMU();


		//GetFSR(0);
		//GetFSR(1);


		// Clear any breakpoint, it'll interfere with singlestepping etc.
	}

	ClearBreakpoint(0);

}
//###########################################################################

void 
JTAG_arm9tdmi::SingleStep()
{
	// Set singlestep bit.
	int Control = ReadICEReg(0);
	Control |= 8;
	WriteICEReg(0, Control);
	Continue();

	// Clear singlestep bit.
	Control = ReadICEReg(0);
	Control &= ~8;
	WriteICEReg(0, Control);
	
	// Wait for system to get back into debug mode.
	bool bHalted = WaitStatus(1 << 3);
	if (bHalted) {
		GetContext();

		// Mask interrupts.
		MaskInt();
		// Set vector trapping.
		//WriteICEReg(2, 0x1a | 1);
	} else {
		// We timed out, and the core is not halted.
		// Do something
	}
}
//###########################################################################

void 
JTAG_arm9tdmi::CommChannelWrite(unsigned long data)
{
	unsigned long CommsCTRL;
	while (((CommsCTRL = ReadICEReg(4)) & 0x01)) {
		// Wait..
	}


	WriteICEReg(5, data);
}
//###########################################################################

unsigned long 
JTAG_arm9tdmi::CommChannelRead()
{
	unsigned long CommsCTRL;
	while (!((CommsCTRL = ReadICEReg(4)) & 0x02)) {
		// Wait..
	}

	unsigned long data = ReadICEReg(5);

	return data;
}
//###########################################################################


void
JTAG_arm9tdmi::CommChannelWriteStream(unsigned char *indata, int bytes)
{
	unsigned int data[2];
	// Lets push the data out as bloody fast as we can.
	SelectScanChain(2); // EICE scan-chain
	EnterInstruction(INSTR_INTEST);


	jtag_transaction *t = getTrans();
	while(bytes > 0) {
		data[1] = 5 | (1 << 5); // Write reg <iReg>

		size_t wsize = 0;
		if (bytes > 4) {
			wsize = 4;
		} else {
			wsize = bytes;
		}

		memcpy(&data[0], indata, wsize);
		indata += wsize;
		bytes -= wsize;

		//data[0] = *indata++;
		*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)data, NULL, 38, true);
		m_device->Enqueue(t);
		while(!t->done());
	}
	relTrans(t);
}
//###########################################################################



void
JTAG_arm9tdmi::CommChannelReadStream(unsigned char *indata, int bytes)
{
	unsigned int data[2];
	// Lets push the data out as bloody fast as we can.
	SelectScanChain(2); // EICE scan-chain
	EnterInstruction(INSTR_INTEST);

	while (!((ReadICEReg(4)) & 0x02)) {
		// Wait..
	}

	jtag_transaction *t1 = getTrans();

	while(bytes > 0) {
		// Select register
		data[0] = 0;
		data[1] = 5 | (0 << 5); // Read reg 
		*t1 = jtag_transaction(jtag_transaction::eShiftDR, (char *)data, NULL, 38, true);
		m_device->Enqueue(t1);
		while(!t1->done());

		// Read data.
		*t1 = jtag_transaction(jtag_transaction::eShiftDR, NULL, (char *)data, 38, true);
		m_device->Enqueue(t1);
		while(!t1->done());


		size_t wsize = 0;
		if (bytes > 4) {
			wsize = 4;
		} else {
			wsize = bytes;
		}
		memcpy(indata, &data[0], wsize);
		indata += wsize;
		bytes -= wsize;

	}
	relTrans(t1);
}
//###########################################################################


void
JTAG_arm9tdmi::ClearBreakpoint(unsigned long Address)
{
	// For now, just disable the breakpoint we're using.
	WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL, 0);
}

//###########################################################################

void
JTAG_arm9tdmi::SwitchToARM()
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(THUMB_STR(0, 0), 0, false); // Store r0 first
	Execute(THUMB_NOP, 0, false);
	Execute(THUMB_NOP, 0, false);
	unsigned long r0 = Execute(THUMB_NOP, 0, false);

	Execute(THUMB_MOV(0, 15), 0, false); // Copy PC
	Execute(THUMB_STR(0, 0), 0, false); // Store PC.
	Execute(THUMB_NOP, 0, false);
	Execute(THUMB_NOP, 0, false);
	unsigned long pc = Execute(THUMB_NOP, 0, false);

	Execute(THUMB_MOV_IMM(0, 0), 0, false);
	Execute(THUMB_BX(0), 0, false); // Should switch to ARM here.
	Execute(THUMB_NOP, 0, false);
	Execute(THUMB_NOP, 0, false);

	Execute(NOP, 0, false);
	Execute(NOP, 0, false);

	m_savedPC = pc;
	m_savedR0 = r0;

}
//###########################################################################

void
JTAG_arm9tdmi::setFlag(unsigned long flag, unsigned long value)
{
	switch (flag) {
		case 0: // Exception vector trapping
			if (0 == value) {
				WriteICEReg(2, 0);
				return;
			}
			WriteICEReg(2, 0x1a | 1);
			break;
		default: // Unknown.
			break;
	}
}
//###########################################################################




void 
JTAG_arm9tdmi::ReadByte(unsigned long Address, void *pData)
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(LDR_R0_PC, 0, false);
	Execute(NOP, Address, false); // Clock 1
	Execute(NOP, Address, false); // Clock 1

	// Load Address.
	Execute(NOP, Address, false);


	Execute(LDRB_R1_R0, 0, false);
	Execute(NOP, 0, true);  // SYSSPEED high for system speed access.

	// Restart to execute my STM in system speed.
	EnterInstruction(INSTR_RESTART);
	EnterInstruction(INSTR_INTEST);
	WaitStatus(STATUS_SYSSPEED);
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(STR_R1_R0, 0, false);
	Execute(NOP, 0, false); // Clock 1
	Execute(NOP, 0, false); // Clock 2
	unsigned long val = Execute(NOP, 0, false); // get data.
	((unsigned char *)pData)[0] = val & 0xff;
	Execute(NOP, 0, false); // Get it out of the system.



}
//###########################################################################


void 
JTAG_arm9tdmi::WriteByte(unsigned long Address, void *pData)
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(LDR_R0_PC, 0, false);
	Execute(NOP, Address, false); // Clock 1
	Execute(NOP, Address, false); // Clock 1
	// Load Address.
	Execute(NOP, Address, false);

	unsigned long data = ((unsigned char *)pData)[0];


	Execute(LDR_R1_PC, 0, false);
	Execute(NOP, data, false); // Clock 1
	Execute(NOP, data, false); // Clock 1
	// Load Address.
	Execute(NOP, data, false);

	Execute(STRB_R1_R0, 0, false);
	Execute(NOP, 0, true);

	EnterInstruction(INSTR_RESTART);
	EnterInstruction(INSTR_INTEST);
	WaitStatus(STATUS_SYSSPEED);
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
}
//###########################################################################

bool
JTAG_arm9tdmi::HasFeature(target::EFeature feature)
{
	switch (feature) {
		case target::eFeatureDCC:
			return true;

		default:
			return false;
	}
}
//###########################################################################

void 
JTAG_arm9tdmi::FlushDCache()
{
}
//###########################################################################

void 
JTAG_arm9tdmi::FlushICache()
{
	// MCR p15, 0, r0, c7, c5, 0 - Ivalidate ICache
	unsigned long retVal;
	unsigned int C15State;


	// Enter interpreted mode.
	SC15Physical(SC15_PHYS_C15State, 0, false);
	C15State = SC15Physical(SC15_PHYS_C15State, 0, false);
	SC15Physical(SC15_PHYS_C15State, C15State | 1, true);

	// MRC p15, 0, r0, c5, c0, <cache>
	//SC15Interpreted(CP_INSN(1, 0, 0, 5, 0, cache));


	// Push a LDR instruction and run it in sys-speed
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
	Execute(LDR_R1_PC, 0, false);
	Execute(NOP, 0, true);

	// Restart to execute my LDR in system speed.
	EnterInstruction(INSTR_RESTART);
	EnterInstruction(INSTR_INTEST);
	WaitStatus(STATUS_SYSSPEED);

	// Turn interpreted access off.
	SC15Physical(SC15_PHYS_C15State, C15State & 0xfffffffe, true);


	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
	Execute(STR_R1_R0, 0, false);
	Execute(NOP, 0, false); // 1
	Execute(NOP, 0, false); // 2
	retVal = Execute(NOP, 0, false); // 3 -- value should be here now?!
	Execute(NOP, 0, false); // Get it out of the pipeline.

}
//###########################################################################

void 
JTAG_arm9tdmi::SetupSWBreakWatchPoint()
{
	// Only support one breakpoint for now.
	// SW breakpoints can be handled in some other layer above this.
	unsigned int swbkpt;
	GetSWBreakpointInstruction((unsigned char *)&swbkpt, 4);

	unsigned long WP0Control = (1 << 8)/* | (1 << 4)*/ | 0;

	WriteICEReg(ARM_ICE_WP0_ADDRESS, 0);
	WriteICEReg(ARM_ICE_WP0_ADDRESS_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_DATA, swbkpt);
	WriteICEReg(ARM_ICE_WP0_DATA_MASK, 0);
	WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL, WP0Control);

}

