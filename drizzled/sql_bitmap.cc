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

#include "config.h"

#include <drizzled/sql_bitmap.h>
#include "drizzled/internal/m_string.h"
#include "drizzled/internal/my_bit.h"

#include <memory>

using namespace std;

namespace drizzled
{

bool bitmap_is_subset(const boost::dynamic_bitset<>& map1, const boost::dynamic_bitset<>& map2)
{
  for (boost::dynamic_bitset<>::size_type i= 0; i < map2.size(); i++)
  {
    if (map1.test(i) && ! map2.test(i))
    {
        return false;
    }
  }
  return true;
}


bool bitmap_is_overlapping(const boost::dynamic_bitset<>& map1, const boost::dynamic_bitset<>& map2)
{
  for (boost::dynamic_bitset<>::size_type i= 0; i < map2.size(); i++)
  {
    if (map1.test(i) && map2.test(i))
    {
        return true;
    }
  }
  return false;
}


void bitmap_union(boost::dynamic_bitset<>& map, const boost::dynamic_bitset<>& map2)
{
  map|= map2;
}

} /* namespace drizzled */
