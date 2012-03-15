/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/memory/sql_alloc.h>
#include <drizzled/base.h>

namespace drizzled
{

namespace optimizer
{

class QuickRange : public memory::SqlAlloc
{
public:
  unsigned char *min_key;
  unsigned char *max_key;
  uint16_t min_length;
  uint16_t max_length;
  uint16_t flag;
  key_part_map min_keypart_map; /**< bitmap of used keyparts in min_key */
  key_part_map max_keypart_map; /**< bitmap of used keyparts in max_key */

  QuickRange(); /**< Constructor for a "full range" */
  QuickRange(const unsigned char *min_key_arg,
              uint32_t min_length_arg,
              key_part_map min_keypart_map_arg,
	            const unsigned char *max_key_arg,
              uint32_t max_length_arg,
              key_part_map max_keypart_map_arg,
	            uint32_t flag_arg)
    :
      min_key((unsigned char*) memory::sql_memdup(min_key_arg,min_length_arg+1)),
      max_key((unsigned char*) memory::sql_memdup(max_key_arg,max_length_arg+1)),
      min_length((uint16_t) min_length_arg),
      max_length((uint16_t) max_length_arg),
      flag((uint16_t) flag_arg),
      min_keypart_map(min_keypart_map_arg),
      max_keypart_map(max_keypart_map_arg)
    {}
};

} /* namespace optimizer */

} /* namespace drizzled */

