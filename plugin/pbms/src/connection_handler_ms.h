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
 * Connection Handler.
 *
 */

#pragma once
#ifndef __CONNECTIONHANDLER_MS_H__
#define __CONNECTIONHANDLER_MS_H__

#include "cslib/CSDefs.h"
#include "cslib/CSThread.h"
#include "cslib/CSHTTPStream.h"

#include "engine_ms.h"

class MSConnectionHandler : public CSDaemon {
public:
	bool	amWaitingToListen;
	bool	shuttingDown;
	time_t	lastUse;

	MSConnectionHandler(CSThreadList *list);
	virtual ~MSConnectionHandler(){} // Do nothing here because 'self' will no longer be valid, use completeWork().

	void close();

	virtual bool initializeWork();

	virtual bool doWork();

	virtual void *completeWork();

	virtual bool handleException();

	static int getHTTPStatus(int err);

	void writeException(const char *qualifier);
	void writeException();

	bool openStream();
	void closeStream();

	void serviceConnection();
	void parseRequestURI();
	void freeRequestURI();

	void writeFile(CSString *file_path);
	void handleGet(bool info_only);
	void handlePut();

	bool replyPending;

	static MSConnectionHandler *newHandler(CSThreadList *list);

private:
	CSHTTPInputStream *iInputStream;
	CSHTTPOutputStream *iOutputStream;

	CSString			*iTableURI;
	
public:
	static u_long	gMaxKeepAlive;
};

#endif
