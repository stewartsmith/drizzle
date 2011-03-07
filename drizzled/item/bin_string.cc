/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>
#include <drizzled/item/bin_string.h>

namespace drizzled
{

/*
  bin item.
  In string context this is a binary string. 
  In number context this is a int64_t value.
*/

Item_bin_string::Item_bin_string(const char *str, uint32_t str_length)
{
  const char *end= str + str_length - 1;
  unsigned char bits= 0;
  uint32_t power= 1;

  max_length= (str_length + 7) >> 3;
  char *ptr= (char*) memory::sql_alloc(max_length + 1);
  if (!ptr)
    return;
  str_value.set(ptr, max_length, &my_charset_bin);
  ptr+= max_length - 1;
  ptr[1]= 0;                     // Set end null for string
  for (; end >= str; end--)
  {
    if (power == 256)
    {
      power= 1;
      *ptr--= bits;
      bits= 0;
    }
    if (*end == '1')
      bits|= power;
    power<<= 1;
  }
  *ptr= (char) bits;
  collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
  fixed= 1;
}


} /* namespace drizzled */
