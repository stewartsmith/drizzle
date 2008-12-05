/* Copyright (C) 2007 MySQL AB

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
  Implementation for the thread scheduler
*/

#include <drizzled/server_includes.h>
#include <libdrizzle/libdrizzle.h>
#include <event.h>
#include <drizzled/gettext.h>
#include <drizzled/sql_parse.h>
#include <drizzled/scheduler.h>
#include <drizzled/session.h>

/*
  'Dummy' functions to be used when we don't need any handling for a scheduler
  event
 */

static bool init_dummy(void) {return 0;}
static void post_kill_dummy(Session *session __attribute__((unused))) {}
static void end_dummy(void) {}
static bool end_thread_dummy(Session *session __attribute__((unused)),
                             bool cache_thread __attribute__((unused)))
{ return 0; }

/*
  Initialize default scheduler with dummy functions so that setup functions
  only need to declare those that are relvant for their usage
*/

scheduler_functions::scheduler_functions()
  :init(init_dummy),
   init_new_connection_thread(init_new_connection_handler_thread),
   add_connection(0),                           // Must be defined
   post_kill_notification(post_kill_dummy),
   end_thread(end_thread_dummy), end(end_dummy)
{}

static uint32_t created_threads, killed_threads;
static bool kill_pool_threads;

static struct event session_add_event;
static struct event session_kill_event;

static pthread_mutex_t LOCK_session_add;    /* protects sessions_need_adding */
static LIST *sessions_need_adding;    /* list of sessions to add to libevent queue */

static int session_add_pipe[2]; /* pipe to signal add a connection to libevent*/
static int session_kill_pipe[2]; /* pipe to signal kill a connection in libevent */

/*
  LOCK_event_loop protects the non-thread safe libevent calls (event_add and 
  event_del) and sessions_need_processing and sessions_waiting_for_io.
*/
static pthread_mutex_t LOCK_event_loop;
static LIST *sessions_need_processing; /* list of sessions that needs some processing */
static LIST *sessions_waiting_for_io; /* list of sessions with added events */

pthread_handler_t libevent_thread_proc(void *arg);
static void libevent_end();
static bool libevent_needs_immediate_processing(Session *session);
static void libevent_connection_close(Session *session);
static bool libevent_should_close_connection(Session* session);
static void libevent_session_add(Session* session);
void libevent_io_callback(int Fd, short Operation, void *ctx);
void libevent_add_session_callback(int Fd, short Operation, void *ctx);
void libevent_kill_session_callback(int Fd, short Operation, void *ctx);


/*
  Create a pipe and set to non-blocking.
  Returns true if there is an error.
*/

static bool init_pipe(int pipe_fds[])
{
  int flags;
  return pipe(pipe_fds) < 0 ||
          (flags= fcntl(pipe_fds[0], F_GETFL)) == -1 ||
          fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == -1;
          (flags= fcntl(pipe_fds[1], F_GETFL)) == -1 ||
          fcntl(pipe_fds[1], F_SETFL, flags | O_NONBLOCK) == -1;
}


/*
  session_scheduler keeps the link between Session and events.
  It's embedded in the Session class.
*/

session_scheduler::session_scheduler()
  : logged_in(false), io_event(NULL), thread_attached(false)
{  
}


session_scheduler::~session_scheduler()
{
  free(io_event);
}


session_scheduler::session_scheduler(const session_scheduler&)
  : logged_in(false), io_event(NULL), thread_attached(false)
{}

void session_scheduler::operator=(const session_scheduler&)
{}

bool session_scheduler::init(Session *parent_session)
{
  io_event= (struct event*)malloc(sizeof(*io_event));
    
  if (!io_event)
  {
    sql_print_error(_("Memory allocation error in session_scheduler::init\n"));
    return true;
  }
  memset(io_event, 0, sizeof(*io_event));
 
  event_set(io_event, net_get_sd(&(parent_session->net)), EV_READ, 
            libevent_io_callback, (void*)parent_session);
    
  list.data= parent_session;
  
  return false;
}


/*
  Attach/associate the connection with the OS thread, for command processing.
*/

bool session_scheduler::thread_attach()
{
  assert(!thread_attached);
  Session* session = (Session*)list.data;
  if (libevent_should_close_connection(session) ||
      setup_connection_thread_globals(session))
  {
    return true;
  }
  my_errno= 0;
  session->mysys_var->abort= 0;
  thread_attached= true;

  return false;
}


/*
  Detach/disassociate the connection with the OS thread.
*/

