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
 * Basic socket I/O.
 *
 */

#include "CSConfig.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "CSSocket.h"
#include "CSStream.h"
#include "CSGlobal.h"
#include "CSStrUtil.h"
#include "CSFile.h"

/*
 * ---------------------------------------------------------------
 * CORE SYSTEM SOCKET FACTORY
 */

CSSocket *CSSocket::newSocket()
{
	SCSocket *s;
	
	new_(s, SCSocket());
	return (CSSocket *) s;
}

/*
 * ---------------------------------------------------------------
 * INTERNAL UTILITIES
 */

void SCSocket::formatAddress(size_t size, char *buffer)
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

void SCSocket::throwError(const char *func, const char *file, int line, char *address, int err)
{
	if (err)
		CSException::throwFileError(func, file, line, address, err);
	else
		CSException::throwEOFError(func, file, line, address);
}

void SCSocket::throwError(const char *func, const char *file, int line, int err)
{
	char address[CS_SOCKET_ADDRESS_SIZE];

	formatAddress(CS_SOCKET_ADDRESS_SIZE, address);
	throwError(func, file, line, address, err);
}

void SCSocket::setInternalOptions()
{
	int flag = 1;

	if (setsockopt(iHandle, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)) == -1)
		CSException::throwOSError(CS_CONTEXT, errno);
}

void SCSocket::openInternal()
{
	iHandle = socket(AF_INET, SOCK_STREAM, 0);
	if (iHandle == -1)
		CSException::throwOSError(CS_CONTEXT, errno);
	setInternalOptions();
}

/*
 * ---------------------------------------------------------------
 * SOCKET BASED ON THE STANDARD C SOCKET
 */

CSOutputStream *CSSocket::getOutputStream()
{
	return CSSocketOutputStream::newStream(RETAIN(this));
}

CSInputStream *CSSocket::getInputStream()
{
	return CSSocketInputStream::newStream(RETAIN(this));
}

void SCSocket::publish(char *service, int default_port)
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
			CSException::throwOSError(CS_CONTEXT, errno);

		if (bind(iHandle, (struct sockaddr *) &server, sizeof(server)) == -1)
			CSException::throwOSError(CS_CONTEXT, errno);

		if (listen(iHandle, SOMAXCONN) == -1)
			CSException::throwOSError(CS_CONTEXT, errno);
	}
	catch_(a) {
		close();
		throw_();
	}
	cont_(a);
	exit_();
}

void SCSocket::open(CSSocket *listener)
{
	enter_();

	close();
	try_(a) {
		int listener_handle;
		char address[CS_SOCKET_ADDRESS_SIZE];
		struct sockaddr_in	remote;
		socklen_t			addrlen = sizeof(remote);

		/* First get all the information we need from the listener: */
		listener_handle = ((SCSocket *) listener)->iHandle;
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
			throwError(CS_CONTEXT, address, errno);

		cs_strcpy(100, iHost, inet_ntoa(remote.sin_addr));
		iPort = ntohs(remote.sin_port);

		setInternalOptions();
	}
	catch_(a) {
		close();
		throw_();
	}
	cont_(a);
	exit_();
}

void SCSocket::open(char *address, int default_port)
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
			throwError(CS_CONTEXT, errno);
	}
	catch_(a) {
		close();
		throw_();
	}
	cont_(a);
	exit_();
}

void SCSocket::close()
{
	if (iHandle != -1) {
		shutdown(iHandle, SHUT_RDWR);
		/* shutdown does not close the socket!!? */
		unix_file_close(iHandle);
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
	iPort = 0;
}

size_t SCSocket::read(void *data, size_t len)
{
	ssize_t in;

	enter_();
	/* recv, by default will block until at lease one byte is
	 * returned.
	 * So a return of zero means EOF!
	 */
	retry:
	in = recv(iHandle, data, len, 0);
	self->interrupted();
	if (in == -1) {
		/* Note, we actually ignore all errors on the socket.
		 * If no data was returned by the read so far, then
		 * the error will be considered EOF.
		 */
		if (errno == EAGAIN || errno == EINTR)
			goto retry;
		in = 0;
	}
	return_((size_t) in);
}

int SCSocket::read()
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

int SCSocket::peek()
{
	return -1;
}

void SCSocket::write(const void *data, size_t len)
{
	ssize_t	out;

	enter_();
	while (len > 0) {
		out = send(iHandle, data, len, 0);
		self->interrupted();
		if (out == -1) {
			int err = errno;

			if (err == EAGAIN || errno == EINTR)
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

void SCSocket::write(char ch)
{
	enter_();
	write(&ch, 1);
	exit_();
}

void SCSocket::flush()
{
}


