/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Original author: Paul McCullagh (H&G2JCtL)
 * Continued development: Barry Leslie
 *
 * 2007-06-07
 *
 * CORE SYSTEM:
 * Basic input and output streams.
 *
 * These objects wrap the system streams, and simplify things.
 * I also want to standardize exceptions and implement
 * socket based streams.
 *
 */

#include "CSConfig.h"

#include <string.h>
#include <inttypes.h>

#include "CSMemory.h"
#include "CSStream.h"
#include "CSGlobal.h"

/*
 * ---------------------------------------------------------------
 * STREAM UTILITIES
 */

void CSStream::pipe(CSOutputStream *out, CSInputStream *in)
{
	char	*buffer;
	size_t	size;

	enter_();
	push_(out);
	push_(in);
	
	buffer = (char *) cs_malloc(DEFAULT_BUFFER_SIZE);
	push_ptr_(buffer);
	
	for (;;) {
		size = in->read(buffer, DEFAULT_BUFFER_SIZE);
		self->interrupted();
		if (!size)
			break;
		out->write(buffer, size);
		self->interrupted();
	}
	in->close();
	out->close();
	
	release_(buffer);
	release_(in);
	release_(out);
	exit_();
}

/*
 * ---------------------------------------------------------------
 * INPUT STREAMS
 */

CSStringBuffer *CSInputStream::readLine()
{
	int				ch;
	CSStringBuffer	*sb = NULL;

	enter_();
	
	ch = read();
	if (ch != -1) {
		new_(sb, CSStringBuffer(20));
		push_(sb);
		
		while (ch != '\n' && ch != '\r' && ch != -1) {
			sb->append((char) ch);
			ch = read();
		}
		if (ch == '\r') {
			if (peek() == '\n')
				ch = read();
		}

		pop_(sb);
	}

	return_(sb);
}

/*
 * ---------------------------------------------------------------
 * OUTPUT STREAMS
 */

void CSOutputStream::printLine(const char *cstr)
{
	enter_();
	print(cstr);
	print(getEOL());
	flush();
	exit_();
}

void CSOutputStream::print(const char *cstr)
{
	enter_();
	write(cstr, strlen(cstr));
	exit_();
}

void CSOutputStream::print(CSString *s)
{
	enter_();
	print(s->getCString());
	exit_();
}

void CSOutputStream::print(int value)
{
	char buffer[20];

	snprintf(buffer, 20, "%d", value);
	print(buffer);
}

void CSOutputStream::print(uint64_t value)
{
	char buffer[30];

	snprintf(buffer, 30, "%"PRIu64"", value);
	print(buffer);
}

/*
 * ---------------------------------------------------------------
 * FILE INPUT STREAMS
 */

CSFileInputStream::~CSFileInputStream()
{
	if (iFile)
		iFile->release();
}

size_t CSFileInputStream::read(char *b, size_t len)
{
	size_t size;

	enter_();
	size = iFile->read(b, iReadOffset, len, 0);
	iReadOffset += size;
	return_(size);
}

int CSFileInputStream::read()
{
	size_t	size;
	char	ch;

	enter_();
	size = iFile->read(&ch, iReadOffset, 1, 0);
	iReadOffset += size;
	return_(size == 0 ? -1 : (int) ch);
}

void CSFileInputStream::reset()
{
	iReadOffset = 0;
}

const char *CSFileInputStream::identify()
{
	return iFile->myFilePath->getCString();
}

int CSFileInputStream::peek()
{
	size_t	size;
	char	ch;

	enter_();
	size = iFile->read(&ch, iReadOffset, 1, 0);
	return_(size == 0 ? -1 : (int) ch);
}

void CSFileInputStream::close()
{
	enter_();
	iFile->close();
	exit_();
}

