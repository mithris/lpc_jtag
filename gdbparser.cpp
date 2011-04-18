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
#include "gdbparser.h"
#include "jtag.h"
#include "stream.h"
#include "HexConversion.h"
#include "Serial.h"
#include "target.h"
#include "TargetManager.h"
#include "WatchPointManager.h"
#include "config.h"
#include "LPCFlash.h"
#include "LPC214x.h"
#include "debug_out.h"
#include <string>



static const char SupportedString[] = "PacketSize=100;qXfer:memory-map:read+";

static const struct {
	const char *name;
	gdbparser::ERcmd id;
} Rcmds[] = {
	{"mmuoff",	gdbparser::eRcmdMMUOff},
	{"mmuon",	gdbparser::eRcmdMMUOn},
	{"ll",		gdbparser::eRcmdLL},
	{"id",		gdbparser::eRcmdID},
	{"targets",	gdbparser::eRcmdTargets},
	{"test",	gdbparser::eRcmdTest},
	{"flash",	gdbparser::eRcmdFlash},
	{"lpcfreq", gdbparser::eRcmdLPCTargetFreq},
	{NULL,		gdbparser::eRcmdNone}
};



#define SIGNAL_SIGINT	2
//###########################################################################

gdbparser::gdbparser(jtag *device, Stream *data_stream) : 
	m_device(device), 
	m_stream(data_stream),
	m_state(eState_start), 
	m_gotpacket(false),
	m_bIdentified(false),
	m_payloadindex(0),
	m_checksumindex(0),
	m_target(NULL),
	m_id(0x55aa55aa),
	m_instruction_id(0),
	m_reset(jtag_transaction::eReset, NULL, NULL, 0, false),
	m_idcode(jtag_transaction::eShiftDR, NULL, (char*)&m_id, 32, false),
	m_instruction(jtag_transaction::eEnterInstruction, (char*)&m_instruction_id, (char *)&m_previns, 5, false),
	m_treset(jtag_transaction::eTestReset, NULL, NULL, 0, false),
	m_timerTick(0),
	m_bWasRunning(false),
	m_previns(0),
	m_MemTransfered(0),
	m_bEscaped(false),
	m_bUseDCC(false),
	m_bDCCActivated(false),
	m_DCCAddress(0),
	m_flashdriver(NULL),
	m_lpcFreq(0),
	m_flashfifo(FLASH_FIFO_SIZE),
	m_flashhead(0xffffffff),
	m_flashbase(0xffffffff)
{
	m_reset.setDone(true);
	m_idcode.setDone(true);
	//  Setup a timer.
	m_Timer = new Timer(1, 60000, 500, new gdbparserFunctor(this));
	m_Timer->Enable();


}
//###########################################################################


bool
gdbparser::run()
{
	if (m_timerTick > 7) {
		bool bRunning = false;
		if (NULL != m_target && m_bIdentified) {
			bRunning = !m_target->Halted();
			
			if (!bRunning && m_bWasRunning) {
				// Fetch registers etc.
				debug_printf("Stopping!\n\rStatus: [%s]\n\r", bRunning?"Running":"Halted");
				m_target->Halt();
				sendSignal(1);

				// Disable watchpoints.
				m_watchpoints.disable();

				// Enter DCC mode if possible.
				EnterDCCMode();
			}
			m_bWasRunning = bRunning;
		}
		m_timerTick = 0;
		debug_printf("Memory transferred: %x bytes - Status: [%s]\n\r", m_MemTransfered, bRunning?"Running":"Halted");
	}


	while(m_stream->dataReady()) {
		char c;
		char buf[65];
		char *bptr = buf;

		int datasize = m_stream->read(buf, 64);

		if (datasize > 64) {
			while(1);
		}

		while(datasize--) {
			c = *bptr++;
			switch (m_state) {
				case eState_start:
					// Check for a break char.
					if (c == BREAK_CHAR) {
						if (NULL != m_target && m_bIdentified) {
							m_target->Halt();
							EnterDCCMode();
							m_bWasRunning = false;
						}
						sendSignal(SIGNAL_SIGINT);
						continue;
					}
					if (c == '+') {
						continue;
					}
					if (c != '$') {
						continue;
					}
					m_state = eState_payload;
					m_payloadindex = 0;
					m_checksumindex = 0;
	
					break;
				case eState_payload:
					if (c == 0x7d) {
						m_bEscaped = true;
						continue;
					}
	
					if (c != '#') {
						// $# and 0x7d are escaped when doing binary transfers.
						if (m_bEscaped) {
							c ^= 0x20;
							m_bEscaped = false;
						}
	
						m_payload[m_payloadindex++] = c;
					} else {
						m_state = eState_checksum;
					}
					break;
				case eState_checksum:
					m_checksum[m_checksumindex++] = c;
	
					if (m_checksumindex == 2) {
						// This would be a good place to check the checksum, and send a NAK if it wasn't ok.
						m_gotpacket = true;
						m_state = eState_start;
					}
					break;
				default:
					continue;
	
			}
	
			if (m_gotpacket) {
				m_payload[m_payloadindex] = 0;
				sendACK();
				parseCommand();
				m_gotpacket = false;
			}
		}
	}

	return true;
}
//###########################################################################

