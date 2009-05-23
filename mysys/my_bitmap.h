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

#ifndef MYSYS_MY_BITMAP_H
#define MYSYS_MY_BITMAP_H

#include <pthread.h>
#include <string.h>

static const uint32_t MY_BIT_NONE= UINT32_MAX;

typedef uint32_t my_bitmap_map;

typedef struct st_bitmap
{
  my_bitmap_map *bitmap;
  uint32_t n_bits; /* number of bits occupied by the above */
  my_bitmap_map last_word_mask;
  my_bitmap_map *last_word_ptr;
  st_bitmap()
    : bitmap(NULL), n_bits(0), last_word_mask(0), last_word_ptr(NULL) {}
} MY_BITMAP;

void create_last_word_mask(MY_BITMAP *map);
bool bitmap_init(MY_BITMAP *map, my_bitmap_map *buf, uint32_t n_bits);
bool bitmap_is_clear_all(const MY_BITMAP *map);
bool bitmap_is_prefix(const MY_BITMAP *map, uint32_t prefix_size);
bool bitmap_is_set_all(const MY_BITMAP *map);
bool bitmap_is_subset(const MY_BITMAP *map1, const MY_BITMAP *map2);
bool bitmap_is_overlapping(const MY_BITMAP *map1,
                                     const MY_BITMAP *map2);
bool bitmap_test_and_set(MY_BITMAP *map, uint32_t bitmap_bit);
bool bitmap_test_and_clear(MY_BITMAP *map, uint32_t bitmap_bit);
uint32_t bitmap_set_next(MY_BITMAP *map);
uint32_t bitmap_get_first(const MY_BITMAP *map);
uint32_t bitmap_get_first_set(const MY_BITMAP *map);
uint32_t bitmap_bits_set(const MY_BITMAP *map);
void bitmap_free(MY_BITMAP *map);
void bitmap_set_above(MY_BITMAP *map, uint32_t from_byte, uint32_t use_bit);
void bitmap_set_prefix(MY_BITMAP *map, uint32_t prefix_size);
void bitmap_intersect(MY_BITMAP *map, const MY_BITMAP *map2);
void bitmap_subtract(MY_BITMAP *map, const MY_BITMAP *map2);
void bitmap_union(MY_BITMAP *map, const MY_BITMAP *map2);
void bitmap_xor(MY_BITMAP *map, const MY_BITMAP *map2);
void bitmap_invert(MY_BITMAP *map);
void bitmap_copy(MY_BITMAP *map, const MY_BITMAP *map2);


/* Fast, not thread safe, bitmap functions */
/* This one is a define because it gets used in an array decl */
#define bitmap_buffer_size(bits) ((bits+31)/32)*4

static inline uint32_t no_bytes_in_map(const MY_BITMAP *map)
{
  return ((map->n_bits + 7)/8);
}

static inline uint32_t no_words_in_map(const MY_BITMAP *map)
{
  return ((map->n_bits + 31)/32);
}

template <class T>
inline T bytes_word_aligned(T bytes)
{
  return (4*((bytes + 3)/4));
}

static inline void bitmap_set_bit(MY_BITMAP const *map, uint32_t bit)
{
  ((unsigned char *)map->bitmap)[bit / 8] |= (1 << ((bit) & 7));
}

static inline void bitmap_flip_bit(MY_BITMAP const *map, uint32_t bit)
{
  ((unsigned char *)map->bitmap)[bit / 8] ^= (1 << ((bit) & 7));
}

static inline void bitmap_clear_bit(MY_BITMAP const *map, uint32_t bit)
{
  ((unsigned char *)map->bitmap)[bit / 8] &= ~ (1 << ((bit) & 7));
}

static inline bool bitmap_is_set(const MY_BITMAP *map, uint32_t bit)
{
  return (bool)(((unsigned char *)map->bitmap)[bit / 8] &  (1 << ((bit) & 7)));
}

static inline bool bitmap_cmp(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  *(map1)->last_word_ptr|= (map1)->last_word_mask;
  *(map2)->last_word_ptr|= (map2)->last_word_mask;
  return memcmp((map1)->bitmap, (map2)->bitmap, 4*no_words_in_map((map1)))==0;
}

static inline void bitmap_clear_all(MY_BITMAP const *map)
{
   memset(map->bitmap, 0, 4*no_words_in_map(map));
}

static inline void bitmap_set_all(MY_BITMAP const *map)
{
   memset(map->bitmap, 0xFF, 4*no_words_in_map(map));
}

#endif /* MYSYS_MY_BITMAP_H */
