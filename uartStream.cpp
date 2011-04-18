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
#include "uartStream.h"
#include "Serial.h"
#include "LPC214x.h"
#include <string.h>

static uartStream *g_instances[2] = {NULL, NULL};

/*
NC - 3v
gnd
P0.16 - TDI
P0.17 - TMS
NC    - NC
P0.18 - TCK
P0.19 - TDO
P0.20 - nTRST
*/

uartStream::uartStream(int baudrate, int port) : 
	m_dataready(false),
	m_buffersize(0)
{
	
	if (0 == port) {
		m_irqmask = 0x40;

	} else {
		m_irqmask = 0x80;
	}
	maskIRQ();
	// Setup the UART0 IRQ, as second highest prio.
	VICVectAddr1 = (unsigned int)uartISR;
	VICVectCntl1 = 0x26;

	InitUart(60000000 / (baudrate * 16));

	g_instances[port] = this;

	unmaskIRQ();
}

uartStream::~uartStream()
{
	// mask IRQ's etc.
}

bool 
uartStream::dataReady()
{
	return m_dataready;
}

int 
uartStream::read(void *buffer, int maxsize)
{
	int size = maxsize;

	if (maxsize > m_buffersize) {
		size = m_buffersize;
	}

	if (size == 0) {
		return 0;
	}

	maskIRQ(); // We don't want to keep filling the buffer just right now.
	memcpy(buffer, m_buffer, size);

	if ((m_buffersize - size) > 0) {
		// Do this as a for-loop, to make sure we don't do any overwriting.
		for (int i = 0; i < (m_buffersize - size); i++) {
			m_buffer[i] = m_buffer[size + i];
		}
	}
	m_buffersize -= size;

	if (0 == m_buffersize) {
		m_dataready = false;
	}

	unmaskIRQ();

	return size;
}


int 
uartStream::write(const void *buffer, int size)
{
	UART_putn((char *)buffer, size);
}

void 
uartStream::uartISR()
{
	uartStream *instance;

	if (VICIRQStatus & 0x40) {
		instance = g_instances[0];
	} else {
		instance = g_instances[1];
	}
	if (NULL == instance) {
		return;
	}

	// Data in.
	while((U0LSR & 0x01) == 1) {
		char c = UART_getChar();
		instance->append(c);
	}
	instance->m_dataready = true;

	VICVectAddr = 0x00;
}

void
uartStream::append(char c)
{
	if (m_buffersize >= DEFAULT_BUFFER_SIZE) { // Bad, buffer overflow.
		return;
	}
	

	m_buffer[m_buffersize++] = c;
}

void
uartStream::maskIRQ()
{
	VICIntEnClr = m_irqmask;
}

void
uartStream::unmaskIRQ()
{

	VICIntEnable = m_irqmask;
}




