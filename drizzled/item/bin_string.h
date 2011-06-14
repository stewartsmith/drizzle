/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/charset.h>
#include <drizzled/item/hex_string.h>

#include <drizzled/visibility.h>

namespace drizzled
{

class Item_bin_string: public Item_hex_string
{
public:
  Item_bin_string(const char *str,uint32_t str_length);
};

class DRIZZLED_API Item_result_field :
  public Item    /* Item with result field */
{
public:
  Field *result_field;                          /* Save result here */
  Item_result_field() :result_field(0) {}
  // Constructor used for Item_sum/Item_cond_and/or (see Item comment)
  Item_result_field(Session *session, Item_result_field *item):
    Item(session, item), result_field(item->result_field)
  {}
  ~Item_result_field() {}                       /* Required with gcc 2.95 */
  Field *get_tmp_table_field() { return result_field; }
  Field *tmp_table_field(Table *)
  { return result_field; }
  table_map used_tables() const { return 1; }
  virtual void fix_length_and_dec()=0;
  void set_result_field(Field *field) { result_field= field; }
  bool is_result_field() { return 1; }
  void save_in_result_field(bool no_conversions)
  {
    save_in_field(result_field, no_conversions);
  }
  void cleanup();
};

} /* namespace drizzled */

