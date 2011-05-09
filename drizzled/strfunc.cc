/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/* Some useful string utility functions used by the MySQL server */
#include <config.h>

#include <drizzled/typelib.h>
#include <drizzled/charset.h>

namespace drizzled {

/*
  Function to find a string in a TYPELIB
  (Same format as mysys/typelib.c)

  SYNOPSIS
   find_type()
   lib			TYPELIB (struct of pointer to values + count)
   find			String to find
   length		Length of string to find
   part_match		Allow part matching of value

 RETURN
  0 error
  > 0 position in TYPELIB->type_names +1
*/

uint32_t TYPELIB::find_type(const char *find, uint32_t length, bool part_match) const
{
  uint32_t found_count=0, found_pos=0;
  const char* end= find + length;
  const char* i;
  const char* j;
  for (uint32_t pos= 0 ; (j= type_names[pos++]) ; )
  {
    for (i= find ; i != end && my_toupper(system_charset_info, *i) == my_toupper(system_charset_info, *j); i++, j++) 
    {
    }
    if (i == end)
    {
      if (not *j)
        return pos;
      found_count++;
      found_pos= pos;
    }
  }
  return found_count == 1 && part_match ? found_pos : 0;
}


/*
  Find a string in a list of strings according to collation

  SYNOPSIS
   find_type2()
   lib			TYPELIB (struct of pointer to values + count)
   x			String to find
   length               String length
   cs			Character set + collation to use for comparison

  NOTES

  RETURN
    0	No matching value
    >0  Offset+1 in typelib for matched string
*/

uint32_t TYPELIB::find_type2(const char *x, uint32_t length, const charset_info_st *cs) const
{
  if (!count)
    return 0;
  const char *j;
  for (int pos=0 ; (j= type_names[pos]) ; pos++)
  {
    if (!my_strnncoll(cs, (const unsigned char*) x, length,
                          (const unsigned char*) j, type_lengths[pos]))
      return pos + 1;
  }
  return 0;
} /* find_type */

} /* namespace drizzled */
