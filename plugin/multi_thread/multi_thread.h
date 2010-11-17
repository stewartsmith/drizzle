/* Copyright (C) 2009 Sun Microsystems

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

#ifndef PLUGIN_MULTI_THREAD_MULTI_THREAD_H
#define PLUGIN_MULTI_THREAD_MULTI_THREAD_H

#include <drizzled/atomics.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin/scheduler.h>
#include "drizzled/internal/my_sys.h"
#include <drizzled/sql_parse.h>
#include <string>

namespace drizzled {
class Session;
}

class MultiThreadScheduler: public drizzled::plugin::Scheduler
{
private:
  drizzled::atomic<uint32_t> thread_count;

public:
  MultiThreadScheduler(const char *name_arg): 
    Scheduler(name_arg)
  {
    setStackSize();
    thread_count= 0;
  }

  ~MultiThreadScheduler();
  bool addSession(drizzled::Session::shared_ptr &session);
  void killSessionNow(drizzled::Session::shared_ptr &session);
  
  void runSession(drizzled::session_id_t);
private:
  void setStackSize();
};

#endif /* PLUGIN_MULTI_THREAD_MULTI_THREAD_H */
