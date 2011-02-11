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
 * 2007-05-21
 *
 * General logging class
 *
 */

#include "CSConfig.h"

#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "CSLog.h"
#include "CSMemory.h"
#include "CSUTF8.h"
#include "CSStrUtil.h"
#include "CSThread.h"
#include "CSGlobal.h"


//#ifdef DEBUG
//#define DEFAULT_LOG_BUFFER_SIZE			10
//#else
#define DEFAULT_LOG_BUFFER_SIZE			2000
//#endif

/*
 * The global logging object.
 */
CSLog CSL(stdout, CSLog::Warning);

void CSLog::getNow(char *buffer, size_t len)
{
	time_t		ticks;
	struct tm	ltime;

	ticks = time(NULL);
	if (ticks == (time_t) -1) {
		int err = errno;

		fprintf(iStream, "*** ERROR (%d): While getting time\n", err);
		cs_strcpy(len, buffer, "-- TIME? --");
		return;
	}
	localtime_r(&ticks, &ltime);
	strftime(buffer, len, "%y%m%d %H:%M:%S", &ltime);
}

void CSLog::header(CSThread *self, const char *func, const char *file, int line, int level)
{
	char buffer[300];

	getNow(buffer, 300);
	
	fprintf(iStream, "%s", buffer);

	switch (level) {
		case CSLog::Error:
			fprintf(iStream, " [Error] ");
			break;
		case CSLog::Warning:
			fprintf(iStream, " [Warning] ");
			break;
		case CSLog::Trace:
			fprintf(iStream, " [Trace] ");
			break;
		case CSLog::Protocol:
		default:
			fprintf(iStream, " [Note] ");
			break;
	}

	if (self && self->threadName && self->threadName->length() > 0)
		fprintf(iStream, "%s: ", self->threadName->getCString());

	cs_format_context(300, buffer, func, file, line);
	if (*buffer) {
		cs_strcat(300, buffer, " ");
		fprintf(iStream, "%s", buffer);
	}
}

void CSLog::log(CSThread *self, const char *func, const char *file, int line, int level, const char* buffer)
{
	const char	*end_ptr;
	size_t		len;
	size_t ret;

	if (level > iLogLevel)
		return;

	lock();
	while (*buffer) {
		if (iHeaderPending) {
			iHeaderPending = false;
			header(self, func, file, line, level);
		}
		/* Write until the next \n... */
		if ((end_ptr = strchr((char*)buffer, '\n'))) {
			len = end_ptr - buffer;
			ret= fwrite(buffer, len, 1, iStream);
			fprintf(iStream, "\n");
			fflush(iStream);
			iHeaderPending = true;
			len++;
		}
		else {
			len = strlen(buffer);
                        ret = fwrite(buffer, len, 1, iStream);

		}
		buffer += len;
	}
	unlock();
        (void)ret;
}

void CSLog::log(CSThread *self, int level, const char *buffer)
{
	log(self, NULL, NULL, 0, level, buffer);
}

void CSLog::log(CSThread *self, int level, CSString& wstr)
{
	log(self, level, wstr.getCString());
}

void CSLog::log(CSThread *self, int level, CSString* wstr)
{
	log(self, level, wstr->getCString());
}

void CSLog::log(CSThread *self, int level, int v)
{
	char buffer[100];

	snprintf(buffer, 100, "%d", v);
	log(self, level, buffer);
}

void CSLog::eol(CSThread *self, int level)
{
	log(self, level, "\n");
}

void CSLog::logLine(CSThread *self, int level, const char *buffer)
{
	lock();
	log(self, level, buffer);
	eol(self, level);
	unlock();
}

void CSLog::log_va(CSThread *self, int level, const char *func, const char *file, int line, const char *fmt, va_list ap)
{
	char buffer[DEFAULT_LOG_BUFFER_SIZE];
	char *log_string = NULL;

	lock();

#if !defined(va_copy) || defined(OS_SOLARIS)
	int len;

	len = vsnprintf(buffer, DEFAULT_LOG_BUFFER_SIZE-1, fmt, ap);
	if (len > DEFAULT_LOG_BUFFER_SIZE-1)
		len = DEFAULT_LOG_BUFFER_SIZE-1;
	buffer[len] = 0;
	log_string = buffer;
#else
	/* Use the buffer, unless it is too small */
	va_list ap2;

	va_copy(ap2, ap);
	if (vsnprintf(buffer, DEFAULT_LOG_BUFFER_SIZE, fmt, ap) >= DEFAULT_LOG_BUFFER_SIZE) {
		if (vasprintf(&log_string, fmt, ap2) == -1)
			log_string = NULL;
	}
	else
		log_string = buffer;
#endif

	if (log_string) {
		log(self, func, file, line, level, log_string);

		if (log_string != buffer)
			free(log_string);
	}

	unlock();
}

void CSLog::logf(CSThread *self, int level, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	log_va(self, level, NULL, NULL, 0, fmt, ap);
	va_end(ap);
}

void CSLog::logf(CSThread *self, int level, const char *func, const char *file, int line, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	log_va(self, level, func, file, line, fmt, ap);
	va_end(ap);
}

