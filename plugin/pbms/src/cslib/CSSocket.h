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
 * 2007-05-24
 *
 * CORE SYSTEM:
 * Basic file I/O.
 *
 */

#pragma once
#ifndef __CSSOCKET_H__
#define __CSSOCKET_H__

#include <stdio.h>

#include "CSDefs.h"
#include "CSPath.h"
#include "CSException.h"
#include "CSMemory.h"
#include "CSMutex.h"

class CSOutputStream;
class CSInputStream;

#define CS_SOCKET_ADDRESS_SIZE		300

/* This is only required if you do
 * not use an output buffer stream!
 */
//#define CS_USE_OUTPUT_BUFFER

#ifdef CS_USE_OUTPUT_BUFFER
#ifdef DEBUG
#define CS_OUTPUT_BUFFER_SIZE		80
#define CS_MIN_WRITE_SIZE			40
#else
#define CS_OUTPUT_BUFFER_SIZE		(4*1024)
#define CS_MIN_WRITE_SIZE			(1500)
#endif
#endif

class CSSocket : public CSRefObject {
public:
	CSSocket(): iHandle(-1), iHost(NULL), iService(NULL), iIdentity(NULL), iPort(0), iTimeout(0) {
#ifdef CS_USE_OUTPUT_BUFFER
	iDataLen = 0;
#endif
	}

	virtual ~CSSocket() {
		close();
	}

	void setTimeout(uint32_t milli_sec);

	CSOutputStream *getOutputStream();

	CSInputStream *getInputStream();

	virtual void formatAddress(size_t size, char *address);

	/*
	 * Publish a listener:
	 */
	virtual void publish(char *service, int default_port);

	/*
	 * Accept a connection from a listening socket:
	 */
	virtual void open(CSSocket *listener);

	/*
	 * Connect to a listening socket.
	 */
	virtual void open(char *address, int default_port);

	/*
	 * Close the socket.
	 */
	virtual void close();

	/*
	 * Read at least one byte from the socket.
	 * This function returns 0 on EOF.
	 * If the function returns at least
	 * one byte, then you must call the function
	 * again, there may be more data available.
	 *
	 * Note: Errors on the socket do not cause
	 * an exception!
	 */
	virtual size_t read(void *data, size_t size);

	/*
	 * Returns -1 on EOF!
	 * Otherwize it returns a character value >= 0
	 * Just like read, error on the socket do
	 * not throw an exception.
	 */
	virtual int read();

	/*
	 * Look at the next character in the file without
	 * taking from the input.
	 */
	virtual int peek();

	/*
	 * Write the given number of bytes.
	 * Throws IOException if an error occurs.
	 */
	virtual void write(const  void *data, size_t size);

	/*
	 * Write a character to the file.
	 */
	virtual void write(char ch);

	/*
	 * Flush the data written.
	 */
	virtual void flush();

	virtual const char *identify();

	static void initSockets();

	static CSSocket	*newSocket();

private:
	void throwError(const char *func, const char *file, int line, char *address, int err);
	void throwError(const char *func, const char *file, int line, int err);
	void setNoDelay();
	void setNonBlocking();
	void setBlocking();
	void openInternal();
	void writeBlock(const void *data, size_t len);
	int timeoutRead(CSThread *self, void *buffer, size_t length);

	int			iHandle;
	char		*iHost;
	char		*iService;
	char		*iIdentity;
	int			iPort;
	uint32_t	iTimeout;

#ifdef CS_USE_OUTPUT_BUFFER
	char	iOutputBuffer[CS_OUTPUT_BUFFER_SIZE];
	size_t	iDataLen;
#endif

};

#endif
