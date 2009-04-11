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

#ifndef _SQL_BITMAP_H_
#define _SQL_BITMAP_H_

#include <mysys/my_bitmap.h>
#include <drizzled/util/test.h>
#include <drizzled/key_map.h>

#include <bitset>

#define BIT_NONE (~(uint32_t) 0)

typedef uint64_t table_map;          /* Used for table bits in join */
typedef uint32_t nesting_map;  /* Used for flags of nesting constructs */

/*
  Used to identify NESTED_JOIN structures within a join (applicable only to
  structures that have not been simplified away and embed more the one
  element)
*/
typedef uint64_t nested_join_map; /* Needed by sql_select.h and table.h */

/* useful constants */#
extern const key_map key_map_empty;
extern key_map key_map_full;          /* Should be threaded as const */

/*
 * Class to be used when a thread safe version of std::bitset
 * is needed. We just use a lock here to protect any modifications
 * to the bitset.
 */
class ThreadSafeBitset
{
public:
  ThreadSafeBitset()
  {
    pthread_mutex_init(&mutex, NULL);
  }
  ~ThreadSafeBitset()
  {
    pthread_mutex_destroy(&mutex);
  }
  /*
   * Resets a bit at the given bit position.
   *
   * @param position of the bit to reset
   */
  void resetBit(uint32_t pos);
  /*
   * Finds the first bit that is not set and sets
   * it.
   */
  uint32_t setNextBit();
private:
  std::bitset<MAX_FIELDS> bitmap;
  pthread_mutex_t mutex;
};

/*
 * Returns the position of the first bit in the
 * given bitmap which is not set. If every bit is set
 * in the bitmap, return BIT_NONE.
 *
 * @param the bitmap to work with
 */
uint32_t getFirstBitPos(const std::bitset<MAX_FIELDS> &bitmap);

/*
 * Returns true if map1 is a subset of map2; otherwise,
 * it returns false.
 *
 * @param the bitmap to check with
 * @param the bitmap to check against
 */
bool isBitmapSubset(const std::bitset<MAX_FIELDS> *map1, const std::bitset<MAX_FIELDS> *map2);

/*
 * Returns true if there is any overlapping bits between
 * the 2 given bitmaps.
 *
 * @param the first bitmap to work with
 * @param the second bitmap to work with
 */
bool isBitmapOverlapping(const std::bitset<MAX_FIELDS> *map1, const std::bitset<MAX_FIELDS> *map2);

#endif /* _SQL_BITMAP_H_ */
