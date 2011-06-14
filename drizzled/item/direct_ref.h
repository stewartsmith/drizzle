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

#include <drizzled/item/ref.h>

namespace drizzled
{

/*
  The same as Item_ref, but get value from val_* family of method to get
  value of item on which it referred instead of result* family.
*/
class Item_direct_ref :public Item_ref
{
public:
  Item_direct_ref(Name_resolution_context *context_arg, Item **item,
                  const char *table_name_arg,
                  const char *field_name_arg,
                  bool alias_name_used_arg= false)
    :Item_ref(context_arg, item, table_name_arg,
              field_name_arg, alias_name_used_arg)
  {}
  /* Constructor need to process subselect with temporary tables (see Item) */
  Item_direct_ref(Session *session, Item_direct_ref *item) : Item_ref(session, item) {}

  double val_real();
  int64_t val_int();
  String *val_str(String* tmp);
  type::Decimal *val_decimal(type::Decimal *);
  bool val_bool();
  bool is_null();
  bool get_date(type::Time &ltime,uint32_t fuzzydate);
  virtual Ref_Type ref_type() { return DIRECT_REF; }
};

} /* namespace drizzled */

