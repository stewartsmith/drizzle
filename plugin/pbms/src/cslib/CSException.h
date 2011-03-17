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
 * 2007-05-20
 *
 * The root of all exceptions.
 *
 */

#pragma once
#ifndef __CSEXEPTION_H__
#define __CSEXEPTION_H__

#include <errno.h>
#include <limits.h>
#include <stdarg.h>

#include "CSDefs.h"
#include "CSString.h"
#include "CSObject.h"

#define CS_ERR_ASSERTION						-14000
#define CS_ERR_EOF								-14001
#define CS_ERR_JUMP_OVERFLOW					-14002
#define CS_ERR_BAD_ADDRESS						-14003
#define CS_ERR_UNKNOWN_SERVICE					-14004
#define CS_ERR_UNKNOWN_HOST						-14005
#define CS_ERR_UNKNOWN_METHOD					-14006
#define CS_ERR_NO_LISTENER						-14007
#define CS_ERR_GENERIC_ERROR					-14008
#define CS_ERR_RELEASE_OVERFLOW					-14009
#define CS_ERR_IMPL_MISSING						-14010
#define CS_ERR_BAD_HEADER_MAGIC					-14011
#define CS_ERR_VERSION_TOO_NEW					-14012
#define CS_ERR_BAD_FILE_HEADER					-14013
#define CS_ERR_BAD_HTTP_HEADER					-14014
#define CS_ERR_INVALID_RECORD					-14015
#define CS_ERR_CHECKSUM_ERROR					-14016
#define CS_ERR_MISSING_HTTP_HEADER				-14017
#define CS_ERR_OPERATION_NOT_SUPPORTED			-14018
#define CS_ERR_BODY_INCOMPLETE					-14019
#define CS_ERR_RECEIVE_TIMEOUT					-14020

#define CS_EXC_CONTEXT_SIZE						300
#define CS_EXC_MESSAGE_SIZE						(PATH_MAX + 300)
#define CS_EXC_DETAILS_SIZE						(CS_EXC_CONTEXT_SIZE + CS_EXC_MESSAGE_SIZE)

class CSException : public CSObject {
public:
	CSException() {
		iErrorCode = 0;
		iContext[0] = 0;
		iMessage[0] = 0;
	}
	virtual ~CSException() { }

	void setErrorCode(int e) { iErrorCode = e; }
	int getErrorCode() { return iErrorCode; }
	const char *getContext(){ return iContext; }
	const char *getMessage(){ return iMessage; }

	void setStackTrace(CSThread *self, const char *stack);
	void setStackTrace(CSThread *self);
	const char *getStackTrace();

	void log(CSThread *self);
	void log(CSThread *self, const char *message);

	void initException_va(const char *func, const char *file, int line, int err, const char *fmt, va_list ap);
	void initException(CSException &exception);
	void initExceptionf(const char *func, const char *file, int line, int err, const char *fmt, ...);
	void initException(const char *func, const char *file, int line, int err, const char *message);
	void initAssertion(const char *func, const char *file, int line, const char *message);
	void getCoreError(uint32_t size, char *buffer, int err);
	void initCoreError(const char *func, const char *file, int line, int err);
	void initCoreError(const char *func, const char *file, int line, int err, const char *item);
	void initOSError(const char *func, const char *file, int line, int err);
	void initFileError(const char *func, const char *file, int line, const char *path, int err);
	void initSignal(const char *func, const char *file, int line, int err);
	void initEOFError(const char *func, const char *file, int line, const char *path);

	static void RecordException(const char *func, const char *file, int line, int err, const char *message);
	static void ClearException();

	static void throwException(const char *func, const char *file, int line, int err, const char *message, const char *stack);
	static void throwException(const char *func, const char *file, int line, int err, const char *message);
	static void throwExceptionf(const char *func, const char *file, int line, int err, const char *fmt, ...);
	static void throwAssertion(const char *func, const char *file, int line, const char *message);
	static void throwCoreError(const char *func, const char *file, int line, int err);
	static void throwCoreError(const char *func, const char *file, int line, int err, const char *item);
	static void throwOSError(const char *func, const char *file, int line, int err);
	static void throwFileError(const char *func, const char *file, int line, const char *path, int err);
	static void throwFileError(const char *func, const char *file, int line, CSString *path, int err);
	static void throwSignal(const char *func, const char *file, int line, int err);
	static void throwEOFError(const char *func, const char *file, int line, const char *path);
	static void throwLastError(const char *func, const char *file, int line); /* Throw the last OS error: */

	static void logOSError(const char *func, const char *file, int line, int err);
	static void logOSError(CSThread *self, const char *func, const char *file, int line, int err);
	static void logException(const char *func, const char *file, int line, int err, const char *message);

private:

	int iErrorCode;
	char iContext[CS_EXC_CONTEXT_SIZE];		/* func(file:line) */
	char iMessage[CS_EXC_MESSAGE_SIZE];		/* The actual message of the error. */
	CSStringBuffer iStackTrace; 
};

#endif
