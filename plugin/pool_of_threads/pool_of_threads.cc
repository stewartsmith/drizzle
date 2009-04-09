/* Copyright (C) 2006 MySQL AB

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

#include <drizzled/server_includes.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin/scheduler.h>
#include <drizzled/serialize/serialize.h>
#include <drizzled/connect.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include "session_scheduler.h"
#include <string>
#include <list>
#include <event.h>

using namespace std;

static volatile bool kill_pool_threads= false;

static volatile uint32_t created_threads= 0;
static int deinit(void *);

static struct event session_add_event;
static struct event session_kill_event;

static pthread_mutex_t LOCK_session_add;    /* protects sessions_need_adding */
static list<Session *> sessions_need_adding; /* list of sessions to add to libevent queue */

static int session_add_pipe[2]; /* pipe to signal add a connection to libevent*/
static int session_kill_pipe[2]; /* pipe to signal kill a connection in libevent */

/*
  LOCK_event_loop protects the non-thread safe libevent calls (event_add and
  event_del) and sessions_need_processing and sessions_waiting_for_io.
*/
static pthread_mutex_t LOCK_event_loop;
static list<Session *> sessions_need_processing; /* list of sessions that needs some processing */
static list<Session *> sessions_waiting_for_io; /* list of sessions with added events */

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

/*
  Create a pipe and set to non-blocking.
  Returns true if there is an error.
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





/*
  This is called when data is ready on the socket.

  NOTES
    This is only called by the thread that owns LOCK_event_loop.

    We add the session that got the data to sessions_need_processing, and
    cause the libevent event_loop() to terminate. Then this same thread will
    return from event_loop and pick the session value back up for processing.
*/

void libevent_io_callback(int, short, void *ctx)
{
  safe_mutex_assert_owner(&LOCK_event_loop);
  Session *session= (Session*)ctx;
  session_scheduler *scheduler= (session_scheduler *)session->scheduler;
  assert(scheduler);
  sessions_waiting_for_io.remove(scheduler->session);
  sessions_need_processing.push_front(scheduler->session);
}

/*
  Function object which is used to determine whether to remove
  a session from the sessions_waiting_for_io list.
*/
class remove_session_if
{
  public:
  remove_session_if() { }
  inline bool operator()(const Session *session) const
  {
    return (session->killed == Session::KILL_CONNECTION);
  }
};

/*
  This is called when we have a thread we want to be killed.

  NOTES
    This is only called by the thread that owns LOCK_event_loop.
*/

void libevent_kill_session_callback(int Fd, short, void*)
{
  safe_mutex_assert_owner(&LOCK_event_loop);

  /* clear the pending events */
  char c;
  while (read(Fd, &c, sizeof(c)) == sizeof(c))
  {}

  list<Session *>::iterator it= sessions_waiting_for_io.begin();
  while (it != sessions_waiting_for_io.end())
  {
    Session *session= *it;
    if (session->killed == Session::KILL_CONNECTION)
    {
      session_scheduler *scheduler= (session_scheduler *)session->scheduler;
      assert(scheduler);
      /*
        Delete from libevent and add to the processing queue.
      */
      event_del(&scheduler->io_event);
      sessions_need_processing.push_front(scheduler->session);
    }
    ++it;
  }

  /* 
    safely remove elements from the sessions_waiting_for_io list
  */
  sessions_waiting_for_io.erase(std::remove_if(sessions_waiting_for_io.begin(),
                                               sessions_waiting_for_io.end(),
                                               remove_session_if()),
                                sessions_waiting_for_io.end());
}


/*
  This is used to add connections to the pool. This callback is invoked from
  the libevent event_loop() call whenever the session_add_pipe[1] pipe has a byte
  written to it.

  NOTES
    This is only called by the thread that owns LOCK_event_loop.
*/

