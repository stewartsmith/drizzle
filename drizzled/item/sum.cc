/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


/**
  @file

  @brief
  Sum functions (COUNT, MIN...)
*/
#include <config.h>
#include <cstdio>
#include <math.h>
#include <drizzled/sql_select.h>
#include <drizzled/error.h>
#include <drizzled/hybrid_type_traits.h>
#include <drizzled/hybrid_type_traits_integer.h>
#include <drizzled/hybrid_type_traits_decimal.h>
#include <drizzled/sql_base.h>
#include <drizzled/session.h>
#include <drizzled/item/sum.h>
#include <drizzled/field/decimal.h>
#include <drizzled/field/double.h>
#include <drizzled/field/int64.h>
#include <drizzled/field/date.h>
#include <drizzled/field/datetime.h>
#include <drizzled/unique.h>
#include <drizzled/type/decimal.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/item/subselect.h>
#include <drizzled/sql_lex.h>
#include <drizzled/system_variables.h>

#include <algorithm>

using namespace std;

namespace drizzled {

extern plugin::StorageEngine *heap_engine;

/**
  Prepare an aggregate function item for checking context conditions.

    The function initializes the members of the Item_sum object created
    for a set function that are used to check validity of the set function
    occurrence.
    If the set function is not allowed in any subquery where it occurs
    an error is reported immediately.

  @param session      reference to the thread context info

  @note
    This function is to be called for any item created for a set function
    object when the traversal of trees built for expressions used in the query
    is performed at the phase of context analysis. This function is to
    be invoked at the descent of this traversal.
  @retval
    TRUE   if an error is reported
  @retval
    FALSE  otherwise
*/

bool Item_sum::init_sum_func_check(Session *session)
{
  if (!session->lex().allow_sum_func)
  {
    my_message(ER_INVALID_GROUP_FUNC_USE, ER(ER_INVALID_GROUP_FUNC_USE),
               MYF(0));
    return true;
  }
  /* Set a reference to the nesting set function if there is  any */
  in_sum_func= session->lex().in_sum_func;
  /* Save a pointer to object to be used in items for nested set functions */
  session->lex().in_sum_func= this;
  nest_level= session->lex().current_select->nest_level;
  ref_by= 0;
  aggr_level= -1;
  aggr_sel= NULL;
  max_arg_level= -1;
  max_sum_func_level= -1;
  outer_fields.clear();
  return false;
}

/**
  Check constraints imposed on a usage of a set function.

    The method verifies whether context conditions imposed on a usage
    of any set function are met for this occurrence.
    It checks whether the set function occurs in the position where it
    can be aggregated and, when it happens to occur in argument of another
    set function, the method checks that these two functions are aggregated in
    different subqueries.
    If the context conditions are not met the method reports an error.
    If the set function is aggregated in some outer subquery the method
    adds it to the chain of items for such set functions that is attached
    to the the Select_Lex structure for this subquery.

    A number of designated members of the object are used to check the
    conditions. They are specified in the comment before the Item_sum
    class declaration.
    Additionally a bitmap variable called allow_sum_func is employed.
    It is included into the session->lex structure.
    The bitmap contains 1 at n-th position if the set function happens
    to occur under a construct of the n-th level subquery where usage
    of set functions are allowed (i.e either in the SELECT list or
    in the HAVING clause of the corresponding subquery)
    Consider the query:
    @code
       SELECT SUM(t1.b) FROM t1 GROUP BY t1.a
         HAVING t1.a IN (SELECT t2.c FROM t2 WHERE AVG(t1.b) > 20) AND
                t1.a > (SELECT MIN(t2.d) FROM t2);
    @endcode
    allow_sum_func will contain:
    - for SUM(t1.b) - 1 at the first position
    - for AVG(t1.b) - 1 at the first position, 0 at the second position
    - for MIN(t2.d) - 1 at the first position, 1 at the second position.

  @param session  reference to the thread context info
  @param ref  location of the pointer to this item in the embedding expression

  @note
    This function is to be called for any item created for a set function
    object when the traversal of trees built for expressions used in the query
    is performed at the phase of context analysis. This function is to
    be invoked at the ascent of this traversal.

  @retval
    TRUE   if an error is reported
  @retval
    FALSE  otherwise
*/

bool Item_sum::check_sum_func(Session *session, Item **ref)
{
  bool invalid= false;
  nesting_map allow_sum_func= session->lex().allow_sum_func;
  /*
    The value of max_arg_level is updated if an argument of the set function
    contains a column reference resolved  against a subquery whose level is
    greater than the current value of max_arg_level.
    max_arg_level cannot be greater than nest level.
    nest level is always >= 0
  */
  if (nest_level == max_arg_level)
  {
    /*
      The function must be aggregated in the current subquery,
      If it is there under a construct where it is not allowed
      we report an error.
    */
    invalid= !(allow_sum_func & (1 << max_arg_level));
  }
  else if (max_arg_level >= 0 || !(allow_sum_func & (1 << nest_level)))
  {
    /*
      The set function can be aggregated only in outer subqueries.
      Try to find a subquery where it can be aggregated;
      If we fail to find such a subquery report an error.
    */
    if (register_sum_func(session, ref))
      return true;
    invalid= aggr_level < 0 && !(allow_sum_func & (1 << nest_level));
    if (!invalid && false)
      invalid= aggr_level < 0 && max_arg_level < nest_level;
  }
  if (!invalid && aggr_level < 0)
  {
    aggr_level= nest_level;
    aggr_sel= session->lex().current_select;
  }
  /*
    By this moment we either found a subquery where the set function is
    to be aggregated  and assigned a value that is  >= 0 to aggr_level,
    or set the value of 'invalid' to TRUE to report later an error.
  */
  /*
    Additionally we have to check whether possible nested set functions
    are acceptable here: they are not, if the level of aggregation of
    some of them is less than aggr_level.
  */
  if (!invalid)
    invalid= aggr_level <= max_sum_func_level;
  if (invalid)
  {
    my_message(ER_INVALID_GROUP_FUNC_USE, ER(ER_INVALID_GROUP_FUNC_USE),
               MYF(0));
    return true;
  }

  if (in_sum_func)
  {
    /*
      If the set function is nested adjust the value of
      max_sum_func_level for the nesting set function.
      We take into account only enclosed set functions that are to be
      aggregated on the same level or above of the nest level of
      the enclosing set function.
      But we must always pass up the max_sum_func_level because it is
      the maximum nested level of all directly and indirectly enclosed
      set functions. We must do that even for set functions that are
      aggregated inside of their enclosing set function's nest level
      because the enclosing function may contain another enclosing
      function that is to be aggregated outside or on the same level
      as its parent's nest level.
    */
    if (in_sum_func->nest_level >= aggr_level)
      set_if_bigger(in_sum_func->max_sum_func_level, aggr_level);
    set_if_bigger(in_sum_func->max_sum_func_level, max_sum_func_level);
  }

  /*
    Check that non-aggregated fields and sum functions aren't mixed in the
    same select in the ONLY_FULL_GROUP_BY mode.
  */
  if (outer_fields.size())
  {
    Item_field *field;
    /*
      Here we compare the nesting level of the select to which an outer field
      belongs to with the aggregation level of the sum function. All fields in
      the outer_fields list are checked.

      If the nesting level is equal to the aggregation level then the field is
        aggregated by this sum function.
      If the nesting level is less than the aggregation level then the field
        belongs to an outer select. In this case if there is an embedding sum
        function add current field to functions outer_fields list. If there is
        no embedding function then the current field treated as non aggregated
        and the select it belongs to is marked accordingly.
      If the nesting level is greater than the aggregation level then it means
        that this field was added by an inner sum function.
        Consider an example:

          select avg ( <-- we are here, checking outer.f1
            select (
              select sum(outer.f1 + inner.f1) from inner
            ) from outer)
          from most_outer;

        In this case we check that no aggregate functions are used in the
        select the field belongs to. If there are some then an error is
        raised.
    */
    List<Item_field>::iterator of(outer_fields.begin());
    while ((field= of++))
    {
      Select_Lex *sel= field->cached_table->select_lex;
      if (sel->nest_level < aggr_level)
      {
        if (in_sum_func)
        {
          /*
            Let upper function decide whether this field is a non
            aggregated one.
          */
          in_sum_func->outer_fields.push_back(field);
        }
        else
        {
          sel->full_group_by_flag.set(NON_AGG_FIELD_USED);
        }
      }
      if (sel->nest_level > aggr_level &&
          (sel->full_group_by_flag.test(SUM_FUNC_USED)) &&
          ! sel->group_list.elements)
      {
        my_message(ER_MIX_OF_GROUP_FUNC_AND_FIELDS,
                   ER(ER_MIX_OF_GROUP_FUNC_AND_FIELDS), MYF(0));
        return true;
      }
    }
  }
  aggr_sel->full_group_by_flag.set(SUM_FUNC_USED);
  update_used_tables();
  session->lex().in_sum_func= in_sum_func;
  return false;
}

/**
  Attach a set function to the subquery where it must be aggregated.

    The function looks for an outer subquery where the set function must be
    aggregated. If it finds such a subquery then aggr_level is set to
    the nest level of this subquery and the item for the set function
    is added to the list of set functions used in nested subqueries
    inner_sum_func_list defined for each subquery. When the item is placed
    there the field 'ref_by' is set to ref.

  @note
    Now we 'register' only set functions that are aggregated in outer
    subqueries. Actually it makes sense to link all set function for
    a subquery in one chain. It would simplify the process of 'splitting'
    for set functions.

  @param session  reference to the thread context info
  @param ref  location of the pointer to this item in the embedding expression

  @retval
    FALSE  if the executes without failures (currently always)
  @retval
    TRUE   otherwise
*/

bool Item_sum::register_sum_func(Session *session, Item **ref)
{
  Select_Lex *sl;
  nesting_map allow_sum_func= session->lex().allow_sum_func;
  for (sl= session->lex().current_select->master_unit()->outer_select() ;
       sl && sl->nest_level > max_arg_level;
       sl= sl->master_unit()->outer_select() )
  {
    if (aggr_level < 0 && (allow_sum_func & (1 << sl->nest_level)))
    {
      /* Found the most nested subquery where the function can be aggregated */
      aggr_level= sl->nest_level;
      aggr_sel= sl;
    }
  }
  if (sl && (allow_sum_func & (1 << sl->nest_level)))
  {
    /*
      We reached the subquery of level max_arg_level and checked
      that the function can be aggregated here.
      The set function will be aggregated in this subquery.
    */
    aggr_level= sl->nest_level;
    aggr_sel= sl;

  }
  if (aggr_level >= 0)
  {
    ref_by= ref;
    /* Add the object to the list of registered objects assigned to aggr_sel */
    if (!aggr_sel->inner_sum_func_list)
      next= this;
    else
    {
      next= aggr_sel->inner_sum_func_list->next;
      aggr_sel->inner_sum_func_list->next= this;
    }
    aggr_sel->inner_sum_func_list= this;
    aggr_sel->with_sum_func= 1;

    /*
      Mark Item_subselect(s) as containing aggregate function all the way up
      to aggregate function's calculation context.
      Note that we must not mark the Item of calculation context itself
      because with_sum_func on the calculation context Select_Lex is
      already set above.

      with_sum_func being set for an Item means that this Item refers
      (somewhere in it, e.g. one of its arguments if it's a function) directly
      or through intermediate items to an aggregate function that is calculated
      in a context "outside" of the Item (e.g. in the current or outer select).

      with_sum_func being set for an Select_Lex means that this Select_Lex
      has aggregate functions directly referenced (i.e. not through a sub-select).
    */
    for (sl= session->lex().current_select;
         sl && sl != aggr_sel && sl->master_unit()->item;
         sl= sl->master_unit()->outer_select() )
      sl->master_unit()->item->with_sum_func= 1;
  }
  session->lex().current_select->mark_as_dependent(aggr_sel);
  return false;
}


Item_sum::Item_sum(List<Item> &list) :arg_count(list.size()),
  forced_const(false)
{
  if ((args=(Item**) memory::sql_alloc(sizeof(Item*)*arg_count)))
  {
    uint32_t i=0;
    List<Item>::iterator li(list.begin());
    Item *item;

    while ((item=li++))
    {
      args[i++]= item;
    }
  }
  mark_as_sum_func();
  list.clear();					// Fields are used
}


/**
  Constructor used in processing select with temporary tebles.
*/

Item_sum::Item_sum(Session *session, Item_sum *item):
  Item_result_field(session, item), arg_count(item->arg_count),
  aggr_sel(item->aggr_sel),
  nest_level(item->nest_level), aggr_level(item->aggr_level),
  quick_group(item->quick_group), used_tables_cache(item->used_tables_cache),
  forced_const(item->forced_const)
{
  if (arg_count <= 2)
    args= tmp_args;
  else
    args= new (session->mem) Item*[arg_count];
  memcpy(args, item->args, sizeof(Item*)*arg_count);
}


void Item_sum::mark_as_sum_func()
{
  Select_Lex *cur_select= getSession().lex().current_select;
  cur_select->n_sum_items++;
  cur_select->with_sum_func= 1;
  with_sum_func= 1;
}


void Item_sum::make_field(SendField *tmp_field)
{
  if (args[0]->type() == Item::FIELD_ITEM && keep_field_type())
  {
    ((Item_field*) args[0])->field->make_field(tmp_field);
    /* For expressions only col_name should be non-empty string. */
    char *empty_string= (char*)"";
    tmp_field->db_name= empty_string;
    tmp_field->org_table_name= empty_string;
    tmp_field->table_name= empty_string;
    tmp_field->org_col_name= empty_string;
    tmp_field->col_name= name;
    if (maybe_null)
      tmp_field->flags&= ~NOT_NULL_FLAG;
  }
  else
    init_make_field(tmp_field, field_type());
}


void Item_sum::print(String *str)
{
  str->append(func_name());
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
  }
  str->append(')');
}

