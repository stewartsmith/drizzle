/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Authors:
 *
 * Patrick Galbraith <pat@patg.net>
 *
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
 
#include <config.h>

#include <unistd.h>
#include <time.h>

#include <drizzled/session.h>
#include <drizzled/item/func.h>
#include <drizzled/internal/my_pthread.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/plugin/function.h>

#include <string>

using namespace std;
using namespace drizzled;


class Item_func_sleep : public Item_int_func
{
  /* for thread-safe sleep() */
  pthread_mutex_t LOCK_sleep;

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

  void fix_length_and_dec()
  {
    max_length= 1;
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }

};

int64_t Item_func_sleep::val_int()
{
  /* int time in seconds, decimal allowed */
  double dtime;

  Session &session(getSession());

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

  {
    boost::this_thread::restore_interruption dl(session.getThreadInterupt());

    try {
      boost::xtime xt; 
      xtime_get(&xt, boost::TIME_UTC); 
      xt.nsec += (uint64_t)(dtime * 1000000000ULL); 
      session.getThread()->sleep(xt);
    }
    catch(boost::thread_interrupted const& error)
    {
      my_error(drizzled::ER_QUERY_INTERRUPTED, MYF(0));
      null_value= true;

      return 0;
    }
  }


  null_value= false;

  return (int64_t) 0;
}

static int sleep_plugin_init(drizzled::module::Context &context)
{
  context.add(new plugin::Create_function<Item_func_sleep>("sleep"));

  return 0;
}

DRIZZLE_PLUGIN(sleep_plugin_init, NULL, NULL);
