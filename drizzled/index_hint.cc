/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  Authors:
 *
 *  Jay Pipes <jay.pipes@sun.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/**
 * @file 
 *
 * Implementation of SQL index hints
 */
#include <config.h>
#include <drizzled/sql_string.h>
#include <drizzled/session.h>
#include <drizzled/index_hint.h>
#include <drizzled/sql_table.h>

namespace drizzled {

/*
 * Names of the index hints (for error messages). Keep in sync with
 * index_hint_type
 */
const char *index_hint_type_name[] =
{
  "IGNORE INDEX"
, "USE INDEX"
, "FORCE INDEX"
};

/**
 * Print an index hint
 *
 * Prints out the USE|FORCE|IGNORE index hint.
 *
 * @param Session pointer
 * @param[out] Appends the index hint here
 */
void Index_hint::print(String& str) const
{
  str.append(STRING_WITH_LEN(index_hint_type_name[type]));
  str.append(STRING_WITH_LEN(" ("));
  if (*key_name)
  {
    if (is_primary_key(key_name))
    {
      str.append(str_ref(key_name));
    }
    else
    {
      str.append_identifier(str_ref(key_name));
    }
  }
  str.append(')');
}

} /* namespace drizzled */
