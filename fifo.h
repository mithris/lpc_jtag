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
#ifndef _FIFO_H_
#define _FIFO_H_

#include <cstring>

/** Simple FIFO template used in the stream implementations.
 * 
 */
template <class T>
class fifo
{	
	public:
		fifo(size_t size);
		~fifo();

        /** Pushes data into the fifo
         * Data will be copied.
         * 
         * @param data  - Pointer to the data to be copied.
         * @param size  - Size of the data to be pushed.
		 */
		void push(const T *data, int size);

        /** Pushes a single item into the FIFO.
         * Data will be copied.
		 */
		void push(T item);

        /** Pops a single entry from the FIFO.
         * 
		 * @return T
		 */
		T pop();

        /** Pops data from the fifo.
         * The data is copied to the pointer provided.
         * 
         * @param dest  - Destination
         * @param size  - Number of entries
		 */
		void pop(T *dest, int size);

        /** Returns the amount of data the FIFO contains.
		 * 
		 * @return size_t
		 */
		int size();



		int m_size;
		int m_head;
		int m_tail;
		int	freeSpace();
		T	*m_data;
		private:

};


template <class T>
fifo<T>::fifo(size_t size) : m_size(size), m_head(0), m_tail(0)
{
	m_data = new T[size + 1]; // +1 to avoid one-off problems.
	
}

template <class T>
fifo<T>::~fifo()
{
	delete[] m_data;
}

template <class T>
void fifo<T>::push(const T *data, int dsize)
{
	// Check if the fifo is big enough.
	if (dsize >= (freeSpace() - 1)) {
		//It's not. TODO: How should we handle this?
		return;
	}
	// Check if we got enough room behind the tail

	size_t tailroom = (m_tail >= m_head)?(m_size - m_tail):(m_head - m_tail);

	if (dsize > tailroom) {
		std::memcpy(&m_data[m_tail], data, tailroom * sizeof(T));
		dsize -= tailroom;
		m_tail = 0;
		push(&data[tailroom], dsize);
		return;
	}

	std::memcpy(&m_data[m_tail], data, dsize * sizeof(T));
	m_tail += dsize;
	if (m_tail == m_size) {
		m_tail = 0;
	}
}

template <class T>
void fifo<T>::push(T item)
{
	// TODO: optimize this, tho, would prefer to only use one push function.
	push(&item, 1);
}


template <class T>
void fifo<T>::pop(T *dest, int dsize)
{
	if (dsize > size()) {
		return;
	}

	size_t headroom = (m_tail >= m_head)?(m_tail - m_head): (m_size - m_head);

	if (dsize > headroom) {
		std::memcpy(dest, &m_data[m_head], headroom * sizeof(T));
		dsize -= headroom;
		m_head = 0;
		pop(&dest[headroom], dsize);
		return;
	}

	std::memcpy(dest, &m_data[m_head], dsize * sizeof(T));
	m_head += dsize;
	if (m_head == m_size) {
		m_head = 0;
	}
}

template <class T>
T fifo<T>::pop()
{
	T data;
	pop(&data, 1);
	return data;
}

template <class T>
int fifo<T>::size()
{
	return m_size - freeSpace();
}

/*
head==117
tail==118

(512-118) + 117


*/

template <class T>
int fifo<T>::freeSpace()
{
	if (m_tail >= m_head) {
		return (m_size - m_tail) + (m_head);
	} 
	return (m_head - m_tail);
	
}

#endif // _FIFO_H_
