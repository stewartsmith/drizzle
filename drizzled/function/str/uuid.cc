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
#include CSTDINT_H
#include <drizzled/function/str/uuid.h>
#include <uuid/uuid.h>

String *Item_func_uuid::val_str(String *str)
{
  uuid_t uu;
  char *uuid_string;

  /* 36 characters for uuid string +1 for NULL */
  str->realloc(36+1);
  str->length(36);
  str->set_charset(system_charset_info);
  uuid_string= (char *) str->ptr();
  uuid_generate_time(uu);
  uuid_unparse(uu, uuid_string);

  return str;
}




