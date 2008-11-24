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

/*
  Implementation of a bitmap type.
  The idea with this is to be able to handle any constant number of bits but
  also be able to use 32 or 64 bits bitmaps very efficiently
*/

/// TODO: OMG FIX THIS

#include <mysys/my_bitmap.h>
#include <drizzled/util/test.h>

template <uint32_t default_width> class Bitmap
{
  MY_BITMAP map;
  uint32_t buffer[(default_width+31)/32];
public:
  Bitmap() : map() { init(); }
  Bitmap(const Bitmap& from) : map() { *this=from; }
  explicit Bitmap(uint32_t prefix_to_set) : map(0) { init(prefix_to_set); }
  void init() { bitmap_init(&map, buffer, default_width, 0); }
  void init(uint32_t prefix_to_set) { init(); set_prefix(prefix_to_set); }
  uint32_t length() const { return default_width; }
  Bitmap& operator=(const Bitmap& map2)
  {
    init();
    memcpy(buffer, map2.buffer, sizeof(buffer));
    return *this;
  }
  void set_bit(uint32_t n) { bitmap_set_bit(&map, n); }
  void clear_bit(uint32_t n) { bitmap_clear_bit(&map, n); }
  void set_prefix(uint32_t n) { bitmap_set_prefix(&map, n); }
  void set_all() { bitmap_set_all(&map); }
  void clear_all() { bitmap_clear_all(&map); }
  void intersect(Bitmap& map2) { bitmap_intersect(&map, &map2.map); }
  void intersect(uint64_t map2buff)
  {
    MY_BITMAP map2;
    bitmap_init(&map2, (uint32_t *)&map2buff, sizeof(uint64_t)*8, 0);
    bitmap_intersect(&map, &map2);
  }
  /* Use highest bit for all bits above sizeof(uint64_t)*8. */
  void intersect_extended(uint64_t map2buff)
  {
    intersect(map2buff);
    if (map.n_bits > sizeof(uint64_t) * 8)
      bitmap_set_above(&map, sizeof(uint64_t),
                       test(map2buff & (1 << (sizeof(uint64_t) * 8 - 1))));
  }
  void subtract(Bitmap& map2) { bitmap_subtract(&map, &map2.map); }
  void merge(Bitmap& map2) { bitmap_union(&map, &map2.map); }
  bool is_set(uint32_t n) const { return bitmap_is_set(&map, n); }
  bool is_set() const { return !bitmap_is_clear_all(&map); }
  bool is_prefix(uint32_t n) const { return bitmap_is_prefix(&map, n); }
  bool is_clear_all() const { return bitmap_is_clear_all(&map); }
  bool is_set_all() const { return bitmap_is_set_all(&map); }
  bool is_subset(const Bitmap& map2) const { return bitmap_is_subset(&map, &map2.map); }
  bool is_overlapping(const Bitmap& map2) const { return bitmap_is_overlapping(&map, &map2.map); }
  bool operator==(const Bitmap& map2) const { return bitmap_cmp(&map, &map2.map); }
  bool operator!=(const Bitmap& map2) const { return !bitmap_cmp(&map, &map2.map); }
  Bitmap operator&=(uint32_t n)
  {
    if (bitmap_is_set(&map, n))
    {
      bitmap_clear_all(&map);
      bitmap_set_bit(&map, n);
    }
    else
      bitmap_clear_all(&map);
    return *this;
  }
  Bitmap operator&=(const Bitmap& map2)
  {
    bitmap_intersect(&map, &map2.map);
    return *this;
  }
  Bitmap operator&(uint32_t n)
  {
    Bitmap bm(*this);
    bm&= n;
    return bm;
  }
  Bitmap operator&(const Bitmap& map2)
  {
    Bitmap bm(*this);
    bm&= map2;
    return bm;
  }
  Bitmap operator|=(uint32_t n)
  {
    bitmap_set_bit(&map, n);
    return *this;
  }
  Bitmap operator|=(const Bitmap& map2)
  {
    bitmap_union(&map, &map2.map);
  }
  Bitmap operator|(uint32_t n)
  {
    Bitmap bm(*this);
    bm|= n;
    return bm;
  }
  Bitmap operator|(const Bitmap& map2)
  {
    Bitmap bm(*this);
    bm|= map2;
    return bm;
  }
  Bitmap operator~()
  {
    Bitmap bm(*this);
    bitmap_invert(&bm.map);
    return bm;
  }
  char *print(char *buf) const
  {
    char *s=buf;
    const unsigned char *e=(unsigned char *)buffer, *b=e+sizeof(buffer)-1;
    while (!*b && b>e)
      b--;
    if ((*s=_dig_vec_upper[*b >> 4]) != '0')
        s++;
    *s++=_dig_vec_upper[*b & 15];
    while (--b>=e)
    {
      *s++=_dig_vec_upper[*b >> 4];
      *s++=_dig_vec_upper[*b & 15];
    }
    *s=0;
    return buf;
  }
  uint64_t to_uint64_t() const
  {
    if (sizeof(buffer) >= 8)
      return uint8korr(buffer);
    assert(sizeof(buffer) >= 4);
    return (uint64_t) uint4korr(buffer);
  }
};

template <> class Bitmap<64>
{
  uint64_t map;
public:
  Bitmap<64>() : map(0) { }
  explicit Bitmap<64>(uint32_t prefix_to_set) : map(0) { set_prefix(prefix_to_set); }
  void init() { }
  void init(uint32_t prefix_to_set) { set_prefix(prefix_to_set); }
  uint32_t length() const { return 64; }
  void set_bit(uint32_t n) { map|= ((uint64_t)1) << n; }
  void clear_bit(uint32_t n) { map&= ~(((uint64_t)1) << n); }
  void set_prefix(uint32_t n)
  {
    if (n >= length())
      set_all();
    else
      map= (((uint64_t)1) << n)-1;
  }
  void set_all() { map=~(uint64_t)0; }
  void clear_all() { map=(uint64_t)0; }
  void intersect(Bitmap<64>& map2) { map&= map2.map; }
  void intersect(uint64_t map2) { map&= map2; }
  void intersect_extended(uint64_t map2) { map&= map2; }
  void subtract(Bitmap<64>& map2) { map&= ~map2.map; }
  void merge(Bitmap<64>& map2) { map|= map2.map; }
  bool is_set(uint32_t n) const { return test(map & (((uint64_t)1) << n)); }
  bool is_prefix(uint32_t n) const { return map == (((uint64_t)1) << n)-1; }
  bool is_clear_all() const { return map == (uint64_t)0; }
  bool is_set_all() const { return map == ~(uint64_t)0; }
  bool is_subset(const Bitmap<64>& map2) const { return !(map & ~map2.map); }
  bool is_overlapping(const Bitmap<64>& map2) const { return (map & map2.map)!= 0; }
  bool operator==(const Bitmap<64>& map2) const { return map == map2.map; }
  char *print(char *buf) const { int64_t2str(map,buf,16); return buf; }
  uint64_t to_uint64_t() const { return map; }
};


typedef uint64_t table_map;          /* Used for table bits in join */
#if MAX_INDEXES <= 64
typedef Bitmap<64>  key_map;          /* Used for finding keys */
#else
typedef Bitmap<((MAX_INDEXES+7)/8*8)> key_map; /* Used for finding keys */
#endif
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

#endif /* _SQL_BITMAP_H_ */
