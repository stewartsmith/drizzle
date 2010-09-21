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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Functions to handle initializating and allocationg of all mysys & debug
  thread variables.
*/

#include "config.h"

#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/my_pthread.h"
#include "drizzled/internal/thread_var.h"
#include "drizzled/internal/m_string.h"

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

namespace drizzled
{
namespace internal
{

boost::thread_specific_ptr<st_my_thread_var> THR_KEY_mysys;
boost::mutex THR_LOCK_threads;
pthread_cond_t  THR_COND_threads;
uint32_t THR_thread_count= 0;
static uint32_t my_thread_end_wait_time= 5;

/*
  initialize thread environment

  SYNOPSIS
    my_thread_global_init()

  RETURN
    0  ok
    1  error (Couldn't create THR_KEY_mysys)
*/

bool my_thread_global_init(void)
{
  pthread_cond_init(&THR_COND_threads, NULL);
  if (my_thread_init())
  {
    my_thread_global_end();			/* Clean up */
    return 1;
  }
  return 0;
}


void my_thread_global_end(void)
{
  struct timespec abstime;
  bool all_threads_killed= 1;

  set_timespec(abstime, my_thread_end_wait_time);
  {
    boost::mutex::scoped_lock scopedLock(THR_LOCK_threads);
    while (THR_thread_count > 0)
    {
      int error= pthread_cond_timedwait(&THR_COND_threads, THR_LOCK_threads.native_handle(),
                                        &abstime);
      if (error == ETIMEDOUT || error == ETIME)
      {
        /*
          We shouldn't give an error here, because if we don't have
          pthread_kill(), programs like mysqld can't ensure that all threads
          are killed when we enter here.
        */
        if (THR_thread_count)
          fprintf(stderr,
                  "Error in my_thread_global_end(): %d threads didn't exit\n",
                  THR_thread_count);
        all_threads_killed= 0;
        break;
      }
    }
  }

  if (all_threads_killed)
  {
    pthread_cond_destroy(&THR_COND_threads);
  }
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

bool my_thread_init(void)
{
  bool error=0;
  st_my_thread_var *tmp= NULL;

  // We should mever see my_thread_init()  called twice
  if (THR_KEY_mysys.get())
    return 0;

  tmp= new st_my_thread_var;
  if (tmp == NULL)
  {
    return 1;
  }
  THR_KEY_mysys.reset(tmp);

  boost::mutex::scoped_lock scopedLock(THR_LOCK_threads);
  tmp->id= ++thread_id;
  ++THR_thread_count;

  return error;
}


/*
  Deallocate memory used by the thread for book-keeping

  SYNOPSIS
    my_thread_end()

  NOTE
    This may be called multiple times for a thread.
    This happens for example when one calls 'mysql_server_init()'
    mysql_server_end() and then ends with a mysql_end().
*/

void my_thread_end(void)
{
  st_my_thread_var *tmp= THR_KEY_mysys.get();

  if (tmp)
  {
    delete tmp;
    THR_KEY_mysys.release();

    /*
      Decrement counter for number of running threads. We are using this
      in my_thread_global_end() to wait until all threads have called
      my_thread_end and thus freed all memory they have allocated in
      my_thread_init()
    */
    boost::mutex::scoped_lock scopedLock(THR_LOCK_threads);
    assert(THR_thread_count != 0);
    if (--THR_thread_count == 0)
      pthread_cond_signal(&THR_COND_threads);
  }
}

struct st_my_thread_var *_my_thread_var(void)
{
  return THR_KEY_mysys.get();
}

} /* namespace internal */
} /* namespace drizzled */
