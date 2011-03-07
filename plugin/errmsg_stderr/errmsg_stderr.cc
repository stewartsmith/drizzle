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

#include <config.h>
#include <drizzled/plugin/error_message.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin.h>

#include <stdio.h>  /* for vsnprintf */
#include <stdarg.h>  /* for va_list */
#include <unistd.h>  /* for write(2) */

using namespace drizzled;

/* todo, make this dynamic as needed */
#define MAX_MSG_LEN 8192

class Error_message_stderr : public plugin::ErrorMessage
{
public:
  Error_message_stderr()
   : plugin::ErrorMessage("Error_message_stderr") {}
  virtual bool errmsg(error::level_t , const char *format, va_list ap)
  {
    char msgbuf[MAX_MSG_LEN];
    int prv, wrv;

    prv= vsnprintf(msgbuf, MAX_MSG_LEN, format, ap);
    if (prv < 0) return true;

    /* a single write has a OS level thread lock
       so there is no need to have mutexes guarding this write,
    */
    wrv= write(fileno(stderr), msgbuf, prv);
    fputc('\n', stderr);
    if ((wrv < 0) || (wrv != prv))
      return true;

    return false;
  }
};

static Error_message_stderr *handler= NULL;
static int errmsg_stderr_plugin_init(module::Context &context)
{
  handler= new Error_message_stderr();
  context.add(handler);

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "errmsg_stderr",
  "0.1",
  "Mark Atwood <mark@fallenpegasus.com>",
  N_("Error Messages to stderr"),
  PLUGIN_LICENSE_GPL,
  errmsg_stderr_plugin_init,
  NULL, /* depends */
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
