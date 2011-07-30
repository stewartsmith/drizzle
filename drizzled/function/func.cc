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

#include <config.h>

#include <drizzled/check_stack_overrun.h>
#include <drizzled/current_session.h>
#include <drizzled/error.h>
#include <drizzled/field/decimal.h>
#include <drizzled/field/double.h>
#include <drizzled/field/int32.h>
#include <drizzled/field/int64.h>
#include <drizzled/field/size.h>
#include <drizzled/function/math/int.h>
#include <drizzled/item/field.h>
#include <drizzled/session.h>
#include <drizzled/sql_list.h>
#include <drizzled/sql_string.h>

#include <limits>
#include <algorithm>

using namespace std;

namespace drizzled
{


Item_func::Item_func(void):
  allowed_arg_cols(1), arg_count(0),
  const_item_cache(false)
  {
    with_sum_func= 0;
    collation.set(DERIVATION_SYSCONST);
  }

Item_func::Item_func(Item *a):
  allowed_arg_cols(1), arg_count(1),
  const_item_cache(false)
  {
    args= tmp_arg;
    args[0]= a;
    with_sum_func= a->with_sum_func;
    collation.set(DERIVATION_SYSCONST);
  }

Item_func::Item_func(Item *a,Item *b):
  allowed_arg_cols(1), arg_count(2),
  const_item_cache(false)
  {
    args= tmp_arg;
    args[0]= a; args[1]= b;
    with_sum_func= a->with_sum_func || b->with_sum_func;
    collation.set(DERIVATION_SYSCONST);
  }

Item_func::Item_func(Item *a,Item *b,Item *c):
  allowed_arg_cols(1),
  const_item_cache(false)
  {
    arg_count= 0;
    if ((args= (Item**) memory::sql_alloc(sizeof(Item*)*3)))
    {
      arg_count= 3;
      args[0]= a; args[1]= b; args[2]= c;
      with_sum_func= a->with_sum_func || b->with_sum_func || c->with_sum_func;
    }
    collation.set(DERIVATION_SYSCONST);
  }

Item_func::Item_func(Item *a,Item *b,Item *c,Item *d):
  allowed_arg_cols(1),
  const_item_cache(false)
  {
    arg_count= 0;
    if ((args= (Item**) memory::sql_alloc(sizeof(Item*)*4)))
    {
      arg_count= 4;
      args[0]= a; args[1]= b; args[2]= c; args[3]= d;
      with_sum_func= a->with_sum_func || b->with_sum_func ||
        c->with_sum_func || d->with_sum_func;
    }
    collation.set(DERIVATION_SYSCONST);
  }

Item_func::Item_func(Item *a,Item *b,Item *c,Item *d,Item* e):
  allowed_arg_cols(1),
  const_item_cache(false)
  {
    arg_count= 5;
    if ((args= (Item**) memory::sql_alloc(sizeof(Item*)*5)))
    {
      args[0]= a; args[1]= b; args[2]= c; args[3]= d; args[4]= e;
      with_sum_func= a->with_sum_func || b->with_sum_func ||
        c->with_sum_func || d->with_sum_func || e->with_sum_func ;
    }
    collation.set(DERIVATION_SYSCONST);
  }


void Item_func::set_arguments(List<Item> &list)
{
  allowed_arg_cols= 1;
  arg_count=list.size();
  args= tmp_arg;                                // If 2 arguments
  if (arg_count <= 2 || (args=(Item**) memory::sql_alloc(sizeof(Item*)*arg_count)))
  {
    List<Item>::iterator li(list.begin());
    Item **save_args= args;

    while (Item* item=li++)
    {
      *(save_args++)= item;
      with_sum_func|=item->with_sum_func;
    }
  }
  list.clear();          // Fields are used
}

Item_func::Item_func(List<Item> &list) :
  allowed_arg_cols(1),
  const_item_cache(false)
{
  collation.set(DERIVATION_SYSCONST);
  set_arguments(list);
}

Item_func::Item_func(Session *session, Item_func *item) :
  Item_result_field(session, item),
  allowed_arg_cols(item->allowed_arg_cols),
  arg_count(item->arg_count),
  used_tables_cache(item->used_tables_cache),
  not_null_tables_cache(item->not_null_tables_cache),
  const_item_cache(item->const_item_cache)
{
  if (arg_count)
  {
    if (arg_count <=2)
      args= tmp_arg;
    else
    {
      args= new (getSession().mem) Item*[arg_count];
    }
    memcpy(args, item->args, sizeof(Item*)*arg_count);
  }
  collation.set(DERIVATION_SYSCONST);
}


/*
  Resolve references to table column for a function and its argument

  SYNOPSIS:
  fix_fields()
  session    Thread object
  ref    Pointer to where this object is used.  This reference
  is used if we want to replace this object with another
  one (for example in the summary functions).

  DESCRIPTION
  Call fix_fields() for all arguments to the function.  The main intention
  is to allow all Item_field() objects to setup pointers to the table fields.

  Sets as a side effect the following class variables:
  maybe_null  Set if any argument may return NULL
  with_sum_func  Set if any of the arguments contains a sum function
  used_tables_cache Set to union of the tables used by arguments

  str_value.charset If this is a string function, set this to the
  character set for the first argument.
  If any argument is binary, this is set to binary

  If for any item any of the defaults are wrong, then this can
  be fixed in the fix_length_and_dec() function that is called
  after this one or by writing a specialized fix_fields() for the
  item.

  RETURN VALUES
  false  ok
  true  Got error.  Stored with my_error().
*/

bool
Item_func::fix_fields(Session *session, Item **)
{
  assert(fixed == 0);
  Item **arg,**arg_end;
  void *save_session_marker= session->session_marker;
  unsigned char buff[STACK_BUFF_ALLOC];      // Max argument in function
  session->session_marker= 0;
  used_tables_cache= not_null_tables_cache= 0;
  const_item_cache= true;

  if (check_stack_overrun(session, STACK_MIN_SIZE, buff))
    return true;        // Fatal error if flag is set!
  if (arg_count)
  {            // Print purify happy
    for (arg=args, arg_end=args+arg_count; arg != arg_end ; arg++)
    {
      Item *item;
      /*
        We can't yet set item to *arg as fix_fields may change *arg
        We shouldn't call fix_fields() twice, so check 'fixed' field first
      */
      if ((!(*arg)->fixed && (*arg)->fix_fields(session, arg)))
        return true;
      item= *arg;

      if (allowed_arg_cols)
      {
        if (item->check_cols(allowed_arg_cols))
          return 1;
      }
      else
      {
        /*  we have to fetch allowed_arg_cols from first argument */
        assert(arg == args); // it is first argument
        allowed_arg_cols= item->cols();
        assert(allowed_arg_cols); // Can't be 0 any more
      }

      if (item->maybe_null)
        maybe_null=1;

      with_sum_func= with_sum_func || item->with_sum_func;
      used_tables_cache|=     item->used_tables();
      not_null_tables_cache|= item->not_null_tables();
      const_item_cache&=      item->const_item();
      with_subselect|=        item->with_subselect;
    }
  }
  fix_length_and_dec();
  if (session->is_error()) // An error inside fix_length_and_dec occured
    return true;
  fixed= 1;
  session->session_marker= save_session_marker;
  return false;
}


void Item_func::fix_after_pullout(Select_Lex *new_parent,
                                  Item **)
{
  Item **arg,**arg_end;

  used_tables_cache= not_null_tables_cache= 0;
  const_item_cache= false;

  if (arg_count)
  {
    for (arg=args, arg_end=args+arg_count; arg != arg_end ; arg++)
    {
      (*arg)->fix_after_pullout(new_parent, arg);
      Item *item= *arg;

      used_tables_cache|=     item->used_tables();
      not_null_tables_cache|= item->not_null_tables();
      const_item_cache&=      item->const_item();
    }
  }
}


bool Item_func::walk(Item_processor processor, bool walk_subquery,
                     unsigned char *argument)
{
  if (arg_count)
  {
    Item **arg,**arg_end;
    for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
    {
      if ((*arg)->walk(processor, walk_subquery, argument))
        return 1;
    }
  }
  return (this->*processor)(argument);
}

void Item_func::traverse_cond(Cond_traverser traverser,
                              void *argument, traverse_order order)
{
  if (arg_count)
  {
    Item **arg,**arg_end;

    switch (order) {
    case (T_PREFIX):
      (*traverser)(this, argument);
      for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
      {
        (*arg)->traverse_cond(traverser, argument, order);
      }
      break;
    case (T_POSTFIX):
      for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
      {
        (*arg)->traverse_cond(traverser, argument, order);
      }
      (*traverser)(this, argument);
    }
  }
  else
    (*traverser)(this, argument);
}


/**
   Transform an Item_func object with a transformer callback function.

   The function recursively applies the transform method to each
   argument of the Item_func node.
   If the call of the method for an argument item returns a new item
   the old item is substituted for a new one.
   After this the transformer is applied to the root node
   of the Item_func object.
   @param transformer   the transformer callback function to be applied to
   the nodes of the tree of the object
   @param argument      parameter to be passed to the transformer

   @return
   Item returned as the result of transformation of the root node
*/

Item *Item_func::transform(Item_transformer transformer, unsigned char *argument)
{
  if (arg_count)
  {
    Item **arg,**arg_end;
    for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
    {
      Item *new_item= (*arg)->transform(transformer, argument);
      if (!new_item)
        return 0;
      *arg= new_item;
    }
  }
  return (this->*transformer)(argument);
}


/**
   Compile Item_func object with a processor and a transformer
   callback functions.

   First the function applies the analyzer to the root node of
   the Item_func object. Then if the analizer succeeeds (returns true)
   the function recursively applies the compile method to each argument
   of the Item_func node.
   If the call of the method for an argument item returns a new item
   the old item is substituted for a new one.
   After this the transformer is applied to the root node
   of the Item_func object.

   @param analyzer      the analyzer callback function to be applied to the
   nodes of the tree of the object
   @param[in,out] arg_p parameter to be passed to the processor
   @param transformer   the transformer callback function to be applied to the
   nodes of the tree of the object
   @param arg_t         parameter to be passed to the transformer

   @return
   Item returned as the result of transformation of the root node
*/

Item *Item_func::compile(Item_analyzer analyzer, unsigned char **arg_p,
                         Item_transformer transformer, unsigned char *arg_t)
{
  if (!(this->*analyzer)(arg_p))
    return 0;
  if (arg_count)
  {
    Item **arg,**arg_end;
    for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
    {
      /*
        The same parameter value of arg_p must be passed
        to analyze any argument of the condition formula.
      */
      unsigned char *arg_v= *arg_p;
      Item *new_item= (*arg)->compile(analyzer, &arg_v, transformer, arg_t);
      if (new_item && *arg != new_item)
        *arg= new_item;
    }
  }
  return (this->*transformer)(arg_t);
}

/**
   See comments in Item_cmp_func::split_sum_func()
*/

void Item_func::split_sum_func(Session *session, Item **ref_pointer_array,
                               List<Item> &fields)
{
  Item **arg, **arg_end;
  for (arg= args, arg_end= args+arg_count; arg != arg_end ; arg++)
    (*arg)->split_sum_func(session, ref_pointer_array, fields, arg, true);
}


void Item_func::update_used_tables()
{
  used_tables_cache=0;
  const_item_cache= true;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    args[i]->update_used_tables();
    used_tables_cache|=args[i]->used_tables();
    const_item_cache&=args[i]->const_item();
  }
}


