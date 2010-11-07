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


/* Function with list databases, tables or fields */
#include "config.h"
#include <drizzled/sql_select.h>
#include <drizzled/show.h>
#include <drizzled/gettext.h>
#include <drizzled/util/convert.h>
#include <drizzled/error.h>
#include <drizzled/tztime.h>
#include <drizzled/data_home.h>
#include <drizzled/item/blob.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/return_int.h>
#include <drizzled/item/empty_string.h>
#include <drizzled/item/return_date_time.h>
#include <drizzled/sql_base.h>
#include <drizzled/db.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/field/decimal.h>
#include <drizzled/lock.h>
#include <drizzled/item/return_date_time.h>
#include <drizzled/item/empty_string.h>
#include "drizzled/session_list.h"
#include <drizzled/message/schema.pb.h>
#include <drizzled/plugin/client.h>
#include <drizzled/cached_directory.h>
#include "drizzled/sql_table.h"
#include "drizzled/global_charset_info.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/internal/m_string.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/message/statement_transform.h"


#include <sys/stat.h>

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

using namespace std;

namespace drizzled
{

inline const char *
str_or_nil(const char *str)
{
  return str ? str : "<nil>";
}

int wild_case_compare(const CHARSET_INFO * const cs, const char *str, const char *wildstr)
{
  register int flag;

  while (*wildstr)
  {
    while (*wildstr && *wildstr != internal::wild_many && *wildstr != internal::wild_one)
    {
      if (*wildstr == internal::wild_prefix && wildstr[1])
        wildstr++;
      if (my_toupper(cs, *wildstr++) != my_toupper(cs, *str++))
        return (1);
    }
    if (! *wildstr )
      return (*str != 0);
    if (*wildstr++ == internal::wild_one)
    {
      if (! *str++)
        return (1);	/* One char; skip */
    }
    else
    {						/* Found '*' */
      if (! *wildstr)
        return (0);		/* '*' as last char: OK */
      flag=(*wildstr != internal::wild_many && *wildstr != internal::wild_one);
      do
      {
        if (flag)
        {
          char cmp;
          if ((cmp= *wildstr) == internal::wild_prefix && wildstr[1])
            cmp= wildstr[1];
          cmp= my_toupper(cs, cmp);
          while (*str && my_toupper(cs, *str) != cmp)
            str++;
          if (! *str)
            return (1);
        }
        if (wild_case_compare(cs, str, wildstr) == 0)
          return (0);
      } while (*str++);
      return (1);
    }
  }

  return (*str != '\0');
}

/*
  Get the quote character for displaying an identifier.

  SYNOPSIS
    get_quote_char_for_identifier()

  IMPLEMENTATION
    Force quoting in the following cases:
      - name is empty (for one, it is possible when we use this function for
        quoting user and host names for DEFINER clause);
      - name is a keyword;
      - name includes a special character;
    Otherwise identifier is quoted only if the option OPTION_QUOTE_SHOW_CREATE
    is set.

  RETURN
    EOF	  No quote character is needed
    #	  Quote character
*/

int get_quote_char_for_identifier()
{
  return '`';
}

} /* namespace drizzled */