CSFileInputStream *CSFileInputStream::newStream(CSFile *f)
{
	CSFileInputStream *s;

	if (!(s = new CSFileInputStream())) {
		f->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	s->iFile = f;
	return s;
}

CSFileInputStream *CSFileInputStream::newStream(CSFile *f, off64_t offset)
{
	CSFileInputStream *s;

	if (!(s = new CSFileInputStream())) {
		f->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	s->iFile = f;
	s->iReadOffset = offset;
	return s;
}

/*
 * ---------------------------------------------------------------
 * FILE OUTPUT STREAMS
 */

CSFileOutputStream::~CSFileOutputStream()
{
	if (iFile)
		iFile->release();
}

void CSFileOutputStream::write(const char *b, size_t len)
{
	enter_();
	iFile->write(b, iWriteOffset, len);
	iWriteOffset += len;
	exit_();
}

const char *CSFileOutputStream::getEOL()
{
	enter_();
	return_(iFile->getEOL());
}

void CSFileOutputStream::flush()
{
	enter_();
	iFile->flush();
	exit_();
}

void CSFileOutputStream::write(char b)
{
	enter_();
	iFile->write(&b, iWriteOffset, 1);
	iWriteOffset += 1;
	exit_();
}

void CSFileOutputStream::reset()
{
	iWriteOffset = 0;
}

const char *CSFileOutputStream::identify()
{
	return iFile->myFilePath->getCString();
}

void CSFileOutputStream::close()
{
	enter_();
	iFile->close();
	exit_();
}

CSFileOutputStream *CSFileOutputStream::newStream(CSFile *f)
{
	CSFileOutputStream *s;

	if (!(s = new CSFileOutputStream())) {
		f->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	s->iFile = f;
	return  s;
}

CSFileOutputStream *CSFileOutputStream::newStream(CSFile *f, off64_t offset)
{
	CSFileOutputStream *s;

	if (!(s = new CSFileOutputStream())) {
		f->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	s->iFile = f;
	s->iWriteOffset = offset;
	return  s;
}

/*
 * ---------------------------------------------------------------
 * SOCKET INPUT STREAMS
 */

CSSocketInputStream::~CSSocketInputStream()
{
	if (iSocket)
		iSocket->release();
}

void CSSocketInputStream::close()
{
	enter_();
	iSocket->close();
	exit_();
}

size_t CSSocketInputStream::read(char *b, size_t len)
{
	enter_();
	return_(iSocket->read(b, len));
}

int CSSocketInputStream::read()
{
	enter_();
	return_(iSocket->read());
}

int CSSocketInputStream::peek()
{
	enter_();
	return_(iSocket->peek());
}

void CSSocketInputStream::reset()
{
	enter_();
	CSException::throwException(CS_CONTEXT, CS_ERR_OPERATION_NOT_SUPPORTED, "CSSocketInputStream::reset() not supported");
	exit_();
}

const char *CSSocketInputStream::identify()
{
	return iSocket->identify();
}

CSSocketInputStream *CSSocketInputStream::newStream(CSSocket *s)
{
	CSSocketInputStream *str;

	if (!(str = new CSSocketInputStream())) {
		s->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	str->iSocket = s;
	return  str;
}

/*
 * ---------------------------------------------------------------
 * SOCKET OUTPUT STREAMS
 */

CSSocketOutputStream::~CSSocketOutputStream()
{
	if (iSocket)
		iSocket->release();
}

void CSSocketOutputStream::close()
{
	enter_();
	iSocket->close();
	exit_();
}

void CSSocketOutputStream::write(const char *b, size_t len)
{
	enter_();
	iSocket->write(b, len);
	exit_();
}

void CSSocketOutputStream::flush()
{
	enter_();
	iSocket->flush();
	exit_();
}

void CSSocketOutputStream::write(char b)
{
	enter_();
	iSocket->write(b);
	exit_();
}

void CSSocketOutputStream::reset()
{
	enter_();
	CSException::throwException(CS_CONTEXT, CS_ERR_OPERATION_NOT_SUPPORTED, "CSSocketOutputStream::reset() not supported");
	exit_();
}

const char *CSSocketOutputStream::identify()
{
	return iSocket->identify();
}

CSSocketOutputStream *CSSocketOutputStream::newStream(CSSocket *s)
{
	CSSocketOutputStream *str;

	if (!(str = new CSSocketOutputStream())) {
		s->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	str->iSocket = s;
	return  str;
}

/*
 * ---------------------------------------------------------------
 * BUFFERED INPUT STREAMS
 */

CSBufferedInputStream::~CSBufferedInputStream()
{
	if (iStream)
		iStream->release();
}

void CSBufferedInputStream::close()
{
	enter_();
	iStream->close();
	exit_();
}

size_t CSBufferedInputStream::read(char *b, size_t len)
{
	size_t tfer;

	enter_();
	if (iBuffPos < iBuffTotal) {
		tfer = iBuffTotal - iBuffPos;
		if (tfer > len)
			tfer = len;
		memcpy(b, iBuffer + iBuffPos, tfer);
		iBuffPos += tfer;
	}
	else
		tfer = iStream->read(b, len);
	return_(tfer);
}

int CSBufferedInputStream::read()
{
	int ch;
	
	enter_();
	if (iBuffPos == iBuffTotal) {
		iBuffTotal = iStream->read((char *) iBuffer, CS_STREAM_BUFFER_SIZE);
		iBuffPos = 0;
	}
	if (iBuffPos < iBuffTotal) {
		ch = iBuffer[iBuffPos];
		iBuffPos++;
	}
	else
		ch = -1;
	return_(ch);
}

int CSBufferedInputStream::peek()
{
	int ch;
	
	enter_();
	if (iBuffPos == iBuffTotal) {
		iBuffTotal = iStream->read((char *) iBuffer, CS_STREAM_BUFFER_SIZE);
		iBuffPos = 0;
	}
	if (iBuffPos < iBuffTotal)
		ch = iBuffer[iBuffPos];
	else
		ch = -1;
	return_(ch);
}

void CSBufferedInputStream::reset()
{
	iBuffPos = iBuffTotal =0;
	iStream->reset();
}

const char *CSBufferedInputStream::identify()
{
	return iStream->identify();
}

CSBufferedInputStream *CSBufferedInputStream::newStream(CSInputStream* i)
{
	CSBufferedInputStream *s;

	if (!(s = new CSBufferedInputStream())) {
		i->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	s->iStream = i;
	return  s;
}

/*
 * ---------------------------------------------------------------
 * BUFFERED OUTPUT STREAMS
 */

CSBufferedOutputStream::~CSBufferedOutputStream()
{
	if (iStream)
		iStream->release();
}

void CSBufferedOutputStream::close()
{
	enter_();
	iStream->close();
	exit_();
}

void CSBufferedOutputStream::write(const char *b, size_t len)
{
	size_t tfer;

	// If the length of the data being written is greater than half
	// the buffer size then the data is written directly through
	// with out buffering.
	enter_();
	if (iBuffTotal < CS_STREAM_BUFFER_SIZE/2) {
		tfer = CS_STREAM_BUFFER_SIZE - iBuffTotal;
		
		if (tfer > len)
			tfer = len;
		memcpy(iBuffer + iBuffTotal, b, tfer);
		iBuffTotal += tfer;
		b += tfer;
		len -= tfer;
	}
	if (len > 0) {
		flush();
		if (len > CS_STREAM_BUFFER_SIZE/2)
			iStream->write(b, len);
		else {
			memcpy(iBuffer, b, len);
			iBuffTotal = len;
		}
	}
	exit_();
}

void CSBufferedOutputStream::flush()
{
	size_t len;

	enter_();
	if ((len = iBuffTotal)) {
		/* Discard the contents of the buffer
		 * if flush fails, because we do
		 * not know how much was written anyway!
		 */
		iBuffTotal = 0;
		iStream->write((char *) iBuffer, len);
	}
	exit_();
}

void CSBufferedOutputStream::write(char b)
{
	enter_();
	if (iBuffTotal == CS_STREAM_BUFFER_SIZE)
		flush();
	iBuffer[iBuffTotal] = b;
	iBuffTotal++;
	exit_();
}

void CSBufferedOutputStream::reset()
{
	iBuffTotal = 0;
	iStream->reset();
}

const char *CSBufferedOutputStream::identify()
{
	return iStream->identify();
}

CSBufferedOutputStream *CSBufferedOutputStream::newStream(CSOutputStream* i)
{
	CSBufferedOutputStream *s;

	if (!(s = new CSBufferedOutputStream())) {
		i->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	s->iStream = i;
	return s;
}

/*
 * ---------------------------------------------------------------
 * MEMORY INPUT STREAMS
 */
CSMemoryInputStream *CSMemoryInputStream::newStream(const u_char* buffer, uint32_t length)
{
	CSMemoryInputStream *s;

	if (!(s = new CSMemoryInputStream())) {
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	s->iMemory = buffer;
	s->iMemTotal = length;
	return s;
}


CSMemoryOutputStream *CSMemoryOutputStream::newStream(size_t init_length, size_t min_alloc)
{
	CSMemoryOutputStream *s;

	if (!(s = new CSMemoryOutputStream())) {
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	
	s->iMemory = (u_char *) cs_malloc(init_length);
	s->iMemTotal = init_length;
	s->iMemSpace = init_length;
	s->iMemPos = s->iMemory;
	s->iMemMin = min_alloc;
	return s;
}

CSMemoryOutputStream::~CSMemoryOutputStream()
{
	if (iMemory)
		cs_free(iMemory);
}

void CSMemoryOutputStream::write(const char *b, size_t len)
{
	if (iMemSpace < len) {
		size_t new_size = iMemTotal + ((len < iMemMin)? iMemMin:len);
		
		cs_realloc((void**) &iMemory, new_size);
		iMemPos = iMemory + (iMemTotal - iMemSpace);
		iMemSpace += (new_size - iMemTotal);
		iMemTotal = new_size;		
	}
	memcpy(iMemPos, b, len);
	iMemPos +=len;
	iMemSpace -= len;
}

void CSMemoryOutputStream::write(const char b)
{
	if (!iMemSpace) {
		cs_realloc((void**) &iMemory, iMemTotal + iMemMin);
		iMemPos = iMemory + iMemTotal;
		iMemSpace += iMemMin;
		iMemTotal += iMemMin;		
	}
	*iMemPos = b;
	iMemPos++;
	iMemSpace--;
}

void CSMemoryOutputStream::reset()
{
	iMemPos = iMemory;
	iMemSpace = iMemTotal;
}

const char *CSMemoryOutputStream::identify()
{
	return "memory stream";
}

/*
 * ---------------------------------------------------------------
 * STATIC (user) MEMORY OUTPUT STREAM
 */
void CSStaticMemoryOutputStream::write(const char *b, size_t len)
{
	if (iMemSpace < len) {
		enter_();
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "CSStaticMemoryOutputStream: overflow");
		exit_();
	}
	memcpy(iMemPos, b, len);
	iMemPos +=len;
	iMemSpace -= len;
}

void CSStaticMemoryOutputStream::write(const char b)
{
	if (!iMemSpace) {
		enter_();
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "CSStaticMemoryOutputStream: overflow");
		exit_();
	}
	*iMemPos = b;
	iMemPos++;
	iMemSpace--;
}

/*
 * ---------------------------------------------------------------
 * Callback InPUT STREAM
 */

CSCallbackInputStream *CSCallbackInputStream::newStream(CSStreamReadCallbackFunc in_callback, void *user_data)
{
	CSCallbackInputStream *s;

	if (!(s = new CSCallbackInputStream())) {
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	
	s->callback = in_callback;
	s->cb_data = user_data;
	return s;
}

