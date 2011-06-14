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

/// TODO: Rename this file - func.h is stupid.

#include <drizzled/charset.h>
#include <drizzled/item.h>
#include <drizzled/item/bin_string.h>
#include <drizzled/lex_string.h>
#include <drizzled/sql_list.h>
#include <drizzled/type/decimal.h>

#include <drizzled/visibility.h>

namespace drizzled
{

class DRIZZLED_API Item_func : public Item_result_field
{
protected:
  Item **args, *tmp_arg[2];
  /*
    Allowed numbers of columns in result (usually 1, which means scalar value)
    0 means get this number from first argument
  */
  uint32_t allowed_arg_cols;

public:

  using Item::split_sum_func;

  uint32_t arg_count;
  table_map used_tables_cache, not_null_tables_cache;
  bool const_item_cache;
  enum Functype { UNKNOWN_FUNC,EQ_FUNC,EQUAL_FUNC,NE_FUNC,LT_FUNC,LE_FUNC,
                  GE_FUNC,GT_FUNC,
                  LIKE_FUNC,ISNULL_FUNC,ISNOTNULL_FUNC,
                  COND_AND_FUNC, COND_OR_FUNC, COND_XOR_FUNC,
                  BETWEEN, IN_FUNC, MULT_EQUAL_FUNC,
                  INTERVAL_FUNC, ISNOTNULLTEST_FUNC,
                  NOT_FUNC, NOT_ALL_FUNC,
                  NOW_FUNC, TRIG_COND_FUNC,
                  SUSERVAR_FUNC, GUSERVAR_FUNC, COLLATE_FUNC,
                  EXTRACT_FUNC, CHAR_TYPECAST_FUNC, FUNC_SP,
                  NEG_FUNC };
  enum optimize_type { OPTIMIZE_NONE,OPTIMIZE_KEY,OPTIMIZE_OP, OPTIMIZE_NULL,
                       OPTIMIZE_EQUAL };
  enum Type type() const { return FUNC_ITEM; }
  virtual enum Functype functype() const   { return UNKNOWN_FUNC; }
  virtual ~Item_func() {}

  Item_func(void);

  Item_func(Item *a);
  
  Item_func(Item *a,Item *b);
  
  Item_func(Item *a,Item *b,Item *c);
  
  Item_func(Item *a,Item *b,Item *c,Item *d);
  
  Item_func(Item *a,Item *b,Item *c,Item *d,Item* e);
  
  Item_func(List<Item> &list);
  
  // Constructor used for Item_cond_and/or (see Item comment)
  Item_func(Session *session, Item_func *item);
  
  bool fix_fields(Session *, Item **ref);
  void fix_after_pullout(Select_Lex *new_parent, Item **ref);
  table_map used_tables() const;
  table_map not_null_tables() const;
  void update_used_tables();
  bool eq(const Item *item, bool binary_cmp) const;
  virtual optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  virtual bool have_rev_func() const { return 0; }
  virtual Item *key_item() const { return args[0]; }
  /*
    This method is used for debug purposes to print the name of an
    item to the debug log. The second use of this method is as
    a helper function of print(), where it is applicable.
    To suit both goals it should return a meaningful,
    distinguishable and sintactically correct string.  This method
    should not be used for runtime type identification, use enum
    {Sum}Functype and Item_func::functype()/Item_sum::sum_func()
    instead.
  */
  virtual const char *func_name() const { return NULL; }
  virtual bool const_item() const { return const_item_cache; }
  Item **arguments() const { return args; }
  void set_arguments(List<Item> &list);
  uint32_t argument_count() const { return arg_count; }
  void remove_arguments() { arg_count=0; }

  /**
   * Check if the UDF supports the number of arguments passed in
   * @param number of args
   */
  virtual bool check_argument_count(int) { return true ; }
  virtual void split_sum_func(Session *session, Item **ref_pointer_array,
                              List<Item> &fields);

  virtual void print(String *str);
  void print_op(String *str);
  void print_args(String *str, uint32_t from);
  virtual void fix_num_length_and_dec();
  void count_only_length();
  void count_real_length();
  void count_decimal_length();

  bool get_arg0_date(type::Time &ltime, uint32_t fuzzy_date);
  bool get_arg0_time(type::Time &ltime);

  bool is_null();

  virtual bool deterministic() const
  {
    return false;
  }

  void signal_divide_by_null();

  virtual Field *tmp_table_field() { return result_field; }
  virtual Field *tmp_table_field(Table *t_arg);

  Item *get_tmp_table_item(Session *session);

  type::Decimal *val_decimal(type::Decimal *);

  bool agg_arg_collations(DTCollation &c, Item **items, uint32_t nitems,
                          uint32_t flags);
  bool agg_arg_collations_for_comparison(DTCollation &c,
                                         Item **items, uint32_t nitems,
                                         uint32_t flags);
  bool agg_arg_charsets(DTCollation &c, Item **items, uint32_t nitems,
                        uint32_t flags, int item_sep);
  bool walk(Item_processor processor, bool walk_subquery, unsigned char *arg);
  Item *transform(Item_transformer transformer, unsigned char *arg);
  Item* compile(Item_analyzer analyzer, unsigned char **arg_p,
                Item_transformer transformer, unsigned char *arg_t);
  void traverse_cond(Cond_traverser traverser,
                     void * arg, traverse_order order);
  double fix_result(double value);
};

} /* namespace drizzled */


