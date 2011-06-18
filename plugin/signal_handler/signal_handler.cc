/* 
   Copyright (C) 2011 Brian Aker
   Copyright (C) 2006 MySQL AB

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

#include <config.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/internal/my_pthread.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/plugin/daemon.h>
#include <drizzled/signal_handler.h>
#include <drizzled/session.h>
#include <drizzled/session/cache.h>
#include <drizzled/debug.h>
#include <drizzled/drizzled.h>
#include <drizzled/open_tables_state.h>

#include <boost/thread/thread.hpp>
#include <boost/filesystem.hpp>

#include <sys/stat.h>
#include <fcntl.h>


static bool kill_in_progress= false;
void signal_hand(void);

namespace drizzled
{
extern int cleanup_done;
extern bool volatile abort_loop;
extern bool volatile shutdown_in_progress;
extern boost::filesystem::path pid_file;
/* Prototypes -> all of these should be factored out into a propper shutdown */
extern void close_connections(void);
}

using namespace drizzled;




/**
  Force server down. Kill all connections and threads and exit.

  @param  sig_ptr       Signal number that caused kill_server to be called.

  @note
    A signal number of 0 mean that the function was not called
    from a signal handler and there is thus no signal to block
    or stop, we just want to kill the server.
*/

static void kill_server(int sig)
{
  // if there is a signal during the kill in progress, ignore the other
  if (kill_in_progress)				// Safety
    return;
  kill_in_progress=true;
  abort_loop=1;					// This should be set
  if (sig != 0) // 0 is not a valid signal number
    ignore_signal(sig);                    /* purify inspected */
  if (sig == SIGTERM || sig == 0)
    errmsg_printf(error::INFO, _(ER(ER_NORMAL_SHUTDOWN)),internal::my_progname);
  else
    errmsg_printf(error::ERROR, _(ER(ER_GOT_SIGNAL)),internal::my_progname,sig);

  close_connections();
  clean_up(1);
}

/**
  Create file to store pid number.
*/
static void create_pid_file()
{
  int file;
  char buff[1024];

  if ((file = open(pid_file.file_string().c_str(), O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU|S_IRGRP|S_IROTH)) > 0)
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
  memset(buff, 0, sizeof(buff));
  snprintf(buff, sizeof(buff)-1, "Can't start server: can't create PID file (%s)", pid_file.file_string().c_str());
  sql_perror(buff);
  exit(1);
}


/** This threads handles all signals and alarms. */
void signal_hand()
{
  sigset_t set;
  int sig;
  internal::my_thread_init();				// Init new thread
  signal_thread_in_use= true;

  if ((drizzled::getDebug().test(drizzled::debug::ALLOW_SIGINT)))
  {
    (void) sigemptyset(&set);			// Setup up SIGINT for debug
    (void) sigaddset(&set,SIGINT);		// For debugging
    (void) pthread_sigmask(SIG_UNBLOCK, &set, NULL);
  }
  (void) sigemptyset(&set);			// Setup up SIGINT for debug
#ifndef IGNORE_SIGHUP_SIGQUIT
  if (sigaddset(&set,SIGQUIT))
  {
    std::cerr << "failed setting sigaddset() with SIGQUIT\n";
  }
  if (sigaddset(&set,SIGHUP))
  {
    std::cerr << "failed setting sigaddset() with SIGHUP\n";
  }
#endif
  if (sigaddset(&set,SIGTERM))
  {
    std::cerr << "failed setting sigaddset() with SIGTERM\n";
  }
  if (sigaddset(&set,SIGTSTP))
  {
    std::cerr << "failed setting sigaddset() with SIGTSTP\n";
  }

  /* Save pid to this process (or thread on Linux) */
  create_pid_file();

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
  session::Cache::mutex().lock();
  session::Cache::mutex().unlock();
  COND_thread_count.notify_all();

  if (pthread_sigmask(SIG_BLOCK, &set, NULL))
  {
    std::cerr << "Failed to set pthread_sigmask() in signal handler\n";
  }

  for (;;)
  {
    int error;					// Used when debugging

    if (shutdown_in_progress && !abort_loop)
    {
      sig= SIGTERM;
      error=0;
    }
    else
    {
      while ((error= sigwait(&set, &sig)) == EINTR) ;
    }

    if (cleanup_done)
    {
      signal_thread_in_use= false;

      return;
    }
    switch (sig) {
    case SIGTERM:
    case SIGQUIT:
    case SIGKILL:
    case SIGTSTP:
      /* switch to the old log message processing */
      if (!abort_loop)
      {
        abort_loop=1;				// mark abort for threads
        kill_server(sig);		// MIT THREAD has a alarm thread
      }
      break;
    case SIGHUP:
      if (!abort_loop)
      {
        g_refresh_version++;
        drizzled::plugin::StorageEngine::flushLogs(NULL);
      }
      break;
    default:
      break;
    }
  }
}

class SignalHandler :
  public drizzled::plugin::Daemon
{
  SignalHandler(const SignalHandler &);
  SignalHandler& operator=(const SignalHandler &);
  boost::thread thread;

public:
  SignalHandler() :
    drizzled::plugin::Daemon("Signal Handler")
  {
    // @todo fix spurious wakeup issue
    boost::mutex::scoped_lock scopedLock(session::Cache::mutex());
    thread= boost::thread(signal_hand);
    signal_thread= thread.native_handle();
    COND_thread_count.wait(scopedLock);
  }

  /**
    This is mainly needed when running with purify, but it's still nice to
    know that all child threads have died when drizzled exits.
  */
  ~SignalHandler()
  {
    /*
      Wait up to 100000 micro-seconds for signal thread to die. We use this mainly to
      avoid getting warnings that internal::my_thread_end has not been called
    */
    bool completed= false;
    /*
     * We send SIGTERM and then do a timed join. If that fails we will on
     * the last pthread_kill() call SIGTSTP. OSX (and FreeBSD) seem to
     * prefer this. -Brian
   */
    uint32_t count= 2; // How many times to try join and see if the caller died.
    while (not completed and count--)
    {
      int error;
      int signal= count == 1 ? SIGTSTP : SIGTERM;
      
      if ((error= pthread_kill(thread.native_handle(), signal)))
      {
        char buffer[1024]; // No reason for number;
        strerror_r(error, buffer, sizeof(buffer));
        std::cerr << "pthread_kill() error on shutdown of signal thread (" << buffer << ")\n";
        break;
      }
      else
      {
        boost::posix_time::milliseconds duration(100);
        completed= thread.timed_join(duration);
      }
    }
  }
};

static int init(drizzled::module::Context& context)
{
  context.add(new SignalHandler);

  return 0;
}


DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "signal_handler",
  "0.1",
  "Brian Aker",
  "Default Signal Handler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
