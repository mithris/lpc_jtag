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
#include "Serial.h"
#include "stdio.h"
#include "string.h"
#include "uartStream.h"
#include "jtag.h"
#include "USBSerialStream.h"
#include "gdbparser.h"
#include "Timer.h"
#include "TargetManager.h"
#include "JTAG_arm9tdmi.h"
#include "JTAG_arm7tdmi.h"
#include "JTAG_bf533.h"
#include "debug_out.h"


/*
file <myelf>
set debug remote 1
target remote /dev/ttyACM0
CTRL-C
monitor id/ll/mmuon/mmuoff/targets/test/flash/lpcfreq


LL:
monitor ll@<address>
example:
monitor ll@0x8000




*/


/* PLL Settings
*
NC - 3v
gnd
P0.16 - TDI
P0.17 - TMS
NC    - NC
P0.18 - TCK
P0.19 - TDO
P0.20 - nTRST


Fosc = 12 mhz
M = 60/12 = 5
60 = x / (2 * P)

Fcco = 60 * 2 * P

Fcc = 60 * 2 * 2

M = 4(5)
P = 2


UART SETTINGS
115200 Forsok

60000000 / 16 * 115200 = 32.552


*/
extern "C" {
	void UndefHandler() __attribute__((interrupt("UNDEF")));
	void SWIHandler() __attribute__((interrupt("SWI")));
	
	void PabtHandler() __attribute__((interrupt("PABT")));
	void DabtHandler() __attribute__((interrupt("DABT")));
	void FIQHandler() __attribute__((interrupt("FIQ")));
	void Uart0Handler() __attribute__((interrupt("IRQ")));
	void USBHandler() __attribute__((interrupt("IRQ")));
}

void UndefHandler()
{
	while(1);
}

void SWIHandler()
{
	while(1);

}

void PabtHandler()
{
	while(1);
}
void DabtHandler()
{
	while(1);
}
void FIQHandler()
{
	while(1);
}

void Uart0Handler()
{
	VICVectAddr = 0x00;
}




int getCPSR()
{
	volatile int cpsr = 0xfa9087;

	asm volatile("mrs r0, CPSR \n"
				 "str r0, [%0] \n"
				 :
				 : "r"(&cpsr)
				 :"r0");

	return cpsr;
}

extern char __eheap_start;

extern char TestISR;

uartStream *g_stream;


int main()
{


	// 
	// Setup PLL

	PLLCFG = 4 | (1 << 5);
	PLL48CFG = 3 | (1 << 5);

	PLLCON = 1;

	PLLFEED = 0xaa;
	PLLFEED = 0x55;

	// Wait for the PLL to lock.
	while((PLLSTAT & (1 << 10)) == 0);

	// Connect the PLL
	PLLCON = 3;
	PLLFEED = 0xaa;
	PLLFEED = 0x55;

	MEMMAP = 1; // User flash.
	MAMTIM = 3;
	MAMCR = 2;

	// Set the APB divider
	VPBDIV = 1;

	


	// VIC Default address.
	VICDefVectAddr = (unsigned int)Uart0Handler; // Reset
	VICIntSelect = 0;

	//  Create a UART stream
	g_stream = new uartStream(38400, 0);
	set_debug_stream(g_stream);

	debug_printf("Starting up. Hex: %x string: %s float %f\n\r", 0x1234abcd, "hejsan", 0x1234abcd);

	g_stream->m_dataready = false;

	// Create a USB stream
	USBSerialStream *usbStream = USBSerialStream::instance();

	//  Create JTAG driver
	jtag j;
	j.initialize();


	// Setup the target manager
	TargetManager *tman = TargetManager::instance();
	/*
	HManu: 0x00000787
	revision: 0x00000004
	prodid: 0x0000f1f0
	unsigned long rev = idcode >> 28;
	unsigned long prodid = (idcode >> 12) & 0xffff;
	unsigned long manufacturer = ((idcode >> 1) & 2047); // 11 bits
	*/

	// Register targets.
	tman->registerTarget(new JTAG_arm9tdmi, 0x0032409d, "Samsung S3C2400x");
	tman->registerTarget(new JTAG_bf533(), 0x527a50cb, "JXD 301");
	//tman->registerTarget(new JTAG_arm9tdmi, 0x10920f0f, "MES MP2520F");
	tman->registerDevice(new JTAG_arm9tdmi, 0xffffffff, 0x0920, 0xffffffff, "Generic ARM920");
	tman->registerDevice(new JTAG_arm9tdmi, 0xffffffff, 0x0922, 0xffffffff, "Generic ARM922");
	tman->registerDevice(new JTAG_arm9tdmi, 0xffffffff, 0x0940, 0xffffffff, "Generic ARM940");
	tman->registerDevice(new JTAG_arm7tdmi, 0x04, 0x0f1f0, 0xffffffff, "ARM7TDMI-S (r4)");

	// Create a GDB parser.
	gdbparser p(&j, usbStream);

	
	// BF533 - 	ID:0x327A50CB
	// JXD - 	ID:0x527a50cb
	// LPC213x 	ID:0x4f1f0f0f

	

	char buf[15];
	int counter = 5;
	while(1) {
		
		if (g_stream->dataReady()) {
			int bytes = g_stream->read(buf, 14);
			usbStream->write(buf,bytes);
			//g_stream->write(buf, bytes);
			//g_stream->write("\n\r", 2);
		}

		p.run();
		
	}
	return 0;
}
