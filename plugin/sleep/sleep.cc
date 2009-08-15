/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Patrick Galbraith
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <unistd.h>
#include <time.h>

#include <drizzled/server_includes.h>
#include <drizzled/sql_udf.h>
#include <drizzled/session.h>
#include <drizzled/item/func.h>
#include <mysys/my_pthread.h>
#include <drizzled/function/str/strfunc.h>

#include <string>

using namespace std;

/* for thread-safe sleep() */
static pthread_mutex_t LOCK_sleep;

class Item_func_sleep : public Item_int_func
{
  String value;

public:
  int64_t val_int();
  Item_func_sleep() : Item_int_func()
  {
    unsigned_flag= true;
  }

  const char *func_name() const
  {
    return "sleep";
  }

  void fix_length_and_dec() {
    max_length=1;
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }

};

int64_t Item_func_sleep::val_int()
{
  int error= 0;

  /* int time in seconds, decimal allowed */
  double dtime;

  struct timespec abstime;

  pthread_cond_t cond;

  Session *session= current_session;

  if ((arg_count != 1) || ! (dtime= args[0]->val_real()))
  {
    null_value= true;
    return 0;
  }

  /*
    On 64-bit OSX pthread_cond_timedwait() waits forever
    if passed abstime time has already been exceeded by 
    the system time.
    When given a very short timeout (< 10 mcs) just return 
    immediately.
    We assume that the lines between this test and the call 
    to pthread_cond_timedwait() will be executed in less than 0.00001 sec.
  */
  if (dtime < 0.00001)
    return 0;

  /* need to obtain time value for passing to cond_timedwait */
  set_timespec_nsec(abstime, (uint64_t)(dtime * 1000000000ULL));

  /* init cond */
  pthread_cond_init(&cond, NULL);
  /* lock */
  pthread_mutex_lock(&LOCK_sleep);
  session->set_proc_info("you ahr veddy sleepy!");

  /* set current mutex to the mutex of interest */
  session->mysys_var->current_mutex= &LOCK_sleep;
  session->mysys_var->current_cond= &cond;

  /* don't run if not killed */
  while(!session->killed)
  {
    error= pthread_cond_timedwait(&cond, &LOCK_sleep, &abstime);
    if (error == ETIMEDOUT || error == ETIME)
      break;
    null_value= true;
    error= 0;
  }
  /* indicate that we're done */
  session->set_proc_info(0);
  /* unlock */
  pthread_mutex_unlock(&LOCK_sleep);

  /* now, we need to clean up, set back to zero */
  pthread_mutex_lock(&session->mysys_var->mutex);
  session->mysys_var->current_mutex= 0;
  session->mysys_var->current_cond= 0;
  pthread_mutex_unlock(&session->mysys_var->mutex);

  /* relenquish pthread cond */
  pthread_cond_destroy(&cond);

  null_value= false;
  return 0;
}

Create_function<Item_func_sleep>
  sleep_udf(string("sleep"));

static int sleep_plugin_init(PluginRegistry &registry)
{
  registry.add(&sleep_udf);
  /* is this the right place for this? */
  pthread_mutex_init(&LOCK_sleep, MY_MUTEX_INIT_FAST);
  return 0;
}

static int sleep_plugin_deinit(PluginRegistry &registry)
{
  registry.remove(&sleep_udf);
  return 0;
}


drizzle_declare_plugin(sleep)
{
  "sleep",
  "1.0",
  "Patrick Galbraith",
  "sleep()",
  PLUGIN_LICENSE_GPL,
  sleep_plugin_init, /* Plugin Init */
  sleep_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
