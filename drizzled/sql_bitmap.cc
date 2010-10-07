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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "config.h"

#include <drizzled/sql_bitmap.h>
#include "drizzled/internal/m_string.h"
#include "drizzled/internal/my_bit.h"

#include <memory>

using namespace std;

namespace drizzled
{

void MyBitmap::createLastWordMask()
{
  /* Get the number of used bits (1..8) in the last byte */
  unsigned int const used= 1U + ((n_bits-1U) & 0x7U);

  /*
    Create a mask with the upper 'unused' bits set and the lower 'used'
    bits clear. The bits within each byte is stored in big-endian order.
   */
  unsigned char const mask= static_cast<unsigned char const>((~((1 << used) - 1)) & 255); 

  /*
    The first bytes are to be set to zero since they represent real  bits
    in the bitvector. The last bytes are set to 0xFF since they  represent
    bytes not used by the bitvector. Finally the last byte contains  bits
    as set by the mask above.
  */
  unsigned char *ptr= (unsigned char*)&last_word_mask;

  last_word_ptr= bitmap + numOfWordsInMap()-1;
  switch (numOfBytesInMap() & 3) 
  {
  case 1:
    last_word_mask= UINT32_MAX;
    ptr[0]= mask;
    return;
  case 2:
    last_word_mask= UINT32_MAX;
    ptr[0]= 0;
    ptr[1]= mask;
    return;
  case 3:
    last_word_mask= 0;
    ptr[2]= mask;
    ptr[3]= 0xFFU;
    return;
  case 0:
    last_word_mask= 0U;
    ptr[3]= mask;
    return;
  }
}


bool MyBitmap::init(my_bitmap_map *buf, uint32_t num_bits)
{
  if (! buf)
  {
    uint32_t size_in_bytes= bitmap_buffer_size(num_bits);
    if (! (buf= new(nothrow) my_bitmap_map[size_in_bytes]()))
    {
      return true;
    }
  }

  bitmap= buf;
  n_bits= num_bits;
  createLastWordMask();
  clearAll();

  return false;
}


bool MyBitmap::testAndSet(const uint32_t bitPos)
{
  unsigned char *value= ((unsigned char*) bitmap) + (bitPos / 8);
  unsigned char bit= static_cast<unsigned char>(1 << ((bitPos) & 7));
  unsigned char res= static_cast<unsigned char>((*value) & bit);
  *value= static_cast<unsigned char>(*value | bit);
  return res;
}



bool MyBitmap::testAndClear(const uint32_t bitPos)
{
  unsigned char *byte= (unsigned char*) bitmap + (bitPos / 8);
  unsigned char bit= static_cast<unsigned char>(1 << ((bitPos) & 7));
  unsigned char res= static_cast<unsigned char>((*byte) & bit);
  *byte= static_cast<unsigned char>(*byte & ~bit);
  return res;
}



uint32_t MyBitmap::setNext()
{
  uint32_t bit_found;
  assert(bitmap);
  if ((bit_found= getFirst()) != MY_BIT_NONE)
  {
    setBit(bit_found);
  }
  return bit_found;
}


void MyBitmap::setPrefix(uint32_t prefix_size)
{
  uint32_t prefix_bytes, prefix_bits, d;
  unsigned char *m= (unsigned char *) bitmap;

  assert(bitmap &&
	 (prefix_size <= n_bits || prefix_size == UINT32_MAX));
  set_if_smaller(prefix_size, n_bits);
  if ((prefix_bytes= prefix_size / 8))
  {
    memset(m, 0xff, prefix_bytes);
  }
  m+= prefix_bytes;
  if ((prefix_bits= prefix_size & 7))
  {
    *m++= static_cast<unsigned char>((1 << prefix_bits)-1);
  }
  if ((d= numOfBytesInMap() - prefix_bytes))
  {
    memset(m, 0, d);
  }
}


bool MyBitmap::isPrefix(const uint32_t prefix_size) const
{
  uint32_t prefix_bits= prefix_size & 0x7, res;
  unsigned char *m= (unsigned char*) bitmap;
  unsigned char *end_prefix= m + prefix_size/8;
  unsigned char *end;
  assert(m && prefix_size <= n_bits);
  end= m + numOfBytesInMap();

  while (m < end_prefix)
  {
    if (*m++ != 0xff)
    {
      return 0;
    }
  }

  *last_word_ptr&= ~last_word_mask; /*Clear bits*/
  res= 0;
  if (prefix_bits && *m++ != (1 << prefix_bits)-1)
  {
    goto ret;
  }

  while (m < end)
  {
    if (*m++ != 0)
    {
      goto ret;
    }
  }
  res= 1;
ret:
  return res;
}


bool MyBitmap::isSetAll() const
{
  my_bitmap_map *data_ptr= bitmap;
  my_bitmap_map *end= last_word_ptr;
  *last_word_ptr |= last_word_mask;
  for (; data_ptr <= end; data_ptr++)
  {
    if (*data_ptr != 0xFFFFFFFF)
    {
      return false;
    }
  }
  return true;
}


bool MyBitmap::isClearAll() const
{
  my_bitmap_map *data_ptr= bitmap;
  my_bitmap_map *end;
  if (*last_word_ptr & ~last_word_mask)
  {
    return false;
  }
  end= last_word_ptr;
  for (; data_ptr < end; data_ptr++)
  {
    if (*data_ptr)
    {
      return false;
    }
  }
  return true;
}

/* Return true if map1 is a subset of map2 */

bool bitmap_is_subset(const MyBitmap *map1, const MyBitmap *map2)
{
  my_bitmap_map *m1= map1->getBitmap(), *m2= map2->getBitmap(), *end;

  assert(map1->getBitmap() && map2->getBitmap() &&
         map1->numOfBitsInMap() == map2->numOfBitsInMap());

  end= map1->getLastWordPtr();
  map1->subtractMaskFromLastWord();
  map2->subtractMaskFromLastWord();
  while (m1 <= end)
  {
    if ((*m1++) & ~(*m2++))
    {
      return 0;
    }
  }
  return 1;
}

/* True if bitmaps has any common bits */

bool bitmap_is_overlapping(const MyBitmap *map1, const MyBitmap *map2)
{
  my_bitmap_map *m1= map1->getBitmap(), *m2= map2->getBitmap(), *end;

  assert(map1->getBitmap() && map2->getBitmap() &&
         map1->numOfBitsInMap() == map2->numOfBitsInMap());

  end= map1->getLastWordPtr();
  map1->subtractMaskFromLastWord();
  map2->subtractMaskFromLastWord();
  while (m1 <= end)
  {
    if ((*m1++) & (*m2++))
      return 1;
  }
  return 0;
}


void bitmap_intersect(MyBitmap *map, const MyBitmap *map2)
{
  my_bitmap_map *to= map->getBitmap(), *from= map2->getBitmap(), *end;
  uint32_t len= map->numOfWordsInMap(), len2 = map2->numOfWordsInMap();

  assert(map->getBitmap() && map2->getBitmap());

  end= to+min(len,len2);
  map2->subtractMaskFromLastWord(); /* Clear last bits in map2 */
  while (to < end)
  {
    *to++ &= *from++;
  }

  if (len2 < len)
  {
    end+=len-len2;
    while (to < end)
    {
      *to++=0;
    }
  }
}


void MyBitmap::setAbove(const uint32_t from_byte, const uint32_t use_bit)
{
  unsigned char use_byte= static_cast<unsigned char>(use_bit ? 0xff : 0);
  unsigned char *to= (unsigned char *) bitmap + from_byte;
  unsigned char *end= (unsigned char *) bitmap + (n_bits+7)/8;

  while (to < end)
  {
    *to++= use_byte;
  }
}


void bitmap_subtract(MyBitmap *map, const MyBitmap *map2)
{
  my_bitmap_map *to= map->getBitmap(), *from= map2->getBitmap(), *end;
  assert(map->getBitmap() && map2->getBitmap() &&
         map->numOfBitsInMap() == map2->numOfBitsInMap());

  end= map->getLastWordPtr();

  while (to <= end)
  {
    *to++ &= ~(*from++);
  }
}


void bitmap_union(MyBitmap *map, const MyBitmap *map2)
{
  my_bitmap_map *to= map->getBitmap(), *from= map2->getBitmap(), *end;

  assert(map->getBitmap() && map2->getBitmap() &&
         map->numOfBitsInMap() == map2->numOfBitsInMap());
  end= map->getLastWordPtr();

  while (to <= end)
  {
    *to++ |= *from++;
  }
}


void bitmap_xor(MyBitmap *map, const MyBitmap *map2)
{
  my_bitmap_map *to= map->getBitmap();
  my_bitmap_map *from= map2->getBitmap();
  my_bitmap_map *end= map->getLastWordPtr();
  assert(map->getBitmap() && map2->getBitmap() &&
         map->numOfBitsInMap() == map2->numOfBitsInMap());
  while (to <= end)
  {
    *to++ ^= *from++;
  }
}


void bitmap_invert(MyBitmap *map)
{
  my_bitmap_map *to= map->getBitmap(), *end;

  assert(map->getBitmap());
  end= map->getLastWordPtr();

  while (to <= end)
  {
    *to++ ^= 0xFFFFFFFF;
  }
}


uint32_t MyBitmap::getBitsSet()
{
  unsigned char *m= (unsigned char*) bitmap;
  unsigned char *end= m + numOfBytesInMap();
  uint32_t res= 0;

  assert(bitmap);
  *last_word_ptr&= ~last_word_mask; /*Reset last bits to zero*/
  while (m < end)
  {
    res+= internal::my_count_bits_uint16(*m++);
  }
  return res;
}

MyBitmap::MyBitmap(const MyBitmap& rhs)
{
  my_bitmap_map *to= this->bitmap, *from= rhs.bitmap, *end;

  if (this->bitmap && rhs.bitmap &&
      this->n_bits == rhs.n_bits)
  {
    end= this->last_word_ptr;
    while (to <= end)
    {
      *to++ = *from++;
    }
  }
  else
  {
    this->n_bits= rhs.n_bits;
    this->bitmap= rhs.bitmap;
  }
}

MyBitmap& MyBitmap::operator=(const MyBitmap& rhs)
{
  if (this == &rhs)
    return *this;

  my_bitmap_map *to= this->bitmap, *from= rhs.bitmap, *end;

  if (this->bitmap && rhs.bitmap &&
      this->n_bits == rhs.n_bits)
  {
    end= this->last_word_ptr;
    while (to <= end)
    {
      *to++ = *from++;
    }
  }
  else
  {
    this->n_bits= rhs.n_bits;
    this->bitmap= rhs.bitmap;
  }

  return *this;
}

uint32_t MyBitmap::getFirstSet()
{
  unsigned char *byte_ptr;
  uint32_t i,j,k;
  my_bitmap_map *data_ptr, *end= last_word_ptr;

  assert(bitmap);
  data_ptr= bitmap;
  *last_word_ptr &= ~last_word_mask;

  for (i=0; data_ptr <= end; data_ptr++, i++)
  {
    if (*data_ptr)
    {
      byte_ptr= (unsigned char*)data_ptr;
      for (j=0; ; j++, byte_ptr++)
      {
        if (*byte_ptr)
        {
          for (k=0; ; k++)
          {
            if (*byte_ptr & (1 << k))
              return (i*32) + (j*8) + k;
          }
        }
      }
    }
  }
  return MY_BIT_NONE;
}


uint32_t MyBitmap::getFirst()
{
  unsigned char *byte_ptr;
  uint32_t i,j,k;
  my_bitmap_map *data_ptr, *end= last_word_ptr;

  assert(bitmap);
  data_ptr= bitmap;
  *last_word_ptr|= last_word_mask;

  for (i=0; data_ptr <= end; data_ptr++, i++)
  {
    if (*data_ptr != 0xFFFFFFFF)
    {
      byte_ptr= (unsigned char*)data_ptr;
      for (j=0; ; j++, byte_ptr++)
      {
        if (*byte_ptr != 0xFF)
        {
          for (k=0; ; k++)
          {
            if (!(*byte_ptr & (1 << k)))
              return (i*32) + (j*8) + k;
          }
        }
      }
    }
  }
  return MY_BIT_NONE;
}


#ifdef MAIN

uint32_t get_rand_bit(uint32_t bitsize)
{
  return (rand() % bitsize);
}

bool test_set_get_clear_bit(MyBitmap *map, uint32_t bitsize)
{
  uint32_t i, test_bit;
  uint32_t no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit= get_rand_bit(bitsize);
    map->setBit(test_bit);
    if (! map->isBitSet(test_bit))
    {
      goto error1;
    }
    map->clearBit(test_bit);
    if (map->isBitSet(test_bit))
    {
      goto error2;
    }
  }
  return false;
error1:
  printf("Error in set bit, bit %u, bitsize = %u", test_bit, bitsize);
  return true;
error2:
  printf("Error in clear bit, bit %u, bitsize = %u", test_bit, bitsize);
  return true;
}

