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

#include "mysys_priv.h"

#if !defined(DONT_USE_THR_ALARM)
#include <mysys/my_pthread.h>
#include <mysys/my_sys.h>
#include <mystrings/m_string.h>
#include <mysys/thr_alarm.h>
#include <drizzled/gettext.h>

#include <stdio.h>
#include <signal.h>
#include <errno.h>

#include <vector>
#include <algorithm>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>				/* AIX needs this for fd_set */
#endif

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


#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

uint32_t thr_client_alarm;
static int alarm_aborted=1;			/* No alarm thread */
bool thr_alarm_inited= 0;
volatile bool alarm_thread_running= 0;
time_t next_alarm_expire_time= ~ (time_t) 0;
static void process_alarm_part2(int sig);

static pthread_mutex_t LOCK_alarm;
static pthread_cond_t COND_alarm;
static sigset_t full_signal_set;
static std::vector<ALARM*> alarm_stack;
static uint32_t max_used_alarms=0;
pthread_t alarm_thread;

#ifdef USE_ALARM_THREAD
static void *alarm_handler(void *arg);
#define reschedule_alarms() pthread_cond_signal(&COND_alarm)
#else
#define reschedule_alarms() pthread_kill(alarm_thread,THR_SERVER_ALARM)
#endif

class alarm_if 
{
  public:
  inline bool operator()(ALARM *current_alarm)
  {
    current_alarm->alarmed= 1;           /* Info to thread */
    if (pthread_equal(current_alarm->thread,alarm_thread) ||
        pthread_kill(current_alarm->thread, thr_client_alarm))
    {
      return true;
    }
    return false;
  }
};

class alarm_if_time
{
  time_t alarm_time;
  time_t alarm_time_next;
  public:
  alarm_if_time(time_t now, time_t next) : alarm_time(now), alarm_time_next(next) { }
  inline bool operator()(ALARM *current_alarm)
  {
    if (current_alarm->expire_time <= alarm_time)
    {
      current_alarm->alarmed= 1;          /* Info to thread */
      if (pthread_equal(current_alarm->thread,alarm_thread) ||
          pthread_kill(current_alarm->thread,thr_client_alarm))
      {
        return true;
      }
      else
      {
        current_alarm->expire_time= alarm_time_next;
      }
    }
    return false;
  }
};
      
int compare_uint32_t(void *, unsigned char *a_ptr,unsigned char* b_ptr)
{
  uint32_t a=*((uint32_t*) a_ptr),b= *((uint32_t*) b_ptr);
  return (a < b) ? -1  : (a == b) ? 0 : 1;
}

void init_thr_alarm(uint32_t max_alarms)
{
  sigset_t s;
  alarm_aborted=0;
  next_alarm_expire_time= ~ (time_t) 0;
  alarm_stack.reserve(max_alarms+1);
  sigfillset(&full_signal_set);			/* Neaded to block signals */
  pthread_mutex_init(&LOCK_alarm,MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_alarm,NULL);
  if (thd_lib_detected == THD_LIB_LT)
    thr_client_alarm= SIGALRM;
  else
    thr_client_alarm= SIGUSR1;
#ifndef USE_ALARM_THREAD
  if (thd_lib_detected != THD_LIB_LT)
#endif
  {
    my_sigset(thr_client_alarm, thread_alarm);
  }
  sigemptyset(&s);
  sigaddset(&s, THR_SERVER_ALARM);
  alarm_thread=pthread_self();
#if defined(USE_ALARM_THREAD)
  {
    pthread_attr_t thr_attr;
    pthread_attr_init(&thr_attr);
    pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_PROCESS);
    pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&thr_attr,8196);

    my_pthread_attr_setprio(&thr_attr,100);	/* Very high priority */
    pthread_create(&alarm_thread,&thr_attr,alarm_handler,NULL);
    pthread_attr_destroy(&thr_attr);
  }
#elif defined(USE_ONE_SIGNAL_HAND)
  pthread_sigmask(SIG_BLOCK, &s, NULL);		/* used with sigwait() */
  if (thd_lib_detected == THD_LIB_LT)
  {
    my_sigset(thr_client_alarm, process_alarm);        /* Linuxthreads */
    pthread_sigmask(SIG_UNBLOCK, &s, NULL);
  }
