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
#include <drizzled/functions/int.h>
#include <drizzled/functions/int_divide.h>
#include <drizzled/functions/length.h>
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
#include <drizzled/functions/dec.h>
#include <drizzled/functions/int_val.h>
#include <drizzled/functions/acos.h>
#include <drizzled/functions/asin.h>
#include <drizzled/functions/atan.h>
#include <drizzled/functions/ceiling.h>
#include <drizzled/functions/cos.h>
#include <drizzled/functions/exp.h>
#include <drizzled/functions/floor.h>
#include <drizzled/functions/ln.h>
#include <drizzled/functions/log.h>
#include <drizzled/functions/pow.h>
#include <drizzled/functions/rand.h>
#include <drizzled/functions/round.h>
#include <drizzled/functions/sin.h>
#include <drizzled/functions/sqrt.h>
#include <drizzled/functions/sign.h>
#include <drizzled/functions/signed.h>
#include <drizzled/functions/tan.h>
#include <drizzled/functions/unsigned.h>

class Item_func_integer :public Item_int_func
{
public:
  inline Item_func_integer(Item *a) :Item_int_func(a) {}
  void fix_length_and_dec();
};

class Item_func_units :public Item_real_func
{
  char *name;
  double mul,add;
public:
  Item_func_units(char *name_arg,Item *a,double mul_arg,double add_arg)
    :Item_real_func(a),name(name_arg),mul(mul_arg),add(add_arg) {}
  double val_real();
  const char *func_name() const { return name; }
  void fix_length_and_dec()
  { decimals= NOT_FIXED_DEC; max_length= float_length(decimals); }
};


/* 
  Objects of this class are used for ROLLUP queries to wrap up 
  each constant item referred to in GROUP BY list. 
*/

class Item_func_rollup_const :public Item_func
{
public:
  Item_func_rollup_const(Item *a) :Item_func(a)
  {
    name= a->name;
    name_length= a->name_length;
  }
  double val_real() { return args[0]->val_real(); }
  int64_t val_int() { return args[0]->val_int(); }
  String *val_str(String *str) { return args[0]->val_str(str); }
  my_decimal *val_decimal(my_decimal *dec) { return args[0]->val_decimal(dec); }
  const char *func_name() const { return "rollup_const"; }
  bool const_item() const { return 0; }
  Item_result result_type() const { return args[0]->result_type(); }
  void fix_length_and_dec()
  {
    collation= args[0]->collation;
    max_length= args[0]->max_length;
    decimals=args[0]->decimals; 
    /* The item could be a NULL constant. */
    null_value= args[0]->is_null();
  }
};

class Item_func_bit_length :public Item_func_length
{
public:
  Item_func_bit_length(Item *a) :Item_func_length(a) {}
  int64_t val_int()
    { assert(fixed == 1); return Item_func_length::val_int()*8; }
  const char *func_name() const { return "bit_length"; }
};

class Item_func_char_length :public Item_int_func
{
  String value;
public:
  Item_func_char_length(Item *a) :Item_int_func(a) {}
  int64_t val_int();
  const char *func_name() const { return "char_length"; }
  void fix_length_and_dec() { max_length=10; }
};

class Item_func_coercibility :public Item_int_func
{
public:
  Item_func_coercibility(Item *a) :Item_int_func(a) {}
  int64_t val_int();
  const char *func_name() const { return "coercibility"; }
  void fix_length_and_dec() { max_length=10; maybe_null= 0; }
  table_map not_null_tables() const { return 0; }
};

