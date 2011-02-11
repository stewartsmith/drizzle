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
 * Basic socket I/O.
 *
 */

#include "CSConfig.h"

#include <stdio.h>
#include <sys/types.h>

#ifdef OS_WINDOWS
#include <winsock.h>
typedef int socklen_t;
#define SHUT_RDWR 2
#define CLOSE_SOCKET(s)	closesocket(s)
#define IOCTL_SOCKET	ioctlsocket
#define SOCKET_ERRORNO	WSAGetLastError()
#define	EWOULDBLOCK		WSAEWOULDBLOCK

#else
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <fcntl.h>

extern void unix_close(int h);
#define CLOSE_SOCKET(s)	unix_close(s)
#define IOCTL_SOCKET	ioctl
#define SOCKET_ERRORNO	errno

#endif

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "CSSocket.h"
#include "CSStream.h"
#include "CSGlobal.h"
#include "CSStrUtil.h"
#include "CSFile.h"

void CSSocket::initSockets()
{
#ifdef OS_WINDOWS
	int		err;
	WSADATA	data;
	WORD	version = MAKEWORD (1,1);
	static int inited = 0;

	if (!inited) {
		err = WSAStartup(version, &data);

		if (err != 0) {
			CSException::throwException(CS_CONTEXT, err, "WSAStartup error");
		}
		
		inited = 1;
	}
	
#endif
}

/*
 * ---------------------------------------------------------------
 * CORE SYSTEM SOCKET FACTORY
 */

CSSocket *CSSocket::newSocket()
{
	CSSocket *s;
	
	new_(s, CSSocket());
	return s;
}

/*
 * ---------------------------------------------------------------
 * INTERNAL UTILITIES
 */

void CSSocket::formatAddress(size_t size, char *buffer)
{
	if (iHost) {
		cs_strcpy(size, buffer, iHost);
		if (iService)
			cs_strcat(size, buffer, ":");
	}
	else
		*buffer = 0;
	if (iService)
		cs_strcat(size, buffer, iService);
}

void CSSocket::throwError(const char *func, const char *file, int line, char *address, int err)
{
	if (err)
		CSException::throwFileError(func, file, line, address, err);
	else
		CSException::throwEOFError(func, file, line, address);
}

void CSSocket::throwError(const char *func, const char *file, int line, int err)
{
	char address[CS_SOCKET_ADDRESS_SIZE];

	formatAddress(CS_SOCKET_ADDRESS_SIZE, address);
	throwError(func, file, line, address, err);
}

void CSSocket::setNoDelay()
{
	int flag = 1;

	if (setsockopt(iHandle, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)) == -1)
		CSException::throwOSError(CS_CONTEXT, SOCKET_ERRORNO);
}

void CSSocket::setNonBlocking()
{
	if (iTimeout) {
		unsigned long block = 1;

		if (IOCTL_SOCKET(iHandle, FIONBIO, &block) != 0)
			throwError(CS_CONTEXT, SOCKET_ERRORNO);
	}
}

void CSSocket::setBlocking()
{
	/* No timeout, set blocking: */
	if (!iTimeout) {
		unsigned long block = 0;

		if (IOCTL_SOCKET(iHandle, FIONBIO, &block) != 0)
			throwError(CS_CONTEXT, SOCKET_ERRORNO);
	}
}

void CSSocket::openInternal()
{
	iHandle = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (iHandle == -1)
		CSException::throwOSError(CS_CONTEXT, SOCKET_ERRORNO);
	setNoDelay();
	setNonBlocking();
}

void CSSocket::writeBlock(const void *data, size_t len)
{
	ssize_t	out;

	enter_();
	while (len > 0) {
		out = send(iHandle, (const char *) data, len, 0);
		self->interrupted();
		if (out == -1) {
			int err = SOCKET_ERRORNO;

			if (err == EWOULDBLOCK || err == EINTR)
				continue;
			throwError(CS_CONTEXT, err);
		}
		if ((size_t) out > len)
			break;
		len -= (size_t) out;
		data = ((char *) data) + (size_t) out;
	}
	exit_();
}

