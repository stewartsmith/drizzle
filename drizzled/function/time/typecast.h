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
#include <drizzled/field.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/temporal.h>

namespace drizzled
{

class Item_typecast :public Item_str_func
{
public:
  using Item_func::tmp_table_field;

  Item_typecast(Item *a) :Item_str_func(a) {}
  String *val_str(String *a)
  {
    assert(fixed == 1);
    String *tmp=args[0]->val_str(a);
    null_value=args[0]->null_value;
    if (tmp)
      tmp->set_charset(collation.collation);
    return tmp;
  }
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length=args[0]->max_length;
  }
  virtual const char* cast_type() const= 0;
  virtual void print(String *str);
};

class Item_typecast_maybe_null :public Item_typecast
{
public:
  Item_typecast_maybe_null(Item *a) :Item_typecast(a) {}
  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length=args[0]->max_length;
    maybe_null= 1;
  }
};

class Item_char_typecast :public Item_typecast
{
  int cast_length;
  const charset_info_st *cast_cs, *from_cs;
  bool charset_conversion;
  String tmp_value;
public:
  using Item_func::tmp_table_field;

  Item_char_typecast(Item *a, int length_arg, const charset_info_st * const cs_arg)
    :Item_typecast(a), cast_length(length_arg), cast_cs(cs_arg) {}
  enum Functype functype() const { return CHAR_TYPECAST_FUNC; }
  bool eq(const Item *item, bool binary_cmp) const;
  const char *func_name() const { return "cast_as_char"; }
  const char* cast_type() const { return "char"; }
  String *val_str(String *a);
  void fix_length_and_dec();
  virtual void print(String *str);
};

class Item_date_typecast :public Item_typecast_maybe_null
{
public:
  using Item_func::tmp_table_field;

  Item_date_typecast(Item *a) :
    Item_typecast_maybe_null(a)
  {}

  const char *func_name() const { return "cast_as_date"; }
  String *val_str(String *str);
  bool get_date(type::Time &ltime, uint32_t fuzzy_date);
  bool get_time(type::Time &ltime);
  const char *cast_type() const { return "date"; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_DATE; }

  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }

  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    max_length= 10;
    maybe_null= 1;
  }

  bool result_as_int64_t() { return true; }
  int64_t val_int();
  double val_real() { return (double) val_int(); }

  type::Decimal *val_decimal(type::Decimal *decimal_value)
  {
    assert(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }

  int save_in_field(Field *field, bool )
  {
    return save_date_in_field(field);
  }
};

class Item_datetime_typecast :public Item_typecast_maybe_null
{
public:
  using Item_func::tmp_table_field;

  Item_datetime_typecast(Item *a) :Item_typecast_maybe_null(a) {}
  const char *func_name() const { return "cast_as_datetime"; }
  String *val_str(String *str);
  const char *cast_type() const { return "datetime"; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_DATETIME; }

  Field *tmp_table_field(Table *table)
  {
    return tmp_table_field_from_field_type(table, 0);
  }

  void fix_length_and_dec()
  {
    collation.set(&my_charset_bin);
    maybe_null= 1;
    max_length= DateTime::MAX_STRING_LENGTH * MY_CHARSET_BIN_MB_MAXLEN;
    decimals= DATETIME_DEC;
  }

  bool result_as_int64_t()
  {
    return true;
  }

  int64_t val_int();
  double val_real()
  {
    return val_real_from_decimal();
  }

  double val()
  {
    return (double) val_int();
  }

  type::Decimal *val_decimal(type::Decimal *decimal_value)
  {
    assert(fixed == 1);
    return  val_decimal_from_date(decimal_value);
  }
  int save_in_field(Field *field, bool )
  {
    return save_date_in_field(field);
  }
};

} /* namespace drizzled */