void session_scheduler::thread_detach()
{
  if (thread_attached)
  {
    Session* session = (Session*)list.data;
    session->mysys_var= NULL;
    thread_attached= false;
  }
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

static bool libevent_init(void)
{
  uint32_t i;

  event_init();
  
  created_threads= 0;
  killed_threads= 0;
  kill_pool_threads= false;
  
  pthread_mutex_init(&LOCK_event_loop, NULL);
  pthread_mutex_init(&LOCK_session_add, NULL);
  
  /* set up the pipe used to add new sessions to the event pool */
  if (init_pipe(session_add_pipe))
  {
    sql_print_error(_("init_pipe(session_add_pipe) error in libevent_init\n"));
    return(1);
  }
  /* set up the pipe used to kill sessions in the event queue */
  if (init_pipe(session_kill_pipe))
  {
    sql_print_error(_("init_pipe(session_kill_pipe) error in libevent_init\n"));
    close(session_add_pipe[0]);
    close(session_add_pipe[1]);
    return(1);
  }
  event_set(&session_add_event, session_add_pipe[0], EV_READ|EV_PERSIST,
            libevent_add_session_callback, NULL);
  event_set(&session_kill_event, session_kill_pipe[0], EV_READ|EV_PERSIST,
            libevent_kill_session_callback, NULL);
 
 if (event_add(&session_add_event, NULL) || event_add(&session_kill_event, NULL))
 {
   sql_print_error(_("session_add_event event_add error in libevent_init\n"));
   libevent_end();
   return(1);
   
 }
  /* Set up the thread pool */
  created_threads= killed_threads= 0;
  pthread_mutex_lock(&LOCK_thread_count);

  for (i= 0; i < thread_pool_size; i++)
  {
    pthread_t thread;
    int error;
    if ((error= pthread_create(&thread, &connection_attrib,
                               libevent_thread_proc, 0)))
    {
      sql_print_error(_("Can't create completion port thread (error %d)"),
                      error);
      pthread_mutex_unlock(&LOCK_thread_count);
      libevent_end();                      // Cleanup
      return(true);
    }
  }

  /* Wait until all threads are created */
  while (created_threads != thread_pool_size)
    pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  pthread_mutex_unlock(&LOCK_thread_count);
  
  return(false);
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
  sessions_waiting_for_io= list_delete(sessions_waiting_for_io, &session->scheduler.list);
  sessions_need_processing= list_add(sessions_need_processing, &session->scheduler.list);
}

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

  LIST* list= sessions_waiting_for_io;
  while (list)
  {
    Session *session= (Session*)list->data;
    list= list_rest(list);
    if (session->killed == Session::KILL_CONNECTION)
    {
      /*
        Delete from libevent and add to the processing queue.
      */
      event_del(session->scheduler.io_event);
      sessions_waiting_for_io= list_delete(sessions_waiting_for_io,
                                       &session->scheduler.list);
      sessions_need_processing= list_add(sessions_need_processing,
                                     &session->scheduler.list);
    }
  }
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
  while (sessions_need_adding)
  {
    /* pop the first session off the list */
    Session* session= (Session*)sessions_need_adding->data;
    sessions_need_adding= list_delete(sessions_need_adding, sessions_need_adding);

    pthread_mutex_unlock(&LOCK_session_add);
    
    if (!session->scheduler.logged_in || libevent_should_close_connection(session))
    {
      /*
        Add session to sessions_need_processing list. If it needs closing we'll close
        it outside of event_loop().
      */
      sessions_need_processing= list_add(sessions_need_processing,
                                     &session->scheduler.list);
    }
    else
    {
      /* Add to libevent */
      if (event_add(session->scheduler.io_event, NULL))
      {
        sql_print_error(_("event_add error in libevent_add_session_callback\n"));
        libevent_connection_close(session);
      } 
      else
      {
        sessions_waiting_for_io= list_add(sessions_waiting_for_io,
                                      &session->scheduler.list);
      }
    }
    pthread_mutex_lock(&LOCK_session_add);
  }
  pthread_mutex_unlock(&LOCK_session_add);
}


/**
  Notify the thread pool about a new connection

  NOTES
    LOCK_thread_count is locked on entry. This function MUST unlock it!
*/

static void libevent_add_connection(Session *session)
{
  if (session->scheduler.init(session))
  {
    sql_print_error(_("Scheduler init error in libevent_add_new_connection\n"));
    pthread_mutex_unlock(&LOCK_thread_count);
    libevent_connection_close(session);
    return;
  }
  threads.append(session);
  libevent_session_add(session);
  
  pthread_mutex_unlock(&LOCK_thread_count);
  return;
}