void Item_sum::fix_num_length_and_dec()
{
  decimals=0;
  for (uint32_t i=0 ; i < arg_count ; i++)
    set_if_bigger(decimals,args[i]->decimals);
  max_length=float_length(decimals);
}

Item *Item_sum::get_tmp_table_item(Session *session)
{
  Item_sum* sum_item= (Item_sum *) copy_or_same(session);
  if (sum_item && sum_item->result_field)	   // If not a const sum func
  {
    Field *result_field_tmp= sum_item->result_field;
    for (uint32_t i=0 ; i < sum_item->arg_count ; i++)
    {
      Item *arg= sum_item->args[i];
      if (!arg->const_item())
      {
	if (arg->type() == Item::FIELD_ITEM)
	  ((Item_field*) arg)->field= result_field_tmp++;
	else
	  sum_item->args[i]= new Item_field(result_field_tmp++);
      }
    }
  }
  return sum_item;
}


bool Item_sum::walk (Item_processor processor, bool walk_subquery,
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


Field *Item_sum::create_tmp_field(bool ,
                                  Table *table,
                                  uint32_t convert_blob_length)
{
  Field *field= NULL;

  switch (result_type()) {
  case REAL_RESULT:
    field= new Field_double(max_length, maybe_null, name, decimals, true);
    break;

  case INT_RESULT:
    field= new field::Int64(max_length, maybe_null, name, unsigned_flag);
    break;

  case STRING_RESULT:
    if (max_length/collation.collation->mbmaxlen <= 255 ||
        convert_blob_length > Field_varstring::MAX_SIZE ||
        !convert_blob_length)
    {
      return make_string_field(table);
    }

    table->setVariableWidth();
    field= new Field_varstring(convert_blob_length, maybe_null,
                               name, collation.collation);
    break;

  case DECIMAL_RESULT:
    field= new Field_decimal(max_length, maybe_null, name,
                             decimals, unsigned_flag);
    break;

  case ROW_RESULT:
    // This case should never be choosen
    assert(0);
    return 0;
  }

  if (field)
    field->init(table);

  return field;
}


void Item_sum::update_used_tables ()
{
  if (!forced_const)
  {
    used_tables_cache= 0;
    for (uint32_t i=0 ; i < arg_count ; i++)
    {
      args[i]->update_used_tables();
      used_tables_cache|= args[i]->used_tables();
    }

    used_tables_cache&= PSEUDO_TABLE_BITS;

    /* the aggregate function is aggregated into its local context */
    used_tables_cache |=  (1 << aggr_sel->join->tables) - 1;
  }
}


String *
Item_sum_num::val_str(String *str)
{
  return val_string_from_real(str);
}


int64_t Item_sum_num::val_int()
{
  assert(fixed == 1);
  return (int64_t) rint(val_real());             /* Real as default */
}


type::Decimal *Item_sum_num::val_decimal(type::Decimal *decimal_value)
{
  return val_decimal_from_real(decimal_value);
}


String *
Item_sum_int::val_str(String *str)
{
  return val_string_from_int(str);
}


type::Decimal *Item_sum_int::val_decimal(type::Decimal *decimal_value)
{
  return val_decimal_from_int(decimal_value);
}


bool
Item_sum_num::fix_fields(Session *session, Item **ref)
{
  assert(fixed == 0);

  if (init_sum_func_check(session))
    return true;

  decimals=0;
  maybe_null=0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    if (args[i]->fix_fields(session, args + i) || args[i]->check_cols(1))
      return true;
    set_if_bigger(decimals, args[i]->decimals);
    maybe_null |= args[i]->maybe_null;
  }
  result_field=0;
  max_length=float_length(decimals);
  null_value=1;
  fix_length_and_dec();

  if (check_sum_func(session, ref))
    return true;

  fixed= 1;
  return false;
}


Item_sum_hybrid::Item_sum_hybrid(Session *session, Item_sum_hybrid *item)
  :Item_sum(session, item), value(item->value), hybrid_type(item->hybrid_type),
  hybrid_field_type(item->hybrid_field_type), cmp_sign(item->cmp_sign),
  was_values(item->was_values)
{
  /* copy results from old value */
  switch (hybrid_type) {
  case INT_RESULT:
    sum_int= item->sum_int;
    break;
  case DECIMAL_RESULT:
    class_decimal2decimal(&item->sum_dec, &sum_dec);
    break;
  case REAL_RESULT:
    sum= item->sum;
    break;
  case STRING_RESULT:
    /*
      This can happen with ROLLUP. Note that the value is already
      copied at function call.
    */
    break;
  case ROW_RESULT:
    assert(0);
  }
  collation.set(item->collation);
}

bool
Item_sum_hybrid::fix_fields(Session *session, Item **ref)
{
  assert(fixed == 0);

  Item *item= args[0];

  if (init_sum_func_check(session))
    return true;

  // 'item' can be changed during fix_fields
  if ((!item->fixed && item->fix_fields(session, args)) ||
      (item= args[0])->check_cols(1))
    return true;
  decimals=item->decimals;

  switch (hybrid_type= item->result_type()) {
  case INT_RESULT:
    max_length= 20;
    sum_int= 0;
    break;
  case DECIMAL_RESULT:
    max_length= item->max_length;
    sum_dec.set_zero();
    break;
  case REAL_RESULT:
    max_length= float_length(decimals);
    sum= 0.0;
    break;
  case STRING_RESULT:
    max_length= item->max_length;
    break;
  case ROW_RESULT:
    assert(0);
  };
  /* MIN/MAX can return NULL for empty set indepedent of the used column */
  maybe_null= 1;
  unsigned_flag=item->unsigned_flag;
  collation.set(item->collation);
  result_field=0;
  null_value=1;
  fix_length_and_dec();
  item= item->real_item();
  if (item->type() == Item::FIELD_ITEM)
    hybrid_field_type= ((Item_field*) item)->field->type();
  else
    hybrid_field_type= Item::field_type();

  if (check_sum_func(session, ref))
    return true;

  fixed= 1;
  return false;
}

Field *Item_sum_hybrid::create_tmp_field(bool group, Table *table,
					 uint32_t convert_blob_length)
{
  Field *field;
  if (args[0]->type() == Item::FIELD_ITEM)
  {
    field= ((Item_field*) args[0])->field;

    if ((field= create_tmp_field_from_field(&getSession(), field, name, table,
					    NULL, convert_blob_length)))
      field->flags&= ~NOT_NULL_FLAG;
    return field;
  }
  /*
    DATE/TIME fields have STRING_RESULT result types.
    In order to preserve field type, it's needed to handle DATE/TIME
    fields creations separately.
  */
  switch (args[0]->field_type()) {
  case DRIZZLE_TYPE_DATE:
    field= new Field_date(maybe_null, name);
    break;
  case DRIZZLE_TYPE_TIMESTAMP:
  case DRIZZLE_TYPE_DATETIME:
    field= new Field_datetime(maybe_null, name);
    break;
  default:
    return Item_sum::create_tmp_field(group, table, convert_blob_length);
  }

  if (field)
    field->init(table);

  return field;
}


/***********************************************************************
** reset and add of sum_func
***********************************************************************/

/**
  @todo
  check if the following assignments are really needed
*/
Item_sum_sum::Item_sum_sum(Session *session, Item_sum_sum *item)
  :Item_sum_num(session, item), hybrid_type(item->hybrid_type),
   curr_dec_buff(item->curr_dec_buff)
{
  /* TODO: check if the following assignments are really needed */
  if (hybrid_type == DECIMAL_RESULT)
  {
    class_decimal2decimal(item->dec_buffs, dec_buffs);
    class_decimal2decimal(item->dec_buffs + 1, dec_buffs + 1);
  }
  else
    sum= item->sum;
}

Item *Item_sum_sum::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_sum(session, this);
}


void Item_sum_sum::clear()
{
  null_value=1;
  if (hybrid_type == DECIMAL_RESULT)
  {
    curr_dec_buff= 0;
    dec_buffs->set_zero();
  }
  else
    sum= 0.0;
  return;
}


