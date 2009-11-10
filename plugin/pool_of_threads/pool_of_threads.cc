/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2006 MySQL AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <drizzled/server_includes.h>
#include <plugin/pool_of_threads/pool_of_threads.h>

using namespace std;
using namespace drizzled;

/* We add this so as to avoid ambiguity with "struct queue" defined in Solaris system header */
using std::queue;

/* Global's (TBR) */
static PoolOfThreadsScheduler *scheduler= NULL;

/**
 * Set this to true to trigger killing of all threads in the pool
 */
static volatile bool kill_pool_threads= false;

static volatile uint32_t created_threads= 0;
static int deinit(drizzled::plugin::Registry &registry);

static struct event session_add_event;
static struct event session_kill_event;

static pthread_mutex_t LOCK_session_add;    /* protects sessions_need_adding */
static queue<Session *> sessions_need_adding; /* queue of sessions to add to libevent queue */

static pthread_mutex_t LOCK_session_kill;    /* protects sessions_to_be_killed */
static queue<Session *> sessions_to_be_killed; /* queue of sessions to be killed */

static int session_add_pipe[2]; /* pipe to signal add a connection to libevent*/
static int session_kill_pipe[2]; /* pipe to signal kill a connection in libevent */

/**
 * LOCK_event_loop protects the non-thread safe libevent calls (event_add
 * and event_del) and sessions_need_processing and sessions_waiting_for_io.
 */
static pthread_mutex_t LOCK_event_loop;
static queue<Session *> sessions_need_processing; /* queue of sessions that needs some processing */

/** 
 * Collection of sessions with added events 
 *  
 * This should really be a collection of unordered Sessions. No one is more
 * promising to encounter an io event earlier than another; so no order
 * indeed! We will change this to unordered_set/hash_set when c++0x comes.
 */
static set<Session *> sessions_waiting_for_io;

static bool libevent_needs_immediate_processing(Session *session);
static void libevent_connection_close(Session *session);
void libevent_session_add(Session* session);
bool libevent_should_close_connection(Session* session);
extern "C" {
  pthread_handler_t libevent_thread_proc(void *arg);
  void libevent_io_callback(int Fd, short Operation, void *ctx);
  void libevent_add_session_callback(int Fd, short Operation, void *ctx);
  void libevent_kill_session_callback(int Fd, short Operation, void *ctx);
}

static uint32_t size= 0;

/**
 * @brief 
 *  Create a pipe and set to non-blocking. 
 * @return 
 *  True if there is an error.
 */
static bool init_pipe(int pipe_fds[])
{
  int flags;
  return pipe(pipe_fds) < 0 ||
          (flags= fcntl(pipe_fds[0], F_GETFL)) == -1 ||
          fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == -1 ||
          (flags= fcntl(pipe_fds[1], F_GETFL)) == -1 ||
          fcntl(pipe_fds[1], F_SETFL, flags | O_NONBLOCK) == -1;
}





/**
 * @brief 
 *  This is called when data is ready on the socket.  
 *
 * @details 
 *  This is only called by the thread that owns LOCK_event_loop.
 *
 *  We add the session that got the data to sessions_need_processing, and
 *  cause the libevent event_loop() to terminate. Then this same thread will
 *  return from event_loop and pick the session value back up for
 *  processing.
 */
void libevent_io_callback(int, short, void *ctx)
{
  safe_mutex_assert_owner(&LOCK_event_loop);
  Session *session= (Session*)ctx;
  session_scheduler *sched= (session_scheduler *)session->scheduler_arg;
  assert(sched);
  sessions_waiting_for_io.erase(sched->session);
  sessions_need_processing.push(sched->session);
}

/**
 * @brief 
 *  This is called when we have a thread we want to be killed.
 *
 * @details
 *  This is only called by the thread that owns LOCK_event_loop.
 */
