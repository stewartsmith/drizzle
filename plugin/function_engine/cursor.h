/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/cursor.h>

#include <plugin/function_engine/function.h>
#include <drizzled/base.h>

class FunctionCursor: public drizzled::Cursor
{
  drizzled::plugin::TableFunction *tool;
  drizzled::plugin::TableFunction::Generator *generator;
  size_t row_cache_position;
  std::vector<unsigned char> row_cache;
  drizzled::ha_rows estimate_of_rows;
  drizzled::ha_rows rows_returned;

  void wipeCache();

  std::vector <unsigned char> record_buffer; // for pack_row
  uint32_t max_row_length();
  unsigned int pack_row(const unsigned char *record);

public:
  FunctionCursor(drizzled::plugin::StorageEngine &engine,
                 drizzled::Table &table_arg);
  ~FunctionCursor() {}

  int open(const char *name, int mode, uint32_t test_if_locked);

  int close(void);

  int reset()
  {
    return extra(drizzled::HA_EXTRA_RESET_STATE);
  }

  int doStartTableScan(bool scan);

  /* get the next row and copy it into buf */
  int rnd_next(unsigned char *buf);

  /* locate row pointed to by pos, copy it into buf */
  int rnd_pos(unsigned char *buf, unsigned char *pos);

  int doEndTableScan();

  int extra(enum drizzled::ha_extra_function);

  /* record position of a record for reordering */
  void position(const unsigned char *record);

  int info(uint32_t flag);

  /**
   * @return an upper bound estimate for the number of rows in the table
   */
  drizzled::ha_rows estimate_rows_upper_bound()
  {
    return estimate_of_rows;
  }

  void get_auto_increment(uint64_t, uint64_t,
                          uint64_t,
                          uint64_t *,
                          uint64_t *)
  {}
};

