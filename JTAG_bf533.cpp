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
#include "JTAG_bf533.h"
#include "blackfin.h"
#include "Serial.h"
#include <string.h>

//###########################################################################

// List all registers
bf_reg_data bf_registers[] = 
{
	{REG_R0, "R0", 0, true}, {REG_R1, "R1", 0, true}, {REG_R2, "R2", 0, true}, {REG_R3, "R3", 0, true}, {REG_R4, "R4", 0, true}, {REG_R5, "R5", 0, true}, {REG_R6, "R6", 0, true}, {REG_R7, "R7", 0, true},
	{REG_P0, "P0", 0, true}, {REG_P1, "P1", 0, true}, {REG_P2, "P2", 0, true}, {REG_P3, "P3", 0, true}, {REG_P4, "P4", 0, true}, {REG_P5, "P5", 0, true}, {REG_SP, "SP", 0, true}, {REG_FP, "FP", 0, true},
	{REG_I0, "I0", 0, true},	{REG_I1, "I1", 0, true}, {REG_I2, "I2", 0, true}, {REG_I3, "I3", 0, true}, 
	{REG_M0, "M0", 0, true},	{REG_M1, "M1", 0, true}, {REG_M2, "M2", 0, true}, {REG_M3, "M3", 0, true}, 
	{REG_B0, "B0", 0, true},	{REG_B1, "B1", 0, true}, {REG_B2, "B2", 0, true}, {REG_B3, "B3", 0, true}, 
	{REG_L0, "L0", 0, false},	{REG_L1, "L1", 0, false}, {REG_L2, "L2", 0, false}, {REG_L3, "L3", 0, false}, 
	{REG_A0x, "A0x", 0, false}, {REG_A0w, "A0w", 0, false}, {REG_A1x, "A1x", 0, false}, {REG_A1w, "A1w", 0, false},
	{REG_ASTAT, "ASTAT", 0, false}, {REG_RETS, "RETS", 0, false},
	{REG_LC0, "LC0", 0, false}, {REG_LT0, "LT0", 0, false}, {REG_LB0, "LB0", 0, false}, {REG_LC1, "LC1", 0, false}, {REG_LT1, "LB1", 0, false}, {REG_LB1, "LB1", 0, false}, {REG_CYCLES, "CYCLES", 0, false}, {REG_CYCLES2, "CYCLES2", 0, false},
	{REG_USP, "USP", 0, false}, {REG_SEQSTAT, "SEQSTAT", 0, false}, {REG_SYSCFG, "SYSCFG", 0, false}, {REG_RETI, "RETI", 0, false}, {REG_RETX, "RETX", 0, false}, {REG_RETN, "RETN", 0, false}, {REG_RETE, "RETE", 0, true}, {REG_EMUDAT, "EMUDAT", 0, false}
};

static const int bf_reg_count = sizeof(bf_registers) / sizeof(bf_reg_data);

/*
enum bfin_registers {
        REG_R0    = T_REG_R, REG_R1, REG_R2, REG_R3, REG_R4, REG_R5, REG_R6, REG_R7, 
        REG_P0    = T_REG_P, REG_P1, REG_P2, REG_P3, REG_P4, REG_P5, REG_SP, REG_FP,
        REG_I0    = T_REG_I, REG_I1, REG_I2, REG_I3,
        REG_M0    = T_REG_M, REG_M1, REG_M2, REG_M3, 
        REG_B0    = T_REG_B, REG_B1, REG_B2, REG_B3,
        REG_L0    = T_REG_L, REG_L1, REG_L2, REG_L3, 
        REG_A0x   = T_REG_A, REG_A0w, REG_A1x, REG_A1w,
        REG_ASTAT = T_REG_A + 6, REG_RETS,

        REG_LC0   = T_REG_LP, REG_LT0, REG_LB0,  REG_LC1, REG_LT1, REG_LB1,
                          REG_CYCLES, REG_CYCLES2,

        REG_USP   = T_REG_X, REG_SEQSTAT, REG_SYSCFG,
                          REG_RETI, REG_RETX, REG_RETN, REG_RETE, REG_EMUDAT,
};
*/
int map_dwarf_reg[64] =
{
  REG_R0,
  REG_R1,
  REG_R2,
  REG_R3,
  REG_R4,
  REG_R5,
  REG_R6,
  REG_R7,
  REG_P0,
  REG_P1,
  REG_P2,
  REG_P3,
  REG_P4,
  REG_P5,
  REG_SP, // 0x0e
  REG_FP, // 0x0f
  REG_I0,
  REG_I1,
  REG_I2,
  REG_I3,
REG_M0,
REG_M1,
REG_M2,
REG_M3,
  REG_B0,
  REG_B1,
  REG_B2,
  REG_B3,
  REG_L0,
  REG_L1,
  REG_L2,
  REG_L3,
  REG_A0x,
	REG_A0w,
  REG_A1x,
	REG_A1w,
  REG_ASTAT,
  REG_RETS,
	REG_LC0,
	REG_LT0, 
	REG_LB0,
	REG_LC1,
	REG_LT1, 
	REG_LB1,
	REG_CYCLES,
	REG_CYCLES2,
	REG_USP,
	REG_SEQSTAT,
	REG_ASTAT,
  REG_RETI,
  REG_RETX,
  REG_RETN,
  REG_RETE,
  REG_RETE, /*PC*/
	REG_RETE, /*CC*/
	/* Additional registers
32:text_addr
32:text_end_addr
32:data_addr
32:fdpic_exec
32:fdpic_interp
32:ipend
	*/
  -1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
};


