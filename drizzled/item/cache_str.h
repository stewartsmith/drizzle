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

#include <drizzled/item/cache.h>

#ifndef DRIZZLED_ITEM_CACHE_STR_H
#define DRIZZLED_ITEM_CACHE_STR_H

namespace drizzled
{

class Item_cache_str: public Item_cache
{
  char buffer[STRING_BUFFER_USUAL_SIZE];
  String *value, value_buff;
  bool is_varbinary;

public:
  Item_cache_str(const Item *item);
  void store(Item *item);
  double val_real();
  int64_t val_int();
  String* val_str(String *) { assert(fixed == 1); return value; }
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type() const { return STRING_RESULT; }
  const CHARSET_INFO *charset() const { return value->charset(); };
  int save_in_field(Field *field, bool no_conversions);
};

} /* namespace drizzled */

#endif /* DRIZZLED_ITEM_CACHE_STR_H */
