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

/* Defines to make different thread packages compatible */



#pragma once

#include <unistd.h>

#include <boost/date_time.hpp>

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

#include <drizzled/visibility.h>

namespace drizzled
{
namespace internal
{

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
  boost::posix_time::ptime mytime(boost::posix_time::microsec_clock::local_time());\
  boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1));\
  uint64_t t_mark= (mytime-epoch).total_microseconds();\
  uint64_t now= t_mark + (NSEC/100); \
  (ABSTIME).tv_sec=  (time_t) (now / 10000000UL);                  \
  (ABSTIME).tv_nsec= (long) (now % 10000000UL * 100 + ((NSEC) % 100)); \
}
#endif /* !set_timespec_nsec */

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

extern void my_thread_global_init();
DRIZZLED_API void my_thread_init();
extern const char *my_thread_name();

/* All thread specific variables are in the following struct */

/**
  A default thread stack size of zero means that we are going to use
  the OS defined thread stack size (this varies from OS to OS).
 */
#define DEFAULT_THREAD_STACK	0

} /* namespace internal */
} /* namespace drizzled */