void Item_sum_sum::fix_length_and_dec()
{
  maybe_null=null_value=1;
  decimals= args[0]->decimals;
  switch (args[0]->result_type()) {
  case REAL_RESULT:
  case STRING_RESULT:
    hybrid_type= REAL_RESULT;
    sum= 0.0;
    break;
  case INT_RESULT:
  case DECIMAL_RESULT:
    {
      /* SUM result can't be longer than length(arg) + length(MAX_ROWS) */
      int precision= args[0]->decimal_precision() + DECIMAL_LONGLONG_DIGITS;
      max_length= class_decimal_precision_to_length(precision, decimals,
                                                 unsigned_flag);
      curr_dec_buff= 0;
      hybrid_type= DECIMAL_RESULT;
      dec_buffs->set_zero();
      break;
    }
  case ROW_RESULT:
    assert(0);
  }
}


bool Item_sum_sum::add()
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    type::Decimal value, *val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      class_decimal_add(E_DEC_FATAL_ERROR, dec_buffs + (curr_dec_buff^1),
                     val, dec_buffs + curr_dec_buff);
      curr_dec_buff^= 1;
      null_value= 0;
    }
  }
  else
  {
    sum+= args[0]->val_real();
    if (!args[0]->null_value)
      null_value= 0;
  }
  return 0;
}


int64_t Item_sum_sum::val_int()
{
  assert(fixed == 1);
  if (hybrid_type == DECIMAL_RESULT)
  {
    int64_t result;
    (dec_buffs + curr_dec_buff)->val_int32(E_DEC_FATAL_ERROR, unsigned_flag, &result);
    return result;
  }
  return (int64_t) rint(val_real());
}


double Item_sum_sum::val_real()
{
  assert(fixed == 1);
  if (hybrid_type == DECIMAL_RESULT)
    class_decimal2double(E_DEC_FATAL_ERROR, dec_buffs + curr_dec_buff, &sum);
  return sum;
}


String *Item_sum_sum::val_str(String *str)
{
  if (hybrid_type == DECIMAL_RESULT)
    return val_string_from_decimal(str);
  return val_string_from_real(str);
}


type::Decimal *Item_sum_sum::val_decimal(type::Decimal *val)
{
  if (hybrid_type == DECIMAL_RESULT)
    return (dec_buffs + curr_dec_buff);
  return val_decimal_from_real(val);
}

/***************************************************************************/

/* Declarations for auxilary C-callbacks */

static int simple_raw_key_cmp(void* arg, const void* key1, const void* key2)
{
    return memcmp(key1, key2, *(uint32_t *) arg);
}


static int item_sum_distinct_walk(void *element,
                                  uint32_t ,
                                  void *item)
{
  return ((Item_sum_distinct*) (item))->unique_walk_function(element);
}

/* Item_sum_distinct */

Item_sum_distinct::Item_sum_distinct(Item *item_arg)
  :Item_sum_num(item_arg), tree(0)
{
  /*
    quick_group is an optimizer hint, which means that GROUP BY can be
    handled with help of index on grouped columns.
    By setting quick_group to zero we force creation of temporary table
    to perform GROUP BY.
  */
  quick_group= 0;
}


Item_sum_distinct::Item_sum_distinct(Session *session, Item_sum_distinct *original)
  :Item_sum_num(session, original), val(original->val), tree(0),
  table_field_type(original->table_field_type)
{
  quick_group= 0;
}


/**
  Behaves like an Integer except to fix_length_and_dec().
  Additionally div() converts val with this traits to a val with true
  decimal traits along with conversion of integer value to decimal value.
  This is to speedup SUM/AVG(DISTINCT) evaluation for 8-32 bit integer
  values.
*/
struct Hybrid_type_traits_fast_decimal: public
       Hybrid_type_traits_integer
{
  virtual Item_result type() const { return DECIMAL_RESULT; }
  virtual void fix_length_and_dec(Item *item, Item *arg) const
  { Hybrid_type_traits_decimal::instance()->fix_length_and_dec(item, arg); }

  virtual void div(Hybrid_type *val, uint64_t u) const
  {
    int2_class_decimal(E_DEC_FATAL_ERROR, val->integer, 0, val->dec_buf);
    val->used_dec_buf_no= 0;
    val->traits= Hybrid_type_traits_decimal::instance();
    val->traits->div(val, u);
  }
  static const Hybrid_type_traits_fast_decimal *instance();
  Hybrid_type_traits_fast_decimal() {};
};

static const Hybrid_type_traits_fast_decimal fast_decimal_traits_instance;

const Hybrid_type_traits_fast_decimal
  *Hybrid_type_traits_fast_decimal::instance()
{
  return &fast_decimal_traits_instance;
}


void Item_sum_distinct::fix_length_and_dec()
{
  assert(args[0]->fixed);

  null_value= maybe_null= true;
  table_field_type= args[0]->field_type();

  /* Adjust tmp table type according to the chosen aggregation type */
  switch (args[0]->result_type()) {
  case STRING_RESULT:
  case REAL_RESULT:
    val.traits= Hybrid_type_traits::instance();
    table_field_type= DRIZZLE_TYPE_DOUBLE;
    break;
  case INT_RESULT:
    /*
      Preserving int8, int16, int32 field types gives ~10% performance boost
      as the size of result tree becomes significantly smaller.
      Another speed up we gain by using int64_t for intermediate
      calculations. The range of int64 is enough to hold sum 2^32 distinct
      integers each <= 2^32.
    */
    if (table_field_type == DRIZZLE_TYPE_LONG)
    {
      val.traits= Hybrid_type_traits_fast_decimal::instance();
      break;
    }
    table_field_type= DRIZZLE_TYPE_LONGLONG;
    /* fallthrough */
  case DECIMAL_RESULT:
    val.traits= Hybrid_type_traits_decimal::instance();
    if (table_field_type != DRIZZLE_TYPE_LONGLONG)
      table_field_type= DRIZZLE_TYPE_DECIMAL;
    break;
  case ROW_RESULT:
    assert(0);
  }

  val.traits->fix_length_and_dec(this, args[0]);
}


enum Item_result Item_sum_distinct::result_type () const
{
  return val.traits->type();
}


/**
  @todo
  check that the case of CHAR(0) works OK
*/
bool Item_sum_distinct::setup(Session *session)
{
  /* It's legal to call setup() more than once when in a subquery */
  if (tree)
    return false;

  /*
    Virtual table and the tree are created anew on each re-execution of
    PS/SP. Hence all further allocations are performed in the runtime
    mem.
  */
  null_value= maybe_null= 1;
  quick_group= 0;

  assert(args[0]->fixed);

  std::list<CreateField> field_list;
  field_list.push_back(CreateField());
  CreateField& field_def = field_list.back();
  field_def.init_for_tmp_table(table_field_type, args[0]->max_length, args[0]->decimals, args[0]->maybe_null);
  table= &session->getInstanceTable(field_list);

  /* XXX: check that the case of CHAR(0) works OK */
  tree_key_length= table->getShare()->getRecordLength() - table->getShare()->null_bytes;

  /*
    Unique handles all unique elements in a tree until they can't fit
    in.  Then the tree is dumped to the temporary file. We can use
    simple_raw_key_cmp because the table contains numbers only; decimals
    are converted to binary representation as well.
  */
  tree= new Unique(simple_raw_key_cmp, &tree_key_length, tree_key_length, (size_t)session->variables.max_heap_table_size);
  is_evaluated= false;
  return false;
}

bool Item_sum_distinct::add()
{
  args[0]->save_in_field(table->getField(0), false);
  is_evaluated= false;
  if (!table->getField(0)->is_null())
  {
    assert(tree);
    null_value= 0;
    /*
      '0' values are also stored in the tree. This doesn't matter
      for SUM(DISTINCT), but is important for AVG(DISTINCT)
    */
    return tree->unique_add(table->getField(0)->ptr);
  }
  return 0;
}


bool Item_sum_distinct::unique_walk_function(void *element)
{
  memcpy(table->getField(0)->ptr, element, tree_key_length);
  ++count;
  val.traits->add(&val, table->getField(0));
  return 0;
}


void Item_sum_distinct::clear()
{
  assert(tree != 0);                        /* we always have a tree */
  null_value= 1;
  tree->reset();
  is_evaluated= false;
  return;
}

void Item_sum_distinct::cleanup()
{
  Item_sum_num::cleanup();
  delete tree;
  tree= 0;
  table= 0;
  is_evaluated= false;
}

Item_sum_distinct::~Item_sum_distinct()
{
  delete tree;
  /* no need to free the table */
}


void Item_sum_distinct::calculate_val_and_count()
{
  if (!is_evaluated)
  {
    count= 0;
    val.traits->set_zero(&val);
    /*
      We don't have a tree only if 'setup()' hasn't been called;
      this is the case of sql_select.cc:return_zero_rows.
     */
    if (tree)
    {
      table->getField(0)->set_notnull();
      tree->walk(item_sum_distinct_walk, (void*) this);
    }
    is_evaluated= true;
  }
}


double Item_sum_distinct::val_real()
{
  calculate_val_and_count();
  return val.traits->val_real(&val);
}


type::Decimal *Item_sum_distinct::val_decimal(type::Decimal *to)
{
  calculate_val_and_count();
  if (null_value)
    return 0;
  return val.traits->val_decimal(&val, to);
}


int64_t Item_sum_distinct::val_int()
{
  calculate_val_and_count();
  return val.traits->val_int(&val, unsigned_flag);
}


String *Item_sum_distinct::val_str(String *str)
{
  calculate_val_and_count();
  if (null_value)
    return 0;
  return val.traits->val_str(&val, str, decimals);
}

/* end of Item_sum_distinct */

/* Item_sum_avg_distinct */

void
Item_sum_avg_distinct::fix_length_and_dec()
{
  Item_sum_distinct::fix_length_and_dec();
  prec_increment= getSession().variables.div_precincrement;
  /*
    AVG() will divide val by count. We need to reserve digits
    after decimal point as the result can be fractional.
  */
  decimals= min(decimals + prec_increment, (unsigned int)NOT_FIXED_DEC);
}


void
Item_sum_avg_distinct::calculate_val_and_count()
{
  if (!is_evaluated)
  {
    Item_sum_distinct::calculate_val_and_count();
    if (count)
      val.traits->div(&val, count);
    is_evaluated= true;
  }
}


Item *Item_sum_count::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_count(session, this);
}


void Item_sum_count::clear()
{
  count= 0;
}


bool Item_sum_count::add()
{
  if (!args[0]->maybe_null || !args[0]->is_null())
    count++;
  return 0;
}

int64_t Item_sum_count::val_int()
{
  assert(fixed == 1);
  return (int64_t) count;
}


void Item_sum_count::cleanup()
{
  count= 0;
  Item_sum_int::cleanup();
  return;
}


