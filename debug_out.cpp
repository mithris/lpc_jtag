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


#include "debug_out.h"
#include "stream.h"
#include <stdarg.h>


static Stream *_debug_stream = NULL;


///////////////////////////////////////////////////////////////
// Debug printf.
void debug_printf(const char *pFormat, ...)
{
	int				iLength;
	int				iOffset;
	unsigned char	MyChar;
	char            Temp[256];
	va_list         VaList;
	int				iState = 0;
	int				iPos = 0;
	unsigned int	ui;
	const char		*pString;
	char			Color;
	int				nibble;
	const 			char *HexDigits = "0123456789abcdef";
	
	if (NULL == _debug_stream) {
		return;
	}

	va_start(VaList , pFormat);
	
	while(0 != *pFormat) {
		switch (iState) {
			case 0: // searching for %.
				if (*pFormat == '%') {
					iState = 1;
					pFormat++;
					break;
				}
				Temp[iPos++] = *pFormat++;
				break;
			case 1: // Search for command.
				switch (*pFormat) {
					case '%': // just add a '%' to the output buffer.
						Temp[iPos++] = *pFormat++;
						iState = 0;
						break;
					case 'X': 
					case 'x': // Print a hex number
						ui = va_arg(VaList, unsigned int);
						Temp[iPos++] = HexDigits[(ui >> 28) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 24) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 20) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 16) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 12) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 8) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 4) & 0x0f];
						Temp[iPos++] = HexDigits[(ui) & 0x0f];
						iState = 0;
						pFormat++;
						break;
					case 'f': // Print a hex number
						ui = va_arg(VaList, unsigned int);
						Temp[iPos++] = HexDigits[(ui >> 28) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 24) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 20) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 16) & 0x0f];
						Temp[iPos++] = '.';//HexDigits[(ui >> 12) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 12) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 8) & 0x0f];
						Temp[iPos++] = HexDigits[(ui >> 4) & 0x0f];
						Temp[iPos++] = HexDigits[(ui) & 0x0f];
						iState = 0;
						pFormat++;
						break;
					case 'd': // Print a decimal number
						pFormat++;
						break;
					case 's': // Print a string.
						pString = va_arg(VaList, const char *);
						while(0 != *pString) Temp[iPos++] = *pString++;
						iState = 0;
						pFormat++;
						break;

					default:
						iState = 0;
						pFormat++;
						break;

				}
				break;
		}
	}

	Temp[iPos] = 0;
	
	va_end(VaList);
	
	_debug_stream->write(Temp, iPos);

}


void set_debug_stream(Stream *s)
{
	_debug_stream = s;
}