int CSSocket::timeoutRead(CSThread *self, void *buffer, size_t length)
{      
	int			in;
	uint64_t	start_time;
	uint64_t	timeout = iTimeout * 1000;
	
	start_time = CSTime::getTimeCurrentTicks();

	retry:
	in = recv(iHandle, (char *) buffer, length, 0);
	if (in == -1) {
		if (SOCKET_ERRORNO == EWOULDBLOCK) {
			fd_set			readfds;
			uint64_t		time_diff;
			struct timeval	tv_timeout;

			FD_ZERO(&readfds);
			self->interrupted();

			time_diff = CSTime::getTimeCurrentTicks() - start_time;
			if (time_diff >= timeout) {
				char address[CS_SOCKET_ADDRESS_SIZE];

				formatAddress(CS_SOCKET_ADDRESS_SIZE, address);
				CSException::throwExceptionf(CS_CONTEXT, CS_ERR_RECEIVE_TIMEOUT, "Receive timeout: %lu ms, on: %s", iTimeout, address);
			}

			/* Calculate how much time we can wait: */
			time_diff = timeout - time_diff;
			tv_timeout.tv_sec = (long)time_diff / 1000000;
			tv_timeout.tv_usec = (long)time_diff % 1000000;

			FD_SET(iHandle, &readfds);
			in = select(iHandle+1, &readfds, NULL, NULL, &tv_timeout);
			if (in != -1)
				goto retry;
		}
	}
 	return in;
}

/*
 * ---------------------------------------------------------------
 * SOCKET BASED ON THE STANDARD C SOCKET
 */

void CSSocket::setTimeout(uint32_t milli_sec)
{
	if (iTimeout != milli_sec) {
		if ((iTimeout = milli_sec))
			setNonBlocking();
		else
			setBlocking();
	}
}

CSOutputStream *CSSocket::getOutputStream()
{
	return CSSocketOutputStream::newStream(RETAIN(this));
}

CSInputStream *CSSocket::getInputStream()
{
	return CSSocketInputStream::newStream(RETAIN(this));
}

void CSSocket::publish(char *service, int default_port)
{
	enter_();
	close();
	try_(a) {
		struct servent		*servp;
		struct sockaddr_in	server;
		struct servent		s;
		int					flag = 1;

		openInternal();
		if (service) {
			if (isdigit(service[0])) {
				int i =  atoi(service);

				if (!i)
					CSException::throwCoreError(CS_CONTEXT, CS_ERR_BAD_ADDRESS, service);
				servp = &s;
				s.s_port = htons((uint16_t) i);
				iService = cs_strdup(service);
			}
			else if ((servp = getservbyname(service, "tcp")) == NULL) {
				if (!default_port)
					CSException::throwCoreError(CS_CONTEXT, CS_ERR_UNKNOWN_SERVICE, service);
				servp = &s;
				s.s_port = htons((uint16_t) default_port);
				iService = cs_strdup(default_port);
			}
			else
				iService = cs_strdup(service);
		}
		else {
			if (!default_port)
				CSException::throwCoreError(CS_CONTEXT, CS_ERR_UNKNOWN_SERVICE, "");
			servp = &s;
			s.s_port = htons((uint16_t) default_port);
			iService = cs_strdup(default_port);
		}
			
		iPort = ntohs(servp->s_port);

		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = INADDR_ANY;
		server.sin_port = (uint16_t) servp->s_port;

		if (setsockopt(iHandle, SOL_SOCKET, SO_REUSEADDR, (char *) &flag, sizeof(int)) == -1)
			CSException::throwOSError(CS_CONTEXT, SOCKET_ERRORNO);

		if (bind(iHandle, (struct sockaddr *) &server, sizeof(server)) == -1)
			CSException::throwOSError(CS_CONTEXT, SOCKET_ERRORNO);

		if (listen(iHandle, SOMAXCONN) == -1)
			CSException::throwOSError(CS_CONTEXT, SOCKET_ERRORNO);
	}
	catch_(a) {
		close();
		throw_();
	}
	cont_(a);
	exit_();
}

void CSSocket::open(CSSocket *listener)
{
	enter_();

	close();
	try_(a) {
		int listener_handle;
		char address[CS_SOCKET_ADDRESS_SIZE];
		struct sockaddr_in	remote;
		socklen_t			addrlen = sizeof(remote);

		/* First get all the information we need from the listener: */
		listener_handle = ((CSSocket *) listener)->iHandle;
		listener->formatAddress(CS_SOCKET_ADDRESS_SIZE, address);

		/* I want to make sure no error occurs after the connect!
		 * So I allocate a buffer for the host name up front.
		 * This means it may be to small, but this is not a problem
		 * because the host name stored here is is only used for display
		 * of error message etc.
		 */
		iHost = (char *) cs_malloc(100);
		iHandle = accept(listener_handle, (struct sockaddr *) &remote, &addrlen);
		if (iHandle == -1)
			throwError(CS_CONTEXT, address, SOCKET_ERRORNO);

		cs_strcpy(100, iHost, inet_ntoa(remote.sin_addr));
		iPort = ntohs(remote.sin_port);

		setNoDelay();
		setNonBlocking();
	}
	catch_(a) {
		close();
		throw_();
	}
	cont_(a);
	exit_();
}

