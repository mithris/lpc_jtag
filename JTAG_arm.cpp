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

#include "JTAG_arm.h"
#include "Serial.h"
#include "debug_out.h"
#include <stdlib.h>
#include <string.h>

//###########################################################################
/*
00000118 <ARM_DCC_Program>:
 118:	e3a00000 	mov	r0, #0	; 0x0
 11c:	e3a01000 	mov	r1, #0	; 0x0

00000120 <DCC_Loop>:
 120:	e3500003 	cmp	r0, #3	; 0x3
 124:	0a00001e 	beq	1a4 <ReadData>
 128:	ee104e10 	mrc	14, 0, r4, cr0, cr0, {0}
 12c:	e3140001 	tst	r4, #1	; 0x1
 130:	0afffffa 	beq	120 <DCC_Loop>

00000134 <WaitCommand>:
 134:	e3500000 	cmp	r0, #0	; 0x0
 138:	1a000002 	bne	148 <WaitAddress>
 13c:	ee111e10 	mrc	14, 0, r1, cr1, cr0, {0}
 140:	e3a00001 	mov	r0, #1	; 0x1
 144:	eafffff5 	b	120 <DCC_Loop>

00000148 <WaitAddress>:
 148:	e3500001 	cmp	r0, #1	; 0x1
 14c:	1a000002 	bne	15c <WaitSize>
 150:	ee112e10 	mrc	14, 0, r2, cr1, cr0, {0}
 154:	e3a00002 	mov	r0, #2	; 0x2
 158:	eafffff0 	b	120 <DCC_Loop>

0000015c <WaitSize>:
 15c:	e3500002 	cmp	r0, #2	; 0x2
 160:	1a000002 	bne	170 <WriteData>
 164:	ee113e10 	mrc	14, 0, r3, cr1, cr0, {0}
 168:	e1a00001 	mov	r0, r1
 16c:	eaffffeb 	b	120 <DCC_Loop>

00000170 <WriteData>:
 170:	e3500004 	cmp	r0, #4	; 0x4
 174:	1affffe7 	bne	118 <ARM_DCC_Program>
 178:	ee114e10 	mrc	14, 0, r4, cr1, cr0, {0}

0000017c <WriteBytes>:
 17c:	e3a05004 	mov	r5, #4	; 0x4
 180:	e1550003 	cmp	r5, r3
 184:	c1a05003 	movgt	r5, r3
 188:	e0533005 	subs	r3, r3, r5
 18c:	03a00000 	moveq	r0, #0	; 0x0

00000190 <WriteByteLoop>:
 190:	e4c24001 	strb	r4, [r2], #1
 194:	e1a04424 	mov	r4, r4, lsr #8
 198:	e2555001 	subs	r5, r5, #1	; 0x1
 19c:	1afffffb 	bne	190 <WriteByteLoop>
 1a0:	eaffffde 	b	120 <DCC_Loop>

000001a4 <ReadData>:
 1a4:	ee104e10 	mrc	14, 0, r4, cr0, cr0, {0}
 1a8:	e3140002 	tst	r4, #2	; 0x2
 1ac:	1affffdb 	bne	120 <DCC_Loop>
// 1ac:	0affffdb 	beq	120 <DCC_Loop>

000001b0 <ReadBytes>:
 1b0:	e3a05004 	mov	r5, #4	; 0x4
 1b4:	e1550003 	cmp	r5, r3
 1b8:	c1a05003 	movgt	r5, r3
 1bc:	e2657004 	rsb	r7, r5, #4	; 0x4
 1c0:	e1a07187 	mov	r7, r7, lsl #3
 1c4:	e0533005 	subs	r3, r3, r5
 1c8:	03a00000 	moveq	r0, #0	; 0x0
 1cc:	e3a04000 	mov	r4, #0	; 0x0

000001d0 <ReadBytesLoop>:
 1d0:	e1a04464 	mov	r4, r4, ror #8
 1d4:	e4d26001 	ldrb	r6, [r2], #1
 1d8:	e1844006 	orr	r4, r4, r6
 1dc:	e2555001 	subs	r5, r5, #1	; 0x1
 1e0:	1afffffa 	bne	1d0 <ReadBytesLoop>
 1e4:	e1a04464 	mov	r4, r4, ror #8
 1e8:	e1a04734 	mov	r4, r4, lsr r7
 1ec:	ee014e10 	mcr	14, 0, r4, cr1, cr0, {0}
 1f0:	eaffffca 	b	120 <DCC_Loop>
*/