JTAG_bf533::JTAG_bf533() : 
	m_device(NULL),
	m_wpuPower(false),
	m_singleStepEnabled(false),
	m_dataSRAMShadowAddress(0xffffffff),
	m_instructionSRAMShadowAddress(0xffffffff)
{


	m_status = 0;
	//EnterInstruction(BF_TAP_INSTR_DBGCTL);
	m_ctl = BF_DBGCTL_EMPWR;
	//usleep(10000);
	m_ctl |= BF_DBGCTL_EMFEN;
	//usleep(1000);

	m_ctl |= BF_DBGCTL_EMPEN;
	//usleep(1000);

	m_ctl |= (2 << 4);
	//usleep(1000);


	//m_callback.reportDebug("DBGSTAT: 0x%x\n", Status(0));
	//m_callback.reportDebug("DBGCTL: 0x%x\n", Control(m_ctl));
	//Halt();
	//m_callback.reportDebug("DBGSTAT: 0x%x\n", Status(0));
/*
	Device()->EnterInstruction(BF_TAP_INSTR_DBGCTL);
	m_callback.reportDebug("DBGCTL: 0x%x\n", Control(m_ctl));

	m_callback.reportDebug("DBGSTAT: 0x%x\n", Status(0));
	m_callback.reportDebug("IDCODE: 0x%x\n", Device()->GetIDCode());

	setRegister(REG_P0, 0xb4dc0de);
	writeWord(getRegister(REG_SP), 0xb44df00d);
	m_callback.reportDebug("P0: 0x%x\n", getRegister(REG_P0));
	m_callback.reportDebug("SP: 0x%x\n", getRegister(REG_SP));
	m_callback.reportDebug("MEM@0x00 = 0x%x\n", readWord(getRegister(REG_SP)));
*/
	/*
	PushInsn(0x140000, false);
	Device()->EnterInstruction(BF_TAP_INSTR_DBGCTL);
	m_ctl &= ~(BF_DBGCTL_EMEEN | BF_DBGCTL_WAKEUP);
	Control(m_ctl, true);
	m_callback.reportDebug("DBGSTAT: 0x%x\n", Status(0));
*/

	// Initialize watchpoints.
	for (int i = 0; i < BF_N_DATA_WATCHPOINT; ++i) {
		m_datawatchpoints[i].address = 0;
		m_datawatchpoints[i].enabled = false;
		m_datawatchpoints[i].counter = 0;
		m_datawatchpoints[i].data = true;
		m_datawatchpoints[i].index = i;
		m_datawatchpoints[i].write = false;
	}

	for (int i = 0; i < BF_N_INSTRUCTION_WATCHPOINT; ++i) {
		m_insnwatchpoints[i].address = 0;
		m_insnwatchpoints[i].enabled = false;
		m_insnwatchpoints[i].counter = 0;
		m_insnwatchpoints[i].data = true;
		m_insnwatchpoints[i].index = i;
	}


}
//###########################################################################

void
JTAG_bf533::SetDevice(jtag *device)
{
	m_device = device;
	volatile int * volatile dummy = (int *)device;

	// Init stuff
	m_status = 0;
	EnterInstruction(BF_TAP_INSTR_DBGCTL);
	m_ctl = BF_DBGCTL_EMPWR;
	Control(m_ctl);
	// Need a delay.
	for (int i = 0; i < 10000; ++i) {
		m_device = (jtag *)dummy;
	}
	

	m_ctl |= BF_DBGCTL_EMFEN;
	Control(m_ctl);
	//usleep(1000);

	m_ctl |= BF_DBGCTL_EMPEN;
	Control(m_ctl);
	//usleep(1000);

	m_ctl |= (2 << 4);
	Control(m_ctl);
	//usleep(1000);

}


JTAG_bf533::~JTAG_bf533()
{
	// Nothing so far //
}
//###########################################################################

void
JTAG_bf533::EnterInstruction(int insn)
{

	jtag_transaction *t = getTrans();

	*t = jtag_transaction(jtag_transaction::eEnterInstruction, (char *)&insn, NULL, 5, false);

	m_device->Enqueue(t);

	while(!t->done());
	relTrans(t);

}
//###########################################################################

void
JTAG_bf533::ShiftDRBits(unsigned char *in, unsigned char *out, int bits, bool bRTI)
{
	jtag_transaction *t = getTrans();

	*t = jtag_transaction(jtag_transaction::eShiftDR, (char *)in, (char *)out, bits, bRTI); // I think the previous behaviour for ARM was to always run by RTI

	m_device->Enqueue(t);

	while(!t->done());
	relTrans(t);

}
//###########################################################################


unsigned long 
JTAG_bf533::GetRegisterCount()
{
	return 64;
}
//###########################################################################

void 
JTAG_bf533::ReadRegs()
{
	/* It's likely this function won't do as much on BF as on ARM, since
	   we're a bit more free to play around with the regs here
	*/

	for (int i = 0,n = bf_reg_count; i < n; ++i) {
		bf_registers[i].value = getRegister(bf_registers[i].ireg);
	}
}
//###########################################################################

