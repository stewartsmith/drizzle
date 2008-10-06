/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Brian Aker
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

#ifndef DRIZZLED_SJ_TMP_TABLE_H
#define DRIZZLED_SJ_TMP_TABLE_H

#include <drizzled/server_includes.h>
#include "table.h"

/*
  Describes use of one temporary table to weed out join duplicates.
  The temporar

  Used to
    - create a temp table
    - when we reach the weed-out tab, walk through rowid-ed tabs and
      and copy rowids.
      For each table we need
       - rowid offset
       - null bit address.
*/

class SJ_TMP_TABLE : public Sql_alloc
{
public:
  /* Array of pointers to tables that should be "used" */
  class TAB
  {
  public:
    struct st_join_table *join_tab;
    uint32_t rowid_offset;
    uint16_t null_byte;
    unsigned char null_bit;
  };
  TAB *tabs;
  TAB *tabs_end;

  uint32_t null_bits;
  uint32_t null_bytes;
  uint32_t rowid_len;

  Table *tmp_table;

  MI_COLUMNDEF *start_recinfo;
  MI_COLUMNDEF *recinfo;

  /* Pointer to next table (next->start_idx > this->end_idx) */
  SJ_TMP_TABLE *next; 
};

Table *create_duplicate_weedout_tmp_table(THD *thd, 
					  uint32_t uniq_tuple_length_arg,
					  SJ_TMP_TABLE *sjtbl);

#endif /* DRIZZLED_SJ_TMP_TABLE_H */
