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
#include <stdio.h>
#include <sys/stat.h>


extern char __eheap_start;
unsigned long heapEnd = 0;


caddr_t _sbrk(size_t incr)
{

	if (0 == heapEnd) {
		heapEnd = (unsigned long)&__eheap_start;
	}

	caddr_t retVal = (caddr_t)heapEnd;

	heapEnd += incr;

	return retVal;
}


int isatty(int fd)
{
	return 1;
}

int _fstat(int fd, struct stat *buf)
{
	memset(buf,0, sizeof(stat));

	buf->st_blocks = 0;
	buf->st_blksize = 1;
	buf->st_mode = S_IFCHR | S_IRWXU | S_IRWXG | S_IRWXO;

	return 0;
}


int _close(int fd)
{
	return 0;
}

off_t _lseek(int fd, off_t offset, int whence)
{
	return 0;
}


int _write(int fd, void *buffer, size_t size)
{
	switch (fd) {
		case 1: // stdout
			//PutString(buffer, size);
			break;
			
	}

	return size;
}

int _read(int fd, void *buffer, size_t size)
{
	return size;
}

int _open(const char *name, int flags, ...)
{
	return -1;
}
