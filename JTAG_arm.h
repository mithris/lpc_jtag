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
#ifndef _JTAG_ARM_H_
#define _JTAG_ARM_H_

#include "target.h"
#include "jtag.h"
#include "ARM.h"

/** Common ARM helper functions.
 * 
 */
class JTAG_arm : public target
{
public:
	JTAG_arm();

	virtual unsigned long GetRegisterCount();
	virtual const char * GetRegisterName(unsigned long reg);
	virtual unsigned long GetRegister(int iReg);
	virtual void SetRegister(int iReg, unsigned long value);

	/** Translates a special register name to a target specific
	 * register index.
	 * (ex. eSpecRegPC would be 15 on ARM)
	 *
	 * @param reg   Special register
	 *
	 * @return int  Target register index.
	 */
	virtual int GetSpecialRegister(ESpecialReg reg);

	virtual void GetSWBreakpointInstruction(unsigned char *data, int size);

    /** Returns the register index holding the PC
     * Note: this is the internal representation, which does not
     * necessary match with GDB. Use MapGDBRegister with the result
     * from this method to be certain.
	 * 
     * @return unsigned long    - Index of register holding the PC
	 */

	virtual unsigned long GetPCReg();
	/** GDB has some funny notion of having the FP registers before
	 *  the CPSR etc, so it's not a linear list from r0->cpsr
	 *
	 * @param ireg  GDB register index.
	 *
	 * @return int  Actual register index.
	 */
	virtual int MapGDBRegister(int ireg);
	
    /** Set the JTAG device to use
	 * 
	 * @param device
	 */
	virtual void SetDevice(jtag *device);


	/** Enters DCC mode. (Direct Communication Channel)
	 *  On ARM processors this uploads and starts a small
	 *  program at 'address' which can read data directly
	 *  from the JTAG port, making memory transfers alot
	 *  quicker.
	 *
	 * @param address
	 */

	virtual void EnterDCCMode(unsigned long address);
	/** Leaves DCC mode, stops the core and restores the
	 *  register contents.
	 *
	 */

	virtual void LeaveDCCMode();
	/** Checks if a target supports a certain feature.
	 *
	 * @param feature   Feature in question
	 *
	 * @return bool     True if the feature is supported, otherwise
	 *         false.
	 */
protected:
	ARM_Register	m_registers[ARM_REG_COUNT];


	jtag			*m_device;

	bool			m_targetWasThumb;

	// DCC backup etc.
	ARM_Register	m_dcc_backup_registers[ARM_REG_COUNT];

};


#endif // _JTAG_ARM_H_