const char * 
JTAG_bf533::GetRegisterName(unsigned long reg)
{
	reg = map_dwarf_reg[reg];

	if (reg <= GetRegisterCount()) {
		int ireg = findReg((bfin_registers)reg);
		if (ireg != -1) {
			return bf_registers[ireg].name;
		}
	}
	return "<none>";
}
//###########################################################################

void 
JTAG_bf533::ReadMem(char *pDest, int Address, int size)
{
	// We need to use different approaches for different memory.
	// For example, the L1-instruction memory can only be accessed through DMA.
	if (Address >= BF_L1_INSTR_SRAM_START && Address <= BF_L1_INSTR_SRAM_END) {
		// Do nothing.
		memset(pDest, 0xfe, size);
		return;
	}
	
	switch (size) {
		case 0:
			return;
			break;
		case 1:
			{
				unsigned long word = readData(1, Address);
				pDest[0] = word & 0xff;
			} return;
		case 2:
			{
				unsigned long word = readData(2, Address);
				pDest[0] = (word) & 0xff;
				pDest[1] = (word >> 8) & 0xff;
			} return;
		case 3: // Who the hell reads 3 bytes?!
			return;
		case 4:
			{
				int word = readData(4, Address);
				*(int *)pDest = word;
			} return;
	
		default: // read a bunch of bytes.
			break;
	}

	int startaddr = Address & ~0x03;
	int endaddr = (Address + size + 0x03) & ~0x03;
	int blocksize = endaddr - startaddr;

	unsigned char *buf = new unsigned char[blocksize];
	readDataBlock(buf, startaddr, blocksize);

	memcpy(pDest, &buf[Address & 0x03], size);
	delete[] buf;
}
//###########################################################################

unsigned long 
JTAG_bf533::GetRegister(int iReg)
{

	//iReg = map_dwarf_reg[iReg];

	if (iReg <= GetRegisterCount() && iReg != -1) {
		int reg = findReg( (bfin_registers)iReg );
		if (-1 == reg) {
			return 0xfefefefe;
		}
		return bf_registers[reg].value;
	}
	return 0xfefefefe;
	
}
//###########################################################################

void 
JTAG_bf533::SetRegister(int iReg, unsigned long value)
{
	//iReg = map_dwarf_reg[iReg];

	int reg = findReg((bfin_registers)iReg);
	if (-1 == reg) {
		return;
	}

	if (bf_registers[reg].restore) {
		bf_registers[reg].value = value;
	} else {
	}
}
//###########################################################################

void 
JTAG_bf533::Halt()
{
	
	m_status = Status(m_status);
	if (m_status & BF_DBGSTAT_EMUACK) {
	}

	PushInsn(BF_INSN_NOP, false);
	//unsigned long ctl = readControl();
	m_ctl |= (BF_DBGCTL_EMEEN | BF_DBGCTL_WAKEUP);
	EnterInstruction(BF_TAP_INSTR_DBGCTL);
	Control(m_ctl, true);

	int retries = 1000;
	while(!(Status(0) & BF_DBGSTAT_EMUREADY) && (retries > 0)) { 
		//sleep(100);
		retries--;
	}
	if (0 == retries) {
		UART_puts("Halt failed!\n\r");
		return;
	}

	m_ctl &= ~BF_DBGCTL_EMEEN; // Not 100% sure this is needed, why disable EMEN?

	getContext();

	// Make sure WPU is alive
	startWPU();

}
//###########################################################################

bool 
JTAG_bf533::Halted()
{
	int stat = Status(0);
	if (stat & BF_DBGSTAT_EMUREADY) {
		return true;
	}
	return false;


}
//###########################################################################

void 
JTAG_bf533::WriteMem(char *pData, int Address, int size)
{
	// We need to use different approaches for different memory.
	// For example, the L1-instruction memory can only be accessed through DMA.
	if (Address >= BF_L1_INSTR_SRAM_START && Address <= BF_L1_INSTR_SRAM_END) {
		// Do nothing.
		return;
	}


	switch (size) {
		case 0: return;
		case 1:
			writeData(1, Address, pData[0]);
			return;
		case 2:
			writeData(2, Address, *(unsigned short *)pData);
			return;
		case 3: break;
		case 4:
			writeData(4, Address, *(unsigned long *)pData);
			return;
	}


	unsigned long pos = 0;
	if (Address & 0x03) {
		// lets write some single bytes
		for (int i = (Address & 0x03); i < 4; i++) {
			writeData(1, (Address & ~0x03) + i, pData[pos++]);
		}
	}

	// Align address.
	Address = (Address + 0x03) & ~0x03;
	unsigned long dataleft = size - pos;
	writeDataBlock((unsigned char *)&pData[pos], Address, (dataleft & ~0x03));
	pos += (dataleft & ~0x03);

	if (dataleft & 0x03) {
		for (int i = 0; i < (dataleft & 0x03); ++i) {
			writeData(1, Address + (dataleft & 0x03) + i, pData[pos++]);
		}

	}

}
//###########################################################################

