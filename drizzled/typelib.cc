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
#include <drizzled/charset_info.h>
#include <drizzled/typelib.h>

namespace drizzled
{

static const char field_separator=',';

int st_typelib::find_type_or_exit(const char *x, const char *option) const
{
  int res= find_type(const_cast<char*>(x), 2);
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
			If & 1 accept only whole names
			If & 2 don't expand if half field
			If & 4 allow #number# as type
			If & 8 use ',' as string terminator

  NOTES
    If part, uniq field is found and full_name == 0 then x is expanded
    to full field.

  RETURN
    -1	Too many matching values
    0	No matching value
    >0  Offset+1 in typelib for matched string
*/


int st_typelib::find_type(const char *x, uint32_t full_name) const
{
  assert(full_name & 2);
  return find_type(const_cast<char*>(x), full_name);
}

int st_typelib::find_type(char *x, uint32_t full_name) const
{
  if (!count)
    return 0;
  int find= 0;
  int findpos= 0;
  const char *j;
  for (int pos= 0; (j= type_names[pos]); pos++)
  {
    const char *i;
    for (i= x;
    	*i && (!(full_name & 8) || *i != field_separator) &&
        my_toupper(&my_charset_utf8_general_ci,*i) ==
    		my_toupper(&my_charset_utf8_general_ci,*j) ; i++, j++) ;
    if (! *j)
    {
      while (*i == ' ')
	i++;					/* skip_end_space */
      if (! *i || ((full_name & 8) && *i == field_separator))
	return(pos+1);
    }
    if ((!*i && (!(full_name & 8) || *i != field_separator)) &&
        (!*j || !(full_name & 1)))
    {
      find++;
      findpos=pos;
    }
  }
  if (find == 0 && (full_name & 4) && x[0] == '#' && strchr(x, '\0')[-1] == '#' &&
      (findpos=atoi(x+1)-1) >= 0 && (uint32_t) findpos < count)
    find=1;
  else if (find == 0 || ! x[0])
  {
    return(0);
  }
  else if (find != 1 || (full_name & 1))
  {
    return(-1);
  }
  if (!(full_name & 2))
    strcpy(x, type_names[findpos]);
  return findpos + 1;
} /* find_type */


	/* Get name of type nr 'nr' */
	/* Warning first type is 1, 0 = empty field */

void st_typelib::make_type(char *to, uint32_t nr) const
{
  if (!nr)
    to[0]= 0;
  else
    strcpy(to, get_type(nr - 1));
} /* make_type */


	/* Get type */
	/* Warning first type is 0 */

const char *st_typelib::get_type(uint32_t nr) const
{
  if (nr < count && type_names)
    return type_names[nr];
  return "?";
}


/*
  Create an integer value to represent the supplied comma-seperated
  string where each string in the TYPELIB denotes a bit position.

  SYNOPSIS
    find_typeset()
    x		string to decompose
    lib		TYPELIB (struct of pointer to values + count)
    err		index (not char position) of string element which was not
                found or 0 if there was no error

  RETURN
    a integer representation of the supplied string
*/

uint64_t st_typelib::find_typeset(const char *x, int *err) const
{
  if (!count)
    return 0;
  uint64_t result= 0;
  *err= 0;
  while (*x)
  {
    (*err)++;
    const char *i= x;
    while (*x && *x != field_separator) x++;
    int find= find_type(i, 2 | 8) - 1;
    if (find < 0)
      return 0;
    result|= (1ULL << find);
  }
  *err= 0;
  return result;
} /* find_set */


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

TYPELIB *st_typelib::copy_typelib(memory::Root *root) const
{
  TYPELIB *to;
  uint32_t i;

  if (!this)
    return NULL;

  if (!(to= (TYPELIB*) root->alloc_root(sizeof(TYPELIB))))
    return NULL;

  if (!(to->type_names= (const char **)
        root->alloc_root((sizeof(char *) + sizeof(int)) * (count + 1))))
    return NULL; // leaking
  to->type_lengths= (unsigned int *)(to->type_names + count + 1);
  to->count= count;
  if (name)
  {
    if (!(to->name= root->strdup_root(name)))
      return NULL; // leaking
  }
  else
    to->name= NULL;

  for (i= 0; i < count; i++)
  {
    if (!(to->type_names[i]= root->strmake_root(type_names[i], type_lengths[i])))
      return NULL; // leaking
    to->type_lengths[i]= type_lengths[i];
  }
  to->type_names[to->count]= NULL;
  to->type_lengths[to->count]= 0;

  return to;
}

} /* namespace drizzled */
