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

#include <config.h>

#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>

#include <algorithm>

using namespace std;

namespace drizzled
{
namespace internal
{

	/* Functions definied in this file */

size_t dirname_length(const char *name)
{
  const char *pos, *gpos;
#ifdef FN_DEVCHAR
  if ((pos=(char*)strrchr(name,FN_DEVCHAR)) == 0)
#endif
    pos=name-1;

  gpos= pos++;
  for ( ; *pos ; pos++)				/* Find last FN_LIBCHAR */
  {
    if (*pos == FN_LIBCHAR || *pos == '/')
      gpos=pos;
  }
  return gpos-name+1;
}


/*
  Gives directory part of filename. Directory ends with '/'

  SYNOPSIS
    dirname_part()
    to		Store directory name here
    name	Original name
    to_length	Store length of 'to' here

  RETURN
   #  Length of directory part in 'name'
*/

size_t dirname_part(char *to, const char *name, size_t *to_res_length)
{
  size_t length;

  length=dirname_length(name);
  *to_res_length= (size_t) (convert_dirname(to, name, name+length) - to);
  return(length);
} /* dirname */


/*
  Convert directory name to use under this system

  SYNPOSIS
    convert_dirname()
    to				Store result here. Must be at least of size
    				min(FN_REFLEN, strlen(from) + 1) to make room
    				for adding FN_LIBCHAR at the end.
    from			Original filename. May be == to
    from_end			Pointer at end of filename (normally end \0)

  IMPLEMENTATION
    If MSDOS converts '/' to '\'
    If VMS converts '<' to '[' and '>' to ']'
    Adds a FN_LIBCHAR to end if the result string if there isn't one
    and the last isn't dev_char.
    Copies data from 'from' until ASCII(0) for until from == from_end
    If you want to use the whole 'from' string, just send NULL as the
    last argument.

    If the result string is larger than FN_REFLEN -1, then it's cut.

  RETURN
   Returns pointer to end \0 in to
*/

#ifndef FN_DEVCHAR
#define FN_DEVCHAR '\0'				/* For easier code */
#endif

char *convert_dirname(char *to, const char *from, const char *from_end)
{
  char *to_org=to;

  /* We use -2 here, becasue we need place for the last FN_LIBCHAR */
  if (!from_end || (from_end - from) > FN_REFLEN-2)
    from_end=from+FN_REFLEN -2;

#if FN_LIBCHAR != '/'
  {
    for (; from != from_end && *from ; from++)
    {
      if (*from == '/')
	*to++= FN_LIBCHAR;
      else
      {
        *to++= *from;
      }
    }
    *to=0;
  }
#else
  /* This is ok even if to == from, becasue we need to cut the string */
  size_t len= min(strlen(from),(size_t)(from_end-from));
  void *ret= memmove(to, from, len);
  assert(ret != NULL);
  to+= len;
  to[0]= '\0';
#endif

  /* Add FN_LIBCHAR to the end of directory path */
  if (to != to_org && (to[-1] != FN_LIBCHAR && to[-1] != FN_DEVCHAR))
  {
    *to++=FN_LIBCHAR;
    *to=0;
  }
  return(to);                              /* Pointer to end of dir */
} /* convert_dirname */

} /* namespace internal */
} /* namespace drizzled */