void 
JTAG_bf533::SystemReset()
{
#ifndef RESET_CORE_ONLY
	// System reset.

	// First update SYSCR
	setRegister(REG_P0, BF_SYSCR);
	setRegister(REG_R0, 0x4);
	PushInsn(generateInsn(eStr16, REG_P0, REG_R0), true);
	PushInsn(BF_INSN_SSYNC, true);


	setRegister(REG_P0, BF_SWRST);
	setRegister(REG_R0, 7);
	PushInsn(generateInsn(eStr16, REG_P0, REG_R0), true);
	PushInsn(BF_INSN_SSYNC, true);

	setRegister(REG_R0, 0);
	PushInsn(generateInsn(eStr16, REG_P0, REG_R0), true);
	PushInsn(BF_INSN_SSYNC, true);
	

#endif
	if (!Halted()) {
		return;
	}

	enableSingleStep();

	
	PushInsn(BF_INSN_RTE, false);

	m_ctl |= BF_DBGCTL_SYSRST;
	Control(m_ctl, true);

	// TODO: NEED SLEEP!!!
	//sleep(1);

	m_ctl &= ~BF_DBGCTL_SYSRST;
	Control(m_ctl, false);

	// Wait for us to return to EMU mode.
	int retries = 100;
	while(!(Status(0) & BF_DBGSTAT_EMUREADY) && (retries > 0)) { 
		// TODO: Sleep
		//sleep(100);
		retries--;
	}
	if (0 == retries) {
		return;
	}

	if (emulationCause() != BF_EMUCAUSE_STEP) {
	}

	// Retrieve context.
	getContext();

	m_wpuPower = false;

	// Make sure WPU is alive.
	startWPU();
}
//###########################################################################

void 
JTAG_bf533::UnMaskInt()
{
}
//###########################################################################

void 
JTAG_bf533::HandleException()
{
	// Due to a bug, watchpoint is not recognized as a cause.
	// And we'll end up with previous cause. So, lets have a look
	// at the watchpoint regs first.

	// Save context before we do anything further.
	//getContext();
	// Context should have been saved because Halt has already been called


	// Disable any turned on watchpoints.
	disableWatchPoints();

	unsigned long wpstat = readWord(BF_WPU_WPSTAT);
	if (wpstat & 0xff) {
		// Looks like we got a watchpoint.
		return;
	}


	switch (emulationCause()) {
		case BF_EMUCAUSE_EMUEXCPT:
			break;
		case BF_EMUCAUSE_EMUIN:
			break;
		case BF_EMUCAUSE_WATCHPT:
			break;
		case BF_EMUCAUSE_PERFMON0:
			break;
		case BF_EMUCAUSE_PERFMON1:
			break;
		case BF_EMUCAUSE_STEP:
			break;
	}
}
//###########################################################################

void 
JTAG_bf533::Continue()
{
	if (!Halted()) {
		return;
	}


	// Re-set the watchpoints.
	refreshWatchPoints();

	// Leave singlestep, if we have it enabled.
	disableSingleStep();

	UART_puts("Returning to: ");
	UART_putHex(GetRegister(REG_RETE));
	UART_puts("\r\n");

	// Restore all registers
	restoreRegisters();

	// Return the control to the core.
	PushInsn(BF_INSN_RTE, false);
	EnterInstruction(BF_TAP_INSTR_DBGCTL);
	m_ctl &= ~(BF_DBGCTL_EMEEN | BF_DBGCTL_WAKEUP);
	Control(m_ctl, true);
	

}
//###########################################################################

void 
JTAG_bf533::MaskInt()
{
}
//###########################################################################

void 
JTAG_bf533::SingleStep()
{
	enableSingleStep();

	UART_puts("Returning to: ");
	UART_putHex(GetRegister(REG_RETE));
	UART_puts("\r\n");

	restoreRegisters();
	PushInsn(BF_INSN_RTE, true);

	// Wait until we're in emulation mode again.
	int retries = 1000;
	while(!(Status(0) & BF_DBGSTAT_EMUREADY) && (retries > 0)) {
		// TODO: Sleep
		//sleep(1);
		retries--;
	}

	if (0 == retries) {
		return;
	}
	// Cache regs etc.
	getContext();

	UART_puts("at: ");
	UART_putHex(GetRegister(REG_RETE));
	UART_puts("\r\n");

	// Get/Show exception status
	HandleException();

}
//###########################################################################

void 
JTAG_bf533::KillMMU()
{
}
//###########################################################################

void 
JTAG_bf533::SetBreakpoint(unsigned long Address)
{
	bf_watchpoint *wp = NULL;

	for (int i = 0; i < BF_N_INSTRUCTION_WATCHPOINT; ++i) {
		if (!m_insnwatchpoints[i].enabled) {
			wp = &m_insnwatchpoints[i];
			break;
		}
	}

	if (NULL == wp) {
		return;
	}

	wp->address = Address;
	wp->enabled = true;

}
//###########################################################################

void 
JTAG_bf533::ClearBreakpoint(unsigned long Address)
{
	for (int i = 0; i < BF_N_INSTRUCTION_WATCHPOINT; ++i) {
		if (m_insnwatchpoints[i].enabled && m_insnwatchpoints[i].address == Address) {
			m_insnwatchpoints[i].enabled = false;
			return;
		}
	}
}
//###########################################################################

void 
JTAG_bf533::CommChannelWriteStream(unsigned char *indata, int words)
{
}
//###########################################################################

unsigned long 
JTAG_bf533::CommChannelRead()
{
	return 0;
}
//###########################################################################

void 
JTAG_bf533::setFlag(unsigned long flag, unsigned long value)
{
	switch (flag) {
	case 0: // Set shadow address (For DATA SRAM)
		m_dataSRAMShadowAddress = value;
		break;
	case 1: // Set shadow address (For INSN SRAM)
		m_instructionSRAMShadowAddress = value;
		break;
	default:
		break;
	}
}

//###########################################################################

