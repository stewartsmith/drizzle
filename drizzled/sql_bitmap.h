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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DRIZZLED_SQL_BITMAP_H
#define DRIZZLED_SQL_BITMAP_H

/*
  Implementation of a bitmap type.
  The idea with this is to be able to handle any constant number of bits but
  also be able to use 32 or 64 bits bitmaps very efficiently
*/

#include <drizzled/definitions.h>
#include <drizzled/util/test.h>
#include <drizzled/key_map.h>
#include <pthread.h>

#include <cstring>
#include <boost/dynamic_bitset.hpp>

namespace drizzled
{

typedef uint64_t table_map;          /* Used for table bits in join */
typedef uint32_t nesting_map;  /* Used for flags of nesting constructs */


static const uint32_t MY_BIT_NONE= UINT32_MAX;

bool bitmap_is_subset(const boost::dynamic_bitset<>& map1, const boost::dynamic_bitset<>& map2);
bool bitmap_is_overlapping(const boost::dynamic_bitset<>& map1,
                           const boost::dynamic_bitset<>& map2);
void bitmap_union(boost::dynamic_bitset<>& map, const boost::dynamic_bitset<>& map2);
/* Fast, not thread safe, bitmap functions */
/* This one is a define because it gets used in an array decl */
#define bitmap_buffer_size(bits) ((bits+31)/32)*4

} /* namespace drizzled */

#endif /* DRIZZLED_SQL_BITMAP_H */
