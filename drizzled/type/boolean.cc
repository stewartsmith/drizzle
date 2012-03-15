/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

#include <config.h>

#include <drizzled/sql_string.h>
#include <drizzled/type/boolean.h>
#include <drizzled/charset.h>

namespace drizzled {
namespace type {

const char* convert(bool source, bool ansi_display)
{
  if (source)
    return ansi_display ? "YES" : "TRUE";
  return ansi_display ? "NO" : "FALSE";
}

void convert(String& destination, bool source, bool ansi_display)
{
  const char* v= convert(source, ansi_display);
  destination.alloc(strlen(v));
  strcpy(destination.c_ptr(), v);
  destination.length(strlen(v));
}

bool convert(bool &destination, const char *source, const size_t source_length)
{
  switch (source_length)
  {
  case 1:
    {
      switch (source[0])
      {
      case 'y': case 'Y':
      case 't': case 'T': // PG compatibility
        destination= true;
        return true;

      case 'n': case 'N':
      case 'f': case 'F': // PG compatibility
        destination= false;
        return true;
      }
    }
    break;

  case 5:
    if (not (system_charset_info->strcasecmp(source, "FALSE")))
    {
      destination= false;
      return true;
    }
    break;

  case 4:
    if (not (system_charset_info->strcasecmp(source, "TRUE")))
    {
      destination= true;
      return true;
    }
    break;

  case 3:
    if (not (system_charset_info->strcasecmp(source, "YES")))
    {
      destination= true;
      return true;
    }
    break;

  case 2:
    if (not (system_charset_info->strcasecmp(source, "NO")))
    {
      destination= false;
      return true;
    }
    break;
  }

  // Failure to match
  destination= false;
  return false;
}

bool convert(bool &destination, String &source)
{
  return convert(destination, source.c_ptr(), source.length());
}

} /* namespace type */
} /* namespace drizzled */
