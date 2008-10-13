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
#include <drizzled/functions/int.h>
#include <drizzled/functions/numhybrid.h>
#include <drizzled/functions/num_op.h>
#include <drizzled/functions/real.h>


/* function where type of result detected by first argument */
class Item_func_num1: public Item_func_numhybrid
{
public:
  Item_func_num1(Item *a) :Item_func_numhybrid(a) {}
  Item_func_num1(Item *a, Item *b) :Item_func_numhybrid(a, b) {}

  void fix_num_length_and_dec();
  void find_num_type();
  String *str_op(String *str __attribute__((unused)))
  { assert(0); return 0; }
};


class Item_func_connection_id :public Item_int_func
{
  int64_t value;

public:
  Item_func_connection_id() {}
  const char *func_name() const { return "connection_id"; }
  void fix_length_and_dec();
  bool fix_fields(THD *thd, Item **ref);
  int64_t val_int() { assert(fixed == 1); return value; }
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
};


class Item_func_signed :public Item_int_func
{
public:
  Item_func_signed(Item *a) :Item_int_func(a) {}
  const char *func_name() const { return "cast_as_signed"; }
  int64_t val_int();
  int64_t val_int_from_str(int *error);
  void fix_length_and_dec()
  { max_length=args[0]->max_length; unsigned_flag=0; }
  virtual void print(String *str, enum_query_type query_type);
  uint32_t decimal_precision() const { return args[0]->decimal_precision(); }
};


class Item_func_unsigned :public Item_func_signed
{
public:
  Item_func_unsigned(Item *a) :Item_func_signed(a) {}
  const char *func_name() const { return "cast_as_unsigned"; }
  void fix_length_and_dec()
  { max_length=args[0]->max_length; unsigned_flag=1; }
  int64_t val_int();
  virtual void print(String *str, enum_query_type query_type);
};


class Item_decimal_typecast :public Item_func
{
  my_decimal decimal_value;
public:
  Item_decimal_typecast(Item *a, int len, int dec) :Item_func(a)
  {
    decimals= dec;
    max_length= my_decimal_precision_to_length(len, dec, unsigned_flag);
  }
  String *val_str(String *str);
  double val_real();
  int64_t val_int();
  my_decimal *val_decimal(my_decimal*);
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  enum_field_types field_type() const { return DRIZZLE_TYPE_NEWDECIMAL; }
  void fix_length_and_dec() {};
  const char *func_name() const { return "decimal_typecast"; }
  virtual void print(String *str, enum_query_type query_type);
};


class Item_func_additive_op :public Item_num_op
{
public:
  Item_func_additive_op(Item *a,Item *b) :Item_num_op(a,b) {}
  void result_precision();
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return false; }
};


class Item_func_plus :public Item_func_additive_op
{
public:
  Item_func_plus(Item *a,Item *b) :Item_func_additive_op(a,b) {}
  const char *func_name() const { return "+"; }
  int64_t int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
};

class Item_func_minus :public Item_func_additive_op
{
public:
  Item_func_minus(Item *a,Item *b) :Item_func_additive_op(a,b) {}
  const char *func_name() const { return "-"; }
  int64_t int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  void fix_length_and_dec();
};


class Item_func_mul :public Item_num_op
{
public:
  Item_func_mul(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "*"; }
  int64_t int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  void result_precision();
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return false; }
};


class Item_func_div :public Item_num_op
{
public:
  uint32_t prec_increment;
  Item_func_div(Item *a,Item *b) :Item_num_op(a,b) {}
  int64_t int_op() { assert(0); return 0; }
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "/"; }
  void fix_length_and_dec();
  void result_precision();
};


class Item_func_int_div :public Item_int_func
{
public:
  Item_func_int_div(Item *a,Item *b) :Item_int_func(a,b)
  {}
  int64_t val_int();
  const char *func_name() const { return "DIV"; }
  void fix_length_and_dec();

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return false; }
};


class Item_func_mod :public Item_num_op
{
public:
  Item_func_mod(Item *a,Item *b) :Item_num_op(a,b) {}
  int64_t int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "%"; }
  void result_precision();
  void fix_length_and_dec();
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return false; }
};


class Item_func_neg :public Item_func_num1
{
public:
  Item_func_neg(Item *a) :Item_func_num1(a) {}
  double real_op();
  int64_t int_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "-"; }
  enum Functype functype() const   { return NEG_FUNC; }
  void fix_length_and_dec();
  void fix_num_length_and_dec();
  uint32_t decimal_precision() const { return args[0]->decimal_precision(); }
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return false; }
};


class Item_func_abs :public Item_func_num1
{
public:
  Item_func_abs(Item *a) :Item_func_num1(a) {}
  double real_op();
  int64_t int_op();
  my_decimal *decimal_op(my_decimal *);
  const char *func_name() const { return "abs"; }
  void fix_length_and_dec();
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return false; }
};

// A class to handle logarithmic and trigonometric functions

class Item_dec_func :public Item_real_func
{
 public:
  Item_dec_func(Item *a) :Item_real_func(a) {}
  Item_dec_func(Item *a,Item *b) :Item_real_func(a,b) {}
  void fix_length_and_dec()
  {
    decimals=NOT_FIXED_DEC; max_length=float_length(decimals);
    maybe_null=1;
  }
};

class Item_func_exp :public Item_dec_func
{
public:
  Item_func_exp(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "exp"; }
};


class Item_func_ln :public Item_dec_func
{
public:
  Item_func_ln(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "ln"; }
};


