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
 * The main exception classes
 *
 */

#include "CSConfig.h"

#ifdef OS_WINDOWS
#define strsignal(s) NULL
#else
#include <sys/signal.h>
#endif

#include <limits.h>
#include <string.h>

#include "CSGlobal.h"
#include "CSException.h"
#include "CSStrUtil.h"
#include "CSLog.h"

void CSException::setStackTrace(CSThread *self, const char *stack)
{
	char buffer[CS_EXC_CONTEXT_SIZE];

	self->myException.iStackTrace.setLength(0);
	if (stack)
		self->myException.iStackTrace.append(stack);
	for (int i=self->callTop-1; i>=0; i--) {
		cs_format_context(CS_EXC_CONTEXT_SIZE, buffer,
			self->callStack[i].cs_func, self->callStack[i].cs_file, self->callStack[i].cs_line);
		self->myException.iStackTrace.append(buffer);
		self->myException.iStackTrace.append('\n');
	}
}

void CSException::setStackTrace(CSThread *self)
{
	setStackTrace(self, NULL);
}

const char *CSException::getStackTrace()
{
	return iStackTrace.getCString();
}

void CSException::log(CSThread *self)
{
	CSL.lock();
	CSL.log(self, CSLog::Error, getContext());
	CSL.log(self, CSLog::Error, " ");
	CSL.log(self, CSLog::Error, getMessage());
	CSL.eol(self, CSLog::Error);
#ifdef DUMP_STACK_TRACE
	CSL.log(self, CSLog::Error, getStackTrace());
#endif
	CSL.unlock();
}

void CSException::log(CSThread *self, const char *message)
{
	CSL.lock();
	CSL.log(self, CSLog::Error, message);
	CSL.eol(self, CSLog::Error);
	CSL.log(self, CSLog::Error, getContext());
	CSL.log(self, CSLog::Error, " ");
	CSL.log(self, CSLog::Error, getMessage());
	CSL.eol(self, CSLog::Error);
#ifdef DUMP_STACK_TRACE
	CSL.log(self, CSLog::Error, getStackTrace());
#endif
	CSL.unlock();
}

void CSException::initException_va(const char *func, const char *file, int line, int err, const char *fmt, va_list ap)
{

	cs_format_context(CS_EXC_CONTEXT_SIZE, iContext, func, file, line);
	iErrorCode = err;
#ifdef OS_WINDOWS
	vsprintf(iMessage, fmt, ap);
#else
	size_t len;
	len = vsnprintf(iMessage, CS_EXC_MESSAGE_SIZE-1, fmt, ap);
	if (len > CS_EXC_MESSAGE_SIZE-1)
		len = CS_EXC_MESSAGE_SIZE-1;
	iMessage[len] = 0;
#endif
}

void CSException::initExceptionf(const char *func, const char *file, int line, int err, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	initException_va(func, file, line, err, fmt, ap);
	va_end(ap);
}

void CSException::initException(const char *func, const char *file, int line, int err, const char *message)
{
	cs_format_context(CS_EXC_CONTEXT_SIZE, iContext, func, file, line);
	iErrorCode = err;
	cs_strcpy(CS_EXC_MESSAGE_SIZE, iMessage, message);
}

void CSException::initException(CSException &exception)
{
	iErrorCode = exception.iErrorCode;
	strcpy(iContext, exception.iContext);
	strcpy(iMessage, exception.iMessage);
	
	iStackTrace.setLength(0);
	iStackTrace.append(exception.iStackTrace.getCString());

}

void CSException::initAssertion(const char *func, const char *file, int line, const char *message)
{
	cs_format_context(CS_EXC_CONTEXT_SIZE, iContext, func, file, line);
	iErrorCode = CS_ERR_ASSERTION;
	cs_strcpy(CS_EXC_MESSAGE_SIZE, iMessage, "Assertion failed: ");
	cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, message);
}

