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

#ifndef _ARM_H_
#define _ARM_H_

/* Some handy instructions */
#define STM_ALL_REGS	0xE800FFFF // stm r0, {r0-r15}
#define NOP				0xE1A00000 // mov r0, r0 or smth
#define LDR_R0_PC		0xE59F0000 // ldr r0, pc+xx (doesn't really matter what the offset is, since we'll feed the data bus)
#define LDR_R1_PC		0xE59F1000
#define LDR_R0_R0		0xE5900000 // ldr r0, [r0]
#define LDMIA_R0_R1_R8	0xE8B00002 //0xE8B00006// 0xE8900004 //0xE8B001C0 // // 0xE9100002
#define LDMIA_R0_R0_R8	0xE8900ffe //0xE8B00006// 0xE8900004 //0xE8B001C0 // // 0xE9100002
#define LDMIA_R0_XX		0xE8900000
#define STM_R0_XX		0xE8000000
#define LDR_R0_SP		0xE59D0000
#define LDR_R1_R0		0xe4901004 // 0xE5901000
#define LDR_R3_R0		0xE5903000
#define STR_R1_R0		0xE5801000
#define STR_R7_R0		0xE5807000
#define STR_PC_R0		0xE580f000
#define MRS_R1_CPSR		0xE10F1000  // mrs r1, CPSR
#define MRS_R1_SPSR		0xE14F1000	// mrs r1, SPSR
#define MSR_R1_CPSR		0xE12FF001	// msr CPSR_fsxc, r1
#define MSR_R1_CPSR_C	0xE121F001	// msr CPSR_c, r1
#define STMIA_R0_XX		0xE8800000
#define MOV_PC_0		0xE3A0f000

#define STR(source, dest) (0xE5800000 | (source << 12) | (dest << 16))

#define LDRH_R1_R0		0xE1D010B0
#define STRH_R1_R0		0xE1C010B0

#define LDRB_R1_R0		0xE5D01000 // From E5DCC000	ldrb	r12, [r12]

#define STRB_R1_R0		0xE5C01000 // From E4C34001	strb	r4, [r3]

#define MOV_IMM(reg, immed)		(0xE3A00000 | (reg << 12) | immed)

// 0xE3800000
// orr Rd, Rm, #<imm> - Imm should be a 12bit shifter operand.
#define ORR_IMM(Rd, Rn, imm) (0xE3800000 | (Rd << 12) | (Rn << 16) | imm)

// 0xE120001f
// bx pc
#define BX_PC			 0xE120001f

// bx pc
#define BX(reg)			(0xE12FFF10 | reg)

//01100 00000 xxx yyy
// str rX, [rY + 0*4]
#define THUMB_STR(x, y) (((0x6000 | (y << 3) | x) << 16) | (0x6000 | (y << 3) | x))


// 0001110 000 000 000
// mov r0, r0? (is that r8, r8?)
#define THUMB_NOP ((0x1c00 << 16) | (0x1c00))


// 00100 xxx yyyyyyyy
// mov rX, #yy
#define THUMB_MOV_IMM(x, y)	(((0x2000 | (x << 8) | y) << 16) | ((0x2000 | (x << 8) | y)))

// 01000110 H1 H2 xxx yyy
// mov rX, rY
#define THUMB_MOV(x, y)	(((0x4600 | ((x & 0x08) << 4) | (y << 3) | (x & 0x07)) << 16) | (0x4600 | ((x & 0x08) << 4) | (y << 3) | (x & 0x07)))

// 01101 00000 000 000
// ldr rX, [rY + #0]
#define THUMB_LDR(x, y) (((0x6800 | (y << 3) | x) << 16) | (0x6800 | (y << 3) | x))


// 010001110 xxxx 000
// bx rX
#define THUMB_BX(x) (((0x4700 | (x << 3)) << 16) | (0x4700 | (x << 3)))


// 11100 00000000000
// b <x>
#define THUMB_B(x)	(((0xe000 | x) << 16) | (0xe000 | x))


#define ARM_REG_COUNT (16 + 2) // r0-r15, CPSR, SPSR

//unsigned long instruction = (1 << 20) | (0x0e << 28) | (0x0e << 24) | (0x05 << 21) | (0x0f << 16) | (0x00 << 12) | (0x0f << 8) | (0x02 << 5) | (0x01 << 4) | 1;
//p15, 5, r0, c15, c1, 2
	// MRC p15, 5, r0, c15, c1, 2

	// Condition: 0xE << 28
    // instruction: 0xE << 24
	// opcode 1:  0x5 << 21
	// CRn : 0x0f << 16
	// Rd : 0x00 << 12
	// const: 0x0f << 8
	// opcode 2: 2 << 5
	// Const: 1 << 4
	// CRm: 1

#define CP_INSN(L, OpCode1, Rd, CRn, CRm, OpCode2) \
							((L << 20) | (0x0e << 28) | (0x0e << 24) | (OpCode1 << 21) \
							 | (CRn << 16) | (Rd << 12) | (0x0f << 8) | (OpCode2 << 5) | (1 << 4) | (CRm)) \



// SC15 registers (Physical mode)
#define SC15_PHYS_Control	0x2
#define SC15_PHYS_C15State	0x1e



// EICE Status
#define STATUS_SYSSPEED	(1 << 3)
#define STATUS_SYSCOMP	(1 << 3)
#define STATUS_DBGACK	(1)

// EICE Control.
#define CTRL_DBGACK		0x01
#define CTRL_DBGRQ		0x02
#define CTRL_INTDIS		0x04
#define CTRL_SSTEP		0x08


class ARM_Register
{
	public:
		unsigned long value;
		const char *pName;

};

#define ARM_REG_CPSR	16
#define ARM_REG_SPSR	17
#define ARM_CPSR_BIT_THUMB	32



/*
	user: 10000 = 0x10
	FIQ:10001 = 0x11
	IRQ:10010 = 0x12
	svc:10011 = 0x13
	abort: 10111 = 0x17
	undef: 11011 = 0x1b
	system: 11111 = 0x1f
*/

#define ARM_MODE_USR	0x10
#define ARM_MODE_FIQ	0x11
#define ARM_MODE_IRQ	0x12
#define ARM_MODE_SVC	0x13
#define ARM_MODE_ABT	0x17
#define ARM_MODE_UDF	0x1b
#define ARM_MODE_SYS	0x1f


#define ARM_ICE_WP0_ADDRESS			0x8
#define ARM_ICE_WP0_ADDRESS_MASK	0x9
#define ARM_ICE_WP0_DATA			0xa
#define ARM_ICE_WP0_DATA_MASK		0xb
#define ARM_ICE_WP0_CONTROL			0xc
#define ARM_ICE_WP0_CONTROL_MASK	0xd


#define ARM_ICE_WP1_ADDRESS			0x10
#define ARM_ICE_WP1_ADDRESS_MASK	0x11
#define ARM_ICE_WP1_DATA			0x12
#define ARM_ICE_WP1_DATA_MASK		0x13
#define ARM_ICE_WP1_CONTROL			0x14
#define ARM_ICE_WP1_CONTROL_MASK	0x15


#define HALT_INVALID		0
#define HALT_DBGREQ			1
#define HALT_EXCEPTION		2
#define HALT_BREAKPOINT		3

#define ARM_ADDRESS_SIZE	4
#define THUMB_ADDRESS_SIZE	2



#endif // _ARM_H_
