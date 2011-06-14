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
 * A independently running thread.
 *
 */

#pragma once
#ifndef __CSTHREAD_H__
#define __CSTHREAD_H__

#include <pthread.h>
#include <setjmp.h>

#include "CSDefs.h"
#include "CSMutex.h"
#include "CSException.h"
#include "CSObject.h"
#include "CSStorage.h"

#define CS_THREAD_TYPE				int

/* Types of threads: */
#define CS_ANY_THREAD				0
#define CS_THREAD					1

typedef struct CSCallStack {
	const char*	cs_func;
	const char*	cs_file;
	int			cs_line;
} CSCallStack, *CSCallStackPtr;

/* 
 * The release stack contains objects that need to be
 * released when an exception occurs.
 */
#define CS_RELEASE_OBJECT		1
#define CS_RELEASE_MUTEX		2
#define CS_RELEASE_POOLED		3
#define CS_RELEASE_MEM			4
#define CS_RELEASE_OBJECT_PTR	5

typedef struct CSRelease {
	int						r_type;
	union {
		CSObject			*r_object;					/* The object to be released. */
		CSMutex				*r_mutex;					/* The mutex to be unlocked! */
		CSPooled			*r_pooled;
		void				*r_mem;
		CSObject			**r_objectPtr;
	} x;
} CSReleaseRec, *CSReleasePtr;

typedef struct CSJumpBuf {
	CSReleasePtr			jb_res_top;
	int						jb_call_top;
	jmp_buf					jb_buffer;
} CSJumpBufRec, *CSJumpBufPtr;

class CSThreadList: public CSLinkedList, public CSMutex {
public:
	CSThreadList():
		CSLinkedList(),
		CSMutex()
	{
	}

	virtual ~CSThreadList() {
		stopAllThreads();
	}

	/**
	 * Send the given signal to all threads, except to self!
	 */
	void signalAllThreads(int sig);

	void quitAllThreads();

	void stopAllThreads();
};

typedef void *(*ThreadRunFunc)();

class CSThread : public CSRefObject {
public:
	/* The name of the thread. */
	CSString		*threadName;
	CSThreadList	*myThreadList;				/* The thread list that this thread belongs to. */

	/* If this value is non-zero, this signal is pending and
	 * must be thrown.
	 *
	 * SIGTERM, SIGQUIT - Means the thread has been terminated.
	 * SIGINT - Means the thread has been interrupted.
	 *
	 * When a signal is throw it clears this value. This includes
	 * the case when system calls return error due to interrupt.
	 */
	int				signalPending;
	bool			ignoreSignals;

	/* Set to true once the thread is running (never reset!). */
	bool			isRunning;

	/* Set to true when the thread must quit (never reset!): */
	bool			myMustQuit;	
	
	CSException		myException;
#if defined(MYSQL_SERVER) ||  defined(DRIZZLED)

	/* Set to true when this tread was initialized through the internal PBMS api. */
	/* When this is the case than it must only be freed via the API as well. */
	bool			pbms_api_owner;

	/* Transaction references. */
#ifdef DRIZZLED
	CSSortedList	mySavePoints;
#endif
	uint32_t		myTID;			// Current transaction ID
	uint32_t		myTransRef;		// Reference to the current transaction cache index
	bool			myIsAutoCommit;	// Is the current transaction in auto commit mode.
	uint32_t		myCacheVersion;	// The last transaction cache version checked. Used during overflow.
	bool			myStartTxn;		// A flag to indicate the start of a new transaction.
	uint32_t		myStmtCount;	// Counts the number of statements in the current transaction.
	uint32_t		myStartStmt;	// The myStmtCount at the start of the last logical statement. (An update is 2 statements but only 1 logical statement.)
	void			*myInfo;
#endif
	
	/* The call stack */
	int				callTop;
	CSCallStack		callStack[CS_CALL_STACK_SIZE];

	/* The long jump stack: */
	int				jumpDepth;							/* The current jump depth */
	CSJumpBufRec	jumpEnv[CS_JUMP_STACK_SIZE];		/* The process environment to be restored on exception */

	/* The release stack */
 	CSReleasePtr	relTop;								/* The top of the resource stack (reference next free space). */
	CSReleaseRec	relStack[CS_RELEASE_STACK_SIZE];	/* Temporary data to be freed if an exception occurs. */

	CSThread(CSThreadList *list):
		CSRefObject(),
		threadName(NULL),
		myThreadList(list),
		signalPending(0),
		ignoreSignals(false),
		isRunning(false),
		myMustQuit(false),
#if defined(MYSQL_SERVER) ||  defined(DRIZZLED)
		pbms_api_owner(false),
		myTID(0),
		myTransRef(0),
		myIsAutoCommit(true),
		myCacheVersion(0),
		myStartTxn(true),
		myStmtCount(0),
		myStartStmt(0),
		myInfo(NULL),
#endif
		callTop(0),
		jumpDepth(0),
		relTop(relStack),
		iIsMain(false),
		isDetached(false),
		iRunFunc(NULL),
		iNextLink(NULL),
		iPrevLink(NULL)
	{
	}