bool test_flip_bit(MyBitmap *map, uint32_t bitsize)
{
  uint32_t i, test_bit;
  uint32_t no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit= get_rand_bit(bitsize);
    map->flipBit(test_bit);
    if (!map->isBitSet(test_bit))
      goto error1;
    map->flipBit(test_bit);
    if (map->isBitSet(test_bit))
      goto error2;
  }
  return false;
error1:
  printf("Error in flip bit 1, bit %u, bitsize = %u", test_bit, bitsize);
  return true;
error2:
  printf("Error in flip bit 2, bit %u, bitsize = %u", test_bit, bitsize);
  return true;
}

bool test_operators(MyBitmap *, uint32_t)
{
  return false;
}

bool test_get_all_bits(MyBitmap *map, uint32_t bitsize)
{
  uint32_t i;
  map->setAll();
  if (!map->isSetAll())
    goto error1;
  if (!map->isPrefix(bitsize))
    goto error5;
  map->clearAll();
  if (!map->isClearAll())
    goto error2;
  if (!map->isPrefix(0))
    goto error6;
  for (i=0; i<bitsize;i++)
    map->setBit(i);
  if (!map->isSetAll())
    goto error3;
  for (i=0; i<bitsize;i++)
    map->clearBit(i);
  if (!map->isClearAll())
    goto error4;
  return false;
error1:
  printf("Error in set_all, bitsize = %u", bitsize);
  return true;
error2:
  printf("Error in clear_all, bitsize = %u", bitsize);
  return true;
error3:
  printf("Error in bitmap_is_set_all, bitsize = %u", bitsize);
  return true;
error4:
  printf("Error in bitmap_is_clear_all, bitsize = %u", bitsize);
  return true;
error5:
  printf("Error in set_all through set_prefix, bitsize = %u", bitsize);
  return true;
error6:
  printf("Error in clear_all through set_prefix, bitsize = %u", bitsize);
  return true;
}