void CSSocket::open(char *address, int default_port)
{
	enter_();
	close();
	try_(a) {
		char				*portp = strchr(address, ':');
		struct servent		s;
		struct servent		*servp;
		struct hostent		*hostp;
		struct sockaddr_in	server;

		openInternal();
		if (!portp) {
			iHost = cs_strdup(address);
			if (!default_port)
				CSException::throwCoreError(CS_CONTEXT, CS_ERR_BAD_ADDRESS, address);
			iService = cs_strdup(default_port);
		}
		else {
			iHost = cs_strdup(address, (size_t) (portp - address));
			iService = cs_strdup(portp+1);
		}
	
		if (isdigit(iService[0])) {
			int i =  atoi(iService);

			if (!i)
				CSException::throwCoreError(CS_CONTEXT, CS_ERR_BAD_ADDRESS, address);
			servp = &s;
			s.s_port = htons((uint16_t) i);
		}
		else if ((servp = getservbyname(iService, "tcp")) == NULL)
			CSException::throwCoreError(CS_CONTEXT, CS_ERR_UNKNOWN_SERVICE, iService);
		iPort = (int) ntohs(servp->s_port);

		if ((hostp = gethostbyname(iHost)) == 0)
			CSException::throwCoreError(CS_CONTEXT, CS_ERR_UNKNOWN_HOST, iHost);

		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		memcpy(&server.sin_addr, hostp->h_addr, (size_t) hostp->h_length);
		server.sin_port = (uint16_t) servp->s_port;
		if (connect(iHandle, (struct sockaddr *) &server, sizeof(server)) == -1)
			throwError(CS_CONTEXT, SOCKET_ERRORNO);
	}
	catch_(a) {
		close();
		throw_();
	}
	cont_(a);
	exit_();
}

void CSSocket::close()
{
	flush();
	if (iHandle != -1) {
		shutdown(iHandle, SHUT_RDWR);
		/* shutdown does not close the socket!!? */
		CLOSE_SOCKET(iHandle);
		iHandle = -1;
	}
	if (iHost) {
		cs_free(iHost);
		iHost = NULL;
	}
	if (iService) {
		cs_free(iService);
		iService = NULL;
	}
	if (iIdentity) {
		cs_free(iIdentity);
		iIdentity = NULL;
	}
	iPort = 0;
}

size_t CSSocket::read(void *data, size_t len)
{
	ssize_t in;

	enter_();
	/* recv, by default will block until at lease one byte is
	 * returned.
	 * So a return of zero means EOF!
	 */
	retry:
	if (iTimeout)
		in = timeoutRead(self, data, len);
	else
		in = recv(iHandle, (char *) data, len, 0);
	self->interrupted();
	if (in == -1) {
		/* Note, we actually ignore all errors on the socket.
		 * If no data was returned by the read so far, then
		 * the error will be considered EOF.
		 */
		int err = SOCKET_ERRORNO;

		if (err == EWOULDBLOCK || err == EINTR)
			goto retry;
		throwError(CS_CONTEXT, err);
		in = 0;
	}
	return_((size_t) in);
}

int CSSocket::read()
{
	int		ch;
	u_char	buffer[1];

	enter_();
	if (read(buffer, 1) == 1)
		ch = buffer[0];
	else
		ch = -1;
	return_(ch);
}

int CSSocket::peek()
{
	return -1;
}

void CSSocket::write(const void *data, size_t len)
{
#ifdef CS_USE_OUTPUT_BUFFER
	if (len <= CS_MIN_WRITE_SIZE) {
		if (iDataLen + len > CS_OUTPUT_BUFFER_SIZE) {
			/* This is the amount of data that will still fit
			 * intp the buffer:
			 */
			size_t tfer = CS_OUTPUT_BUFFER_SIZE - iDataLen;

			memcpy(iOutputBuffer + iDataLen, data, tfer);
			flush();
			len -= tfer;
			memcpy(iOutputBuffer, ((char *) data) + tfer, len);
			iDataLen = len;
		}
		else {
			memcpy(iOutputBuffer + iDataLen, data, len);
			iDataLen += len;
		}
	}
	else {
		/* If the block give is large enough, the
		 * writing directly from the block saves copying the
		 * data to the local output buffer buffer:
		 */
		flush();
		writeBlock(data, len);
	}
#else
	writeBlock(data, len);
#endif
}

void CSSocket::write(char ch)
{
	enter_();
	writeBlock(&ch, 1);
	exit_();
}

void CSSocket::flush()
{
#ifdef CS_USE_OUTPUT_BUFFER
	uint32_t len;

	if ((len = iDataLen)) {
		iDataLen = 0;
		/* Note: we discard the data to be written if an
		 * exception occurs.
		 */
		writeBlock(iOutputBuffer, len);
	}
#endif
}

const char *CSSocket::identify()
{
	enter_();
	if (!iIdentity) {
		char buffer[200];

		formatAddress(200, buffer);
		iIdentity = cs_strdup(buffer);
	}
	return_(iIdentity);
}



