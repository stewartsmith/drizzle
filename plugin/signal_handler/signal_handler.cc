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

static bool kill_in_progress= false;
static bool volatile signal_thread_in_use= false;
extern int cleanup_done;


/* Prototypes -> all of these should be factored out into a propper shutdown */
void close_connections(void);
extern "C" void unireg_end(void);
extern "C" void unireg_abort(int exit_code);
bool reload_cache(Session *session, ulong options, TableList *tables);


/**
  Force server down. Kill all connections and threads and exit.

  @param  sig_ptr       Signal number that caused kill_server to be called.

  @note
    A signal number of 0 mean that the function was not called
    from a signal handler and there is thus no signal to block
    or stop, we just want to kill the server.
*/

static void *kill_server(void *sig_ptr)
{
  int sig=(int) (long) sig_ptr;			// This is passed a int
  // if there is a signal during the kill in progress, ignore the other
  if (kill_in_progress)				// Safety
    return NULL;
  kill_in_progress=true;
  abort_loop=1;					// This should be set
  if (sig != 0) // 0 is not a valid signal number
    my_sigset(sig, SIG_IGN);                    /* purify inspected */
  if (sig == SIGTERM || sig == 0)
    errmsg_printf(ERRMSG_LVL_INFO, _(ER(ER_NORMAL_SHUTDOWN)),my_progname);
  else
    errmsg_printf(ERRMSG_LVL_ERROR, _(ER(ER_GOT_SIGNAL)),my_progname,sig); /* purecov: inspected */

  close_connections();
  if (sig != SIGTERM && sig != 0)
    unireg_abort(1);				/* purecov: inspected */
  else
    unireg_end();

  /* purecov: begin deadcode */

  my_thread_end();
  pthread_exit(0);
  /* purecov: end */

  return NULL;;
}

/**
  Create file to store pid number.
*/
static void create_pid_file()
{
  int file;
  char buff[1024];

  assert(pidfile_name[0]);
  if ((file = open(pidfile_name, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU|S_IRGRP|S_IROTH)) > 0)
  {
    int length;

    length= snprintf(buff, 1024, "%ld\n", (long) getpid()); 

    if ((write(file, buff, length)) == length)
    {
      if (close(file) != -1)
        return;
    }
    (void)close(file); /* We can ignore the error, since we are going to error anyway at this point */
  }
  snprintf(buff, 1024, "Can't start server: can't create PID file (%s)", pidfile_name);
  sql_perror(buff);
  exit(1);
}


/** This threads handles all signals and alarms. */
/* ARGSUSED */
extern "C"
pthread_handler_t signal_hand(void *)
{
  sigset_t set;
  int sig;
  my_thread_init();				// Init new thread
  signal_thread_in_use= true;

  if (thd_lib_detected != THD_LIB_LT && (test_flags & TEST_SIGINT))
  {
    (void) sigemptyset(&set);			// Setup up SIGINT for debug
    (void) sigaddset(&set,SIGINT);		// For debugging
    (void) pthread_sigmask(SIG_UNBLOCK,&set,NULL);
  }
  (void) sigemptyset(&set);			// Setup up SIGINT for debug
#ifndef IGNORE_SIGHUP_SIGQUIT
  (void) sigaddset(&set,SIGQUIT);
  (void) sigaddset(&set,SIGHUP);
#endif
  (void) sigaddset(&set,SIGTERM);
  (void) sigaddset(&set,SIGTSTP);

  /* Save pid to this process (or thread on Linux) */
  create_pid_file();

#ifdef HAVE_STACK_TRACE_ON_SEGV
  if (opt_do_pstack)
  {
    sprintf(pstack_file_name,"drizzled-%lu-%%d-%%d.backtrace", (uint32_t)getpid());
    pstack_install_segv_action(pstack_file_name);
  }
#endif /* HAVE_STACK_TRACE_ON_SEGV */

  /*
    signal to init that we are ready
    This works by waiting for init to free mutex,
    after which we signal it that we are ready.
    At this pointer there is no other threads running, so there
    should not be any other pthread_cond_signal() calls.

    We call lock/unlock to out wait any thread/session which is
    dieing. Since only comes from this code, this should be safe.
    (Asked MontyW over the phone about this.) -Brian

  */
  if (pthread_mutex_lock(&LOCK_thread_count) == 0)
    (void) pthread_mutex_unlock(&LOCK_thread_count);
  (void) pthread_cond_broadcast(&COND_thread_count);

  (void) pthread_sigmask(SIG_BLOCK,&set,NULL);
  for (;;)
  {
    int error;					// Used when debugging
    if (shutdown_in_progress && !abort_loop)
    {
      sig= SIGTERM;
      error=0;
    }
    else
      while ((error= sigwait(&set,&sig)) == EINTR) ;
    if (cleanup_done)
    {
      my_thread_end();
      signal_thread_in_use= false;

      return NULL;
    }
    switch (sig) {
    case SIGTERM:
    case SIGQUIT:
    case SIGKILL:
      /* switch to the old log message processing */
      if (!abort_loop)
      {
        abort_loop=1;				// mark abort for threads
        kill_server((void*) sig);	// MIT THREAD has a alarm thread
      }
      break;
    case SIGHUP:
      if (!abort_loop)
        reload_cache(NULL, (REFRESH_LOG | REFRESH_TABLES | REFRESH_FAST ), NULL); // Flush logs
      break;
    default:
      break;					/* purecov: tested */
    }
  }
}


static int init(PluginRegistry&)
{
  int error;
  pthread_attr_t thr_attr;
  size_t my_thread_stack_size= 65536;

  (void) pthread_attr_init(&thr_attr);
  pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
  (void) pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);
  {
    struct sched_param tmp_sched_param;

    memset(&tmp_sched_param, 0, sizeof(tmp_sched_param));
    tmp_sched_param.sched_priority= INTERRUPT_PRIOR;
    (void)pthread_attr_setschedparam(&thr_attr, &tmp_sched_param);
  }
#if defined(__ia64__) || defined(__ia64)
  /*
    Peculiar things with ia64 platforms - it seems we only have half the
    stack size in reality, so we have to double it here
  */
  pthread_attr_setstacksize(&thr_attr, my_thread_stack_size*2);
# else
  pthread_attr_setstacksize(&thr_attr, my_thread_stack_size);
#endif

  (void) pthread_mutex_lock(&LOCK_thread_count);
  if ((error=pthread_create(&signal_thread, &thr_attr, signal_hand, 0)))
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Can't create interrupt-thread (error %d, errno: %d)"),
                    error,errno);
    exit(1);
  }
  (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  pthread_mutex_unlock(&LOCK_thread_count);

  (void) pthread_attr_destroy(&thr_attr);

  return 0;
}

/**
  This is mainly needed when running with purify, but it's still nice to
  know that all child threads have died when drizzled exits.
*/
static int deinit(PluginRegistry&)
{
  uint32_t i;
  /*
    Wait up to 10 seconds for signal thread to die. We use this mainly to
    avoid getting warnings that my_thread_end has not been called
  */
  for (i= 0 ; i < 100 && signal_thread_in_use; i++)
  {
    if (pthread_kill(signal_thread, SIGTERM) != ESRCH)
      break;
    usleep(100);				// Give it time to die
  }

  return 0;
}

static struct st_mysql_sys_var* system_variables[]= {
  NULL
};

drizzle_declare_plugin(signal_handler)
{
  "signal_handler",
  "0.1",
  "Brian Aker",
  "Default Signal Handler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  system_variables,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
