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
.global _start
.global _startup
.global _bss_start
.global _bss_end
.global TestISR

_startup:
_vectors:

    b _start 			@ Reset
    ldr pc, _undef		@ Undef
    ldr pc, _swi		@ SWI
    ldr pc, _pabt		@ Prefetch Abort
    ldr pc, _dabt		@ Data Abort
    nop				@ Checksum
    ldr pc, [pc, #-0xff0]	@ IRQ
    ldr pc, _fiq		@ FIQ


_undef:	.word	UndefHandler
_swi: 	.word	SWIHandler
_pabt:	.word	PabtHandler
_dabt:	.word	DabtHandler
_fiq:	.word	FIQHandler


_start:
start:

    @ Make sure we're in supervisor mode.
    mov	r0, #0xd3
    msr	CPSR_fsxc, r0

    

    @ Setup a stack.
    mov	r0, #0x40000000
    orr	r0, r0, #0x8000
    sub	r0, r0, #0x04
    mov	r13, r0
    
    @ Store the old mode
    mrs	r1, CPSR
    mov	r2, r1

    @ Setup an IRQ stack.
    mov	r1, #0x12
    orr	r1, r1, #0xc0
    msr	CPSR_fsxc, r1
    sub	r0, r0, #0x2000
    mov	r13, r0

    @ ABT Stack
    mov	r1, #0x17
    orr	r1, r1, #0xc0
    msr	CPSR_fsxc, r1
    sub	r0, r0, #0x250
    mov	r13, r0

    @ FIQ Stack
    @mov	r1, #0x11
    @orr	r1, r1, #0xc0
    @msr	CPSR_fsxc, r1
    @sub	r0, r0, #0x50
    @mov	r13, r0

    @UNDEF stack.
    mov	r1, #0x1b
    orr	r1, r1, #0xc0
    msr	CPSR_fsxc, r1
    sub	r0, r0, #0x250
    mov	r13, r0

    @SYS stack.
    @mov	r1, #0x1f
    @orr	r1, r1, #0xc0
    @msr	CPSR_fsxc, r1
    @sub	r0, r0, #0x100
    @mov	r13, r0

    @ Turn IRQ's on.
    bic	r2, r2, #0xc0
    orr	r2, r2, #0x40
    msr	CPSR_fsxc, r2

    @ Clear the stack
    ldr	r0, =0xdeadbeef
    mov	r1, #0x40000000
    mov	r2, #0x8000
    sub	r2, r2, #0x4
StackClear: str	r0, [r1, +r2]
    subs	r2, r2, #0x04
    bne		StackClear



    @ Clear the BSS
    ldr r0, =_bss_start
    ldr r1, =_bss_end
    sub r2, r1, r0
    mov r3, #0x00
BSSClear:  cmp r0, r1
    strlo r3, [r0], #0x04
    blo BSSClear



    @ Copy the data to RAM.
    ldr	r0, =_etext
    ldr	r1, =_data
    ldr	r2, =_edata
DATACopy: cmp r1, r2
    ldrlo r3, [r0], #0x04
    strlo r3, [r1], #0x04
    blo DATACopy
    

ldr	r10, =main
bx	r10


ARM_DCC_Program:
    
@   // Debug comm control read
@mrc p14, 0, rd, c0, c0, 0

@    // Write to debug comm data register
@mcr p14, 0, rd, c1, c0, 0

@    // read debug comm data register
@mrc p14, 0, rd, c1, c0, 0


@ r0 - state
@    0 - Wait for command
@    1 - Wait for address
@    2 - Wait for Size
@    3 - Read Data
@    4 - Write Data.
@
@ r1 - Current Command
@	0 - Nothing
@	3 - Write Data
@	4 - Read Data
@ r2 - Address
@ r3 - Size

    mov r0, #0x00
    mov	r1, #0x00
DCC_Loop:
    
    @ Check if we're in the Read Data state.
    @ Because then we don't need to wait for incoming data.
    cmp		r0, #0x03
    beq		ReadData

    mrc		p14, 0, r4, c0, c0, 0
    tst		r4, #0x01
    beq		DCC_Loop


WaitCommand:
    cmp		r0, #0x00
    bne		WaitAddress
    mrc		p14, 0, r1, c1, c0, 0

    @ Switch to wait for address state.
    mov		r0, #0x01
    b		DCC_Loop
    

WaitAddress:
    cmp		r0, #0x01
    bne		WaitSize

    @ Fetch address.
    mrc		p14, 0, r2, c1, c0, 0

    @ Switch to wait size state.
    mov		r0, #0x02
    b		DCC_Loop

WaitSize:
    cmp		r0, #0x02
    bne		WriteData

    @ Fetch Size.
    mrc		p14, 0, r3, c1, c0, 0

    @ Switch to current command.
    mov		r0, r1
    b		DCC_Loop

WriteData:
    cmp		r0, #0x04
    bne		ARM_DCC_Program

    @ Fetch Data.
    mrc		p14, 0, r4, c1, c0, 0

WriteBytes:
    
    @ Calculate number of bytes to write.
    mov		r5, #0x04
    cmp		r5, r3
    movgt	r5, r3
    
    @ Back to state 0 if we're done.
    subs	r3, r3, r5
    moveq	r0, #0x00

WriteByteLoop:
    strb	r4, [r2], #0x01
    mov		r4, r4, lsr#8
    subs	r5, r5, #0x01
    bne		WriteByteLoop

    b		DCC_Loop

ReadData:

    @ Check if it's safe to write
    mrc p14, 0, r4, c0, c0, 0

    tst		r4, #0x02
    bne		DCC_Loop

ReadBytes:

    @ Calculate number of bytes to read.
    mov		r5, #0x04
    cmp		r5, r3
    movgt	r5, r3
    rsb		r7, r5, #0x04
    mov		r7, r7, lsl#3
    
    @ Go back to state 0 if we're done after this transfer.
    subs	r3, r3, r5
    moveq	r0, #0x00

    @ Clear data.
    mov		r4, #0x00

ReadBytesLoop:
    mov		r4, r4, ror#8
    ldrb	r6, [r2], #0x01
    orr		r4, r4, r6
    subs	r5, r5, #0x01
    bne		ReadBytesLoop

    @ last rotate
    mov		r4, r4, ror#8

    @ Shift result
    mov		r4, r4, lsr r7

    @ Write result.
    mcr 	p14, 0, r4, c1, c0, 0

    b		DCC_Loop





TestISR:
    b TestISR


mov	r0, #0x00
mov	r1, #0x00
mov	r2, #0x00
mov	r3, #0x00
mov	r4, #0x00
mov	r5, #0x00
mov	r6, #0x00
mov	r7, #0x00
mov	r8, #0x00
mov	r9, #0x00
mov	r10, #0x00

b	l1
ldr	r11, =lt1
add	r11, r11, #0x01
bx	r11

Test:


mov	r0, #0x10
mov	r1, #0x20
mov	r2, #0x30
mov	r3, #0x40
mov	r4, #0x50
mov	r5, #0x60
mov	r6, #0x70
mov	r7, #0x80
mov	r8, #0x90
mov	r9, #0xa0
mov	r10, #0xb0
l1:	b l1
mov	r0, #0x01
mov	r1, #0x02
mov	r2, #0x03
mov	r3, #0x04
mov	r4, #0x05
mov	r5, #0x06
mov	r6, #0x07
mov	r7, #0x08
mov	r8, #0x09
mov	r9, #0x0a
mov	r10, #0x0b
l2:	b l2

.thumb

TestThumb:
mov	r0, #0x10
mov	r1, #0x20
mov	r2, #0x30
mov	r3, #0x40
mov	r4, #0x50
mov	r5, #0x60
mov	r6, #0x70
lt1:	b lt1
mov	r0, #0x01
mov	r1, #0x02
mov	r2, #0x03
mov	r3, #0x04
mov	r4, #0x05
mov	r5, #0x06
mov	r6, #0x07
lt2:	b lt2

