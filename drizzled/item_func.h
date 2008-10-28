/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_SERVER_ITEM_FUNC_H
#define DRIZZLED_SERVER_ITEM_FUNC_H

/* Function items used by mysql */


#ifdef HAVE_IEEEFP_H
extern "C"				/* Bug in BSDI include file */
{
#include <ieeefp.h>
}
#endif

#include <drizzled/functions/func.h>
#include <drizzled/functions/additive_op.h>
#include <drizzled/functions/connection_id.h>
#include <drizzled/functions/decimal_typecast.h>
#include <drizzled/functions/divide.h>
#include <drizzled/functions/get_system_var.h>
#include <drizzled/functions/int.h>
#include <drizzled/functions/bit.h>
#include <drizzled/functions/bit_count.h>
#include <drizzled/functions/bit_length.h>
#include <drizzled/functions/field.h>
#include <drizzled/functions/find_in_set.h>
#include <drizzled/functions/integer.h>
#include <drizzled/functions/int_divide.h>
#include <drizzled/functions/length.h>
#include <drizzled/functions/lock.h>
#include <drizzled/functions/master_pos_wait.h>
#include <drizzled/functions/min_max.h>
#include <drizzled/functions/minus.h>
#include <drizzled/functions/mod.h>
#include <drizzled/functions/multiply.h>
#include <drizzled/functions/neg.h>
#include <drizzled/functions/numhybrid.h>
#include <drizzled/functions/num_op.h>
#include <drizzled/functions/num1.h>
#include <drizzled/functions/abs.h>
#include <drizzled/functions/plus.h>
#include <drizzled/functions/real.h>
#include <drizzled/functions/rollup_const.h>
#include <drizzled/functions/row_count.h>
#include <drizzled/functions/dec.h>
#include <drizzled/functions/int_val.h>
#include <drizzled/functions/acos.h>
#include <drizzled/functions/ascii.h>
#include <drizzled/functions/asin.h>
#include <drizzled/functions/atan.h>
#include <drizzled/functions/benchmark.h>
#include <drizzled/functions/char_length.h>
#include <drizzled/functions/ceiling.h>
#include <drizzled/functions/cos.h>
#include <drizzled/functions/exp.h>
#include <drizzled/functions/floor.h>
#include <drizzled/functions/last_insert.h>
#include <drizzled/functions/ln.h>
#include <drizzled/functions/log.h>
#include <drizzled/functions/units.h>
#include <drizzled/functions/ord.h>
#include <drizzled/functions/pow.h>
#include <drizzled/functions/rand.h>
#include <drizzled/functions/round.h>
#include <drizzled/functions/sin.h>
#include <drizzled/functions/sqrt.h>
#include <drizzled/functions/sign.h>
#include <drizzled/functions/signed.h>
#include <drizzled/functions/tan.h>
#include <drizzled/functions/unsigned.h>

/* Handling of user definable variables */

class user_var_entry;

class Item_func_set_user_var :public Item_func
{
  enum Item_result cached_result_type;
  user_var_entry *entry;
  char buffer[MAX_FIELD_WIDTH];
  String value;
  my_decimal decimal_buff;
  bool null_item;
  union
  {
    int64_t vint;
    double vreal;
    String *vstr;
    my_decimal *vdec;
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
  my_decimal *val_decimal(my_decimal *);
  double val_result();
  int64_t val_int_result();
  String *str_result(String *str);
  my_decimal *val_decimal_result(my_decimal *);
  bool update_hash(void *ptr, uint32_t length, enum Item_result type,
  		   const CHARSET_INFO * const cs, Derivation dv, bool unsigned_arg);
  bool send(Protocol *protocol, String *str_arg);
  void make_field(Send_field *tmp_field);
  bool check(bool use_result_field);
  bool update();
  enum Item_result result_type () const { return cached_result_type; }
  bool fix_fields(Session *session, Item **ref);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
  void print_as_stmt(String *str, enum_query_type query_type);
  const char *func_name() const { return "set_user_var"; }
  int save_in_field(Field *field, bool no_conversions,
                    bool can_use_result_field);
  int save_in_field(Field *field, bool no_conversions)
  {
    return save_in_field(field, no_conversions, 1);
  }
  void save_org_in_field(Field *field) { (void)save_in_field(field, 1, 0); }
  bool register_field_in_read_map(unsigned char *arg);
  bool register_field_in_bitmap(unsigned char *arg);
};


class Item_func_get_user_var :public Item_func
{
  user_var_entry *var_entry;
  Item_result m_cached_result_type;

public:
  LEX_STRING name; // keep it public
  Item_func_get_user_var(LEX_STRING a):
    Item_func(), m_cached_result_type(STRING_RESULT), name(a) {}
  enum Functype functype() const { return GUSERVAR_FUNC; }
  LEX_STRING get_name() { return name; }
  double val_real();
  int64_t val_int();
  my_decimal *val_decimal(my_decimal*);
  String *val_str(String* str);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
  enum Item_result result_type() const;
  /*
    We must always return variables as strings to guard against selects of type
    select @t1:=1,@t1,@t:="hello",@t from foo where (@t1:= t2.b)
  */
  const char *func_name() const { return "get_user_var"; }
  bool const_item() const;
  table_map used_tables() const
  { return const_item() ? 0 : RAND_TABLE_BIT; }
  bool eq(const Item *item, bool binary_cmp) const;
};


/*
  This item represents user variable used as out parameter (e.g in LOAD DATA),
  and it is supposed to be used only for this purprose. So it is simplified
  a lot. Actually you should never obtain its value.

  The only two reasons for this thing being an Item is possibility to store it
  in List<Item> and desire to place this code somewhere near other functions
  working with user variables.
*/
class Item_user_var_as_out_param :public Item
{
  LEX_STRING name;
  user_var_entry *entry;
public:
  Item_user_var_as_out_param(LEX_STRING a) : name(a) {}
  /* We should return something different from FIELD_ITEM here */
  enum Type type() const { return STRING_ITEM;}
  double val_real();
  int64_t val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *decimal_buffer);
  /* fix_fields() binds variable name with its entry structure */
  bool fix_fields(Session *session, Item **ref);
  virtual void print(String *str, enum_query_type query_type);
  void set_null_value(const CHARSET_INFO * const cs);
  void set_value(const char *str, uint32_t length, const CHARSET_INFO * const cs);
};


/* For type casts */

enum Cast_target
{
  ITEM_CAST_BINARY, ITEM_CAST_SIGNED_INT, ITEM_CAST_UNSIGNED_INT,
  ITEM_CAST_DATE, ITEM_CAST_TIME, ITEM_CAST_DATETIME, ITEM_CAST_CHAR,
  ITEM_CAST_DECIMAL
};


/*
 *
 * Stored FUNCTIONs
 *
 */

struct st_sp_security_context;

class Item_func_found_rows :public Item_int_func
{
public:
  Item_func_found_rows() :Item_int_func() {}
  int64_t val_int();
  const char *func_name() const { return "found_rows"; }
  void fix_length_and_dec() { decimals= 0; maybe_null=0; }
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

#endif /* DRIZZLE_SERVER_ITEM_FUNC_H */