table_map Item_func::used_tables() const
{
  return used_tables_cache;
}


table_map Item_func::not_null_tables() const
{
  return not_null_tables_cache;
}


void Item_func::print(String *str)
{
  str->append(func_name());
  str->append('(');
  print_args(str, 0);
  str->append(')');
}


void Item_func::print_args(String *str, uint32_t from)
{
  for (uint32_t i=from ; i < arg_count ; i++)
  {
    if (i != from)
      str->append(',');
    args[i]->print(str);
  }
}


void Item_func::print_op(String *str)
{
  str->append('(');
  for (uint32_t i=0 ; i < arg_count-1 ; i++)
  {
    args[i]->print(str);
    str->append(' ');
    str->append(func_name());
    str->append(' ');
  }
  args[arg_count-1]->print(str);
  str->append(')');
}


bool Item_func::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM)
    return 0;
  Item_func *item_func=(Item_func*) item;
  Item_func::Functype func_type;
  if ((func_type= functype()) != item_func->functype() ||
      arg_count != item_func->arg_count ||
      (func_type != Item_func::FUNC_SP &&
       func_name() != item_func->func_name()) ||
      (func_type == Item_func::FUNC_SP &&
       my_strcasecmp(system_charset_info, func_name(), item_func->func_name())))
    return 0;
  for (uint32_t i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func->args[i], binary_cmp))
      return 0;
  return 1;
}