/*
  Avgerage
*/
void Item_sum_avg::fix_length_and_dec()
{
  Item_sum_sum::fix_length_and_dec();
  maybe_null=null_value=1;
  prec_increment= getSession().variables.div_precincrement;

  if (hybrid_type == DECIMAL_RESULT)
  {
    int precision= args[0]->decimal_precision() + prec_increment;
    decimals= min(args[0]->decimals + prec_increment, (unsigned int) DECIMAL_MAX_SCALE);
    max_length= class_decimal_precision_to_length(precision, decimals,
                                               unsigned_flag);
    f_precision= min(precision+DECIMAL_LONGLONG_DIGITS, DECIMAL_MAX_PRECISION);
    f_scale=  args[0]->decimals;
    dec_bin_size= class_decimal_get_binary_size(f_precision, f_scale);
  }
  else {
    decimals= min(args[0]->decimals + prec_increment, (unsigned int) NOT_FIXED_DEC);
    max_length= args[0]->max_length + prec_increment;
  }
}


Item *Item_sum_avg::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_avg(session, this);
}


Field *Item_sum_avg::create_tmp_field(bool group, Table *table,
                                      uint32_t )
{
  Field *field;
  if (group)
  {
    /*
      We must store both value and counter in the temporary table in one field.
      The easiest way is to do this is to store both value in a string
      and unpack on access.
    */
    table->setVariableWidth();
    field= new Field_varstring(((hybrid_type == DECIMAL_RESULT) ?
                                dec_bin_size : sizeof(double)) + sizeof(int64_t),
                               0, name, &my_charset_bin);
  }
  else if (hybrid_type == DECIMAL_RESULT)
    field= new Field_decimal(max_length, maybe_null, name,
                             decimals, unsigned_flag);
  else
    field= new Field_double(max_length, maybe_null, name, decimals, true);
  if (field)
    field->init(table);
  return field;
}


void Item_sum_avg::clear()
{
  Item_sum_sum::clear();
  count=0;
}


bool Item_sum_avg::add()
{
  if (Item_sum_sum::add())
    return true;
  if (!args[0]->null_value)
    count++;
  return false;
}

double Item_sum_avg::val_real()
{
  assert(fixed == 1);
  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  return Item_sum_sum::val_real() / uint64_t2double(count);
}


int64_t Item_sum_avg::val_int()
{
  return (int64_t) rint(val_real());
}


type::Decimal *Item_sum_avg::val_decimal(type::Decimal *val)
{
  type::Decimal sum_buff, cnt;
  const type::Decimal *sum_dec;
  assert(fixed == 1);
  if (!count)
  {
    null_value=1;
    return NULL;
  }

  /*
    For non-DECIMAL hybrid_type the division will be done in
    Item_sum_avg::val_real().
  */
  if (hybrid_type != DECIMAL_RESULT)
    return val_decimal_from_real(val);

  sum_dec= dec_buffs + curr_dec_buff;
  int2_class_decimal(E_DEC_FATAL_ERROR, count, 0, &cnt);
  class_decimal_div(E_DEC_FATAL_ERROR, val, sum_dec, &cnt, prec_increment);
  return val;
}


String *Item_sum_avg::val_str(String *str)
{
  if (hybrid_type == DECIMAL_RESULT)
    return val_string_from_decimal(str);
  return val_string_from_real(str);
}


/*
  Standard deviation
*/

double Item_sum_std::val_real()
{
  assert(fixed == 1);
  double nr= Item_sum_variance::val_real();
  assert(nr >= 0.0);
  return sqrt(nr);
}

Item *Item_sum_std::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_std(session, this);
}


/*
  Variance
*/


/**
  Variance implementation for floating-point implementations, without
  catastrophic cancellation, from Knuth's _TAoCP_, 3rd ed, volume 2, pg232.
  This alters the value at m, s, and increments count.
*/

/*
  These two functions are used by the Item_sum_variance and the
  Item_variance_field classes, which are unrelated, and each need to calculate
  variance.  The difference between the two classes is that the first is used
  for a mundane SELECT, while the latter is used in a GROUPing SELECT.
*/
static void variance_fp_recurrence_next(double *m, double *s, uint64_t *count, double nr)
{
  *count += 1;

  if (*count == 1)
  {
    *m= nr;
    *s= 0;
  }
  else
  {
    double m_kminusone= *m;
    *m= m_kminusone + (nr - m_kminusone) / (double) *count;
    *s= *s + (nr - m_kminusone) * (nr - *m);
  }
}


static double variance_fp_recurrence_result(double s, uint64_t count, bool is_sample_variance)
{
  if (count == 1)
    return 0.0;

  if (is_sample_variance)
    return s / (count - 1);

  /* else, is a population variance */
  return s / count;
}


Item_sum_variance::Item_sum_variance(Session *session, Item_sum_variance *item):
  Item_sum_num(session, item), hybrid_type(item->hybrid_type),
    count(item->count), sample(item->sample),
    prec_increment(item->prec_increment)
{
  recurrence_m= item->recurrence_m;
  recurrence_s= item->recurrence_s;
}


void Item_sum_variance::fix_length_and_dec()
{
  maybe_null= null_value= 1;
  prec_increment= getSession().variables.div_precincrement;

  /*
    According to the SQL2003 standard (Part 2, Foundations; sec 10.9,
    aggregate function; paragraph 7h of Syntax Rules), "the declared
    type of the result is an implementation-defined aproximate numeric
    type.
  */
  hybrid_type= REAL_RESULT;

  switch (args[0]->result_type()) {
  case REAL_RESULT:
  case STRING_RESULT:
    decimals= min(args[0]->decimals + 4, (int)NOT_FIXED_DEC);
    break;
  case INT_RESULT:
  case DECIMAL_RESULT:
    {
      int precision= args[0]->decimal_precision()*2 + prec_increment;
      decimals= min(args[0]->decimals + prec_increment, (unsigned int) DECIMAL_MAX_SCALE);
      max_length= class_decimal_precision_to_length(precision, decimals,
                                                 unsigned_flag);

      break;
    }
  case ROW_RESULT:
    assert(0);
  }
}


Item *Item_sum_variance::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_variance(session, this);
}


/**
  Create a new field to match the type of value we're expected to yield.
  If we're grouping, then we need some space to serialize variables into, to
  pass around.
*/
Field *Item_sum_variance::create_tmp_field(bool group, Table *table,
                                           uint32_t )
{
  Field *field;
  if (group)
  {
    /*
      We must store both value and counter in the temporary table in one field.
      The easiest way is to do this is to store both value in a string
      and unpack on access.
    */
    table->setVariableWidth();
    field= new Field_varstring(sizeof(double)*2 + sizeof(int64_t), 0, name, &my_charset_bin);
  }
  else
    field= new Field_double(max_length, maybe_null, name, decimals, true);

  if (field != NULL)
    field->init(table);

  return field;
}


void Item_sum_variance::clear()
{
  count= 0;
}

bool Item_sum_variance::add()
{
  /*
    Why use a temporary variable?  We don't know if it is null until we
    evaluate it, which has the side-effect of setting null_value .
  */
  double nr= args[0]->val_real();

  if (!args[0]->null_value)
    variance_fp_recurrence_next(&recurrence_m, &recurrence_s, &count, nr);
  return 0;
}

double Item_sum_variance::val_real()
{
  assert(fixed == 1);

  /*
    'sample' is a 1/0 boolean value.  If it is 1/true, id est this is a sample
    variance call, then we should set nullness when the count of the items
    is one or zero.  If it's zero, i.e. a population variance, then we only
    set nullness when the count is zero.

    Another way to read it is that 'sample' is the numerical threshhold, at and
    below which a 'count' number of items is called NULL.
  */
  assert((sample == 0) || (sample == 1));
  if (count <= sample)
  {
    null_value=1;
    return 0.0;
  }

  null_value=0;
  return variance_fp_recurrence_result(recurrence_s, count, sample);
}


int64_t Item_sum_variance::val_int()
{
  /* can't be fix_fields()ed */
  return (int64_t) rint(val_real());
}


type::Decimal *Item_sum_variance::val_decimal(type::Decimal *dec_buf)
{
  assert(fixed == 1);
  return val_decimal_from_real(dec_buf);
}


void Item_sum_variance::reset_field()
{
  double nr;
  unsigned char *res= result_field->ptr;

  nr= args[0]->val_real();              /* sets null_value as side-effect */

  if (args[0]->null_value)
    memset(res, 0, sizeof(double)*2+sizeof(int64_t));
  else
  {
    /* Serialize format is (double)m, (double)s, (int64_t)count */
    uint64_t tmp_count;
    double tmp_s;
    float8store(res, nr);               /* recurrence variable m */
    tmp_s= 0.0;
    float8store(res + sizeof(double), tmp_s);
    tmp_count= 1;
    int8store(res + sizeof(double)*2, tmp_count);
  }
}


void Item_sum_variance::update_field()
{
  uint64_t field_count;
  unsigned char *res=result_field->ptr;

  double nr= args[0]->val_real();       /* sets null_value as side-effect */

  if (args[0]->null_value)
    return;

  /* Serialize format is (double)m, (double)s, (int64_t)count */
  double field_recurrence_m, field_recurrence_s;
  float8get(field_recurrence_m, res);
  float8get(field_recurrence_s, res + sizeof(double));
  field_count=sint8korr(res+sizeof(double)*2);

  variance_fp_recurrence_next(&field_recurrence_m, &field_recurrence_s, &field_count, nr);

  float8store(res, field_recurrence_m);
  float8store(res + sizeof(double), field_recurrence_s);
  res+= sizeof(double)*2;
  int8store(res,field_count);
}


/* min & max */

void Item_sum_hybrid::clear()
{
  switch (hybrid_type) {
  case INT_RESULT:
    sum_int= 0;
    break;
  case DECIMAL_RESULT:
    sum_dec.set_zero();
    break;
  case REAL_RESULT:
    sum= 0.0;
    break;
  default:
    value.length(0);
  }
  null_value= 1;
}

double Item_sum_hybrid::val_real()
{
  assert(fixed == 1);
  if (null_value)
    return 0.0;

  switch (hybrid_type) {
  case STRING_RESULT:
    {
      char *end_not_used;
      int err_not_used;
      String *res;  res=val_str(&str_value);
      return (res ? my_strntod(res->charset(), (char*) res->ptr(), res->length(),
                               &end_not_used, &err_not_used) : 0.0);
    }
  case INT_RESULT:
    return (double) sum_int;
  case DECIMAL_RESULT:
    class_decimal2double(E_DEC_FATAL_ERROR, &sum_dec, &sum);
    return sum;
  case REAL_RESULT:
    return sum;
  case ROW_RESULT:
    // This case should never be choosen
    break;
  }

  assert(0);
  return 0;
}

int64_t Item_sum_hybrid::val_int()
{
  assert(fixed == 1);
  if (null_value)
    return 0;
  switch (hybrid_type) {
  case INT_RESULT:
    return sum_int;
  case DECIMAL_RESULT:
  {
    int64_t result;
    sum_dec.val_int32(E_DEC_FATAL_ERROR, unsigned_flag, &result);
    return sum_int;
  }
  default:
    return (int64_t) rint(Item_sum_hybrid::val_real());
  }
}


