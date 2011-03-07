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

#include <config.h>

#include <drizzled/field.h>
#include <drizzled/item/cache.h>
#include <drizzled/item/cache_decimal.h>
#include <drizzled/item/cache_int.h>
#include <drizzled/item/cache_real.h>
#include <drizzled/item/cache_row.h>
#include <drizzled/item/cache_str.h>
#include <drizzled/lex_string.h>

namespace drizzled
{

Item_cache* Item_cache::get_cache(const Item *item)
{
  switch (item->result_type()) {
  case INT_RESULT:
    return new Item_cache_int();

  case REAL_RESULT:
    return new Item_cache_real();

  case DECIMAL_RESULT:
    return new Item_cache_decimal();

  case STRING_RESULT:
    return new Item_cache_str(item);

  case ROW_RESULT:
    return new Item_cache_row();
  }

  assert(0);
  abort();
}


void Item_cache::print(String *str)
{
  str->append(STRING_WITH_LEN("<cache>("));
  if (example)
    example->print(str);
  else
    Item::print(str);
  str->append(')');
}

bool Item_cache::eq_def(Field *field)
{
  return cached_field ? cached_field->eq_def (field) : false;
}

} /* namespace drizzled */
