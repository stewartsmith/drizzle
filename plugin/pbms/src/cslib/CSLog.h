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
 * CORE SYSTEM:
 * General logging class
 *
 */

#pragma once
#ifndef __CSLOG_H__
#define __CSLOG_H__

#include <stdio.h>
#include <stdarg.h>


#include "CSDefs.h"
#include "CSString.h"

class CSLog {
public:
	static const int Protocol = 0;
	static const int Error = 1;
	static const int Warning = 2;
	static const int Trace = 3;

	CSLog(FILE *s, int level):
		iStream(s),
		iHeaderPending(true),
		iLogLevel(level),
		iLockCount(0)
		 {
		pthread_mutex_init(&iMutex, NULL);
		memset(&iLockThread, 0, sizeof(iLockThread));
	}

	virtual ~CSLog() {
		memset(&iLockThread, 0, sizeof(iLockThread));
		iLockCount = 0;
		pthread_mutex_destroy(&iMutex);
	}

	void lock() {
		pthread_t	thd = pthread_self();

		if (iLockCount > 0 && pthread_equal(iLockThread, thd))
			iLockCount++;
		else {
			pthread_mutex_lock(&iMutex);
			iLockThread = thd;
			iLockCount = 1;
		}
	}

	void unlock() {
		if (iLockCount > 0) {
			iLockCount--;
			if (iLockCount == 0)
				pthread_mutex_unlock(&iMutex);
		}
	}

	void getNow(char *buffer, size_t len);
	void log(CSThread *self, const char *func, const char *file, int line, int level, const char* buffer);
	void log(CSThread *self, int level, const char*);
	void log(CSThread *self, int level, CSString&);
	void log(CSThread *self, int level, CSString*);
	void log(CSThread *self, int level, int);
	void eol(CSThread *self, int level);

	void logLine(CSThread *self, int level, const char *buffer);
	void log_va(CSThread *self, int level, const char *func, const char *file, int line, const char *fmt, va_list ap);
	void logf(CSThread *self, int level, const char *fmt, ...);
	void logf(CSThread *self, int level, const char *func, const char *file, int line, const char *fmt, ...);
	
	void flush() {fflush(iStream);}
private:
	/* Write out a logging header: */
	void header(CSThread *self, const char *func, const char *file, int line, int level);

	/* The output stream: */
	FILE *iStream;

	bool iHeaderPending;		/* True if we must write a header before the next text. */

	int iLogLevel;				/* The current log level. */

	pthread_t iLockThread;
	int iLockCount;
	pthread_mutex_t	iMutex;
};

extern CSLog CSL;

#endif