/**
  @brief Signal a waiting connection it's time to die.
 
  @details This function will signal libevent the Session should be killed.
    Either the global LOCK_session_count or the Session's LOCK_delete must be locked
    upon entry.
 
  @param[in]  session The connection to kill
*/

static void libevent_post_kill_notification(Session *)
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


/*
  Close and delete a connection.
*/

static void libevent_connection_close(Session *session)
{
  session->killed= Session::KILL_CONNECTION;          // Avoid error messages

  if (net_get_sd(&(session->net)) >= 0)                  // not already closed
  {
    end_connection(session);
    close_connection(session, 0, 1);
  }
  session->scheduler.thread_detach();
  unlink_session(session);   /* locks LOCK_thread_count and deletes session */
  pthread_mutex_unlock(&LOCK_thread_count);

  return;
}


/*
  Returns true if we should close and delete a Session connection.
*/

static bool libevent_should_close_connection(Session* session)
{
  return net_should_close(&(session->net)) ||
         session->killed == Session::KILL_CONNECTION;
}


/*
  libevent_thread_proc is the outer loop of each thread in the thread pool.
  These procs only return/terminate on shutdown (kill_pool_threads == true).
*/

pthread_handler_t libevent_thread_proc(void *arg __attribute__((unused)))
{
  if (init_new_connection_handler_thread())
  {
    my_thread_global_end();
    sql_print_error(_("libevent_thread_proc: my_thread_init() failed\n"));
    exit(1);
  }

  /*
    Signal libevent_init() when all threads has been created and are ready to
    receive events.
  */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  created_threads++;
  if (created_threads == thread_pool_size)
    (void) pthread_cond_signal(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  
  for (;;)
  {
    Session *session= NULL;
    (void) pthread_mutex_lock(&LOCK_event_loop);
    
    /* get session(s) to process */
    while (!sessions_need_processing)
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
    session= (Session*)sessions_need_processing->data;
    sessions_need_processing= list_delete(sessions_need_processing,
                                      sessions_need_processing);
    
    (void) pthread_mutex_unlock(&LOCK_event_loop);
    
    /* now we process the connection (session) */
    
    /* set up the session<->thread links. */
    session->thread_stack= (char*) &session;
    
    if (session->scheduler.thread_attach())
    {
      libevent_connection_close(session);
      continue;
    }

    /* is the connection logged in yet? */
    if (!session->scheduler.logged_in)
    {
      if (login_connection(session))
      {
        /* Failed to log in */
        libevent_connection_close(session);
        continue;
      }
      else
      {
        /* login successful */
        session->scheduler.logged_in= true;
        prepare_new_connection_state(session);
        if (!libevent_needs_immediate_processing(session))
          continue; /* New connection is now waiting for data in libevent*/
      }
    }

    do
    {
      /* Process a query */
      if (do_command(session))
      {
        libevent_connection_close(session);
        break;
      }
    } while (libevent_needs_immediate_processing(session));
  }
  
thread_exit:
  (void) pthread_mutex_lock(&LOCK_thread_count);
  killed_threads++;
  pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  my_thread_end();
  pthread_exit(0);
  return(0);                               /* purify: deadcode */
}


/*
  Returns true if the connection needs immediate processing and false if 
  instead it's queued for libevent processing or closed,
*/

static bool libevent_needs_immediate_processing(Session *session)
{
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
  if (net_more_data(&(session->net)))
    return true;
  
  session->scheduler.thread_detach();
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

static void libevent_session_add(Session* session)
{
  char c=0;
  pthread_mutex_lock(&LOCK_session_add);
  /* queue for libevent */
  sessions_need_adding= list_add(sessions_need_adding, &session->scheduler.list);
  /* notify libevent */
  assert(write(session_add_pipe[1], &c, sizeof(c))==sizeof(c));
  pthread_mutex_unlock(&LOCK_session_add);
}


/**
  Wait until all pool threads have been deleted for clean shutdown
*/

static void libevent_end()
{
  (void) pthread_mutex_lock(&LOCK_thread_count);
  
  kill_pool_threads= true;
  while (killed_threads != created_threads)
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
  return;
}


void pool_of_threads_scheduler(scheduler_functions* func)
{
  func->max_threads= thread_pool_size;
  func->init= libevent_init;
  func->end=  libevent_end;
  func->post_kill_notification= libevent_post_kill_notification;
  func->add_connection= libevent_add_connection;
}
