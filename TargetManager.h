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
#ifndef _TARGET_MANAGER_H_
#define _TARGET_MANAGER_H_

#include "target.h"


/** Target manager, used to determine target based on the TAP
 *  idcode.
 *
 *  However, some targets don't follow the standard spec, and
 *  their idcodes must be added manually.
 * 
 * 
 */
class TargetManager
{
	public:
        /** Used to dunamically register a new target.
         * Most likely to make an alias to some other target.
		 * 
         * @param t         target instance.
         * @param idcode    idcode associated with target.
         * @param name      name of target.
         * 
         * @return          True if the target was successfully
         *                  registered, otherwise false.
		 */
		bool 		registerTarget(target *t, unsigned long idcode, const char *name);
		
        /** Used to specify which target to use for a certain
         *  specification revision and product code.
		 * 
         * @param t             -   Target Instande.
         * @param specrev       -   Specification revision.
         * @param prodcode      -   Product code
         * @param manufacturer  -   Manufacturer
         * @param name      	-	name of target.
		 * 
         * @return bool         -   True if the target was successfully
         *                          registered.
		 */
		bool 		registerDevice(target *t, unsigned long specrev, unsigned long prodcode, unsigned long manufacturer, const char *name);


		/** Returns the name of a target.
		 * 
		 * @param idcode
		 * 
         * @return const char*  Target name, of NULL if the target
         *         doesn't exist.
		 */
		const char	*getTargetName(unsigned long idcode);

        /** Finds a target based on it's IDCODE
		 * 
         * @param idcode    IDCODE associated with target.
		 * 
         * @return target*  Target instance, or NULL if target does not
         *         exist.
		 */
		target		*getTarget(unsigned long idcode);

        /** Finds a target based on it's name.
         *  See: getTarget(unsigned long idcode)
		 * 
		 * @param name
		 * 
		 * @return target*
		 */
		target		*getTarget(const char *name);


        /** Returns the number of targets stored.
         * Or actually the maximum number of targets allowed.
         * 
         *  @return Number of target slots
         */
		int			getMaxNumTargets() const {return max_targets;}

        /** Returns a target name from a index.
         * 
         *  @return target struct from index.
         */
		const char	*getTargetNameFromIndex(int iTarget) const {return m_targets[iTarget].m_name;}
		


		static TargetManager *instance();

	private:
		TargetManager();

		struct targetdef
		{
			target			*m_target;
			unsigned long	m_idcode;
			unsigned long 	m_revision;
			unsigned long 	m_prodid;
			unsigned long 	m_manufacturer;
			const char 		*m_name;
		};


		targetdef *getDef(unsigned long idcode);

		static const unsigned long max_targets = 10;
		static	targetdef m_targets[max_targets];

};



#endif // _TARGET_MANAGER_H_

