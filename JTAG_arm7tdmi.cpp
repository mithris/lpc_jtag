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
#include "JTAG_arm7tdmi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//###########################################################################


JTAG_arm7tdmi::JTAG_arm7tdmi()
{
	m_TTB = 0xffffffff;
}
//###########################################################################

JTAG_arm7tdmi::~JTAG_arm7tdmi()
{
}
//###########################################################################

void
JTAG_arm7tdmi::EnterInstruction(int insn)
{
	jtag_transaction *t = getTrans();

	*t = jtag_transaction(jtag_transaction::eEnterInstruction, (char *)&insn, NULL, 4, insn==INSTR_RESTART?true:false);

	m_device->Enqueue(t);

	while(!t->done());
	relTrans(t);

}
//###########################################################################

void
JTAG_arm7tdmi::ShiftDRBits(unsigned char *in, unsigned char *out, int bits)
{
	jtag_transaction *t = getTrans();

	*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)in, (char *)out, bits, true); // I think the previous behaviour for ARM was to always run by RTI

	m_device->Enqueue(t);

	while(!t->done());
	relTrans(t);

}
//###########################################################################


void
JTAG_arm7tdmi::SelectScanChain(int chain)
{
	EnterInstruction(INSTR_SCAN_N);

	jtag_transaction *t = getTrans();
	*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)&chain, NULL, 4, true);
	m_device->Enqueue(t);

	while(!t->done());
	relTrans(t);
}
//###########################################################################

unsigned long
JTAG_arm7tdmi::Execute(unsigned long insn, bool speed)
{
	unsigned int in[4];
	unsigned int out[4];

	CreateSC1Packet(in, insn, speed);

	jtag_transaction *t = getTrans();
	*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)in, (char *)out, 33, true); // Should be false?
	m_device->Enqueue(t);

	while(!t->done());


	int datain, dataout;
	dataout = 0;
	datain = (out[0] >> 1) | (out[1] << 31);
	for (int i = 0; i < 32; i++) {
		dataout |= (((datain >> i) & 1) << (31 - i));
	}

	relTrans(t);
	return dataout;
}
//###########################################################################

void
JTAG_arm7tdmi::ReadRegs()
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);


	Execute(STM_ALL_REGS, false);
	Execute(NOP, false);
	Execute(NOP, false); 

	for (int i = 0; i < 16; i++) {
		unsigned long val = Execute(NOP, false);
		m_registers[i].value = val;
	}
	Execute(MRS_R1_CPSR, false); 
	Execute(STR_R1_R0, false);
	Execute(NOP, false);
	Execute(NOP, false);
	m_registers[16].value = Execute(NOP, false);
	//printf("Reg: CPSR = 0x%x \n",m_registers[16].value);

	Execute(MRS_R1_SPSR, false);
	Execute(STR_R1_R0, false);
	Execute(NOP, false);
	Execute(NOP, false);
	m_registers[17].value = Execute(NOP, false);
	//printf("Reg: SPSR = 0x%x \n",m_registers[17].value);
	

	// Adjust PC.
	m_registers[15].value -= 2 * ARM_ADDRESS_SIZE;
	//printf("Adjusted PC: 0x%x\n", m_registers[15].value);
}
//###########################################################################

void 
JTAG_arm7tdmi::CreateSC1Packet(unsigned int *indata, unsigned int opcode, bool speed)
{
	unsigned int buf[2];
	unsigned int instr = 0;

	// First reverse the instruction.
	for (int i = 0; i < 32; i++) {
		instr |= (((opcode >> i) & 1) << (31 - i));
	}


	buf[0] = (instr << 1) | (speed?1:0);
	buf[1] = instr >> 31;

	indata[0] = buf[0];
	indata[1] = buf[1];
}
//###########################################################################

