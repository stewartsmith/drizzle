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

#include <config.h>

#include <iostream>

#include <drizzled/pthread_globals.h>
#include <drizzled/module/option_map.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/session.h>
#include <drizzled/session/cache.h>
#include <drizzled/abort_exception.h>
#include <drizzled/transaction_services.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin.h>
#include <drizzled/statistics_variables.h>

#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>

#include "multi_thread.h"

namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

/* Configuration variables. */
typedef constrained_check<uint32_t, 4096, 1> max_threads_constraint;
static max_threads_constraint max_threads;

namespace drizzled
{
  extern size_t my_thread_stack_size;
}

namespace multi_thread {

void MultiThreadScheduler::runSession(drizzled::session_id_t id)
{
  char stack_dummy;
  boost::this_thread::disable_interruption disable_by_default;

  Session::shared_ptr session(session::Cache::find(id));

  try
  {

    if (not session)
    {
      std::cerr << _("Session killed before thread could execute") << endl;
      return;
    }
    session->pushInterrupt(&disable_by_default);
    drizzled::internal::my_thread_init();
    session->thread_stack= (char*) &stack_dummy;
    session->run();

    killSessionNow(session);
  }
  catch (abort_exception& ex)
  {
    cout << _("Drizzle has receieved an abort event.") << endl;
    cout << _("In Function: ") << *::boost::get_error_info<boost::throw_function>(ex) << endl;
    cout << _("In File: ") << *::boost::get_error_info<boost::throw_file>(ex) << endl;
    cout << _("On Line: ") << *::boost::get_error_info<boost::throw_line>(ex) << endl;

    TransactionServices::sendShutdownEvent(*session.get());
  }
  // @todo remove hard spin by disconnection the session first from the
  // thread.
  while (not session.unique()) {}
}

void MultiThreadScheduler::setStackSize()
{
  pthread_attr_t attr;

  (void) pthread_attr_init(&attr);

  /* Get the thread stack size that the OS will use and make sure
    that we update our global variable. */
  int err= pthread_attr_getstacksize(&attr, &my_thread_stack_size);
  pthread_attr_destroy(&attr);

  if (err != 0)
  {
    errmsg_printf(error::ERROR, _("Unable to get thread stack size"));
    my_thread_stack_size= 524288; // At the time of the writing of this code, this was OSX's
  }

  if (my_thread_stack_size == 0)
  {
    my_thread_stack_size= 524288; // At the time of the writing of this code, this was OSX's
  }
#ifdef __sun
  /*
   * Solaris will return zero for the stack size in a call to
   * pthread_attr_getstacksize() to indicate that the OS default stack
   * size is used. We need an actual value in my_thread_stack_size so that
   * check_stack_overrun() will work. The Solaris man page for the
   * pthread_attr_getstacksize() function says that 2M is used for 64-bit
   * processes. We'll explicitly set it here to make sure that is what
   * will be used.
   */
  if (my_thread_stack_size == 0)
  {
    my_thread_stack_size= 2 * 1024 * 1024;
  }
#endif
}

bool MultiThreadScheduler::addSession(const Session::shared_ptr& session)
{
  if (thread_count >= max_threads)
    return true;

  thread_count.increment();
  try
  {
    session->getThread().reset(new boost::thread((boost::bind(&MultiThreadScheduler::runSession, this, session->getSessionId()))));
  }
  catch (std::exception&)
  {
    thread_count.decrement();
    return true;
  }

  if (not session->getThread())
  {
    thread_count.decrement();
    return true;
  }

  if (not session->getThread()->joinable())
  {
    thread_count.decrement();
    return true;
  }

  return false;
}


void MultiThreadScheduler::killSession(Session *session)
{
  thread_ptr thread(session->getThread());

  if (thread)
  {
    thread->interrupt();
  }
}

void MultiThreadScheduler::killSessionNow(const Session::shared_ptr& session)
{
  killSession(session.get());

  session->disconnect();

  /* Locks LOCK_thread_count and deletes session */
  Session::unlink(session);
  thread_count.decrement();
}

MultiThreadScheduler::~MultiThreadScheduler()
{
  boost::mutex::scoped_lock scopedLock(drizzled::session::Cache::mutex());
  while (thread_count)
  {
    COND_thread_count.wait(scopedLock);
  }
}

} // multi_thread namespace

  
static int init(drizzled::module::Context &context)
{
  
  context.add(new multi_thread::MultiThreadScheduler("multi_thread"));

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("max-threads",
          po::value<max_threads_constraint>(&max_threads)->default_value(2048),
          _("Maximum number of user threads available."));
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "multi_thread",
  "0.1",
  "Brian Aker",
  "One Thread Per Session Scheduler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  NULL,   /* depends */
  init_options    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
