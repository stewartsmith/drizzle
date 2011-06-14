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
 * Original Author: Paul McCullagh
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

#pragma once
#ifndef __CSSTREAM_H__
#define __CSSTREAM_H__

#define DEFAULT_BUFFER_SIZE		64000

#include "CSDefs.h"
#include "CSFile.h"
#include "CSSocket.h"

class CSInputStream : public CSRefObject {
public:
	CSInputStream() { }
	virtual ~CSInputStream() { }

	/*
	 * Closes this input stream and releases any system
	 * resources associated  with the stream.
	 */
	virtual void close() = 0;

	/*
	 * Reads up to len bytes of data from the input stream into an
	 * array of bytes. Return the number of bytes read.
	 *
	 * Zero will be returned in the case of EOF.
	 *
	 * This call blocks until at least one byte is
	 * returned.
	 */
	virtual size_t read(char *b, size_t len) = 0;

	/* Reads the next byte of data from the input stream.
	 * Returns -1 if EOF.
	 */
	virtual int read() = 0;

	/* Read one character ahead. */
	virtual int peek() = 0;

	/*
	 *  Reset this output stream to the start inorder to restart the write.
	 */
	virtual void reset() = 0; 

	/* Return the name of the file, or whatever: */
	virtual const char *identify() = 0;

	/*
	 * Read a line from the input stream. This function
	 * handles all types of line endings. The function
	 * return NULL on EOF.
	 */
	CSStringBuffer *readLine();
};

class CSOutputStream : public CSRefObject {
public:
 	/*
	 * Closes this input stream and releases any system
	 * resources associated  with the stream.
	 */
	virtual void close() = 0;

	/*
	 * Writes len bytes from the specified byte array starting at
	 * offset off to this output stream.
	 */
	virtual void write(const char *b, size_t len) = 0;

	/*
	 * Returns the default EOL indicator.
	 * Will be \n, \r or \r\n.
	 */
	virtual const char *getEOL() = 0;

	/*
	 *  Flushes this output stream and forces any buffered output
	 * bytes to be written out.
	 */
	virtual void flush() = 0;

	/*
	 * Writes the specified byte to this output stream.
	 */
	virtual void write(char b) = 0; 

	/*
	 * Reset this output stream to the start inorder to restart the write.
	 */
	virtual void reset() = 0; 

	virtual const char *identify() = 0;

	/*
	 * Write a line. Terminator is specific to the
	 * type of output stream and may depend on the
	 * current platform.
	 */
	void printLine(const char *cstr);

	/*
	 * Various write types:
	 */
	virtual void print(const char *cstr);
	virtual void print(CSString *s);
	virtual void print(int value);
	virtual void print(uint64_t value);
};

class CSStream : public CSObject {
public:
	static void pipe(CSOutputStream *out, CSInputStream *in);
};

/* File Stream: */

class CSFileInputStream : public CSInputStream {
public:
	CSFileInputStream(): iFile(NULL), iReadOffset(0) { }
	virtual ~CSFileInputStream();

	virtual void close();

	virtual size_t read(char *b, size_t len);

	virtual int read();

	virtual int peek();

	/*
	 *  Reset this output stream to the start inorder to restart the read.
	 */
	virtual void reset(); 

	virtual const char *identify();

	static CSFileInputStream *newStream(CSFile* f);
	static CSFileInputStream *newStream(CSFile* f, off64_t offset);

private:
	CSFile	*iFile;
	off64_t	iReadOffset;
};

class CSFileOutputStream : public CSOutputStream {
public:
	CSFileOutputStream(): iFile(NULL), iWriteOffset(0) { }
	virtual ~CSFileOutputStream();

	virtual void close();

	virtual void write(const char *b, size_t len);

	virtual const char *getEOL();

	virtual void flush();

	virtual void write(char b);

	virtual void reset(); 

	virtual const char *identify();

	static CSFileOutputStream *newStream(CSFile* f);
	static CSFileOutputStream *newStream(CSFile* f, off64_t offset);

private:
	CSFile	*iFile;
	off64_t	iWriteOffset;
};

/* Socket Stream */

class CSSocketInputStream : public CSInputStream {
public:
	CSSocketInputStream(): iSocket(NULL) { }
	virtual ~CSSocketInputStream();

	virtual void close();

	virtual size_t read(char *b, size_t len);

	virtual int read();

	virtual int peek();

	virtual void reset(); 

	virtual const char *identify();

	static CSSocketInputStream *newStream(CSSocket *s);

private:
	CSSocket* iSocket;
};

class CSSocketOutputStream : public CSOutputStream {
public:
	CSSocketOutputStream(): iSocket(NULL) { }
	virtual ~CSSocketOutputStream();

	virtual void close();

	virtual void write(const char *b, size_t len);

	virtual const char *getEOL() { return "\n"; };

	virtual void flush();

	virtual void write(char b);

	virtual void reset(); 

	virtual const char *identify();

	static CSSocketOutputStream *newStream(CSSocket *s);

private:
	CSSocket* iSocket;
};

/* Buffered Stream: */
#ifdef DEBUG_disabled
#define CS_STREAM_BUFFER_SIZE			80
#else
#define CS_STREAM_BUFFER_SIZE			(32 * 1024)
#endif

class CSBufferedInputStream : public CSInputStream {
public:
	CSBufferedInputStream(): iStream(NULL), iBuffTotal(0), iBuffPos(0) { }
	virtual ~CSBufferedInputStream();

	virtual void close();

	virtual size_t read(char *b, size_t len);

	virtual int read();