void
gdbparser::sendACK()
{
	m_stream->write("+", 1);
}
//###########################################################################

void
gdbparser::sendNAK()
{
	m_stream->write("+", 1);
}
//###########################################################################

void
gdbparser::unsupported()
{
	m_stream->write("$#00", 4);
}
//###########################################################################

void
gdbparser::sendOK()
{
	sendGDBPacket("OK");
}
//###########################################################################

void
gdbparser::sendError(int error)
{
	char str[16];

	sprintf(str, "E%.2x", error);
	sendGDBPacket(str);
}
//###########################################################################

void
gdbparser::sendGDBPacket(const char *contents, size_t len)
{
	unsigned int checksum = 0;
	char outbuf[256];
	int index;

	outbuf[0] = '$';
	index = 1;
	while(*contents != 0 && ((len == 0) || (index < len))) {
		checksum += *contents;
		outbuf[index++] = *contents++;
	}
	checksum &= 0xff;
	outbuf[index++] = '#';
	outbuf[index++] = HexConversion::nibbleToHex(checksum >> 4);
	outbuf[index++] = HexConversion::nibbleToHex(checksum);
	m_stream->write(outbuf, index);
}
//###########################################################################

void
gdbparser::parseCommand()
{
	EResult res = eResult_none;

	// We got a command in our payloadbuffer

	switch (m_payload[0]) {
		case '!': // Extended mode.
			res = eResult_UnSupported;
			break;
		case '?': // Last signal.
			// Check if target is running.
			if (NULL == m_target) {
				//sendGDBString("No Target");
				sendTrap(0);
				break;
			} else if (m_target->Halted()) {
				// For some reason, we can't respont with both a message,
				// And a trap, i would like to tell GDB whats up.

				// Fetch registers etc.
				m_target->Halt();

				sendTrap(1);
			} else {
				sendGDBString("Running");
			}
			break;

		case 'A': // Program arguments.
			res = eResult_UnSupported;
			break;
		case 'b': // Set baud.
			res = eResult_UnSupported;
			break;
		case 'B': // Set breakpoint. (apparently deprecated by the 'Z' and 'z' commands.
			res = eResult_UnSupported;
			break;
		case 'c': // Continue.
			if (NULL == m_target) {
				res = eResult_UnSupported;
				break;
			}
			LeaveDCCMode();
			// Enable watchpoints etc.
			m_watchpoints.enable();
			parseContinue();
			break;
		case 'C': // Continue with signal
			if (NULL == m_target) {
				res = eResult_UnSupported;
				break;
			}
			LeaveDCCMode();
			parseContinueWithSignal();
			break;
		case 'd': // Toggle debug.
			res = eResult_UnSupported;
			break;
		case 'D': // Detach, sent by GDB when it detaches.
			break;
		case 'g': // Read registers.
			// Sending only one register, will force GDB to ask for the rest of them
			// individually.
			if (NULL == m_target) {
				res = eResult_Error;
				break;
			}
			sendRegister(0);
			break;
		case 'G': // Write registers
			res = eResult_UnSupported;
			break;
		case 'H': // Set Thread.
			// We don't really care or handle thrads at this level.
			res = eResult_OK;
			break;
		case 'i': // Cycle step.
		case 'I': // Cycle step with signal.
			res = eResult_UnSupported;
			break;
		case 'k': // Kill request.
			res = eResult_UnSupported;
			break;
		case 'm': // Read memory.
			if (NULL == m_target) {
				res = eResult_UnSupported;
				break;
			}
			parseReadMemory();
			break;
		case 'M': // Write memory.
			if (NULL == m_target) {
				res = eResult_UnSupported;
				break;
			}
			parseWriteMemory();
			
			res = eResult_OK;
			break;
		case 'p': // Read register.
			if (NULL == m_target) {
				sendError(0);
				break;
			}
			parseReadRegister();
			break;
		case 'P': // Write register.
			if (NULL == m_target) {
				res = eResult_Error;
				break;
			}
			if (parseWriteRegister()) {
				res = eResult_OK;
			} else {
				res = eResult_Error;
			}
			break;
		case 'q': // Querys.
			if (strncmp(&m_payload[1], "Offsets", strlen("Offsets")) == 0) {
				sendOffsets(0, 0, 0);
			} else if (strncmp(&m_payload[1], "Supported", strlen("Supported")) == 0) {
				sendGDBPacket(SupportedString);
			} else if (strncmp(&m_payload[1], "Rcmd", strlen("Rcmd")) == 0) {
				res = parseRcmd();
			} else if (strncmp(&m_payload[1], "Xfer", strlen("Xfer")) == 0) {
				res = parseXfer();
			} else {
				res = eResult_UnSupported;
			}
			break;
		case 's': // Single step
			res = eResult_none;
			parseSingleStep();
			sendTrap(1);
			break;
		case 'S': // Step with signal.
			res = eResult_UnSupported;
			break;
		case 'v': // IO commands.
			res = parseMulti();
			break;
		case 'X': // Binary write memory
			if (NULL == m_target) {
				res = eResult_Error;
				break;
			}
			// Bit of a hack to speed things up.
			sendOK(); 
			parseWriteMemoryBinary();
			break;
		case 'Z':
		case 'z':
			//unsupported();
			res = parseWatchpoint();
			break;
		default:
			unsupported();
	}

	switch (res) {
		case eResult_UnSupported:
			unsupported();
			break;
		case eResult_OK:
			sendOK();
			break;
		case eResult_Error:
			sendError(0);
			break;

	}
}
//###########################################################################