unsigned long DCC_Program[] = {
0xe3a00000,// 	mov	r0, #0	; 0x0
0xe3a01000,// 	mov	r1, #0	; 0x0
0xe3500003,// 	cmp	r0, #3	; 0x3
0x0a00001e,// 	beq	1a4 <ReadData>
0xee104e10,// 	mrc	14, 0, r4, cr0, cr0, {0}
0xe3140001,// 	tst	r4, #1	; 0x1
0x0afffffa,// 	beq	120 <DCC_Loop>
0xe3500000,// 	cmp	r0, #0	; 0x0
0x1a000002,// 	bne	148 <WaitAddress>
0xee111e10,// 	mrc	14, 0, r1, cr1, cr0, {0}
0xe3a00001,// 	mov	r0, #1	; 0x1
0xeafffff5,// 	b	120 <DCC_Loop>
0xe3500001,// 	cmp	r0, #1	; 0x1
0x1a000002,// 	bne	15c <WaitSize>
0xee112e10,// 	mrc	14, 0, r2, cr1, cr0, {0}
0xe3a00002,// 	mov	r0, #2	; 0x2
0xeafffff0,// 	b	120 <DCC_Loop>
0xe3500002,// 	cmp	r0, #2	; 0x2
0x1a000002,// 	bne	170 <WriteData>
0xee113e10,// 	mrc	14, 0, r3, cr1, cr0, {0}
0xe1a00001,// 	mov	r0, r1
0xeaffffeb,// 	b	120 <DCC_Loop>
0xe3500004,// 	cmp	r0, #4	; 0x4
0x1affffe7,// 	bne	118 <ARM_DCC_Program>
0xee114e10,// 	mrc	14, 0, r4, cr1, cr0, {0}
0xe3a05004,// 	mov	r5, #4	; 0x4
0xe1550003,// 	cmp	r5, r3
0xc1a05003,// 	movgt	r5, r3
0xe0533005,// 	subs	r3, r3, r5
0x03a00000,// 	moveq	r0, #0	; 0x0
0xe4c24001,// 	strb	r4, [r2], #1
0xe1a04424,// 	mov	r4, r4, lsr #8
0xe2555001,// 	subs	r5, r5, #1	; 0x1
0x1afffffb,// 	bne	190 <WriteByteLoop>
0xeaffffde,// 	b	120 <DCC_Loop>
0xee104e10,// 	mrc	14, 0, r4, cr0, cr0, {0}
0xe3140002,// 	tst	r4, #2	; 0x2
0x1affffdb,// 	bne	120 <DCC_Loop>

//0x0affffdb,// 	beq	120 <DCC_Loop>
0xe3a05004,// 	mov	r5, #4	; 0x4
0xe1550003,// 	cmp	r5, r3
0xc1a05003,// 	movgt	r5, r3
0xe2657004,// 	rsb	r7, r5, #4	; 0x4
0xe1a07187,// 	mov	r7, r7, lsl #3
0xe0533005,// 	subs	r3, r3, r5
0x03a00000,// 	moveq	r0, #0	; 0x0
0xe3a04000,// 	mov	r4, #0	; 0x0
0xe1a04464,// 	mov	r4, r4, ror #8
0xe4d26001,// 	ldrb	r6, [r2], #1
0xe1844006,// 	orr	r4, r4, r6
0xe2555001,// 	subs	r5, r5, #1	; 0x1
0x1afffffa,// 	bne	1d0 <ReadBytesLoop>
0xe1a04464,// 	mov	r4, r4, ror #8
0xe1a04734,// 	mov	r4, r4, lsr r7
0xee014e10,// 	mcr	14, 0, r4, cr1, cr0, {0}
0xeaffffca,// 	b	120 <DCC_Loop>
};

//###########################################################################