void libevent_add_session_callback(int Fd, short, void *)
{
  safe_mutex_assert_owner(&LOCK_event_loop);

  /* clear the pending events */
  char c;
  while (read(Fd, &c, sizeof(c)) == sizeof(c))
  {}

  pthread_mutex_lock(&LOCK_session_add);
  while (!sessions_need_adding.empty())
  {
    /* pop the first session off the list */
    Session* session= sessions_need_adding.front();
    sessions_need_adding.pop_front();
    session_scheduler *scheduler= (session_scheduler *)session->scheduler;
    assert(scheduler);

    pthread_mutex_unlock(&LOCK_session_add);

    if (!scheduler->logged_in || libevent_should_close_connection(session))
    {
      /*
        Add session to sessions_need_processing list. If it needs closing we'll close
        it outside of event_loop().
      */
      sessions_need_processing.push_front(scheduler->session);
    }
    else
    {
      /* Add to libevent */
      if (event_add(&scheduler->io_event, NULL))
      {
        errmsg_printf(ERRMSG_LVL_ERROR, _("event_add error in libevent_add_session_callback\n"));
        libevent_connection_close(session);
      }
      else
      {
        sessions_waiting_for_io.push_front(scheduler->session);
      }
    }
    pthread_mutex_lock(&LOCK_session_add);
  }
  pthread_mutex_unlock(&LOCK_session_add);
}


class Pool_of_threads_scheduler: public Scheduler
{
private:
  pthread_attr_t thread_attrib;

public:
  Pool_of_threads_scheduler(uint32_t max_size_in)
    : Scheduler(max_size_in)
  {
    /* Parameter for threads created for connections */
    (void) pthread_attr_init(&thread_attrib);
    (void) pthread_attr_setdetachstate(&thread_attrib,
  				     PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&thread_attrib, PTHREAD_SCOPE_SYSTEM);
    {
      struct sched_param tmp_sched_param;
  
      memset(&tmp_sched_param, 0, sizeof(tmp_sched_param));
      tmp_sched_param.sched_priority= WAIT_PRIOR;
      (void)pthread_attr_setschedparam(&thread_attrib, &tmp_sched_param);
    }
  }

  ~Pool_of_threads_scheduler()
  {
    (void) pthread_mutex_lock(&LOCK_thread_count);
  
    kill_pool_threads= true;
    while (created_threads)
    {
      /* wake up the event loop */
      char c= 0;
      assert(write(session_add_pipe[1], &c, sizeof(c))==sizeof(c));
  
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
  }

  /**
    Notify the thread pool about a new connection
  
    NOTES
      LOCK_thread_count is locked on entry. This function MUST unlock it!
  */
  
  virtual bool add_connection(Session *session)
  {
    assert(session->scheduler == NULL);
    session_scheduler *scheduler= new session_scheduler(session);
  
    if (scheduler == NULL)
      return true;
  
    session->scheduler= (void *)scheduler;
  
    libevent_session_add(session);
  
    return false;
  }
  
  
  /**
    @brief Signal a waiting connection it's time to die.
  
    @details This function will signal libevent the Session should be killed.
      Either the global LOCK_session_count or the Session's LOCK_delete must be locked
      upon entry.
  
    @param[in]  session The connection to kill
  */
  
  virtual void post_kill_notification(Session *)
  {
    /*
      Note, we just wake up libevent with an event that a Session should be killed,
      It will search its list of sessions for session->killed ==  KILL_CONNECTION to
      find the Sessions it should kill.
  
      So we don't actually tell it which one and we don't actually use the
      Session being passed to us, but that's just a design detail that could change
      later.
    */
    char c= 0;
    assert(write(session_kill_pipe[1], &c, sizeof(c))==sizeof(c));
  }

  virtual uint32_t count(void)
  {
    return created_threads;
  }

  /**
    Create all threads for the thread pool
  
    NOTES
      After threads are created we wait until all threads has signaled that
      they have started before we return
  
    RETURN
      0  ok
      1  We got an error creating the thread pool
         In this case we will abort all created threads
  */
  
  bool libevent_init(void)
  {
    uint32_t x;
  
    event_init();
  
    pthread_mutex_init(&LOCK_event_loop, NULL);
    pthread_mutex_init(&LOCK_session_add, NULL);
  
    /* set up the pipe used to add new sessions to the event pool */
    if (init_pipe(session_add_pipe))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("init_pipe(session_add_pipe) error in libevent_init\n"));
      return true;
    }
    /* set up the pipe used to kill sessions in the event queue */
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
     deinit(NULL);
     return true;
  
   }
    /* Set up the thread pool */
    pthread_mutex_lock(&LOCK_thread_count);
  
    for (x= 0; x < size; x++)
    {
      pthread_t thread;
      int error;
      if ((error= pthread_create(&thread, &thread_attrib, libevent_thread_proc, 0)))
      {
        errmsg_printf(ERRMSG_LVL_ERROR, _("Can't create completion port thread (error %d)"),
                        error);
        pthread_mutex_unlock(&LOCK_thread_count);
        deinit(NULL);                      // Cleanup
        return true;
      }
    }
  
    /* Wait until all threads are created */
    while (created_threads != size)
      pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
    pthread_mutex_unlock(&LOCK_thread_count);
  
    return false;
  }
}; 