void
gdbparser::sendSignal(int signal)
{
	char sig[4];

	sig[0] = 'S';
	HexConversion::intToHex(signal, &sig[1], 2);
	sig[3] = 0;
	sendGDBPacket(sig);
}
//###########################################################################

void 
gdbparser::sendTrap(int trap)
{
	char buffer[128];
	int pcreg = 0x0f; // TODO: ask target which register is the PC
	int fpreg = 0x0b; // TODO: ask target which register is the FP
	int spreg = 0x0d; // TODO: ask target which register is the SP

	if (NULL == m_target) {
		sprintf(buffer, "T%.2x%.2x:%.8x;%.2x:%.8x;%.2x:%.8x;", trap, pcreg, 0xcccccccc, fpreg, 0xffffffff, spreg, 0xaaaaaaaa); // TODO: fetch the real regs.
		sendGDBPacket(buffer);
		return;
	}

	pcreg = m_target->GetSpecialRegister(target::eSpecialRegPC);
	fpreg = m_target->GetSpecialRegister(target::eSpecialRegFP);
	spreg = m_target->GetSpecialRegister(target::eSpecialRegSP);

	unsigned long pcregval = SwapEndian(m_target->GetRegister(m_target->MapGDBRegister(pcreg)));
	unsigned long fpregval = SwapEndian(m_target->GetRegister(m_target->MapGDBRegister(fpreg)));
	unsigned long spregval = SwapEndian(m_target->GetRegister(m_target->MapGDBRegister(spreg)));

	sprintf(buffer, "T%.2x%.2x:%.8x;%.2x:%.8x;%.2x:%.8x;", trap, 
			pcreg, pcregval, 
			fpreg, fpregval, 
			spreg, spregval );
	sendGDBPacket(buffer);

}
//###########################################################################


void
gdbparser::sendOffsets(unsigned long text, unsigned long data, unsigned long bss)
{
	//`Text=xxx;Data=yyy;Bss=zzz'
	char reply[128];

	text = SwapEndian(text);
	data = SwapEndian(data);
	bss = SwapEndian(bss);

	sprintf(reply, "Text=%.8x;Data=%.8x;Bss=%.8x", text, data, bss);
	sendGDBPacket(reply);
}
//###########################################################################

