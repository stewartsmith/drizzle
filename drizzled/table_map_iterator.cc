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

#include <config.h>
#include CSTDINT_H
#include <drizzled/table_map_iterator.h>

int Table_map_iterator::next_bit()
{

  static const char last_bit[16]= {32, 0, 1, 0,
				   2, 0, 1, 0,
				   3, 0, 1, 0,
				   2, 0, 1, 0};
  uint32_t bit;
  while ((bit= last_bit[bmp & 0xF]) == 32)
  {
    no += 4;
    bmp= bmp >> 4;
    if (!bmp)
      return BITMAP_END;
  }
  bmp &= ~(1 << bit);
  return no + bit;

}
