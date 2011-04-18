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

#include "LPCFlash.h"
#include "ARM.h"
#include "Serial.h"
#include "debug_out.h"

#define LPC_RAM_BASE	(0x40000000)
#define LPC_CALL_ADDR	(LPC_RAM_BASE + 0x100)
#define LPC_CMD_BASE	(LPC_CALL_ADDR + 0x10)
#define LPC_RESULT_BASE	(LPC_CMD_BASE + 0x20)
#define LPC_DATA_AREA	(LPC_RESULT_BASE + 0x10)

#define LPC_CMD_SIZE	(4 * 5)
#define LPC_RESULT_SIZE	(4 * 2)
#define LPC_IAP_LOCATION (0x7ffffff1)
//###########################################################################

unsigned long LPCFlashList[] = {
	4096,	// 0
	4096,	// 1
	4096,	// 2
	4096,	// 3
	4096,	// 4
	4096,	// 5
	4096,	// 6
	4096,	// 7
	32768,	// 8
	32768,	// 9
	32768,	// 10
	32768,	// 11
	32768,	// 12
	32768,	// 13
	32768,	// 14
	32768,	// 15
	32768,	// 16
	32768,	// 17
	32768,	// 18
	32768,	// 19
	32768,	// 20
	32768,	// 21
	4096,	// 22
	4096,	// 23
	4096,	// 24
	4096,	// 25
	4096,	// 26

};

LPCFlash::LPCFlash() : m_target(NULL)
{

}
//###########################################################################


LPCFlash::~LPCFlash()
{

}
//###########################################################################


void
LPCFlash::initialize(target *t)
{
	m_target = t;
}
//###########################################################################


bool
LPCFlash::eraseSector(int sector)
{
	return false;
}
//###########################################################################

bool
LPCFlash::writeFlash(unsigned long address, const unsigned char *data, unsigned long size)
{
	// This may not be the perfect place to do this, but.
	if (address == 0 && size > 0x20) {
		debug_printf("Auto correcting LPC boot checksum\n\r");
		// With this information we can write a new checksum.
		calcChecksum(data, size);
	}


	// Find sector.
	int sector = findSectorContaining(address);
	debug_printf("Sector: [%x] Address: [%x] Data: [%x] ", sector, address, (unsigned int)data);
	unsigned long cmd[5];
	unsigned long ret[2];
	cmd[0] = 50;
	cmd[1] = sector;
	cmd[2] = sector;
	if (!IAPCall(cmd, ret)) {
		debug_printf("[E] IAP Error \n\r");
		return false;
	}
	if (ret[0] != 0) {
		debug_printf("[E] IAP return error code: [%x] \n\r", ret[0]);
		return false;
	}
	debug_printf("Prepared, ");

	// Stuff the targets RAM.
	m_target->WriteMem((char *)data, LPC_DATA_AREA, size);

	cmd[0] = 51;
	cmd[1] = address;
	cmd[2] = LPC_DATA_AREA;
	cmd[3] = 256;
	cmd[4] = m_targetFreq;
	if (!IAPCall(cmd, ret)) {
		debug_printf("[E] IAP Error \n\r");
		return false;
	}
	if (ret[0] != 0) {
		debug_printf("[E] IAP return error code: [%x] \n\r", ret[0]);
		return false;
	}
	debug_printf("Written\n\r");
	
}
//###########################################################################

bool 
LPCFlash::IAPCall(unsigned long *command, unsigned long *result)
{
	if (NULL == m_target || !m_target->Halted()) {
		return false;
	}


	// Write command to target.
	m_target->WriteMem((char *)command, LPC_CMD_BASE, LPC_CMD_SIZE);

	// Setup registers for the call.
	m_target->SetRegister(15, LPC_CALL_ADDR);
	m_target->SetRegister(14, LPC_CALL_ADDR + 4);
	m_target->SetRegister(0, LPC_CMD_BASE);
	m_target->SetRegister(1, LPC_RESULT_BASE);
	m_target->SetRegister(2, LPC_IAP_LOCATION);

	// Remove THUMB bit if there was any.
	// And disable FIQ/IRQ
	unsigned long cpsr = m_target->GetRegister(16);
	//cpsr &= (~(1 << 5));
	//cpsr |= ((1 << 6) | (1 << 7));
	cpsr = 0xd3;
	m_target->SetRegister(16, cpsr);

	unsigned long insns[4];
	insns[0] = BX(2);
	insns[1] = NOP;
	insns[2] = NOP;
	insns[3] = NOP;

	m_target->WriteMem((char *)insns, LPC_CALL_ADDR, 16);

	m_target->SetBreakpoint(LPC_CALL_ADDR + 8);

	// Go!
	m_target->Continue();
	int retries = 12000;
	volatile int *ptr = &retries;
	while(!m_target->Halted() && retries > 0) {
		retries--;
		for (int i = 0; i < 32000; ++i) {
			*ptr = retries;
		}
	}

	// Do what needs to be done.
	m_target->Halt();

	m_target->ReadMem((char *)result, LPC_RESULT_BASE, LPC_RESULT_SIZE);

	return true;
}
//###########################################################################

