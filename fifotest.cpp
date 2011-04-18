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
#include "fifo.h"
#include <iostream>

using namespace std;

int main()
{
	fifo<int> intfifo(255);

	int	TestData[255];
	int odata[255];


	srand(10);
	for (int i = 0; i < 255; ++i) {
		TestData[i] = rand();
	}

	
	int nextdata = 0;
	while(intfifo.size() < 240) {
		int rnd = (rand()%5) + 1;
		intfifo.push(&TestData[nextdata], rnd);
		nextdata += rnd;
	}

	cout << "pushed: " << nextdata << "bytes" << endl;

	intfifo.pop(odata, nextdata >> 1);

	for (int i = 0; i < nextdata >> 1; ++i) {
		if (TestData[i] != odata[i]) {
			cout << "Error " << i << endl;
		} else {
			cout << "OK " << i << endl;
		}
	}

	int push2 = 1;
	while(intfifo.size() < 240) {
		intfifo.push(&TestData[push2++], 1);
	}

	intfifo.pop(&odata[nextdata >> 1], nextdata >> 1);

	for (int i = nextdata >> 1; i < nextdata ; ++i) {
		if (TestData[i] != odata[i]) {
			cout << "Error " << i << " " << TestData[i] << " " << odata[i] << endl;
		} else {
			cout << "OK " << i << endl;
		}
	}


	intfifo.pop(odata, 120);

	for (int i = 0; i < 120; ++i) {
		if (odata[i] != TestData[i+1]) {
			cout << "Error " << i << endl;
		} else {
			cout << "Ok " << i << endl;
		}
	}


}



