/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
 *
 *  Authors:
 *
 *  Padraig O'Sullivan <osullivan.padraig@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/**
 * @file 
 *
 * Implementation of helper methods for working with
 * bitsets and methods for thread-safe bitsets.
 */

#include "drizzled/sql_bitmap.h"

#include <bitset>

using namespace std;

void ThreadSafeBitset::resetBit(uint32_t pos)
{
  pthread_mutex_lock(&mutex);
  bitmap.reset(pos);
  pthread_mutex_unlock(&mutex);
}

uint32_t ThreadSafeBitset::setNextBit()
{
  uint32_t bit_found;
  pthread_mutex_lock(&mutex);
  if ((bit_found= getFirstBitPos(bitmap)) != MY_BIT_NONE)
    bitmap.set(bit_found);
  pthread_mutex_unlock(&mutex);
  return bit_found;
}

uint32_t getFirstBitPos(const bitset<MAX_FIELDS> &bitmap)
{
  uint32_t first_bit= MY_BIT_NONE;
  for (int idx= 0; idx < MAX_FIELDS; idx++)
  {
    if (!bitmap.test(idx))
    {
      first_bit= idx;
      break;
    }
  }
  return first_bit;
}

bool isBitmapSubset(const bitset<MAX_FIELDS> *map1, const bitset<MAX_FIELDS> *map2)
{
  bitset<MAX_FIELDS> tmp1= *map2;
  bitset<MAX_FIELDS> tmp2= *map1 & tmp1.flip();
  return (!tmp2.any());
}

bool isBitmapOverlapping(const bitset<MAX_FIELDS> *map1, const bitset<MAX_FIELDS> *map2)
{
  bitset<MAX_FIELDS> tmp= *map1 & *map2;
  return (tmp.any());
}
