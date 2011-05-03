/* Copyright (C) 2000 MySQL AB

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
  Functions to handle initializating and allocationg of all mysys & debug
  thread variables.
*/

#include <config.h>

#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/thread_var.h>
#include <drizzled/internal/m_string.h>

#include <cstdio>
#include <signal.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/tss.hpp>

namespace drizzled {
namespace internal {

boost::thread_specific_ptr<st_my_thread_var> THR_KEY_mysys;
boost::mutex THR_LOCK_threads;

/*
  initialize thread environment

  SYNOPSIS
    my_thread_global_init()

  RETURN
    0  ok
    1  error (Couldn't create THR_KEY_mysys)
*/

void my_thread_global_init()
{
  my_thread_init();
}


static uint64_t thread_id= 0;

/*
  Allocate thread specific memory for the thread, used by mysys

  SYNOPSIS
    my_thread_init()

  RETURN
    0  ok
    1  Fatal error; mysys/dbug functions can't be used
*/

void my_thread_init()
{
  // We should mever see my_thread_init()  called twice
  if (THR_KEY_mysys.get())
  {
    abort();
  }
  boost::mutex::scoped_lock scopedLock(THR_LOCK_threads);
  THR_KEY_mysys.reset(new st_my_thread_var(++thread_id));
}

st_my_thread_var* _my_thread_var()
{
  return THR_KEY_mysys.get();
}

} /* namespace internal */
} /* namespace drizzled */