	virtual ~CSThread() {
		if (threadName)
			threadName->release();
	}

    /**
     * Task to be performed by this thread.
	 *
     * @exception CSSystemException thrown if thread cannot be run.
	 */
	virtual void *run();

	/**
	 * Start execution of the thread.
	 *
     * @exception CSSystemException thrown if thread is invalid.
	 */
	void start(bool detached = false);

	/*
	 * Stop execution of the thread.
	 */
	virtual void stop();

	/**
	 * Wait for this thread to die.
	 *
     * @exception CSSystemException thrown if thread is invalid.
	 */
	void *join();

	/**
	 * Signal the thread. Throws CSSystemException 
     * if the thread is invalid.
	 */
	void signal(unsigned int);

	void setSignalPending(unsigned int);

	/**
	 * Check to see if we have been interrupted by a signal
	 * (i.e. there is a signal pending).
	 * This function throws CSSignalException if
	 * there is a signal pending.
	 */
	void interrupted() { if (signalPending) throwSignal(); }
	void throwSignal();

	/* Log the stack to the specified depth along with the message. */
	void logStack(int depth, const char *msg);

	/* Log the exception, and the current stack. */
	void logException();
	
	/* Log the exception, and the current stack. */
	void logMessage();
	
	/*
	 * Return true if this is the main thread.
	 */
	bool isMain();

	/*
	 * Throwing exceptions:
	 */
	void releaseObjects(CSReleasePtr top);
	void throwException();
	void caught();
	bool isMe(CSThread *me) { return (pthread_equal(me->iThread,iThread) != 0);}
	/* Make this object linkable: */
	virtual CSObject *getNextLink() { return iNextLink; }
	virtual CSObject *getPrevLink() { return iPrevLink; }
	virtual void setNextLink(CSObject *link) { iNextLink = link; }
	virtual void setPrevLink(CSObject *link) { iPrevLink = link; }

	friend class CSDaemon;

private:
	pthread_t		iThread;
	bool			iIsMain;
	bool			isDetached;
	ThreadRunFunc	iRunFunc;
	CSObject		*iNextLink;
	CSObject		*iPrevLink;

	void addToList();
	void removeFromList();

public:
	/* Each thread stores is thread object in this key: */
	static pthread_key_t sThreadKey;

   /**
     * Put the currently executing thread to sleep for a given amount of
     * time.
     *
     * @param timeout maximum amount of time (milliseconds) this method could block
     *
     * @exception TDInterruptedException thrown if the threads sleep is interrupted
     *            before <i>timeout</i> milliseconds expire.
     */
	static void sleep(unsigned long timeout);

	/* Do static initialization and de-initialization. */
	static bool isUp;
	static bool startUp();
	static void shutDown();

	/* Attach and detach an already running thread: */
	static bool attach(CSThread *thread);
	static void detach(CSThread *thread);

	/**
	 * Return the thread object of the current
	 * thread.
	 */
	static CSThread *getSelf();
	static bool setSelf(CSThread *self);

	static CSThread *newCSThread();
	static CSThread *newThread(CSString *name, ThreadRunFunc run_func, CSThreadList *list);

	/* called for a newly created thread. */
	static void *dispatch(void *arg);

};

class CSDaemon : public CSThread, public CSSync {
public:
	time_t			myWaitTime;					/* Wait time in milli-seconds */

	CSDaemon(time_t wait_time, CSThreadList *list);
	CSDaemon(CSThreadList *list);
	virtual ~CSDaemon() { }

	virtual void *run();

	/* Return false if startup failed, and the thread must quit. */
	virtual bool initializeWork() { return true; };

	/* Return true of the thread should sleep before doing more work. */
	virtual bool doWork();

	virtual void *completeWork() { return NULL; };

	/* Return false if the excpetion is not handled and the thread must quit.
	 * Set must_sleep to true of the thread should pause before doing work
	 * again.
	 */
	virtual bool handleException();

	virtual void stop();

	void wakeup();

	void suspend();

	bool isSuspend() { return (iSuspendCount != 0);} // Don't use iSuspended, we are interested in if suspend() was called.

	void resume();

	virtual void returnToPool() {
		resume();
		release();
	}

	void suspended();

	void suspendedWait();

	void suspendedWait(time_t milli_sec);

private:
	void		try_Run(CSThread *self, const bool must_sleep);
	bool		iSuspended;
	uint32_t	iSuspendCount;
};

#endif