void
gdbparser::sendRegisters()
{
	char buf[8*26 + 1];

	for (int i = 0; i < 26; ++i) {
		sprintf(&buf[i*8], "%.8x", i);
	}
	sendGDBPacket(buf);
}
//###########################################################################


void
gdbparser::sendRegister(int reg)
{
	char buf[9];

	if (NULL == m_target) {
		// WE shouldn't even get to this point unless we have a device i think.
		sprintf(buf, "%.8x", 0xfefefefe);
		sendGDBPacket(buf);

	}

	unsigned long value = m_target->GetRegister(m_target->MapGDBRegister(reg));
	value = SwapEndian(value);
	sprintf(buf, "%.8x", value);
	sendGDBPacket(buf);
}
//###########################################################################


void
gdbparser::sendGDBString(const char *str)
{
	char outbuf[MAX_GDB_STRING_LENGTH+10];

	int n = 1;
	outbuf[0] = 'O';
	
	while(*str != 0 && n < (MAX_GDB_STRING_LENGTH - 1)) {
		outbuf[n++] = HexConversion::nibbleToHex((*str >> 4) & 0x0f);
		outbuf[n++] = HexConversion::nibbleToHex(*str & 0x0f);
		str++;
	}
	outbuf[n] = 0;

	sendGDBPacket(outbuf);
}
//###########################################################################

void
gdbparser::TimerTick()
{
	// Increase our counter.
	m_timerTick++;


	if (NULL == m_device) {
		return;
	}

	//  See if we recognize the IDcode.
	if (m_idcode.done() && !m_bIdentified) {
		target *t = TargetManager::instance()->getTarget(m_id);
		if (NULL != t) {
			debug_printf("Identified: [%s]\n\r", (char *)TargetManager::instance()->getTargetName(m_id));
			m_bIdentified = true;
			m_target = t;
			m_target->SetDevice(m_device);
			if (m_flashdriver) m_flashdriver->initialize(m_target);
			m_watchpoints.attach(m_target);
			//m_target->setFlag(0, 1); // Turn on break on exceptions.
		} else {
			debug_printf("Unidentified: ID [%x]\n\r", m_id);
			m_previns = 0x0;
			m_id = 0;
		}
	}

	// Keep sending reset and read the IDCODE from the device.
	if (!m_bIdentified && m_idcode.done() && m_reset.done()) {
		m_device->Enqueue(&m_reset);
		m_device->Enqueue(&m_treset);
		m_device->Enqueue(&m_idcode);
		debug_printf("ID: [%x]\n\r", m_id);
	}
}
//###########################################################################

void
gdbparser::parseContinue()
{	
	// Continue can have an optional address.
	// $cAddress#checksum
	
	if (m_payload[1] != 0) { // payload[0] is 'c' ofcourse.
		unsigned long address = strtoul(&m_payload[1], NULL, 16);
		m_target->SetRegister(m_target->GetSpecialRegister(target::eSpecialRegPC), address);
	}

	m_target->Continue();
}
//###########################################################################


void
gdbparser::parseContinueWithSignal()
{	
	// We Don't really care about no signals.
	// The command looks like $Cxx;aaaa#yy
	// We can skip the two bytes for signal and just see if we have an address

	if (m_payload[3] != 0) { // payload[0] is 'c' ofcourse.
		unsigned long address = strtoul(&m_payload[4], NULL, 16);
		m_target->SetRegister(m_target->GetSpecialRegister(target::eSpecialRegPC), address);
	}

	m_target->Continue();
}
//###########################################################################

void
gdbparser::parseReadMemory()
{
	// $mAddr,Length#Checksum

	unsigned long address = strtoul(&m_payload[1], NULL, 16);

	char *lenPtr = strchr(&m_payload[1], ',');

	if (NULL == lenPtr) {
		return;
	}

	char outbuf[MAX_MEM_READ * 2 + 1];
	char *p = outbuf;
	{ // To be a bit nicer on the stack.
		char buf[MAX_MEM_READ];

		unsigned long length = strtoul(&lenPtr[1], 0, 16);

		if (length > MAX_MEM_READ) length = MAX_MEM_READ;

		ReadMemory(buf, address, length);
		//m_target->ReadMem(buf, address, length);

		// Create a GDB packet.
		for (int i = 0; i < length; ++i) {
			*p++ = HexConversion::nibbleToHex((buf[i] >> 4) & 0x0f);
			*p++ = HexConversion::nibbleToHex((buf[i]) & 0x0f);
		}
	}
	*p = 0;
	sendGDBPacket(outbuf);

}
//###########################################################################