JTAG_arm::JTAG_arm() : m_device(NULL), m_targetWasThumb(false)
{

	// Setup register names.
	m_registers[0].pName = "r0";
	m_registers[1].pName = "r1";
	m_registers[2].pName = "r2";
	m_registers[3].pName = "r3";
	m_registers[4].pName = "r4";
	m_registers[5].pName = "r5";
	m_registers[6].pName = "r6";
	m_registers[7].pName = "r7";
	m_registers[8].pName = "r8";
	m_registers[9].pName = "r9";
	m_registers[10].pName = "r10";
	m_registers[11].pName = "r11";
	m_registers[12].pName = "r12";
	m_registers[13].pName = "r13";
	m_registers[14].pName = "r14";
	m_registers[15].pName = "r15";
	m_registers[16].pName = "CPSR";
	m_registers[17].pName = "SPSR";


}

unsigned long 
JTAG_arm::GetRegisterCount()
{
	return ARM_REG_COUNT;
}
//###########################################################################

const char * 
JTAG_arm::GetRegisterName(unsigned long reg)
{
	return m_registers[reg].pName;
}
//###########################################################################

unsigned long 
JTAG_arm::GetRegister(int iReg)
{
	return m_registers[iReg].value;
}
//###########################################################################

void 
JTAG_arm::SetRegister(int iReg, unsigned long value)
{
	// Check if the CPSR gets the T bit set/removed
	// Or, switch to arm mode otherwise.

	if (iReg == 16) { 
		if (value & (1 << 5)) {
			m_targetWasThumb = true;
		}else {
			m_targetWasThumb = false;
		} 
	}

	m_registers[iReg].value = value;

	// When setting a register while the DCC is active, we should also update the backuped registers.
	m_dcc_backup_registers[iReg].value = value;


	debug_printf("SetRegister[%x] = 0x%x\r\n", iReg, value);

}
//###########################################################################

int 
JTAG_arm::GetSpecialRegister(ESpecialReg reg)
{
	switch (reg) {
		case target::eSpecialRegPC:
			return 15;
		case target::eSpecialRegSP:
			return 13;
		case target::eSpecialRegFP:
			return 11;
		default:
			return 0;
	}

}
//###########################################################################

void 
JTAG_arm::GetSWBreakpointInstruction(unsigned char *data, int size)
{
	switch (size) {
		case 2: // Thumb breakpoint
			{
				unsigned short bkpt = 0xdffe;
				memcpy(data, &bkpt, 2);
			}
			break;
		case 4: // ARM breakpoint.
			{
				unsigned int bkpt = 0xe7ffdefe;
				memcpy(data, &bkpt, 4);
			}
			break;
		default:
			memset(data, 0, size);
	}

}
//###########################################################################

unsigned long 
JTAG_arm::GetPCReg()
{
	return -1; // Is this function used?
}
//###########################################################################

int 
JTAG_arm::MapGDBRegister(int iReg)
{
	switch (iReg) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			return iReg;

		case 25: // GDB believes this to be CPSR due to some FP registers beeing in between.
			return 16;

		default:
			return 0;


	}

}
//###########################################################################

void 
JTAG_arm::SetDevice(jtag *device)
{
	m_device = device;
}
//###########################################################################

void 
JTAG_arm::EnterDCCMode(unsigned long address)
{
	// Load DCC program.
	WriteMem((char *)DCC_Program, address, sizeof(DCC_Program));

	UART_puts("DCC Size: ");
	UART_putHex(sizeof(DCC_Program));
	UART_puts("\n\r");

	UART_puts("At: ");
	UART_putHex(address);
	UART_puts("\n\r");


	// Register backup.
	for (int i = 0; i < ARM_REG_COUNT; ++i) {
		m_dcc_backup_registers[i] = m_registers[i];
	}

	// Set PC.
	SetRegister(15, address);

	// Make sure interrupts are masked, and that we're in supervisor mode.
	SetRegister(16, 0xd3);

	// Go!
	Continue();

}
//###########################################################################

void 
JTAG_arm::LeaveDCCMode()
{
	if (Halted()) {
		// This is no good.
		UART_puts("Trying to leave DCC mode, but core alreasy stopped!\n\r");
		return;
	}
	UART_puts("Leaving DCC Mode\n\r");

	// Stop the core.
	Halt();

	// Restore registers
	for (int i = 0; i < ARM_REG_COUNT; ++i) {
		m_registers[i] = m_dcc_backup_registers[i];
	}
}
//###########################################################################


