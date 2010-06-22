/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 * H&G2JCtL
 *
 * 2007-05-30
 *
 * CORE SYSTEM:
 * Unit tests.
 *
 */

#include "CSConfig.h"

#include <assert.h>
#include <signal.h>
#include <string.h>

#include "CSGlobal.h"
#include "CSTest.h"
#include "CSLog.h"
#include "CSSocket.h"
#include "CSStream.h"

void *tst_test_thread_run()
{
	enter_();
	CSL.log(self, CSLog::Protocol, "Hello from a thread!\n");
	return_(NULL);
}

void *tst_fail_thread_run()
{
	enter_();
	for (;;) {
		CSThread::sleep(1000);
		CSException::throwAssertion(CS_CONTEXT, "Fail now!");
	}
	return_(NULL);
}

void *tst_kill_thread_run()
{
	enter_();
	for (;;) {
		CSThread::sleep(1000000);
	}
	return_(NULL);
}

void *tst_telnet_thread_run()
{
	CSSocket *listener = NULL;
	CSSocket *telnet = NULL;
	CSOutputStream *out = NULL;
	CSInputStream *in = NULL;
	CSStringBuffer *sb = NULL;

	enter_();
	try_(a) {
		listener = CSSocket::newSocket();
		telnet = CSSocket::newSocket();

		CSL.log(self, CSLog::Protocol, "Waiting for telnet on port 8080...\n");
		listener->publish(NULL, 8080);
		telnet->open(listener);
		out = telnet->getOutputStream();
		out = CSBufferedOutputStream::newStream(out);
		in = telnet->getInputStream();
		in = CSBufferedInputStream::newStream(in);
		out->printLine("Hallo!, enter quit to exit!");
		for (;;) {
			sb = in->readLine();
			out->printLine(sb->getCString());
			if (strcmp(sb->getCString(), "quit") == 0)
				break;
			sb->release();
			sb = NULL;
		}
	}
	finally_(a) {
		if (listener)
			listener->release();
		if (telnet)
			telnet->release();
		if (in)
			in->release();
		if (out)
			out->release();
		if (sb)
			sb->release();
	}
	finally_end_block(a);
	
	return_(NULL);
}

/* Run all tests */
void CSTest::runAll()
{
	CSObject *obj;

	enter_();
	new_(obj, CSObject);
	obj->release();

	new_(obj, CSRefObject);
	obj->retain();
	obj->release();
	obj->release();

	CSString *s = CSString::newString("This is a log test\nThe 2nd line\n");
	CSL.log(self, CSLog::Protocol, s);
	s->release();

	CSThread *t;
	
	t = CSThread::newThread(CSString::newString("test_thread"), tst_test_thread_run, NULL);
	t->start();
	t->release();

	t = CSThread::newThread(CSString::newString("fail_thread"), tst_fail_thread_run, NULL);
	t->start();
	t->join();
	t->release();

	t = CSThread::newThread(CSString::newString("kill_thread"), tst_kill_thread_run, NULL);
	t->start();
	CSThread::sleep(1000);
	t->signal(SIGTERM);	// Kill the thread
	t->join();
	t->release();

	t = CSThread::newThread(CSString::newString("telnet_thread"), tst_telnet_thread_run, NULL);
	t->start();
	t->join();
	t->release();

	exit_();
}

