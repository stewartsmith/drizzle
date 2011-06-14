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
#include <drizzled/item/direct_ref.h>
#include <drizzled/item/field.h>

/*
  Class for outer fields.
  An object of this class is created when the select where the outer field was
  resolved is a grouping one. After it has been fixed the ref field will point
  to either an Item_ref or an Item_direct_ref object which will be used to
  access the field.
  See also comments for the fix_inner_refs() and the
  Item_field::fix_outer_field() functions.
*/

namespace drizzled
{

class Item_outer_ref :public Item_direct_ref
{
public:
  Item *outer_ref;
  /* The aggregate function under which this outer ref is used, if any. */
  Item_sum *in_sum_func;
  /*
    true <=> that the outer_ref is already present in the select list
    of the outer select.
  */
  bool found_in_select_list;
  Item_outer_ref(Name_resolution_context *context_arg,
                 Item_field *outer_field_arg)
    :Item_direct_ref(context_arg, 0, outer_field_arg->table_name,
                     outer_field_arg->field_name),
    outer_ref(outer_field_arg), in_sum_func(0),
    found_in_select_list(0)
  {
    ref= &outer_ref;
    set_properties();
    fixed= 0;
  }
  Item_outer_ref(Name_resolution_context *context_arg, Item **item,
                 const char *table_name_arg, const char *field_name_arg,
                 bool alias_name_used_arg)
    :Item_direct_ref(context_arg, item, table_name_arg, field_name_arg,
                     alias_name_used_arg),
    outer_ref(0), in_sum_func(0), found_in_select_list(1)
  {}
  void save_in_result_field(bool)
  {
    outer_ref->save_org_in_field(result_field);
  }
  bool fix_fields(Session *, Item **);
  void fix_after_pullout(Select_Lex *new_parent, Item **ref);
  table_map used_tables() const
  {
    return (*ref)->const_item() ? 0 : OUTER_REF_TABLE_BIT;
  }
  virtual Ref_Type ref_type() { return OUTER_REF; }
};

} /* namespace drizzled */