void 
JTAG_arm7tdmi::ReadMem(char *pDest, int Address, int size)
{
	int	iWords;
	int LDM_Mask;
	int iReads;
	bool bAlignment = ((Address & 3) != 0);
	int *pIntPtr = (int *)pDest;

	if (size == 2) {
		ReadHWord(Address, pDest);
		return;
	}

	Address &= ~3; // Alignment.

	iWords = (size >> 2);
	if (0 == iWords) {
		iWords = 1;
	}

	
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

		Execute(LDR_R0_PC, false);
		Execute(NOP, false); // Decode
		Execute(NOP, false); // Execute

		Execute(Address, false); // Clock in data
		Execute(NOP, false); // Keep pipeline happy?

		// Load LDMIa instruction.
		Execute(NOP, false);
		Execute(NOP, true); // SYSSPEED high for system speed access.
		Execute(LDMIA_R0_XX | LDM_Mask, false);

		// Restart to execute my LDM in system speed.
		EnterInstruction(INSTR_RESTART);
		EnterInstruction(INSTR_INTEST);
		WaitStatus(STATUS_SYSSPEED);
		SelectScanChain(1);
		EnterInstruction(INSTR_INTEST);

		// Load STM instruction.
		Execute(STM_R0_XX | LDM_Mask, false);
		Execute(NOP, false); // Clock 1
		Execute(NOP, false); // Clock 2
		for (int i = 0; i < iReads;  i++) {
			unsigned long val = Execute(NOP, false);
			*pIntPtr++ = val;
		}
		Address += iReads * 4;
		iWords -= iReads;
	} while(iWords > 0);
}
//###########################################################################

void
JTAG_arm7tdmi::ReadHWord(unsigned long Address, void *pData)
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(LDR_R0_PC, false);
	Execute(NOP, false); // Decode
	Execute(NOP, false); // Execute

	// Load Address.
	Execute(Address, false);
	Execute(NOP, false);


	Execute(NOP, false);
	Execute(NOP, true);  // SYSSPEED high for system speed access.
	Execute(LDRH_R1_R0, false);

	// Restart to execute my STM in system speed.
	EnterInstruction(INSTR_RESTART);
	EnterInstruction(INSTR_INTEST);
	WaitStatus(STATUS_SYSSPEED);
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(STR_R1_R0, false);
	Execute(NOP, false); // Clock 1
	Execute(NOP, false); // Clock 2
	unsigned long val = Execute(NOP, false); // get data.
	((int *)pData)[0] = val & 0xffff;
	Execute(NOP, false); // Get it out of the system.


}
//###########################################################################
void
JTAG_arm7tdmi::WriteHWord(unsigned long Address, void *pData)
{

	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	Execute(LDR_R0_PC, false);
	Execute(NOP, false); // Decode
	Execute(NOP, false); // Execute
	// Load Address.
	Execute(Address, false);
	Execute(NOP, false);

	unsigned long data = ((unsigned short *)pData)[0];

	Execute(LDR_R1_PC, false);
	Execute(NOP, false); // Decode
	Execute(NOP, false); // Execute
	// Load Data
	Execute(data, false);
	Execute(NOP, false);

	Execute(NOP, false);
	Execute(NOP, true);
	Execute(STRH_R1_R0, false);

	EnterInstruction(INSTR_RESTART);
	EnterInstruction(INSTR_INTEST);
	WaitStatus(STATUS_SYSSPEED);
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
}
//###########################################################################

void 
JTAG_arm7tdmi::WriteMem(char *pData, int Address, int size)
{
	int	iWords;
	int LDM_Mask;
	int iReads;
	bool bAlignment = ((Address & 3) != 0);
	int *piData = (int *)pData;

	if (size == 2) {
		WriteHWord(Address, pData);
		return;
	}

	Address &= ~3; // Alignment.

	iWords = (size >> 2);
	if (0 == iWords) {
		iWords = 1;
	}


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
		Execute(LDR_R0_PC, false);
		Execute(NOP, false); // Decode
		Execute(NOP, false); // Execute

		// Load Address.
		Execute(Address, false);
		Execute(NOP, false);

		// Load data.
		// Load LDMIa instruction.
		Execute(LDMIA_R0_XX | LDM_Mask, false);
		Execute(NOP, false); // Clock 1
		Execute(NOP, false); // Clock 1
		for (int i = 0; i < iReads; i++) {
			Execute(*piData++, false); // Load Data.
		}
		Execute(NOP, false);
		Execute(NOP, false);

		// Load STMIa instruction.
		Execute(NOP, false);
		Execute(NOP, true);  // SYSSPEED high for system speed access.
		Execute(STMIA_R0_XX | LDM_Mask, false);

		// Restart to execute my STM in system speed.
		EnterInstruction(INSTR_RESTART);
		EnterInstruction(INSTR_INTEST);

		WaitStatus(STATUS_SYSSPEED);

		SelectScanChain(1);
		EnterInstruction(INSTR_INTEST);

		Address += iReads * 4;
		iWords -= iReads;
	} while(iWords > 0);


}

//###########################################################################