void
gdbparser::parseWriteMemory()
{
	// $mAddr,Length:Data#Checksum

	unsigned long address = strtoul(&m_payload[1], NULL, 16);

	// Find the Address<->Length delimiter
	char *lenPtr = strchr(&m_payload[1], ',');

	if (NULL == lenPtr) {
		return;
	}

	// Find the Length<->Data delimiter
	unsigned long length = strtoul(&lenPtr[1], 0, 16);

	char *dataPtr = strchr(lenPtr, ':');

	if (NULL == dataPtr) {
		return;
	}

	// Fetch data from payload.
	char outbuf[MAX_MEM_READ];
	int outindex = 0;
	dataPtr++;
	while(*dataPtr != 0 && outindex < MAX_MEM_READ) {
		char byte = 0;
		byte = HexConversion::hexToNibble((*dataPtr++)) << 4;
		byte |= HexConversion::hexToNibble((*dataPtr++));
		outbuf[outindex++] = byte;
	}

	// Update statistics.
	m_MemTransfered += length;



	// Write to target.
	//m_target->WriteMem(outbuf, address, length);
	WriteMemory(outbuf, address, length);

}
//###########################################################################

void
gdbparser::parseReadRegister()
{
	// $pREG#checksum

	unsigned long reg = strtoul(&m_payload[1], NULL, 16);

	sendRegister(reg);
}
//###########################################################################

bool
gdbparser::parseWriteRegister()
{
	// $Preg=value#checksum

	unsigned long reg = strtoul(&m_payload[1], NULL, 16);

	char *valPtr = strchr(m_payload, '=');

	unsigned long value = strtoul(&valPtr[1], NULL, 16);

	value = SwapEndian(value);

	

	m_target->SetRegister(m_target->MapGDBRegister(reg), value);

}
//###########################################################################

unsigned long
gdbparser::SwapEndian(unsigned long value)
{
	typedef union {
		unsigned long ival;
		char cval[4];
	} uval;

	uval inval;
	uval outval;

	inval.ival = value;

	outval.cval[0] = inval.cval[3];
	outval.cval[1] = inval.cval[2];
	outval.cval[2] = inval.cval[1];
	outval.cval[3] = inval.cval[0];

	return outval.ival;

}
//###########################################################################

void
gdbparser::parseWriteMemoryBinary()
{
	// $XAddr,Length:Data#Checksum

	unsigned long address = strtoul(&m_payload[1], NULL, 16);

	// Find the Address<->Length delimiter
	char *lenPtr = strchr(&m_payload[1], ',');

	if (NULL == lenPtr) {
		return;
	}

	// Find the Length<->Data delimiter
	unsigned long length = strtoul(&lenPtr[1], 0, 16);

	char *dataPtr = strchr(lenPtr, ':');

	if (NULL == dataPtr) {
		return;
	}

	dataPtr++;
	

	// Update statistics.
	m_MemTransfered += length;
/*
	UART_puts("M ");
	UART_putHex(address);
	UART_puts(" L ");
	UART_putHex(length);
	UART_puts(" D ");
	UART_putHex(*(int *)dataPtr);
	UART_puts("\r\n");
*/

	// Write to target.
	//m_target->WriteMem(dataPtr, address, length);
	// 

	WriteMemory(dataPtr, address, length);

}

//###########################################################################
gdbparser::EResult
gdbparser::parseRcmd()
{
	char *payloadPtr = strchr(&m_payload[1], ',');

	if (NULL == payloadPtr) {
		return eResult_Error;
	}

	// Skip the comma.
	payloadPtr++;

	// Decode the command.
	char command[MAX_GDB_STRING_LENGTH];

	char *cmdPtr = command;
	int len = 0;
	while(*payloadPtr != 0 && len < MAX_GDB_STRING_LENGTH) {
		*cmdPtr = HexConversion::hexToNibble(*payloadPtr++) << 4;
		*cmdPtr++ |= HexConversion::hexToNibble(*payloadPtr++);
	}

	// Terminate command.
	*cmdPtr = 0;

	debug_printf("Remote Command: [%s] \n\r", command);

	int iCmdIndex = 0;
	while(Rcmds[iCmdIndex].name != NULL) {
		if (strncmp(command, Rcmds[iCmdIndex].name, strlen(Rcmds[iCmdIndex].name)) == 0) {
			runRcmd(Rcmds[iCmdIndex].id, command);
			return eResult_OK;
		}
		iCmdIndex++;
	}

	return eResult_UnSupported;
}
//###########################################################################


