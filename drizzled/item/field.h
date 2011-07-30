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

class COND_EQUAL;

extern Item **not_found_item;

class Item_field :public Item_ident
{
protected:
  void set_field(Field *field);
public:
  Field *field,*result_field;
  Item_equal *item_equal;
  bool no_const_subst;
  /*
    if any_privileges set to true then here real effective privileges will
    be stored
  */
  uint32_t have_privileges;
  /* field need any privileges (for VIEW creation) */
  bool any_privileges;
  Item_field(Name_resolution_context *context_arg,
             const char *db_arg,const char *table_name_arg,
	     const char *field_name_arg);
  /*
    Constructor needed to process subselect with temporary tables (see Item)
  */
  Item_field(Session *session, Item_field *item);
  /*
    Constructor used inside setup_wild(), ensures that field, table,
    and database names will live as long as Item_field (this is important
    in prepared statements).
  */
  Item_field(Session *session, Name_resolution_context *context_arg, Field *field);
  /*
    If this constructor is used, fix_fields() won't work, because
    db_name, table_name and column_name are unknown. It's necessary to call
    reset_field() before fix_fields() for all fields created this way.
  */
  Item_field(Field *field);
  enum Type type() const { return FIELD_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const;
  double val_real();
  int64_t val_int();
  type::Decimal *val_decimal(type::Decimal *);
  String *val_str(String*);
  double val_result();
  int64_t val_int_result();
  String *str_result(String* tmp);
  type::Decimal *val_decimal_result(type::Decimal *);
  bool val_bool_result();
  void send(plugin::Client *client, String *str_arg);
  void reset_field(Field *f);
  bool fix_fields(Session *, Item **);
  void fix_after_pullout(Select_Lex *new_parent, Item **ref);
  void make_field(SendField *tmp_field);
  int save_in_field(Field *field,bool no_conversions);
  void save_org_in_field(Field *field);
  table_map used_tables() const;
  enum Item_result result_type () const;
  Item_result cast_to_int_type() const;
  enum_field_types field_type() const;
  int64_t val_int_endpoint(bool left_endp, bool *incl_endp);
  Field *get_tmp_table_field() { return result_field; }
  Field *tmp_table_field(Table *) { return result_field; }
  bool get_date(type::Time &ltime,uint32_t fuzzydate);
  bool get_date_result(type::Time &ltime, uint32_t fuzzydate);
  bool get_time(type::Time &ltime);
  bool is_null();
  void update_null_value();
  Item *get_tmp_table_item(Session *session);
  bool collect_item_field_processor(unsigned char * arg);
  bool find_item_in_field_list_processor(unsigned char *arg);
  bool register_field_in_read_map(unsigned char *arg);
  void cleanup();
  bool result_as_int64_t();
  Item_equal *find_item_equal(COND_EQUAL *cond_equal);
  bool subst_argument_checker(unsigned char **arg);
  Item *equal_fields_propagator(unsigned char *arg);
  bool set_no_const_sub(unsigned char *arg);
  Item *replace_equal_field(unsigned char *arg);
  uint32_t max_disp_length();
  Item *safe_charset_converter(const charset_info_st * const tocs);
  int fix_outer_field(Session *session, Field **field, Item **reference);
  virtual Item *update_value_transformer(unsigned char *select_arg);
  virtual void print(String *str);

  friend class Item_default_value;
  friend class Item_insert_value;
  friend class Select_Lex_Unit;
};

} /* namespace drizzled */

