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
#ifndef _GDB_PARSER_H_
#define _GDB_PARSER_H_

#include "target.h"
#include "Timer.h"
#include "jtag.h"
#include "WatchPointManager.h"
#include "config.h"
#include "FlashDriver.h"
#include "fifo.h"

class Stream;
class gdbparserFunctor;

class gdbparser
{
	public:
		friend class gdbparserFunctor;
		gdbparser(jtag *device, Stream *stream);
		~gdbparser();

		
		bool run();


		typedef enum {
			eRcmdNone = 0,
			eRcmdMMUOff,
			eRcmdMMUOn,
			eRcmdLL,
			eRcmdID,
			eRcmdTargets,
			eRcmdTest,
			eRcmdFlash,
			eRcmdLPCTargetFreq,
		} ERcmd;
	private:
		typedef enum {
			eResult_OK,
			eResult_Error,
			eResult_UnSupported,
			eResult_none
		} EResult;

		void TimerTick();
		void sendACK();
		void sendNAK();
		void waitACK();
		void sendOK();
		void sendError(int error);
		void unsupported();
		void parseCommand();
		char nibbleTohex(int nibble);
		void sendSignal(int signal);
		void sendOffsets(unsigned long text, unsigned long data, unsigned long bss);
		void sendRegisters();
		void sendRegister(int reg);
		void sendTrap(int trap);
		void sendGDBString(const char *str);
		void sendGDBPacket(const char *contents, size_t len = 0);

		void parseContinue();
		void parseContinueWithSignal();
		void parseReadMemory();
		void parseWriteMemory();
		void parseWriteMemoryBinary();
		void parseReadRegister();
		bool parseWriteRegister();
		void parseSingleStep();
		EResult parseRcmd();
		EResult parseWatchpoint();
		EResult parseXfer();
		EResult parseMulti();

		void runRcmd(ERcmd cmd, const char *data);

		void EnterDCCMode();
		void LeaveDCCMode();

		void ReadMemory(void *dest, unsigned long address, unsigned long size);
		void WriteMemory(void *src, unsigned long address, unsigned long size);

		WatchPointManager	m_watchpoints;


		// Endian swapping.
		unsigned long	SwapEndian(unsigned long);

		FlashDriver	*m_flashdriver;
		jtag	*m_device;
		target	*m_target;
		Stream	*m_stream;
		char 	m_payload[MAX_GDB_PAYLOAD_SIZE];
		char	m_checksum[3];
		int 	m_payloadindex;
		int		m_checksumindex;
		bool	m_gotpacket;
		bool	m_bIdentified;
		jtag_transaction m_idcode;
		jtag_transaction m_reset;
		jtag_transaction m_instruction;
		jtag_transaction m_treset;
		int		m_instruction_id;
		int		m_id;
		int		m_previns;
		Timer	*m_Timer;
		int		m_timerTick;
		bool	m_bWasRunning;
		bool	m_bEscaped;
		bool	m_bUseDCC;
		bool	m_bDCCActivated;
		unsigned long	m_DCCAddress;
		unsigned long	m_lpcFreq;

		fifo<char>		m_flashfifo;
		unsigned long	m_flashhead;
		unsigned long	m_flashbase;


		// Statistics.
		unsigned long 	m_MemTransfered;

		typedef enum {
			eState_start = 0,
			eState_payload,
			eState_checksum,
		} EState;
		EState	m_state;



};


class gdbparserFunctor : public Timer::Functor
{
	public:
		gdbparserFunctor(gdbparser *target) : m_target(target) {}

		virtual void operator ()() {
			m_target->TimerTick();
		}

	private:
		gdbparser *m_target;
};


#endif // _GDB_PARSER_H_
