#ifndef _FLASH_DRIVER_H_
#define _FLASH_DRIVER_H_


class target;

class FlashDriver
{
	public:

		virtual void initialize(target *t) = 0;

		virtual bool eraseSector(int sector) = 0;
		virtual bool writeFlash(unsigned long address, const unsigned char *data, unsigned long size) = 0;

		virtual bool eraseFlash(unsigned long start, unsigned long length) = 0;

};


#endif // _FLASH_DRIVER_H_

