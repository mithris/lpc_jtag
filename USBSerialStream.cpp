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
#include <stdlib.h>
#include <string.h>
#include "USBSerialStream.h"
#include "LPC214x.h"
#include "fifo.h"
#include "Serial.h"
#include "debug_out.h"




extern "C" {
	#include "lpcusb/target/type.h"
	#include "lpcusb/target/usbdebug.h"
	#include "lpcusb/target/usbapi.h"

	void USBHandlerSerial(void) __attribute__((interrupt("IRQ")));
}


#define BAUD_RATE		115200
#define INT_IN_EP		0x81
#define BULK_OUT_EP		0x05
#define BULK_IN_EP		0x82
#define MAX_PACKET_SIZE	64
#define LE_WORD(x)		((x)&0xFF),((x)>>8)
// CDC definitions
#define CS_INTERFACE			0x24
#define CS_ENDPOINT				0x25
#define	SET_LINE_CODING			0x20
#define	GET_LINE_CODING			0x21
#define	SET_CONTROL_LINE_STATE	0x22

#define	INT_VECT_NUM	0

#define IRQ_MASK 0x00000080

// data structure for GET_LINE_CODING / SET_LINE_CODING class requests
typedef struct {
	U32		dwDTERate;
	U8		bCharFormat;
	U8		bParityType;
	U8		bDataBits;
} TLineCoding;

static TLineCoding LineCoding = {460800, 0, 0, 8};
static unsigned char dataBuf[65];
static unsigned char abClassReqData[8];

USBSerialStream *USBSerialStream::m_instance = NULL;


// USB Descriptors.
static const unsigned char abDescriptors[] = {

// device descriptor
	0x12,
	DESC_DEVICE,
	LE_WORD(0x0200),			// bcdUSB
	0x02,						// bDeviceClass
	0x00,						// bDeviceSubClass
	0x00,						// bDeviceProtocol
	MAX_PACKET_SIZE0,			// bMaxPacketSize
	LE_WORD(0xFFFF),			// idVendor
	LE_WORD(0x0005),			// idProduct
	LE_WORD(0x0100),			// bcdDevice
	0x01,						// iManufacturer
	0x02,						// iProduct
	0x03,						// iSerialNumber
	0x01,						// bNumConfigurations

// configuration descriptor
	0x09,
	DESC_CONFIGURATION,
	LE_WORD(67),				// wTotalLength
	0x02,						// bNumInterfaces
	0x01,						// bConfigurationValue
	0x00,						// iConfiguration
	0xC0,						// bmAttributes
	0x32,						// bMaxPower
// control class interface
	0x09,
	DESC_INTERFACE,
	0x00,						// bInterfaceNumber
	0x00,						// bAlternateSetting
	0x01,						// bNumEndPoints
	0x02,						// bInterfaceClass
	0x02,						// bInterfaceSubClass
	0x01,						// bInterfaceProtocol, linux requires value of 1 for the cdc_acm module
	0x00,						// iInterface
// header functional descriptor
	0x05,
	CS_INTERFACE,
	0x00,
	LE_WORD(0x0110),
// call management functional descriptor
	0x05,
	CS_INTERFACE,
	0x01,
	0x01,						// bmCapabilities = device handles call management
	0x01,						// bDataInterface
// ACM functional descriptor
	0x04,
	CS_INTERFACE,
	0x02,
	0x02,						// bmCapabilities
// union functional descriptor
	0x05,
	CS_INTERFACE,
	0x06,
	0x00,						// bMasterInterface
	0x01,						// bSlaveInterface0
// notification EP
	0x07,
	DESC_ENDPOINT,
	INT_IN_EP,					// bEndpointAddress
	0x03,						// bmAttributes = intr
	LE_WORD(8),					// wMaxPacketSize
	0x0A,						// bInterval
// data class interface descriptor
	0x09,
	DESC_INTERFACE,
	0x01,						// bInterfaceNumber
	0x00,						// bAlternateSetting
	0x02,						// bNumEndPoints
	0x0A,						// bInterfaceClass = data
	0x00,						// bInterfaceSubClass
	0x00,						// bInterfaceProtocol
	0x00,						// iInterface
// data EP OUT
	0x07,
	DESC_ENDPOINT,
	BULK_OUT_EP,				// bEndpointAddress
	0x02,						// bmAttributes = bulk
	LE_WORD(MAX_PACKET_SIZE),	// wMaxPacketSize
	0x00,						// bInterval
// data EP in
	0x07,
	DESC_ENDPOINT,
	BULK_IN_EP,					// bEndpointAddress
	0x02,						// bmAttributes = bulk
	LE_WORD(MAX_PACKET_SIZE),	// wMaxPacketSize
	0x00,						// bInterval
	
	// string descriptors
	0x04,
	DESC_STRING,
	LE_WORD(0x0409),

	0x0E,
	DESC_STRING,
	'L', 0, 'P', 0, 'C', 0, 'U', 0, 'S', 0, 'B', 0,

	0x14,
	DESC_STRING,
	'U', 0, 'S', 0, 'B', 0, 'S', 0, 'e', 0, 'r', 0, 'i', 0, 'a', 0, 'l', 0,

	0x12,
	DESC_STRING,
	'D', 0, 'E', 0, 'A', 0, 'D', 0, 'C', 0, '0', 0, 'D', 0, 'E', 0,

// terminating zero
	0
};

extern Stream *g_stream;


void 
USBSerialStream::BulkOut(U8 bEP, U8 bEPStatus)
{
	int i, iLen;
	iLen = USBHwEPRead(bEP, dataBuf, sizeof(dataBuf));

	instance()->m_outfifo.push(dataBuf, iLen);
}


