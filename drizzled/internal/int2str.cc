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
#include <drizzled/internal/m_string.h>

namespace drizzled
{
namespace internal
{

/*
  Converts integer to its string representation in decimal notation.

  SYNOPSIS
    int10_to_str()
      val     - value to convert
      dst     - points to buffer where string representation should be stored
      radix   - flag that shows whenever val should be taken as signed or not

  DESCRIPTION
    This is version of int2str() function which is optimized for normal case
    of radix 10/-10. It takes only sign of radix parameter into account and
    not its absolute value.

  RETURN VALUE
    Pointer to ending NUL character.
*/

char *int10_to_str(int32_t val,char *dst,int radix)
{
  char buffer[65];
  int32_t new_val;
  uint32_t uval = (uint32_t) val;

  if (radix < 0)				/* -10 */
  {
    if (val < 0)
    {
      *dst++ = '-';
      /* Avoid integer overflow in (-val) for INT32_MIN (BUG#31799). */
      uval = (uint32_t)0 - uval;
    }
  }

  char* p = &buffer[sizeof(buffer)-1];
  *p = '\0';
  new_val= (int32_t) (uval / 10);
  *--p = '0'+ (char) (uval - (uint32_t) new_val * 10);
  val = new_val;

  while (val != 0)
  {
    new_val=val/10;
    *--p = '0' + (char) (val-new_val*10);
    val= new_val;
  }
  while ((*dst++ = *p++) != 0) ;
  return dst-1;
}

} /* namespace internal */
} /* namespace drizzled */
