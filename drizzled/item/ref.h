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

namespace drizzled {

class Item_ref :public Item_ident
{
protected:
  void set_properties();
public:
  enum Ref_Type { REF, DIRECT_REF, OUTER_REF };
  Field *result_field;			 /* Save result here */
  Item **ref;
  Item_ref(Name_resolution_context *context_arg,
           const char *db_arg, const char *table_name_arg,
           const char *field_name_arg)
    :Item_ident(context_arg, db_arg, table_name_arg, field_name_arg),
     result_field(0), ref(0) {}
  /*
    This constructor is used in two scenarios:
    A) *item = NULL
      No initialization is performed, fix_fields() call will be necessary.

    B) *item points to an Item this Item_ref will refer to. This is
      used for GROUP BY. fix_fields() will not be called in this case,
      so we call set_properties to make this item "fixed". set_properties
      performs a subset of action Item_ref::fix_fields does, and this subset
      is enough for Item_ref's used in GROUP BY.

    TODO we probably fix a superset of problems like in BUG#6658. Check this
         with Bar, and if we have a more broader set of problems like this.
  */
  Item_ref(Name_resolution_context *context_arg, Item **item,
           const char *table_name_arg, const char *field_name_arg,
           bool alias_name_used_arg= false);

  /* Constructor need to process subselect with temporary tables (see Item) */
  Item_ref(Session *session, Item_ref *item)
    :Item_ident(session, item), result_field(item->result_field), ref(item->ref) {}
  enum Type type() const		{ return REF_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const
  {
    const Item *it= item->real_item();
    return ref && (*ref)->eq(it, binary_cmp);
  }
  double val_real();
  int64_t val_int();
  type::Decimal *val_decimal(type::Decimal *);
  bool val_bool();
  String *val_str(String* tmp);
  bool is_null();
  bool get_date(type::Time &ltime,uint32_t fuzzydate);
  double val_result();
  int64_t val_int_result();
  String *str_result(String* tmp);
  type::Decimal *val_decimal_result(type::Decimal *);
  bool val_bool_result();
  void send(plugin::Client *client, String *tmp);
  void make_field(SendField *field);
  bool fix_fields(Session *, Item **);
  void fix_after_pullout(Select_Lex *new_parent, Item **ref);
  int save_in_field(Field *field, bool no_conversions);
  void save_org_in_field(Field *field);
  enum Item_result result_type () const { return (*ref)->result_type(); }
  enum_field_types field_type() const   { return (*ref)->field_type(); }
  Field *get_tmp_table_field()
  { return result_field ? result_field : (*ref)->get_tmp_table_field(); }
  Item *get_tmp_table_item(Session *session);
  table_map used_tables() const
  {
    return depended_from ? OUTER_REF_TABLE_BIT : (*ref)->used_tables();
  }
  void update_used_tables()
  {
    if (!depended_from)
      (*ref)->update_used_tables();
  }
  table_map not_null_tables() const { return (*ref)->not_null_tables(); }
  void set_result_field(Field *field)	{ result_field= field; }
  bool is_result_field() { return 1; }
  void save_in_result_field(bool no_conversions)
  {
    (*ref)->save_in_field(result_field, no_conversions);
  }
  Item *real_item(void)
  {
    return ref ? (*ref)->real_item() : this;
  }
  const Item *real_item(void) const
  {
    return ref ? (*ref)->real_item() : this;
  }
  bool walk(Item_processor processor, bool walk_subquery, unsigned char *arg)
  { return (*ref)->walk(processor, walk_subquery, arg); }
  virtual void print(String *str);
  bool result_as_int64_t()
  {
    return (*ref)->result_as_int64_t();
  }
  void cleanup();
  virtual Ref_Type ref_type() { return REF; }

  // Row emulation: forwarding of ROW-related calls to ref
  uint32_t cols()
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->cols() : 1;
  }
  Item* element_index(uint32_t i)
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->element_index(i) : this;
  }
  Item** addr(uint32_t i)
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->addr(i) : 0;
  }
  bool check_cols(uint32_t c)
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->check_cols(c)
                                              : Item::check_cols(c);
  }
  bool null_inside()
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->null_inside() : 0;
  }
  void bring_value()
  {
    if (ref && result_type() == ROW_RESULT)
      (*ref)->bring_value();
  }
  bool basic_const_item() const
  {
    return (*ref)->basic_const_item();
  }
};

} /* namespace drizzled */