void 
JTAG_bf533::CommChannelWrite(unsigned long data)
{
}
//###########################################################################

unsigned long
JTAG_bf533::Status(unsigned long status)
{
	unsigned char in[4];
	unsigned char out[4];

	memset(out, 0, 4);
	memset(in, 0, 4);

	in[0] = status & 0xff;
	in[1] = (status >> 8) & 0xff;

	bitshift((unsigned char *)&status, in, 16);

	EnterInstruction(BF_TAP_INSTR_DBGSTAT);
	ShiftDRBits(in, out, 16, false);

	bitshift(out, in, 16);

	unsigned long ret;
	memcpy(&ret, in, 4);
	return ret;
}
//###########################################################################

unsigned long
JTAG_bf533::Control(unsigned long ctl, bool bRTI)
{
	unsigned char in[4];
	unsigned char out[4];
	memset(out, 0, 4);
	memset(in, 0, 4);

	bitshift((unsigned char *)&ctl, in, 16);

	EnterInstruction(BF_TAP_INSTR_DBGCTL);
	if (bRTI) {
	}
	ShiftDRBits(in, out, 16, bRTI);

	memset(in, 0, 4);
	bitshift(out, in, 16);

	unsigned long ret;
	memcpy(&ret, in, 4);
	return ret;
}
//###########################################################################

unsigned long
JTAG_bf533::PushInsn(unsigned long insn, bool bRTI)
{
	unsigned char out[4];

	memset(out, 0, 4);

	bitshift((unsigned char *)&insn, out, 32);

	EnterInstruction(BF_TAP_INSTR_EMUIR);
	ShiftDRBits(out, NULL, 32, bRTI);
}
//###########################################################################

unsigned long
JTAG_bf533::readEmuDat()
{
	unsigned char out[4];
	unsigned char in[4];

	memset(in, 0, 4);
	memset(out, 0, 4);

	EnterInstruction(BF_TAP_INSTR_EMUDAT);
	ShiftDRBits(NULL, out, 32, true);

	bitshift(out, in, 32);
	

	unsigned long ret;
	memcpy(&ret, in, 4);
	return ret;
}
//###########################################################################

void
JTAG_bf533::writeEmuDat(unsigned long value)
{
	unsigned char in[4];

	memset(in, 0, 4);

	bitshift((unsigned char *)&value, in, 32);

	EnterInstruction(BF_TAP_INSTR_EMUDAT);
	ShiftDRBits(in, NULL, 32, true);
}
//###########################################################################


void
JTAG_bf533::bitshift(unsigned char *in, unsigned char *out, int bits)
{
	int iByte = 0;
	int iShift = 0;

	for (int i = 0; i < bits; ++i) {
		int inbit = (in[iByte] >> iShift) & 1;

		int outbit = (bits - 1) - i;
		int outbyte = (outbit >> 3);
		out[outbyte] |= inbit << (outbit & 0x07);
		iShift++;
		if (iShift == 8) {
			iShift = 0;
			iByte++;
		}

	}

}
//###########################################################################

unsigned long
JTAG_bf533::getRegister(bfin_registers reg)
{
	if (!(Status(0) & BF_DBGSTAT_EMUREADY)) {
		return -1;
	}

	if (reg >= REG_I0) {
		PushInsn(generateInsn(eMove, reg, REG_R0), true);
		PushInsn(generateInsn(eMove, REG_R0, REG_EMUDAT), true);
	} else {
		PushInsn(generateInsn(eMove, reg, REG_EMUDAT), true);
	}


	PushInsn(BF_INSN_NOP, false);
	unsigned long value = readEmuDat();

	if (reg == REG_R0) {
	}

	return value;
}
//###########################################################################
void 
JTAG_bf533::setRegister(bfin_registers reg, unsigned long value)
{

	writeEmuDat(value);
	PushInsn(generateInsn(eMove, REG_EMUDAT, reg), true);
	PushInsn(BF_INSN_NOP, false);

}
//###########################################################################

unsigned long
JTAG_bf533::readWord(unsigned long address)
{
	setRegister(REG_P0, address);
	PushInsn(generateInsn(eLdr, REG_P0, REG_R0), true);
	PushInsn(generateInsn(eMove, REG_R0, REG_EMUDAT), true);
	PushInsn(BF_INSN_NOP, false);
	return readEmuDat();
}
//###########################################################################

void
JTAG_bf533::writeWord(unsigned long address, unsigned long value)
{
	setRegister(REG_P0, address);
	setRegister(REG_R0, value);
	PushInsn(generateInsn(eStr, REG_P0, REG_R0), true);
	PushInsn(BF_INSN_NOP, false);
}
//###########################################################################


void
JTAG_bf533::readDataBlock(unsigned char *buf, unsigned long address, unsigned long size)
{
	unsigned long *ulbuf = (unsigned long *)buf;

	// Size _should_ be word aligned now.
	for (int i = 0; i < (size >> 2); ++i) {
		*ulbuf++ = readWord(address + (i << 2));
	}

}
//###########################################################################

void
JTAG_bf533::writeDataBlock(unsigned char *buf, unsigned long address, unsigned long size)
{
	unsigned long *ulbuf = (unsigned long *)buf;
	unsigned char *lbuf = buf;
		
	// Size _should_ be word aligned now.
	for (int i = 0; i < (size >> 2); ++i) {
		unsigned long word;
		memcpy(&word, lbuf, 4);
		lbuf += 4;
		writeWord(address + (i << 2), word);
	}

}
//###########################################################################

