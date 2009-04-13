/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Mark Atwood
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

#include <drizzled/server_includes.h>
#include <drizzled/logging.h>
#include <drizzled/gettext.h>
#include "drizzled/plugin_registry.h"

#include <vector>

using namespace std;

static vector<Logging_handler *> all_loggers;

void add_logger(Logging_handler *handler)
{
  if (handler != NULL)
    all_loggers.push_back(handler);
}

void remove_logger(Logging_handler *handler)
{
  if (handler != NULL)
    all_loggers.erase(find(all_loggers.begin(), all_loggers.end(), handler));
}


class LoggingPreIterate : public unary_function<Logging_handler *, bool>
{
  Session *session;
public:
  LoggingPreIterate(Session *session_arg) :
    unary_function<Logging_handler *, bool>(),
    session(session_arg) {}

  inline result_type operator()(argument_type handler)
  {
    if (handler->pre(session))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("logging '%s' pre() failed"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};


class LoggingPostIterate : public unary_function<Logging_handler *, bool>
{
  Session *session;
public:
  LoggingPostIterate(Session *session_arg) :
    unary_function<Logging_handler *, bool>(),
    session(session_arg) {}

  /* This gets called once for each loaded logging plugin */
  inline result_type operator()(argument_type handler)
  {
    if (handler->post(session))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("logging '%s' post() failed"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};


/* This is the logging_pre_do entry point.
   This gets called by the rest of the Drizzle server code */
bool logging_pre_do (Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<Logging_handler *>::iterator iter=
    find_if(all_loggers.begin(), all_loggers.end(),
            LoggingPreIterate(session)); 
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_loggers.end();
}

/* This is the logging_post_do entry point.
   This gets called by the rest of the Drizzle server code */
bool logging_post_do (Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<Logging_handler *>::iterator iter=
    find_if(all_loggers.begin(), all_loggers.end(),
            LoggingPreIterate(session)); 
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_loggers.end();
}
