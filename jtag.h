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

#ifndef _JTAG_H_
#define _JTAG_H_

#include "Timer.h"

#define INSTR_SCAN_N	0x02
#define INSTR_IDCODE	0x0e
#define INSTR_EXTEST	0x00
#define INSTR_INTEST 	0x0c //1100
#define INSTR_BYPASS 	0x0f //1111
#define INSTR_CLAMP 	0x05 //0101
#define INSTR_HIGHZ 	0x07 //0111
#define INSTR_CLAMPZ 	0x09 //1001
#define INSTR_S_P 		0x03 //0011
#define INSTR_RESTART 	0x04 //0100

#define COMMAND_BUFFER_SIZE	256

#define CMD_BIT_TMS 1
#define CMD_BIT_TDI 2
#define CMD_BIT_TDO 4

#define CMD_SHIFT_TMS  0
#define CMD_SHIFT_TDI  1
#define CMD_SHIFT_TDO  2


typedef enum {
	eTestReset = 0,
	eRTI,
	eSelectDR,
	eSelectIR,
	eCaptureDR,
	eCaptureIR,
	eShiftDR,
	eShiftIR,
	eExit1DR,
	eExit1IR,
	ePauseDR,
	ePauseIR,
	eExit2DR,
	eExit2IR,
	eUpdateDR,
	eUpdateIR,
	eNStates
}ETAP_States;


class jtag_transaction;
class jtagCB;



class jtag 
{
	public:
		friend class jtagCB;
		jtag();
		~jtag();

		void initialize();

		void 	Enqueue(jtag_transaction *trans);
		jtag_transaction *Dequeue();


		void	TMS(bool b);
		void	TCK(bool b);
		void	TDI(bool b);
		int		TDO();
		void	nTRSR(bool b);
		void	pulseTCK();

	private:
		void	SetSignals(jtag_transaction *trans);
		void	EnqueueCommand(unsigned char pins);
		unsigned char DequeueCommand();
		void	StartTimer();
		void	StopTimer();
		void	ProcessTransaction(jtag_transaction *trans);

		int		CommandHead();
		int		CommandTail();
		void	CommandPushTDO(int tdo);
		void	CommandClear();

		void	Tick();

		ETAP_States	m_curState;	

		char	m_commandBuffer[COMMAND_BUFFER_SIZE];
		int		m_commandHead;
		int		m_commandTail;
		int		m_commandSize;
		int		m_TCKPulse;
		
		bool	m_bTimerRunning;
		Timer	*m_Timer;


		jtag_transaction *m_trans_head;
		jtag_transaction *m_trans_tail;
		jtag_transaction *m_trans_cur;
};

class jtagCB : public Timer::Functor
{
	public:
		jtagCB(jtag *target) : m_target(target) {}
		virtual void operator ()() {
			m_target->Tick();
		}
	private:
		jtag	*m_target;
};


class jtag_transaction
{
	public:
		friend class jtag;
		typedef enum { 
			eNone,
			eShiftDR = ::eShiftDR, 					// Data Register shift.
			eEnterInstruction = ::eShiftIR, 		// Instruction register shift.
			eTestReset = ::eTestReset,				// TAP controller reset
			eRTI = ::eRTI, 	 						// Cycle RTI.
			eReset									// Actual reset, via TRST

		} ETransaction;

        /** Default constructor.
		 * 
		 */
		jtag_transaction() :
			m_transType(eNone),
			m_indata(0),
			m_outdata(0),
			m_bits(0),
			m_bRTI(false),
			m_bDone(false)
		{

		}
        /** Copy constructor.
		 * 
		 * @param t
		 */
		jtag_transaction(const jtag_transaction &t) :
			m_transType(t.m_transType),
			m_indata(t.m_indata),
			m_outdata(t.m_outdata),
			m_bits(t.m_bits),
			m_bRTI(t.m_bRTI),
			m_bDone(false)
		{

		}


		/** Constructor.
		 * 
         * @param trans_type        - Transaction type.
         * @param indata            - Data to be shifted into the TAP
         * @param outdata           - Data shifted out of the TAP
         * @param bits              - Number of bits to shift.
         * @param bRTI              - Pass RTI on the way?
		 */
		jtag_transaction(ETransaction trans_type, char *indata, char *outdata, int bits, bool bRTI) : 
			m_transType(trans_type),
			m_indata(indata),
			m_outdata(outdata),
			m_bits(bits),
			m_bRTI(bRTI),
			m_bDone(false)

		{

		}

        /** Returns the type of the transaction
         * See ETransaction
		 * 
		 * @return ETransaction
		 */
		ETransaction type() {
			return m_transType;
		}

        /** Fetches the bit in the outdata corresponding to index
		 * 
         * @param index     - Index bit
		 * 
         * @return bool     - Returns true if the bit on <index> is set,
         *                    false, if the bit is not, or the index is
         *                    out of range.
		 */
		bool getoutbit(int index) const {
			if (0 == m_indata) {
				return false;
			}
			if (index >= m_bits) {
				return false;
			}

			int byte = index >> 3;
			int bit = index & 0x07;

			return (m_indata[byte] & (0x01 << bit));
		}

		int bits() const {return m_bits;}
		bool rti() const {return m_bRTI;}
		bool done() const {return m_bDone;}
		void setDone(bool bDone) {m_bDone = bDone;}
		void setLength(int len) {m_bits = len;}
		
		
		
		int				m_tdo;
		
	private:

		char			*m_indata;
		char			*m_outdata;
		int				m_bits;

		ETransaction	m_transType;
		volatile bool	m_bDone;
		bool			m_bRTI;

		jtag_transaction	*m_next;
};


#endif // _JTAG_H_
