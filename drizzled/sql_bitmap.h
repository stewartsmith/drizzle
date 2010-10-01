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

typedef uint32_t my_bitmap_map;

/**
 * @class MyBitmap
 * @brief Represents a dynamically sized bitmap.
 *
 * Handling of unsigned char arrays as large bitmaps.
 *
 * API limitations (or, rather asserted safety assumptions,
 * to encourage correct programming)
 *   - the internal size is a set of 32 bit words
 *   - the number of bits specified in creation can be any number
 *     greater than 0
 *
 * Original version created by Sergei Golubchik 2001 - 2004.
 * New version written and test program added and some changes to the
 * interface was made by Mikael Ronström 2005, with assistance of Tomas
 * Ulin and Mats Kindahl.
 */
class MyBitmap
{
public:
  MyBitmap()
    :
      bitmap(NULL),
      n_bits(0),
      last_word_mask(0),
      last_word_ptr(NULL)
  {}

  MyBitmap(const MyBitmap& rhs);

  ~MyBitmap()
  {
      /*
       * We are not de-allocating memory here correctly at the moment!!!!
       * Placing a delete [] statement here causes bad bad things to
       * happen......
       * Placing this comment here in the hope that someone can help me
       * resolve this issue....
       */
      bitmap= 0;
  }

  MyBitmap& operator=(const MyBitmap& rhs);

  bool init(my_bitmap_map *buf, uint32_t num_bits);

  /**
   * Compare the given bitmap with this bitmap.
   *
   * @param[in] map bitmap to perform a comparison against
   * @return true if bitmaps are equal; false otherwise
   */
  bool operator==(const MyBitmap *map)
  {
    *last_word_ptr|= last_word_mask;
    *(map)->last_word_ptr|= (map)->last_word_mask;
    return memcmp(bitmap, map->getBitmap(), 4*numOfWordsInMap())==0;
  }

  /**
   * Test whether the specified bit is set in the bitmap
   * already and set it if it was not already set.
   *
   * @param[in] bitPos position to test and set
   * @return false if the bit was not set; true otherwise
   */
  bool testAndSet(const uint32_t bitPos);

  /**
   * Test whether the specified bit is set in the bitmap
   * and clear it if it was set.
   *
   * @param[in] bitPos position to test and clear
   * @return false if the bit was not set; true otherwise
   */
  bool testAndClear(const uint32_t bitPos);

  /**
   * Set the next bit position that is not yet set starting
   * from the beginning of the bitmap.
   *
   * @return the bit position that was set
   */
  uint32_t setNext();

  uint32_t getFirst();

  /**
   * @return the position of the first bit set.
   */
  uint32_t getFirstSet();

  /**
   * @return the bits set in this bitmap.
   */
  uint32_t getBitsSet();

  /**
   * Set/clear all bits above a bit.
   * Only full bytes can be set/cleared.
   * The function is meant for the situation that you can copy
   * a smaller bitmap to a bigger bitmap. Bitmap lengths are
   * always a multiple of 8 (byte size). Using 'from_byte' saves
   * multiplication and division by 8 during parameter passing.
   *
   * @param[in] from_byte bitmap buffer byte offset to start with
   * @param[in] use_bit bit value (1/0) to use for all upper bits
   */
  void setAbove(const uint32_t from_byte, const uint32_t use_bit);


  /**
   * Test whether all the bits in the bitmap are clear or not.
   *
   * @return true if all bits in bitmap are cleared; false otherwise
   */
  bool isClearAll() const;

  bool isPrefix(const uint32_t prefix_size) const;

  void setPrefix(uint32_t prefix_size);

  /**
   * Test whether all the bits in the bitmap are set or not.
   *
   * @return true if all bits in bitmap are set; false otherwise
   */
  bool isSetAll() const;


  /**
   * Test if the specified bit is set or not.
   *
   * @param[in] bit position of bit to test
   * @return true if the bit is set; false otherwise
   */
  bool isBitSet(const uint32_t bit) const
  {
     return (bool)((reinterpret_cast<unsigned char *>(bitmap))[bit / 8] &  (1 << ((bit) & 7)));
  }

  /**
   * Set the specified bit.
   *
   * @param[in] bit position of bit to set in bitmap
   */
  void setBit(const uint32_t bit)
  {
    reinterpret_cast<unsigned char *>(bitmap)[bit / 8]=
      static_cast<unsigned char>(
      (reinterpret_cast<unsigned char *>(bitmap))[bit / 8] |
      (1 << ((bit) & 7)));
  }

