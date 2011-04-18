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
#ifndef _JTAG_ARM9TDMI_H_
#define _JTAG_ARM9TDMI_H_

#include "jtag.h"
#include "ARM.h"
#include "JTAG_arm.h"




class JTAG_arm9tdmi : public JTAG_arm
{
	public:
		JTAG_arm9tdmi();
		virtual ~JTAG_arm9tdmi();

		// initialization

		// General overloaded functionality.
		virtual void ReadRegs();
		virtual void ReadMem(char *pDest, int Address, int size);
		virtual void WriteMem(char *pData, int Address, int size);
		virtual void SystemReset();
		virtual void Halt();
		virtual bool Halted();
		virtual void Continue();
		virtual void MaskInt();
		virtual void UnMaskInt();
		virtual void HandleException();
		virtual void SetBreakpoint(unsigned long Address);
		virtual void ClearBreakpoint(unsigned long Address);
		virtual void SingleStep();
		virtual void KillMMU();
		virtual void CommChannelWrite(unsigned long data);
		virtual void CommChannelWriteStream(unsigned char *indata, int bytes);
		virtual void CommChannelReadStream(unsigned char *indata, int bytes);
		virtual void setFlag(unsigned long flag, unsigned long value);
		virtual unsigned long CommChannelRead();
		virtual void SetWatchPoint(unsigned long Address, unsigned long size, unsigned long count, bool bWrite = true) {};
		virtual void ClearWatchPoint(unsigned long Address) {};
		virtual unsigned long TranslateAddress(unsigned long MVA);
		virtual void FlushDCache();
		virtual void FlushICache();

		virtual bool HasFeature(EFeature feature);

		// Arm specific stuff.
		int ReadICEReg(int iReg);
		void WriteICEReg(int iReg, int data);

	protected:
		void SetupSWBreakWatchPoint();

		void GetContext();

		unsigned long SC15Physical(unsigned long reg, unsigned long value, bool bwrite);
		void SC15Interpreted(unsigned long insn);
		void ShiftSC1Bits(unsigned char *pIn, unsigned char *pOut, int bits);
		void CreateSC1Packet(unsigned int *indata, unsigned int opcode, unsigned int data, bool speed);
		unsigned long Execute(unsigned long insn, unsigned long data, bool speed);
		bool WaitStatus(int status);
		void GetRegsFromMode(unsigned long cpsr);

		// MMU Code.
		void StartMMU();
		unsigned long Level2Decode(unsigned long Level2Descriptor, unsigned long MVA);
		unsigned long GetFSR(int cache);


		void ReadHWord(unsigned long address, void *pData);
		void WriteHWord(unsigned long address, void *pData);

		void ReadByte(unsigned long address, void *pData);
		void WriteByte(unsigned long address, void *pData);


		void SwitchToARM();

		void	SelectScanChain(int chain);
		void	EnterInstruction(int insn);

	private:



		// MMU stuff.
		unsigned long	m_TTB;
		unsigned long	m_CP15_C1;

		unsigned long	m_savedPC;
		unsigned long	m_savedR0;


};


#endif // _JTAG_ARM9TDMI_H_
