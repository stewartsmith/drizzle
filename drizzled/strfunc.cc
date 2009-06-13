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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Some useful string utility functions used by the MySQL server */
#include <drizzled/server_includes.h>

/*
  Return bitmap for strings used in a set

  SYNOPSIS
  find_set()
  lib			Strings in set
  str			Strings of set-strings separated by ','
  err_pos		If error, set to point to start of wrong set string
  err_len		If error, set to the length of wrong set string
  set_warning		Set to 1 if some string in set couldn't be used

  NOTE
    We delete all end space from str before comparison

  RETURN
    bitmap of all sets found in x.
    set_warning is set to 1 if there was any sets that couldn't be set
*/

static const char field_separator=',';

uint64_t find_set(TYPELIB *lib, const char *str, uint32_t length,
                  const CHARSET_INFO * const cs,
                  char **err_pos, uint32_t *err_len, bool *set_warning)
{
  const CHARSET_INFO * const strip= cs ? cs : &my_charset_utf8_general_ci;
  const char *end= str + strip->cset->lengthsp(strip, str, length);
  uint64_t found= 0;
  *err_pos= 0;                  // No error yet
  if (str != end)
  {
    const char *start= str;
    for (;;)
    {
      const char *pos= start;
      uint32_t var_len;
      int mblen= 1;

      for (; pos != end && *pos != field_separator; pos++) 
      {}
      var_len= (uint32_t) (pos - start);
      uint32_t find= cs ? find_type2(lib, start, var_len, cs) :
                      find_type(lib, start, var_len, (bool) 0);
      if (!find)
      {
        *err_pos= (char*) start;
        *err_len= var_len;
        *set_warning= 1;
      }
      else
        found|= ((int64_t) 1 << (find - 1));
      if (pos >= end)
        break;
      start= pos + mblen;
    }
  }
  return found;
}


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

uint32_t find_type(const TYPELIB *lib, const char *find, uint32_t length,
               bool part_match)
{
  uint32_t found_count=0, found_pos=0;
  const char *end= find+length;
  const char *i;
  const char *j;
  for (uint32_t pos=0 ; (j=lib->type_names[pos++]) ; )
  {
    for (i=find ; i != end &&
	   my_toupper(system_charset_info,*i) ==
	   my_toupper(system_charset_info,*j) ; i++, j++) ;
    if (i == end)
    {
      if (! *j)
	return(pos);
      found_count++;
      found_pos= pos;
    }
  }
  return(found_count == 1 && part_match ? found_pos : 0);
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

uint32_t find_type2(const TYPELIB *typelib, const char *x, uint32_t length,
                const CHARSET_INFO * const cs)
{
  int pos;
  const char *j;

  if (!typelib->count)
  {
    return(0);
  }

  for (pos=0 ; (j=typelib->type_names[pos]) ; pos++)
  {
    if (!my_strnncoll(cs, (const unsigned char*) x, length,
                          (const unsigned char*) j, typelib->type_lengths[pos]))
      return(pos+1);
  }
  return(0);
} /* find_type */


/*
  Un-hex all elements in a typelib

  SYNOPSIS
   unhex_type2()
   interval       TYPELIB (struct of pointer to values + lengths + count)

  NOTES

  RETURN
    N/A
*/

void unhex_type2(TYPELIB *interval)
{
  for (uint32_t pos= 0; pos < interval->count; pos++)
  {
    char *from, *to;
    for (from= to= (char*) interval->type_names[pos]; *from; )
    {
      /*
        Note, hexchar_to_int(*from++) doesn't work
        one some compilers, e.g. IRIX. Looks like a compiler
        bug in inline functions in combination with arguments
        that have a side effect. So, let's use from[0] and from[1]
        and increment 'from' by two later.
      */

      *to++= (char) (hexchar_to_int(from[0]) << 4) +
                     hexchar_to_int(from[1]);
      from+= 2;
    }
    interval->type_lengths[pos] /= 2;
  }
}


/*
  Check if the first word in a string is one of the ones in TYPELIB

  SYNOPSIS
    check_word()
    lib		TYPELIB
    val		String to check
    end		End of input
    end_of_word	Store value of last used byte here if we found word

  RETURN
    0	 No matching value
    > 1  lib->type_names[#-1] matched
	 end_of_word will point to separator character/end in 'val'
*/

uint32_t check_word(TYPELIB *lib, const char *val, const char *end,
		const char **end_of_word)
{
  int res;
  const char *ptr;

  /* Fiend end of word */
  for (ptr= val ; ptr < end && my_isalpha(&my_charset_utf8_general_ci, *ptr) ; ptr++)
    ;
  if ((res=find_type(lib, val, (uint32_t) (ptr - val), 1)) > 0)
    *end_of_word= ptr;
  return res;
}


/*
  Searches for a LEX_STRING in an LEX_STRING array.

  SYNOPSIS
    find_string_in_array()
      heap    The array
      needle  The string to search for

  NOTE
    The last LEX_STRING in the array should have str member set to NULL

  RETURN VALUES
    -1   Not found
    >=0  Ordinal position
*/

int find_string_in_array(LEX_STRING * const haystack, LEX_STRING * const needle,
                         const CHARSET_INFO * const cs)
{
  const LEX_STRING *pos;
  for (pos= haystack; pos->str; pos++)
    if (!cs->coll->strnncollsp(cs, (unsigned char *) pos->str, pos->length,
                               (unsigned char *) needle->str, needle->length, 0))
    {
      return (pos - haystack);
    }
  return -1;
}
