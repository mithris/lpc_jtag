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
#include <stdlib.h>
#include "LPC214x.h"


void InitUart(int divider)
{
	PINSEL0 = (PINSEL0 & ~0x0000000F) | 0x00000005; /* Enable RxD0 and TxD0              */          


	U0LCR = 0x83; // 8N1, and enable divisor latches.
	U0DLL = divider;
	U0LCR = 0x03; // Disable divisor latches.
	U0IER = 1;
	// FIFO enable
	U0FCR = 1;

	
}


char UART_getChar()
{
	while((U0LSR & 0x01) == 0);
	return U0RBR;

}

void UART_putChar(char c)
{

	while((U0LSR & 0x20) == 0);

	U0THR = c;
}


void UART_puts(const char *str)
{
	while(*str != 0) UART_putChar(*str++);
}

void UART_putHex(int val)
{
	char *lut = "0123456789abcdef";

	UART_puts("0x");
	UART_putChar(lut[(val >> 28) & 0x0f]);
	UART_putChar(lut[(val >> 24) & 0x0f]);
	UART_putChar(lut[(val >> 20) & 0x0f]);
	UART_putChar(lut[(val >> 16) & 0x0f]);
	UART_putChar(lut[(val >> 12) & 0x0f]);
	UART_putChar(lut[(val >> 8) & 0x0f]);
	UART_putChar(lut[(val >> 4) & 0x0f]);
	UART_putChar(lut[(val >> 0) & 0x0f]);

}

void UART_putn(char *buf, int n)
{
	for (int i = 0; i < n; i++) {
		UART_putChar(buf[i]);
	}
}

