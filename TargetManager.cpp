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
#include "TargetManager.h"
#include "Serial.h"
#include "debug_out.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//###########################################################################

TargetManager::targetdef TargetManager::m_targets[TargetManager::max_targets] = 
{
	{NULL, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, ""},
	{NULL, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, ""},
	{NULL, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, ""},
	{NULL, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, ""},
	{NULL, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, ""},
};


//###########################################################################

TargetManager::TargetManager()
{

}
//###########################################################################

TargetManager *
TargetManager::instance()
{
	static TargetManager instance;

	return &instance;
}
//###########################################################################


bool
TargetManager::registerTarget(target *t, unsigned long idcode, const char *name)
{
	// Find free target slot.

	for (int i = 0; i < max_targets; ++i) {
		if (NULL == m_targets[i].m_target) {
			m_targets[i].m_target = t;
			m_targets[i].m_name = name;
			m_targets[i].m_idcode = idcode;
			debug_printf("Adding target [%x]\n\r", i);
			return true;
		}
	}
	debug_printf("[E] Failed to add target\n\r");

	return false;
}
//###########################################################################

const char *
TargetManager::getTargetName(unsigned long idcode)
{
	targetdef *def = getDef(idcode);

	if (NULL != def) {
		return def->m_name;
	}

	return NULL;
}
//###########################################################################

target *
TargetManager::getTarget(unsigned long idcode)
{
	targetdef *def = getDef(idcode);

	if (NULL != def) {
		return def->m_target;
	}

	return NULL;
}
//###########################################################################

TargetManager::targetdef *
TargetManager::getDef(unsigned long idcode)
{

	unsigned long rev = idcode >> 28;
	unsigned long prodid = (idcode >> 12) & 0xffff;
	unsigned long manufacturer = ((idcode >> 1) & 2047); // 11 bits

	debug_printf("Manufacturer: [%x]\n\rRevision: [%x]\n\rProduct ID: [%x]\n\r", manufacturer, rev, prodid);

	
	for (int i = 0; i < max_targets; ++i) {
		// If the target is identified by a hardcoded IDCode
		if (NULL != m_targets[i].m_target && m_targets[i].m_idcode != 0xffffffff && m_targets[i].m_idcode == idcode) {
			return &m_targets[i];
		}
		
		if (NULL != m_targets[i].m_target && m_targets[i].m_idcode == 0xffffffff) {
			targetdef &def = m_targets[i];

			if (def.m_revision != 0xffffffff && def.m_revision != rev) {
				continue;
			}
			if (def.m_prodid != 0xffffffff && def.m_prodid != prodid) {
				continue;
			}
			if (def.m_manufacturer != 0xffffffff && def.m_manufacturer != manufacturer) {
				continue;
			}
			debug_printf("Manufacturer: [%x]\n\rRevision: [%x]\n\rProduct ID: [%x]\n\r", manufacturer, rev, prodid);

			// We accept!
			return &m_targets[i];
		}

	}

	return NULL;
}
//###########################################################################

target *
TargetManager::getTarget(const char *name)
{
	for (int i = 0; i < max_targets; ++i) {
		if (NULL != m_targets[i].m_target && strcasecmp(name, m_targets[i].m_name) == 0) {
			return m_targets[i].m_target;
		}
	}

	return NULL;
}
//###########################################################################

bool
TargetManager::registerDevice(target *t, unsigned long specrev, unsigned long prodcode, unsigned long manufacturer, const char *name)
{
	targetdef *def = NULL;

	for (int i = 0; i < max_targets; ++i) {
		if (NULL == m_targets[i].m_target) {
			def = &m_targets[i];
			debug_printf("Adding target: [%x]\n\r", i);
			break;
		}
	}

	if (NULL == def) {
		return false;
	}

	def->m_idcode = 0xffffffff; // make the IDcode invalid.
	def->m_target = t;
	def->m_revision = specrev;
	def->m_name = name;
	def->m_manufacturer = manufacturer;
	def->m_prodid = prodcode;
	return true;
	
}
//###########################################################################

