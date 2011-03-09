/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

/*
  Just a test application for threads.
  */

#include <config.h>

#include "azio.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>                             /* Pull in memset() */
#ifndef __WIN__
#include <sys/wait.h>
#endif
#include <memory>

#ifdef __WIN__
#define srandom  srand
#define random   rand
#define snprintf _snprintf
#endif

#include <boost/scoped_ptr.hpp>

#include "azio.h"

#define DEFAULT_CONCURRENCY	10
#define DEFAULT_INITIAL_LOAD 10000
#define DEFAULT_EXECUTE_SECONDS 120
#define TEST_FILENAME "concurrency_test.az"

#define HUGE_STRING_LENGTH 8192

/* Global Thread counter */
unsigned int thread_counter;
pthread_mutex_t counter_mutex;
pthread_cond_t count_threshhold;
unsigned int master_wakeup;
pthread_mutex_t sleeper_mutex;
pthread_cond_t sleep_threshhold;
static bool timer_alarm= false;
pthread_mutex_t timer_alarm_mutex;
pthread_cond_t timer_alarm_threshold;

pthread_mutex_t row_lock;

/* Prototypes */
extern "C" {
  void *run_concurrent_task(void *p);
  void *timer_thread(void *p);
}
void scheduler(az_method use_aio);
void create_data_file(azio_stream *write_handler, uint64_t rows);
unsigned int write_row(azio_stream *s);

typedef struct thread_context_st thread_context_st;
struct thread_context_st {
  unsigned int how_often_to_write;
  uint64_t counter;
  az_method use_aio;
  azio_stream *writer;
};

/* Use this for string generation */
static const char ALPHANUMERICS[]=
  "0123456789ABCDEFGHIJKLMNOPQRSTWXYZabcdefghijklmnopqrstuvwxyz";

#define ALPHANUMERICS_SIZE (sizeof(ALPHANUMERICS)-1)

static void get_random_string(char *buffer, size_t size)
{
  char *buffer_ptr= buffer;

  while (--size)
    *buffer_ptr++= ALPHANUMERICS[random() % ALPHANUMERICS_SIZE];
  *buffer_ptr++= ALPHANUMERICS[random() % ALPHANUMERICS_SIZE];
}

int main(int argc, char *argv[])
{

  unsigned int method;
  drizzled::internal::my_init();

  MY_INIT(argv[0]);

  if (argc > 1)
    exit(1);

  srandom(time(NULL));

  pthread_mutex_init(&counter_mutex, NULL);
  pthread_cond_init(&count_threshhold, NULL);
  pthread_mutex_init(&sleeper_mutex, NULL);
  pthread_cond_init(&sleep_threshhold, NULL);
  pthread_mutex_init(&timer_alarm_mutex, NULL);
  pthread_cond_init(&timer_alarm_threshold, NULL);
  pthread_mutex_init(&row_lock, NULL);

  for (method= AZ_METHOD_BLOCK; method < AZ_METHOD_MAX; method++)
    scheduler((az_method)method);

  (void)pthread_mutex_destroy(&counter_mutex);
  (void)pthread_cond_destroy(&count_threshhold);
  (void)pthread_mutex_destroy(&sleeper_mutex);
  (void)pthread_cond_destroy(&sleep_threshhold);
  pthread_mutex_destroy(&timer_alarm_mutex);
  pthread_cond_destroy(&timer_alarm_threshold);
  pthread_mutex_destroy(&row_lock);

  return 0;
}