void libevent_kill_session_callback(int Fd, short, void*)
{
  safe_mutex_assert_owner(&LOCK_event_loop);

  /*
   For pending events clearing
  */
  char c;
  int count= 0;

  pthread_mutex_lock(&LOCK_session_kill);
  while (! sessions_to_be_killed.empty())
  {
    /*
     Fetch a session from the queue
    */
    Session* session= sessions_to_be_killed.front();
    pthread_mutex_unlock(&LOCK_session_kill);

    session_scheduler *sched= (session_scheduler *)session->scheduler_arg;
    assert(sched);
    /*
     Delete from libevent and add to the processing queue.
    */
    event_del(&sched->io_event);
    /*
     Remove from the sessions_waiting_for_io set
    */
    sessions_waiting_for_io.erase(session);
    /*
     Push into the sessions_need_processing; the kill action will be
     performed out of the event loop
    */
    sessions_need_processing.push(sched->session);

    pthread_mutex_lock(&LOCK_session_kill);
    /*
     Pop until this session is already processed
    */
    sessions_to_be_killed.pop();
  }
  
  /*
   Clear the pending events 
   One and only one charactor should be in the pipe
  */
  while (read(Fd, &c, sizeof(c)) == sizeof(c))
  {
    count++;
  }
  assert(count == 1);
  pthread_mutex_unlock(&LOCK_session_kill);
}


/**
 * @brief
 *  This is used to add connections to the pool. This callback is invoked
 *  from the libevent event_loop() call whenever the session_add_pipe[1]
 *  pipe has a byte written to it.
 *
 * @details     
 *  This is only called by the thread that owns LOCK_event_loop.
 */
void libevent_add_session_callback(int Fd, short, void *)
{
  safe_mutex_assert_owner(&LOCK_event_loop);

  /*
   For pending events clearing
  */
  char c;
  int count= 0;

  pthread_mutex_lock(&LOCK_session_add);
  while (! sessions_need_adding.empty())
  {
    /*
     Pop the first session off the queue 
    */
    Session* session= sessions_need_adding.front();
    session_scheduler *sched= (session_scheduler *)session->scheduler_arg;
    assert(sched);

    pthread_mutex_unlock(&LOCK_session_add);

    if (!sched->logged_in || libevent_should_close_connection(session))
    {
      /*
       Add session to sessions_need_processing queue. If it needs closing
       we'll close it outside of event_loop().
      */
      sessions_need_processing.push(sched->session);
    }
    else
    {
      /* Add to libevent */
      if (event_add(&sched->io_event, NULL))
      {
        errmsg_printf(ERRMSG_LVL_ERROR, _("event_add error in libevent_add_session_callback\n"));
        libevent_connection_close(session);
      }
      else
      {
        sessions_waiting_for_io.insert(sched->session);
      }
    }

    pthread_mutex_lock(&LOCK_session_add);
    /*
     Pop until this session is already processed
    */
    sessions_need_adding.pop();
  }

  /*
   Clear the pending events 
   One and only one charactor should be in the pipe
  */
  while (read(Fd, &c, sizeof(c)) == sizeof(c))
  {
    count++;
  }
  assert(count == 1);
  pthread_mutex_unlock(&LOCK_session_add);
}

/**
 * @brief 
 *  Close and delete a connection.
 */
static void libevent_connection_close(Session *session)
{
  session_scheduler *sched= (session_scheduler *)session->scheduler_arg;
  assert(sched);
  session->killed= Session::KILL_CONNECTION;    /* Avoid error messages */

  if (session->client->getFileDescriptor() >= 0) /* not already closed */
  {
    session->disconnect(0, true);
  }
  sched->thread_detach();
  
  delete sched;
  session->scheduler_arg= NULL;

  unlink_session(session);   /* locks LOCK_thread_count and deletes session */

  return;
}


/**
 * @brief 
 *  Checks if a session should be closed.
 *  
 * @retval true this session should be closed.  
 * @retval false not to be closed.
 */
bool libevent_should_close_connection(Session* session)
{
  return session->client->haveError() ||
         session->killed == Session::KILL_CONNECTION;
}


/**
 * @brief
 *  libevent_thread_proc is the outer loop of each thread in the thread pool.
 *  These procs only return/terminate on shutdown (kill_pool_threads ==
 *  true).
 */