  /**
   * Flip the specified bit.
   *
   * @param[in] bit position of bit to flip in bitmap
   */
  void flipBit(const uint32_t bit)
  {
    reinterpret_cast<unsigned char *>(bitmap)[bit / 8]=
      static_cast<unsigned char>(
      (reinterpret_cast<unsigned char *>(bitmap))[bit / 8] ^
      (1 << ((bit) & 7)));
  }

  /**
   * Clear the specified bit.
   *
   * @param[in] bit position of bit to clear in bitmap
   */
  void clearBit(const uint32_t bit)
  {
    reinterpret_cast<unsigned char *>(bitmap)[bit / 8]=
      static_cast<unsigned char>(
      (reinterpret_cast<unsigned char *>(bitmap))[bit / 8] &
      ~ (1 << ((bit) & 7)));
  }

  /**
   * Clear all the bits in the bitmap i.e. set every
   * bit to 0.
   */
  void clearAll()
  {
     memset(bitmap, 0, 4*numOfWordsInMap());
  }

  /**
   * Set all the bits in the bitmap i.e. set every
   * bit to 1.
   */
  void setAll()
  {
     memset(bitmap, 0xFF, 4*numOfWordsInMap());
  }

  /**
   * @return the number of words in this bitmap.
   */
  uint32_t numOfWordsInMap() const
  {
    return ((n_bits + 31)/32);
  }

  /**
   * @return the number of bytes in this bitmap.
   */
  uint32_t numOfBytesInMap() const
  {
    return ((n_bits + 7)/8);
  }

  /**
   * Get the size of the bitmap in bits.
   *
   * @return the size of the bitmap in bits.
   */
  uint32_t numOfBitsInMap() const
  {
    return n_bits;
  }

  /**
   * @return the current bitmap
   */
  my_bitmap_map *getBitmap() const
  {
    return bitmap;
  }

  /**
   * Set the bitmap to the specified bitmap.
   *
   * @param[in] new_bitmap bitmap to use
   */
  void setBitmap(my_bitmap_map *new_bitmap)
  {
    bitmap= new_bitmap;
  }

  /**
   * Obtains the number of used bits (1..8) in the last
   * byte and creates a mask with the upper 'unused' bits
   * set and the lower 'used' bits clear.
   */
  void createLastWordMask();

  /**
   * @return the last word pointer for this bitmap
   */
  my_bitmap_map *getLastWordPtr() const
  {
    return last_word_ptr;
  }

  void addMaskToLastWord() const
  {
    *last_word_ptr|= last_word_mask;
  }

  /**
   * This resets the last bits in the bitmap to 0.
   */
  void subtractMaskFromLastWord() const
  {
    *last_word_ptr&= ~last_word_mask;
  }

private:

  /**
   * The bitmap is stored in this variable.
   */
  my_bitmap_map *bitmap;

  /**
   * Number of bits occupied by the bitmap.
   */
  uint32_t n_bits;

  /**
   * A mask used for the last word in the bitmap.
   */
  my_bitmap_map last_word_mask;

  /**
   * A pointer to the last word in the bitmap.
   */
  my_bitmap_map *last_word_ptr;

};

bool bitmap_is_subset(const MyBitmap *map1, const MyBitmap *map2);
/** temporary until MyBitmap goes away */
bool bitmap_is_subset(const MyBitmap *map1, const boost::dynamic_bitset<>& map2);
bool bitmap_is_overlapping(const MyBitmap *map1,
                           const MyBitmap *map2);

void bitmap_intersect(MyBitmap *map, const MyBitmap *map2);
void bitmap_subtract(MyBitmap *map, const MyBitmap *map2);
/** temporary function until MyBitmap is replaced */
void bitmap_subtract(boost::dynamic_bitset<>& map, const MyBitmap *map2);
void bitmap_union(MyBitmap *map, const MyBitmap *map2);
/** temporary functions until MyBitmap is replaced */
void bitmap_union(MyBitmap *map, const boost::dynamic_bitset<>& map2);
void bitmap_union(boost::dynamic_bitset<>& map, const MyBitmap *map2);
void bitmap_xor(MyBitmap *map, const MyBitmap *map2);
void bitmap_invert(MyBitmap *map);

/* Fast, not thread safe, bitmap functions */
/* This one is a define because it gets used in an array decl */
#define bitmap_buffer_size(bits) ((bits+31)/32)*4

template <class T>
inline T bytes_word_aligned(T bytes)
{
  return (4*((bytes + 3)/4));
}

static inline bool bitmap_cmp(const MyBitmap *map1, const MyBitmap *map2)
{
  map1->addMaskToLastWord();
  map2->addMaskToLastWord();
  return memcmp((map1)->getBitmap(),
                (map2)->getBitmap(),
                4*map1->numOfWordsInMap()) == 0;
}

} /* namespace drizzled */

#endif /* DRIZZLED_SQL_BITMAP_H */
