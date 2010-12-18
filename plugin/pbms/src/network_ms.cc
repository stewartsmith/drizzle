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
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 *
 * 2007-05-25
 *
 * H&G2JCtL
 *
 * Network interface.
 *
 */
#include "cslib/CSConfig.h"

#include "defs_ms.h"

#include "cslib/CSGlobal.h"
#include "cslib/CSLog.h"

#include "network_ms.h"
#include "connection_handler_ms.h"

MSSystemThread		*MSNetwork::gSystemThread;
time_t				MSNetwork::gCurrentTime;
time_t				MSNetwork::gLastService;
CSThreadList		*MSNetwork::gHandlerList;
CSSync				MSNetwork::gListenerLock;
CSSocket			*MSNetwork::gListenerSocket;
MSConnectionHandler	*MSNetwork::gListenerThread;
uint32_t				MSNetwork::gWaitingToListen;
int					MSNetwork::handlerCount;

/*
 * -------------------------------------------------------------------------
 * SYSTEM THREAD
 */

bool MSSystemThread::doWork()
{
	bool	killed = true;

	enter_();
	MSNetwork::gCurrentTime = time(NULL);
	if ((MSNetwork::gCurrentTime - MSNetwork::gLastService) >= (MS_IDLE_THREAD_TIMEOUT/2)) {
		MSNetwork::gLastService = MSNetwork::gCurrentTime;
		while (!myMustQuit && killed) {
			killed = MSNetwork::killListener();
			MSNetwork::gCurrentTime = time(NULL);
		}
	}
	return_(true);
}

/*
 * -------------------------------------------------------------------------
 * NETWORK FUNCTIONS
 */

void MSNetwork::startUp(int port)
{
	enter_();
	gCurrentTime = time(NULL);
	gLastService = gCurrentTime;
	gListenerSocket = NULL;
	handlerCount = 0;
	
	CSL.lock();
	CSL.log(self, CSLog::Protocol, "Media Stream Daemon ");
	if (port) {
	CSL.log(self, CSLog::Protocol, " listening on port ");
	CSL.log(self, CSLog::Protocol, port);
	} else
		CSL.log(self, CSLog::Protocol, " not published ");
	CSL.log(self, CSLog::Protocol, "\n");
	CSL.unlock();

	new_(gHandlerList, CSThreadList());
	if (port) {
	gListenerSocket = CSSocket::newSocket();
	gListenerSocket->publish(NULL, port);
	} else 
		gListenerSocket = NULL;

	new_(gSystemThread, MSSystemThread(1000 /* 1 sec */, NULL));
	gSystemThread->start();
	exit_();
}

void MSNetwork::shutDown()
{
	enter_();

	if (gSystemThread) {
		gSystemThread->stop();
		gSystemThread->release();
		gSystemThread = NULL;
	}

	/* This will set all threads to quiting: */
	if (gHandlerList)
		gHandlerList->quitAllThreads();

	/* Close the socket: */
	if (gListenerThread)
		gListenerThread->shuttingDown = true; // Block error messages as a result of the listener being killed
	
	lock_(&gListenerLock);
	if (gListenerSocket) {
		try_(a) {
			gListenerSocket->release();
		}
		catch_(a) {
			self->logException();
		}
		cont_(a);
	}
	gListenerSocket = NULL;
	unlock_(&gListenerLock);

	if (gHandlerList) {
		try_(b) {
			/* This will stop any threads remaining: */
			gHandlerList->release();
		}
		catch_(b) {
			self->logException();
		}
		cont_(b);
	}

	CSL.log(self, CSLog::Protocol, "PrimeBase Media Stream Daemon no longer published\n");
	exit_();
}

void MSNetwork::startConnectionHandler()
{
	char				buffer[120];
	MSConnectionHandler	*thread;

	enter_();
	handlerCount++;
	snprintf(buffer, 120, "NetworkHandler%d", handlerCount);
	lock_(gHandlerList);
	thread = MSConnectionHandler::newHandler(MSNetwork::gHandlerList);
	unlock_(gHandlerList);
	push_(thread);
	thread->threadName = CSString::newString(buffer);
	thread->start();
	release_(thread);
	exit_();
}

