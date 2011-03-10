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
 * 2007-06-06
 *
 * A basic mutex (mutual exclusion) object.
 *
 */

#include "CSConfig.h"

#include <assert.h>
#ifdef OS_WINDOWS
extern int gettimeofday(struct timeval *tv, struct timezone *tz);
#else
#include <sys/time.h>
#endif

#include <unistd.h>

#include "CSException.h"
#include "CSMutex.h"
#include "CSGlobal.h"
#include "CSLog.h"

/*
 * ---------------------------------------------------------------
 * A MUTEX ENTITY
 */

CSMutex::CSMutex()
#ifdef DEBUG
:
iLocker(NULL),
trace(false)
#endif
{
	int err;

	if ((err = pthread_mutex_init(&iMutex, NULL)))
		CSException::throwOSError(CS_CONTEXT, err);
}

CSMutex::~CSMutex()
{
	pthread_mutex_destroy(&iMutex);
}

void CSMutex::lock()
{
	int err = 0;

#ifdef DEBUG
	int waiting = 2000;
	while (((err = pthread_mutex_trylock(&iMutex)) == EBUSY) && (waiting > 0)) {
		usleep(500);
		waiting--;
	}
	if (err) {
		if (err == EBUSY) {
			CSL.logf(iLocker, CSLog::Protocol, "Thread holding lock.\n");
		}
		
		if ((err) || (err = pthread_mutex_lock(&iMutex)))
			CSException::throwOSError(CS_CONTEXT, err);
	}

	iLocker = CSThread::getSelf();
	if (trace)
		CSL.logf(iLocker, CSLog::Protocol, "Mutex locked\n");
#else
	if ((err = pthread_mutex_lock(&iMutex)))
		CSException::throwOSError(CS_CONTEXT, err);
#endif
}

void CSMutex::unlock()
{
#ifdef DEBUG
	if (trace)
		CSL.logf(iLocker, CSLog::Protocol, "Mutex unlocked\n");
	iLocker = NULL;
#endif
	pthread_mutex_unlock(&iMutex);
}

/*
 * ---------------------------------------------------------------
 * A LOCK ENTITY
 */

CSLock::CSLock():
CSMutex(),
iLockingThread(NULL),
iLockCount(0)
{
}

CSLock::~CSLock()
{
}

void CSLock::lock()
{
	int err;

	enter_();
	if (iLockingThread != self) {
		if ((err = pthread_mutex_lock(&iMutex)))
			CSException::throwOSError(CS_CONTEXT, err);
		iLockingThread = self;
	}
	iLockCount++;
	exit_();
}

void CSLock::unlock()
{
	enter_();
	ASSERT(iLockingThread == self);
	if (!(--iLockCount)) {
		iLockingThread = NULL;
		pthread_mutex_unlock(&iMutex);
	}
	exit_();
}

bool CSLock::haveLock()
{
	enter_();
	return_(iLockingThread == self);
}

/*
 * ---------------------------------------------------------------
 * A SYNCRONISATION ENTITY
 */

CSSync::CSSync():
CSLock()
{
	int err;

	if ((err = pthread_cond_init(&iCondition, NULL)))
		CSException::throwOSError(CS_CONTEXT, err);
}

CSSync::~CSSync()
{
	pthread_cond_destroy(&iCondition);
}

void CSSync::wait()
{
	int err;
	int lock_count;

	enter_();
	ASSERT(iLockingThread == self);
	lock_count = iLockCount;
	iLockCount = 0;
	iLockingThread = NULL;
	err = pthread_cond_wait(&iCondition, &iMutex);
	iLockCount = lock_count;
	iLockingThread = self;
	if (err)
		CSException::throwOSError(CS_CONTEXT, err);
	exit_();
}

void CSSync::wait(time_t milli_sec)
{
	struct timespec	abstime;
	int				lock_count;
	int				err;
	uint64_t		micro_sec;

	enter_();
	struct timeval	now;

	/* Get the current time in microseconds: */
	gettimeofday(&now, NULL);
	micro_sec = (uint64_t) now.tv_sec * (uint64_t) 1000000 + (uint64_t) now.tv_usec;
	
	/* Add the timeout which is in milli seconds */
	micro_sec += (uint64_t) milli_sec * (uint64_t) 1000;

	/* Setup the end time, which is in nano-seconds. */
	abstime.tv_sec = (long) (micro_sec / 1000000);				/* seconds */
	abstime.tv_nsec = (long) ((micro_sec % 1000000) * 1000);	/* and nanoseconds */

	ASSERT(iLockingThread == self);
	lock_count = iLockCount;
	iLockCount = 0;
	iLockingThread = NULL;
	err = pthread_cond_timedwait(&iCondition, &iMutex, &abstime);
	iLockCount = lock_count;
	iLockingThread = self;
	if (err && err != ETIMEDOUT)
		CSException::throwOSError(CS_CONTEXT, err);
	exit_();
}

void CSSync::wakeup()
{
	int err;

	if ((err = pthread_cond_broadcast(&iCondition)))
		CSException::throwOSError(CS_CONTEXT, err);
}



