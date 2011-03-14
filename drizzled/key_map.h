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

#pragma once

#include <drizzled/definitions.h>

#include <bitset>

#include <drizzled/visibility.h>

namespace drizzled
{

/* Used for finding keys */
#if MAX_INDEXES <= 64
typedef std::bitset<72> key_map;
#else
typedef std::bitset<((MAX_INDEXES+7)/8*8)> key_map;
#endif

/* useful constants */
extern const key_map key_map_empty;
extern DRIZZLED_API key_map key_map_full;          /* Should be threaded as const */

bool is_keymap_prefix(const key_map& map, const uint32_t n);
bool is_overlapping(const key_map& map, const key_map& map2);
void set_prefix(key_map& map, const uint32_t n);
void key_map_subtract(key_map& map1, key_map& map2);

} /* namespace drizzled */

