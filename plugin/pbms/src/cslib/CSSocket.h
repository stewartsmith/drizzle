/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
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

#ifndef __CSSOCKET_H__
#define __CSSOCKET_H__

#include <stdio.h>

#include "CSDefs.h"
#include "CSPath.h"
#include "CSException.h"
#include "CSMemory.h"
#include "CSMutex.h"

using namespace std;

class CSOutputStream;
class CSInputStream;

class CSSocket : public CSRefObject {
public:
	CSSocket() { }

	virtual ~CSSocket() { }

	CSOutputStream *getOutputStream();

	CSInputStream *getInputStream();

	virtual void formatAddress(size_t size, char *address) = 0;

	/*
	 * Publish a listener:
	 */
	virtual void publish(char *service, int default_port) = 0;

	/*
	 * Accept a connection from a listening socket:
	 */
	virtual void open(CSSocket *listener) = 0;

	/*
	 * Connect to a listening socket.
	 */
	virtual void open(char *address, int default_port) = 0;

	/*
	 * Close the socket.
	 */
	virtual void close() { }

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
	virtual size_t read(void *data, size_t size) = 0;

	/*
	 * Returns -1 on EOF!
	 * Otherwize it returns a character value >= 0
	 * Just like read, error on the socket do
	 * not throw an exception.
	 */
	virtual int read() = 0;

	/*
	 * Look at the next character in the file without
	 * taking from the input.
	 */
	virtual int peek() = 0;

	/*
	 * Write the given number of bytes.
	 * Throws IOException if an error occurs.
	 */
	virtual void write(const  void *data, size_t size) = 0;

	/*
	 * Write a character to the file.
	 */
	virtual void write(char ch) = 0;

	/*
	 * Flush the data written.
	 */
	virtual void flush() = 0;

	static CSSocket	*newSocket();

	friend class SCSocket;

private:
};

#define CS_SOCKET_ADDRESS_SIZE		300

class SCSocket : public CSSocket {
public:
	SCSocket(): CSSocket(), iHandle(-1), iHost(NULL), iService(NULL), iPort(0) { }

	virtual ~SCSocket() {
		close();
	}

	virtual void formatAddress(size_t size, char *address);

	virtual void publish(char *service, int default_port);

	virtual void open(CSSocket *listener);

	virtual void open(char *address, int default_port);

	virtual void close();

	virtual size_t read(void *data, size_t size);

	virtual int read();

	virtual int peek();

	virtual void write(const void *data, size_t size);

	virtual void write(char ch);

	virtual void flush();

private:
	void throwError(const char *func, const char *file, int line, char *address, int err);
	void throwError(const char *func, const char *file, int line, int err);
	void setInternalOptions();
	void openInternal();

	int iHandle;
	char *iHost;
	char *iService;
	int iPort;
};

#endif
