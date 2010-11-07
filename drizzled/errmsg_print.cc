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

/* This file and these functions are a stopgap until all the
   sql_print_foo() function calls are replaced with calls to
   errmsg_printf()
*/

#include "config.h"

#include <drizzled/definitions.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/plugin/error_message.h>

#include <cerrno>
#include <cstring>

namespace drizzled
{

void sql_perror(const char *message)
{
  char errmsg[STRERROR_MAX];
  strerror_r(errno, errmsg, sizeof(errmsg));
  errmsg_printf(ERRMSG_LVL_ERROR, "%s: %s\n", message, errmsg);
}

bool errmsg_printf (int priority, char const *format, ...)
{
  bool rv;
  va_list args;
  va_start(args, format);
  rv= plugin::ErrorMessage::vprintf(NULL, priority, format, args);
  va_end(args);
  return rv;
}

} /* namespace drizzled */
