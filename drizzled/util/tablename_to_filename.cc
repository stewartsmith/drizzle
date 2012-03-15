/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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
#include <string>

#include <boost/foreach.hpp>
#include <drizzled/util/tablename_to_filename.h>
#include <drizzled/internal/my_sys.h>

namespace drizzled {
namespace util {

static const char* hexchars= "0123456789abcdef";


/*
  Translate a table name to a cursor name (WL #1324).

  SYNOPSIS
    tablename_to_filename()
      from                      The table name
      to                OUT     The cursor name

  RETURN
    true if errors happen. false on success.
*/
std::string tablename_to_filename(const std::string &from)
{
  std::string to;
  BOOST_FOREACH(char it, from)
  {
    if (isascii(it))
    {
      if (isdigit(it) || islower(it) || it == '_' || it == ' ' || it == '-')
      {
        to.push_back(it);
        continue;
      }

      if (isupper(it))
      {
        to.push_back(tolower(it));
        continue;
      }
    }
   
    /* We need to escape this char in a way that can be reversed */
    to.push_back('@');
    to.push_back(hexchars[(it >> 4) & 15]);
    to.push_back(hexchars[it & 15]);
  }

  if (drizzled::internal::check_if_legal_tablename(to.c_str()))
  {
    to += "@@@";
  }
  return to;
}

} /* namespace util */
} /* namespace drizzled */
