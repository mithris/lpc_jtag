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
.text
.arm



@@ Transaction data structure.
@@ 0x00 - Type (0 == ShiftDR, 1==Instruction)
@@ 0x04 - InBits (To shift out TDO)
@@ 0x08 - OutBits (To shift in TDI)
@@ 0x0C - bRTE
@@#define FIO0CLR        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x1C))
@@#define FIO0PIN        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x14))


.equ FIO_BASE_ADDR, 0x3FFFC000
.equ FIO0PIN, (FIO_BASE_ADDR + 0x14)
.equ FIO0SET, (FIO_BASE_ADDR + 0x18)
.equ FIO0CLR, (FIO_BASE_ADDR + 0x1C)

.equ BIT_TDO_SHIFT, 19
.equ BIT_TDI_SHIFT, 16

.equ BIT_TDI, 	(1 << 16)
.equ BIT_TMS, 	(1 << 17)
.equ BIT_TCK, 	(1 << 18)
.equ BIT_TDO, 	(1 << 19)
.equ BIT_nTRST,	(1 << 20)

.equ BIT_TMS_TDI_TCK, (BIT_TDI | BIT_TMS | BIT_TCK)

@@ Send data function.
@@
@@ *r0 - Low: Type(0x00 == ShiftDR, 0x01 == Shift Instruction) Hi: Bits
@@ r0 - Inbits (To TDI)
@@ r1 - Outbits (From TDO)
@@ r2 - bRTE
@@ r3 - bits
@@ r4 - FIOSET
@@ r5 - FIOCLR
@@ r6 - FIOPIN
.global ShiftDR_asm
.align 4
.code 32
ShiftDR_asm:
    stmfd sp!, {r0-r12, r14}

    ldr		r4, =FIO0SET
    ldr		r5, =FIO0CLR
    ldr		r6, =FIO0PIN


    @ Select DR
    @ Clear all bits, TDI, TMS, TCK
    mov		r8, #BIT_TMS_TDI_TCK
    str		r8, [r5]

    @ Pulse TCK
    mov		r8, #BIT_TCK
    str		r8, [r4]
    str		r8, [r5]
    
    @ Capture DR
    str		r8, [r4]
    str		r8, [r5]

    @ Start shifting the data.
    mov		r9, #0x00
.Shift_DR:
    @ Get current word

    @ Word index
    mov		r14, r9, lsr#3
    @ Bit index
    and		r10, r9, #7

    @ Check if we have any TDO buffer
    cmp		r1, #0x00
    beq		.NoOutBits
    @ Get TDO Word from buffer
    ldrb	r8, [r1, r14]
    @ Get TDO
    ldr		r11, [r6]
    mov		r12, #0x01
    mov		r12, r12, lsl r10
    tst		r11, #BIT_TDO
    biceq	r8, r8, r12
    orrne	r8, r8, r12
    strb	r8, [r1, r14]
.NoOutBits:

    cmp		r0, #0x00
    beq		.NoInBits

    @ Get data word.
    ldrb	r11, [r0, r14]
    mov		r11, r11, lsr r10
    tst		r11, #0x01

    @ First, set/clear TDI
    mov		r8, #BIT_TDI
    streq	r8, [r5]
    strne	r8, [r4]

.NoInBits:

    @ Set TMS if this is the last one.
    cmp		r3, #0x01
    moveq	r8, #BIT_TMS
    streq	r8, [r4]

    @ Toggle TCK
    mov		r8, #BIT_TCK
    str		r8, [r4]
    str		r8, [r5]

    add		r9, r9, #0x01
    subs	r3, r3, #0x01
    bne		.Shift_DR

    @@ End of shift loop

    @ Clear TDI
    mov		r8, #BIT_TDI
    str		r8, [r5]

    @ Toggle TCK
    mov		r8, #BIT_TCK
    str		r8, [r4]
    str		r8, [r5]

    
    cmp		r2, #0x00
    beq		.NoRTI

    @ Clear TMS
    mov		r8, #BIT_TMS
    str		r8, [r5]

    @ Toggle TCK
    mov		r8, #BIT_TCK
    str		r8, [r4]
    str		r8, [r5]

.NoRTI:
	    
    @ Set TMS
    mov		r8, #BIT_TMS
    str		r8, [r4]

    @ Toggle TCK
    mov		r8, #BIT_TCK
    str		r8, [r4]
    str		r8, [r5]
    
    @ Pop stuff
    ldmfd sp!, {r0-r12, r14}

    @return
    bx		lr
    


@@ Set TAP instruction
@@
@@ r0 - Inbits (To TDI)
@@ r1 - Outbits (From TDO)
@@ r2 - bRTE
@@ r3 - bits
@@ r4 - FIOSET
@@ r5 - FIOCLR
@@ r6 - FIOPIN
.global EnterInsn_asm
.align 4
.code 32
EnterInsn_asm:

    stmfd sp!, {r0-r12, r14}

    ldr		r4, =FIO0SET
    ldr		r5, =FIO0CLR
    ldr		r6, =FIO0PIN

    @ Set TMS
    mov		r8, #BIT_TMS
    str		r8, [r4]

    @ Toggle TCK
    mov		r9, #BIT_TCK
    str		r9, [r4]
    str		r9, [r5]


    @ Clear TMS
    str		r8, [r5]

    @ Toggle TCK
    str		r9, [r4]
    str		r9, [r5]

    @ Toggle TCK
    str		r9, [r4]
    str		r9, [r5]

    @ At this point, we should be at the Shift-IR state in the TAP controller
    @ Start shifting the data.
    mov		r9, #0x00
.Shift_Insn:
    @ Get current word

    @ Word index
    mov		r14, r9, lsr#3
    @ Bit index
    and		r10, r9, #7

    @ Get data word.
    ldrb	r11, [r0, r14]
    mov		r11, r11, lsr r10
    tst		r11, #0x01

    @ First, set/clear TDI
    mov		r8, #BIT_TDI
    streq	r8, [r5]
    strne	r8, [r4]

    @ Set TMS if this is the last one.
    cmp		r3, #0x01
    moveq	r8, #BIT_TMS
    streq	r8, [r4]

    @ Toggle TCK
    mov		r8, #BIT_TCK
    str		r8, [r4]
    str		r8, [r5]

    add		r9, r9, #0x01
    subs	r3, r3, #0x01
    bne		.Shift_Insn

    @@ End of shift loop

    @ Clear TDI
    mov		r8, #BIT_TDI
    str		r8, [r5]

    @ Toggle TCK
    mov		r8, #BIT_TCK
    str		r8, [r4]
    str		r8, [r5]

    
    cmp		r2, #0x00
    beq		.NoRTI_Insn

    @ Clear TMS
    mov		r8, #BIT_TMS
    str		r8, [r5]

    @ Toggle TCK
    mov		r8, #BIT_TCK
    str		r8, [r4]
    str		r8, [r5]

.NoRTI_Insn:
	    
    @ Set TMS
    mov		r8, #BIT_TMS
    str		r8, [r4]

    @ Toggle TCK
    mov		r8, #BIT_TCK
    str		r8, [r4]
    str		r8, [r5]
    
    @ Pop stuff
    ldmfd sp!, {r0-r12, r14}

    @return
    bx		lr


/*
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
*/