void
gdbparser::runRcmd(ERcmd cmd, const char *data)
{
	switch (cmd) {
		case eRcmdNone:
			break;
		case eRcmdMMUOff:
			if (NULL != m_target && m_bIdentified) {
				debug_printf("Killing MMU\r\n");
				m_target->KillMMU();
				debug_printf("Masking interrupts\r\n");
				m_target->MaskInt();
			}
			break;
		case eRcmdMMUOn:
			break;
		case eRcmdLL:
			// Here we have some additional parsing to do.
			// Command should look like "remote LL@0x8000" or smth.
			{
				char *adrPtr = strchr(data, '@');
				if (NULL == adrPtr) {
					return;
				}
				// Skip the '@'
				adrPtr++;

				unsigned long address = strtoul(adrPtr, NULL, 16);
				debug_printf("Loader at: [%x]\n\r", address);

				if (NULL != m_target && 
					m_bIdentified && 
					m_target->HasFeature(target::eFeatureDCC)) 
				{
					m_bUseDCC = true;
					m_DCCAddress = address;
					// Enter DCC mode if the target is halted.
					if (m_target->Halted()) {
						EnterDCCMode();
					} else {
						debug_printf("[W] - Target not halted\n\r");
					}
				}

			}
			break;
		case eRcmdID:
			{
				char buf[32];
				memset(buf, 0, 32);
				HexConversion::intToHex(m_id, buf, 8, true);
				if (m_bIdentified) sendGDBString("IDCode: ");
				else sendGDBString("Unidentified IDCode: ");
				sendGDBString(buf);
				sendGDBString("\n");
				
			}
			break;
		case eRcmdTargets:
			{
				for (int i = 0; i < TargetManager::instance()->getMaxNumTargets(); ++i) {
					sendGDBString(TargetManager::instance()->getTargetNameFromIndex(i));
					sendGDBString("\n");
				}
			}
			break;
		case eRcmdTest:
			{
				unsigned long pllcfg;// = PLLCFG;
				m_target->ReadMem((char *)&pllcfg, 0xe01fc084, 4);

				unsigned long msel = pllcfg & 0x1f;
				unsigned long psel = (pllcfg  >> 5) & 0x03;
				unsigned long cclk = (msel + 1) * m_lpcFreq;

				debug_printf("msel: %x\n\rpsel: %s\n\rcclk: %x\n\r", msel, psel, cclk);
			}
			break;
		case eRcmdFlash: // Set flash driver
			{
				// "flash"
				const char *driverptr = &data[5];

				// Clear whitespaces
				while(((*driverptr == ' ') || (*driverptr == '\t')) && *driverptr != 0) driverptr++;

				if (strncmp(driverptr, "lpc", strlen("lpc")) == 0) {
					// Create a LPC flash driver.
					sendGDBString("Creating LPC Flash driver\r\n");
					LPCFlash *driver = new LPCFlash;
					driver->initialize(m_target);
					driver->setTargetFreq(m_lpcFreq);
					m_flashdriver = driver;
					return;
				}
				sendGDBString("Unknown flash driver\n");

				debug_printf("%s \n\r", driverptr);
			}
			break;
		case eRcmdLPCTargetFreq:
			{
				// LPCFreq <number>
				const char *ptr = &data[7];
				m_lpcFreq = strtoul(ptr, NULL, 10);
				debug_printf("LPCFreq set to: [%x]kHz\n\r", m_lpcFreq);
			}
			break;
	}
}

//###########################################################################
void
gdbparser::EnterDCCMode()
{
	if (m_bUseDCC && !m_bDCCActivated) {
		if (m_bIdentified && NULL != m_target) {
			m_target->EnterDCCMode(m_DCCAddress);
			m_bDCCActivated = true;
			debug_printf("DCC activated\n");
		}
	}
}
//###########################################################################

