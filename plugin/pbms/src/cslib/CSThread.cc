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

#include "CSConfig.h"

#ifdef OS_WINDOWS
#include <signal.h>
//#include "uniwin.h"
#define SIGUSR1 30
#define SIGUSR2 31

#else
#include <signal.h>
#include <sys/signal.h>
#include <unistd.h>
#endif
#include <errno.h>

#include "CSGlobal.h"
#include "CSLog.h"
#include "CSException.h"
#include "CSThread.h"
#include "CSStrUtil.h"
#include "CSMemory.h"

#define PBMS_THREAD_SIG SIGUSR1
/*
 * ---------------------------------------------------------------
 * SIGNAL HANDLERS
 */

extern "C" {


static void td_catch_signal(int sig)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		if (self->isMain()) {
			/* The main thread will pass on a signal to all threads: */
			if (self->myThreadList)
				self->myThreadList->signalAllThreads(sig);
			self->setSignalPending(sig);
		}
	}
	
}

static  void td_throw_signal(int sig)
{
	CSThread *self;

	if ((self = CSThread::getSelf())) {
		if (self->isMain()) {
			/* The main thread will pass on a signal to all threads: */
			if (self->myThreadList)
				self->myThreadList->signalAllThreads(sig);
		}
		self->setSignalPending(sig);
		self->interrupted();
	}
}

static bool td_setup_signals(CSThread *thread)
{
#ifdef OS_WINDOWS
	return true;
#else
	struct sigaction action;

    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    action.sa_handler = td_catch_signal;

	if (sigaction(PBMS_THREAD_SIG, &action, NULL) == -1)
		goto error_occurred;

    action.sa_handler = td_throw_signal;

	return true;

	error_occurred:

	if (thread) {
		thread->myException.initOSError(CS_CONTEXT, errno);
		thread->myException.setStackTrace(thread);
	}
	else
		CSException::throwOSError(CS_CONTEXT, errno);
	return false;
#endif
}

}

/*
 * ---------------------------------------------------------------
 * THREAD LISTS
 */

void CSThreadList::signalAllThreads(int sig)
{
	CSThread *ptr;

	enter_();
	lock_(this);
	ptr = (CSThread *) getBack();
	while (ptr) {
		if (ptr != self)
			ptr->signal(sig);
		ptr = (CSThread *) ptr->getNextLink();
	}
	unlock_(this);

	exit_();
}

void CSThreadList::quitAllThreads()
{
	CSThread *ptr;

	enter_();
	lock_(this);
	
	ptr = (CSThread *) getBack();
	while (ptr) {
		if (ptr != self)
			ptr->myMustQuit = true;
		ptr = (CSThread *) ptr->getNextLink();
	}
	
	unlock_(this);
	exit_();
}

void CSThreadList::stopAllThreads()
{
	CSThread *thread;

	enter_();
	for (;;) {
		/* Get a thread that is not self! */
		lock_(this);
		if ((thread = (CSThread *) getBack())) {
			while (thread) {
				if (thread != self)
					break;
				thread = (CSThread *) thread->getNextLink();
			}
		}
		if (thread)
			thread->retain();
		unlock_(this);
		
		if (!thread)
			break;
			
		push_(thread);
		thread->stop();
		release_(thread);
	}
	exit_();
}

/*
 * ---------------------------------------------------------------
 * CSTHREAD
 */

void CSThread::addToList()
{
	if (myThreadList) {
		enter_();
		ASSERT(self == this);
		lock_(myThreadList);
		myThreadList->addFront(self);
		isRunning = true;
		unlock_(myThreadList);
		exit_();
	}
	else
		isRunning = true;
}
	
void CSThread::removeFromList()
{
	if (myThreadList && isRunning) {
		CSThread *myself = this; // pop_() wants to take a reference to its parameter.
		enter_();
		/* Retain the thread in order to ensure
		 * that after it is removed from the list,
		 * that it is not freed! This would make the
		 * unlock_() call invalid, because it requires
		 * on the thread.
		 */
		push_(myself);
		lock_(myThreadList);
		myThreadList->remove(RETAIN(myself));
		unlock_(myThreadList);
		pop_(myself);
		outer_();
	}
	this->release();
}

