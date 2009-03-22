/* Copyright (C) 2006 MySQL AB

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

#include <drizzled/server_includes.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin_scheduling.h>
#include <drizzled/serialize/serialize.h>
#include <drizzled/connect.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <drizzled/connect.h>
#include <string>
using namespace std;

static bool init_new_connection_thread(void) {return 0;}

/*
  Simple scheduler that use the main thread to handle the request

  NOTES
  This is only used for debugging, when starting mysqld with
  --thread-handling=no-threads or --one-thread

  When we enter this function, LOCK_thread_count is held!
*/

bool add_connection(Session *session)
{
  handle_one_connection((void*) session);

  return false;
}


/*
  End connection, in case when we are using 'no-threads'
*/

static bool end_thread(Session *session, bool)
{
  unlink_session(session);   /* locks LOCK_thread_count and deletes session */

  return true;                                     // Abort handle_one_connection
}


static uint32_t count_of_threads(void)
{
  return 0;
}


static int init(void *p)
{
  scheduling_st* func= (scheduling_st *)p;

  func->max_threads= 1;
  func->add_connection= add_connection;
  func->init_new_connection_thread= init_new_connection_thread;
  func->end_thread= end_thread;
  func->count= count_of_threads;

  return 0;
}

static int deinit(void *)
{
  return 0;
}

static struct st_mysql_sys_var* system_variables[]= {
  NULL
};

drizzle_declare_plugin(single_thread)
{
  DRIZZLE_SCHEDULING_PLUGIN,
  "single_thread",
  "0.1",
  "Brian Aker",
  "Single Thread Scheduler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  system_variables,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