void
JTAG_arm7tdmi::SystemReset()
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);
	Execute(MOV_PC_0, false);
	Execute(NOP, true);
	EnterInstruction(INSTR_RESTART);
}



//###########################################################################
void
JTAG_arm7tdmi::Halt()
{
	int HaltReason = HALT_INVALID;

	WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL, 0);

	int status = ReadICEReg(1);
	bool bThumb = (status & 0x10)?true:false;
	if (status & 1) {
		// System already halted!.
		HaltReason = HALT_BREAKPOINT;
	} else {
		// Halt system.
		
		unsigned long control = ReadICEReg(0);
		control |= CTRL_DBGRQ | CTRL_INTDIS;
		WriteICEReg(0, control);
		/*
		unsigned long WP0Control = (1 << 8) | (1 << 4);
		WriteICEReg(ARM_ICE_WP0_ADDRESS, 0);
		WriteICEReg(ARM_ICE_WP0_ADDRESS_MASK, 0xffffffff);
		WriteICEReg(ARM_ICE_WP0_DATA, 0);
		WriteICEReg(ARM_ICE_WP0_DATA_MASK, 0xffffffff);
		WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, 0xffffffff);
		WriteICEReg(ARM_ICE_WP0_CONTROL, WP0Control);
		*/
		bool ok = WaitStatus(STATUS_DBGACK);
		if (!ok) {
			return;
		}
		// For now, just disable the breakpoint we're using.
		/*
		WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, 0xffffffff);
		WriteICEReg(ARM_ICE_WP0_CONTROL, 0);
		*/
		control = ReadICEReg(0);
		control &= ~(CTRL_DBGRQ);
		control |= (CTRL_INTDIS);
		WriteICEReg(0, control);

		// Set halt reason so we can adjust the PC later.
		HaltReason = HALT_DBGREQ;
	}
	if (bThumb) {
		SwitchToARM();
		// Obtain all registers.
		ReadRegs();

		m_registers[15].value = m_savedPC - (0x04 * THUMB_ADDRESS_SIZE); // 4 addresses added to the PC when switching to ARM
		m_registers[0].value = m_savedR0;
		//printf("SavedPC: 0x%x PC = 0x%x\n", m_savedPC, m_registers[15].value);
		m_targetWasThumb = true;
		
	} else {
		// Get rid of annoying breakpoint.
		WriteICEReg(ARM_ICE_WP0_CONTROL, 0);

		// Obtain all registers.
		ReadRegs();

		// Mask interrupts.
		//MaskInt();

		// Set vector trapping.
		//WriteICEReg(2, 0x1a | 1);

		// Kill the MMU before we start playing around too much!
		//KillMMU();

		// Clear any breakpoint, it'll interfere with singlestepping etc.

		//ClearBreakpoint(0);
		m_targetWasThumb = false;

	}

	// Adjust the program counter accordingly.
	switch (HaltReason) {
		case HALT_DBGREQ:
			m_registers[15].value -= 3 * (m_targetWasThumb?THUMB_ADDRESS_SIZE:ARM_ADDRESS_SIZE);
			break;
		case HALT_BREAKPOINT:
			m_registers[15].value -= 4 * (m_targetWasThumb?THUMB_ADDRESS_SIZE:ARM_ADDRESS_SIZE);
			break;
		case HALT_EXCEPTION:
			m_registers[15].value -= 4 * (m_targetWasThumb?THUMB_ADDRESS_SIZE:ARM_ADDRESS_SIZE);
			break;
		case HALT_INVALID:
		default:
			break;
		
	}

}
//###########################################################################


int
JTAG_arm7tdmi::ReadICEReg(int iReg)
{
	int data[2];

	SelectScanChain(2); // EICE scan-chain
	EnterInstruction(INSTR_INTEST);

	data[0] = 0;
	data[1] = iReg | (0 << 5); // Read reg <iReg>
	ShiftDRBits((unsigned char *)data, NULL, 38);
	ShiftDRBits(NULL, (unsigned char *)data, 38);

	return data[0];
}
//###########################################################################

void
JTAG_arm7tdmi::WriteICEReg(int iReg, int indata)
{
	int data[2];

	SelectScanChain(2); // EICE scan-chain
	EnterInstruction(INSTR_INTEST);

	data[1] = iReg | (1 << 5); // Write reg <iReg>
	data[0] = indata;
	ShiftDRBits((unsigned char *)data, NULL, 38);

}
//###########################################################################