pthread_handler_t libevent_thread_proc(void *)
{
  if (my_thread_init())
  {
    my_thread_global_end();
    errmsg_printf(ERRMSG_LVL_ERROR, _("libevent_thread_proc: my_thread_init() failed\n"));
    exit(1);
  }

  /*
   Signal libevent_init() when all threads has been created and are ready
   to receive events.
  */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  created_threads++;
  if (created_threads == size)
    (void) pthread_cond_signal(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  for (;;)
  {
    Session *session= NULL;
    (void) pthread_mutex_lock(&LOCK_event_loop);

    /* get session(s) to process */
    while (sessions_need_processing.empty())
    {
      if (kill_pool_threads)
      {
        /* the flag that we should die has been set */
        (void) pthread_mutex_unlock(&LOCK_event_loop);
        goto thread_exit;
      }
      event_loop(EVLOOP_ONCE);
    }

    /* pop the first session off the queue */
    session= sessions_need_processing.front();
    sessions_need_processing.pop();
    session_scheduler *sched= (session_scheduler *)session->scheduler_arg;

    (void) pthread_mutex_unlock(&LOCK_event_loop);

    /* now we process the connection (session) */

    /* set up the session<->thread links. */
    session->thread_stack= (char*) &session;

    if (sched->thread_attach())
    {
      libevent_connection_close(session);
      continue;
    }

    /* is the connection logged in yet? */
    if (!sched->logged_in)
    {
      if (session->authenticate())
      {
        /* Failed to log in */
        libevent_connection_close(session);
        continue;
      }
      else
      {
        /* login successful */
        sched->logged_in= true;
        session->prepareForQueries();
        if (!libevent_needs_immediate_processing(session))
          continue; /* New connection is now waiting for data in libevent*/
      }
    }

    do
    {
      /* Process a query */
      if (! session->executeStatement())
      {
        libevent_connection_close(session);
        break;
      }
    } while (libevent_needs_immediate_processing(session));

    if (kill_pool_threads) /* the flag that we should die has been set */
      goto thread_exit;
  }

thread_exit:
  (void) pthread_mutex_lock(&LOCK_thread_count);
  created_threads--;
  pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  my_thread_end();
  pthread_exit(0);

  return NULL;                               /* purify: deadcode */
}


/**
 * @brief
 *  Checks if a session needs immediate processing
 *
 * @retval true the session needs immediate processing 
 * @retval false if not, and is detached from the thread waiting for another
 * adding. The naming of the function is misleading in this case; it
 * actually does more than just checking if immediate processing is needed.
 */
static bool libevent_needs_immediate_processing(Session *session)
{
  session_scheduler *sched= (session_scheduler *)session->scheduler_arg;

  if (libevent_should_close_connection(session))
  {
    libevent_connection_close(session);
    return false;
  }
  /*
   If more data in the socket buffer, return true to process another command.
  
   Note: we cannot add for event processing because the whole request
   might already be buffered and we wouldn't receive an event. This is
   indeed the root of the reason of low performace. Need to be changed
   when nonblocking Protocol is finished.
  */
  if (session->client->haveMoreData())
    return true;

  sched->thread_detach();
  libevent_session_add(session);

  return false;
}


/**
 * @brief 
 *  Adds a Session to queued for libevent processing.
 * 
 * @details
 *  This call does not actually register the event with libevent.
 *  Instead, it places the Session onto a queue and signals libevent by writing
 *  a byte into session_add_pipe, which will cause our libevent_add_session_callback to
 *  be invoked which will find the Session on the queue and add it to libevent.
 */
void libevent_session_add(Session* session)
{
  char c= 0;
  session_scheduler *sched= (session_scheduler *)session->scheduler_arg;
  assert(sched);

  pthread_mutex_lock(&LOCK_session_add);
  if (sessions_need_adding.empty())
  {
    /* notify libevent */
    size_t written= write(session_add_pipe[1], &c, sizeof(c));
    assert(written == sizeof(c));
  }
  /* queue for libevent */
  sessions_need_adding.push(sched->session);
  pthread_mutex_unlock(&LOCK_session_add);
}


PoolOfThreadsScheduler::~PoolOfThreadsScheduler()
{
  (void) pthread_mutex_lock(&LOCK_thread_count);

  kill_pool_threads= true;
  while (created_threads)
  {
    /* 
      Wake up the event loop 
    */
    char c= 0;
    size_t written= write(session_add_pipe[1], &c, sizeof(c));
    assert(written == sizeof(c));

    pthread_cond_wait(&COND_thread_count, &LOCK_thread_count);
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  event_del(&session_add_event);
  close(session_add_pipe[0]);
  close(session_add_pipe[1]);
  event_del(&session_kill_event);
  close(session_kill_pipe[0]);
  close(session_kill_pipe[1]);

  (void) pthread_mutex_destroy(&LOCK_event_loop);
  (void) pthread_mutex_destroy(&LOCK_session_add);
  (void) pthread_mutex_destroy(&LOCK_session_kill);
  (void) pthread_attr_destroy(&attr);
}


bool PoolOfThreadsScheduler::addSession(Session *session)
{
  assert(session->scheduler_arg == NULL);
  session_scheduler *sched= new session_scheduler(session);

  if (sched == NULL)
    return true;

  session->scheduler_arg= (void *)sched;

  libevent_session_add(session);

  return false;
}


void PoolOfThreadsScheduler::killSession(Session *session)
{
  char c= 0;

  pthread_mutex_lock(&LOCK_session_kill);

  if (sessions_to_be_killed.empty())
  {
    /* 
      Notify libevent with the killing event if this's the first killing
      notification of the batch
    */
    size_t written= write(session_kill_pipe[1], &c, sizeof(c));
    assert(written == sizeof(c));
  }

  /*
    Push into the sessions_to_be_killed queue
  */
  sessions_to_be_killed.push(session);
  pthread_mutex_unlock(&LOCK_session_kill);
}


bool PoolOfThreadsScheduler::libevent_init(void)
{
  uint32_t x;

  event_init();

  pthread_mutex_init(&LOCK_event_loop, NULL);
  pthread_mutex_init(&LOCK_session_add, NULL);
  pthread_mutex_init(&LOCK_session_kill, NULL);

  /* Set up the pipe used to add new sessions to the event pool */
  if (init_pipe(session_add_pipe))
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("init_pipe(session_add_pipe) error in libevent_init\n"));
    return true;
  }
  /* Set up the pipe used to kill sessions in the event queue */
  if (init_pipe(session_kill_pipe))
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("init_pipe(session_kill_pipe) error in libevent_init\n"));
    close(session_add_pipe[0]);
    close(session_add_pipe[1]);
    return true;
  }
  event_set(&session_add_event, session_add_pipe[0], EV_READ|EV_PERSIST,
            libevent_add_session_callback, NULL);
  event_set(&session_kill_event, session_kill_pipe[0], EV_READ|EV_PERSIST,
            libevent_kill_session_callback, NULL);

  if (event_add(&session_add_event, NULL) || event_add(&session_kill_event, NULL))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("session_add_event event_add error in libevent_init\n"));
    return true;

  }
  /* Set up the thread pool */
  pthread_mutex_lock(&LOCK_thread_count);

  for (x= 0; x < size; x++)
  {
    pthread_t thread;
    int error;
    if ((error= pthread_create(&thread, &attr, libevent_thread_proc, 0)))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Can't create completion port thread (error %d)"),
                    error);
      pthread_mutex_unlock(&LOCK_thread_count);
      return true;
    }
  }

  /* Wait until all threads are created */
  while (created_threads != size)
    pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  pthread_mutex_unlock(&LOCK_thread_count);

  return false;
}


/**
 * @brief
 *  Called to initialize the pool of threads scheduler plugin
 * 
 * @param[in] registry holding the record of the plugins
 */
static int init(drizzled::plugin::Registry &registry)
{
  assert(size != 0);

  scheduler= new PoolOfThreadsScheduler("pool_of_threads");
  registry.add(scheduler);

  return 0;
}

/**
 * @brief
 *  Waits until all pool threads have been deleted for clean shutdown
 */
static int deinit(drizzled::plugin::Registry &registry)
{
  registry.remove(scheduler);
  delete scheduler;

  return 0;
}

/*
 The defaults here were picked based on what I see (aka Brian). They should
 be vetted across a larger audience.
*/
static DRIZZLE_SYSVAR_UINT(size, size,
                           PLUGIN_VAR_RQCMDARG,
                           N_("Size of Pool."),
                           NULL, NULL, 8, 1, 1024, 0);

static struct st_mysql_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(size),
  NULL,
};

drizzle_declare_plugin
{
  "pool_of_threads",
  "0.1",
  "Brian Aker",
  "Pool of Threads Scheduler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  system_variables,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
