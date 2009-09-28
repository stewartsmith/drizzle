/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (c) 2009, Patrick "CaptTofu" Galbraith
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of Patrick Galbraith nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
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
using namespace drizzled;

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
    max_length= 1;
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

  pthread_cond_init(&cond, NULL);
  pthread_mutex_lock(&LOCK_sleep);

  /* don't run if not killed */
  while (! session->killed)
  {
    error= pthread_cond_timedwait(&cond, &LOCK_sleep, &abstime);
    if (error == ETIMEDOUT || error == ETIME)
    {
      break;
    }
    error= 0;
  }
  pthread_mutex_unlock(&LOCK_sleep);

  /* relenquish pthread cond */
  pthread_cond_destroy(&cond);

  null_value= false;
  return (int64_t) 0;
}

plugin::Create_function<Item_func_sleep> *sleep_udf= NULL;

static int sleep_plugin_init(drizzled::plugin::Registry &registry)
{
  sleep_udf= new plugin::Create_function<Item_func_sleep>("sleep");
  registry.function.add(sleep_udf);
  pthread_mutex_init(&LOCK_sleep, MY_MUTEX_INIT_FAST);
  return 0;
}

static int sleep_plugin_deinit(drizzled::plugin::Registry &registry)
{
  registry.function.remove(sleep_udf);
  delete sleep_udf;
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