void
JTAG_arm7tdmi::Continue()
{

	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	// try whacking the IRQ's
	//m_registers[16].value |= 0xc0;

	// Reload CPSR
	Execute(LDR_R1_PC, false);
	Execute(NOP, false);
	Execute(NOP, false);
	Execute(m_registers[16].value, false);
	Execute(NOP, false);
	Execute(MSR_R1_CPSR, false);
	Execute(NOP, false);
	Execute(NOP, false);
	Execute(NOP, false);
	Execute(NOP, false);

	// Reload all registers.
	Execute(MOV_IMM(0, 0), false);
	Execute(LDMIA_R0_XX | 0xffff, false);
	Execute(NOP, false);
	Execute(NOP, false);


	if (m_targetWasThumb) {
		//Execute(m_registers[15].value | 1, false);
		Execute(m_registers[15].value | 1, false);
		for (int i = 1; i < 16; i++) {
			Execute(m_registers[i].value, false);
		}

		Execute(NOP, false);
		Execute(BX(0), false);
		Execute(NOP, false);
		Execute(NOP, false);
		// Thumb now.
		Execute(THUMB_NOP, false);
		Execute(THUMB_NOP, false);
		Execute(THUMB_MOV_IMM(0, 0x00), false);
		Execute(THUMB_LDR(0, 0), false);
		Execute(THUMB_NOP, false);
		Execute(THUMB_NOP, false);
		//Execute(m_registers[0].value, false);
		Execute(m_registers[0].value, false);
		Execute(THUMB_NOP, false);

		Execute(THUMB_NOP, false);
		Execute(THUMB_NOP, true);
		Execute(THUMB_B(0x7f6), false);

		// Unmask INTDIS before restart.
		unsigned int control = ReadICEReg(0);
		control &= ~(CTRL_INTDIS);
		WriteICEReg(0, control);
		EnterInstruction(INSTR_RESTART);
	} else {
		for (int i = 0; i < 16; i++) {
			Execute(m_registers[i].value, false);
		}
		Execute(NOP, false);

		// Branch X addresses back.
		Execute(NOP, false);
		//Execute(0xeafffffe, 0, false);
		Execute(NOP, true);
		Execute(0xeafffffc, false); // 4 addresses back. lets assume only one of them are from here.
		// Unmask INTDIS before restart.
		unsigned int control = ReadICEReg(0);
		control &= ~(CTRL_INTDIS);
		WriteICEReg(0, control);
		//  Restart
		EnterInstruction(INSTR_RESTART);
	}
}

//###########################################################################

void 
JTAG_arm7tdmi::KillMMU()
{
}
//###########################################################################


bool
JTAG_arm7tdmi::WaitStatus(int status)
{
	int s = 0;
	for (int i = 0; i != 15; i++) {
		s = ReadICEReg(1);
		if (s & status) {
			return true;
		}
	}
	return false;
}
//###########################################################################



bool
JTAG_arm7tdmi::Halted()
{
	int status = ReadICEReg(1);
	//printf("HaltedStatus: 0x%x\n", ~status);
	return (status & 1)?true:false;
}
//###########################################################################

void 
JTAG_arm7tdmi::MaskInt()
{
	// Try to disable interrupts.
	unsigned long newCPSR = m_registers[16].value | 0xc0;
	Execute(LDR_R1_PC, false);
	Execute(NOP, false);
	Execute(NOP, false);
	Execute(newCPSR, false);
	Execute(NOP, false);
	Execute(MSR_R1_CPSR_C, false);

	Execute(NOP, false); 
	Execute(NOP, false); 
	Execute(NOP, false); 
	Execute(NOP, false); 

}
//###########################################################################
void 
JTAG_arm7tdmi::UnMaskInt()
{

}
//###########################################################################

// This method should only be called when the core was halted through a breakpoint
// Be that a vector trap, watchpoint or a breakpoint.
// Anything but a debugrequest.
void 
JTAG_arm7tdmi::HandleException()
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
			// 
			break;

	}
}
//###########################################################################

void
JTAG_arm7tdmi::GetRegsFromMode(unsigned long cpsr)
{
	// Get back to whichever mode it is, but with IRQ/FIQ disabled.
	unsigned long newCPSR = cpsr | 0xc0;

	Execute(LDR_R1_PC, false);
	Execute(NOP, false);
	Execute(NOP, false);
	Execute(cpsr, false);
	Execute(NOP, false);
	Execute(MSR_R1_CPSR_C, false);

	// This is odd.
	Execute(NOP, false); 
	Execute(NOP, false); 
	Execute(NOP, false); 
	Execute(NOP, false); 

	Execute(STM_R0_XX | (1 << 13) | (1 << 14), false);
	Execute(NOP, false); // Clock 1
	Execute(NOP, false); // Clock 2

	m_registers[13].value = Execute(NOP, false);
	m_registers[14].value = Execute(NOP, false);

	Execute(NOP, false); // Last clock of STM

}
//###########################################################################

