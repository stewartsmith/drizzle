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

namespace drizzled
{
namespace internal
{

uint32_t thd_lib_detected= 0;

pthread_key_t THR_KEY_mysys;
pthread_mutex_t THR_LOCK_lock;
pthread_mutex_t THR_LOCK_threads;
pthread_cond_t  THR_COND_threads;
uint32_t            THR_thread_count= 0;
static uint32_t my_thread_end_wait_time= 5;
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
pthread_mutexattr_t my_fast_mutexattr;
#endif

static uint32_t get_thread_lib(void);

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
  int pth_ret;
  thd_lib_detected= get_thread_lib();

  if ((pth_ret= pthread_key_create(&THR_KEY_mysys, NULL)) != 0)
  {
    fprintf(stderr,"Can't initialize threads: error %d\n", pth_ret);
    return 1;
  }

#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  /*
    Set mutex type to "fast" a.k.a "adaptive"

    In this case the thread may steal the mutex from some other thread
    that is waiting for the same mutex.  This will save us some
    context switches but may cause a thread to 'starve forever' while
    waiting for the mutex (not likely if the code within the mutex is
    short).
  */
  pthread_mutexattr_init(&my_fast_mutexattr);
  pthread_mutexattr_settype(&my_fast_mutexattr,
                            PTHREAD_MUTEX_ADAPTIVE_NP);
#endif

  pthread_mutex_init(&THR_LOCK_lock,MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&THR_LOCK_threads,MY_MUTEX_INIT_FAST);
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
  pthread_mutex_lock(&THR_LOCK_threads);
  while (THR_thread_count > 0)
  {
    int error= pthread_cond_timedwait(&THR_COND_threads, &THR_LOCK_threads,
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
  pthread_mutex_unlock(&THR_LOCK_threads);

  pthread_key_delete(THR_KEY_mysys);
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  pthread_mutexattr_destroy(&my_fast_mutexattr);
#endif
  pthread_mutex_destroy(&THR_LOCK_lock);
  if (all_threads_killed)
  {
    pthread_mutex_destroy(&THR_LOCK_threads);
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

#ifdef EXTRA_DEBUG_THREADS
  fprintf(stderr,"my_thread_init(): thread_id: 0x%lx\n",
          (uint32_t) pthread_self());
#endif

  if (pthread_getspecific(THR_KEY_mysys))
  {
#ifdef EXTRA_DEBUG_THREADS
    fprintf(stderr,"my_thread_init() called more than once in thread 0x%lx\n",
            (long) pthread_self());
#endif
    goto end;
  }
  tmp= static_cast<st_my_thread_var *>(calloc(1, sizeof(*tmp)));
  if (tmp == NULL)
  {
    error= 1;
    goto end;
  }
  pthread_setspecific(THR_KEY_mysys,tmp);
  tmp->pthread_self= pthread_self();
  pthread_mutex_init(&tmp->mutex,MY_MUTEX_INIT_FAST);
  pthread_cond_init(&tmp->suspend, NULL);
  tmp->init= 1;

  pthread_mutex_lock(&THR_LOCK_threads);
  tmp->id= ++thread_id;
  ++THR_thread_count;
  pthread_mutex_unlock(&THR_LOCK_threads);

end:
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
  st_my_thread_var *tmp=
    static_cast<st_my_thread_var *>(pthread_getspecific(THR_KEY_mysys));

#ifdef EXTRA_DEBUG_THREADS
  fprintf(stderr,"my_thread_end(): tmp: 0x%lx  pthread_self: 0x%lx  thread_id: %ld\n",
	  (long) tmp, (long) pthread_self(), tmp ? (long) tmp->id : 0L);
#endif
  if (tmp && tmp->init)
  {
#if !defined(__bsdi__) && !defined(__OpenBSD__)
 /* bsdi and openbsd 3.5 dumps core here */
    pthread_cond_destroy(&tmp->suspend);
#endif
    pthread_mutex_destroy(&tmp->mutex);
    free(tmp);

    /*
      Decrement counter for number of running threads. We are using this
      in my_thread_global_end() to wait until all threads have called
      my_thread_end and thus freed all memory they have allocated in
      my_thread_init()
    */
    pthread_mutex_lock(&THR_LOCK_threads);
    assert(THR_thread_count != 0);
    if (--THR_thread_count == 0)
      pthread_cond_signal(&THR_COND_threads);
    pthread_mutex_unlock(&THR_LOCK_threads);
  }
}

struct st_my_thread_var *_my_thread_var(void)
{
  struct st_my_thread_var *tmp= (struct st_my_thread_var*)pthread_getspecific(THR_KEY_mysys);
  return tmp;
}

static uint32_t get_thread_lib(void)
{
#ifdef _CS_GNU_LIBPTHREAD_VERSION
  char buff[64];

  confstr(_CS_GNU_LIBPTHREAD_VERSION, buff, sizeof(buff));

  if (!strncasecmp(buff, "NPTL", 4))
    return THD_LIB_NPTL;
  if (!strncasecmp(buff, "linuxthreads", 12))
    return THD_LIB_LT;
#endif
  return THD_LIB_OTHER;
}

} /* namespace internal */
} /* namespace drizzled */