/*
 * Return NULL of a connection cannot be openned, and the
 * thread must quit.
 */
class OpenConnectioCleanUp : public CSRefObject {
	bool do_cleanup;

	public:
	
	OpenConnectioCleanUp(): CSRefObject(),
		do_cleanup(false){}
		
	~OpenConnectioCleanUp() 
	{
		if (do_cleanup) {
			MSNetwork::unlockListenerSocket();
		}
	}
	
	void setCleanUp()
	{
		do_cleanup = true;
	}
	
	void cancelCleanUp()
	{
		do_cleanup = false;
	}
	
};

/*
 * Return NULL if a connection cannot be openned, and the
 * thread must quit.
 */
CSSocket *MSNetwork::openConnection(MSConnectionHandler *handler)
{
	CSSocket *sock = NULL;
	OpenConnectioCleanUp *cleanup;

	enter_();
	
	if(!MSNetwork::gListenerSocket) {
		return_(NULL);
	}
	
	sock = CSSocket::newSocket();
	push_(sock);

	/* Wait for a connection: */
	if (!lockListenerSocket(handler)) {
		release_(sock);
		return_(NULL);
	}

	new_(cleanup, OpenConnectioCleanUp());
	push_(cleanup);
	
	cleanup->setCleanUp();
	sock->open(MSNetwork::gListenerSocket);
	cleanup->cancelCleanUp();

	handler->lastUse = gCurrentTime;

	unlockListenerSocket();

	release_(cleanup);
	pop_(sock);
	return_(sock);
}

void MSNetwork::startNetwork()
{
	enter_();
	startConnectionHandler();
	exit_();
}

bool MSNetwork::lockListenerSocket(MSConnectionHandler *handler)
{
	bool socket_locked = false;

	enter_();
	if (handler->myMustQuit)
		return false;
	lock_(&gListenerLock);
	if (gListenerSocket) {
		/* Wait for the listen socket to be freed: */
		if (gListenerThread) {
			gWaitingToListen++;
			handler->amWaitingToListen = true;
			while (gListenerThread) {
				if (handler->myMustQuit)
					break;
				try_(a) {
					gListenerLock.wait(2000);
				}
				catch_(a) {
					/* Catch any error */;
				}
				cont_(a);
			}
			gWaitingToListen--;
			handler->amWaitingToListen = false;
		}
		if (!handler->myMustQuit) {
			gListenerThread = handler;
			socket_locked = true;
		}
	}
	unlock_(&gListenerLock);
	return_(socket_locked);
}

void MSNetwork::unlockListenerSocket()
{
	enter_();
	lock_(&gListenerLock);
	gListenerThread = NULL;
	gListenerLock.wakeup();
	unlock_(&gListenerLock);
	exit_();
}

/* Kill a listener if possible!
 * Return true if a thread was killed.
 */
bool MSNetwork::killListener()
{
	MSConnectionHandler	*ptr = NULL;

	enter_();
	lock_(&gListenerLock);
	if (gListenerThread && gWaitingToListen > 0) {
		/* Kill one: */
		lock_(gHandlerList);
		ptr = (MSConnectionHandler *) gHandlerList->getBack();
		while (ptr) {
			if (ptr->amWaitingToListen) {
				if (gCurrentTime > ptr->lastUse && (gCurrentTime - ptr->lastUse) > MS_IDLE_THREAD_TIMEOUT) {
					ptr->myMustQuit = true;
					ptr->wakeup();
					break;
				}
			}
			ptr = (MSConnectionHandler *) ptr->getNextLink();
		}
		unlock_(gHandlerList);
	}
	unlock_(&gListenerLock);
	if (ptr) {
		ptr->join();
		return_(true);
	}
	return_(false);
}


