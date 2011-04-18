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
#include "Timer.h"
#include "Serial.h"
#include "LPC214x.h"
#include "debug_out.h"
#include <stdlib.h>

static Timer *Timers[2] = {0, 0};



void TimerHandler()
{
	if (T0IR & 0x01) {
		if (Timers[0] != 0) {
			Timers[0]->Callback();
		}
	}
	if (T1IR & 0x01) {
		if (Timers[1] != 0) {
			Timers[1]->Callback();
		}
	}


	if (T0IR & 0x01) {
		T0IR = 0x01;
		//UART_putChar('T');
	}
	if (T1IR & 0x01) {
		T1IR = 0x01;
		//UART_putChar('Q');
	}

	VICVectAddr = 0x00;

}

Timer::Timer(int timer, unsigned long divider, unsigned long match, Functor *callback) : 
	m_timer(timer), 
	m_divider(divider), 
	m_match(match), 
	m_callback(callback)
{

	switch (m_timer) {
		case 0:
			T0TCR = 0; // Disable timer
			T0TCR = 2; // Reset timer.
			T0CTCR = 0; // Increase every rising PCLK edge.
			T0TC = 0; // Reset counter.
			T0PR = m_divider;
			T0MR0 = m_match;
			T0MCR = 1 | (1 << 1); // Interrupt on MR0 match, and reset TC on match.
			T0CCR = 0; // Disable capture.
			T0IR = 1; // Enable MR0 interrupt.

			// Enable interrupt
			VICVectAddr2 = (unsigned int)TimerHandler;
			VICVectCntl2 = 0x24;
			VICIntSelect &= (1 << 4); 
			VICIntEnable |= (1 << 4); 
			Timers[0] = this;
			T0TCR = 0;

			break;
		case 1:
			T1TCR = 0; // Disable timer
			T1TCR = 2; // Reset timer
			T1CTCR = 0; // Increase every rising PCLK edge.
			T1TC = 0; // Reset counter.
			T1PR = m_divider;
			T1MR0 = m_match;
			T1MCR = 1 | (1 << 1); // Interrupt on MR0 match, and reset TC on match.
			T1CCR = 0; // Disable capture.
			T1IR = 1; // Enable MR0 interrupt.

			// Enable interrupt
			VICVectAddr3 = (unsigned int)TimerHandler;
			VICVectCntl3 = 0x25;
			VICIntSelect &= (1 << 5); 
			VICIntEnable |= (1 << 5); 
			Timers[1] = this;
			T1TCR = 0;


			break;
		default:
			return;
	}

}

Timer::~Timer()
{

}

void
Timer::Callback()
{
	if (NULL != m_callback) {
		(*m_callback)();
	}
}

void
Timer::Enable()
{
	switch (m_timer) {
		case 0:
			T0TCR = 1; // Enable timer
			break;
		case 1:
			T1TCR = 1; // Enable timer;
			break;
	}
}

void
Timer::Disable()
{
	switch (m_timer) {
		case 0:
			T0TCR = 0; // Disable timer
			break;
		case 1:
			T1TCR = 0; // Disable timer;
			break;
	}
}

