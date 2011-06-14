/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008, 2009 Sun Microsystems, Inc.
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

/*
  Some useful bit functions
*/

#pragma once

namespace drizzled
{
namespace internal
{

extern const char _my_bits_nbits[256];
extern const unsigned char _my_bits_reverse_table[256];

/*
  Find smallest X in 2^X >= value
  This can be used to divide a number with value by doing a shift instead
*/

static inline uint32_t my_bit_log2(uint32_t value)
{
  uint32_t bit;
  for (bit=0 ; value > 1 ; value>>=1, bit++) ;
  return bit;
}

static inline uint32_t my_count_bits(uint64_t v)
{
  /* The following code is a bit faster on 16 bit machines than if we would
     only shift v */
  uint32_t v2=(uint32_t) (v >> 32);
  return (uint32_t) (unsigned char) (_my_bits_nbits[(unsigned char)  v] +
                         _my_bits_nbits[(unsigned char) (v >> 8)] +
                         _my_bits_nbits[(unsigned char) (v >> 16)] +
                         _my_bits_nbits[(unsigned char) (v >> 24)] +
                         _my_bits_nbits[(unsigned char) (v2)] +
                         _my_bits_nbits[(unsigned char) (v2 >> 8)] +
                         _my_bits_nbits[(unsigned char) (v2 >> 16)] +
                         _my_bits_nbits[(unsigned char) (v2 >> 24)]);
}

static inline uint32_t my_count_bits_uint16(uint16_t v)
{
  return _my_bits_nbits[v];
}


static inline uint32_t my_clear_highest_bit(uint32_t v)
{
  uint32_t w=v >> 1;
  w|= w >> 1;
  w|= w >> 2;
  w|= w >> 4;
  w|= w >> 8;
  w|= w >> 16;
  return v & w;
}

static inline uint32_t my_reverse_bits(uint32_t key)
{
  return
    (_my_bits_reverse_table[ key      & 255] << 24) |
    (_my_bits_reverse_table[(key>> 8) & 255] << 16) |
    (_my_bits_reverse_table[(key>>16) & 255] <<  8) |
     _my_bits_reverse_table[(key>>24)      ];
}

} /* namespace internal */
} /* namespace drizzled */