type::Decimal *Item_sum_hybrid::val_decimal(type::Decimal *val)
{
  assert(fixed == 1);
  if (null_value)
    return 0;

  switch (hybrid_type) {
  case STRING_RESULT:
    val->store(E_DEC_FATAL_ERROR, &value);
    break;
  case REAL_RESULT:
    double2_class_decimal(E_DEC_FATAL_ERROR, sum, val);
    break;
  case DECIMAL_RESULT:
    val= &sum_dec;
    break;
  case INT_RESULT:
    int2_class_decimal(E_DEC_FATAL_ERROR, sum_int, unsigned_flag, val);
    break;
  case ROW_RESULT:
    // This case should never be choosen
    assert(0);
    break;
  }

  return val;					// Keep compiler happy
}


String *
Item_sum_hybrid::val_str(String *str)
{
  assert(fixed == 1);
  if (null_value)
    return 0;

  switch (hybrid_type) {
  case STRING_RESULT:
    return &value;
  case REAL_RESULT:
    str->set_real(sum,decimals, &my_charset_bin);
    break;
  case DECIMAL_RESULT:
    class_decimal2string(&sum_dec, 0, str);
    return str;
  case INT_RESULT:
    str->set_int(sum_int, unsigned_flag, &my_charset_bin);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    break;
  }

  return str;					// Keep compiler happy
}


void Item_sum_hybrid::cleanup()
{
  Item_sum::cleanup();
  forced_const= false;

  /*
    by default it is TRUE to avoid TRUE reporting by
    Item_func_not_all/Item_func_nop_all if this item was never called.

    no_rows_in_result() set it to FALSE if was not results found.
    If some results found it will be left unchanged.
  */
  was_values= true;
  return;
}

void Item_sum_hybrid::no_rows_in_result()
{
  was_values= false;
  clear();
}


Item *Item_sum_min::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_min(session, this);
}


bool Item_sum_min::add()
{
  switch (hybrid_type) {
  case STRING_RESULT:
    {
      String *result=args[0]->val_str(&tmp_value);
      if (!args[0]->null_value &&
          (null_value || sortcmp(&value,result,collation.collation) > 0))
      {
        value.copy(*result);
        null_value=0;
      }
    }
    break;
  case INT_RESULT:
    {
      int64_t nr=args[0]->val_int();
      if (!args[0]->null_value && (null_value ||
                                   (unsigned_flag &&
                                    (uint64_t) nr < (uint64_t) sum_int) ||
                                   (!unsigned_flag && nr < sum_int)))
      {
        sum_int=nr;
        null_value=0;
      }
    }
    break;
  case DECIMAL_RESULT:
    {
      type::Decimal value_buff, *val= args[0]->val_decimal(&value_buff);
      if (!args[0]->null_value &&
          (null_value || (class_decimal_cmp(&sum_dec, val) > 0)))
      {
        class_decimal2decimal(val, &sum_dec);
        null_value= 0;
      }
    }
    break;
  case REAL_RESULT:
    {
      double nr= args[0]->val_real();
      if (!args[0]->null_value && (null_value || nr < sum))
      {
        sum=nr;
        null_value=0;
      }
    }
    break;
  case ROW_RESULT:
    // This case should never be choosen
    assert(0);
    break;
  }
  return 0;
}


Item *Item_sum_max::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_max(session, this);
}


bool Item_sum_max::add()
{
  switch (hybrid_type) {
  case STRING_RESULT:
    {
      String *result=args[0]->val_str(&tmp_value);
      if (!args[0]->null_value &&
          (null_value || sortcmp(&value,result,collation.collation) < 0))
      {
        value.copy(*result);
        null_value=0;
      }
    }
    break;
  case INT_RESULT:
    {
      int64_t nr=args[0]->val_int();
      if (!args[0]->null_value && (null_value ||
                                   (unsigned_flag &&
                                    (uint64_t) nr > (uint64_t) sum_int) ||
                                   (!unsigned_flag && nr > sum_int)))
      {
        sum_int=nr;
        null_value=0;
      }
    }
    break;
  case DECIMAL_RESULT:
    {
      type::Decimal value_buff, *val= args[0]->val_decimal(&value_buff);
      if (!args[0]->null_value &&
          (null_value || (class_decimal_cmp(val, &sum_dec) > 0)))
      {
        class_decimal2decimal(val, &sum_dec);
        null_value= 0;
      }
    }
    break;
  case REAL_RESULT:
    {
      double nr= args[0]->val_real();
      if (!args[0]->null_value && (null_value || nr > sum))
      {
        sum=nr;
        null_value=0;
      }
    }
    break;
  case ROW_RESULT:
    // This case should never be choosen
    assert(0);
    break;
  }

  return 0;
}


/* bit_or and bit_and */

int64_t Item_sum_bit::val_int()
{
  assert(fixed == 1);
  return (int64_t) bits;
}


void Item_sum_bit::clear()
{
  bits= reset_bits;
}

Item *Item_sum_or::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_or(session, this);
}


bool Item_sum_or::add()
{
  uint64_t value= (uint64_t) args[0]->val_int();
  if (!args[0]->null_value)
    bits|=value;
  return 0;
}

Item *Item_sum_xor::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_xor(session, this);
}


bool Item_sum_xor::add()
{
  uint64_t value= (uint64_t) args[0]->val_int();
  if (!args[0]->null_value)
    bits^=value;
  return 0;
}

Item *Item_sum_and::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_and(session, this);
}


bool Item_sum_and::add()
{
  uint64_t value= (uint64_t) args[0]->val_int();
  if (!args[0]->null_value)
    bits&=value;
  return 0;
}

/************************************************************************
** reset result of a Item_sum with is saved in a tmp_table
*************************************************************************/

void Item_sum_num::reset_field()
{
  double nr= args[0]->val_real();
  unsigned char *res=result_field->ptr;

  if (maybe_null)
  {
    if (args[0]->null_value)
    {
      nr=0.0;
      result_field->set_null();
    }
    else
      result_field->set_notnull();
  }
  float8store(res,nr);
}


void Item_sum_hybrid::reset_field()
{
  switch(hybrid_type) {
  case STRING_RESULT:
    {
      char buff[MAX_FIELD_WIDTH];
      String tmp(buff,sizeof(buff),result_field->charset()),*res;

      res=args[0]->val_str(&tmp);
      if (args[0]->null_value)
      {
        result_field->set_null();
        result_field->reset();
      }
      else
      {
        result_field->set_notnull();
        result_field->store(res->ptr(),res->length(),tmp.charset());
      }
      break;
    }
  case INT_RESULT:
    {
      int64_t nr=args[0]->val_int();

      if (maybe_null)
      {
        if (args[0]->null_value)
        {
          nr=0;
          result_field->set_null();
        }
        else
          result_field->set_notnull();
      }
      result_field->store(nr, unsigned_flag);
      break;
    }
  case REAL_RESULT:
    {
      double nr= args[0]->val_real();

      if (maybe_null)
      {
        if (args[0]->null_value)
        {
          nr=0.0;
          result_field->set_null();
        }
        else
          result_field->set_notnull();
      }
      result_field->store(nr);
      break;
    }
  case DECIMAL_RESULT:
    {
      type::Decimal value_buff, *arg_dec= args[0]->val_decimal(&value_buff);

      if (maybe_null)
      {
        if (args[0]->null_value)
          result_field->set_null();
        else
          result_field->set_notnull();
      }
      /*
        We must store zero in the field as we will use the field value in
        add()
      */
      if (!arg_dec)                               // Null
        arg_dec= &decimal_zero;
      result_field->store_decimal(arg_dec);
      break;
    }
  case ROW_RESULT:
    assert(0);
  }
}


void Item_sum_sum::reset_field()
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    type::Decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!arg_val)                               // Null
      arg_val= &decimal_zero;
    result_field->store_decimal(arg_val);
  }
  else
  {
    assert(hybrid_type == REAL_RESULT);
    double nr= args[0]->val_real();			// Nulls also return 0
    float8store(result_field->ptr, nr);
  }
  if (args[0]->null_value)
    result_field->set_null();
  else
    result_field->set_notnull();
}


void Item_sum_count::reset_field()
{
  unsigned char *res=result_field->ptr;
  int64_t nr=0;

  if (!args[0]->maybe_null || !args[0]->is_null())
    nr=1;
  int8store(res,nr);
}


void Item_sum_avg::reset_field()
{
  unsigned char *res=result_field->ptr;
  if (hybrid_type == DECIMAL_RESULT)
  {
    int64_t tmp;
    type::Decimal value, *arg_dec= args[0]->val_decimal(&value);
    if (args[0]->null_value)
    {
      arg_dec= &decimal_zero;
      tmp= 0;
    }
    else
      tmp= 1;
    arg_dec->val_binary(E_DEC_FATAL_ERROR, res, f_precision, f_scale);
    res+= dec_bin_size;
    int8store(res, tmp);
  }
  else
  {
    double nr= args[0]->val_real();

    if (args[0]->null_value)
      memset(res, 0, sizeof(double)+sizeof(int64_t));
    else
    {
      int64_t tmp= 1;
      float8store(res,nr);
      res+=sizeof(double);
      int8store(res,tmp);
    }
  }
}


void Item_sum_bit::reset_field()
{
  reset();
  int8store(result_field->ptr, bits);
}

void Item_sum_bit::update_field()
{
  unsigned char *res=result_field->ptr;
  bits= uint8korr(res);
  add();
  int8store(res, bits);
}


/**
  calc next value and merge it with field_value.
*/

void Item_sum_sum::update_field()
{
  if (hybrid_type == DECIMAL_RESULT)
  {
    type::Decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      if (!result_field->is_null())
      {
        type::Decimal field_value,
                   *field_val= result_field->val_decimal(&field_value);
        class_decimal_add(E_DEC_FATAL_ERROR, dec_buffs, arg_val, field_val);
        result_field->store_decimal(dec_buffs);
      }
      else
      {
        result_field->store_decimal(arg_val);
        result_field->set_notnull();
      }
    }
  }
  else
  {
    double old_nr,nr;
    unsigned char *res=result_field->ptr;

    float8get(old_nr,res);
    nr= args[0]->val_real();
    if (!args[0]->null_value)
    {
      old_nr+=nr;
      result_field->set_notnull();
    }
    float8store(res,old_nr);
  }
}


void Item_sum_count::update_field()
{
  int64_t nr;
  unsigned char *res=result_field->ptr;

  nr=sint8korr(res);
  if (!args[0]->maybe_null || !args[0]->is_null())
    nr++;
  int8store(res,nr);
}