#else
  my_sigset(THR_SERVER_ALARM, process_alarm);
  pthread_sigmask(SIG_UNBLOCK, &s, NULL);
#endif /* USE_ALARM_THREAD */
  return;
}


void resize_thr_alarm(uint32_t max_alarms)
{
  pthread_mutex_lock(&LOCK_alarm);
  /*
    It's ok not to shrink the queue as there may be more pending alarms than
    than max_alarms
  */
  if (alarm_stack.capacity() < max_alarms)
    alarm_stack.resize(max_alarms+1);
  pthread_mutex_unlock(&LOCK_alarm);
}


/*
  Request alarm after sec seconds.

  SYNOPSIS
    thr_alarm()
    alrm		Pointer to alarm detection
    alarm_data		Structure to store in alarm queue

  NOTES
    This function can't be called from the alarm-handling thread.

  RETURN VALUES
    0 ok
    1 If no more alarms are allowed (aborted by process)

    Stores in first argument a pointer to a non-zero int which is set to 0
    when the alarm has been given
*/

bool thr_alarm(thr_alarm_t *alrm, uint32_t sec, ALARM *alarm_data)
{
  time_t now;
#ifndef USE_ONE_SIGNAL_HAND
  sigset_t old_mask;
#endif
  bool reschedule;
  struct st_my_thread_var *current_my_thread_var= my_thread_var;

  now= time(NULL);
  if(now == (time_t)-1)
  {
    fprintf(stderr, _("%s: Warning: time() call failed\n"), my_progname);
    return 1;
  }

#ifndef USE_ONE_SIGNAL_HAND
  pthread_sigmask(SIG_BLOCK,&full_signal_set,&old_mask);
#endif
  pthread_mutex_lock(&LOCK_alarm);        /* Lock from threads & alarms */
  if (alarm_aborted > 0)
  {					/* No signal thread */
    *alrm= 0;					/* No alarm */
    pthread_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
    pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
    return(1);
  }
  if (alarm_aborted < 0)
    sec= 1;					/* Abort mode */

  if (alarm_stack.size() >= max_used_alarms)
  {
    if (alarm_stack.size() == alarm_stack.capacity())
    {
      fprintf(stderr,_("Warning: thr_alarm queue is full\n"));
      *alrm= 0;					/* No alarm */
      pthread_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
      pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
      return(1);
    }
    max_used_alarms= alarm_stack.size() + 1;
  }
  reschedule= (uint32_t) next_alarm_expire_time > (uint32_t) now + sec;
  if (!alarm_data)
  {
    if (!(alarm_data=(ALARM*) malloc(sizeof(ALARM))))
    {
      *alrm= 0;					/* No alarm */
      pthread_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
      pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
      return(1);
    }
    alarm_data->malloced=1;
  }
  else
    alarm_data->malloced=0;
  alarm_data->expire_time=now+sec;
  alarm_data->alarmed=0;
  alarm_data->thread=    current_my_thread_var->pthread_self;
  alarm_data->thread_id= current_my_thread_var->id;
  /* No need to coerce to uchar* since std::vector<> is templatized */
  alarm_stack.insert(alarm_stack.begin(),alarm_data);

  /* Reschedule alarm if the current one has more than sec left */
  if (reschedule)
  {
    if (pthread_equal(pthread_self(),alarm_thread))
    {
      alarm(sec);				/* purecov: inspected */
      next_alarm_expire_time= now + sec;
    }
    else
      reschedule_alarms();			/* Reschedule alarms */
  }
  pthread_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
  pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
  (*alrm)= &alarm_data->alarmed;
  return(0);
}


/*
  Remove alarm from list of alarms
*/

void thr_end_alarm(thr_alarm_t *alarmed)
{
  ALARM *alarm_data;
#ifndef USE_ONE_SIGNAL_HAND
  sigset_t old_mask;
#endif
  uint32_t found=0;

#ifndef USE_ONE_SIGNAL_HAND
  pthread_sigmask(SIG_BLOCK,&full_signal_set,&old_mask);
#endif
  pthread_mutex_lock(&LOCK_alarm);

  alarm_data= (ALARM*) ((unsigned char*) *alarmed - offsetof(ALARM,alarmed));
  std::vector<ALARM*>::iterator p= alarm_stack.begin();
  while (p != alarm_stack.end() && *p != alarm_data)
    p++;
  if (p != alarm_stack.end())
  {
    /* p now points to the current alarm pointer; remove it */
    if (alarm_data->malloced)
      free(*p);
    alarm_stack.erase(p);
    found= 1;
  }
  assert(!*alarmed || found == 1);
  if (!found)
  {
    if (*alarmed)
      fprintf(stderr,_("Warning: Didn't find alarm 0x%lx"
                       "in queue of %"PRIu64" alarms\n"),
	      (long)*alarmed, (uint64_t)alarm_stack.size());
  }
  pthread_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
  pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
  return;
}

