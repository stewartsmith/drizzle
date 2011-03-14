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

#pragma once
#ifndef __NETWORK_MS_H__
#define __NETWORK_MS_H__

#include "cslib/CSDefs.h"
#include "cslib/CSSocket.h"
#include "cslib/CSThread.h"
#include "connection_handler_ms.h"

class MSSystemThread : public CSDaemon {
public:
	MSSystemThread(time_t wait_time, CSThreadList *list): CSDaemon(wait_time, list) { }

	virtual bool doWork();
};

class MSNetwork {
public:
	static void startUp(int port);
	static void shutDown();
	static CSSocket *openConnection(MSConnectionHandler *handler);
	static void startConnectionHandler();
	static void startNetwork();
	static bool lockListenerSocket(MSConnectionHandler *handle);
	static void unlockListenerSocket();
	static CSMutex *getListenerLock() { return &gListenerLock; }
	static bool killListener();

public:
	static MSSystemThread		*gSystemThread;
	static time_t				gCurrentTime;
	static time_t				gLastService;
	static CSThreadList			*gHandlerList;
	static CSSync				gListenerLock;
	static CSSocket				*gListenerSocket;
	static MSConnectionHandler	*gListenerThread;
	static uint32_t				gWaitingToListen;

private:
	static int					handlerCount;
};

#endif