void *CSThread::dispatch(void *arg)
{
	CSThread		*self;
	void			*return_data = NULL;
	int				err;

	/* Get a reference to myself: */
	self = reinterpret_cast<CSThread*>(arg);
	ASSERT(self);

	/* Store my thread in the thread key: */
	if ((err = pthread_setspecific(CSThread::sThreadKey, self))) {
		CSException::logOSError(self, CS_CONTEXT, err);
		return NULL;
	}

	/*
	 * Make sure the thread is not freed while we
	 * are running:
	 */
	self->retain();

	try_(a) {
		td_setup_signals(NULL);

		/* Add the thread to the list: */
		self->addToList();

		// Run the task from the correct context
		return_data = self->run();
	}
	catch_(a) {
		self->logException();
	}
	cont_(a);

	/*
	 * Removing from the thread list will also release the thread.
	 */
	self->removeFromList();

	// Exit the thread
	return return_data;
}


extern "C"
{

static void *dispatch_wrapper(void *arg)
{
	return CSThread::dispatch(arg);
}

}

void *CSThread::run()
{
	if (iRunFunc)
		return iRunFunc();
	return NULL;
}

void CSThread::start(bool detached)
{
	int err;

	err = pthread_create(&iThread, NULL, dispatch_wrapper, (void *) this);
	if (err)
		CSException::throwOSError(CS_CONTEXT, err);
	while (!isRunning) {
		/* Check if the thread is still alive,
		 * so we don't hang forever.
		 */
		if (pthread_kill(iThread, 0))
			break;
		usleep(10);
	}
	
	isDetached = detached;
	if (detached)
		pthread_detach(iThread);
}

void CSThread::stop()
{
	signal(SIGTERM);
	join();
}

void *CSThread::join()
{
	void	*return_data = NULL;
	int		err;

	enter_();
	if (isDetached) {
		while (isRunning && !pthread_kill(iThread, 0)) 
			usleep(100);
	} else if ((err = pthread_join(iThread, &return_data))) {
		CSException::throwOSError(CS_CONTEXT, err);
	}

	return_(return_data);
}

void CSThread::setSignalPending(unsigned int sig)
{
	if (sig == SIGTERM)
		/* The terminate signal takes priority: */
		signalPending = SIGTERM;
	else if (!signalPending)
		/* Otherwise, first signal wins... */
		signalPending = sig;
}

void CSThread::signal(unsigned int sig)
{
#ifndef OS_WINDOWS // Currently you cannot signal threads on windows.
	int err;

	setSignalPending(sig);
	if ((err = pthread_kill(iThread, PBMS_THREAD_SIG)))
	{
		/* Ignore the error if the process does not exist! */
		if (err != ESRCH) /* No such process */
			CSException::throwOSError(CS_CONTEXT, err);
	}
#endif
}

void CSThread::throwSignal()
{
	int sig;

	if ((sig = signalPending) && !ignoreSignals) {
		signalPending = 0;
		CSException::throwSignal(CS_CONTEXT, sig);
	}
}

bool CSThread::isMain()
{
	return iIsMain;
}

/*
 * -----------------------------------------------------------------------
 * THROWING EXCEPTIONS
 */

/* 
 * When an exception is .
 */

void CSThread::releaseObjects(CSReleasePtr top)
{
	CSObject *obj;

	while (relTop > top) {
		/* Remove and release or unlock the object on the top of the stack: */
		relTop--;
		switch(relTop->r_type) {
			case CS_RELEASE_OBJECT:
				if ((obj = relTop->x.r_object))
					obj->release();
				break;
			case CS_RELEASE_MUTEX:
				if (relTop->x.r_mutex)
					relTop->x.r_mutex->unlock();
				break;
			case CS_RELEASE_POOLED:
				if (relTop->x.r_pooled)
					relTop->x.r_pooled->returnToPool();
				break;
			case CS_RELEASE_MEM:
				if (relTop->x.r_mem)
					cs_free(relTop->x.r_mem);
				break;
			case CS_RELEASE_OBJECT_PTR:
				if ((relTop->x.r_objectPtr) && (obj = *(relTop->x.r_objectPtr)))
					obj->release();
				break;
		}
	}
}