/*
  Come here when some alarm in queue is due.
  Mark all alarms with are finnished in list.
  Shedule alarms to be sent again after 1-10 sec (many alarms at once)
  If alarm_aborted is set then all alarms are given and resent
  every second.
*/

void process_alarm(int sig)
{
  sigset_t old_mask;

  if (thd_lib_detected == THD_LIB_LT &&
      !pthread_equal(pthread_self(),alarm_thread))
  {
#ifndef HAVE_BSD_SIGNALS
    my_sigset(thr_client_alarm, process_alarm);	/* int. thread system calls */
#endif
    return;
  }

#ifndef USE_ALARM_THREAD
  pthread_sigmask(SIG_SETMASK,&full_signal_set,&old_mask);
  pthread_mutex_lock(&LOCK_alarm);
#endif
  process_alarm_part2(sig);
#ifndef USE_ALARM_THREAD
#if !defined(HAVE_BSD_SIGNALS) && !defined(USE_ONE_SIGNAL_HAND)
  my_sigset(THR_SERVER_ALARM,process_alarm);
#endif
  pthread_mutex_unlock(&LOCK_alarm);
  pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
  return;
}


static void process_alarm_part2(int)
{
  if (alarm_stack.size())
  {
    if (alarm_aborted)
    {
      alarm_stack.erase(remove_if(alarm_stack.begin(),alarm_stack.end(),
                        alarm_if()),alarm_stack.end());

#ifndef USE_ALARM_THREAD
      if (alarm_stack.size())
        alarm(1);				/* Signal soon again */
#endif
    }
    else
    {
      time_t now= time(NULL);
      time_t next= now+10-(now%10);

      alarm_stack.erase(remove_if(alarm_stack.begin(),alarm_stack.end(),
                        alarm_if_time(now, next)),alarm_stack.end());
#ifndef USE_ALARM_THREAD
      if (alarm_stack.size())
      {
        std::vector<ALARM*>::iterator p= alarm_stack.begin();
        ALARM *smallest_expire_time;
        while (p != alarm_stack.end())
        {
          ALARM *current_alarm= *p;
          if (!smallest_expire_time)
          {
            smallest_expire_time= current_alarm;
          }
          else
          {
            if (current_alarm->expire_time < smallest_expire_time->expire_time)
              smallest_expire_time= current_alarm;
          }
          p++;
        }
        if (smallest_expire_time)
        {
          alarm((uint32_t) (smallest_expire_time->expire_time-now));
          next_alarm_expire_time= smallest_expire_time->expire_time;
        }
      }
#endif
    }
  }
  else
  {
    /*
      Ensure that next time we call thr_alarm(), we will schedule a new alarm
    */
    next_alarm_expire_time= ~(time_t) 0;
  }
  return;
}


/*
  Schedule all alarms now and optionally free all structures

  SYNPOSIS
    end_thr_alarm()
      free_structures		Set to 1 if we should free memory used for
				the alarm queue.
				When we call this we should KNOW that there
				is no active alarms
  IMPLEMENTATION
    Set alarm_abort to -1 which will change the behavior of alarms as follows:
    - All old alarms will be rescheduled at once
    - All new alarms will be rescheduled to one second
*/

