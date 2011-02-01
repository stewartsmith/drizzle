/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

#include <cstdio>  /* for vsnprintf */
#include <stdarg.h>  /* for va_list */
#include <unistd.h>  /* for write(2) */
#include <iostream>
#include <libnotifymm.h>
#include <boost/scoped_ptr.hpp>

#include <string>
#include <vector>

#include <drizzled/plugin/error_message.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin.h>


/* todo, make this dynamic as needed */
#define MAX_MSG_LEN 8192

using namespace drizzled;

static bool local_notify(Notify::Notification &_notify)
{
  /**
   * @TODO: Make this timeout a system variable
 */
  _notify.set_timeout(3000);

#ifdef GLIBMM_EXCEPTIONS_ENABLED
  try
  {
    if (not _notify.show())
#else
      boost::scoped_ptr<Glib::Error> error;
    if (not _notify.show(error))
#endif
    {
      std::cerr << _("Failed to send error message to libnotify") << std::endl;
      return true;
    }
#ifdef GLIBMM_EXCEPTIONS_ENABLED
  }
  catch (Glib::Error& err)
  {
    std::cerr << err.what() << std::endl;
  }
#endif

  return false;
}

class Error_message_notify : public plugin::ErrorMessage
{
  std::vector<std::string> errmsg_tags;

public:
  Error_message_notify()
   : plugin::ErrorMessage("Error_message_notify"),
     errmsg_tags()
  {
    errmsg_tags.push_back("Unknown");
    errmsg_tags.push_back("Debug");
    errmsg_tags.push_back("Info");
    errmsg_tags.push_back("Warn");
    errmsg_tags.push_back("Error");
  }

  virtual bool errmsg(error::level_t priority, const char *format, va_list ap)
  {
    char msgbuf[MAX_MSG_LEN];
    int prv;

    prv= vsnprintf(msgbuf, MAX_MSG_LEN, format, ap);
    if (prv < 0)
      return true;

    switch (priority)
    {
    case error::INFO:
      Notify::Notification n("Info", msgbuf);
      return local_notify(n);

    case error::INSPECT:
      Notify::Notification n("Debug", msgbuf);
      return local_notify(n);

    case error::WARN:
      Notify::Notification n("Warn", msgbuf);
      return local_notify(n);

    case error::ERROR:
      Notify::Notification n("Error", msgbuf);
      return local_notify(n);
    }

  return false;
  
  }
};

static int plugin_init(module::Context &context)
{
  Notify::init("Drizzled");
  context.add(new Error_message_notify());

  return 0;
}

DRIZZLE_PLUGIN(plugin_init, NULL, NULL);
