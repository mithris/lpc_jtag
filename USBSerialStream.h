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
#ifndef _USB_SERIAL_STREAM_H_
#define _USB_SERIAL_STREAM_H_

#include "stream.h"
#include "fifo.h"



class USBSerialStream : public Stream
{
	public:
		virtual ~USBSerialStream();
		virtual bool dataReady();
		virtual int read(void *buffer, int maxsize);
		virtual int write(const void *buffer, int size);
		virtual size_t size() {
			return m_outfifo.size();
		}
		static USBSerialStream *instance();
	private:
		static void BulkIn(unsigned char bEP, unsigned char bEPStatus);
		static void BulkOut(unsigned char bEP, unsigned char bEPStatus);
		static void FrameHandler(unsigned short frame);
		static void DevIntHandler(unsigned char devStatus);

		void TransferData(bool bInit);

		USBSerialStream(Stream *echoStream);
		static USBSerialStream *m_instance;

		// fifo stuff.
		fifo<unsigned char>	m_infifo;
		fifo<unsigned char>	m_outfifo;

		bool m_bTransferInProgress;
		bool m_bDontStartNew;
		bool m_bConnected;
};


#endif // _USB_SERIAL_STREAM_H_