/*
  Close and delete a connection.
*/

static void libevent_connection_close(Session *session)
{
  session_scheduler *scheduler= (session_scheduler *)session->scheduler;
  assert(scheduler);
  session->killed= Session::KILL_CONNECTION;          // Avoid error messages

  if (session->protocol->fileDescriptor() >= 0)      // not already closed
  {
    session->disconnect(0, true);
  }
  scheduler->thread_detach();
  
  delete scheduler;
  session->scheduler= NULL;

  unlink_session(session);   /* locks LOCK_thread_count and deletes session */

  return;
}


/*
  Returns true if we should close and delete a Session connection.
*/

bool libevent_should_close_connection(Session* session)
{
  return session->protocol->haveError() ||
         session->killed == Session::KILL_CONNECTION;
}


/*
  libevent_thread_proc is the outer loop of each thread in the thread pool.
  These procs only return/terminate on shutdown (kill_pool_threads == true).
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
    Signal libevent_init() when all threads has been created and are ready to
    receive events.
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

    /* pop the first session off the list */
    session= sessions_need_processing.front();
    sessions_need_processing.pop_front();
    session_scheduler *scheduler= (session_scheduler *)session->scheduler;

    (void) pthread_mutex_unlock(&LOCK_event_loop);

    /* now we process the connection (session) */

    /* set up the session<->thread links. */
    session->thread_stack= (char*) &session;

    if (scheduler->thread_attach())
    {
      libevent_connection_close(session);
      continue;
    }

    /* is the connection logged in yet? */
    if (!scheduler->logged_in)
    {
      if (! session->authenticate())
      {
        /* Failed to log in */
        libevent_connection_close(session);
        continue;
      }
      else
      {
        /* login successful */
        scheduler->logged_in= true;
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


/*
  Returns true if the connection needs immediate processing and false if
  instead it's queued for libevent processing or closed,
*/

static bool libevent_needs_immediate_processing(Session *session)
{
  session_scheduler *scheduler= (session_scheduler *)session->scheduler;

  if (libevent_should_close_connection(session))
  {
    libevent_connection_close(session);
    return false;
  }
  /*
    If more data in the socket buffer, return true to process another command.

    Note: we cannot add for event processing because the whole request might
    already be buffered and we wouldn't receive an event.
  */
  if (session->protocol->haveMoreData())
    return true;

  scheduler->thread_detach();
  libevent_session_add(session);

  return false;
}


/*
  Adds a Session to queued for libevent processing.

  This call does not actually register the event with libevent.
  Instead, it places the Session onto a queue and signals libevent by writing
  a byte into session_add_pipe, which will cause our libevent_add_session_callback to
  be invoked which will find the Session on the queue and add it to libevent.
*/

void libevent_session_add(Session* session)
{
  char c= 0;
  session_scheduler *scheduler= (session_scheduler *)session->scheduler;
  assert(scheduler);

  pthread_mutex_lock(&LOCK_session_add);
  /* queue for libevent */
  sessions_need_adding.push_front(scheduler->session);
  /* notify libevent */
  assert(write(session_add_pipe[1], &c, sizeof(c))==sizeof(c));
  pthread_mutex_unlock(&LOCK_session_add);
}



static int init(void *p)
{
  assert(size != 0);

  void **plugin= static_cast<void **>(p);

  Pool_of_threads_scheduler *sched=
    new Pool_of_threads_scheduler(size);
  if (sched->libevent_init())
  {
    delete sched;
    return 1;
  }

  *plugin= static_cast<void *>(sched);

  return 0;
}

/**
  Wait until all pool threads have been deleted for clean shutdown
*/

static int deinit(void *p)
{
  Scheduler *sched= static_cast<Scheduler *>(p);
  delete sched;

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

drizzle_declare_plugin(pool_of_threads)
{
  DRIZZLE_SCHEDULING_PLUGIN,
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