void Item_sum_avg::update_field()
{
  int64_t field_count;
  unsigned char *res=result_field->ptr;
  if (hybrid_type == DECIMAL_RESULT)
  {
    type::Decimal value, *arg_val= args[0]->val_decimal(&value);
    if (!args[0]->null_value)
    {
      binary2_class_decimal(E_DEC_FATAL_ERROR, res,
                        dec_buffs + 1, f_precision, f_scale);
      field_count= sint8korr(res + dec_bin_size);
      class_decimal_add(E_DEC_FATAL_ERROR, dec_buffs, arg_val, dec_buffs + 1);
      dec_buffs->val_binary(E_DEC_FATAL_ERROR, res, f_precision, f_scale);
      res+= dec_bin_size;
      field_count++;
      int8store(res, field_count);
    }
  }
  else
  {
    double nr;

    nr= args[0]->val_real();
    if (!args[0]->null_value)
    {
      double old_nr;
      float8get(old_nr, res);
      field_count= sint8korr(res + sizeof(double));
      old_nr+= nr;
      float8store(res,old_nr);
      res+= sizeof(double);
      field_count++;
      int8store(res, field_count);
    }
  }
}


void Item_sum_hybrid::update_field()
{
  switch (hybrid_type) {
  case STRING_RESULT:
    min_max_update_str_field();
    break;
  case INT_RESULT:
    min_max_update_int_field();
    break;
  case DECIMAL_RESULT:
    min_max_update_decimal_field();
    break;
  case REAL_RESULT:
  case ROW_RESULT:
    min_max_update_real_field();
  }
}


void
Item_sum_hybrid::min_max_update_str_field()
{
  String *res_str=args[0]->val_str(&value);

  if (!args[0]->null_value)
  {
    result_field->val_str_internal(&tmp_value);

    if (result_field->is_null() ||
	(cmp_sign * sortcmp(res_str,&tmp_value,collation.collation)) < 0)
      result_field->store(res_str->ptr(),res_str->length(),res_str->charset());
    result_field->set_notnull();
  }
}


void
Item_sum_hybrid::min_max_update_real_field()
{
  double nr,old_nr;

  old_nr=result_field->val_real();
  nr= args[0]->val_real();
  if (!args[0]->null_value)
  {
    if (result_field->is_null(0) ||
	(cmp_sign > 0 ? old_nr > nr : old_nr < nr))
      old_nr=nr;
    result_field->set_notnull();
  }
  else if (result_field->is_null(0))
    result_field->set_null();
  result_field->store(old_nr);
}


void
Item_sum_hybrid::min_max_update_int_field()
{
  int64_t nr,old_nr;

  old_nr=result_field->val_int();
  nr=args[0]->val_int();
  if (!args[0]->null_value)
  {
    if (result_field->is_null(0))
      old_nr=nr;
    else
    {
      bool res=(unsigned_flag ?
		(uint64_t) old_nr > (uint64_t) nr :
		old_nr > nr);
      /* (cmp_sign > 0 && res) || (!(cmp_sign > 0) && !res) */
      if ((cmp_sign > 0) ^ (!res))
	old_nr=nr;
    }
    result_field->set_notnull();
  }
  else if (result_field->is_null(0))
    result_field->set_null();
  result_field->store(old_nr, unsigned_flag);
}


/**
  @todo
  optimize: do not get result_field in case of args[0] is NULL
*/
void
Item_sum_hybrid::min_max_update_decimal_field()
{
  /* TODO: optimize: do not get result_field in case of args[0] is NULL */
  type::Decimal old_val, nr_val;
  const type::Decimal *old_nr= result_field->val_decimal(&old_val);
  const type::Decimal *nr= args[0]->val_decimal(&nr_val);
  if (!args[0]->null_value)
  {
    if (result_field->is_null(0))
      old_nr=nr;
    else
    {
      bool res= class_decimal_cmp(old_nr, nr) > 0;
      /* (cmp_sign > 0 && res) || (!(cmp_sign > 0) && !res) */
      if ((cmp_sign > 0) ^ (!res))
        old_nr=nr;
    }
    result_field->set_notnull();
  }
  else if (result_field->is_null(0))
    result_field->set_null();
  result_field->store_decimal(old_nr);
}


Item_avg_field::Item_avg_field(Item_result res_type, Item_sum_avg *item)
{
  name=item->name;
  decimals=item->decimals;
  max_length= item->max_length;
  unsigned_flag= item->unsigned_flag;
  field=item->result_field;
  maybe_null=1;
  hybrid_type= res_type;
  prec_increment= item->prec_increment;
  if (hybrid_type == DECIMAL_RESULT)
  {
    f_scale= item->f_scale;
    f_precision= item->f_precision;
    dec_bin_size= item->dec_bin_size;
  }
}

double Item_avg_field::val_real()
{
  // fix_fields() never calls for this Item
  double nr;
  int64_t count;
  unsigned char *res;

  if (hybrid_type == DECIMAL_RESULT)
    return val_real_from_decimal();

  float8get(nr,field->ptr);
  res= (field->ptr+sizeof(double));
  count= sint8korr(res);

  if ((null_value= !count))
    return 0.0;
  return nr/(double) count;
}


int64_t Item_avg_field::val_int()
{
  return (int64_t) rint(val_real());
}


type::Decimal *Item_avg_field::val_decimal(type::Decimal *dec_buf)
{
  // fix_fields() never calls for this Item
  if (hybrid_type == REAL_RESULT)
    return val_decimal_from_real(dec_buf);

  int64_t count= sint8korr(field->ptr + dec_bin_size);
  if ((null_value= !count))
    return 0;

  type::Decimal dec_count, dec_field;
  binary2_class_decimal(E_DEC_FATAL_ERROR,
                    field->ptr, &dec_field, f_precision, f_scale);
  int2_class_decimal(E_DEC_FATAL_ERROR, count, 0, &dec_count);
  class_decimal_div(E_DEC_FATAL_ERROR, dec_buf,
                 &dec_field, &dec_count, prec_increment);
  return dec_buf;
}


String *Item_avg_field::val_str(String *str)
{
  // fix_fields() never calls for this Item
  if (hybrid_type == DECIMAL_RESULT)
    return val_string_from_decimal(str);
  return val_string_from_real(str);
}


Item_std_field::Item_std_field(Item_sum_std *item)
  : Item_variance_field(item)
{
}


double Item_std_field::val_real()
{
  double nr;
  // fix_fields() never calls for this Item
  nr= Item_variance_field::val_real();
  assert(nr >= 0.0);
  return sqrt(nr);
}


type::Decimal *Item_std_field::val_decimal(type::Decimal *dec_buf)
{
  /*
    We can't call val_decimal_from_real() for DECIMAL_RESULT as
    Item_variance_field::val_real() would cause an infinite loop
  */
  type::Decimal tmp_dec, *dec;
  double nr;
  if (hybrid_type == REAL_RESULT)
    return val_decimal_from_real(dec_buf);

  dec= Item_variance_field::val_decimal(dec_buf);
  if (!dec)
    return 0;
  class_decimal2double(E_DEC_FATAL_ERROR, dec, &nr);
  assert(nr >= 0.0);
  nr= sqrt(nr);
  double2_class_decimal(E_DEC_FATAL_ERROR, nr, &tmp_dec);
  class_decimal_round(E_DEC_FATAL_ERROR, &tmp_dec, decimals, false, dec_buf);
  return dec_buf;
}


Item_variance_field::Item_variance_field(Item_sum_variance *item)
{
  name=item->name;
  decimals=item->decimals;
  max_length=item->max_length;
  unsigned_flag= item->unsigned_flag;
  field=item->result_field;
  maybe_null=1;
  sample= item->sample;
  prec_increment= item->prec_increment;
  if ((hybrid_type= item->hybrid_type) == DECIMAL_RESULT)
  {
    f_scale0= item->f_scale0;
    f_precision0= item->f_precision0;
    dec_bin_size0= item->dec_bin_size0;
    f_scale1= item->f_scale1;
    f_precision1= item->f_precision1;
    dec_bin_size1= item->dec_bin_size1;
  }
}


int64_t Item_variance_field::val_int()
{
  /* can't be fix_fields()ed */
  return (int64_t) rint(val_real());
}


double Item_variance_field::val_real()
{
  // fix_fields() never calls for this Item
  if (hybrid_type == DECIMAL_RESULT)
    return val_real_from_decimal();

  double recurrence_s;
  uint64_t count;
  float8get(recurrence_s, (field->ptr + sizeof(double)));
  count=sint8korr(field->ptr+sizeof(double)*2);

  if ((null_value= (count <= sample)))
    return 0.0;

  return variance_fp_recurrence_result(recurrence_s, count, sample);
}


/****************************************************************************
** COUNT(DISTINCT ...)
****************************************************************************/

int simple_str_key_cmp(void* arg, unsigned char* key1, unsigned char* key2)
{
  Field *f= (Field*) arg;
  return f->cmp(key1, key2);
}

/**
  Did not make this one static - at least gcc gets confused when
  I try to declare a static function as a friend. If you can figure
  out the syntax to make a static function a friend, make this one
  static
*/

int composite_key_cmp(void* arg, unsigned char* key1, unsigned char* key2)
{
  Item_sum_count_distinct* item = (Item_sum_count_distinct*)arg;
  Field **field    = item->table->getFields();
  Field **field_end= field + item->table->getShare()->sizeFields();
  uint32_t *lengths=item->field_lengths;
  for (; field < field_end; ++field)
  {
    Field* f = *field;
    int len = *lengths++;
    int res = f->cmp(key1, key2);
    if (res)
      return res;
    key1 += len;
    key2 += len;
  }
  return 0;
}

static int count_distinct_walk(void *,
                               uint32_t ,
                               void *arg)
{
  (*((uint64_t*)arg))++;
  return 0;
}

void Item_sum_count_distinct::cleanup()
{
  Item_sum_int::cleanup();

  /* Free objects only if we own them. */
  if (!original)
  {
    /*
      We need to delete the table and the tree in cleanup() as
      they were allocated in the runtime memroot. Using the runtime
      memroot reduces memory footprint for PS/SP and simplifies setup().
    */
    delete tree;
    tree= 0;
    is_evaluated= false;
    if (table)
    {
      table= 0;
    }
    delete tmp_table_param;
    tmp_table_param= 0;
  }
  always_null= false;
  return;
}


/**
  This is used by rollup to create a separate usable copy of
  the function.
*/

void Item_sum_count_distinct::make_unique()
{
  table=0;
  original= 0;
  force_copy_fields= 1;
  tree= 0;
  is_evaluated= false;
  tmp_table_param= 0;
  always_null= false;
}


Item_sum_count_distinct::~Item_sum_count_distinct()
{
  cleanup();
}