void CSException::getCoreError(uint32_t size, char *buffer, int err)
{
	const char *message = NULL;

	switch (err) {
		case CS_ERR_JUMP_OVERFLOW: message = "Jump stack overflow"; break;
		case CS_ERR_BAD_ADDRESS: message = "Incorrect network address: %"; break;
		case CS_ERR_UNKNOWN_SERVICE: message = "Unknown network service: %"; break;
		case CS_ERR_UNKNOWN_HOST:  message = "Unknown host: %"; break;
		case CS_ERR_UNKNOWN_METHOD: message = "Unknown HTTP method: %"; break;
		case CS_ERR_NO_LISTENER: message = "Listening port has been closed"; break;
		case CS_ERR_RELEASE_OVERFLOW: message = "Release stack overflow"; break;
		case CS_ERR_IMPL_MISSING: message = "Function %s not implemented"; break;
		case CS_ERR_BAD_HEADER_MAGIC: message = "Incorrect file type"; break;
		case CS_ERR_VERSION_TOO_NEW: message = "Incompatible file version"; break;
	}
	if (message)
		cs_strcpy(size, buffer, message);
	else {
		cs_strcpy(size, buffer, "Unknown system error ");
		cs_strcat(size, buffer, err);
	}
}

void CSException::initCoreError(const char *func, const char *file, int line, int err)
{
	cs_format_context(CS_EXC_CONTEXT_SIZE, iContext, func, file, line);
	iErrorCode = err;
	getCoreError(CS_EXC_MESSAGE_SIZE, iMessage, err);
}

void CSException::initCoreError(const char *func, const char *file, int line, int err, const char *item)
{
	cs_format_context(CS_EXC_CONTEXT_SIZE, iContext, func, file, line);
	iErrorCode = err;
	getCoreError(CS_EXC_MESSAGE_SIZE, iMessage, err);
	cs_replace_string(CS_EXC_MESSAGE_SIZE, iMessage, "%s", item);
}

void CSException::initOSError(const char *func, const char *file, int line, int err)
{
	char *msg;

	cs_format_context(CS_EXC_CONTEXT_SIZE, iContext, func, file, line);
	iErrorCode = err;

#ifdef XT_WIN
	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, iMessage, CS_EXC_MESSAGE_SIZE, NULL)) {
		char *ptr;

		ptr = &iMessage[strlen(iMessage)];
		while (ptr-1 > err_msg) {
			if (*(ptr-1) != '\n' && *(ptr-1) != '\r' && *(ptr-1) != '.')
				break;
			ptr--;
		}
		*ptr = 0;

		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, " (");
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, err);
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, ")");
		return;
	}
#endif

	msg = strerror(err);
	if (msg) {
		cs_strcpy(CS_EXC_MESSAGE_SIZE, iMessage, msg);
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, " (");
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, err);
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, ")");
	}
	else {
		cs_strcpy(CS_EXC_MESSAGE_SIZE, iMessage, "Unknown OS error code ");
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, err);
	}
}

void CSException::initFileError(const char *func, const char *file, int line, const char *path, int err)
{
	initOSError(func, file, line, err);
	cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, ": '");
	cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, path);
	cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, "'");
}

void CSException::initSignal(const char *func, const char *file, int line, int sig)
{
	char *str;

	cs_format_context(CS_EXC_CONTEXT_SIZE, iContext, func, file, line);
	iErrorCode = sig;
	if (!(str = strsignal(sig))) {
		cs_strcpy(CS_EXC_MESSAGE_SIZE, iMessage, "Unknown signal ");
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, sig);
	}
	else {
		cs_strcpy(CS_EXC_MESSAGE_SIZE, iMessage, str);
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, " (");
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, sig);
		cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, ")");
	}
}

void CSException::initEOFError(const char *func, const char *file, int line, const char *path)
{
	cs_format_context(CS_EXC_CONTEXT_SIZE, iContext, func, file, line);
	iErrorCode = CS_ERR_EOF;
	cs_strcpy(CS_EXC_MESSAGE_SIZE, iMessage, "EOF encountered: '");
	cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, path);
	cs_strcat(CS_EXC_MESSAGE_SIZE, iMessage, "'");
}

void CSException::RecordException(const char *func, const char *file, int line, int err, const char *message)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		if (!self->myException.getErrorCode())
			self->myException.initException(func, file, line, err, message);
	}
}

void CSException::ClearException()
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->myException.setErrorCode(0);
	}
}