unsigned long
JTAG_bf533::readData(int size, unsigned long address)
{

	EInsnType type;
	switch (size) {
		case 0:return 0xfefefefe;
		case 1: type = eLdr8; break;
		case 2: type = eLdr16; break;
		case 4: type = eLdr; break;
		default: return 0xfefefefe;
	}

	setRegister(REG_P0, address);
	PushInsn(generateInsn(type, REG_P0, REG_R0), true);
	PushInsn(generateInsn(eMove, REG_R0, REG_EMUDAT), true);
	PushInsn(BF_INSN_NOP, false);
	return readEmuDat();
	
}
//###########################################################################


void
JTAG_bf533::writeData(int size, unsigned long address, unsigned long value)
{
	EInsnType type;

	switch (size) {
		case 0:return;
		case 1: type = eStr8; break;
		case 2: type = eStr16; break;
		case 4: type = eStr; break;
		default: return;
	}

	setRegister(REG_P0, address);
	setRegister(REG_R0, value);
	PushInsn(generateInsn(type, REG_P0, REG_R0), true);
	PushInsn(BF_INSN_NOP, false);


}
//###########################################################################

void
JTAG_bf533::restoreRegisters()
{
	for (int i = 0; i < bf_reg_count; ++i) {
		if (bf_registers[i].restore) {
			setRegister(bf_registers[i].ireg, bf_registers[i].value);
		}
	}
}
//###########################################################################

int
JTAG_bf533::findReg(bfin_registers reg)
{
	for (int i = 0; i < GetRegisterCount(); ++i) {
		if (bf_registers[i].ireg == reg) {
			return i;
		}
	}
	return -1;
}
//###########################################################################
unsigned long 
JTAG_bf533::GetPCReg()
{
	return DWARF_REG_RETE;
}
//###########################################################################

unsigned long
JTAG_bf533::emulationCause()
{
	unsigned long status = Status(0);

	return ((status >> 6) & 0x0f);
}
//###########################################################################

void
JTAG_bf533::getContext()
{
	ReadRegs();

	// Cache Data SRAM contents.
	if (m_dataSRAMShadowAddress != 0xffffffff) {
		DMATransfer(m_dataSRAMShadowAddress, 0xff800000, 0x8000); // SRAM Bank A
		DMATransfer(m_dataSRAMShadowAddress + 0x8000, 0xff900000, 0x8000); // SRAM Bank B
		// Scratchpad SRAM can't be accessed with DMA. :(
		//DMATransfer(m_dataSRAMShadowAddress + 0x10000,0xffb00000, 0x1000); // SRAM Scratch.
	}

	// Cache Instruction SRAM contents.
	if (m_dataSRAMShadowAddress != 0xffffffff) {
	}

}
//###########################################################################

void 
JTAG_bf533::SetWatchPoint(unsigned long Address, unsigned long size, unsigned long count, bool bWrite)
{
	bf_watchpoint *wp = NULL;

	for (int i = 0; i < BF_N_DATA_WATCHPOINT; ++i) {
		if (m_datawatchpoints[i].data && !m_datawatchpoints[i].enabled) {
			// We got a watchpoint!
			wp = &m_datawatchpoints[i];
		}
	}

	if (NULL == wp) {
		return;
	}

	wp->address = Address;
	wp->write = bWrite;
	wp->counter = 0;	
	wp->size = 4;
	wp->enabled = true;
}
//###########################################################################