void end_thr_alarm(bool free_structures)
{
  if (alarm_aborted != 1)			/* If memory not freed */
  {
    pthread_mutex_lock(&LOCK_alarm);
    alarm_aborted= -1;				/* mark aborted */
    if (alarm_stack.size() || (alarm_thread_running && free_structures))
    {
      if (pthread_equal(pthread_self(),alarm_thread))
	alarm(1);				/* Shut down everything soon */
      else
	reschedule_alarms();
    }
    if (free_structures)
    {
      struct timespec abstime;

      assert(!alarm_stack.size());

      /* Wait until alarm thread dies */
      set_timespec(abstime, 10);		/* Wait up to 10 seconds */
      while (alarm_thread_running)
      {
	int error= pthread_cond_timedwait(&COND_alarm, &LOCK_alarm, &abstime);
	if (error == ETIME || error == ETIMEDOUT)
	  break;				/* Don't wait forever */
      }
      alarm_aborted= 1;
      pthread_mutex_unlock(&LOCK_alarm);
      if (!alarm_thread_running)              /* Safety */
      {
        pthread_mutex_destroy(&LOCK_alarm);
        pthread_cond_destroy(&COND_alarm);
      }
    }
    else
      pthread_mutex_unlock(&LOCK_alarm);
  }
  return;
}


/*
  Remove another thread from the alarm
*/

void thr_alarm_kill(my_thread_id thread_id)
{
  if (alarm_aborted)
    return;
  pthread_mutex_lock(&LOCK_alarm);
  std::vector<ALARM*>::iterator p= alarm_stack.begin();
  while (p != alarm_stack.end())
  {
    ALARM *current_alarm= *p;
    if (current_alarm->thread_id == thread_id)
      break;
    p++;
  }
  if (p != alarm_stack.end())
  {
    (*p)->expire_time= 0;
    reschedule_alarms();
  }
  pthread_mutex_unlock(&LOCK_alarm);
}


void thr_alarm_info(ALARM_INFO *info)
{
  pthread_mutex_lock(&LOCK_alarm);
  info->next_alarm_time= 0;
  info->max_used_alarms= max_used_alarms;
  if ((info->active_alarms=  alarm_stack.size()))
  {
    time_t now= time(NULL);
    long time_diff;
    ALARM *alarm_data= (ALARM*) alarm_stack.front();
    time_diff= (long) (alarm_data->expire_time - now);
    info->next_alarm_time= (uint32_t) (time_diff < 0 ? 0 : time_diff);
  }
  pthread_mutex_unlock(&LOCK_alarm);
}

/*
  This is here for thread to get interruptet from read/write/fcntl
  ARGSUSED
*/


void thread_alarm(int sig)
{
#ifndef HAVE_BSD_SIGNALS
  my_sigset(sig,thread_alarm);		/* int. thread system calls */
#endif
}


#ifdef HAVE_TIMESPEC_TS_SEC
#define tv_sec ts_sec
#define tv_nsec ts_nsec
#endif

/* 
   Set up a alarm thread which uses 'sleep' to sleep between alarms

  RETURNS
    NULL on time() failure
*/

#ifdef USE_ALARM_THREAD
static void *alarm_handler(void *)
{
  int error;
  struct timespec abstime;
  my_thread_init();
  alarm_thread_running= 1;
  pthread_mutex_lock(&LOCK_alarm);
  for (;;)
  {
    if (alarm_stack.size())
    {
      uint32_t sleep_time;

      time_t now= time(NULL);
      if (now == (time_t)-1)
      {
        pthread_mutex_unlock(&LOCK_alarm);
        return NULL;
      }

      if (alarm_aborted)
        sleep_time=now+1;
      else
        sleep_time= ((ALARM*) alarm_stack.front())->expire_time;
      if (sleep_time > now)
      {
        abstime.tv_sec=sleep_time;
        abstime.tv_nsec=0;
        next_alarm_expire_time= sleep_time;
        if ((error=pthread_cond_timedwait(&COND_alarm,&LOCK_alarm,&abstime)) &&
            error != ETIME && error != ETIMEDOUT)
        {
          assert(1);
        }
      }
    }
    else if (alarm_aborted == -1)
      break;
    else
    {
      next_alarm_expire_time= ~ (time_t) 0;
      error= pthread_cond_wait(&COND_alarm,&LOCK_alarm);

      assert(error == 0);
    }
    process_alarm(0);
  }
  memset(&alarm_thread, 0, sizeof(alarm_thread)); /* For easy debugging */
  alarm_thread_running= 0;
  pthread_cond_signal(&COND_alarm);
  pthread_mutex_unlock(&LOCK_alarm);
  pthread_exit(0);
  return 0;					/* Impossible */
}
#endif /* USE_ALARM_THREAD */

#endif /* THREAD */
