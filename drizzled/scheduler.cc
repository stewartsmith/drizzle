/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Implementation for the thread scheduler
*/

#include <drizzled/server_includes.h>
#include <libdrizzle/libdrizzle.h>
#include <drizzled/gettext.h>
#include <drizzled/sql_parse.h>
#include <drizzled/scheduler.h>
/* API for connecting, logging in to a drizzled server */
#include <drizzled/connect.h>

class Session;

/*
  'Dummy' functions to be used when we don't need any handling for a scheduler
  event
 */

static void post_kill_dummy(Session *) {}
static bool end_thread_dummy(Session *, bool)
{ return 0; }

/*
  Initialize default scheduler with dummy functions so that setup functions
  only need to declare those that are relvant for their usage
*/

scheduler_functions::scheduler_functions()
  :init_new_connection_thread(init_new_connection_handler_thread),
   add_connection(0),                           // Must be defined
   post_kill_notification(post_kill_dummy),
   end_thread(end_thread_dummy)
{}