/* Throw an already registered error: */
void CSThread::throwException()
{
	/* Record the stack trace: */
	if (this->jumpDepth > 0 && this->jumpDepth <= CS_JUMP_STACK_SIZE) {
		/*
		 * As recommended by Barry:
		 * release the objects before we jump!
		 * This has the advantage that the stack context is still
		 * valid when the resources are released.
		 */
		releaseObjects(this->jumpEnv[this->jumpDepth-1].jb_res_top);

		/* Then do the longjmp: */
		longjmp(this->jumpEnv[this->jumpDepth-1].jb_buffer, 1);
	}
}

void CSThread::logStack(int depth, const char *msg)
{
	char buffer[CS_EXC_CONTEXT_SIZE +1];
	CSL.lock();
	CSL.log(this, CSLog::Trace, msg);
	
	for (int i= callTop-1; i>=0 && depth; i--, depth--) {
		cs_format_context(CS_EXC_CONTEXT_SIZE, buffer,
			callStack[i].cs_func, callStack[i].cs_file, callStack[i].cs_line);
		strcat(buffer, "\n");
		CSL.log(this, CSLog::Trace, buffer);
	}
	CSL.unlock();
}

void CSThread::logException()
{
	myException.log(this);
}

/*
 * This function is called when an exception is caught.
 * It restores the function call top and frees
 * any resource allocated by lower levels.
 */
void CSThread::caught()
{
	/* Restore the call top: */
	this->callTop = this->jumpEnv[this->jumpDepth].jb_call_top;

	/* 
	 * Release all all objects that were pushed after
	 * this jump position was set:
	 */
	releaseObjects(this->jumpEnv[this->jumpDepth].jb_res_top);
}

/*
 * ---------------------------------------------------------------
 * STATIC METHODS
 */

pthread_key_t	CSThread::sThreadKey;
bool			CSThread::isUp = false;

bool CSThread::startUp()
{
	int err;

	isUp = false;
	if ((err = pthread_key_create(&sThreadKey, NULL))) {
		CSException::logOSError(CS_CONTEXT, errno);
	} else
		isUp = true;
		
	return isUp;
}

void CSThread::shutDown()
{
	isUp = false;
}

bool CSThread::attach(CSThread *thread)
{
	ASSERT(!getSelf());
	
	if (!thread) {
		CSException::logOSError(CS_CONTEXT, ENOMEM);
		return false;
	}

	if (!setSelf(thread))
		return false;

	/* Now we are ready to receive signals: */
	if (!td_setup_signals(thread))
		return false;

	thread->addToList();
	thread->retain();
	return true;
}

void CSThread::detach(CSThread *thread)
{
	ASSERT(!getSelf() || getSelf() == thread);
	thread->removeFromList();
	thread->release();
	pthread_setspecific(sThreadKey, NULL);
}

CSThread* CSThread::getSelf()
{
	CSThread* self = NULL;
	
	if ((!isUp) || !(self = (CSThread*) pthread_getspecific(sThreadKey)))
		return (CSThread*) NULL;
		
#ifdef DEBUG
	/* PMC - Problem is, if this is called when releasing a
	 * thread, then we have the reference count equal to
	 * zero.
	if (self && !self->iRefCount) {
		pthread_setspecific(sThreadKey, NULL);
		CSException::throwAssertion(CS_CONTEXT, "Freed self pointer referenced.");
	}
	*/
#endif

	return self;
}

bool CSThread::setSelf(CSThread *self)
{
	int err;

	if (self) {
		self->iThread = pthread_self();

		/* Store my thread in the thread key: */
		if ((err = pthread_setspecific(sThreadKey, self))) {
			self->myException.initOSError(CS_CONTEXT, err);
			self->myException.setStackTrace(self);
			return false;
		}
	}
	else
		pthread_setspecific(sThreadKey, NULL);
	return true;
}

