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
using namespace std;


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

  virtual bool errmsg(Session *, int priority, const char *format, va_list ap)
  {
    char msgbuf[MAX_MSG_LEN];
    int prv;

    prv= vsnprintf(msgbuf, MAX_MSG_LEN, format, ap);
    if (prv < 0) return true;

    Notify::Notification n(errmsg_tags[priority].c_str(), msgbuf);
    /**
     * @TODO: Make this timeout a system variable
     */
    n.set_timeout(3000);

#ifdef GLIBMM_EXCEPTIONS_ENABLED
    try
    {
      if (!n.show())
#else
      boost::scoped_ptr<Glib::Error> error;
      if (!n.show(error))
#endif
      {
        fprintf(stderr, _("Failed to send error message to libnotify\n"));
        return true;
      }
#ifdef GLIBMM_EXCEPTIONS_ENABLED
     }
     catch (Glib::Error& err)
     {
        cerr << err.what() << endl;
     }
#endif

  return false;
  
  }
};

static Error_message_notify *handler= NULL;
static int plugin_init(module::Context &context)
{
  Notify::init("Drizzled");
  handler= new Error_message_notify();
  context.add(handler);

  return 0;
}

DRIZZLE_PLUGIN(plugin_init, NULL, NULL);
