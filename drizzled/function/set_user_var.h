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

#include <drizzled/function/func.h>

namespace drizzled {

/* Handling of user definable variables */

class Item_func_set_user_var :public Item_func
{
  enum Item_result cached_result_type;
  user_var_entry *entry;
  char buffer[MAX_FIELD_WIDTH];
  String value;
  type::Decimal decimal_buff;
  bool null_item;
  union
  {
    int64_t vint;
    double vreal;
    String *vstr;
    type::Decimal *vdec;
  } save_result;

public:
  LEX_STRING name; // keep it public
  Item_func_set_user_var(LEX_STRING a,Item *b)
    :Item_func(b), cached_result_type(INT_RESULT), name(a)
  {}
  enum Functype functype() const { return SUSERVAR_FUNC; }
  double val_real();
  int64_t val_int();
  String *val_str(String *str);
  type::Decimal *val_decimal(type::Decimal *);
  double val_result();
  int64_t val_int_result();
  String *str_result(String *str);
  type::Decimal *val_decimal_result(type::Decimal *);
  void update_hash(void *ptr, uint32_t length, enum Item_result type,
  		   const charset_info_st * const cs, Derivation dv, bool unsigned_arg);
  void send(plugin::Client *client, String *str_arg);
  void make_field(SendField *tmp_field);
  bool check(bool use_result_field);
  void update();
  Item_result result_type () const { return cached_result_type; }
  bool fix_fields(Session *session, Item **ref);
  void fix_length_and_dec();
  virtual void print(String *str);

  const char *func_name() const { return "set_user_var"; }
  int save_in_field(Field *field, bool no_conversions,
                    bool can_use_result_field);
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_in_field(field, no_conversions, 1);
  }
  void save_org_in_field(Field *field) { (void)save_in_field(field, 1, 0); }
  bool register_field_in_read_map(unsigned char *arg);
};

} /* namespace drizzled */

