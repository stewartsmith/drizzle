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

#ifndef DRIZZLED_TABLE_MAP_ITERATOR_H
#define DRIZZLED_TABLE_MAP_ITERATOR_H

#include CSTDINT_H

/* An iterator to quickly walk over bits in unint64_t bitmap. */
class Table_map_iterator
{
  uint64_t bmp;
  uint32_t no;
public:
  Table_map_iterator(uint64_t t) : bmp(t), no(0) {}
  int next_bit();
  enum { BITMAP_END= 64 };
};

#endif
