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
#ifndef _JTAG_BF533_H_
#define _JTAG_BF533_H_

#include "target.h"
#include "blackfin.h"
#include "jtag.h"

typedef struct 
{
	bfin_registers 		ireg;
	const char			*name;
	unsigned long		value;
	bool				restore;

} bf_reg_data;

typedef struct
{
	bool			data;		// if true, use a data watchpoint.
	bool			enabled;	// If this watchpoint should be enabled again when leaving emulation.
	bool			write;		// For data watchpoints, if the operation is write.
	unsigned long	address;	// Address.
	unsigned long 	index;		// (0-5) valid for address, (0-1) valid for data.
	unsigned long	size;		// Unused for now, always 4, ranges are more advanced.
	unsigned long	counter;	// Match <counter> times before breaking.
} bf_watchpoint;


class JTAG_bf533 : public target
{
		// General overloaded functionality.
	public:

		JTAG_bf533();
		~JTAG_bf533();

		virtual void SetDevice(jtag *device);
		
		virtual unsigned long GetPCReg();
		virtual unsigned long GetRegisterCount();
		virtual void ReadRegs();	
		virtual const char * GetRegisterName(unsigned long reg);
		virtual void ReadMem(char *pDest, int Address, int size);
		virtual unsigned long GetRegister(int iReg);
		virtual void SetRegister(int iReg, unsigned long value);
		virtual void Halt();
		virtual bool Halted();
		virtual void WriteMem(char *pData, int Address, int size);
		virtual void SystemReset();
		virtual void UnMaskInt();
		virtual void HandleException();
		virtual void Continue();
		virtual void MaskInt();
		virtual void SingleStep();
		virtual void KillMMU();
		virtual void SetBreakpoint(unsigned long Address);
		virtual void ClearBreakpoint(unsigned long Address);
		virtual void SetWatchPoint(unsigned long Address, unsigned long size, unsigned long count, bool bWrite = true);
		virtual void ClearWatchPoint(unsigned long Address);

		virtual void CommChannelWriteStream(unsigned char *indata, int words);
		
		virtual void CommChannelReadStream(unsigned char *indata, int words){}
		
		virtual unsigned long CommChannelRead();
		virtual void setFlag(unsigned long flag, unsigned long value);
		virtual void CommChannelWrite(unsigned long data);

		virtual void GetSWBreakpointInstruction(unsigned char *data, int size) {
			*((int *)data) = 0;
		}

		virtual void EnterDCCMode(unsigned long) {}
		virtual void LeaveDCCMode() {}
		virtual bool HasFeature(target::EFeature f) {return false;}

		virtual void FlushICache() {}
		virtual void FlushDCache() {}

		virtual unsigned long TranslateAddress(unsigned long addr) {return addr;}
		virtual int GetSpecialRegister(target::ESpecialReg);
		virtual int MapGDBRegister(int iReg);
		

	private:
		enum EInsnType
		{
			eMove,
			eLdr,
			eStr,
			eLdImm16_hi,
			eLdImm16_lo,
			eLdr16,
			eLdr8,
			eStr16,
			eStr8

		};

		void EnterInstruction(int insn);
		void ShiftDRBits(unsigned char *in, unsigned char *out, int bits, bool bRTI);


		unsigned long	getRegister(bfin_registers reg);
		void			setRegister(bfin_registers reg, unsigned long value);

		unsigned long	readWord(unsigned long address);
		void			writeWord(unsigned long address, unsigned long value);

		unsigned long	readData(int size, unsigned long address);
		void			writeData(int size, unsigned long address, unsigned long value);


		void			readDataBlock(unsigned char *buf, unsigned long address, unsigned long size);
		void			writeDataBlock(unsigned char *buf, unsigned long address, unsigned long size);

		void			restoreRegisters();
		int				findReg(bfin_registers reg);

		unsigned long	emulationCause();

		void			getContext();

		void			startWPU();
		void			clearWatchPoints();
		void			refreshWatchPoints();
		void			disableWatchPoints();
		void			enableSingleStep();
		void			disableSingleStep();


		unsigned long	Status(unsigned long status);
		//unsigned long	Control();
		unsigned long	Control(unsigned long ctl, bool bRTI = false);
		unsigned long 	PushInsn(unsigned long insn, bool bRTI = false);
		unsigned long	readEmuDat();
		void			writeEmuDat(unsigned long value);
		void			bitshift(unsigned char *in, unsigned char *out, int bits);

		unsigned long	generateInsn(EInsnType type, bfin_registers sourcereg, bfin_registers targetreg, unsigned int value = 0);

		bool			DMATransfer(unsigned long Destination, unsigned long source, unsigned long size);

		unsigned long	m_ctl;
		unsigned long	m_status;
		bool			m_wpuPower;
		bool			m_singleStepEnabled;

		//unsigned long	m_dataWatchPoints[BF_N_DATA_WATCHPOINT];

		
		bf_watchpoint	m_datawatchpoints[BF_N_DATA_WATCHPOINT];
		bf_watchpoint	m_insnwatchpoints[BF_N_INSTRUCTION_WATCHPOINT];

		unsigned long	m_dataSRAMShadowAddress;
		unsigned long	m_instructionSRAMShadowAddress;


		jtag			*m_device;

};

#endif // _JTAG_BF533_H_