class Item_func_log :public Item_dec_func
{
public:
  Item_func_log(Item *a) :Item_dec_func(a) {}
  Item_func_log(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val_real();
  const char *func_name() const { return "log"; }
};


class Item_func_log2 :public Item_dec_func
{
public:
  Item_func_log2(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "log2"; }
};


class Item_func_log10 :public Item_dec_func
{
public:
  Item_func_log10(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "log10"; }
};


class Item_func_sqrt :public Item_dec_func
{
public:
  Item_func_sqrt(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "sqrt"; }
};


class Item_func_pow :public Item_dec_func
{
public:
  Item_func_pow(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val_real();
  const char *func_name() const { return "pow"; }
};


class Item_func_acos :public Item_dec_func
{
public:
  Item_func_acos(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "acos"; }
};

class Item_func_asin :public Item_dec_func
{
public:
  Item_func_asin(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "asin"; }
};

class Item_func_atan :public Item_dec_func
{
public:
  Item_func_atan(Item *a) :Item_dec_func(a) {}
  Item_func_atan(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val_real();
  const char *func_name() const { return "atan"; }
};

class Item_func_cos :public Item_dec_func
{
public:
  Item_func_cos(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "cos"; }
};

class Item_func_sin :public Item_dec_func
{
public:
  Item_func_sin(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "sin"; }
};

class Item_func_tan :public Item_dec_func
{
public:
  Item_func_tan(Item *a) :Item_dec_func(a) {}
  double val_real();
  const char *func_name() const { return "tan"; }
};

class Item_func_integer :public Item_int_func
{
public:
  inline Item_func_integer(Item *a) :Item_int_func(a) {}
  void fix_length_and_dec();
};


class Item_func_int_val :public Item_func_num1
{
public:
  Item_func_int_val(Item *a) :Item_func_num1(a) {}
  void fix_num_length_and_dec();
  void find_num_type();
};


class Item_func_ceiling :public Item_func_int_val
{
public:
  Item_func_ceiling(Item *a) :Item_func_int_val(a) {}
  const char *func_name() const { return "ceiling"; }
  int64_t int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return false; }
};


class Item_func_floor :public Item_func_int_val
{
public:
  Item_func_floor(Item *a) :Item_func_int_val(a) {}
  const char *func_name() const { return "floor"; }
  int64_t int_op();
  double real_op();
  my_decimal *decimal_op(my_decimal *);
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return false; }
};

/* This handles round and truncate */

class Item_func_round :public Item_func_num1
{
  bool truncate;
public:
  Item_func_round(Item *a, Item *b, bool trunc_arg)
    :Item_func_num1(a,b), truncate(trunc_arg) {}
  const char *func_name() const { return truncate ? "truncate" : "round"; }
  double real_op();
  int64_t int_op();
  my_decimal *decimal_op(my_decimal *);
  void fix_length_and_dec();
};


class Item_func_rand :public Item_real_func
{
  struct rand_struct *rand;
public:
  Item_func_rand(Item *a) :Item_real_func(a), rand(0) {}
  Item_func_rand()	  :Item_real_func() {}
  double val_real();
  const char *func_name() const { return "rand"; }
  bool const_item() const { return 0; }
  void update_used_tables();
  bool fix_fields(THD *thd, Item **ref);
  bool check_vcol_func_processor(unsigned char *int_arg __attribute__((unused)))
  { return true; }
private:
  void seed_random (Item * val);  
};


class Item_func_sign :public Item_int_func
{
public:
  Item_func_sign(Item *a) :Item_int_func(a) {}
  const char *func_name() const { return "sign"; }
  int64_t val_int();
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


class Item_func_min_max :public Item_func
{
  Item_result cmp_type;
  String tmp_value;
  int cmp_sign;
  /* TRUE <=> arguments should be compared in the DATETIME context. */
  bool compare_as_dates;
  /* An item used for issuing warnings while string to DATETIME conversion. */
  Item *datetime_item;
  THD *thd;
protected:
  enum_field_types cached_field_type;
public:
  Item_func_min_max(List<Item> &list,int cmp_sign_arg) :Item_func(list),
    cmp_type(INT_RESULT), cmp_sign(cmp_sign_arg), compare_as_dates(false),
    datetime_item(0) {}
  double val_real();
  int64_t val_int();
  String *val_str(String *);
  my_decimal *val_decimal(my_decimal *);
  void fix_length_and_dec();
  enum Item_result result_type () const { return cmp_type; }
  bool result_as_int64_t() { return compare_as_dates; };
  uint32_t cmp_datetimes(uint64_t *value);
  enum_field_types field_type() const { return cached_field_type; }
};

class Item_func_min :public Item_func_min_max
{
public:
  Item_func_min(List<Item> &list) :Item_func_min_max(list,1) {}
  const char *func_name() const { return "least"; }
};

class Item_func_max :public Item_func_min_max
{
public:
  Item_func_max(List<Item> &list) :Item_func_min_max(list,-1) {}
  const char *func_name() const { return "greatest"; }
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


class Item_func_length :public Item_int_func
{
  String value;
public:
  Item_func_length(Item *a) :Item_int_func(a) {}
  int64_t val_int();
  const char *func_name() const { return "length"; }
  void fix_length_and_dec() { max_length=10; }
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
  bool fix_fields(THD *thd, Item **ref);
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
  bool fix_fields(THD *thd, Item **ref);
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
  bool fix_fields(THD *thd, Item **ref);
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
  bool fix_fields(THD *thd, Item **ref);
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