bool test_compare_operators(MyBitmap *map, uint32_t bitsize)
{
  uint32_t i, j, test_bit1, test_bit2, test_bit3,test_bit4;
  uint32_t no_loops= bitsize > 128 ? 128 : bitsize;
  MyBitmap map2_obj, map3_obj;
  MyBitmap *map2= &map2_obj, *map3= &map3_obj;
  my_bitmap_map map2buf[1024];
  my_bitmap_map map3buf[1024];
  map2_obj.init(map2buf, bitsize);
  map3_obj.init(map3buf, bitsize);
  map2->clearAll();
  map3->clearAll();
  for (i=0; i < no_loops; i++)
  {
    test_bit1=get_rand_bit(bitsize);
    map->setPrefix(test_bit1);
    test_bit2=get_rand_bit(bitsize);
    map2->setPrefix(test_bit2);
    bitmap_intersect(map, map2);
    test_bit3= test_bit2 < test_bit1 ? test_bit2 : test_bit1;
    map3->setPrefix(test_bit3);
    if (!bitmap_cmp(map, map3))
      goto error1;
    map->clearAll();
    map2->clearAll();
    map3->clearAll();
    test_bit1=get_rand_bit(bitsize);
    test_bit2=get_rand_bit(bitsize);
    test_bit3=get_rand_bit(bitsize);
    map->setPrefix(test_bit1);
    map2->setPrefix(test_bit2);
    test_bit3= test_bit2 > test_bit1 ? test_bit2 : test_bit1;
    map3->setPrefix(test_bit3);
    bitmap_union(map, map2);
    if (!bitmap_cmp(map, map3))
      goto error2;
    map->clearAll();
    map2->clearAll();
    map3->clearAll();
    test_bit1=get_rand_bit(bitsize);
    test_bit2=get_rand_bit(bitsize);
    test_bit3=get_rand_bit(bitsize);
    map->setPrefix(test_bit1);
    map2->setPrefix(test_bit2);
    bitmap_xor(map, map2);
    test_bit3= test_bit2 > test_bit1 ? test_bit2 : test_bit1;
    test_bit4= test_bit2 < test_bit1 ? test_bit2 : test_bit1;
    map3->setPrefix(test_bit3);
    for (j=0; j < test_bit4; j++)
      map3->clearBit(j);
    if (!bitmap_cmp(map, map3))
      goto error3;
    map->clearAll();
    map2->clearAll();
    map3->clearAll();
    test_bit1=get_rand_bit(bitsize);
    test_bit2=get_rand_bit(bitsize);
    test_bit3=get_rand_bit(bitsize);
    map->setPrefix(test_bit1);
    map2->setPrefix(test_bit2);
    bitmap_subtract(map, map2);
    if (test_bit2 < test_bit1)
    {
      map3->setPrefix(test_bit1);
      for (j=0; j < test_bit2; j++)
        map3->clearBit(j);
    }
    if (!bitmap_cmp(map, map3))
      goto error4;
    map->clearAll();
    map2->clearAll();
    map3->clearAll();
    test_bit1=get_rand_bit(bitsize);
    map->setPrefix(test_bit1);
    bitmap_invert(map);
    map3->setAll();
    for (j=0; j < test_bit1; j++)
      map3->clearBit(j);
    if (!bitmap_cmp(map, map3))
      goto error5;
    map->clearAll();
    map3->clearAll();
  }
  return false;
error1:
  printf("intersect error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return true;
error2:
  printf("union error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return true;
error3:
  printf("xor error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return true;
error4:
  printf("subtract error  bitsize=%u,size1=%u,size2=%u", bitsize,
  test_bit1,test_bit2);
  return true;
error5:
  printf("invert error  bitsize=%u,size=%u", bitsize,
  test_bit1);
  return true;
}

bool test_count_bits_set(MyBitmap *map, uint32_t bitsize)
{
  uint32_t i, bit_count=0, test_bit;
  uint32_t no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    if (!map->isBitSet(test_bit))
    {
      map->setBit(test_bit);
      bit_count++;
    }
  }
  if (bit_count==0 && bitsize > 0)
    goto error1;
  if (getBitsSet() != bit_count)
    goto error2;
  return false;
error1:
  printf("No bits set  bitsize = %u", bitsize);
  return true;
error2:
  printf("Wrong count of bits set, bitsize = %u", bitsize);
  return true;
}

bool test_get_first_bit(MyBitmap *map, uint32_t bitsize)
{
  uint32_t i, test_bit;
  uint32_t no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    map->setBit(test_bit);
    if (bitmap_get_first_set(map) != test_bit)
      goto error1;
    map->setAll();
    map->clearBit(test_bit);
    if (getFirst() != test_bit)
      goto error2;
    map->clearAll();
  }
  return false;
error1:
  printf("get_first_set error bitsize=%u,prefix_size=%u",bitsize,test_bit);
  return true;
error2:
  printf("get_first error bitsize= %u, prefix_size= %u",bitsize,test_bit);
  return true;
}

bool test_get_next_bit(MyBitmap *map, uint32_t bitsize)
{
  uint32_t i, j, test_bit;
  uint32_t no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    for (j=0; j < test_bit; j++)
      setNext();
    if (!map->isPrefix(test_bit))
      goto error1;
    map->clearAll();
  }
  return false;
error1:
  printf("get_next error  bitsize= %u, prefix_size= %u", bitsize,test_bit);
  return true;
}