bool Item_func::get_arg0_date(type::Time &ltime, uint32_t fuzzy_date)
{
  return (null_value=args[0]->get_date(ltime, fuzzy_date));
}


bool Item_func::get_arg0_time(type::Time &ltime)
{
  return (null_value= args[0]->get_time(ltime));
}


bool Item_func::is_null()
{
  update_null_value();
  return null_value;
}


Field *Item_func::tmp_table_field(Table *table)
{
  Field *field= NULL;

  switch (result_type()) {
  case INT_RESULT:
    if (unsigned_flag)
    {
      field= new field::Size(max_length, maybe_null, name, true);
    } 
    else if (max_length > MY_INT32_NUM_DECIMAL_DIGITS)
    {
      field= new field::Int64(max_length, maybe_null, name, false);
    }
    else
    {
      field= new field::Int32(max_length, maybe_null, name, false);
    }

    break;

  case REAL_RESULT:
    field= new Field_double(max_length, maybe_null, name, decimals);
    break;

  case STRING_RESULT:
    return make_string_field(table);

  case DECIMAL_RESULT:
    field= new Field_decimal(class_decimal_precision_to_length(decimal_precision(),
                                                            decimals,
                                                            unsigned_flag),
                             maybe_null,
                             name,
                             decimals,
                             unsigned_flag);
    break;
  case ROW_RESULT:
    // This case should never be chosen
    assert(0);
    break;
  }

  if (field)
    field->init(table);

  return field;
}