void 
JTAG_arm7tdmi::SetBreakpoint(unsigned long Address)
{
	unsigned long WP0Control = (1 << 8)/* | (1 << 4)*/ | ((Address & 0x01)?(1 << 1):0);

	WriteICEReg(ARM_ICE_WP0_ADDRESS, Address & ~1);
	WriteICEReg(ARM_ICE_WP0_ADDRESS_MASK, (Address & 1)?0x1:0x3);
	WriteICEReg(ARM_ICE_WP0_DATA, 0);
	WriteICEReg(ARM_ICE_WP0_DATA_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, (Address & 1)?0xfffffffd:0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL, WP0Control);

/*
	// Only support one breakpoint for now.
	// SW breakpoints can be handled in some other layer above this.

	unsigned long WP0Control = (1 << 8) | (1 << 4);


	WriteICEReg(ARM_ICE_WP0_ADDRESS, Address);
	WriteICEReg(ARM_ICE_WP0_ADDRESS_MASK, 0xfe);
	WriteICEReg(ARM_ICE_WP0_DATA, 0);
	WriteICEReg(ARM_ICE_WP0_DATA_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL, WP0Control);
*/
}
//###########################################################################

void 
JTAG_arm7tdmi::SingleStep()
{
	// On ARM7tdmi_v4 there are no singlestep bit to set.
	// Only way is to either instrument the code, or
	// perhaps, if i'm lucky couple the two breakpoint.
	// This however will make watchpoints etc useless
	// while singlestepping.

	// Backup the current Watchpoint registers.
	unsigned long backup[12];
	backup[0] = ReadICEReg(ARM_ICE_WP1_ADDRESS);
	backup[1] = ReadICEReg(ARM_ICE_WP1_ADDRESS_MASK);
	backup[2] = ReadICEReg(ARM_ICE_WP1_DATA);
	backup[3] = ReadICEReg(ARM_ICE_WP1_DATA_MASK);
	backup[4] = ReadICEReg(ARM_ICE_WP1_CONTROL_MASK);
	backup[5] = ReadICEReg(ARM_ICE_WP1_CONTROL);
	backup[6] = ReadICEReg(ARM_ICE_WP0_ADDRESS);
	backup[7] = ReadICEReg(ARM_ICE_WP0_ADDRESS_MASK);
	backup[8] = ReadICEReg(ARM_ICE_WP0_DATA);
	backup[9] = ReadICEReg(ARM_ICE_WP0_DATA_MASK);
	backup[10] = ReadICEReg(ARM_ICE_WP0_CONTROL_MASK);
	backup[11] = ReadICEReg(ARM_ICE_WP0_CONTROL);



	// Try to program the watchpoint unit to singlestep for us.
	unsigned long mask;
	if (m_targetWasThumb) {
		mask = 0x01;
	} else {
		mask = 0x03;
	}

	WriteICEReg(ARM_ICE_WP1_ADDRESS, GetRegister(15));
	WriteICEReg(ARM_ICE_WP1_ADDRESS_MASK, mask);
	WriteICEReg(ARM_ICE_WP1_DATA, 0);
	WriteICEReg(ARM_ICE_WP1_DATA_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP1_CONTROL_MASK, ~((1 << 3))); // Only tells it to match instruction fetch.
	WriteICEReg(ARM_ICE_WP1_CONTROL, 0);


	WriteICEReg(ARM_ICE_WP0_ADDRESS, 0);
	WriteICEReg(ARM_ICE_WP0_ADDRESS_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_DATA, 0);
	WriteICEReg(ARM_ICE_WP0_DATA_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, ~((1 << 7) | (1 << 3))); // Only tells it to match instruction fetch, and use the range input.
	WriteICEReg(ARM_ICE_WP0_CONTROL, (1 << 8));

	Continue();


	// Wait for system to get back into debug mode.
	bool bHalted = WaitStatus(STATUS_DBGACK);
	if (bHalted) {
		Halt();
	} else {
		//printf("Singlestep timed out.\n");
	}

	// Disable watchpoint chain.
	WriteICEReg(ARM_ICE_WP0_CONTROL, (0 << 8));

	// Restore backup.
	WriteICEReg(ARM_ICE_WP1_ADDRESS,		backup[0]);
	WriteICEReg(ARM_ICE_WP1_ADDRESS_MASK,	backup[1]);
	WriteICEReg(ARM_ICE_WP1_DATA,			backup[2]);
	WriteICEReg(ARM_ICE_WP1_DATA_MASK,		backup[3]);
	WriteICEReg(ARM_ICE_WP1_CONTROL_MASK,	backup[4]);
	WriteICEReg(ARM_ICE_WP1_CONTROL,		backup[5]);
	WriteICEReg(ARM_ICE_WP0_ADDRESS,		backup[6]);
	WriteICEReg(ARM_ICE_WP0_ADDRESS_MASK,	backup[7]);
	WriteICEReg(ARM_ICE_WP0_DATA,			backup[8]);
	WriteICEReg(ARM_ICE_WP0_DATA_MASK,		backup[9]);
	WriteICEReg(ARM_ICE_WP0_CONTROL_MASK,	backup[10]);
	WriteICEReg(ARM_ICE_WP0_CONTROL,		backup[11]);

}
//###########################################################################