class Item_func_locate :public Item_int_func
{
  String value1,value2;
  DTCollation cmp_collation;
public:
  Item_func_locate(Item *a,Item *b) :Item_int_func(a,b) {}
  Item_func_locate(Item *a,Item *b,Item *c) :Item_int_func(a,b,c) {}
  const char *func_name() const { return "locate"; }
  int64_t val_int();
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_field :public Item_int_func
{
  String value,tmp;
  Item_result cmp_type;
  DTCollation cmp_collation;
public:
  Item_func_field(List<Item> &list) :Item_int_func(list) {}
  int64_t val_int();
  const char *func_name() const { return "field"; }
  void fix_length_and_dec();
};


class Item_func_ascii :public Item_int_func
{
  String value;
public:
  Item_func_ascii(Item *a) :Item_int_func(a) {}
  int64_t val_int();
  const char *func_name() const { return "ascii"; }
  void fix_length_and_dec() { max_length=3; }
};

class Item_func_ord :public Item_int_func
{
  String value;
public:
  Item_func_ord(Item *a) :Item_int_func(a) {}
  int64_t val_int();
  const char *func_name() const { return "ord"; }
};

class Item_func_find_in_set :public Item_int_func
{
  String value,value2;
  uint32_t enum_value;
  uint64_t enum_bit;
  DTCollation cmp_collation;
public:
  Item_func_find_in_set(Item *a,Item *b) :Item_int_func(a,b),enum_value(0) {}
  int64_t val_int();
  const char *func_name() const { return "find_in_set"; }
  void fix_length_and_dec();
};

/* Base class for all bit functions: '~', '|', '^', '&', '>>', '<<' */

class Item_func_bit: public Item_int_func
{
public:
  Item_func_bit(Item *a, Item *b) :Item_int_func(a, b) {}
  Item_func_bit(Item *a) :Item_int_func(a) {}
  void fix_length_and_dec() { unsigned_flag= 1; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }
};

class Item_func_bit_or :public Item_func_bit
{
public:
  Item_func_bit_or(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "|"; }
};

class Item_func_bit_and :public Item_func_bit
{
public:
  Item_func_bit_and(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "&"; }
};

class Item_func_bit_count :public Item_int_func
{
public:
  Item_func_bit_count(Item *a) :Item_int_func(a) {}
  int64_t val_int();
  const char *func_name() const { return "bit_count"; }
  void fix_length_and_dec() { max_length=2; }
};

class Item_func_shift_left :public Item_func_bit
{
public:
  Item_func_shift_left(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "<<"; }
};

class Item_func_shift_right :public Item_func_bit
{
public:
  Item_func_shift_right(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return ">>"; }
};

class Item_func_bit_neg :public Item_func_bit
{
public:
  Item_func_bit_neg(Item *a) :Item_func_bit(a) {}
  int64_t val_int();
  const char *func_name() const { return "~"; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }
};


class Item_func_last_insert_id :public Item_int_func
{
public:
  Item_func_last_insert_id() :Item_int_func() {}
  Item_func_last_insert_id(Item *a) :Item_int_func(a) {}
  int64_t val_int();
  const char *func_name() const { return "last_insert_id"; }
  void fix_length_and_dec()
  {
    if (arg_count)
      max_length= args[0]->max_length;
  }
  bool fix_fields(Session *thd, Item **ref);
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};


class Item_func_benchmark :public Item_int_func
{
public:
  Item_func_benchmark(Item *count_expr, Item *expr)
    :Item_int_func(count_expr, expr)
  {}
  int64_t val_int();
  const char *func_name() const { return "benchmark"; }
  void fix_length_and_dec() { max_length=1; maybe_null=0; }
  virtual void print(String *str, enum_query_type query_type);
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

/* replication functions */

class Item_master_pos_wait :public Item_int_func
{
  String value;
public:
  Item_master_pos_wait(Item *a,Item *b) :Item_int_func(a,b) {}
  Item_master_pos_wait(Item *a,Item *b,Item *c) :Item_int_func(a,b,c) {}
  int64_t val_int();
  const char *func_name() const { return "master_pos_wait"; }
  void fix_length_and_dec() { max_length=21; maybe_null=1;}
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};


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
  bool fix_fields(Session *thd, Item **ref);
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
  bool fix_fields(Session *thd, Item **ref);
  virtual void print(String *str, enum_query_type query_type);
  void set_null_value(const CHARSET_INFO * const cs);
  void set_value(const char *str, uint32_t length, const CHARSET_INFO * const cs);
};


/* A system variable */

class Item_func_get_system_var :public Item_func
{
  sys_var *var;
  enum_var_type var_type;
  LEX_STRING component;
public:
  Item_func_get_system_var(sys_var *var_arg, enum_var_type var_type_arg,
                           LEX_STRING *component_arg, const char *name_arg,
                           size_t name_len_arg);
  bool fix_fields(Session *thd, Item **ref);
  /*
    Stubs for pure virtual methods. Should never be called: this
    item is always substituted with a constant in fix_fields().
  */
  double val_real()         { assert(0); return 0.0; }
  int64_t val_int()        { assert(0); return 0; }
  String* val_str(String*)  { assert(0); return 0; }
  void fix_length_and_dec() { assert(0); }
  /* TODO: fix to support views */
  const char *func_name() const { return "get_system_var"; }
  /**
    Indicates whether this system variable is written to the binlog or not.

    Variables are written to the binlog as part of "status_vars" in
    Query_log_event, as an Intvar_log_event, or a Rand_log_event.

    @return true if the variable is written to the binlog, false otherwise.
  */
  bool is_written_to_binlog();
};

class Item_func_bit_xor : public Item_func_bit
{
public:
  Item_func_bit_xor(Item *a, Item *b) :Item_func_bit(a, b) {}
  int64_t val_int();
  const char *func_name() const { return "^"; }
};

class Item_func_is_free_lock :public Item_int_func
{
  String value;
public:
  Item_func_is_free_lock(Item *a) :Item_int_func(a) {}
  int64_t val_int();
  const char *func_name() const { return "is_free_lock"; }
  void fix_length_and_dec() { decimals=0; max_length=1; maybe_null=1;}
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

class Item_func_is_used_lock :public Item_int_func
{
  String value;
public:
  Item_func_is_used_lock(Item *a) :Item_int_func(a) {}
  int64_t val_int();
  const char *func_name() const { return "is_used_lock"; }
  void fix_length_and_dec() { decimals=0; max_length=10; maybe_null=1;}
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};

/* For type casts */

enum Cast_target
{
  ITEM_CAST_BINARY, ITEM_CAST_SIGNED_INT, ITEM_CAST_UNSIGNED_INT,
  ITEM_CAST_DATE, ITEM_CAST_TIME, ITEM_CAST_DATETIME, ITEM_CAST_CHAR,
  ITEM_CAST_DECIMAL
};


class Item_func_row_count :public Item_int_func
{
public:
  Item_func_row_count() :Item_int_func() {}
  int64_t val_int();
  const char *func_name() const { return "row_count"; }
  void fix_length_and_dec() { decimals= 0; maybe_null=0; }
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
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