bool test_prefix(MyBitmap *map, uint32_t bitsize)
{
  uint32_t i, j, test_bit;
  uint32_t no_loops= bitsize > 128 ? 128 : bitsize;
  for (i=0; i < no_loops; i++)
  {
    test_bit=get_rand_bit(bitsize);
    map->setPrefix(map, test_bit);
    if (!map->isPrefix(test_bit))
      goto error1;
    map->clearAll();
    for (j=0; j < test_bit; j++)
      map->setBit(j);
    if (!map->isPrefix(test_bit))
      goto error2;
    map->setAll();
    for (j=bitsize - 1; ~(j-test_bit); j--)
      map->clearBit(j);
    if (!map->isPrefix(test_bit))
      goto error3;
    map->clearAll();
  }
  return false;
error1:
  printf("prefix1 error  bitsize = %u, prefix_size = %u", bitsize,test_bit);
  return true;
error2:
  printf("prefix2 error  bitsize = %u, prefix_size = %u", bitsize,test_bit);
  return true;
error3:
  printf("prefix3 error  bitsize = %u, prefix_size = %u", bitsize,test_bit);
  return true;
}


bool do_test(uint32_t bitsize)
{
  MyBitmap map;
  my_bitmap_map buf[1024];
  if (map.init(buf, bitsize))
  {
    printf("init error for bitsize %d", bitsize);
    goto error;
  }
  if (test_set_get_clear_bit(&map,bitsize))
    goto error;
  map.clearAll();
  if (test_flip_bit(&map,bitsize))
    goto error;
  map.clearAll();
  if (test_operators(&map,bitsize))
    goto error;
  map.clearAll();
  if (test_get_all_bits(&map, bitsize))
    goto error;
  map.clearAll();
  if (test_compare_operators(&map,bitsize))
    goto error;
  map.clearAll();
  if (test_count_bits_set(&map,bitsize))
    goto error;
  map.clearAll();
  if (test_get_first_bit(&map,bitsize))
    goto error;
  map.clearAll();
  if (test_get_next_bit(&map,bitsize))
    goto error;
  if (test_prefix(&map,bitsize))
    goto error;
  return false;
error:
  printf("\n");
  return true;
}

int main()
{
  int i;
  for (i= 1; i < 4096; i++)
  {
    printf("Start test for bitsize=%u\n",i);
    if (do_test(i))
      return -1;
  }
  printf("OK\n");
  return 0;
}

/*
  In directory mysys:
  make test_bitmap
  will build the bitmap tests and ./test_bitmap will execute it
*/


#endif

} /* namespace drizzled */