void
JTAG_bf533::refreshWatchPoints()
{
	unsigned long wpdactl = readWord(BF_WPU_WPDACTL);

	// Refresh data watchpoints.
	// TODO: factorise this please.
	for (int i = 0; i < BF_N_DATA_WATCHPOINT; ++i) {
		if (m_datawatchpoints[i].enabled) {
				switch (m_datawatchpoints[i].index) {
					case 0:
						wpdactl &= ~(3 << 8);  // Unmask WPDACC0
						wpdactl &= ~(3 << 6);  // Unmask WPDSRC0
						wpdactl |= (3 << 6);  // Check DAG0, DAG1
						wpdactl |= ((m_datawatchpoints[i].write?1:2) << 8);
						wpdactl |= (1 << 2);	// Enable WPDA0

						if (m_datawatchpoints[i].counter > 0) {
							wpdactl |= (1 << 4); // enable counter.
							// Counter.
							writeWord(BF_WPU_WPDACNT0, m_datawatchpoints[i].counter);
						} else {
							wpdactl &= ~(1 << 4); // disable counter.
						}

						// Set address.
						writeWord(BF_WPU_WPDA0, m_datawatchpoints[i].address);
						// Enable watchpoint.
						writeWord(BF_WPU_WPDACTL, wpdactl);
						break;
					case 1:
						wpdactl &= ~(3 << 12);  // Unmask WPDACC1
						wpdactl &= ~(3 << 10);  // Unmask WPDSRC1
						wpdactl |= (3 << 10);  // Check DAG0, DAG1
						wpdactl |= ((m_datawatchpoints[i].write?1:2) << 12);
						wpdactl |= (1 << 3);	// Enable WPDA1

						if (m_datawatchpoints[i].counter > 0) {
							wpdactl |= (1 << 5); // enable counter.
							// Counter.
							writeWord(BF_WPU_WPDACNT1, m_datawatchpoints[i].counter);
						} else {
							wpdactl &= ~(1 << 5); // disable counter.
						}
						// Set address.
						writeWord(BF_WPU_WPDA1, m_datawatchpoints[i].address);
						// Enable watchpoint.
						writeWord(BF_WPU_WPDACTL, wpdactl);
						break;
				}
		} else { // Disable
		if (m_datawatchpoints[i].data) {
			switch (m_datawatchpoints[i].index) {
				case 0:
					wpdactl &= ~4;
					writeWord(BF_WPU_WPDACTL, wpdactl);
					break;
				case 1: 
					wpdactl &= ~8;
					writeWord(BF_WPU_WPDACTL, wpdactl);
					break;
				}
			}
		}
	}

	
	// Refresh instruction watchpoints.
	unsigned long wpiactl = readWord(BF_WPU_WPIACTL);
	for (int i = 0; i < BF_N_INSTRUCTION_WATCHPOINT; ++i) {
		if (m_insnwatchpoints[i].enabled) {
			switch (m_insnwatchpoints[i].index) {
			case 0:
				wpiactl &= ~((1 << 1) | (1 << 2) | (1 << 5));
				wpiactl |= (1 << 3) | (1 << 7);
				writeWord(BF_WPU_WPIA0, m_insnwatchpoints[i].address);
				break;
			case 1:
				wpiactl &= ~((1 << 2) | (1 << 6));
				wpiactl |= (1 << 4) | (1 << 8);
				writeWord(BF_WPU_WPIA1, m_insnwatchpoints[i].address);
				break;
			case 2:
				wpiactl &= ~((1 << 13) | (1 << 10) | (1 << 9));
				wpiactl |= (1 << 11) | (1 << 15);
				writeWord(BF_WPU_WPIA2, m_insnwatchpoints[i].address);
				break;
			case 3:
				wpiactl &= ~((1 << 14) | (1 << 10) | (1 << 9));
				wpiactl |= (1 << 12) | (1 << 16);
				writeWord(BF_WPU_WPIA3, m_insnwatchpoints[i].address);
				break;
			case 4:
				wpiactl &= ~((1 << 21) | (1 << 17) | (1 << 18));
				wpiactl |= (1 << 19) | (1 << 23);
				writeWord(BF_WPU_WPIA2, m_insnwatchpoints[i].address);
				break;
			case 5:
				wpiactl &= ~((1 << 22) | (1 << 17) | (1 << 18));
				wpiactl |= (1 << 20) | (1 << 24);
				writeWord(BF_WPU_WPIA2, m_insnwatchpoints[i].address);
				break;
			}


		} else {
			switch (m_insnwatchpoints[i].index) {
			case 0:wpiactl &= ~(1 << 3);break;
			case 1:wpiactl &= ~(1 << 4);break;
			case 2:wpiactl &= ~(1 << 11);break;
			case 3:wpiactl &= ~(1 << 12);break;
			case 4:wpiactl &= ~(1 << 19);break;
			case 5:wpiactl &= ~(1 << 20);break;
			}

		}
	}
	writeWord(BF_WPU_WPIACTL, wpiactl);

}
//###########################################################################

void
JTAG_bf533::clearWatchPoints()
{
	for (int i = 0; i < BF_N_DATA_WATCHPOINT; ++i) {
		m_datawatchpoints[i].enabled = false;
	}
}
//###########################################################################

void 
JTAG_bf533::ClearWatchPoint(unsigned long Address)
{
	unsigned long wp = 0xffffffff;

	for (int i = 0; i < BF_N_DATA_WATCHPOINT; ++i) {
		if (m_datawatchpoints[i].address == Address) {
			// This one is free.
			m_datawatchpoints[i].enabled = false;
			break;
		}
	}
}
//###########################################################################
void
JTAG_bf533::disableWatchPoints()
{
	unsigned long wpdactl = readWord(BF_WPU_WPDACTL);

	for (int i = 0; i < BF_N_DATA_WATCHPOINT; ++i) {
		if (m_datawatchpoints[i].enabled) {
			switch(m_datawatchpoints[i].index) {
			case 0:
				wpdactl &= ~4;
				writeWord(BF_WPU_WPDACTL, wpdactl);
				break;
			case 1: 
				wpdactl &= ~8;
				writeWord(BF_WPU_WPDACTL, wpdactl);
				break;
			}
		}
	}

	unsigned long wpiactl = readWord(BF_WPU_WPIACTL);
	for (int i = 0; i < BF_N_INSTRUCTION_WATCHPOINT; ++i) {
		switch (m_insnwatchpoints[i].index) {
			case 0:wpiactl &= ~(1 << 3);break;
			case 1:wpiactl &= ~(1 << 4);break;
			case 2:wpiactl &= ~(1 << 11);break;
			case 3:wpiactl &= ~(1 << 12);break;
			case 4:wpiactl &= ~(1 << 19);break;
			case 5:wpiactl &= ~(1 << 20);break;
		}

	}
	writeWord(BF_WPU_WPIACTL, wpiactl);


}
//###########################################################################


void 
JTAG_bf533::enableSingleStep()
{
	if (m_singleStepEnabled) {
		return;
	}

	m_ctl |= BF_DBGCTL_ESSTEP; // Enable SS
	m_ctl &= ~BF_DBGCTL_EMEEN; // Not 100% sure this is needed, why disable EMEN?


	Control(m_ctl);
	m_singleStepEnabled = true;
}
//###########################################################################

