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

#include <drizzled/server_includes.h>
#include <drizzled/errmsg.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/current_session.h>

// need this for stderr
#include <string.h>

void sql_print_error(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  errmsg_vprintf(current_session, ERROR_LEVEL, format, args);
  va_end(args);
  return;
}

void sql_print_warning(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  errmsg_vprintf(current_session, WARNING_LEVEL, format, args);
  va_end(args);
  return;
}

void sql_print_information(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  errmsg_vprintf(current_session, INFORMATION_LEVEL, format, args);
  va_end(args);
  return;
}

void sql_perror(const char *message)
{
  // is stderr threadsafe?
  errmsg_printf(ERRMSG_LVL_ERROR, "%s: %s", message, strerror(errno));
}


bool errmsg_printf (int priority, char const *format, ...)
{
  bool rv;
  va_list args;
  va_start(args, format);
  rv= errmsg_vprintf(current_session, priority, format, args);
  va_end(args);
  return rv;
}
