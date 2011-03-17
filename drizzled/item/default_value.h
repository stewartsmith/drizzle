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

#include <drizzled/item/ident.h>
#include <drizzled/item/field.h>

namespace drizzled
{

class Item_default_value : public Item_field
{
public:
  Item *arg;
  Item_default_value(Name_resolution_context *context_arg)
    :Item_field(context_arg, (const char *)NULL, (const char *)NULL,
               (const char *)NULL),
     arg(NULL) {}
  Item_default_value(Name_resolution_context *context_arg, Item *a)
    :Item_field(context_arg, (const char *)NULL, (const char *)NULL,
                (const char *)NULL),
     arg(a) {}
  enum Type type() const { return DEFAULT_VALUE_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const;
  bool fix_fields(Session *, Item **);
  virtual void print(String *str);
  int save_in_field(Field *field_arg, bool no_conversions);
  table_map used_tables() const { return (table_map)0L; }

  bool walk(Item_processor processor, bool walk_subquery, unsigned char *args)
  {
    return arg->walk(processor, walk_subquery, args) ||
      (this->*processor)(args);
  }

  Item *transform(Item_transformer transformer, unsigned char *args);
};

} /* namespace drizzled */

