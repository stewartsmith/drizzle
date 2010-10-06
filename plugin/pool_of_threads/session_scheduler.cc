/* Copyright (C) 2009 Sun Microsystems

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  session_scheduler keeps the link between Session and events.
  It's embedded in the Session class.
*/

#include "config.h"
#include <drizzled/session.h>
#include <drizzled/gettext.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/plugin/client.h>
#include <event.h>
#include "session_scheduler.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/thread_var.h"

/* Prototype */
extern "C"
void libevent_io_callback(int Fd, short Operation, void *ctx);
bool libevent_should_close_connection(drizzled::Session* session);
void libevent_session_add(drizzled::Session* session);

session_scheduler::session_scheduler(drizzled::Session *parent_session)
  : logged_in(false), thread_attached(false)
{
  memset(&io_event, 0, sizeof(struct event));

  event_set(&io_event, parent_session->client->getFileDescriptor(), EV_READ,
            libevent_io_callback, (void*)parent_session);

  session= parent_session;
}

/*
  Attach/associate the connection with the OS thread, for command processing.
*/

bool session_scheduler::thread_attach()
{
  assert(!thread_attached);
  if (libevent_should_close_connection(session) ||
      session->initGlobals())
  {
    return true;
  }
  errno= 0;
  session->getThreadVar()->abort= 0;
  thread_attached= true;

  return false;
}


/*
  Detach/disassociate the connection with the OS thread.
*/

void session_scheduler::thread_detach()
{
  if (thread_attached)
  {
    session->resetThreadVar();
    thread_attached= false;
  }
}
