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

#include <drizzled/server_includes.h>
#include <drizzled/errmsg.h>
#include <drizzled/gettext.h>
#include "drizzled/plugin_registry.h"

#include <vector>

using namespace std;

static vector<Error_message_handler *> all_errmsg_handler;

static bool errmsg_has= false;

void add_errmsg_handler(Error_message_handler *handler)
{
  all_errmsg_handler.push_back(handler);
  errmsg_has= true;
}

void remove_errmsg_handler(Error_message_handler *handler)
{
  all_errmsg_handler.erase(find(all_errmsg_handler.begin(),
                                all_errmsg_handler.end(), handler));
}


class ErrorMessagePrint : public unary_function<Error_message_handler *, bool>
{
  Session *session;
  int priority;
  const char *format;
  va_list ap;
public:
  ErrorMessagePrint(Session *session_arg, int priority_arg,
                    const char *format_arg, va_list ap_arg) :
    unary_function<Error_message_handler *, bool>(), session(session_arg),
    priority(priority_arg), format(format_arg)
    {
      va_copy(ap, ap_arg);
    }

  ~ErrorMessagePrint()  { va_end(ap); }

  inline result_type operator()(argument_type handler)
  {
    if (handler->errmsg(session, priority, format, ap))
    {
      /* we're doing the errmsg plugin api,
         so we can't trust the errmsg api to emit our error messages
         so we will emit error messages to stderr */
      /* TRANSLATORS: The leading word "errmsg" is the name
         of the plugin api, and so should not be translated. */
      fprintf(stderr,
              _("errmsg plugin '%s' errmsg() failed"),
              handler->getName().c_str());
      return true;
    }
    return false;
  }
};

bool errmsg_vprintf (Session *session, int priority,
                     char const *format, va_list ap)
{

  /* check to see if any errmsg plugin has been loaded
     if not, just fall back to emitting the message to stderr */
  if (!errmsg_has)
  {
    /* if it turns out that the vfprintf doesnt do one single write
       (single writes are atomic), then this needs to be rewritten to
       vsprintf into a char buffer, and then write() that char buffer
       to stderr */
    vfprintf(stderr, format, ap);
    return false;
  }

  /* Use find_if instead of foreach so that we can collect return codes */
  vector<Error_message_handler *>::iterator iter=
    find_if(all_errmsg_handler.begin(), all_errmsg_handler.end(),
            ErrorMessagePrint(session, priority, format, ap)); 
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_errmsg_handler.end();
}


