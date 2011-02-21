/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <drizzled/key_map.h>

namespace drizzled
{

bool is_keymap_prefix(const key_map& map, const uint32_t prefix_size)
{
  size_t pos= 0;

  for (; pos < prefix_size; pos++)
    if (! map.test(pos))
      return false;

  /*TODO: huh?
    uint32_t prefix_bits= prefix_size & 0x7;
    if (prefix_bits &&  != (1 << prefix_bits)-1)
    return false;
  */

  for (; pos < map.size(); pos++)
    if (map.test(pos))
      return false;

  return true;
}

void set_prefix(key_map& map, const uint32_t prefix_size)
{
  size_t pos= 0;

  for (; pos < prefix_size && pos < map.size(); pos++)
  {
    map.set(pos);
  }
}

bool is_overlapping(const key_map& map, const key_map& map2)
{
  size_t count;
  for (count= 0; count < map.size(); count++)
  {
    if (map[count] & map2[count])
      return false;
  }
  return true;
}

void key_map_subtract(key_map& map1, key_map& map2)
{
  map1&= map2.flip();
  map2.flip();
}

} /* namespace drizzled */
