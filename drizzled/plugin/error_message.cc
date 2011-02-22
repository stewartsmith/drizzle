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

#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin/error_message.h>

#include <cstdio>
#include <algorithm>
#include <vector>

namespace drizzled
{

std::vector<plugin::ErrorMessage *> all_errmsg_handler;

bool plugin::ErrorMessage::addPlugin(plugin::ErrorMessage *handler)
{
  all_errmsg_handler.push_back(handler);
  return false;
}

void plugin::ErrorMessage::removePlugin(plugin::ErrorMessage *)
{
  all_errmsg_handler.clear();
}


class Print : public std::unary_function<plugin::ErrorMessage *, bool>
{
  error::level_t priority;
  const char *format;
  va_list ap;

public:
  Print(error::level_t priority_arg,
        const char *format_arg, va_list ap_arg) : 
    std::unary_function<plugin::ErrorMessage *, bool>(),
    priority(priority_arg), format(format_arg)
  {
    va_copy(ap, ap_arg);
  }

  ~Print()  { va_end(ap); }

  inline result_type operator()(argument_type handler)
  {
    va_list handler_ap;
    va_copy(handler_ap, ap);
    if (handler->errmsg(priority, format, handler_ap))
    {
      /* we're doing the errmsg plugin api,
         so we can't trust the errmsg api to emit our error messages
         so we will emit error messages to stderr */
      /* TRANSLATORS: The leading word "errmsg" is the name
         of the plugin api, and so should not be translated. */
      fprintf(stderr,
              _("errmsg plugin '%s' errmsg() failed"),
              handler->getName().c_str());
      va_end(handler_ap);
      return true;
    }
    va_end(handler_ap);
    return false;
  }
}; 


bool plugin::ErrorMessage::vprintf(error::level_t priority, char const *format, va_list ap)
{
  if (not (priority >= error::verbosity()))
    return false;

  /* 
    Check to see if any errmsg plugin has been loaded
    if not, just fall back to emitting the message to stderr.
  */
  if (not all_errmsg_handler.size())
  {
    /* if it turns out that the vfprintf doesnt do one single write
       (single writes are atomic), then this needs to be rewritten to
       vsprintf into a char buffer, and then write() that char buffer
       to stderr */
      vfprintf(stderr, format, ap);
      fputc('\n', stderr);
    return false;
  }

  /* Use find_if instead of foreach so that we can collect return codes */
  std::vector<plugin::ErrorMessage *>::iterator iter=
    std::find_if(all_errmsg_handler.begin(), all_errmsg_handler.end(),
                 Print(priority, format, ap)); 

  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being != 
   */
  return iter != all_errmsg_handler.end();
}

} /* namespace drizzled */
