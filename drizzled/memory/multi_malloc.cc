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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include <drizzled/memory/multi_malloc.h>
#include <drizzled/definitions.h>

namespace drizzled {
namespace memory {

/*
  Malloc many pointers at the same time
  Only ptr1 can be free'd, and doing this will free all
  the memory allocated. ptr2, etc all point inside big allocated
  memory area.

  SYNOPSIS
    multi_malloc()
      zerofill             Whether or not to fill with 0
	ptr1, length1      Multiple arguments terminated by null ptr
	ptr2, length2      ...
        ...
	NULL
*/

void* multi_malloc(bool zerofill, ...)
{
  va_list args;
  void **ptr, *start;
  char *res;
  size_t tot_length,length;

  va_start(args, zerofill);
  tot_length=0;
  while ((ptr=va_arg(args, void **)))
  {
    /*
     * This must be unsigned int, not size_t.
     * Otherwise, everything breaks.
     */
    length=va_arg(args, unsigned int);
    tot_length+=ALIGN_SIZE(length);
  }
  va_end(args);

#ifdef HAVE_VALGRIND
  if (!(start= calloc(0, tot_length)))
    return 0;
#else
  start= malloc(tot_length);
  if (zerofill)
    memset(start, 0, tot_length);
#endif

  va_start(args, zerofill);
  res= static_cast<char *>(start);
  while ((ptr=va_arg(args, void **)))
  {
    *ptr=res;
    length=va_arg(args,unsigned int);
    res+= ALIGN_SIZE(length);
  }
  va_end(args);
  return start;
}

} /* namespace memory */
} /* namespace drizzled */