	virtual int peek();

	virtual void reset(); 

	virtual const char *identify();

	static CSBufferedInputStream *newStream(CSInputStream* i);

private:
	CSInputStream* iStream;
	u_char iBuffer[CS_STREAM_BUFFER_SIZE];
	uint32_t iBuffTotal;
	uint32_t iBuffPos;
};

class CSBufferedOutputStream : public CSOutputStream {
public:
	CSBufferedOutputStream(): iStream(NULL), iBuffTotal(0) { }
	virtual ~CSBufferedOutputStream();

	virtual void close();

	virtual void write(const char *b, size_t len);

	virtual const char *getEOL() { return "\n"; };

	virtual void flush();

	virtual void write(char b);

	virtual void reset(); 

	virtual const char *identify();

	static CSBufferedOutputStream *newStream(CSOutputStream* i);

private:
	CSOutputStream* iStream;
	u_char iBuffer[CS_STREAM_BUFFER_SIZE];
	uint32_t iBuffTotal;
};

/* memory Stream */
class CSMemoryInputStream : public CSInputStream {
public:
	CSMemoryInputStream(): iMemory(NULL), iMemTotal(0), iMemPos(0) { }
	~CSMemoryInputStream(){}

	virtual void close() {}

	virtual size_t read(char *b, size_t len)
	{
		if (len > (iMemTotal - iMemPos))
			len = iMemTotal - iMemPos;
		
		memcpy(b, iMemory + iMemPos, len);
		iMemPos += len;	
		return len;
	}

	virtual int read()
	{
		int b = -1;
		if (iMemPos < iMemTotal) 
			b = iMemory[iMemPos++];
		return b;
	}

	virtual int peek()
	{
		int b = -1;
		if (iMemPos < iMemTotal) 
			b = iMemory[iMemPos];
		return b;
	}

	virtual void reset() {iMemPos = 0;}
	
	virtual const char *identify() 
	{
		return "memory stream";
	}

	static CSMemoryInputStream *newStream(const u_char* buffer, uint32_t length);

private:
	const u_char *iMemory;
	uint32_t iMemTotal;
	uint32_t iMemPos;
};


class CSMemoryOutputStream : public CSOutputStream {
public:
	CSMemoryOutputStream(): iMemory(NULL), iMemTotal(0), iMemSpace(0), iMemMin(0), iMemPos(NULL){ }
	virtual ~CSMemoryOutputStream();

	virtual void close() {}

	virtual void write(const char *b, size_t len);
	virtual const char *getEOL() { return "\n"; };

	virtual void flush() {}

	virtual void write(char b);

	const u_char *getMemory(size_t *len)
	{
		*len = iMemPos - iMemory;
		return iMemory;
	}
	
	virtual void reset();
	
	virtual const char *identify();
	
	static CSMemoryOutputStream *newStream(size_t init_length, size_t min_alloc);

private:
	u_char *iMemory;
	uint32_t iMemTotal;
	uint32_t iMemSpace;
	uint32_t iMemMin;
	u_char *iMemPos;
};

class CSStaticMemoryOutputStream : public CSOutputStream {
public:
	CSStaticMemoryOutputStream(u_char *mem, off64_t size): iMemory(mem), iMemSpace(size), iMemSize(size), iMemPos(mem){ }
	virtual ~CSStaticMemoryOutputStream() {}

	virtual void close() {}

	virtual void write(const char *b, size_t len);
	virtual const char *getEOL() { return "\n"; };

	virtual void flush() {}

	virtual void write(char b);
	
	virtual void reset() 
	{
		iMemPos = iMemory;
		iMemSpace = iMemSize;
	}
	
	virtual const char *identify() 
	{
		return "memory stream";
	}
	
	off64_t getSize() { return iMemPos - iMemory; }

private:
	u_char *iMemory;
	off64_t iMemSpace;
	off64_t iMemSize;
	u_char *iMemPos;
};

typedef size_t (* CSStreamReadCallbackFunc) (void *caller_data, char *buffer, size_t buffer_size, u_char reset);

class CSCallbackInputStream : public CSInputStream {
public:
	CSCallbackInputStream(): callback(NULL), cb_data(NULL), havePeek(false), doReset(false) { }
	~CSCallbackInputStream(){}

	virtual void close() {}

	virtual size_t read(char *b, size_t len)
	{
		size_t size = 0;
		
		if (havePeek) {
			havePeek = false;
			*b =  peek_char;
			b++; len--;
			if (len) {
				size = callback(cb_data, b, len, doReset);
			}
				
			size++;			
		} else
			size = callback(cb_data, b, len, doReset);
			
		if (doReset)
			doReset = false;
			
		return size;
	}

	virtual int read()
	{
		char c;
		
		if (havePeek) {
			havePeek = false;
			return peek_char;
		}
		if (!callback(cb_data, &c, 1, doReset))
			c = -1;
			
		if (doReset)
			doReset = false;
		
		return c;
	}

	virtual int peek()
	{
		if (!havePeek) {
			if (callback(cb_data, &peek_char, 1, doReset))
				havePeek = true;
			else
				return -1;
		}
		return peek_char;
	}

	virtual void reset() 
	{
		havePeek = false;
		doReset = false;
	}

	virtual const char *identify() 
	{
		return "callback stream";
	}

	static CSCallbackInputStream *newStream(CSStreamReadCallbackFunc callback, void *user_data);

private:
	CSStreamReadCallbackFunc callback;
	void *cb_data;
	char peek_char;
	bool havePeek;
	bool doReset;
};

#endif