void CSException::throwException(const char *func, const char *file, int line, int err, const char *message, const char *stack)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->myException.initException(func, file, line, err, message);
		self->myException.setStackTrace(self, stack);
		self->throwException();
	}
	else {
		CSException e;
		
		e.initException(func, file, line, err, message);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwException(const char *func, const char *file, int line, int err, const char *message)
{
	throwException(func, file, line, err, message, NULL);
}

void CSException::throwExceptionf(const char *func, const char *file, int line, int err, const char *fmt, ...)
{
	CSThread	*self;
	va_list		ap;

	va_start(ap, fmt);
	if ((self = CSThread::getSelf())) {
		self->myException.initException_va(func, file, line, err, fmt, ap);
		va_end(ap);
		self->myException.setStackTrace(self, NULL);
		self->throwException();
	}
	else {
		CSException e;
		
		e.initException_va(func, file, line, err, fmt, ap);
		va_end(ap);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwAssertion(const char *func, const char *file, int line, const char *message)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->myException.initAssertion(func, file, line, message);
		self->myException.setStackTrace(self);
		/* Not sure why we log the excpetion down here?!
		self->logException();
		*/
		self->throwException();
	}
	else {
		CSException e;
		
		e.initAssertion(func, file, line, message);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwCoreError(const char *func, const char *file, int line, int err)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->myException.initCoreError(func, file, line, err);
		self->myException.setStackTrace(self);
		self->throwException();
	}
	else {
		CSException e;
		
		e.initCoreError(func, file, line, err);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwCoreError(const char *func, const char *file, int line, int err, const char *item)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->myException.initCoreError(func, file, line, err, item);
		self->myException.setStackTrace(self);
		self->throwException();
	}
	else {
		CSException e;
		
		e.initCoreError(func, file, line, err, item);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwOSError(const char *func, const char *file, int line, int err)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		/* A pending signal has priority over a system error,
		 * In fact, the pending signal may be the reason for
		 * system error:
		 */
		self->interrupted();
		self->myException.initOSError(func, file, line, err);
		self->myException.setStackTrace(self);
		self->throwException();
	}
	else {
		CSException e;
		
		e.initOSError(func, file, line, err);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwFileError(const char *func, const char *file, int line, const char *path, int err)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->interrupted();
		self->myException.initFileError(func, file, line, path, err);
		self->myException.setStackTrace(self);
		self->throwException();
	}
	else {
		CSException e;
		
		e.initFileError(func, file, line, path, err);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwFileError(const char *func, const char *file, int line, CSString *path, int err)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->interrupted();
		self->myException.initFileError(func, file, line, path->getCString(), err);
		self->myException.setStackTrace(self);
		self->throwException();
	}
	else {
		CSException e;
		
		e.initFileError(func, file, line, path->getCString(), err);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwSignal(const char *func, const char *file, int line, int sig)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->myException.initSignal(func, file, line, sig);
		self->myException.setStackTrace(self);
		self->throwException();
	}
	else {
		CSException e;
		
		e.initSignal(func, file, line, sig);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwEOFError(const char *func, const char *file, int line, const char *path)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->interrupted();
		self->myException.initEOFError(func, file, line, path);
		self->myException.setStackTrace(self);
		self->throwException();
	}
	else {
		CSException e;
		
		e.initEOFError(func, file, line, path);
		e.log(NULL, "*** Uncaught error");
	}
}

void CSException::throwLastError(const char *func, const char *file, int line)
{
#ifdef OS_WINDOWS
	throwOSError(func, file, line, (int) GetLastError());
#else
	throwOSError(func, file, line, (int) errno);
#endif
}

void CSException::logOSError(const char *func, const char *file, int line, int err)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->myException.initOSError(func, file, line, err);
		self->myException.setStackTrace(self);
		self->logException();
	}
	else {
		CSException e;
		
		e.initOSError(func, file, line, err);
		e.log(NULL);
	}
}

void CSException::logOSError(CSThread *self, const char *func, const char *file, int line, int err)
{
	self->myException.initOSError(func, file, line, err);
	self->myException.setStackTrace(self);
	self->logException();
}

void CSException::logException(const char *func, const char *file, int line, int err, const char *message)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		self->myException.initException(func, file, line, err, message);
		self->logException();
	}
	else {
		CSException e;
		
		e.initException(func, file, line, err,message);
		e.log(NULL);
	}
}


