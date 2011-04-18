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
#ifndef _HEX_CONVERSION_H_
#define _HEX_CONVERSION_H_


class HexConversion
{
	public:
        /** Converts 4 bits into a hexadecimal value(0-F)
		 * 
		 * @param nibble
		 * 
         * @return char     '0' -> -'F'
		 */
		static char nibbleToHex(int nibble);

        /** Converts a hex-char to a nibble.
		 * 
         * @param hex   [0-9|A-F|a-f]
		 * 
		 * @return unsigned int
		 */
		static unsigned int hexToNibble(char hex);


        /** converts an integer to a hexadecimal value.
		 * 
         * @param value 	-   Value to be converted
         * @param str   	-   Output string, must have enough space.
         * @param nibbles   -   Number of nibbles to output, default 8.
         * @param prefix	-   flag to enable '0x' prefix.
		 */
		static void intToHex(int value, char *str, int nibbles = 8, bool prefix = false);


		
};


#endif // _HEX_CONVERSION_H_