bool Item_sum_count_distinct::setup(Session *session)
{
  List<Item> list;
  Select_Lex *select_lex= session->lex().current_select;

  /*
    Setup can be called twice for ROLLUP items. This is a bug.
    Please add assert(tree == 0) here when it's fixed.
    It's legal to call setup() more than once when in a subquery
  */
  if (tree || table || tmp_table_param)
    return false;

  tmp_table_param= new Tmp_Table_Param;

  /* Create a table with an unique key over all parameters */
  for (uint32_t i=0; i < arg_count ; i++)
  {
    Item *item=args[i];
    list.push_back(item);
    if (item->const_item() && item->is_null())
      always_null= 1;
  }
  if (always_null)
    return false;
  count_field_types(select_lex, tmp_table_param, list, 0);
  tmp_table_param->force_copy_fields= force_copy_fields;
  assert(table == 0);

  if (!(table= create_tmp_table(session, tmp_table_param, list, (Order*) 0, 1,
				0,
				(select_lex->options | session->options),
				HA_POS_ERROR, (char*)"")))
  {
    return true;
  }
  table->cursor->extra(HA_EXTRA_NO_ROWS);		// Don't update rows
  table->no_rows=1;

  if (table->getShare()->db_type() == heap_engine)
  {
    /*
      No blobs, otherwise it would have been MyISAM: set up a compare
      function and its arguments to use with Unique.
    */
    qsort_cmp2 compare_key;
    void* cmp_arg;
    Field **field= table->getFields();
    Field **field_end= field + table->getShare()->sizeFields();
    bool all_binary= true;

    for (tree_key_length= 0; field < field_end; ++field)
    {
      Field *f= *field;
      enum enum_field_types f_type= f->type();
      tree_key_length+= f->pack_length();
      if (f_type == DRIZZLE_TYPE_VARCHAR)
      {
        all_binary= false;
        break;
      }
    }
    if (all_binary)
    {
      cmp_arg= (void*) &tree_key_length;
      compare_key= (qsort_cmp2) simple_raw_key_cmp;
    }
    else
    {
      if (table->getShare()->sizeFields() == 1)
      {
        /*
          If we have only one field, which is the most common use of
          count(distinct), it is much faster to use a simpler key
          compare method that can take advantage of not having to worry
          about other fields.
        */
        compare_key= (qsort_cmp2) simple_str_key_cmp;
        cmp_arg= (void*) table->getField(0);
        /* tree_key_length has been set already */
      }
      else
      {
        uint32_t *length;
        compare_key= (qsort_cmp2) composite_key_cmp;
        cmp_arg= this;
        field_lengths= new (session->mem) uint32_t[table->getShare()->sizeFields()];
        for (tree_key_length= 0, length= field_lengths, field= table->getFields();
             field < field_end; ++field, ++length)
        {
          *length= (*field)->pack_length();
          tree_key_length+= *length;
        }
      }
    }
    assert(tree == 0);
    tree= new Unique(compare_key, cmp_arg, tree_key_length,
                     (size_t)session->variables.max_heap_table_size);
    /*
      The only time tree_key_length could be 0 is if someone does
      count(distinct) on a char(0) field - stupid thing to do,
      but this has to be handled - otherwise someone can crash
      the server with a DoS attack
    */
    is_evaluated= false;
    if (! tree)
      return true;
  }
  return false;
}


Item *Item_sum_count_distinct::copy_or_same(Session* session)
{
  return new (session->mem) Item_sum_count_distinct(session, this);
}


void Item_sum_count_distinct::clear()
{
  /* tree and table can be both null only if always_null */
  is_evaluated= false;
  if (tree)
  {
    tree->reset();
  }
  else if (table)
  {
    table->cursor->extra(HA_EXTRA_NO_CACHE);
    table->cursor->ha_delete_all_rows();
    table->cursor->extra(HA_EXTRA_WRITE_CACHE);
  }
}

bool Item_sum_count_distinct::add()
{
  int error;
  if (always_null)
    return 0;
  copy_fields(tmp_table_param);
  if (copy_funcs(tmp_table_param->items_to_copy, table->in_use))
    return true;

  for (Field **field= table->getFields() ; *field ; field++)
  {
    if ((*field)->is_real_null(0))
    {
      return 0;					// Don't count NULL
    }
  }

  is_evaluated= false;
  if (tree)
  {
    /*
      The first few bytes of record (at least one) are just markers
      for deleted and NULLs. We want to skip them since they will
      bloat the tree without providing any valuable info. Besides,
      key_length used to initialize the tree didn't include space for them.
    */
    return tree->unique_add(table->record[0] + table->getShare()->null_bytes);
  }
  if ((error= table->cursor->insertRecord(table->record[0])) &&
      table->cursor->is_fatal_error(error, HA_CHECK_DUP))
    return true;
  return false;
}


int64_t Item_sum_count_distinct::val_int()
{
  int error;
  assert(fixed == 1);
  if (!table)					// Empty query
    return 0L;
  if (tree)
  {
    if (is_evaluated)
      return count;

    if (tree->elements == 0)
      return (int64_t) tree->elements_in_tree(); // everything fits in memory
    count= 0;
    tree->walk(count_distinct_walk, (void*) &count);
    is_evaluated= true;
    return (int64_t) count;
  }

  error= table->cursor->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  if(error)
  {
    table->print_error(error, MYF(0));
  }

  return table->cursor->stats.records;
}

/*****************************************************************************
 GROUP_CONCAT function

 SQL SYNTAX:
  GROUP_CONCAT([DISTINCT] expr,... [order_st BY col [ASC|DESC],...]
    [SEPARATOR str_const])

 concat of values from "group by" operation

 BUGS
   Blobs doesn't work with DISTINCT or order_st BY
*****************************************************************************/


/**
  Compares the values for fields in expr list of GROUP_CONCAT.
  @note

     GROUP_CONCAT([DISTINCT] expr [,expr ...]
              [order_st BY {unsigned_integer | col_name | expr}
                  [ASC | DESC] [,col_name ...]]
              [SEPARATOR str_val])

  @return
  @retval -1 : key1 < key2
  @retval  0 : key1 = key2
  @retval  1 : key1 > key2
*/

int group_concat_key_cmp_with_distinct(void* arg, const void* key1,
                                       const void* key2)
{
  Item_func_group_concat *item_func= (Item_func_group_concat*)arg;
  Table *table= item_func->table;

  for (uint32_t i= 0; i < item_func->arg_count_field; i++)
  {
    Item *item= item_func->args[i];
    /*
      If field_item is a const item then either get_tp_table_field returns 0
      or it is an item over a const table.
    */
    if (item->const_item())
      continue;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
    */
    Field *field= item->get_tmp_table_field();
    int res;
    uint32_t offset= field->offset(field->getTable()->record[0])-table->getShare()->null_bytes;
    if((res= field->cmp((unsigned char*)key1 + offset, (unsigned char*)key2 + offset)))
      return res;
  }
  return 0;
}


/**
  function of sort for syntax: GROUP_CONCAT(expr,... ORDER BY col,... )
*/

int group_concat_key_cmp_with_order(void* arg, const void* key1,
                                    const void* key2)
{
  Item_func_group_concat* grp_item= (Item_func_group_concat*) arg;
  Order **order_item, **end;
  Table *table= grp_item->table;

  for (order_item= grp_item->order, end=order_item+ grp_item->arg_count_order;
       order_item < end;
       order_item++)
  {
    Item *item= *(*order_item)->item;
    /*
      We have to use get_tmp_table_field() instead of
      real_item()->get_tmp_table_field() because we want the field in
      the temporary table, not the original field
    */
    Field *field= item->get_tmp_table_field();
    /*
      If item is a const item then either get_tp_table_field returns 0
      or it is an item over a const table.
    */
    if (field && !item->const_item())
    {
      int res;
      uint32_t offset= (field->offset(field->getTable()->record[0]) -
                    table->getShare()->null_bytes);
      if ((res= field->cmp((unsigned char*)key1 + offset, (unsigned char*)key2 + offset)))
        return (*order_item)->asc ? res : -res;
    }
  }
  /*
    We can't return 0 because in that case the tree class would remove this
    item as double value. This would cause problems for case-changes and
    if the returned values are not the same we do the sort on.
  */
  return 1;
}


/**
  Append data from current leaf to item->result.
*/

int dump_leaf_key(unsigned char* key, uint32_t ,
                  Item_func_group_concat *item)
{
  Table *table= item->table;
  String tmp((char *)table->getUpdateRecord(), table->getShare()->getRecordLength(),
             default_charset_info);
  String tmp2;
  String *result= &item->result;
  Item **arg= item->args, **arg_end= item->args + item->arg_count_field;
  uint32_t old_length= result->length();

  if (item->no_appended)
    item->no_appended= false;
  else
    result->append(*item->separator);

  tmp.length(0);

  for (; arg < arg_end; arg++)
  {
    String *res;
    if (! (*arg)->const_item())
    {
      /*
	We have to use get_tmp_table_field() instead of
	real_item()->get_tmp_table_field() because we want the field in
	the temporary table, not the original field
        We also can't use table->field array to access the fields
        because it contains both order and arg list fields.
      */
      Field *field= (*arg)->get_tmp_table_field();
      uint32_t offset= (field->offset(field->getTable()->record[0]) -
                    table->getShare()->null_bytes);
      assert(offset < table->getShare()->getRecordLength());
      res= field->val_str_internal(&tmp, key + offset);
    }
    else
      res= (*arg)->val_str(&tmp);
    if (res)
      result->append(*res);
  }

  /* stop if length of result more than max_length */
  if (result->length() > item->max_length)
  {
    int well_formed_error;
    const charset_info_st * const cs= item->collation.collation;
    const char *ptr= result->ptr();
    uint32_t add_length;
    /*
      It's ok to use item->result.length() as the fourth argument
      as this is never used to limit the length of the data.
      Cut is done with the third argument.
    */
    add_length= cs->cset->well_formed_len(cs,
                                          ptr + old_length,
                                          ptr + item->max_length,
                                          result->length(),
                                          &well_formed_error);
    result->length(old_length + add_length);
    item->count_cut_values++;
    item->warning_for_row= true;
    return 1;
  }
  return 0;
}


/**
  Constructor of Item_func_group_concat.

  @param distinct_arg   distinct
  @param select_list    list of expression for show values
  @param order_list     list of sort columns
  @param separator_arg  string value of separator.
*/

Item_func_group_concat::
Item_func_group_concat(Name_resolution_context *context_arg,
                       bool distinct_arg, List<Item> *select_list,
                       SQL_LIST *order_list, String *separator_arg)
  :tmp_table_param(0), warning(0),
   separator(separator_arg), tree(NULL), unique_filter(NULL), table(0),
   order(0), context(context_arg),
   arg_count_order(order_list ? order_list->elements : 0),
   arg_count_field(select_list->size()),
   count_cut_values(0),
   distinct(distinct_arg),
   warning_for_row(false),
   force_copy_fields(0), original(0)
{
  Item *item_select;
  Item **arg_ptr;

  quick_group= false;
  arg_count= arg_count_field + arg_count_order;

  /*
    We need to allocate:
    args - arg_count_field+arg_count_order
           (for possible order items in temporare tables)
    order - arg_count_order
  */
  if (!(args= (Item**) memory::sql_alloc(sizeof(Item*) * arg_count +
                                 sizeof(Order*)*arg_count_order)))
    return;

  order= (Order**)(args + arg_count);

  /* fill args items of show and sort */
  List<Item>::iterator li(select_list->begin());

  for (arg_ptr=args ; (item_select= li++) ; arg_ptr++)
    *arg_ptr= item_select;

  if (arg_count_order)
  {
    Order **order_ptr= order;
    for (Order *order_item= (Order*) order_list->first;
         order_item != NULL;
         order_item= order_item->next)
    {
      (*order_ptr++)= order_item;
      *arg_ptr= *order_item->item;
      order_item->item= arg_ptr++;
    }
  }
}


