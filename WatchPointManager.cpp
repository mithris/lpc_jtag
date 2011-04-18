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
#include "WatchPointManager.h"
#include "target.h"
#include "debug_out.h"
//###########################################################################

WatchPointManager::WatchPointManager()
{
	// Free all watchpoints.
	for (int i = 0; i < MAX_WATCHPOINTS; ++i) {
		m_watchpoints[i].m_inuse = false;
	}
}
//###########################################################################


WatchPointManager::~WatchPointManager()
{
	
}
//###########################################################################

bool 
WatchPointManager::add(EType type, unsigned long address, unsigned long size) 
{
	Watchpoint *w = 0;

	for (int i = 0; i < MAX_WATCHPOINTS; ++i) {
		if (!m_watchpoints[i].m_inuse) {
			w = &m_watchpoints[i];
			break;
		}
	}

	if (!w) {
		return false;
	}

	w->m_type = type;
	w->m_address = address;
	w->m_size = size;
	w->m_inuse = true;

	return true;
}
//###########################################################################
bool 
WatchPointManager::remove(EType type, unsigned long address, unsigned long size) 
{
	bool bFound = false;

	for (int i = 0; i < MAX_WATCHPOINTS; ++i) {
		if (m_watchpoints[i].m_inuse && m_watchpoints[i].m_address == address && m_watchpoints[i].m_size == size) {
			m_watchpoints[i].m_inuse = false;
			bFound = true;
			break;
		}
	}

	return bFound;
}
//###########################################################################

void
WatchPointManager::enable()
{
	if (!m_target) {
		return;
	}

	for (int i = 0; i < MAX_WATCHPOINTS; ++i) {
		if (m_watchpoints[i].m_inuse) {
			switch (m_watchpoints[i].m_type) {
				case eType_MemoryBreakpoint:
					{
						if (m_watchpoints[i].m_size > 8 && (m_watchpoints[i].m_size & 1)) {
							// We don't support this size.
							break;
						}
						m_target->ReadMem((char *)&m_watchpoints[i].m_backup64, m_watchpoints[i].m_address, m_watchpoints[i].m_size);
						unsigned char bkptinsn[8];
						m_target->GetSWBreakpointInstruction(bkptinsn, m_watchpoints[i].m_size);
						m_target->WriteMem((char *)bkptinsn, m_watchpoints[i].m_address, m_watchpoints[i].m_size);
						debug_printf("Watchpoint enabled at: 0x%x\r\n", m_watchpoints[i].m_address);

					} break;
				case eType_HardwareBreakpoint:
					break;
				case eType_ReadWatchpoint:
					break;
				case eType_WriteWatchpoint:
					break;
			}
		}
	}
}
//###########################################################################

void
WatchPointManager::disable()
{
	if (!m_target) {
		return;
	}

	for (int i = 0; i < MAX_WATCHPOINTS; ++i) {
		if (m_watchpoints[i].m_inuse) {
			switch (m_watchpoints[i].m_type) {
				case eType_MemoryBreakpoint:
					// Restore backed insn
					m_target->WriteMem((char *)&m_watchpoints[i].m_backup64, m_watchpoints[i].m_address, m_watchpoints[i].m_size);
					break;
				case eType_HardwareBreakpoint:
					break;
				case eType_ReadWatchpoint:
					break;
				case eType_WriteWatchpoint:
					break;
			}
			
		}
	}

}
//###########################################################################


