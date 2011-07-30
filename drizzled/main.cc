/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>

#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


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

#if defined(HAVE_LOCALE_H)
# include <locale.h>
#endif

#include <boost/filesystem.hpp>

#include <drizzled/abort_exception.h>
#include <drizzled/catalog/local.h>
#include <drizzled/configmake.h>
#include <drizzled/data_home.h>
#include <drizzled/debug.h>
#include <drizzled/drizzled.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/plugin.h>
#include <drizzled/plugin/client.h>
#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/monitored_in_transaction.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/replication_services.h>
#include <drizzled/session.h>
#include <drizzled/session/cache.h>
#include <drizzled/signal_handler.h>
#include <drizzled/transaction_services.h>
#include <drizzled/unireg.h>
#include <drizzled/util/backtrace.h>
#include <drizzled/current_session.h>
#include <drizzled/daemon.h>
#include <drizzled/diagnostics_area.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_lex.h>
#include <drizzled/system_variables.h>

using namespace drizzled;
using namespace std;

static pthread_t select_thread;
static uint32_t thr_kill_signal;

extern bool opt_daemon;


/**
  All global error messages are sent here where the first one is stored
  for the client.
*/
static void my_message_sql(drizzled::error_t error, const char *str, myf MyFlags)
{
  Session* session= current_session;
  /*
    Put here following assertion when situation with EE_* error codes
    will be fixed
  */
  if (session)
  {
    if (MyFlags & ME_FATALERROR)
      session->is_fatal_error= 1;

    /*
      @TODO There are two exceptions mechanism (Session and sp_rcontext),
      this could be improved by having a common stack of handlers.

    if (session->handle_error(error, str, DRIZZLE_ERROR::WARN_LEVEL_ERROR))
      return;
    */

    /*
      session->lex().current_select == 0 if lex structure is not inited
      (not query command (COM_QUERY))
    */
    if (! (session->lex().current_select &&
           session->lex().current_select->no_error && !session->is_fatal_error))
    {
      if (! session->main_da().is_error())            // Return only first message
      {
        if (error == EE_OK)
          error= ER_UNKNOWN_ERROR;

        if (str == NULL)
          str= ER(error);

        session->main_da().set_error_status(error, str);
      }
    }

    if (!session->no_warnings_for_error && !session->is_fatal_error)
    {
      /*
        Suppress infinite recursion if there a memory allocation error
        inside push_warning.
      */
      session->no_warnings_for_error= true;
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR, error, str);
      session->no_warnings_for_error= false;
    }
  }

  if (not session || MyFlags & ME_NOREFRESH)
  {
    errmsg_printf(error::ERROR, "%s: %s",internal::my_progname,str);
  }
}

static void init_signals(void)
{
  sigset_t set;
  struct sigaction sa;

  if (not (getDebug().test(debug::NO_STACKTRACE) ||
        getDebug().test(debug::CORE_ON_SIGNAL)))
  {
    sa.sa_flags = SA_RESETHAND | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigprocmask(SIG_SETMASK,&sa.sa_mask,NULL);

    sa.sa_handler= drizzled_handle_segfault;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
#ifdef SIGBUS
    sigaction(SIGBUS, &sa, NULL);
#endif
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
  }

  if (getDebug().test(debug::CORE_ON_SIGNAL))
  {
    /* Change limits so that we will get a core file */
    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl) && global_system_variables.log_warnings)
        errmsg_printf(error::WARN,
                      _("setrlimit could not change the size of core files "
                        "to 'infinity';  We may not be able to generate a "
                        "core file on signals"));
  }
  (void) sigemptyset(&set);
  ignore_signal(SIGPIPE);
  sigaddset(&set,SIGPIPE);
#ifndef IGNORE_SIGHUP_SIGQUIT
  sigaddset(&set,SIGQUIT);
  sigaddset(&set,SIGHUP);
#endif
  sigaddset(&set,SIGTERM);

  /* Fix signals if blocked by parents (can happen on Mac OS X) */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = drizzled_print_signal_warning;
  sigaction(SIGTERM, &sa, NULL);
  sa.sa_flags = 0;
  sa.sa_handler = drizzled_print_signal_warning;
  sigaction(SIGHUP, &sa, NULL);
#ifdef SIGTSTP
  sigaddset(&set,SIGTSTP);
#endif
  if (getDebug().test(debug::ALLOW_SIGINT))
  {
    sa.sa_flags= 0;
    sa.sa_handler= drizzled_end_thread_signal;
    sigaction(thr_kill_signal, &sa, NULL);

    // May be SIGINT
    sigdelset(&set, thr_kill_signal);
  }
  else
  {
    sigaddset(&set,SIGINT);
  }
  sigprocmask(SIG_SETMASK,&set,NULL);
  pthread_sigmask(SIG_SETMASK,&set,NULL);
  return;
}

static void GoogleProtoErrorThrower(google::protobuf::LogLevel level,
                                    const char* ,
                                    int, const string& ) throw(const char *)
{
  switch(level)
  {
  case google::protobuf::LOGLEVEL_INFO:
    break;
  case google::protobuf::LOGLEVEL_WARNING:
  case google::protobuf::LOGLEVEL_ERROR:
  case google::protobuf::LOGLEVEL_FATAL:
  default:
    throw("error in google protocol buffer parsing");
  }
}

