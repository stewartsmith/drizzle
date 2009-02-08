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

static bool isEnabled= false;
static bool init_dummy(void) {return 0;}

/*
  Simple scheduler that use the main thread to handle the request

  NOTES
  This is only used for debugging, when starting mysqld with
  --thread-handling=no-threads or --one-thread

  When we enter this function, LOCK_thread_count is hold!
*/

void handle_connection_in_main_thread(Session *session)
{
  safe_mutex_assert_owner(&LOCK_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  handle_one_connection((void*) session);
}


/*
  End connection, in case when we are using 'no-threads'
*/

static bool end_thread(Session *, bool)
{
  pthread_mutex_unlock(&LOCK_thread_count);

  return true;                                     // Abort handle_one_connection
}

static int init(void *p)
{
  scheduling_st* func= (scheduling_st *)p;

  if (isEnabled == false)
  {
    func->is_used= false;
    return 0;
  }
  func->is_used= true;

  func->max_threads= 1;
  func->add_connection= handle_connection_in_main_thread;
  func->init_new_connection_thread= init_dummy;
  func->end_thread= end_thread;

  return 0;
}

static int deinit(void *)
{
  return 0;
}

static DRIZZLE_SYSVAR_BOOL(enabled, isEnabled,
                           PLUGIN_VAR_NOCMDARG,
                           N_("Enable One Thread per Connection Scheduler"),
                           NULL, /* check func */
                           NULL, /* update func */
                           false /* default */);

static struct st_mysql_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(enabled),
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
