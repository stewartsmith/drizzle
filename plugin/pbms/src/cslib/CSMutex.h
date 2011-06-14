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
 * CORE SYSTEM:
 * A basic mutex (mutual exclusion) object.
 *
 */

#pragma once
#ifndef __CSMUTEX_H__
#define __CSMUTEX_H__

#include <pthread.h>

#include "CSDefs.h"

class CSThread;

class CSMutex {
public:
	CSMutex();
	virtual ~CSMutex();

	virtual void lock();
	virtual void unlock();

	friend class CSLock;
	friend class CSSync;

private:
	pthread_mutex_t	iMutex;
#ifdef DEBUG
	CSThread		*iLocker;
public:
	bool			trace;
#endif
};

class CSLock : public CSMutex {
public:
	CSLock();
	virtual ~CSLock();

	virtual void lock();
	virtual void unlock();
	virtual bool haveLock();

	friend class CSSync;

private:
	CSThread		*iLockingThread;
	int				iLockCount;
};

class CSSync : public CSLock {
public:
	CSSync();
	virtual ~CSSync();
	
	/* Wait for resources on the object
	 * to be freed.
	 *
	 * This function may only be called
	 * if the thread has already locked the
	 * object.
	 */
	virtual void wait();

	/* Wait for a certain amount of time, in
	 * milli-seconds (1/1000th of a second).
	 */
	void wait(time_t mill_sec);

	/*
	 * Wakeup any waiters.
	 */
	virtual void wakeup();

private:
	pthread_cond_t	iCondition;
};

#endif