bool
LPCFlash::eraseFlash(unsigned long start, unsigned long length)
{
	unsigned long addr = start;
	unsigned long end = start + length;
	int startsector = findSectorContaining(addr); 
	int lastsector = startsector;

	// Make sure the PLL is disconnected
	//disconnectPLL();

	while (addr < end) {
		lastsector = findSectorContaining(addr);
		addr += LPCFlashList[lastsector];
		debug_printf("Sector: [%x]", lastsector);
		unsigned long cmd[5];
		unsigned long ret[2];
		cmd[0] = 50;
		cmd[1] = lastsector;
		cmd[2] = lastsector;
		if (!IAPCall(cmd, ret)) {
			debug_printf("[E] IAP Error\n\r");
			return false;
		}
		if (ret[0] != 0) {
			debug_printf("[E] IAP return error code: [%x]\n\r", ret[0]);
			return false;
		}
		debug_printf("Prepared, ");
		
		cmd[0] = 52;
		cmd[1] = lastsector;
		cmd[2] = lastsector;
		cmd[3] = m_targetFreq;
		if (!IAPCall(cmd, ret)) {
			debug_printf("[E] IAP Error\n\r");
			return false;
		}
		if (ret[0] != 0) {
			debug_printf("[E] IAP return error code: [%x]\n\r", ret[0]);
			return false;
		}
		debug_printf("Erased\n\r");
	}

	return true;
}
//###########################################################################

int
LPCFlash::findSectorContaining(unsigned long address)
{
	unsigned long base = 0;

	for (int i = 0; i < 27; ++i) {
		if (address >= base && address < (base + LPCFlashList[i])) {
			return i;
		}
		base += LPCFlashList[i];
	}
	return -1;
}
//###########################################################################

bool
LPCFlash::disconnectPLL()
{
	if (NULL == m_target) {
		return false;
	}

	unsigned long instructions[10];
	/* Regs
	r0 = PLLCON
	r1 = PLLFEED
	r2 = PLLSTAT
	r3 = 0
	r4 = 0xaa
	r5 = 0x55


    */
	m_target->SetRegister(0, 0xe01fc080);
	m_target->SetRegister(1, 0xe01fc08c);
	m_target->SetRegister(2, 0xe01fc088);
	m_target->SetRegister(3, 0);
	m_target->SetRegister(4, 0xaa);
	m_target->SetRegister(5, 0x55);
	m_target->SetRegister(15, LPC_CALL_ADDR);

	instructions[0] = STR(3, 0);
	instructions[1] = STR(4, 1);
	instructions[2] = STR(5, 1);
	instructions[3] = NOP;
	instructions[4] = NOP;
	instructions[5] = NOP;
	instructions[6] = NOP;

	m_target->WriteMem((char *)instructions, LPC_CALL_ADDR, 7 * 4);
	m_target->SetBreakpoint(LPC_CALL_ADDR + 16);

	// Remove THUMB bit if there was any.
	// And disable FIQ/IRQ
	// And set supervisor mode.
	unsigned long cpsr = m_target->GetRegister(16);
	cpsr = 0xd3;
	m_target->SetRegister(16, cpsr);

	debug_printf("Stoppin the PLL\n\r");

	// Go!
	m_target->Continue();
	int retries = 12000;
	volatile int *ptr = &retries;
	while(!m_target->Halted() && retries > 0) {
		retries--;
		for (int i = 0; i < 32000; ++i) {
			*ptr = retries;
		}
	}

	if (retries == 0) {
		debug_printf("Something went wrong when trying to stop the PLL\n\r");
	} else {
		debug_printf("PLL Should now be stopped\n\r");
	}

	// Do what needs to be done.
	m_target->Halt();

}
//###########################################################################

unsigned int 
LPCFlash::calcChecksum(const unsigned char *data, unsigned long size)
{
	/* From the documentation:
		The reserved ARM interrupt vector at 0x00000014 should contain
		the 2's complement to the check-sum of the remaining interrupt vectors.
		This causes the checksum of all the vectors together to be 0.

		
		Example: (Note, little endian data here)
			I: 0x00000000:  0B0000EA 14F09FE5 14F09FE5 14F09FE5
			I: 0x00000010:  14F09FE5 A94FC0B4 F0FF1FE5 0CF09FE5
			I: 0x00000020:  D4020000 E8020000 FC020000 0C030000

    */

	// Data must be properly aligned.
	if ((unsigned)data & 0x03) {
		return 0x0; // This is probably an invalid checksum
	}

	unsigned int *idata = (unsigned int *)data;

	unsigned int sum = idata[0] + idata[1] + idata[2] + idata[3] + idata[4] + idata[6] + idata[7];

	// Create 2's complement.
	unsigned int checksum = ~sum;
	checksum++;

	// Update the data with the checksum
	idata[5] = checksum;

	return checksum;
}