void
gdbparser::LeaveDCCMode()
{
	if (m_bUseDCC && m_bDCCActivated) {
		if (m_bIdentified && NULL != m_target) {
			m_target->LeaveDCCMode();
			m_bDCCActivated = false;

			/* Disable DCC aswell, reason for this is that we can't be sure
			the DCC address is available after the program have stopped.
		
			Could be argued it's up to the user to make sure of that, and 
			perhaps a setting should be added to control this behaviour.
			*/
			m_bUseDCC = false;
		}
	}
}
//###########################################################################

void 
gdbparser::ReadMemory(void *dest, unsigned long address, unsigned long size)
{
	if (size <= 0) {
		return; // It happens that GDB wants to 'probe' memory write/read feature with a 0 bytes read.
	}

	if (m_bDCCActivated) {
		m_target->CommChannelWrite(3); // Command for reading data
		m_target->CommChannelWrite(address); // Address
		m_target->CommChannelWrite(size); // Size
		m_target->CommChannelReadStream((unsigned char *)dest, size);
	} else {
		m_target->ReadMem((char *)dest, address, size);
	}
}
//###########################################################################
void 
gdbparser::WriteMemory(void *src, unsigned long address, unsigned long size)
{
	if (size <= 0) {
		return; // It happens that GDB wants to 'probe' memory write/read feature with a 0 bytes read.
	}

	if (m_bDCCActivated) {
		m_target->CommChannelWrite(4); // Command for writing data
		m_target->CommChannelWrite(address); // Address
		m_target->CommChannelWrite(size); // Size
		m_target->CommChannelWriteStream((unsigned char *)src, size);
	} else {
		m_target->WriteMem((char *)src, address, size);
	}
}
//###########################################################################

gdbparser::EResult
gdbparser::parseWatchpoint()
{
	// Packet looks like: $[Zz][0-4],[0-9a-fA-F]+,[0-9a-fA-F]#

	bool bSet = m_payload[0] == 'Z';
	WatchPointManager::EType type;

	switch (m_payload[1]) {
		case '0': // Memory breakpoint.
			type = WatchPointManager::eType_MemoryBreakpoint;
			break;
		case '1': // Hardware breakpoint.
			type = WatchPointManager::eType_HardwareBreakpoint;
			break;
		case '2': // Read Watchpoint.
			type = WatchPointManager::eType_ReadWatchpoint;
			break;
		case '3': // Write Watchpoint.
			type = WatchPointManager::eType_WriteWatchpoint;
			break;
		default:
			return eResult_UnSupported;
	}

	// Address can be found at: m_payload[3] 
	// Example "$Z0,1234,4#"

	unsigned long address = strtoul(&m_payload[3], NULL, 16);

	char *sizePtr = strchr(&m_payload[3], ',');
	if (NULL == sizePtr) {
		return eResult_Error;
	}

	// Skip the ','
	sizePtr++;

	unsigned long size = strtoul(sizePtr, NULL, 16);

	debug_printf("Watchpoint[%x] %s %x size [%x]\n\r", type, bSet?"Set at":"Clear at", address, size);

	if (bSet) {
		m_watchpoints.add(type, address, size);
	} else {
		m_watchpoints.remove(type, address, size);
	}

	return eResult_OK;
}
//###########################################################################

void
gdbparser::parseSingleStep()
{
	// Packet looks like 
	// $c<address> - address is optional
	bool bHasAddress = false;

	if (m_payload[1]) {
		unsigned long address = strtoul(&m_payload[1], NULL, 16);
		m_target->SetRegister(m_target->GetSpecialRegister(target::eSpecialRegPC), address);
		debug_printf("Setting reg fopr single step\r\n");
	}
	LeaveDCCMode();

	if (m_bWasRunning) { // Unflag wasrunning, we might have been running due to DCC mode.
		m_bWasRunning = false;
	}
	m_target->SingleStep();
}
//###########################################################################

