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
#include "drizzled/plugin/logging.h"
#include "drizzled/gettext.h"
#include "drizzled/errmsg_print.h"

#include <vector>
#include <algorithm>

class Session;

using namespace std;

namespace drizzled
{

vector<plugin::Logging *> all_loggers;


bool plugin::Logging::addPlugin(plugin::Logging *handler)
{
  if (handler != NULL)
    all_loggers.push_back(handler);
  return false;
}

void plugin::Logging::removePlugin(plugin::Logging *handler)
{
  if (handler != NULL)
    all_loggers.erase(find(all_loggers.begin(), all_loggers.end(), handler));
}


class PreIterate : public unary_function<plugin::Logging *, bool>
{
  Session *session;
public:
  PreIterate(Session *session_arg) :
    unary_function<plugin::Logging *, bool>(),
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


class PostIterate : public unary_function<plugin::Logging *, bool>
{
  Session *session;
public:
  PostIterate(Session *session_arg) :
    unary_function<plugin::Logging *, bool>(),
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

class PostEndIterate : public unary_function<plugin::Logging *, bool>
{
  Session *session;
public:
  PostEndIterate(Session *session_arg) :
    unary_function<plugin::Logging *, bool>(),
    session(session_arg) {}

  /* This gets called once for each loaded logging plugin */
  inline result_type operator()(argument_type handler)
  {
    if (handler->postEnd(session))
    {
      /* TRANSLATORS: The leading word "logging" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("logging '%s' postEnd() failed"),
                    handler->getName().c_str());
      return true;
    }
    return false;
  }
};

/* This is the Logging::preDo entry point.
   This gets called by the rest of the Drizzle server code */
bool plugin::Logging::preDo(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<plugin::Logging *>::iterator iter=
    find_if(all_loggers.begin(), all_loggers.end(),
            PreIterate(session)); 
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_loggers.end();
}

/* This is the Logging::postDo entry point.
   This gets called by the rest of the Drizzle server code */
bool plugin::Logging::postDo(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<plugin::Logging *>::iterator iter=
    find_if(all_loggers.begin(), all_loggers.end(),
            PostIterate(session)); 
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_loggers.end();
}

/* This gets called in the session destructor */
bool plugin::Logging::postEndDo(Session *session)
{
  /* Use find_if instead of foreach so that we can collect return codes */
  vector<plugin::Logging *>::iterator iter=
    find_if(all_loggers.begin(), all_loggers.end(),
            PostEndIterate(session));
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to
   * return false on success, we return the value of the two being !=
   */
  return iter != all_loggers.end();
}

} /* namespace drizzled */
