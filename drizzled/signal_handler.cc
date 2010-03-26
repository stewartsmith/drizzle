/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include "config.h"

#include <signal.h>

#include "drizzled/signal_handler.h"
#include "drizzled/drizzled.h"
#include "drizzled/session.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/probes.h"
#include "drizzled/plugin.h"
#include "drizzled/plugin/scheduler.h"
#include "plugin/myisam/keycache.h"

using namespace drizzled;

static uint32_t killed_threads;
static bool segfaulted= false;

/*
 * We declare these extern "C" because they are passed to system callback functions
 * and Sun Studio does not like it when those don't have C linkage. We prefix them
 * because extern "C"-ing something effectively removes the namespace from the
 * linker symbols, meaning they would be exporting symbols like "print_signal_warning
 */
extern "C"
{

void drizzled_print_signal_warning(int sig)
{
  if (global_system_variables.log_warnings)
    errmsg_printf(ERRMSG_LVL_WARN, _("Got signal %d from thread %"PRIu64),
                  sig, global_thread_id);
#ifndef HAVE_BSD_SIGNALS
  set_signal(sig,drizzled_print_signal_warning);		/* int. thread system calls */
#endif
  if (sig == SIGALRM)
    alarm(2);					/* reschedule alarm */
}

/** Called when a thread is aborted. */
void drizzled_end_thread_signal(int )
{
  Session *session=current_session;
  if (session)
  {
    statistic_increment(killed_threads, &LOCK_status);
    session->scheduler->killSessionNow(session);
    DRIZZLE_CONNECTION_DONE(session->thread_id);
  }
  return;
}

void drizzled_handle_segfault(int sig)
{
  time_t curr_time;
  struct tm tm;

  /*
    Strictly speaking, one needs a mutex here
    but since we have got SIGSEGV already, things are a mess
    so not having the mutex is not as bad as possibly using a buggy
    mutex - so we keep things simple
  */
  if (segfaulted)
  {
    fprintf(stderr, _("Fatal signal %d while backtracing\n"), sig);
    exit(1);
  }

  segfaulted= true;

  curr_time= time(NULL);
  if(curr_time == (time_t)-1)
  {
    fprintf(stderr, _("Fatal: time() call failed\n"));
    exit(1);
  }

  localtime_r(&curr_time, &tm);
  
  fprintf(stderr,_("%02d%02d%02d %2d:%02d:%02d - drizzled got signal %d;\n"
          "This could be because you hit a bug. It is also possible that "
          "this binary\n or one of the libraries it was linked against is "
          "corrupt, improperly built,\n or misconfigured. This error can "
          "also be caused by malfunctioning hardware.\n"),
          tm.tm_year % 100, tm.tm_mon+1, tm.tm_mday,
          tm.tm_hour, tm.tm_min, tm.tm_sec,
          sig);
  fprintf(stderr, _("We will try our best to scrape up some info that "
                    "will hopefully help diagnose\n"
                    "the problem, but since we have already crashed, "
                    "something is definitely wrong\nand this may fail.\n\n"));
  fprintf(stderr, "key_buffer_size=%u\n",
          (uint32_t) dflt_key_cache->key_cache_mem_size);
  fprintf(stderr, "read_buffer_size=%ld\n", (long) global_system_variables.read_buff_size);
  fprintf(stderr, "max_used_connections=%u\n", max_used_connections);
  fprintf(stderr, "connection_count=%u\n", uint32_t(connection_count));
  fprintf(stderr, _("It is possible that drizzled could use up to \n"
                    "key_buffer_size + (read_buffer_size + "
                    "sort_buffer_size)*thread_count\n"
                    "bytes of memory\n"
                    "Hope that's ok; if not, decrease some variables in the "
                    "equation.\n\n"));

#ifdef HAVE_STACKTRACE
  Session *session= current_session;

  if (! (test_flags.test(TEST_NO_STACKTRACE)))
  {
    fprintf(stderr,"session: 0x%lx\n",(long) session);
    fprintf(stderr,_("Attempting backtrace. You can use the following "
                     "information to find out\n"
                     "where drizzled died. If you see no messages after this, "
                     "something went\n"
                     "terribly wrong...\n"));
    print_stacktrace(session ? (unsigned char*) session->thread_stack : (unsigned char*) 0,
                     my_thread_stack_size);
  }
  if (session)
  {
    const char *kreason= "UNKNOWN";
    switch (session->killed) {
    case Session::NOT_KILLED:
      kreason= "NOT_KILLED";
      break;
    case Session::KILL_BAD_DATA:
      kreason= "KILL_BAD_DATA";
      break;
    case Session::KILL_CONNECTION:
      kreason= "KILL_CONNECTION";
      break;
    case Session::KILL_QUERY:
      kreason= "KILL_QUERY";
      break;
    case Session::KILLED_NO_VALUE:
      kreason= "KILLED_NO_VALUE";
      break;
    }
    fprintf(stderr, _("Trying to get some variables.\n"
                      "Some pointers may be invalid and cause the "
                      "dump to abort...\n"));
    safe_print_str("session->query", session->query, 1024);
    fprintf(stderr, "session->thread_id=%"PRIu32"\n", (uint32_t) session->thread_id);
    fprintf(stderr, "session->killed=%s\n", kreason);
  }
  fflush(stderr);
#endif /* HAVE_STACKTRACE */

  if (calling_initgroups)
    fprintf(stderr, _("\nThis crash occurred while the server was calling "
                      "initgroups(). This is\n"
                      "often due to the use of a drizzled that is statically "
                      "linked against glibc\n"
                      "and configured to use LDAP in /etc/nsswitch.conf. "
                      "You will need to either\n"
                      "upgrade to a version of glibc that does not have this "
                      "problem (2.3.4 or\n"
                      "later when used with nscd), disable LDAP in your "
                      "nsswitch.conf, or use a\n"
                      "drizzled that is not statically linked.\n"));

  if (internal::thd_lib_detected == THD_LIB_LT && !getenv("LD_ASSUME_KERNEL"))
    fprintf(stderr,
            _("\nYou are running a statically-linked LinuxThreads binary "
              "on an NPTL system.\n"
              "This can result in crashes on some distributions due "
              "to LT/NPTL conflicts.\n"
              "You should either build a dynamically-linked binary, or force "
              "LinuxThreads\n"
              "to be used with the LD_ASSUME_KERNEL environment variable. "
              "Please consult\n"
              "the documentation for your distribution on how to do that.\n"));

#ifdef HAVE_WRITE_CORE
  if (test_flags.test(TEST_CORE_ON_SIGNAL))
  {
    fprintf(stderr, _("Writing a core file\n"));
    fflush(stderr);
    write_core(sig);
  }
#endif

  exit(1);
}

} /* extern "C" */