type::Decimal *Item_func::val_decimal(type::Decimal *decimal_value)
{
  assert(fixed);
  int2_class_decimal(E_DEC_FATAL_ERROR, val_int(), unsigned_flag, decimal_value);
  return decimal_value;
}


bool Item_func::agg_arg_collations(DTCollation &c, Item **items,
                                   uint32_t nitems, uint32_t flags)
{
  return agg_item_collations(c, func_name(), items, nitems, flags, 1);
}


bool Item_func::agg_arg_collations_for_comparison(DTCollation &c,
                                                  Item **items,
                                                  uint32_t nitems,
                                                  uint32_t flags)
{
  return agg_item_collations_for_comparison(c, func_name(),
                                            items, nitems, flags);
}


bool Item_func::agg_arg_charsets(DTCollation &c, Item **items, uint32_t nitems,
                                 uint32_t flags, int item_sep)
{
  return agg_item_charsets(c, func_name(), items, nitems, flags, item_sep);
}


double Item_func::fix_result(double value)
{
  static double fix_infinity= numeric_limits<double>::infinity();

  if (value != fix_infinity && value != -fix_infinity)
    return value;
  null_value=1;
  return 0.0;
}


void Item_func::fix_num_length_and_dec()
{
  uint32_t fl_length= 0;
  decimals=0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(decimals,args[i]->decimals);
    set_if_bigger(fl_length, args[i]->max_length);
  }
  max_length=float_length(decimals);
  if (fl_length > max_length)
  {
    decimals= NOT_FIXED_DEC;
    max_length= float_length(NOT_FIXED_DEC);
  }
}

/**
  Set max_length/decimals of function if function is fixed point and
  result length/precision depends on argument ones.
*/

void Item_func::count_decimal_length()
{
  int max_int_part= 0;
  decimals= 0;
  unsigned_flag= 1;
  for (uint32_t i= 0 ; i < arg_count ; i++)
  {
    set_if_bigger(decimals, args[i]->decimals);
    set_if_bigger(max_int_part, args[i]->decimal_int_part());
    set_if_smaller(unsigned_flag, args[i]->unsigned_flag);
  }
  int precision= min(max_int_part + decimals, DECIMAL_MAX_PRECISION);
  max_length= class_decimal_precision_to_length(precision, decimals,
                                             unsigned_flag);
}


/**
  Set max_length of if it is maximum length of its arguments.
*/

void Item_func::count_only_length()
{
  max_length= 0;
  unsigned_flag= 0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length, args[i]->max_length);
    set_if_bigger(unsigned_flag, args[i]->unsigned_flag);
  }
}


/**
  Set max_length/decimals of function if function is floating point and
  result length/precision depends on argument ones.
*/

void Item_func::count_real_length()
{
  uint32_t length= 0;
  decimals= 0;
  max_length= 0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    if (decimals != NOT_FIXED_DEC)
    {
      set_if_bigger(decimals, args[i]->decimals);
      set_if_bigger(length, (args[i]->max_length - args[i]->decimals));
    }
    set_if_bigger(max_length, args[i]->max_length);
  }
  if (decimals != NOT_FIXED_DEC)
  {
    max_length= length;
    length+= decimals;
    if (length < max_length)  // If previous operation gave overflow
      max_length= UINT32_MAX;
    else
      max_length= length;
  }
}



void Item_func::signal_divide_by_null()
{
  my_error(ER_DIVISION_BY_ZERO, MYF(0));
  null_value= 0;
}


Item *Item_func::get_tmp_table_item(Session *session)
{
  if (!with_sum_func && !const_item() && functype() != SUSERVAR_FUNC)
    return new Item_field(result_field);
  return copy_or_same(session);
}


} /* namespace drizzled */