void 
JTAG_arm7tdmi::CommChannelWrite(unsigned long data)
{
	unsigned long CommsCTRL;
	while (((CommsCTRL = ReadICEReg(4)) & 0x01)) {
		// Wait..
	}
	WriteICEReg(5, data);
}
//###########################################################################

unsigned long 
JTAG_arm7tdmi::CommChannelRead()
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
JTAG_arm7tdmi::CommChannelWriteStream(unsigned char *indata, int bytes)
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
JTAG_arm7tdmi::CommChannelReadStream(unsigned char *indata, int bytes)
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
JTAG_arm7tdmi::ClearBreakpoint(unsigned long Address)
{
	// For now, just disable the breakpoint we're using.
	WriteICEReg(ARM_ICE_WP0_CONTROL_MASK, 0xffffffff);
	WriteICEReg(ARM_ICE_WP0_CONTROL, 0);
}

//###########################################################################

void
JTAG_arm7tdmi::SwitchToARM()
{
	SelectScanChain(1);
	EnterInstruction(INSTR_INTEST);

	// Test!!
/*
	Execute(THUMB_NOP, false);
	Execute(THUMB_NOP, false);
	Execute(THUMB_NOP, false);
	Execute(THUMB_NOP, true);
	Execute(THUMB_B(0x7f8), false);
	Device()->EnterInstruction(INSTR_RESTART);
	return;
*/
	Execute(THUMB_STR(0, 0), false); // Store r0 first
	Execute(THUMB_NOP, false);
	Execute(THUMB_NOP, false);
	unsigned long r0 = Execute(THUMB_NOP, false);

	Execute(THUMB_MOV(0, 15), false); // Copy PC
	Execute(THUMB_STR(0, 0), false); // Store PC.
	Execute(THUMB_NOP, false);
	Execute(THUMB_NOP, false);
	unsigned long pc = Execute(THUMB_NOP, false);

	Execute(THUMB_MOV_IMM(0, 0), false);
	Execute(THUMB_BX(0), false); // Should switch to ARM here.
	Execute(THUMB_NOP, false);
	Execute(THUMB_NOP, false);

	Execute(NOP, false);
	Execute(NOP, false);

	//nahprintf("PC 0x%x\nR0:0x%x\n", pc, r0);
	m_savedPC = pc;
	m_savedR0 = r0;

}
//###########################################################################

void
JTAG_arm7tdmi::FlushDCache()
{

}
//###########################################################################


void
JTAG_arm7tdmi::FlushICache()
{

}
//###########################################################################


bool
JTAG_arm7tdmi::HasFeature(EFeature feature)
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
JTAG_arm7tdmi::setFlag(unsigned long flag, unsigned long value)
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

unsigned long 
JTAG_arm7tdmi::TranslateAddress(unsigned long MVA)
{
	return MVA;
}


/*
unsigned int 
JTAG_arm7tdmi::ReadCP15(int instruction)
{
	int data[2];
	int odata[2];

	data[0] = 0;
	data[1] = instruction;

	SelectScanChain(15);
	EnterInstruction(INSTR_INTEST);
	ShiftDRBits((unsigned char *)data, 0, 37);
	ShiftDRBits((unsigned char *)data, (unsigned char *)odata, 37);

}
*/