/* timeout is in milliseconds */
void CSThread::sleep(unsigned long timeout)
{
	enter_();
	usleep(timeout * 1000);
	self->interrupted();
	exit_();
}

#ifdef DEBUG
int cs_assert(const char *func, const char *file, int line, const char *message)
{
	CSException::throwAssertion(func, file, line, message);
	return 0;
}

int cs_hope(const char *func, const char *file, int line, const char *message)
{
	CSException e;
		
	e.initAssertion(func, file, line, message);
	e.log(NULL);
	return 0;
}
#endif

CSThread *CSThread::newThread(CSString *name, ThreadRunFunc run_func, CSThreadList *list)
{
	CSThread *thd;

	enter_();
	if (!(thd = new CSThread(list))) {
		name->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	thd->threadName = name;
	thd->iRunFunc = run_func;
	return_(thd);
}

CSThread *CSThread::newCSThread()
{
	CSThread *thd = NULL;

	if (!(thd = new CSThread(NULL))) {
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	
	return thd;
}

/*
 * ---------------------------------------------------------------
 * DAEMON THREADS
 */

CSDaemon::CSDaemon(time_t wait_time, CSThreadList *list):
CSThread(list),
CSSync(),
myWaitTime(wait_time),
iSuspended(false),
iSuspendCount(0)
{
}

CSDaemon::CSDaemon(CSThreadList *list):
CSThread(list),
CSSync(),
myWaitTime(0),
iSuspended(false),
iSuspendCount(0)
{
}

void CSDaemon::try_Run(CSThread *self, const bool c_must_sleep)
{
	try_(a) {
		 bool must_sleep = c_must_sleep; // This done to avoid longjmp() clobber.
		while (!myMustQuit) {
			if (must_sleep) {
				lock_(this);
				if (myWaitTime)
					suspendedWait(myWaitTime);
				else
					suspendedWait();
				unlock_(this);
				if (myMustQuit)
					break;
			}
			must_sleep = doWork();
		}
	}
	catch_(a) {
		if (!handleException())
			myMustQuit = true;
	}
	cont_(a);
}

void *CSDaemon::run()
{
	bool must_sleep = false;

	enter_();

	myMustQuit = !initializeWork();

	while  (!myMustQuit) {
		try_Run(self, must_sleep);
		must_sleep = true;
	}

	/* Prevent signals from going off in completeWork! */
	ignoreSignals = true;

	return_(completeWork());
}

bool CSDaemon::doWork()
{
	if (iRunFunc)
		(void) iRunFunc();
	return true;
}

bool CSDaemon::handleException()
{
	if (!myMustQuit)
		logException();
	return true;
}

void CSDaemon::wakeup()
{
	CSSync::wakeup();
}

void CSDaemon::stop()
{
	myMustQuit = true;
	wakeup();
	signal(SIGTERM);
	join();
}

void CSDaemon::suspend()
{
	enter_();
	lock_(this);
	iSuspendCount++;
	while (!iSuspended && !myMustQuit)
		wait(500);
	if (!iSuspended)
		iSuspendCount--;
	unlock_(this);
	exit_();
}

void CSDaemon::resume()
{
	enter_();
	lock_(this);
	if (iSuspendCount > 0)
		iSuspendCount--;
	wakeup();
	unlock_(this);
	exit_();
}

void CSDaemon::suspended()
{
	if (!iSuspendCount || myMustQuit) {
		iSuspended = false;
		return;
	}
	enter_();
	lock_(this);
	while (iSuspendCount && !myMustQuit) {
		iSuspended = true;
		wait(500);
	}
	iSuspended = false;
	unlock_(this);
	exit_();
}

void CSDaemon::suspendedWait()
{
	iSuspended = true;
	wait();
	if (iSuspendCount)
		suspended();
}

void CSDaemon::suspendedWait(time_t milli_sec)
{
	iSuspended = true;
	wait(milli_sec);
	if (iSuspendCount)
		suspended();
	else
		iSuspended = false;
}

/*
 * ---------------------------------------------------------------
 * THREAD POOLS
 */


