/* Copyright (C) 2000 MySQL AB

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

/* Functions to handle typelib */

#include <config.h>

#include <stdio.h>

#include <drizzled/internal/m_string.h>
#include <drizzled/charset.h>
#include <drizzled/memory/root.h>
#include <drizzled/typelib.h>

namespace drizzled {

static const char field_separator=',';

int TYPELIB::find_type_or_exit(const char *x, const char *option) const
{
  int res= find_type(x, e_dont_complete);
  if (res > 0)
    return res;
  if (!*x)
    fprintf(stderr, "No option given to %s\n", option);
  else
    fprintf(stderr, "Unknown option to %s: %s\n", option, x);
  const char **ptr= type_names;
  fprintf(stderr, "Alternatives are: '%s'", *ptr);
  while (*++ptr)
    fprintf(stderr, ",'%s'", *ptr);
  fprintf(stderr, "\n");
  exit(1);
}


/*
  Search after a string in a list of strings. Endspace in x is not compared.

  SYNOPSIS
   find_type()
   x			String to find
   lib			TYPELIB (struct of pointer to values + count)
   full_name		bitmap of what to do
			If & 1 accept only whole names - e_match_full
			If & 2 don't expand if half field - e_dont_complete
			If & 4 allow #number# as type - e_allow_int
			If & 8 use ',' as string terminator - e_use_comma

  NOTES
    If part, uniq field is found and full_name == 0 then x is expanded
    to full field.

  RETURN
    -1	Too many matching values
    0	No matching value
    >0  Offset+1 in typelib for matched string
*/


int TYPELIB::find_type(const char *x, e_find_options full_name) const
{
  assert(full_name & e_dont_complete);
  if (!count)
    return 0;
  int find= 0;
  int findpos= 0;
  const char *j;
  for (int pos= 0; (j= type_names[pos]); pos++)
  {
    const char *i= x;
    for (; *i && *i != field_separator &&
      my_toupper(&my_charset_utf8_general_ci, *i) == my_toupper(&my_charset_utf8_general_ci, *j); i++, j++)
    {
    }
    if (not *j)
    {
      while (*i == ' ')
        i++;					/* skip_end_space */
      if (not *i)
        return pos + 1;
    }
    if (not *i && *i != field_separator && (not *j || not (full_name & e_match_full)))
    {
      find++;
      findpos= pos;
    }
  }
  if (find == 0 || not x[0])
    return 0;
  if (find != 1 || (full_name & e_match_full))
    return -1;
  return findpos + 1;
} /* find_type */

/*
  Create a copy of a specified TYPELIB structure.

  SYNOPSIS
    copy_typelib()
    root	pointer to a memory::Root object for allocations
    from	pointer to a source TYPELIB structure

  RETURN
    pointer to the new TYPELIB structure on successful copy, or
    NULL otherwise
*/

TYPELIB *TYPELIB::copy_typelib(memory::Root& root) const
{
  TYPELIB* to= new (root) TYPELIB;
  to->type_names= (const char**)root.alloc((sizeof(char *) + sizeof(int)) * (count + 1));
  to->type_lengths= (unsigned int*)(to->type_names + count + 1);
  to->count= count;
  to->name= name ? root.strdup(name) : NULL;
  for (uint32_t i= 0; i < count; i++)
  {
    to->type_names[i]= root.strmake(type_names[i], type_lengths[i]);
    to->type_lengths[i]= type_lengths[i];
  }
  to->type_names[to->count]= NULL;
  to->type_lengths[to->count]= 0;
  return to;
}

} /* namespace drizzled */