int main(int argc, char **argv)
{
#if defined(ENABLE_NLS)
# if defined(HAVE_LOCALE_H)
  setlocale(LC_ALL, "");
# endif
  bindtextdomain("drizzle7", LOCALEDIR);
  textdomain("drizzle7");
#endif

  module::Registry &modules= module::Registry::singleton();

  MY_INIT(argv[0]);		// init my_sys library & pthreads
  /* nothing should come before this line ^^^ */

  /* Set signal used to kill Drizzle */
  thr_kill_signal= SIGINT;

  google::protobuf::SetLogHandler(&GoogleProtoErrorThrower);

  /* Function generates error messages before abort */
  error_handler_hook= my_message_sql;

  /* init_common_variables must get basic settings such as data_home_dir
     and plugin_load_list. */
  if (init_basic_variables(argc, argv))
    unireg_abort(1);				// Will do exit

  if (opt_daemon)
  {
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
      perror("Failed to ignore SIGHUP");
    }
    if (daemonize())
    {
      fprintf(stderr, "failed to daemon() in order to daemonize\n");
      exit(EXIT_FAILURE);
    }
  }

  if (init_remaining_variables(modules))
    unireg_abort(1);				// Will do exit

  /*
    init signals & alarm
    After this we can't quit by a simple unireg_abort
  */
  init_signals();


  select_thread=pthread_self();
  select_thread_in_use=1;

  if (not opt_help)
  {
    if (chdir(getDataHome().file_string().c_str()))
    {
      errmsg_printf(error::ERROR,
                    _("Data directory %s does not exist\n"),
                    getDataHome().file_string().c_str());
      unireg_abort(1);
    }

    ifstream old_uuid_file ("server.uuid");
    if (old_uuid_file.is_open())
    {
      getline (old_uuid_file, server_uuid);
      old_uuid_file.close();
    } 
    else 
    {
      uuid_t uu;
      char uuid_string[37];
      uuid_generate_random(uu);
      uuid_unparse(uu, uuid_string);
      ofstream new_uuid_file ("server.uuid");
      new_uuid_file << uuid_string;
      new_uuid_file.close();
      server_uuid= string(uuid_string);
    }

    if (mkdir("local", 0700))
    {
      /* We don't actually care */
    }
    if (chdir("local"))
    {
      errmsg_printf(error::ERROR,
                    _("Local catalog %s/local does not exist\n"),
                    getDataHome().file_string().c_str());
      unireg_abort(1);
    }

    setFullDataHome(boost::filesystem::system_complete(getDataHome()));
    errmsg_printf(error::INFO, "Data Home directory is : %s", getFullDataHome().native_file_string().c_str());
  }

  if (server_id == 0)
  {
    server_id= 1;
  }

  try
  {
    init_server_components(modules);
  }
  catch (abort_exception& ex)
  {
#if defined(DEBUG)
    cout << _("Drizzle has receieved an abort event.") << endl;
    cout << _("In Function: ") << *::boost::get_error_info<boost::throw_function>(ex) << endl;
    cout << _("In File: ") << *::boost::get_error_info<boost::throw_file>(ex) << endl;
    cout << _("On Line: ") << *::boost::get_error_info<boost::throw_line>(ex) << endl;
#endif
    unireg_abort(1);
  }


  /**
   * This check must be done after init_server_components for now
   * because we don't yet have plugin dependency tracking...
   *
   * ReplicationServices::evaluateRegisteredPlugins() will print error messages to stderr
   * via errmsg_printf().
   *
   * @todo
   *
   * not checking return since unireg_abort() hangs
   */
  (void) ReplicationServices::evaluateRegisteredPlugins();

  if (plugin::Listen::setup())
    unireg_abort(1);

  assert(plugin::num_trx_monitored_objects > 0);
  drizzle_rm_tmp_tables();
  errmsg_printf(error::INFO, _(ER(ER_STARTUP)), internal::my_progname, PANDORA_RELEASE_VERSION, COMPILATION_COMMENT);

  /* Send server startup event */
  {
    Session::shared_ptr session= Session::make_shared(plugin::Listen::getNullClient(), catalog::local());
    setCurrentSession(session.get());
    TransactionServices::sendStartupEvent(*session);
    plugin_startup_window(modules, *session.get());
  }

  if (opt_daemon)
    daemon_is_ready();

  /*
    Listen for new connections and start new session for each connection
     accepted. The listen.getClient() method will return NULL when the server
     should be shutdown.
   */
  while (plugin::Client* client= plugin::Listen::getClient())
  {
    Session::shared_ptr session= Session::make_shared(client, client->catalog());

    /* If we error on creation we drop the connection and delete the session. */
    if (Session::schedule(session))
      Session::unlink(session);
  }

  /* Send server shutdown event */
  {
    Session::shared_ptr session= Session::make_shared(plugin::Listen::getNullClient(), catalog::local());
    setCurrentSession(session.get());
    TransactionServices::sendShutdownEvent(*session.get());
  }

  {
    boost::mutex::scoped_lock scopedLock(session::Cache::mutex());
    select_thread_in_use= false;			// For close_connections
  }
  COND_thread_count.notify_all();

  /* Wait until cleanup is done */
  session::Cache::shutdownSecond();

  clean_up(1);
  module::Registry::shutdown();
  internal::my_end();

  return 0;
}