void 
USBSerialStream::BulkIn(U8 bEP, U8 bEPStatus)
{
	static bool bLastWasFull = false;

	instance()->TransferData(false);
	return;

	USBHwEPWrite(bEP, (U8*)"a", 1);
	return;
	if (instance()->m_infifo.size() > 0) {
		int datatoWrite = instance()->m_infifo.size();
		if (datatoWrite >= MAX_PACKET_SIZE) {
			bLastWasFull = true;
			datatoWrite = MAX_PACKET_SIZE;
		} else {
			bLastWasFull = false;
		}
		instance()->m_infifo.pop(dataBuf, datatoWrite);
		USBHwEPWrite(bEP, dataBuf, datatoWrite);
	} else if (bLastWasFull) {
		// I think we need to transmit a zero length package in this case.
		USBHwEPWrite(bEP, dataBuf, 0);
	}

}


static BOOL HandleClassRequest(TSetupPacket *pSetup, int *piLen, U8 **ppbData)
{
	switch (pSetup->bRequest) {
		// set line coding
		case SET_LINE_CODING:
			memcpy((unsigned char *)&LineCoding, *ppbData, 7);
			*piLen = 7;
			break;
	
		// get line coding
		case GET_LINE_CODING:
			*ppbData = (U8 *)&LineCoding;
			*piLen = 7;
			break;
	
		// set control line state
		case SET_CONTROL_LINE_STATE:
			// bit0 = DTR, bit = RTS
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

void 
USBSerialStream::FrameHandler(U16 wFrame)
{
	if (instance()->m_infifo.size() > 0 && 
		(false == instance()->m_bDontStartNew) && 
		(instance()->m_bConnected)) 
	{

		instance()->TransferData(true);

	}
}


void 
USBSerialStream::TransferData(bool bInit)
{
	// We'll end up here both if we come from the frame handler, and the bulkin handler


	if (bInit) {
		m_bTransferInProgress = true;
		m_bDontStartNew = true;
	}

	if (!m_bTransferInProgress) {
		return;
	}



	int dataToSend = m_infifo.size();

	if (dataToSend >= MAX_PACKET_SIZE) {
		dataToSend = MAX_PACKET_SIZE;
	} else {
		m_bTransferInProgress = false;
		m_bDontStartNew = false;
	}

	
	instance()->m_infifo.pop(dataBuf, dataToSend);
	USBHwEPWrite(BULK_IN_EP, dataBuf, dataToSend);
}

void USBHandlerSerial(void)
{
	USBHwISR();
	VICVectAddr = 0x00;    // dummy write to VIC to signal end of ISR 	
}

void 
USBSerialStream::DevIntHandler(unsigned char devStatus)
{
	if (devStatus & DEV_STATUS_RESET) {
		instance()->m_bDontStartNew = false;
		instance()->m_bTransferInProgress = false;
	} else if (devStatus & DEV_STATUS_CONNECT) {
		// We're connected
		instance()->m_bDontStartNew = false;
		instance()->m_bConnected = true;
		instance()->m_bTransferInProgress = false;
	} else if (devStatus & DEV_STATUS_SUSPEND) {
		instance()->m_bConnected = false;
		instance()->m_bTransferInProgress = false;
		instance()->m_bDontStartNew = false;
	} else {
	}

}


USBSerialStream::~USBSerialStream()
{
}

USBSerialStream::USBSerialStream(Stream *echoStream) :  m_infifo(512), m_outfifo(512), m_bTransferInProgress(false), m_bDontStartNew(false),
	m_bConnected(false)
{

	// initialise stack
	USBInit();

	// register descriptors
	USBRegisterDescriptors(abDescriptors);

	// register class request handler
	USBRegisterRequestHandler(REQTYPE_TYPE_CLASS, HandleClassRequest, abClassReqData);

	// register endpoint handlers
	USBHwRegisterEPIntHandler(INT_IN_EP, NULL);
	USBHwRegisterEPIntHandler(BULK_IN_EP, BulkIn);
	USBHwRegisterEPIntHandler(BULK_OUT_EP, BulkOut);

	USBHwRegisterDevIntHandler(DevIntHandler);
	USBHwRegisterFrameHandler(FrameHandler);

	// enable bulk-in interrupts on NAKs
	//USBHwNakIntEnable(INACK_BI);

	// set up USB interrupt
	VICIntSelect &= ~(1<<22);               // select IRQ for USB
	VICIntEnable |= (1<<22);

	(*(&VICVectCntl0+INT_VECT_NUM)) = 0x20 | 22; // choose highest priority ISR slot 	
	(*(&VICVectAddr0+INT_VECT_NUM)) = (int)USBHandlerSerial;

	UART_puts("Connecting USB\n\r");

	// connect to bus
	USBHwConnect(TRUE);

}

USBSerialStream*
USBSerialStream::instance()
{
	if (NULL == m_instance) {
		m_instance = new USBSerialStream(NULL);
	}

	return m_instance;
}

bool 
USBSerialStream::dataReady()
{
	return m_outfifo.size() > 0;
}

int 
USBSerialStream::read(void *buffer, int maxsize)
{

	VICIntEnClr |= (1<<22);

	int datatoRead = (m_outfifo.size() > maxsize)?maxsize:m_outfifo.size();
	m_outfifo.pop((unsigned char *)buffer, datatoRead);
	VICIntEnable |= (1<<22);

	return datatoRead;
}

int 
USBSerialStream::write(const void *buffer, int size)
{
	VICIntEnClr |= (1<<22);
	m_infifo.push((unsigned char *)buffer, size);
	VICIntEnable |= (1<<22);

	return size;
}

