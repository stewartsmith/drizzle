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

/* Defines to make different thread packages compatible */

#ifndef DRIZZLED_INTERNAL_MY_PTHREAD_H
#define DRIZZLED_INTERNAL_MY_PTHREAD_H

#include <unistd.h>

#ifndef ETIME
#define ETIME ETIMEDOUT				/* For FreeBSD */
#endif

#include <pthread.h>
#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifdef HAVE_SCHED_H
#include <sched.h>
#endif
#ifdef HAVE_SYNCH_H
#include <synch.h>
#endif

namespace drizzled
{
namespace internal
{

#define pthread_key(T,V) pthread_key_t V
#define pthread_handler_t void *
typedef void *(* pthread_handler)(void *);

#ifndef my_pthread_attr_setprio
#ifdef HAVE_PTHREAD_ATTR_SETPRIO
#define my_pthread_attr_setprio(A,B) pthread_attr_setprio((A),(B))
#else
extern void my_pthread_attr_setprio(pthread_attr_t *attr, int priority);
#endif
#endif

#if !defined(HAVE_PTHREAD_YIELD_ONE_ARG) && !defined(HAVE_PTHREAD_YIELD_ZERO_ARG)
/* no pthread_yield() available */
#ifdef HAVE_SCHED_YIELD
#define pthread_yield() sched_yield()
#elif defined(HAVE_PTHREAD_YIELD_NP) /* can be Mac OS X */
#define pthread_yield() pthread_yield_np()
#endif
#endif

/*
  The defines set_timespec and set_timespec_nsec should be used
  for calculating an absolute time at which
  pthread_cond_timedwait should timeout
*/
#ifndef set_timespec
#define set_timespec(ABSTIME,SEC) \
{\
  struct timeval tv;\
  gettimeofday(&tv,0);\
  (ABSTIME).tv_sec=tv.tv_sec+(time_t) (SEC);\
  (ABSTIME).tv_nsec=tv.tv_usec*1000;\
}
#endif /* !set_timespec */
#ifndef set_timespec_nsec
#define set_timespec_nsec(ABSTIME,NSEC) \
{\
  uint64_t now= my_getsystime() + (NSEC/100); \
  (ABSTIME).tv_sec=  (time_t) (now / 10000000UL);                  \
  (ABSTIME).tv_nsec= (long) (now % 10000000UL * 100 + ((NSEC) % 100)); \
}
#endif /* !set_timespec_nsec */

	/* safe_mutex adds checking to mutex for easier debugging */

typedef struct st_safe_mutex_t
{
  pthread_mutex_t global,mutex;
  const char *file;
  uint32_t line,count;
  pthread_t thread;
} safe_mutex_t;

int safe_mutex_init(safe_mutex_t *mp, const pthread_mutexattr_t *attr,
                    const char *file, uint32_t line);
int safe_mutex_lock(safe_mutex_t *mp, bool try_lock, const char *file, uint32_t line);
int safe_mutex_unlock(safe_mutex_t *mp,const char *file, uint32_t line);
int safe_mutex_destroy(safe_mutex_t *mp,const char *file, uint32_t line);
int safe_cond_wait(pthread_cond_t *cond, safe_mutex_t *mp,const char *file,
		   uint32_t line);
int safe_cond_timedwait(pthread_cond_t *cond, safe_mutex_t *mp,
			struct timespec *abstime, const char *file, uint32_t line);
void safe_mutex_global_init(void);
void safe_mutex_end(void);

	/* Wrappers if safe mutex is actually used */
#define safe_mutex_assert_owner(mp)
#define safe_mutex_assert_not_owner(mp)

	/* READ-WRITE thread locking */

#if !defined(HAVE_PTHREAD_ATTR_SETSTACKSIZE) && ! defined(pthread_attr_setstacksize)
#define pthread_attr_setstacksize(A,B) pthread_dummy(0)
#endif

/* Define mutex types, see my_thr_init.c */
#ifdef THREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
extern pthread_mutexattr_t my_fast_mutexattr;
#define MY_MUTEX_INIT_FAST &my_fast_mutexattr
#else
#define MY_MUTEX_INIT_FAST   NULL
#endif

#ifndef ESRCH
/* Define it to something */
#define ESRCH 1
#endif

extern bool my_thread_global_init(void);
extern void my_thread_global_end(void);
extern bool my_thread_init(void);
extern void my_thread_end(void);
extern const char *my_thread_name(void);

/* All thread specific variables are in the following struct */

/*
  Drizzle can survive with 32K, but some glibc libraries require > 128K stack
  to resolve hostnames. Also recursive stored procedures needs stack.
*/
#define DEFAULT_THREAD_STACK	(256*INT32_C(1024))

struct st_my_thread_var
{
  pthread_cond_t suspend;
  pthread_mutex_t mutex;
  pthread_mutex_t * volatile current_mutex;
  pthread_cond_t * volatile current_cond;
  pthread_t pthread_self;
  uint64_t id;
  int volatile abort;
  bool init;
  struct st_my_thread_var *next,**prev;
  void *opt_info;
};

extern struct st_my_thread_var *_my_thread_var(void);
#define my_thread_var (::drizzled::internal::_my_thread_var())
/*
  Keep track of shutdown,signal, and main threads so that my_end() will not
  report errors with them
*/

/* Which kind of thread library is in use */

#define THD_LIB_OTHER 1
#define THD_LIB_NPTL  2
#define THD_LIB_LT    4

extern uint32_t thd_lib_detected;

/*
  thread_safe_xxx functions are for critical statistic or counters.
  The implementation is guaranteed to be thread safe, on all platforms.
  Note that the calling code should *not* assume the counter is protected
  by the mutex given, as the implementation of these helpers may change
  to use my_atomic operations instead.
*/

#ifndef thread_safe_increment
#define thread_safe_increment(V,L) \
        (pthread_mutex_lock((L)), (V)++, pthread_mutex_unlock((L)))
#define thread_safe_decrement(V,L) \
        (pthread_mutex_lock((L)), (V)--, pthread_mutex_unlock((L)))
#endif

#ifndef thread_safe_add
#define thread_safe_add(V,C,L) \
        (pthread_mutex_lock((L)), (V)+=(C), pthread_mutex_unlock((L)))
#define thread_safe_sub(V,C,L) \
        (pthread_mutex_lock((L)), (V)-=(C), pthread_mutex_unlock((L)))
#endif

/*
  statistics_xxx functions are for non critical statistic,
  maintained in global variables.
  - race conditions can occur, making the result slightly inaccurate.
  - the lock given is not honored.
*/
#define statistic_decrement(V,L) (V)--
#define statistic_increment(V,L) (V)++
#define statistic_add(V,C,L)     (V)+=(C)
#define statistic_sub(V,C,L)     (V)-=(C)

/*
  No locking needed, the counter is owned by the thread
*/
#define status_var_increment(V) (V)++
#define status_var_decrement(V) (V)--
#define status_var_add(V,C)     (V)+=(C)
#define status_var_sub(V,C)     (V)-=(C)

} /* namespace internal */
} /* namespace drizzled */

#endif /* DRIZZLED_INTERNAL_MY_PTHREAD_H */