void scheduler(az_method use_aio)
{
  unsigned int x;
  uint64_t total;
  boost::scoped_ptr<azio_stream> writer_handle_ap(new azio_stream);
  azio_stream &writer_handle= *writer_handle_ap.get();
  thread_context_st *context;
  pthread_t mainthread;            /* Thread descriptor */
  pthread_attr_t attr;          /* Thread attributes */

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,
                              PTHREAD_CREATE_DETACHED);

  pthread_mutex_lock(&counter_mutex);
  thread_counter= 0;

  create_data_file(&writer_handle, DEFAULT_INITIAL_LOAD);

  pthread_mutex_lock(&sleeper_mutex);
  master_wakeup= 1;
  pthread_mutex_unlock(&sleeper_mutex);

  context= (thread_context_st *)malloc(sizeof(thread_context_st) * DEFAULT_CONCURRENCY);
  memset(context, 0, sizeof(thread_context_st) * DEFAULT_CONCURRENCY);

  if (!context)
  {
    fprintf(stderr, "Could not allocate memory for context\n");
    exit(1);
  }

  for (x= 0; x < DEFAULT_CONCURRENCY; x++)
  {

    context[x].how_often_to_write= random()%1000;
    context[x].writer= &writer_handle;
    context[x].counter= 0;
    context[x].use_aio= use_aio;

    /* now you create the thread */
    if (pthread_create(&mainthread, &attr, run_concurrent_task,
                       (void *)context) != 0)
    {
      fprintf(stderr,"Could not create thread\n");
      exit(1);
    }
    thread_counter++;
  }

  if (DEFAULT_EXECUTE_SECONDS)
  {
    time_t opt_timer_length= DEFAULT_EXECUTE_SECONDS;
    pthread_mutex_lock(&timer_alarm_mutex);
    timer_alarm= true;
    pthread_mutex_unlock(&timer_alarm_mutex);

    if (pthread_create(&mainthread, &attr, timer_thread,
                       (void *)&opt_timer_length) != 0)
    {
      fprintf(stderr,"%s: Could not create timer thread\n", drizzled::internal::my_progname);
      exit(1);
    }
  }

  pthread_mutex_unlock(&counter_mutex);
  pthread_attr_destroy(&attr);

  pthread_mutex_lock(&sleeper_mutex);
  master_wakeup= 0;
  pthread_mutex_unlock(&sleeper_mutex);
  pthread_cond_broadcast(&sleep_threshhold);

  /*
    We loop until we know that all children have cleaned up.
  */
  pthread_mutex_lock(&counter_mutex);
  while (thread_counter)
  {
    struct timespec abstime;

    memset(&abstime, 0, sizeof(struct timespec));
    abstime.tv_sec= 1;

    pthread_cond_timedwait(&count_threshhold, &counter_mutex, &abstime);
  }
  pthread_mutex_unlock(&counter_mutex);

  for (total= x= 0; x < DEFAULT_CONCURRENCY; x++)
    total+= context[x].counter;

  free(context);
  azclose(&writer_handle);

  printf("Read %"PRIu64" rows\n", total);
}

void *timer_thread(void *p)
{
  time_t *timer_length= (time_t *)p;
  struct timespec abstime;

  /*
    We lock around the initial call in case were we in a loop. This
    also keeps the value properly syncronized across call threads.
  */
  pthread_mutex_lock(&sleeper_mutex);
  while (master_wakeup)
  {
    pthread_cond_wait(&sleep_threshhold, &sleeper_mutex);
  }
  pthread_mutex_unlock(&sleeper_mutex);

  set_timespec(abstime, *timer_length);

  pthread_mutex_lock(&timer_alarm_mutex);
  pthread_cond_timedwait(&timer_alarm_threshold, &timer_alarm_mutex, &abstime);
  pthread_mutex_unlock(&timer_alarm_mutex);

  pthread_mutex_lock(&timer_alarm_mutex);
  timer_alarm= false;
  pthread_mutex_unlock(&timer_alarm_mutex);

  return 0;
}

void *run_concurrent_task(void *p)
{
  thread_context_st *context= (thread_context_st *)p;
  uint64_t count;
  int ret;
  int error;
  boost::scoped_ptr<azio_stream> reader_handle_ap(new azio_stream);
  azio_stream &reader_handle= *reader_handle_ap.get();

  if (!(ret= azopen(&reader_handle, TEST_FILENAME, O_RDONLY,
                    context->use_aio)))
  {
    printf("Could not open test file\n");
    return 0;
  }

  pthread_mutex_lock(&sleeper_mutex);
  while (master_wakeup)
  {
    pthread_cond_wait(&sleep_threshhold, &sleeper_mutex);
  }
  pthread_mutex_unlock(&sleeper_mutex);

  /* Do Stuff */
  count= 0;
  while (1)
  {
    azread_init(&reader_handle);
    while ((ret= azread_row(&reader_handle, &error)))
      context->counter++;

    if (count % context->how_often_to_write)
    {
      write_row(context->writer);
    }

    /* If the timer is set, and the alarm is not active then end */
    if (timer_alarm == false)
      break;
  }

  pthread_mutex_lock(&counter_mutex);
  thread_counter--;
  pthread_cond_signal(&count_threshhold);
  pthread_mutex_unlock(&counter_mutex);
  azclose(&reader_handle);

  return NULL;
}

void create_data_file(azio_stream *write_handler, uint64_t rows)
{
  int ret;
  uint64_t x;

  if (!(ret= azopen(write_handler, TEST_FILENAME, O_CREAT|O_RDWR|O_TRUNC,
                    AZ_METHOD_BLOCK)))
  {
    printf("Could not create test file\n");
    exit(1);
  }

  for (x= 0; x < rows; x++)
    write_row(write_handler);

  azflush(write_handler, Z_SYNC_FLUSH);
}

unsigned int write_row(azio_stream *s)
{
  size_t length;
  char buffer[HUGE_STRING_LENGTH];

  length= random() % HUGE_STRING_LENGTH;

  /* Avoid zero length strings */
  length++;

  get_random_string(buffer, length);
  pthread_mutex_lock(&row_lock);
  azwrite_row(s, buffer, length);
  pthread_mutex_unlock(&row_lock);

  return 0;
}