gdbparser::EResult
gdbparser::parseXfer()
{

	// the payload looks something like:
	// qXfer:<something>:<annex>
	const char *str = "qXfer";
	const char *cmd = "memory-map";

	size_t len = strlen(str);

	if (strncmp(&m_payload[len + 1], cmd, strlen(cmd)) == 0) {
		// Cool, memory map.
		// Lets see the operation..(better be read.)
		// payload looks like: qXfer:memory-map:read::xx,xx

		char *ptr = &m_payload[strlen("qXfer:memory-map:read::")];
		unsigned long offset = strtoul(ptr, NULL, 16);

		char *lenPtr = strchr(ptr, ',');
		lenPtr++;
		unsigned long length = strtoul(lenPtr, NULL, 16);


/*<memory type="flash" start="addr" length="length">
  <property name="blocksize">blocksize</property>
</memory>
	<memory type="ram" start="addr" length="length"/>
<memory type="rom" start="addr" length="length"/>

*/

		const char memmapHeader[] = "<?xml version=\"1.0\"?> \
<!DOCTYPE memory-map \
PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\" \
\"http://sourceware.org/gdb/gdb-memory-map.dtd\"> \n \
<memory-map> \
<memory type=\"flash\" start=\"0x00000000\" length=\"0x80000\"> \n \
<property name=\"blocksize\"> 0x1000 </property> </memory> \n \
<memory type=\"ram\" start=\"0x40000000\" length=\"0x8000\"/> \n \
</memory-map>";

		size_t maplen = strlen(memmapHeader);
		char outbuf[MAX_MEM_READ * 2 + 1];

		size_t tosend = maplen - offset;
		if (tosend > length) {
			tosend = length;
			outbuf[0] = 'm';
		} else {
			outbuf[0] = 'l';
		}
		memcpy(&outbuf[1], &memmapHeader[offset], tosend);
		outbuf[1 + tosend] = 0;

		debug_printf("qXfer:memory-map:read::[%x], [%x], [%x], [%x]\n\r", offset, length, maplen, tosend);

		sendGDBPacket("l");

		return eResult_none;
	} else {
		return eResult_UnSupported;
	}
	return eResult_UnSupported;
}

//###########################################################################

gdbparser::EResult
gdbparser::parseMulti()
{
	static const char *FlashErase = "FlashErase";
	static const char *FlashWrite = "FlashWrite";
	static const char *FlashDone = "FlashDone";

	if (strncmp(&m_payload[1], FlashErase, strlen(FlashErase)) == 0) {
		// Command: vFlashErase:<addr>,<length>
		char *ptr = &m_payload[strlen("vFlashErase:")];
		unsigned long offset = strtoul(ptr, NULL, 16);

		char *lenPtr = strchr(ptr, ',');
		lenPtr++;
		unsigned long length = strtoul(lenPtr, NULL, 16);

		if (!m_flashdriver->eraseFlash(offset, length))
			return eResult_Error;

		return eResult_OK;
	} else if (strncmp(&m_payload[1], FlashWrite, strlen(FlashWrite)) == 0) {
		// Command: vFlashWrite:<address>:binary data.
		char *ptr = &m_payload[strlen("vFlashWrite:")];
		unsigned long address = strtoul(ptr, NULL, 16);

		if (m_flashbase == 0xffffffff) {
			// Can't be any data left to write without a base address.
			if (m_flashfifo.size() != 0) {
				return eResult_Error;
			}
			m_flashbase = address;

		}
		if (address != m_flashhead) {
			debug_printf("[W] Head/address mismatch [%x]/[%x]\n\r", m_flashhead, address);
		}

		// Find the Length<->Data delimiter
		char *dataPtr = strchr(ptr, ':');
		if (NULL == dataPtr) {
			debug_printf("[W] Data Error\n\r");
			return eResult_Error;
		}
		dataPtr++;

		unsigned long length = (unsigned long)&m_payload[m_payloadindex] - (unsigned long)dataPtr;
		debug_printf("Length: [%x]\n\r", length);

		// Push the data into the Flash writing buffer, and write it once it's done.
		m_flashfifo.push(dataPtr, length);

		// Set a head vector so we can make sure writes are continuous.
		m_flashhead = address + length;

		if (m_flashfifo.size() >= 256) {
			// Make sure the flash head address is properly aligned.
			if (m_flashbase & 0xff) {
				debug_printf("[W] Data alignement error\n\r");
				return eResult_Error;
			}
			char buffer[256];
			m_flashfifo.pop(buffer, 256);

			m_flashdriver->writeFlash(m_flashbase, (unsigned char *)buffer, 256);
			m_flashbase += 256;
		}

		return eResult_OK;
	} else if (strncmp(&m_payload[1], FlashDone, strlen(FlashDone)) == 0) {
		// Write remaining data.

		// Reset flash-head vector.
		m_flashbase = 0xffffffff;
		return eResult_OK;

	}

	return eResult_UnSupported;

}

