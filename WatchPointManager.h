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
#ifndef _WATCHPOINT_MANAGER_H_
#define _WATCHPOINT_MANAGER_H_

#include "config.h"

// Forward declarations
class target;

class WatchPointManager
{
	public:
		WatchPointManager();
		~WatchPointManager();

		typedef enum {
			eType_MemoryBreakpoint,
			eType_HardwareBreakpoint,
			eType_ReadWatchpoint,
			eType_WriteWatchpoint
		} EType;

        /** Attaches to a target.
		 * 
		 * @param pTarget
		 */
		void attach(target *pTarget) {
			m_target = pTarget;
		}

        /** Adds a watchpoint/breakpoint
		 * 
         * @param type      - Type, see <EType> above.
         * @param address   - Where to place the watchpoint.
         * @param size      - Size, instruction or memory area size.
		 * 
		 * @return bool
		 */
		bool add(EType type, unsigned long address, unsigned long size);

        /** Removes a watchpoint/breakpoint
		 * 
		 * @param type      - Type, see <EType> above.
		 * @param address   - Where to place the watchpoint.
		 * @param size      - Size, instruction or memory area size.
		 * 
		 * @return bool
		 */
		bool remove(EType type, unsigned long address, unsigned long size);


        /** Sets the watchpoints in the target.
         * Used when restarting the target after it has been halted.
         *
		 */
		void enable();

        /** Disables the watchpoints in the target.
         * Used when halting the target to make sure the
         * watchpoints don't interfere with the debugging.
		 */
		void disable();

	protected:
	private:

		target	*m_target;

		struct Watchpoint {
			EType			m_type;
			unsigned long 	m_address;
			unsigned long 	m_size;
			bool			m_inuse;

			union {
				unsigned long long m_backup64;
				unsigned long	m_backup32;
				unsigned short	m_backup16;
				unsigned char	m_backup8;

			};
		};

		Watchpoint	m_watchpoints[MAX_WATCHPOINTS];

};


#endif // _WATCHPOINT_MANAGER_H_