Item_func_group_concat::Item_func_group_concat(Session *session,
                                               Item_func_group_concat *item)
  :Item_sum(session, item),
  tmp_table_param(item->tmp_table_param),
  warning(item->warning),
  separator(item->separator),
  tree(item->tree),
  unique_filter(item->unique_filter),
  table(item->table),
  order(item->order),
  context(item->context),
  arg_count_order(item->arg_count_order),
  arg_count_field(item->arg_count_field),
  count_cut_values(item->count_cut_values),
  distinct(item->distinct),
  warning_for_row(item->warning_for_row),
  always_null(item->always_null),
  force_copy_fields(item->force_copy_fields),
  original(item)
{
  quick_group= item->quick_group;
  result.set_charset(collation.collation);
}



void Item_func_group_concat::cleanup()
{
  Item_sum::cleanup();

  /* Adjust warning message to include total number of cut values */
  if (warning)
  {
    char warn_buff[DRIZZLE_ERRMSG_SIZE];
    snprintf(warn_buff, sizeof(warn_buff), ER(ER_CUT_VALUE_GROUP_CONCAT), count_cut_values);
    warning->set_msg(&getSession(), warn_buff);
    warning= 0;
  }

  /*
    Free table and tree if they belong to this item (if item have not pointer
    to original item from which was made copy => it own its objects )
  */
  if (!original)
  {
    delete tmp_table_param;
    tmp_table_param= 0;
    if (table)
    {
      Session *session= table->in_use;
      table= 0;
      if (tree)
      {
        delete_tree(tree);
        tree= 0;
      }

      delete unique_filter;
      unique_filter= NULL;

      if (warning)
      {
        char warn_buff[DRIZZLE_ERRMSG_SIZE];
        snprintf(warn_buff, sizeof(warn_buff), ER(ER_CUT_VALUE_GROUP_CONCAT), count_cut_values);
        warning->set_msg(session, warn_buff);
        warning= 0;
      }
    }
    assert(tree == 0 && warning == 0);
  }
  return;
}


Item *Item_func_group_concat::copy_or_same(Session* session)
{
  return new (session->mem) Item_func_group_concat(session, this);
}


void Item_func_group_concat::clear()
{
  result.length(0);
  result.copy();
  null_value= true;
  warning_for_row= false;
  no_appended= true;
  if (tree)
    reset_tree(tree);
  if (distinct)
    unique_filter->reset();
  /* No need to reset the table as we never call write_row */
}


bool Item_func_group_concat::add()
{
  if (always_null)
    return 0;
  copy_fields(tmp_table_param);
  if (copy_funcs(tmp_table_param->items_to_copy, table->in_use))
    return true;

  for (uint32_t i= 0; i < arg_count_field; i++)
  {
    Item *show_item= args[i];
    if (!show_item->const_item())
    {
      Field *f= show_item->get_tmp_table_field();
      if (f->is_null_in_record((const unsigned char*) table->record[0]))
        return 0;                               // Skip row if it contains null
    }
  }

  null_value= false;
  bool row_eligible= true;

  if (distinct)
  {
    /* Filter out duplicate rows. */
    uint32_t count= unique_filter->elements_in_tree();
    unique_filter->unique_add(table->record[0] + table->getShare()->null_bytes);
    if (count == unique_filter->elements_in_tree())
      row_eligible= false;
  }

  TREE_ELEMENT *el= 0;                          // Only for safety
  if (row_eligible && tree)
    el= tree_insert(tree, table->record[0] + table->getShare()->null_bytes, 0,
                    tree->custom_arg);
  /*
    If the row is not a duplicate (el->count == 1)
    we can dump the row here in case of GROUP_CONCAT(DISTINCT...)
    instead of doing tree traverse later.
  */
  if (row_eligible && !warning_for_row &&
      (!tree || (el->count == 1 && distinct && !arg_count_order)))
    dump_leaf_key(table->record[0] + table->getShare()->null_bytes, 1, this);

  return 0;
}


bool
Item_func_group_concat::fix_fields(Session *session, Item **ref)
{
  uint32_t i;                       /* for loop variable */
  assert(fixed == 0);

  if (init_sum_func_check(session))
    return true;

  maybe_null= 1;

  /*
    Fix fields for select list and order_st clause
  */

  for (i=0 ; i < arg_count ; i++)
  {
    if ((!args[i]->fixed &&
         args[i]->fix_fields(session, args + i)) ||
        args[i]->check_cols(1))
      return true;
  }

  if (agg_item_charsets(collation, func_name(),
                        args,
			/* skip charset aggregation for order columns */
			arg_count - arg_count_order,
			MY_COLL_ALLOW_CONV, 1))
    return 1;

  result.set_charset(collation.collation);
  result_field= 0;
  null_value= 1;
  max_length= (size_t)session->variables.group_concat_max_len;

  if (check_sum_func(session, ref))
    return true;

  fixed= 1;
  return false;
}


bool Item_func_group_concat::setup(Session *session)
{
  List<Item> list;
  Select_Lex *select_lex= session->lex().current_select;

  /*
    Currently setup() can be called twice. Please add
    assertion here when this is fixed.
  */
  if (table || tree)
    return false;

  tmp_table_param= new Tmp_Table_Param;

  /* We'll convert all blobs to varchar fields in the temporary table */
  tmp_table_param->convert_blob_length= max_length *
                                        collation.collation->mbmaxlen;
  /* Push all not constant fields to the list and create a temp table */
  always_null= 0;
  for (uint32_t i= 0; i < arg_count_field; i++)
  {
    Item *item= args[i];
    list.push_back(item);
    if (item->const_item())
    {
      if (item->is_null())
      {
        always_null= 1;
        return false;
      }
    }
  }

  List<Item> all_fields(list);
  /*
    Try to find every order_st expression in the list of GROUP_CONCAT
    arguments. If an expression is not found, prepend it to
    "all_fields". The resulting field list is used as input to create
    tmp table columns.
  */
  if (arg_count_order &&
      setup_order(session, args, context->table_list, list, all_fields, *order))
    return true;

  count_field_types(select_lex, tmp_table_param, all_fields, 0);
  tmp_table_param->force_copy_fields= force_copy_fields;
  assert(table == 0);
  if (arg_count_order > 0 || distinct)
  {
    /*
      Currently we have to force conversion of BLOB values to VARCHAR's
      if we are to store them in TREE objects used for ORDER BY and
      DISTINCT. This leads to truncation if the BLOB's size exceeds
      Field_varstring::MAX_SIZE.
    */
    set_if_smaller(tmp_table_param->convert_blob_length,
                   Field_varstring::MAX_SIZE);
  }

  /*
    We have to create a temporary table to get descriptions of fields
    (types, sizes and so on).

    Note that in the table, we first have the ORDER BY fields, then the
    field list.
  */
  if (!(table= create_tmp_table(session, tmp_table_param, all_fields,
                                (Order*) 0, 0, true,
                                (select_lex->options | session->options),
                                HA_POS_ERROR, (char*) "")))
  {
    return true;
  }

  table->cursor->extra(HA_EXTRA_NO_ROWS);
  table->no_rows= 1;

  /*
     Need sorting or uniqueness: init tree and choose a function to sort.
     Don't reserve space for NULLs: if any of gconcat arguments is NULL,
     the row is not added to the result.
  */
  uint32_t tree_key_length= table->getShare()->getRecordLength() - table->getShare()->null_bytes;

  if (arg_count_order)
  {
    tree= &tree_base;
    /*
      Create a tree for sorting. The tree is used to sort (according to the
      syntax of this function). If there is no ORDER BY clause, we don't
      create this tree.
    */
    init_tree(tree, (uint32_t) min(session->variables.max_heap_table_size,
                                   (uint64_t)(session->variables.sortbuff_size/16)), 
              0,
              tree_key_length,
              group_concat_key_cmp_with_order , false, NULL, (void*) this);
  }

  if (distinct)
    unique_filter= new Unique(group_concat_key_cmp_with_distinct,
                              (void*)this,
                              tree_key_length,
                              (size_t)session->variables.max_heap_table_size);

  return false;
}


/* This is used by rollup to create a separate usable copy of the function */

void Item_func_group_concat::make_unique()
{
  tmp_table_param= 0;
  table=0;
  original= 0;
  force_copy_fields= 1;
  tree= 0;
}

double Item_func_group_concat::val_real()
{
  String *res;  res=val_str(&str_value);
  return res ? internal::my_atof(res->c_ptr()) : 0.0;
}

int64_t Item_func_group_concat::val_int()
{
  String *res;
  char *end_ptr;
  int error;
  if (!(res= val_str(&str_value)))
    return (int64_t) 0;
  end_ptr= (char*) res->ptr()+ res->length();
  return internal::my_strtoll10(res->ptr(), &end_ptr, &error);
}

String* Item_func_group_concat::val_str(String* )
{
  assert(fixed == 1);
  if (null_value)
    return 0;
  if (no_appended && tree)
    /* Tree is used for sorting as in ORDER BY */
    tree_walk(tree, (tree_walk_action)&dump_leaf_key, (void*)this,
              left_root_right);
  if (count_cut_values && !warning)
  {
    /*
      ER_CUT_VALUE_GROUP_CONCAT needs an argument, but this gets set in
      Item_func_group_concat::cleanup().
    */
    assert(table);
    warning= push_warning(table->in_use, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_CUT_VALUE_GROUP_CONCAT,
                          ER(ER_CUT_VALUE_GROUP_CONCAT));
  }
  return &result;
}


void Item_func_group_concat::print(String *str)
{
  str->append(STRING_WITH_LEN("group_concat("));
  if (distinct)
    str->append(STRING_WITH_LEN("distinct "));
  for (uint32_t i= 0; i < arg_count_field; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
  }
  if (arg_count_order)
  {
    str->append(STRING_WITH_LEN(" order by "));
    for (uint32_t i= 0 ; i < arg_count_order ; i++)
    {
      if (i)
        str->append(',');
      (*order[i]->item)->print(str);
      if (order[i]->asc)
        str->append(STRING_WITH_LEN(" ASC"));
      else
        str->append(STRING_WITH_LEN(" DESC"));
    }
  }
  str->append(STRING_WITH_LEN(" separator \'"));
  str->append(*separator);
  str->append(STRING_WITH_LEN("\')"));
}


Item_func_group_concat::~Item_func_group_concat()
{
  if (!original && unique_filter)
    delete unique_filter;
}

} /* namespace drizzled */