void 
JTAG_bf533::disableSingleStep()
{
	if (!m_singleStepEnabled) {
		return;
	}

	m_ctl &= ~BF_DBGCTL_ESSTEP; // disable SS

	Control(m_ctl);
	m_singleStepEnabled = false;
}
//###########################################################################

void
JTAG_bf533::startWPU()
{
	if (!(Status(0) & BF_DBGSTAT_EMUREADY)) {
		return;
	}

	unsigned long wpiactl = 0;


	wpiactl = BF_WPU_WPPWR;
	writeWord(BF_WPU_WPIACTL, wpiactl);

	// Clear Data watchpoint control reg.
	writeWord(BF_WPU_WPDACTL, 0);

	m_wpuPower = true;
}
//###########################################################################

bool
JTAG_bf533::DMATransfer(unsigned long Destination, unsigned long source, unsigned long size)
{

	// Setup a memory DMA transfer

	unsigned long mdma0_d_cfg = readData(2, MDMA_D0_CONFIG);
	unsigned long mdma0_s_cfg = readData(2, MDMA_S0_CONFIG);

	if ((mdma0_d_cfg & 1) || (mdma0_s_cfg & 1)) {
	}

	// Known state.
	writeData(2, MDMA_D0_CONFIG, 0);
	writeData(2, MDMA_S0_CONFIG, 0);
	writeData(2, MDMA_D0_IRQ_STATUS, 3);
	writeData(2, MDMA_S0_IRQ_STATUS, 3);


	// Setup source first.
	writeWord(MDMA_S0_STARTADDR, source);
	writeWord(MDMA_S0_X_COUNT, size);
	writeWord(MDMA_S0_X_MODIFY, 1);

	writeData(2, MDMA_S0_CONFIG, (1 | (1 << 7)));

	// Setup Destination.
	writeWord(MDMA_D0_STARTADDR, Destination);
	writeWord(MDMA_D0_X_COUNT, size);
	writeWord(MDMA_D0_X_MODIFY, 1);

	writeData(2, MDMA_D0_CONFIG, (3 | (1 << 7)));


	unsigned long status;
	for (int i = 0; i < 10; ++i) {
		status = readData(2, MDMA_S0_IRQ_STATUS);
		if (status & 3) {
			break;
		}
		// TODO: Sleep!
		//usleep(100000); // 100ms
	}

	switch (status & 3) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	}

}
//###########################################################################

int
JTAG_bf533::GetSpecialRegister(target::ESpecialReg reg)
{
	switch (reg) {
		case target::eSpecialRegPC:
			return 0x35;
		case target::eSpecialRegSP:
			return 0x0e;
		case target::eSpecialRegFP:
			return 0x0f;
		default:
			return 0;
	}

	return 0;
}
//###########################################################################


int
JTAG_bf533::MapGDBRegister(int iReg)
{
	if (iReg > 64) {
		iReg = 0;
	}
	iReg = map_dwarf_reg[iReg];
	if (iReg < 0) {
		iReg = 0;
	}

	return iReg;
}

unsigned long
JTAG_bf533::generateInsn(EInsnType type, bfin_registers sourcereg, bfin_registers destreg, unsigned int value)
{

	switch (type) {
		case eMove:
		{
			unsigned long insn = 0x3000;

			// source Register index
			insn |= (sourcereg & 0x07); // 8 regs per group.
			// source Register group
			insn |= ((sourcereg >> 3) & 0x07) << 6; // 8 groups.
			
			// dest Register index
			insn |= ((destreg & 0x07) << 3); // 8 regs per group.
			// dest Register group
			insn |= ((destreg >> 3) & 0x07) << 9; // 8 groups.

			return (insn << 16);

		}
		break;
		case eLdr: // Dreg = [Preg] | Dest = [Source]
		case eStr: // [Preg] = Dreg | [Source] = Dest
		{
			unsigned long insn = 0x9100;

			insn |= (destreg & 0x07);
			insn |= (sourcereg & 0x07) << 3;

			if (type == eStr) {
				insn |= (1 << 9);
			}

			return (insn << 16);

		}
			break;
		case eLdImm16_lo:
		case eLdImm16_hi:
		{
			unsigned long insn = 0xe100;

			if (destreg >= T_REG_A) { // only 2 bits for register group.
				return BF_INSN_NOP;
			}

			insn |= (destreg & 0x07);
			insn |= ((destreg >> 3) & 0x03) << 3;

			if (type == eLdImm16_hi) {
				insn |= (1 << 6);
			}


			insn = (insn << 16) | (value & 0xffff);
			return insn;

		}
		case eLdr16:
		{
			unsigned long insn = 0x9500;

			insn |= destreg & 0x07;
			insn |= (sourcereg & 0x07) << 3;


			return (insn << 16);
		}
		case eStr16:
		{
			unsigned long insn = 0x9700;

			insn |= destreg & 0x07;
			insn |= (sourcereg & 0x07) << 3;

			return (insn << 16);
		}
		case eLdr8:
		{
			unsigned long insn = 0x9900;

			insn |= destreg & 0x07;
			insn |= (sourcereg & 0x07) << 3;

			return (insn << 16);
		}
		case eStr8:
		{
			unsigned long insn = 0x9b00;

			insn |= destreg & 0x07;
			insn |= (sourcereg & 0x07) << 3;

			return (insn << 16);

		}

		break;
			
			break;

	}
}

