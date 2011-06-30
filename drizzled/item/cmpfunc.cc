/* Copyright (C) 2000-2006 MySQL AB

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
  This file defines all compare functions
*/

#include <config.h>

#include <drizzled/cached_item.h>
#include <drizzled/check_stack_overrun.h>
#include <drizzled/current_session.h>
#include <drizzled/error.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/item/cache_int.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/int_with_ref.h>
#include <drizzled/item/subselect.h>
#include <drizzled/session.h>
#include <drizzled/sql_select.h>
#include <drizzled/temporal.h>
#include <drizzled/time_functions.h>
#include <drizzled/sql_lex.h>
#include <drizzled/system_variables.h>

#include <math.h>
#include <algorithm>

using namespace std;

namespace drizzled {

extern const double log_10[309];

static Eq_creator eq_creator;
static Ne_creator ne_creator;
static Gt_creator gt_creator;
static Lt_creator lt_creator;
static Ge_creator ge_creator;
static Le_creator le_creator;

static bool convert_constant_item(Session *, Item_field *, Item **);

static Item_result item_store_type(Item_result a, Item *item,
                                   bool unsigned_flag)
{
  Item_result b= item->result_type();

  if (a == STRING_RESULT || b == STRING_RESULT)
    return STRING_RESULT;
  else if (a == REAL_RESULT || b == REAL_RESULT)
    return REAL_RESULT;
  else if (a == DECIMAL_RESULT || b == DECIMAL_RESULT ||
           unsigned_flag != item->unsigned_flag)
    return DECIMAL_RESULT;
  else
    return INT_RESULT;
}

static void agg_result_type(Item_result *type, Item **items, uint32_t nitems)
{
  Item **item, **item_end;
  bool unsigned_flag= 0;

  *type= STRING_RESULT;
  /* Skip beginning NULL items */
  for (item= items, item_end= item + nitems; item < item_end; item++)
  {
    if ((*item)->type() != Item::NULL_ITEM)
    {
      *type= (*item)->result_type();
      unsigned_flag= (*item)->unsigned_flag;
      item++;
      break;
    }
  }
  /* Combine result types. Note: NULL items don't affect the result */
  for (; item < item_end; item++)
  {
    if ((*item)->type() != Item::NULL_ITEM)
      *type= item_store_type(*type, *item, unsigned_flag);
  }
}


/*
  Compare row signature of two expressions

  SYNOPSIS:
    cmp_row_type()
    item1          the first expression
    item2         the second expression

  DESCRIPTION
    The function checks that two expressions have compatible row signatures
    i.e. that the number of columns they return are the same and that if they
    are both row expressions then each component from the first expression has
    a row signature compatible with the signature of the corresponding component
    of the second expression.

  RETURN VALUES
    1  type incompatibility has been detected
    0  otherwise
*/

static int cmp_row_type(Item* item1, Item* item2)
{
  uint32_t n= item1->cols();
  if (item2->check_cols(n))
    return 1;
  for (uint32_t i=0; i<n; i++)
  {
    if (item2->element_index(i)->check_cols(item1->element_index(i)->cols()) ||
        (item1->element_index(i)->result_type() == ROW_RESULT &&
         cmp_row_type(item1->element_index(i), item2->element_index(i))))
      return 1;
  }
  return 0;
}


/**
  Aggregates result types from the array of items.

  SYNOPSIS:
    agg_cmp_type()
    type   [out] the aggregated type
    items        array of items to aggregate the type from
    nitems       number of items in the array

  DESCRIPTION
    This function aggregates result types from the array of items. Found type
    supposed to be used later for comparison of values of these items.
    Aggregation itself is performed by the item_cmp_type() function.
  @param[out] type    the aggregated type
  @param      items        array of items to aggregate the type from
  @param      nitems       number of items in the array

  @retval
    1  type incompatibility has been detected
  @retval
    0  otherwise
*/

static int agg_cmp_type(Item_result *type, Item **items, uint32_t nitems)
{
  uint32_t i;
  type[0]= items[0]->result_type();
  for (i= 1 ; i < nitems ; i++)
  {
    type[0]= item_cmp_type(type[0], items[i]->result_type());
    /*
      When aggregating types of two row expressions we have to check
      that they have the same cardinality and that each component
      of the first row expression has a compatible row signature with
      the signature of the corresponding component of the second row
      expression.
    */
    if (type[0] == ROW_RESULT && cmp_row_type(items[0], items[i]))
      return 1;     // error found: invalid usage of rows
  }
  return 0;
}


/**
  @brief Aggregates field types from the array of items.

  @param[in] items  array of items to aggregate the type from
  @paran[in] nitems number of items in the array

  @details This function aggregates field types from the array of items.
    Found type is supposed to be used later as the result field type
    of a multi-argument function.
    Aggregation itself is performed by the Field::field_type_merge()
    function.

  @note The term "aggregation" is used here in the sense of inferring the
    result type of a function from its argument types.

  @return aggregated field type.
*/

enum_field_types agg_field_type(Item **items, uint32_t nitems)
{
  uint32_t i;
  if (!nitems || items[0]->result_type() == ROW_RESULT )
    return (enum_field_types)-1;
  enum_field_types res= items[0]->field_type();
  for (i= 1 ; i < nitems ; i++)
    res= Field::field_type_merge(res, items[i]->field_type());
  return res;
}

/*
  Collects different types for comparison of first item with each other items

  SYNOPSIS
    collect_cmp_types()
      items             Array of items to collect types from
      nitems            Number of items in the array
      skip_nulls        Don't collect types of NULL items if TRUE

  DESCRIPTION
    This function collects different result types for comparison of the first
    item in the list with each of the remaining items in the 'items' array.

  RETURN
    0 - if row type incompatibility has been detected (see cmp_row_type)
    Bitmap of collected types - otherwise
*/

static uint32_t collect_cmp_types(Item **items, uint32_t nitems, bool skip_nulls= false)
{
  uint32_t i;
  uint32_t found_types;
  Item_result left_result= items[0]->result_type();
  assert(nitems > 1);
  found_types= 0;
  for (i= 1; i < nitems ; i++)
  {
    if (skip_nulls && items[i]->type() == Item::NULL_ITEM)
      continue; // Skip NULL constant items
    if ((left_result == ROW_RESULT ||
         items[i]->result_type() == ROW_RESULT) &&
        cmp_row_type(items[0], items[i]))
      return 0;
    found_types|= 1<< (uint32_t)item_cmp_type(left_result,
                                           items[i]->result_type());
  }
  /*
   Even if all right-hand items are NULLs and we are skipping them all, we need
   at least one type bit in the found_type bitmask.
  */
  if (skip_nulls && !found_types)
    found_types= 1 << (uint)left_result;
  return found_types;
}


Item_bool_func2* Eq_creator::create(Item *a, Item *b) const
{
  return new Item_func_eq(a, b);
}


const Eq_creator* Eq_creator::instance()
{
  return &eq_creator;
}


Item_bool_func2* Ne_creator::create(Item *a, Item *b) const
{
  return new Item_func_ne(a, b);
}


const Ne_creator* Ne_creator::instance()
{
  return &ne_creator;
}


Item_bool_func2* Gt_creator::create(Item *a, Item *b) const
{
  return new Item_func_gt(a, b);
}


const Gt_creator* Gt_creator::instance()
{
  return &gt_creator;
}


Item_bool_func2* Lt_creator::create(Item *a, Item *b) const
{
  return new Item_func_lt(a, b);
}


const Lt_creator* Lt_creator::instance()
{
  return &lt_creator;
}


Item_bool_func2* Ge_creator::create(Item *a, Item *b) const
{
  return new Item_func_ge(a, b);
}


const Ge_creator* Ge_creator::instance()
{
  return &ge_creator;
}


Item_bool_func2* Le_creator::create(Item *a, Item *b) const
{
  return new Item_func_le(a, b);
}

const Le_creator* Le_creator::instance()
{
  return &le_creator;
}


/*
  Test functions
  Most of these  returns 0LL if false and 1LL if true and
  NULL if some arg is NULL.
*/

int64_t Item_func_not::val_int()
{
  assert(fixed == 1);
  bool value= args[0]->val_bool();
  null_value=args[0]->null_value;
  return ((!null_value && value == 0) ? 1 : 0);
}

/*
  We put any NOT expression into parenthesis to avoid
  possible problems with internal view representations where
  any '!' is converted to NOT. It may cause a problem if
  '!' is used in an expression together with other operators
  whose precedence is lower than the precedence of '!' yet
  higher than the precedence of NOT.
*/

void Item_func_not::print(String *str)
{
  str->append('(');
  Item_func::print(str);
  str->append(')');
}

/**
  special NOT for ALL subquery.
*/


int64_t Item_func_not_all::val_int()
{
  assert(fixed == 1);
  bool value= args[0]->val_bool();

  /*
    return true if there was records in underlying select in max/min
    optimization (ALL subquery)
  */
  if (empty_underlying_subquery())
    return 1;

  null_value= args[0]->null_value;
  return ((!null_value && value == 0) ? 1 : 0);
}


bool Item_func_not_all::empty_underlying_subquery()
{
  return ((test_sum_item && !test_sum_item->any_value()) ||
          (test_sub_item && !test_sub_item->any_value()));
}

void Item_func_not_all::print(String *str)
{
  if (show)
    Item_func::print(str);
  else
    args[0]->print(str);
}


/**
  Special NOP (No OPeration) for ALL subquery. It is like
  Item_func_not_all.

  @return
    (return true if underlying subquery do not return rows) but if subquery
    returns some rows it return same value as argument (true/false).
*/

int64_t Item_func_nop_all::val_int()
{
  assert(fixed == 1);
  int64_t value= args[0]->val_int();

  /*
    return false if there was records in underlying select in max/min
    optimization (SAME/ANY subquery)
  */
  if (empty_underlying_subquery())
    return 0;

  null_value= args[0]->null_value;
  return (null_value || value == 0) ? 0 : 1;
}


/**
  Convert a constant item to an int and replace the original item.

    The function converts a constant expression or string to an integer.
    On successful conversion the original item is substituted for the
    result of the item evaluation.
    This is done when comparing DATE/TIME of different formats and
    also when comparing bigint to strings (in which case strings
    are converted to bigints).

  @param  session             thread handle
  @param  field_item      item will be converted using the type of this field
  @param[in,out] item     reference to the item to convert

  @note
    This function is called only at prepare stage.
    As all derived tables are filled only after all derived tables
    are prepared we do not evaluate items with subselects here because
    they can contain derived tables and thus we may attempt to use a
    table that has not been populated yet.

  @retval
    0  Can't convert item
  @retval
    1  Item was replaced with an integer version of the item
*/

static bool convert_constant_item(Session *session, Item_field *field_item,
                                  Item **item)
{
  Field *field= field_item->field;
  int result= 0;

  field->setWriteSet();

  if (!(*item)->with_subselect && (*item)->const_item())
  {
    ulong orig_sql_mode= session->variables.sql_mode;
    enum_check_fields orig_count_cuted_fields= session->count_cuted_fields;
    uint64_t orig_field_val= 0; /* original field value if valid */

    /* For comparison purposes allow invalid dates like 2000-01-32 */
    session->variables.sql_mode= (orig_sql_mode & ~MODE_NO_ZERO_DATE) |
                             MODE_INVALID_DATES;
    session->count_cuted_fields= CHECK_FIELD_IGNORE;

    /*
      Store the value of the field if it references an outer field because
      the call to save_in_field below overrides that value.
    */
    if (field_item->depended_from)
    {
      orig_field_val= field->val_int();
    }

    if (!(*item)->is_null() && !(*item)->save_in_field(field, 1))
    {
      Item *tmp= new Item_int_with_ref(field->val_int(), *item,
                                       test(field->flags & UNSIGNED_FLAG));
      if (tmp)
        *item= tmp;
      result= 1;					// Item was replaced
    }

    /* Restore the original field value. */
    if (field_item->depended_from)
    {
      result= field->store(orig_field_val, field->isUnsigned());
      /* orig_field_val must be a valid value that can be restored back. */
      assert(!result);
    }
    session->variables.sql_mode= orig_sql_mode;
    session->count_cuted_fields= orig_count_cuted_fields;
  }
  return result;
}


void Item_bool_func2::fix_length_and_dec()
{
  max_length= 1;				     // Function returns 0 or 1

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditions here
  */
  if (!args[0] || !args[1])
    return;

  /*
    We allow to convert to Unicode character sets in some cases.
    The conditions when conversion is possible are:
    - arguments A and B have different charsets
    - A wins according to coercibility rules
    - character set of A is superset for character set of B

    If all of the above is true, then it's possible to convert
    B into the character set of A, and then compare according
    to the collation of A.
  */


  DTCollation coll;
  if (args[0]->result_type() == STRING_RESULT &&
      args[1]->result_type() == STRING_RESULT &&
      agg_arg_charsets(coll, args, 2, MY_COLL_CMP_CONV, 1))
    return;

  args[0]->cmp_context= args[1]->cmp_context=
    item_cmp_type(args[0]->result_type(), args[1]->result_type());
  // Make a special case of compare with fields to get nicer DATE comparisons

  if (functype() == LIKE_FUNC)  // Disable conversion in case of LIKE function.
  {
    set_cmp_func();
    return;
  }

  Item_field *field_item= NULL;

  if (args[0]->real_item()->type() == FIELD_ITEM)
  {
    field_item= static_cast<Item_field*>(args[0]->real_item());
    if (field_item->field->can_be_compared_as_int64_t() &&
        !(field_item->is_datetime() && args[1]->result_type() == STRING_RESULT))
    {
      if (convert_constant_item(&getSession(), field_item, &args[1]))
      {
        cmp.set_cmp_func(this, tmp_arg, tmp_arg+1,
                         INT_RESULT);		// Works for all types.
        args[0]->cmp_context= args[1]->cmp_context= INT_RESULT;
        return;
      }
    }

    if (args[1]->real_item()->type() == FIELD_ITEM)
    {
      field_item= static_cast<Item_field*>(args[1]->real_item());
      if (field_item->field->can_be_compared_as_int64_t() &&
          !(field_item->is_datetime() &&
            args[0]->result_type() == STRING_RESULT))
      {
        if (convert_constant_item(&getSession(), field_item, &args[0]))
        {
          cmp.set_cmp_func(this, tmp_arg, tmp_arg+1,
                           INT_RESULT); // Works for all types.
          args[0]->cmp_context= args[1]->cmp_context= INT_RESULT;
          return;
        }
      }
    }
  }
  set_cmp_func();
}

Arg_comparator::Arg_comparator():
  session(current_session),
  a_cache(0),
  b_cache(0)
{}

Arg_comparator::Arg_comparator(Item **a1, Item **a2):
  a(a1),
  b(a2),
  session(current_session),
  a_cache(0),
  b_cache(0)
{}

int Arg_comparator::set_compare_func(Item_bool_func2 *item, Item_result type)
{
  owner= item;
  func= comparator_matrix[type]
    [test(owner->functype() == Item_func::EQUAL_FUNC)];

  switch (type) {
  case ROW_RESULT:
    {
      uint32_t n= (*a)->cols();
      if (n != (*b)->cols())
      {
        my_error(ER_OPERAND_COLUMNS, MYF(0), n);
        comparators= 0;
        return 1;
      }
      comparators= new Arg_comparator[n];
      for (uint32_t i=0; i < n; i++)
      {
        if ((*a)->element_index(i)->cols() != (*b)->element_index(i)->cols())
        {
          my_error(ER_OPERAND_COLUMNS, MYF(0), (*a)->element_index(i)->cols());
          return 1;
        }
        comparators[i].set_cmp_func(owner, (*a)->addr(i), (*b)->addr(i));
      }
      break;
    }

  case STRING_RESULT:
    {
      /*
        We must set cmp_charset here as we may be called from for an automatic
        generated item, like in natural join
      */
      if (cmp_collation.set((*a)->collation, (*b)->collation) ||
          cmp_collation.derivation == DERIVATION_NONE)
      {
        my_coll_agg_error((*a)->collation, (*b)->collation, owner->func_name());
        return 1;
      }
      if (cmp_collation.collation == &my_charset_bin)
      {
        /*
          We are using BLOB/BINARY/VARBINARY, change to compare byte by byte,
          without removing end space
        */
        if (func == &Arg_comparator::compare_string)
          func= &Arg_comparator::compare_binary_string;
        else if (func == &Arg_comparator::compare_e_string)
          func= &Arg_comparator::compare_e_binary_string;

        /*
          As this is binary compassion, mark all fields that they can't be
          transformed. Otherwise we would get into trouble with comparisons
like:
WHERE col= 'j' AND col LIKE BINARY 'j'
which would be transformed to:
WHERE col= 'j'
      */
        (*a)->walk(&Item::set_no_const_sub, false, (unsigned char*) 0);
        (*b)->walk(&Item::set_no_const_sub, false, (unsigned char*) 0);
      }
      break;
    }
  case INT_RESULT:
    {
      if (func == &Arg_comparator::compare_int_signed)
      {
        if ((*a)->unsigned_flag)
          func= (((*b)->unsigned_flag)?
                 &Arg_comparator::compare_int_unsigned :
                 &Arg_comparator::compare_int_unsigned_signed);
        else if ((*b)->unsigned_flag)
          func= &Arg_comparator::compare_int_signed_unsigned;
      }
      else if (func== &Arg_comparator::compare_e_int)
      {
        if ((*a)->unsigned_flag ^ (*b)->unsigned_flag)
          func= &Arg_comparator::compare_e_int_diff_signedness;
      }
      break;
    }
  case DECIMAL_RESULT:
    break;
  case REAL_RESULT:
    {
      if ((*a)->decimals < NOT_FIXED_DEC && (*b)->decimals < NOT_FIXED_DEC)
      {
        precision= 5 / log_10[max((*a)->decimals, (*b)->decimals) + 1];
        if (func == &Arg_comparator::compare_real)
          func= &Arg_comparator::compare_real_fixed;
        else if (func == &Arg_comparator::compare_e_real)
          func= &Arg_comparator::compare_e_real_fixed;
      }
      break;
    }
  }

  return 0;
}


/**
  @brief Convert date provided in a string to the int representation.

  @param[in]   session        thread handle
  @param[in]   str        a string to convert
  @param[in]   warn_type  type of the timestamp for issuing the warning
  @param[in]   warn_name  field name for issuing the warning
  @param[out]  error_arg  could not extract a DATE or DATETIME

  @details Convert date provided in the string str to the int
    representation.  If the string contains wrong date or doesn't
    contain it at all then a warning is issued.  The warn_type and
    the warn_name arguments are used as the name and the type of the
    field when issuing the warning.  If any input was discarded
    (trailing or non-timestampy characters), was_cut will be non-zero.
    was_type will return the type str_to_datetime() could correctly
    extract.

  @return
    converted value. 0 on error and on zero-dates -- check 'failure'
*/

static int64_t
get_date_from_str(Session *session, String *str, type::timestamp_t warn_type,
                  char *warn_name, bool *error_arg)
{
  int64_t value= 0;
  type::cut_t error= type::VALID;
  type::Time l_time;
  type::timestamp_t ret;

  ret= l_time.store(str->ptr(), str->length(),
                    (TIME_FUZZY_DATE | MODE_INVALID_DATES | (session->variables.sql_mode & MODE_NO_ZERO_DATE)),
                    error);

  if (ret == type::DRIZZLE_TIMESTAMP_DATETIME || ret == type::DRIZZLE_TIMESTAMP_DATE)
  {
    /*
      Do not return yet, we may still want to throw a "trailing garbage"
      warning.
    */
    *error_arg= false;
    l_time.convert(value);
  }
  else
  {
    *error_arg= true;
    error= type::CUT;                                   /* force warning */
  }

  if (error != type::VALID)
  {
    make_truncated_value_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                 str->ptr(), str->length(),
                                 warn_type, warn_name);
  }

  return value;
}


/*
  Check whether compare_datetime() can be used to compare items.

  SYNOPSIS
    Arg_comparator::can_compare_as_dates()
    a, b          [in]  items to be compared
    const_value   [out] converted value of the string constant, if any

  DESCRIPTION
    Check several cases when the DATE/DATETIME comparator should be used.
    The following cases are checked:
      1. Both a and b is a DATE/DATETIME field/function returning string or
         int result.
      2. Only a or b is a DATE/DATETIME field/function returning string or
         int result and the other item (b or a) is an item with string result.
         If the second item is a constant one then it's checked to be
         convertible to the DATE/DATETIME type. If the constant can't be
         converted to a DATE/DATETIME then an error is issued back to the Session.
      In all other cases (date-[int|real|decimal]/[int|real|decimal]-date)
      the comparison is handled by other comparators.

    If the datetime comparator can be used and one the operands of the
    comparison is a string constant that was successfully converted to a
    DATE/DATETIME type then the result of the conversion is returned in the
    const_value if it is provided.  If there is no constant or
    compare_datetime() isn't applicable then the *const_value remains
    unchanged.

  RETURN
    the found type of date comparison
*/

enum Arg_comparator::enum_date_cmp_type
Arg_comparator::can_compare_as_dates(Item *in_a, Item *in_b,
                                     int64_t *const_value)
{
  enum enum_date_cmp_type cmp_type= CMP_DATE_DFLT;
  Item *str_arg= 0;

  if (in_a->type() == Item::ROW_ITEM || in_b->type() == Item::ROW_ITEM)
    return CMP_DATE_DFLT;

  if (in_a->is_datetime())
  {
    if (in_b->is_datetime())
    {
      cmp_type= CMP_DATE_WITH_DATE;
    }
    else if (in_b->result_type() == STRING_RESULT)
    {
      cmp_type= CMP_DATE_WITH_STR;
      str_arg= in_b;
    }
  }
  else if (in_b->is_datetime() && in_a->result_type() == STRING_RESULT)
  {
    cmp_type= CMP_STR_WITH_DATE;
    str_arg= in_a;
  }

  if (cmp_type != CMP_DATE_DFLT)
  {
    /*
      Do not cache GET_USER_VAR() function as its const_item() may return true
      for the current thread but it still may change during the execution.
    */
    if (cmp_type != CMP_DATE_WITH_DATE && str_arg->const_item() &&
        (str_arg->type() != Item::FUNC_ITEM ||
        ((Item_func*)str_arg)->functype() != Item_func::GUSERVAR_FUNC))
    {
      /*
       * OK, we are here if we've got a date field (or something which can be 
       * compared as a date field) on one side of the equation, and a constant
       * string on the other side.  In this case, we must verify that the constant
       * string expression can indeed be evaluated as a datetime.  If it cannot, 
       * we throw an error here and stop processsing.  Bad data should ALWAYS 
       * produce an error, and no implicit conversion or truncation should take place.
       *
       * If the conversion to a DateTime temporal is successful, then we convert
       * the Temporal instance to a uint64_t for the comparison operator, which
       * compares date(times) using int64_t semantics.
       *
       * @TODO
       *
       * Does a uint64_t conversion really have to happen here?  Fields return int64_t
       * from val_int(), not uint64_t...
       */
      int64_t value;
      String *str_val;
      String tmp;
      /* DateTime used to pick up as many string conversion possibilities as possible. */
      DateTime temporal;

      str_val= str_arg->val_str(&tmp);
      if (! str_val)
      {
        /* 
         * If we are here, it is most likely due to the comparison item
         * being a NULL.  Although this is incorrect (SQL demands that the term IS NULL
         * be used, not = NULL since no item can be equal to NULL).
         *
         * So, return gracefully.
         */
        return CMP_DATE_DFLT;
      }
      if (temporal.from_string(str_val->c_ptr(), str_val->length()))
      {
        /* String conversion was good.  Convert to an integer for comparison purposes. */
        temporal.to_int64_t(&value);
      }
      else
      {
        /* We aren't a DATETIME but still could be a TIME */
        Time timevalue;
        if (timevalue.from_string(str_val->c_ptr(), str_val->length()))
        {
          uint64_t timeint;
          timevalue.to_uint64_t(timeint);
          value= static_cast<int64_t>(timeint);
        }
        else
        {
          /* Chuck an error. Bad datetime input. */
          my_error(ER_INVALID_DATETIME_VALUE, MYF(ME_FATALERROR), str_val->c_ptr());
          return CMP_DATE_DFLT; /* :( What else can I return... */
        }
      }

      if (const_value)
        *const_value= value;
    }
  }
  return cmp_type;
}


int Arg_comparator::set_cmp_func(Item_bool_func2 *owner_arg,
                                        Item **a1, Item **a2,
                                        Item_result type)
{
  enum_date_cmp_type cmp_type;
  int64_t const_value= -1;
  a= a1;
  b= a2;

  if ((cmp_type= can_compare_as_dates(*a, *b, &const_value)))
  {
    owner= owner_arg;
    a_type= (*a)->field_type();
    b_type= (*b)->field_type();
    a_cache= 0;
    b_cache= 0;

    if (const_value != -1)
    {
      Item_cache_int *cache= new Item_cache_int();
      /* Mark the cache as non-const to prevent re-caching. */
      cache->set_used_tables(1);
      if (!(*a)->is_datetime())
      {
        cache->store((*a), const_value);
        a_cache= cache;
        a= (Item **)&a_cache;
      }
      else
      {
        cache->store((*b), const_value);
        b_cache= cache;
        b= (Item **)&b_cache;
      }
    }
    is_nulls_eq= test(owner && owner->functype() == Item_func::EQUAL_FUNC);
    func= &Arg_comparator::compare_datetime;
    get_value_func= &get_datetime_value;

    return 0;
  }

  return set_compare_func(owner_arg, type);
}


void Arg_comparator::set_datetime_cmp_func(Item **a1, Item **b1)
{
  /* A caller will handle null values by itself. */
  owner= NULL;
  a= a1;
  b= b1;
  a_type= (*a)->field_type();
  b_type= (*b)->field_type();
  a_cache= 0;
  b_cache= 0;
  is_nulls_eq= false;
  func= &Arg_comparator::compare_datetime;
  get_value_func= &get_datetime_value;
}


/*
  Retrieves correct DATETIME value from given item.

  SYNOPSIS
    get_datetime_value()
    session                 thread handle
    item_arg   [in/out] item to retrieve DATETIME value from
    cache_arg  [in/out] pointer to place to store the caching item to
    warn_item  [in]     item for issuing the conversion warning
    is_null    [out]    true <=> the item_arg is null

  DESCRIPTION
    Retrieves the correct DATETIME value from given item for comparison by the
    compare_datetime() function.
    If item's result can be compared as int64_t then its int value is used
    and its string value is used otherwise. Strings are always parsed and
    converted to int values by the get_date_from_str() function.
    This allows us to compare correctly string dates with missed insignificant
    zeros. If an item is a constant one then its value is cached and it isn't
    get parsed again. An Item_cache_int object is used for caching values. It
    seamlessly substitutes the original item.  The cache item is marked as
    non-constant to prevent re-caching it again.  In order to compare
    correctly DATE and DATETIME items the result of the former are treated as
    a DATETIME with zero time (00:00:00).

  RETURN
    obtained value
*/

int64_t
get_datetime_value(Session *session, Item ***item_arg, Item **cache_arg,
                   Item *warn_item, bool *is_null)
{
  int64_t value= 0;
  String buf, *str= 0;
  Item *item= **item_arg;

  if (item->result_as_int64_t())
  {
    value= item->val_int();
    *is_null= item->null_value;
    enum_field_types f_type= item->field_type();
    /*
      Item_date_add_interval may return DRIZZLE_TYPE_STRING as the result
      field type. To detect that the DATE value has been returned we
      compare it with 100000000L - any DATE value should be less than it.
      Don't shift cached DATETIME values up for the second time.
    */
    if (f_type == DRIZZLE_TYPE_DATE ||
        (f_type != DRIZZLE_TYPE_DATETIME && value < 100000000L))
      value*= 1000000L;
  }
  else
  {
    str= item->val_str(&buf);
    *is_null= item->null_value;
  }

  if (*is_null)
    return ~(uint64_t) 0;

  /*
    Convert strings to the integer DATE/DATETIME representation.
    Even if both dates provided in strings we can't compare them directly as
    strings as there is no warranty that they are correct and do not miss
    some insignificant zeros.
  */
  if (str)
  {
    bool error;
    enum_field_types f_type= warn_item->field_type();
    type::timestamp_t t_type= f_type == DRIZZLE_TYPE_DATE ? type::DRIZZLE_TIMESTAMP_DATE : type::DRIZZLE_TIMESTAMP_DATETIME;
    value= get_date_from_str(session, str, t_type, warn_item->name, &error);
    /*
      If str did not contain a valid date according to the current
      SQL_MODE, get_date_from_str() has already thrown a warning,
      and we don't want to throw NULL on invalid date (see 5.2.6
      "SQL modes" in the manual), so we're done here.
    */
  }
  /*
    Do not cache GET_USER_VAR() function as its const_item() may return true
    for the current thread but it still may change during the execution.
  */
  if (item->const_item() && cache_arg && (item->type() != Item::FUNC_ITEM ||
      ((Item_func*)item)->functype() != Item_func::GUSERVAR_FUNC))
  {
    Item_cache_int *cache= new Item_cache_int(DRIZZLE_TYPE_DATETIME);
    /* Mark the cache as non-const to prevent re-caching. */
    cache->set_used_tables(1);
    cache->store(item, value);
    *cache_arg= cache;
    *item_arg= cache_arg;
  }

  return value;
}

/*
  Compare items values as dates.

  SYNOPSIS
    Arg_comparator::compare_datetime()

  DESCRIPTION
    Compare items values as DATE/DATETIME for both EQUAL_FUNC and from other
    comparison functions. The correct DATETIME values are obtained
    with help of the get_datetime_value() function.

  RETURN
    If is_nulls_eq is true:
       1    if items are equal or both are null
       0    otherwise
    If is_nulls_eq is false:
      -1   a < b or one of items is null
       0   a == b
       1   a > b
*/

int Arg_comparator::compare_datetime()
{
  bool is_null= false;
  uint64_t a_value, b_value;

  /* Get DATE/DATETIME/TIME value of the 'a' item. */
  a_value= (*get_value_func)(session, &a, &a_cache, *b, &is_null);
  if (!is_nulls_eq && is_null)
  {
    if (owner)
      owner->null_value= 1;
    return -1;
  }

  /* Get DATE/DATETIME/TIME value of the 'b' item. */
  b_value= (*get_value_func)(session, &b, &b_cache, *a, &is_null);
  if (is_null)
  {
    if (owner)
      owner->null_value= is_nulls_eq ? 0 : 1;
    return is_nulls_eq ? 1 : -1;
  }

  if (owner)
    owner->null_value= 0;

  /* Compare values. */
  if (is_nulls_eq)
    return (a_value == b_value);
  return (a_value < b_value) ? -1 : ((a_value > b_value) ? 1 : 0);
}


int Arg_comparator::compare_string()
{
  String *res1,*res2;
  if ((res1= (*a)->val_str(&owner->tmp_value1)))
  {
    if ((res2= (*b)->val_str(&owner->tmp_value2)))
    {
      owner->null_value= 0;
      return sortcmp(res1,res2,cmp_collation.collation);
    }
  }
  owner->null_value= 1;
  return -1;
}


/**
  Compare strings byte by byte. End spaces are also compared.

  @retval
    <0  *a < *b
  @retval
     0  *b == *b
  @retval
    >0  *a > *b
*/

int Arg_comparator::compare_binary_string()
{
  String *res1,*res2;
  if ((res1= (*a)->val_str(&owner->tmp_value1)))
  {
    if ((res2= (*b)->val_str(&owner->tmp_value2)))
    {
      owner->null_value= 0;
      uint32_t res1_length= res1->length();
      uint32_t res2_length= res2->length();
      int cmp= memcmp(res1->ptr(), res2->ptr(), min(res1_length,res2_length));
      return cmp ? cmp : (int) (res1_length - res2_length);
    }
  }
  owner->null_value= 1;
  return -1;
}


/**
  Compare strings, but take into account that NULL == NULL.
*/


int Arg_comparator::compare_e_string()
{
  String *res1,*res2;
  res1= (*a)->val_str(&owner->tmp_value1);
  res2= (*b)->val_str(&owner->tmp_value2);
  if (!res1 || !res2)
    return test(res1 == res2);
  return test(sortcmp(res1, res2, cmp_collation.collation) == 0);
}


int Arg_comparator::compare_e_binary_string()
{
  String *res1,*res2;
  res1= (*a)->val_str(&owner->tmp_value1);
  res2= (*b)->val_str(&owner->tmp_value2);
  if (!res1 || !res2)
    return test(res1 == res2);
  return test(stringcmp(res1, res2) == 0);
}


int Arg_comparator::compare_real()
{
  /*
    Fix yet another manifestation of Bug#2338. 'Volatile' will instruct
    gcc to flush double values out of 80-bit Intel FPU registers before
    performing the comparison.
  */
  volatile double val1, val2;
  val1= (*a)->val_real();
  if (!(*a)->null_value)
  {
    val2= (*b)->val_real();
    if (!(*b)->null_value)
    {
      owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2) return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}

int Arg_comparator::compare_decimal()
{
  type::Decimal value1;
  type::Decimal *val1= (*a)->val_decimal(&value1);
  if (!(*a)->null_value)
  {
    type::Decimal value2;
    type::Decimal *val2= (*b)->val_decimal(&value2);
    if (!(*b)->null_value)
    {
      owner->null_value= 0;
      return class_decimal_cmp(val1, val2);
    }
  }
  owner->null_value= 1;
  return -1;
}

int Arg_comparator::compare_e_real()
{
  double val1= (*a)->val_real();
  double val2= (*b)->val_real();
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return test(val1 == val2);
}

int Arg_comparator::compare_e_decimal()
{
  type::Decimal value1, value2;
  type::Decimal *val1= (*a)->val_decimal(&value1);
  type::Decimal *val2= (*b)->val_decimal(&value2);
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return test(class_decimal_cmp(val1, val2) == 0);
}


int Arg_comparator::compare_real_fixed()
{
  /*
    Fix yet another manifestation of Bug#2338. 'Volatile' will instruct
    gcc to flush double values out of 80-bit Intel FPU registers before
    performing the comparison.
  */
  volatile double val1, val2;
  val1= (*a)->val_real();
  if (!(*a)->null_value)
  {
    val2= (*b)->val_real();
    if (!(*b)->null_value)
    {
      owner->null_value= 0;
      if (val1 == val2 || fabs(val1 - val2) < precision)
        return 0;
      if (val1 < val2)
        return -1;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}


int Arg_comparator::compare_e_real_fixed()
{
  double val1= (*a)->val_real();
  double val2= (*b)->val_real();
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return test(val1 == val2 || fabs(val1 - val2) < precision);
}


int Arg_comparator::compare_int_signed()
{
  int64_t val1= (*a)->val_int();
  if (!(*a)->null_value)
  {
    int64_t val2= (*b)->val_int();
    if (!(*b)->null_value)
    {
      owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2)   return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}


/**
  Compare values as BIGINT UNSIGNED.
*/

int Arg_comparator::compare_int_unsigned()
{
  uint64_t val1= (*a)->val_int();
  if (!(*a)->null_value)
  {
    uint64_t val2= (*b)->val_int();
    if (!(*b)->null_value)
    {
      owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2)   return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}


/**
  Compare signed (*a) with unsigned (*B)
*/

int Arg_comparator::compare_int_signed_unsigned()
{
  int64_t sval1= (*a)->val_int();
  if (!(*a)->null_value)
  {
    uint64_t uval2= (uint64_t)(*b)->val_int();
    if (!(*b)->null_value)
    {
      owner->null_value= 0;
      if (sval1 < 0 || (uint64_t)sval1 < uval2)
        return -1;
      if ((uint64_t)sval1 == uval2)
        return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}


/**
  Compare unsigned (*a) with signed (*B)
*/

int Arg_comparator::compare_int_unsigned_signed()
{
  uint64_t uval1= (uint64_t)(*a)->val_int();
  if (!(*a)->null_value)
  {
    int64_t sval2= (*b)->val_int();
    if (!(*b)->null_value)
    {
      owner->null_value= 0;
      if (sval2 < 0)
        return 1;
      if (uval1 < (uint64_t)sval2)
        return -1;
      if (uval1 == (uint64_t)sval2)
        return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}


int Arg_comparator::compare_e_int()
{
  int64_t val1= (*a)->val_int();
  int64_t val2= (*b)->val_int();
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return test(val1 == val2);
}

/**
  Compare unsigned *a with signed *b or signed *a with unsigned *b.
*/
int Arg_comparator::compare_e_int_diff_signedness()
{
  int64_t val1= (*a)->val_int();
  int64_t val2= (*b)->val_int();
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return (val1 >= 0) && test(val1 == val2);
}

int Arg_comparator::compare_row()
{
  int res= 0;
  bool was_null= 0;
  (*a)->bring_value();
  (*b)->bring_value();
  uint32_t n= (*a)->cols();
  for (uint32_t i= 0; i<n; i++)
  {
    res= comparators[i].compare();
    if (owner->null_value)
    {
      // NULL was compared
      switch (owner->functype()) {
      case Item_func::NE_FUNC:
        break; // NE never aborts on NULL even if abort_on_null is set
      case Item_func::LT_FUNC:
      case Item_func::LE_FUNC:
      case Item_func::GT_FUNC:
      case Item_func::GE_FUNC:
        return -1; // <, <=, > and >= always fail on NULL
      default: // EQ_FUNC
        if (owner->abort_on_null)
          return -1; // We do not need correct NULL returning
      }
      was_null= 1;
      owner->null_value= 0;
      res= 0;  // continue comparison (maybe we will meet explicit difference)
    }
    else if (res)
      return res;
  }
  if (was_null)
  {
    /*
      There was NULL(s) in comparison in some parts, but there was no
      explicit difference in other parts, so we have to return NULL.
    */
    owner->null_value= 1;
    return -1;
  }
  return 0;
}


int Arg_comparator::compare_e_row()
{
  (*a)->bring_value();
  (*b)->bring_value();
  uint32_t n= (*a)->cols();
  for (uint32_t i= 0; i<n; i++)
  {
    if (!comparators[i].compare())
      return 0;
  }
  return 1;
}


void Item_func_truth::fix_length_and_dec()
{
  maybe_null= 0;
  null_value= 0;
  decimals= 0;
  max_length= 1;
}


void Item_func_truth::print(String *str)
{
  str->append('(');
  args[0]->print(str);
  str->append(STRING_WITH_LEN(" is "));
  if (! affirmative)
    str->append(STRING_WITH_LEN("not "));
  if (value)
    str->append(STRING_WITH_LEN("true"));
  else
    str->append(STRING_WITH_LEN("false"));
  str->append(')');
}


bool Item_func_truth::val_bool()
{
  bool val= args[0]->val_bool();
  if (args[0]->null_value)
  {
    /*
      NULL val IS {true, false} --> false
      NULL val IS NOT {true, false} --> true
    */
    return (! affirmative);
  }

  if (affirmative)
  {
    /* {true, false} val IS {true, false} value */
    return (val == value);
  }

  /* {true, false} val IS NOT {true, false} value */
  return (val != value);
}


int64_t Item_func_truth::val_int()
{
  return (val_bool() ? 1 : 0);
}


bool Item_in_optimizer::fix_left(Session *session, Item **)
{
  if ((!args[0]->fixed && args[0]->fix_fields(session, args)) ||
      (!cache && !(cache= Item_cache::get_cache(args[0]))))
    return 1;

  cache->setup(args[0]);
  if (cache->cols() == 1)
  {
    if ((used_tables_cache= args[0]->used_tables()))
      cache->set_used_tables(OUTER_REF_TABLE_BIT);
    else
      cache->set_used_tables(0);
  }
  else
  {
    uint32_t n= cache->cols();
    for (uint32_t i= 0; i < n; i++)
    {
      if (args[0]->element_index(i)->used_tables())
	((Item_cache *)cache->element_index(i))->set_used_tables(OUTER_REF_TABLE_BIT);
      else
	((Item_cache *)cache->element_index(i))->set_used_tables(0);
    }
    used_tables_cache= args[0]->used_tables();
  }
  not_null_tables_cache= args[0]->not_null_tables();
  with_sum_func= args[0]->with_sum_func;
  if ((const_item_cache= args[0]->const_item()))
    cache->store(args[0]);
  return 0;
}


bool Item_in_optimizer::fix_fields(Session *session, Item **ref)
{
  assert(fixed == 0);
  if (fix_left(session, ref))
    return true;
  if (args[0]->maybe_null)
    maybe_null=1;

  if (!args[1]->fixed && args[1]->fix_fields(session, args+1))
    return true;
  Item_in_subselect * sub= (Item_in_subselect *)args[1];
  if (args[0]->cols() != sub->engine->cols())
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), args[0]->cols());
    return true;
  }
  if (args[1]->maybe_null)
    maybe_null=1;
  with_sum_func= with_sum_func || args[1]->with_sum_func;
  used_tables_cache|= args[1]->used_tables();
  not_null_tables_cache|= args[1]->not_null_tables();
  const_item_cache&= args[1]->const_item();
  fixed= 1;
  return false;
}


int64_t Item_in_optimizer::val_int()
{
  bool tmp;
  assert(fixed == 1);
  cache->store(args[0]);

  if (cache->null_value)
  {
    if (((Item_in_subselect*)args[1])->is_top_level_item())
    {
      /*
        We're evaluating "NULL IN (SELECT ...)". The result can be NULL or
        false, and we can return one instead of another. Just return NULL.
      */
      null_value= 1;
    }
    else
    {
      if (!((Item_in_subselect*)args[1])->is_correlated &&
          result_for_null_param != UNKNOWN)
      {
        /* Use cached value from previous execution */
        null_value= result_for_null_param;
      }
      else
      {
        /*
          We're evaluating "NULL IN (SELECT ...)". The result is:
             false if SELECT produces an empty set, or
             NULL  otherwise.
          We disable the predicates we've pushed down into subselect, run the
          subselect and see if it has produced any rows.
        */
        Item_in_subselect *item_subs=(Item_in_subselect*)args[1];
        if (cache->cols() == 1)
        {
          item_subs->set_cond_guard_var(0, false);
          (void) args[1]->val_bool_result();
          result_for_null_param= null_value= !item_subs->engine->no_rows();
          item_subs->set_cond_guard_var(0, true);
        }
        else
        {
          uint32_t i;
          uint32_t ncols= cache->cols();
          /*
            Turn off the predicates that are based on column compares for
            which the left part is currently NULL
          */
          for (i= 0; i < ncols; i++)
          {
            if (cache->element_index(i)->null_value)
              item_subs->set_cond_guard_var(i, false);
          }

          (void) args[1]->val_bool_result();
          result_for_null_param= null_value= !item_subs->engine->no_rows();

          /* Turn all predicates back on */
          for (i= 0; i < ncols; i++)
            item_subs->set_cond_guard_var(i, true);
        }
      }
    }
    return 0;
  }
  tmp= args[1]->val_bool_result();
  null_value= args[1]->null_value;
  return tmp;
}


void Item_in_optimizer::keep_top_level_cache()
{
  cache->keep_array();
  save_cache= 1;
}


void Item_in_optimizer::cleanup()
{
  item::function::Boolean::cleanup();
  if (!save_cache)
    cache= 0;
  return;
}


bool Item_in_optimizer::is_null()
{
  cache->store(args[0]);
  return (null_value= (cache->null_value || args[1]->is_null()));
}


/**
  Transform an Item_in_optimizer and its arguments with a callback function.

  @param transformer the transformer callback function to be applied to the
         nodes of the tree of the object
  @param parameter to be passed to the transformer

  @detail
    Recursively transform the left and the right operand of this Item. The
    Right operand is an Item_in_subselect or its subclass. To avoid the
    creation of new Items, we use the fact the the left operand of the
    Item_in_subselect is the same as the one of 'this', so instead of
    transforming its operand, we just assign the left operand of the
    Item_in_subselect to be equal to the left operand of 'this'.
    The transformation is not applied further to the subquery operand
    if the IN predicate.

  @returns
    @retval pointer to the transformed item
    @retval NULL if an error occurred
*/

Item *Item_in_optimizer::transform(Item_transformer transformer, unsigned char *argument)
{
  Item *new_item;

  assert(arg_count == 2);

  /* Transform the left IN operand. */
  new_item= (*args)->transform(transformer, argument);
  if (!new_item)
    return 0;
  *args= new_item;

  /*
    Transform the right IN operand which should be an Item_in_subselect or a
    subclass of it. The left operand of the IN must be the same as the left
    operand of this Item_in_optimizer, so in this case there is no further
    transformation, we only make both operands the same.
    TODO: is it the way it should be?
  */
  assert((args[1])->type() == Item::SUBSELECT_ITEM &&
              (((Item_subselect*)(args[1]))->substype() ==
               Item_subselect::IN_SUBS ||
               ((Item_subselect*)(args[1]))->substype() ==
               Item_subselect::ALL_SUBS ||
               ((Item_subselect*)(args[1]))->substype() ==
               Item_subselect::ANY_SUBS));

  Item_in_subselect *in_arg= (Item_in_subselect*)args[1];
  in_arg->left_expr= args[0];

  return (this->*transformer)(argument);
}



int64_t Item_func_eq::val_int()
{
  assert(fixed == 1);
  int value= cmp.compare();
  return value == 0 ? 1 : 0;
}


/** Same as Item_func_eq, but NULL = NULL. */

void Item_func_equal::fix_length_and_dec()
{
  Item_bool_func2::fix_length_and_dec();
  maybe_null=null_value=0;
}

int64_t Item_func_equal::val_int()
{
  assert(fixed == 1);
  return cmp.compare();
}

int64_t Item_func_ne::val_int()
{
  assert(fixed == 1);
  int value= cmp.compare();
  return value != 0 && !null_value ? 1 : 0;
}


int64_t Item_func_ge::val_int()
{
  assert(fixed == 1);
  int value= cmp.compare();
  return value >= 0 ? 1 : 0;
}


int64_t Item_func_gt::val_int()
{
  assert(fixed == 1);
  int value= cmp.compare();
  return value > 0 ? 1 : 0;
}

int64_t Item_func_le::val_int()
{
  assert(fixed == 1);
  int value= cmp.compare();
  return value <= 0 && !null_value ? 1 : 0;
}


int64_t Item_func_lt::val_int()
{
  assert(fixed == 1);
  int value= cmp.compare();
  return value < 0 && !null_value ? 1 : 0;
}


int64_t Item_func_strcmp::val_int()
{
  assert(fixed == 1);
  String *a=args[0]->val_str(&tmp_value1);
  String *b=args[1]->val_str(&tmp_value2);
  if (!a || !b)
  {
    null_value=1;
    return 0;
  }
  int value= sortcmp(a,b,cmp.cmp_collation.collation);
  null_value=0;
  return !value ? 0 : (value < 0 ? (int64_t) -1 : (int64_t) 1);
}


bool Item_func_opt_neg::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM)
    return 0;
  Item_func *item_func=(Item_func*) item;
  if (arg_count != item_func->arg_count ||
      functype() != item_func->functype())
    return 0;
  if (negated != ((Item_func_opt_neg *) item_func)->negated)
    return 0;
  for (uint32_t i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func->arguments()[i], binary_cmp))
      return 0;
  return 1;
}


void Item_func_interval::fix_length_and_dec()
{
  uint32_t rows= row->cols();

  use_decimal_comparison= ((row->element_index(0)->result_type() ==
                            DECIMAL_RESULT) ||
                           (row->element_index(0)->result_type() ==
                            INT_RESULT));
  if (rows > 8)
  {
    bool not_null_consts= true;

    for (uint32_t i= 1; not_null_consts && i < rows; i++)
    {
      Item *el= row->element_index(i);
      not_null_consts&= el->const_item() & !el->is_null();
    }

    if (not_null_consts &&
        (intervals=
          (interval_range*) memory::sql_alloc(sizeof(interval_range) * (rows - 1))))
    {
      if (use_decimal_comparison)
      {
        for (uint32_t i= 1; i < rows; i++)
        {
          Item *el= row->element_index(i);
          interval_range *range= intervals + (i-1);
          if ((el->result_type() == DECIMAL_RESULT) ||
              (el->result_type() == INT_RESULT))
          {
            range->type= DECIMAL_RESULT;
            range->dec.init();
            type::Decimal *dec= el->val_decimal(&range->dec);
            if (dec != &range->dec)
            {
              range->dec= *dec;
              range->dec.fix_buffer_pointer();
            }
          }
          else
          {
            range->type= REAL_RESULT;
            range->dbl= el->val_real();
          }
        }
      }
      else
      {
        for (uint32_t i= 1; i < rows; i++)
        {
          intervals[i-1].dbl= row->element_index(i)->val_real();
        }
      }
    }
  }
  maybe_null= 0;
  max_length= 2;
  used_tables_cache|= row->used_tables();
  not_null_tables_cache= row->not_null_tables();
  with_sum_func= with_sum_func || row->with_sum_func;
  const_item_cache&= row->const_item();
}


/**
  Execute Item_func_interval().

  @note
    If we are doing a decimal comparison, we are evaluating the first
    item twice.

  @return
    - -1 if null value,
    - 0 if lower than lowest
    - 1 - arg_count-1 if between args[n] and args[n+1]
    - arg_count if higher than biggest argument
*/

int64_t Item_func_interval::val_int()
{
  assert(fixed == 1);
  double value;
  type::Decimal dec_buf, *dec= NULL;
  uint32_t i;

  if (use_decimal_comparison)
  {
    dec= row->element_index(0)->val_decimal(&dec_buf);
    if (row->element_index(0)->null_value)
      return -1;
    class_decimal2double(E_DEC_FATAL_ERROR, dec, &value);
  }
  else
  {
    value= row->element_index(0)->val_real();
    if (row->element_index(0)->null_value)
      return -1;
  }

  if (intervals)
  {					// Use binary search to find interval
    uint32_t start,end;
    start= 0;
    end=   row->cols()-2;
    while (start != end)
    {
      uint32_t mid= (start + end + 1) / 2;
      interval_range *range= intervals + mid;
      bool cmp_result;
      /*
        The values in the range intervall may have different types,
        Only do a decimal comparision of the first argument is a decimal
        and we are comparing against a decimal
      */
      if (dec && range->type == DECIMAL_RESULT)
        cmp_result= class_decimal_cmp(&range->dec, dec) <= 0;
      else
        cmp_result= (range->dbl <= value);
      if (cmp_result)
	start= mid;
      else
	end= mid - 1;
    }
    interval_range *range= intervals+start;
    return ((dec && range->type == DECIMAL_RESULT) ?
            class_decimal_cmp(dec, &range->dec) < 0 :
            value < range->dbl) ? 0 : start + 1;
  }

  for (i=1 ; i < row->cols() ; i++)
  {
    Item *el= row->element_index(i);
    if (use_decimal_comparison &&
        ((el->result_type() == DECIMAL_RESULT) ||
         (el->result_type() == INT_RESULT)))
    {
      type::Decimal e_dec_buf, *e_dec= el->val_decimal(&e_dec_buf);
      /* Skip NULL ranges. */
      if (el->null_value)
        continue;
      if (class_decimal_cmp(e_dec, dec) > 0)
        return i - 1;
    }
    else
    {
      double val= el->val_real();
      /* Skip NULL ranges. */
      if (el->null_value)
        continue;
      if (val > value)
        return i - 1;
    }
  }
  return i-1;
}


/**
  Perform context analysis of a BETWEEN item tree.

    This function performs context analysis (name resolution) and calculates
    various attributes of the item tree with Item_func_between as its root.
    The function saves in ref the pointer to the item or to a newly created
    item that is considered as a replacement for the original one.

  @param session     reference to the global context of the query thread
  @param ref     pointer to Item* variable where pointer to resulting "fixed"
                 item is to be assigned

  @note
    Let T0(e)/T1(e) be the value of not_null_tables(e) when e is used on
    a predicate/function level. Then it's easy to show that:
    @verbatim
      T0(e BETWEEN e1 AND e2)     = union(T1(e),T1(e1),T1(e2))
      T1(e BETWEEN e1 AND e2)     = union(T1(e),intersection(T1(e1),T1(e2)))
      T0(e NOT BETWEEN e1 AND e2) = union(T1(e),intersection(T1(e1),T1(e2)))
      T1(e NOT BETWEEN e1 AND e2) = union(T1(e),intersection(T1(e1),T1(e2)))
    @endverbatim

  @retval
    0   ok
  @retval
    1   got error
*/

bool Item_func_between::fix_fields(Session *session, Item **ref)
{
  if (Item_func_opt_neg::fix_fields(session, ref))
    return 1;

  session->lex().current_select->between_count++;

  /* not_null_tables_cache == union(T1(e),T1(e1),T1(e2)) */
  if (pred_level && !negated)
    return 0;

  /* not_null_tables_cache == union(T1(e), intersection(T1(e1),T1(e2))) */
  not_null_tables_cache= (args[0]->not_null_tables() |
                          (args[1]->not_null_tables() &
                           args[2]->not_null_tables()));

  return 0;
}


void Item_func_between::fix_length_and_dec()
{
  max_length= 1;
  int i;
  bool datetime_found= false;
  compare_as_dates= true;

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditions here
  */
  if (!args[0] || !args[1] || !args[2])
    return;
  if ( agg_cmp_type(&cmp_type, args, 3))
    return;
  if (cmp_type == STRING_RESULT &&
      agg_arg_charsets(cmp_collation, args, 3, MY_COLL_CMP_CONV, 1))
   return;

  /*
    Detect the comparison of DATE/DATETIME items.
    At least one of items should be a DATE/DATETIME item and other items
    should return the STRING result.
  */
  if (cmp_type == STRING_RESULT)
  {
    for (i= 0; i < 3; i++)
    {
      if (args[i]->is_datetime())
      {
        datetime_found= true;
        continue;
      }
    }
  }
  if (!datetime_found)
    compare_as_dates= false;

  if (compare_as_dates)
  {
    ge_cmp.set_datetime_cmp_func(args, args + 1);
    le_cmp.set_datetime_cmp_func(args, args + 2);
  }
  else if (args[0]->real_item()->type() == FIELD_ITEM)
  {
    Item_field *field_item= (Item_field*) (args[0]->real_item());
    if (field_item->field->can_be_compared_as_int64_t())
    {
      /*
        The following can't be recoded with || as convert_constant_item
        changes the argument
      */
      if (convert_constant_item(&getSession(), field_item, &args[1]))
        cmp_type=INT_RESULT;			// Works for all types.
      if (convert_constant_item(&getSession(), field_item, &args[2]))
        cmp_type=INT_RESULT;			// Works for all types.
    }
  }
}


int64_t Item_func_between::val_int()
{						// ANSI BETWEEN
  assert(fixed == 1);
  if (compare_as_dates)
  {
    int ge_res, le_res;

    ge_res= ge_cmp.compare();
    if ((null_value= args[0]->null_value))
      return 0;
    le_res= le_cmp.compare();

    if (!args[1]->null_value && !args[2]->null_value)
      return (int64_t) ((ge_res >= 0 && le_res <=0) != negated);
    else if (args[1]->null_value)
    {
      null_value= le_res > 0;			// not null if false range.
    }
    else
    {
      null_value= ge_res < 0;
    }
  }
  else if (cmp_type == STRING_RESULT)
  {
    String *value,*a,*b;
    value=args[0]->val_str(&value0);
    if ((null_value=args[0]->null_value))
      return 0;
    a=args[1]->val_str(&value1);
    b=args[2]->val_str(&value2);
    if (!args[1]->null_value && !args[2]->null_value)
      return (int64_t) ((sortcmp(value,a,cmp_collation.collation) >= 0 &&
                          sortcmp(value,b,cmp_collation.collation) <= 0) !=
                         negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      // Set to not null if false range.
      null_value= sortcmp(value,b,cmp_collation.collation) <= 0;
    }
    else
    {
      // Set to not null if false range.
      null_value= sortcmp(value,a,cmp_collation.collation) >= 0;
    }
  }
  else if (cmp_type == INT_RESULT)
  {
    int64_t value=args[0]->val_int(), a, b;
    if ((null_value=args[0]->null_value))
      return 0;
    a=args[1]->val_int();
    b=args[2]->val_int();
    if (!args[1]->null_value && !args[2]->null_value)
      return (int64_t) ((value >= a && value <= b) != negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= value <= b;			// not null if false range.
    }
    else
    {
      null_value= value >= a;
    }
  }
  else if (cmp_type == DECIMAL_RESULT)
  {
    type::Decimal dec_buf, *dec= args[0]->val_decimal(&dec_buf),
               a_buf, *a_dec, b_buf, *b_dec;
    if ((null_value=args[0]->null_value))
      return 0;
    a_dec= args[1]->val_decimal(&a_buf);
    b_dec= args[2]->val_decimal(&b_buf);
    if (!args[1]->null_value && !args[2]->null_value)
      return (int64_t) ((class_decimal_cmp(dec, a_dec) >= 0 &&
                          class_decimal_cmp(dec, b_dec) <= 0) != negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
      null_value= (class_decimal_cmp(dec, b_dec) <= 0);
    else
      null_value= (class_decimal_cmp(dec, a_dec) >= 0);
  }
  else
  {
    double value= args[0]->val_real(),a,b;
    if ((null_value=args[0]->null_value))
      return 0;
    a= args[1]->val_real();
    b= args[2]->val_real();
    if (!args[1]->null_value && !args[2]->null_value)
      return (int64_t) ((value >= a && value <= b) != negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= value <= b;			// not null if false range.
    }
    else
    {
      null_value= value >= a;
    }
  }
  return (int64_t) (!null_value && negated);
}


void Item_func_between::print(String *str)
{
  str->append('(');
  args[0]->print(str);
  if (negated)
    str->append(STRING_WITH_LEN(" not"));
  str->append(STRING_WITH_LEN(" between "));
  args[1]->print(str);
  str->append(STRING_WITH_LEN(" and "));
  args[2]->print(str);
  str->append(')');
}

void
Item_func_ifnull::fix_length_and_dec()
{
  agg_result_type(&hybrid_type, args, 2);
  maybe_null= args[1]->maybe_null;
  decimals= max(args[0]->decimals, args[1]->decimals);
  unsigned_flag= args[0]->unsigned_flag && args[1]->unsigned_flag;

  if (hybrid_type == DECIMAL_RESULT || hybrid_type == INT_RESULT)
  {
    int len0= args[0]->max_length - args[0]->decimals
      - (args[0]->unsigned_flag ? 0 : 1);

    int len1= args[1]->max_length - args[1]->decimals
      - (args[1]->unsigned_flag ? 0 : 1);

    max_length= max(len0, len1) + decimals + (unsigned_flag ? 0 : 1);
  }
  else
  {
    max_length= max(args[0]->max_length, args[1]->max_length);
  }

  switch (hybrid_type)
  {
  case STRING_RESULT:
    agg_arg_charsets(collation, args, arg_count, MY_COLL_CMP_CONV, 1);
    break;

  case DECIMAL_RESULT:
  case REAL_RESULT:
    break;

  case INT_RESULT:
    decimals= 0;
    break;

  case ROW_RESULT:
    assert(0);
  }

  cached_field_type= agg_field_type(args, 2);
}


uint32_t Item_func_ifnull::decimal_precision() const
{
  int max_int_part= max(args[0]->decimal_int_part(),args[1]->decimal_int_part());
  return min(max_int_part + decimals, DECIMAL_MAX_PRECISION);
}


enum_field_types Item_func_ifnull::field_type() const
{
  return cached_field_type;
}

Field *Item_func_ifnull::tmp_table_field(Table *table)
{
  return tmp_table_field_from_field_type(table, 0);
}

double
Item_func_ifnull::real_op()
{
  assert(fixed == 1);
  double value= args[0]->val_real();
  if (!args[0]->null_value)
  {
    null_value=0;
    return value;
  }
  value= args[1]->val_real();
  if ((null_value=args[1]->null_value))
    return 0.0;
  return value;
}

int64_t
Item_func_ifnull::int_op()
{
  assert(fixed == 1);
  int64_t value=args[0]->val_int();
  if (!args[0]->null_value)
  {
    null_value=0;
    return value;
  }
  value=args[1]->val_int();
  if ((null_value=args[1]->null_value))
    return 0;
  return value;
}


type::Decimal *Item_func_ifnull::decimal_op(type::Decimal *decimal_value)
{
  assert(fixed == 1);
  type::Decimal *value= args[0]->val_decimal(decimal_value);
  if (!args[0]->null_value)
  {
    null_value= 0;
    return value;
  }
  value= args[1]->val_decimal(decimal_value);
  if ((null_value= args[1]->null_value))
    return 0;
  return value;
}


String *
Item_func_ifnull::str_op(String *str)
{
  assert(fixed == 1);
  String *res  =args[0]->val_str(str);
  if (!args[0]->null_value)
  {
    null_value=0;
    res->set_charset(collation.collation);
    return res;
  }
  res=args[1]->val_str(str);
  if ((null_value=args[1]->null_value))
    return 0;
  res->set_charset(collation.collation);
  return res;
}


/**
  Perform context analysis of an IF item tree.

    This function performs context analysis (name resolution) and calculates
    various attributes of the item tree with Item_func_if as its root.
    The function saves in ref the pointer to the item or to a newly created
    item that is considered as a replacement for the original one.

  @param session     reference to the global context of the query thread
  @param ref     pointer to Item* variable where pointer to resulting "fixed"
                 item is to be assigned

  @note
    Let T0(e)/T1(e) be the value of not_null_tables(e) when e is used on
    a predicate/function level. Then it's easy to show that:
    @verbatim
      T0(IF(e,e1,e2)  = T1(IF(e,e1,e2))
      T1(IF(e,e1,e2)) = intersection(T1(e1),T1(e2))
    @endverbatim

  @retval
    0   ok
  @retval
    1   got error
*/

bool
Item_func_if::fix_fields(Session *session, Item **ref)
{
  assert(fixed == 0);
  args[0]->top_level_item();

  if (Item_func::fix_fields(session, ref))
    return 1;

  not_null_tables_cache= (args[1]->not_null_tables() &
                          args[2]->not_null_tables());

  return 0;
}


void
Item_func_if::fix_length_and_dec()
{
  maybe_null= args[1]->maybe_null || args[2]->maybe_null;
  decimals= max(args[1]->decimals, args[2]->decimals);
  unsigned_flag= args[1]->unsigned_flag && args[2]->unsigned_flag;

  enum Item_result arg1_type= args[1]->result_type();
  enum Item_result arg2_type= args[2]->result_type();
  bool null1= args[1]->const_item() && args[1]->null_value;
  bool null2= args[2]->const_item() && args[2]->null_value;

  if (null1)
  {
    cached_result_type= arg2_type;
    collation.set(args[2]->collation.collation);
    cached_field_type= args[2]->field_type();
  }
  else if (null2)
  {
    cached_result_type= arg1_type;
    collation.set(args[1]->collation.collation);
    cached_field_type= args[1]->field_type();
  }
  else
  {
    agg_result_type(&cached_result_type, args+1, 2);
    if (cached_result_type == STRING_RESULT)
    {
      if (agg_arg_charsets(collation, args+1, 2, MY_COLL_ALLOW_CONV, 1))
        return;
    }
    else
    {
      collation.set(&my_charset_bin);	// Number
    }
    cached_field_type= agg_field_type(args + 1, 2);
  }

  if ((cached_result_type == DECIMAL_RESULT )
      || (cached_result_type == INT_RESULT))
  {
    int len1= args[1]->max_length - args[1]->decimals
      - (args[1]->unsigned_flag ? 0 : 1);

    int len2= args[2]->max_length - args[2]->decimals
      - (args[2]->unsigned_flag ? 0 : 1);

    max_length= max(len1, len2) + decimals + (unsigned_flag ? 0 : 1);
  }
  else
    max_length= max(args[1]->max_length, args[2]->max_length);
}


uint32_t Item_func_if::decimal_precision() const
{
  int precision= (max(args[1]->decimal_int_part(),args[2]->decimal_int_part())+
                  decimals);
  return min(precision, DECIMAL_MAX_PRECISION);
}


double
Item_func_if::val_real()
{
  assert(fixed == 1);
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  double value= arg->val_real();
  null_value=arg->null_value;
  return value;
}

int64_t
Item_func_if::val_int()
{
  assert(fixed == 1);
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  int64_t value=arg->val_int();
  null_value=arg->null_value;
  return value;
}

String *
Item_func_if::val_str(String *str)
{
  assert(fixed == 1);
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  String *res=arg->val_str(str);
  if (res)
    res->set_charset(collation.collation);
  null_value=arg->null_value;
  return res;
}


type::Decimal *
Item_func_if::val_decimal(type::Decimal *decimal_value)
{
  assert(fixed == 1);
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  type::Decimal *value= arg->val_decimal(decimal_value);
  null_value= arg->null_value;
  return value;
}


void
Item_func_nullif::fix_length_and_dec()
{
  Item_bool_func2::fix_length_and_dec();
  maybe_null=1;
  if (args[0])					// Only false if EOM
  {
    max_length=args[0]->max_length;
    decimals=args[0]->decimals;
    unsigned_flag= args[0]->unsigned_flag;
    cached_result_type= args[0]->result_type();
    if (cached_result_type == STRING_RESULT &&
        agg_arg_charsets(collation, args, arg_count, MY_COLL_CMP_CONV, 1))
      return;
  }
}


/**
  @note
  Note that we have to evaluate the first argument twice as the compare
  may have been done with a different type than return value
  @return
    NULL  if arguments are equal
  @return
    the first argument if not equal
*/

double
Item_func_nullif::val_real()
{
  assert(fixed == 1);
  double value;
  if (!cmp.compare())
  {
    null_value=1;
    return 0.0;
  }
  value= args[0]->val_real();
  null_value=args[0]->null_value;
  return value;
}

int64_t
Item_func_nullif::val_int()
{
  assert(fixed == 1);
  int64_t value;
  if (!cmp.compare())
  {
    null_value=1;
    return 0;
  }
  value=args[0]->val_int();
  null_value=args[0]->null_value;
  return value;
}

String *
Item_func_nullif::val_str(String *str)
{
  assert(fixed == 1);
  String *res;
  if (!cmp.compare())
  {
    null_value=1;
    return 0;
  }
  res=args[0]->val_str(str);
  null_value=args[0]->null_value;
  return res;
}


type::Decimal *
Item_func_nullif::val_decimal(type::Decimal * decimal_value)
{
  assert(fixed == 1);
  type::Decimal *res;
  if (!cmp.compare())
  {
    null_value=1;
    return 0;
  }
  res= args[0]->val_decimal(decimal_value);
  null_value= args[0]->null_value;
  return res;
}


bool
Item_func_nullif::is_null()
{
  return (null_value= (!cmp.compare() ? 1 : args[0]->null_value));
}


/**
    Find and return matching items for CASE or ELSE item if all compares
    are failed or NULL if ELSE item isn't defined.

  IMPLEMENTATION
    In order to do correct comparisons of the CASE expression (the expression
    between CASE and the first WHEN) with each WHEN expression several
    comparators are used. One for each result type. CASE expression can be
    evaluated up to # of different result types are used. To check whether
    the CASE expression already was evaluated for a particular result type
    a bit mapped variable value_added_map is used. Result types are mapped
    to it according to their int values i.e. STRING_RESULT is mapped to bit
    0, REAL_RESULT to bit 1, so on.

  @retval
    NULL  Nothing found and there is no ELSE expression defined
  @retval
    item  Found item or ELSE item if defined and all comparisons are
           failed
*/

Item *Item_func_case::find_item(String *)
{
  uint32_t value_added_map= 0;

  if (first_expr_num == -1)
  {
    for (uint32_t i=0 ; i < ncases ; i+=2)
    {
      // No expression between CASE and the first WHEN
      if (args[i]->val_bool())
	return args[i+1];
      continue;
    }
  }
  else
  {
    /* Compare every WHEN argument with it and return the first match */
    for (uint32_t i=0 ; i < ncases ; i+=2)
    {
      cmp_type= item_cmp_type(left_result_type, args[i]->result_type());
      assert(cmp_type != ROW_RESULT);
      assert(cmp_items[(uint32_t)cmp_type]);
      if (!(value_added_map & (1<<(uint32_t)cmp_type)))
      {
        cmp_items[(uint32_t)cmp_type]->store_value(args[first_expr_num]);
        if ((null_value=args[first_expr_num]->null_value))
          return else_expr_num != -1 ? args[else_expr_num] : 0;
        value_added_map|= 1<<(uint32_t)cmp_type;
      }
      if (!cmp_items[(uint32_t)cmp_type]->cmp(args[i]) && !args[i]->null_value)
        return args[i + 1];
    }
  }
  // No, WHEN clauses all missed, return ELSE expression
  return else_expr_num != -1 ? args[else_expr_num] : 0;
}


String *Item_func_case::val_str(String *str)
{
  assert(fixed == 1);
  String *res;
  Item *item=find_item(str);

  if (!item)
  {
    null_value=1;
    return 0;
  }
  null_value= 0;
  if (!(res=item->val_str(str)))
    null_value= 1;
  return res;
}


int64_t Item_func_case::val_int()
{
  assert(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff,sizeof(buff),default_charset());
  Item *item=find_item(&dummy_str);
  int64_t res;

  if (!item)
  {
    null_value=1;
    return 0;
  }
  res=item->val_int();
  null_value=item->null_value;
  return res;
}

double Item_func_case::val_real()
{
  assert(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff,sizeof(buff),default_charset());
  Item *item=find_item(&dummy_str);
  double res;

  if (!item)
  {
    null_value=1;
    return 0;
  }
  res= item->val_real();
  null_value=item->null_value;
  return res;
}


type::Decimal *Item_func_case::val_decimal(type::Decimal *decimal_value)
{
  assert(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff, sizeof(buff), default_charset());
  Item *item= find_item(&dummy_str);
  type::Decimal *res;

  if (!item)
  {
    null_value=1;
    return 0;
  }

  res= item->val_decimal(decimal_value);
  null_value= item->null_value;
  return res;
}


bool Item_func_case::fix_fields(Session *session, Item **ref)
{
  /*
    buff should match stack usage from
    Item_func_case::val_int() -> Item_func_case::find_item()
  */
  unsigned char buff[MAX_FIELD_WIDTH*2+sizeof(String)*2+sizeof(String*)*2
                     +sizeof(double)*2+sizeof(int64_t)*2];
  bool res= Item_func::fix_fields(session, ref);
  /*
    Call check_stack_overrun after fix_fields to be sure that stack variable
    is not optimized away
  */
  if (check_stack_overrun(session, STACK_MIN_SIZE, buff))
    return true;				// Fatal error flag is set!
  return res;
}


void Item_func_case::agg_str_lengths(Item* arg)
{
  set_if_bigger(max_length, arg->max_length);
  set_if_bigger(decimals, arg->decimals);
  unsigned_flag= unsigned_flag && arg->unsigned_flag;
}


void Item_func_case::agg_num_lengths(Item *arg)
{
  uint32_t len= class_decimal_length_to_precision(arg->max_length, arg->decimals,
                                           arg->unsigned_flag) - arg->decimals;
  set_if_bigger(max_length, len);
  set_if_bigger(decimals, arg->decimals);
  unsigned_flag= unsigned_flag && arg->unsigned_flag;
}


void Item_func_case::fix_length_and_dec()
{
  Item **agg;
  uint32_t nagg;
  uint32_t found_types= 0;
  if (!(agg= (Item**) memory::sql_alloc(sizeof(Item*)*(ncases+1))))
    return;

  /*
    Aggregate all THEN and ELSE expression types
    and collations when string result
  */

  for (nagg= 0 ; nagg < ncases/2 ; nagg++)
    agg[nagg]= args[nagg*2+1];

  if (else_expr_num != -1)
    agg[nagg++]= args[else_expr_num];

  agg_result_type(&cached_result_type, agg, nagg);
  if ((cached_result_type == STRING_RESULT) &&
      agg_arg_charsets(collation, agg, nagg, MY_COLL_ALLOW_CONV, 1))
    return;

  cached_field_type= agg_field_type(agg, nagg);
  /*
    Aggregate first expression and all THEN expression types
    and collations when string comparison
  */
  if (first_expr_num != -1)
  {
    agg[0]= args[first_expr_num];
    left_result_type= agg[0]->result_type();

    for (nagg= 0; nagg < ncases/2 ; nagg++)
      agg[nagg+1]= args[nagg*2];
    nagg++;
    if (!(found_types= collect_cmp_types(agg, nagg)))
      return;

    for (int i= STRING_RESULT; i <= DECIMAL_RESULT; i++)
    {
      if (found_types & (1 << i) && !cmp_items[i])
      {
        assert((Item_result)i != ROW_RESULT);
        if ((Item_result)i == STRING_RESULT &&
            agg_arg_charsets(cmp_collation, agg, nagg, MY_COLL_CMP_CONV, 1))
          return;
        if (!(cmp_items[i]=
            cmp_item::get_comparator((Item_result)i,
                                     cmp_collation.collation)))
          return;
      }
    }
  }

  if (else_expr_num == -1 || args[else_expr_num]->maybe_null)
    maybe_null=1;

  max_length=0;
  decimals=0;
  unsigned_flag= true;
  if (cached_result_type == STRING_RESULT)
  {
    for (uint32_t i= 0; i < ncases; i+= 2)
      agg_str_lengths(args[i + 1]);
    if (else_expr_num != -1)
      agg_str_lengths(args[else_expr_num]);
  }
  else
  {
    for (uint32_t i= 0; i < ncases; i+= 2)
      agg_num_lengths(args[i + 1]);
    if (else_expr_num != -1)
      agg_num_lengths(args[else_expr_num]);
    max_length= class_decimal_precision_to_length(max_length + decimals, decimals,
                                               unsigned_flag);
  }
}


uint32_t Item_func_case::decimal_precision() const
{
  int max_int_part=0;
  for (uint32_t i=0 ; i < ncases ; i+=2)
    set_if_bigger(max_int_part, args[i+1]->decimal_int_part());

  if (else_expr_num != -1)
    set_if_bigger(max_int_part, args[else_expr_num]->decimal_int_part());
  return min(max_int_part + decimals, DECIMAL_MAX_PRECISION);
}


/**
  @todo
    Fix this so that it prints the whole CASE expression
*/

void Item_func_case::print(String *str)
{
  str->append(STRING_WITH_LEN("(case "));
  if (first_expr_num != -1)
  {
    args[first_expr_num]->print(str);
    str->append(' ');
  }
  for (uint32_t i=0 ; i < ncases ; i+=2)
  {
    str->append(STRING_WITH_LEN("when "));
    args[i]->print(str);
    str->append(STRING_WITH_LEN(" then "));
    args[i+1]->print(str);
    str->append(' ');
  }
  if (else_expr_num != -1)
  {
    str->append(STRING_WITH_LEN("else "));
    args[else_expr_num]->print(str);
    str->append(' ');
  }
  str->append(STRING_WITH_LEN("end)"));
}


void Item_func_case::cleanup()
{
  Item_func::cleanup();
  for (int i= STRING_RESULT; i <= DECIMAL_RESULT; i++)
  {
    delete cmp_items[i];
    cmp_items[i]= 0;
  }
}


/**
  Coalesce - return first not NULL argument.
*/

String *Item_func_coalesce::str_op(String *str)
{
  assert(fixed == 1);
  null_value=0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    String *res;
    if ((res=args[i]->val_str(str)))
      return res;
  }
  null_value=1;
  return 0;
}

int64_t Item_func_coalesce::int_op()
{
  assert(fixed == 1);
  null_value=0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    int64_t res=args[i]->val_int();
    if (!args[i]->null_value)
      return res;
  }
  null_value=1;
  return 0;
}

double Item_func_coalesce::real_op()
{
  assert(fixed == 1);
  null_value=0;
  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    double res= args[i]->val_real();
    if (!args[i]->null_value)
      return res;
  }
  null_value=1;
  return 0;
}


type::Decimal *Item_func_coalesce::decimal_op(type::Decimal *decimal_value)
{
  assert(fixed == 1);
  null_value= 0;
  for (uint32_t i= 0; i < arg_count; i++)
  {
    type::Decimal *res= args[i]->val_decimal(decimal_value);
    if (!args[i]->null_value)
      return res;
  }
  null_value=1;
  return 0;
}


void Item_func_coalesce::fix_length_and_dec()
{
  cached_field_type= agg_field_type(args, arg_count);
  agg_result_type(&hybrid_type, args, arg_count);

  switch (hybrid_type) {
  case STRING_RESULT:
    count_only_length();
    decimals= NOT_FIXED_DEC;
    agg_arg_charsets(collation, args, arg_count, MY_COLL_ALLOW_CONV, 1);
    break;

  case DECIMAL_RESULT:
    count_decimal_length();
    break;

  case REAL_RESULT:
    count_real_length();
    break;

  case INT_RESULT:
    count_only_length();
    decimals= 0;
    break;

  case ROW_RESULT:
    assert(0);
  }
}

/****************************************************************************
 Classes and function for the IN operator
****************************************************************************/

/*
  Determine which of the signed int64_t arguments is bigger

  SYNOPSIS
    cmp_longs()
      a_val     left argument
      b_val     right argument

  DESCRIPTION
    This function will compare two signed int64_t arguments
    and will return -1, 0, or 1 if left argument is smaller than,
    equal to or greater than the right argument.

  RETURN VALUE
    -1          left argument is smaller than the right argument.
    0           left argument is equal to the right argument.
    1           left argument is greater than the right argument.
*/
static inline int cmp_longs (int64_t a_val, int64_t b_val)
{
  return a_val < b_val ? -1 : a_val == b_val ? 0 : 1;
}


/*
  Determine which of the unsigned int64_t arguments is bigger

  SYNOPSIS
    cmp_ulongs()
      a_val     left argument
      b_val     right argument

  DESCRIPTION
    This function will compare two unsigned int64_t arguments
    and will return -1, 0, or 1 if left argument is smaller than,
    equal to or greater than the right argument.

  RETURN VALUE
    -1          left argument is smaller than the right argument.
    0           left argument is equal to the right argument.
    1           left argument is greater than the right argument.
*/
static inline int cmp_ulongs (uint64_t a_val, uint64_t b_val)
{
  return a_val < b_val ? -1 : a_val == b_val ? 0 : 1;
}


/*
  Compare two integers in IN value list format (packed_int64_t)

  SYNOPSIS
    cmp_int64_t()
      cmp_arg   an argument passed to the calling function (my_qsort2)
      a         left argument
      b         right argument

  DESCRIPTION
    This function will compare two integer arguments in the IN value list
    format and will return -1, 0, or 1 if left argument is smaller than,
    equal to or greater than the right argument.
    It's used in sorting the IN values list and finding an element in it.
    Depending on the signedness of the arguments cmp_int64_t() will
    compare them as either signed (using cmp_longs()) or unsigned (using
    cmp_ulongs()).

  RETURN VALUE
    -1          left argument is smaller than the right argument.
    0           left argument is equal to the right argument.
    1           left argument is greater than the right argument.
*/
int cmp_int64_t(void *, in_int64_t::packed_int64_t *a,
                in_int64_t::packed_int64_t *b)
{
  if (a->unsigned_flag != b->unsigned_flag)
  {
    /*
      One of the args is unsigned and is too big to fit into the
      positive signed range. Report no match.
    */
    if ((a->unsigned_flag && ((uint64_t) a->val) > (uint64_t) INT64_MAX) ||
        (b->unsigned_flag && ((uint64_t) b->val) > (uint64_t) INT64_MAX))
      return a->unsigned_flag ? 1 : -1;
    /*
      Although the signedness differs both args can fit into the signed
      positive range. Make them signed and compare as usual.
    */
    return cmp_longs (a->val, b->val);
  }
  if (a->unsigned_flag)
    return cmp_ulongs ((uint64_t) a->val, (uint64_t) b->val);
  else
    return cmp_longs (a->val, b->val);
}

static int cmp_double(void *, double *a, double *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

static int cmp_row(void *, cmp_item_row *a, cmp_item_row *b)
{
  return a->compare(b);
}


static int cmp_decimal(void *, type::Decimal *a, type::Decimal *b)
{
  /*
    We need call of fixing buffer pointer, because fast sort just copy
    decimal buffers in memory and pointers left pointing on old buffer place
  */
  a->fix_buffer_pointer();
  b->fix_buffer_pointer();
  return class_decimal_cmp(a, b);
}


void in_vector::sort()
{
  internal::my_qsort2(base,used_count,size,compare, (void *) collation);
}


int in_vector::find(Item *item)
{
  unsigned char *result=get_value(item);
  if (!result || !used_count)
    return 0;				// Null value

  uint32_t start,end;
  start=0; end=used_count-1;
  while (start != end)
  {
    uint32_t mid=(start+end+1)/2;
    int res;
    if ((res=(*compare)(collation, base+mid*size, result)) == 0)
      return 1;
    if (res < 0)
      start=mid;
    else
      end=mid-1;
  }
  return (int) ((*compare)(collation, base+start*size, result) == 0);
}

in_string::in_string(uint32_t elements,qsort2_cmp cmp_func, const charset_info_st * const cs)
  :in_vector(elements, sizeof(String), cmp_func, cs),
   tmp(buff, sizeof(buff), &my_charset_bin)
{}

in_string::~in_string()
{
  if (base)
  {
    // base was allocated with help of memory::sql_alloc => following is OK
    for (uint32_t i=0 ; i < count ; i++)
      ((String*) base)[i].free();
  }
}

void in_string::set(uint32_t pos,Item *item)
{
  String *str=((String*) base)+pos;
  String *res=item->val_str(str);
  if (res && res != str)
  {
    if (res->uses_buffer_owned_by(str))
      res->copy();
    if (item->type() == Item::FUNC_ITEM)
      str->copy(*res);
    else
      *str= *res;
  }
  if (!str->charset())
  {
    const charset_info_st *cs;
    if (!(cs= item->collation.collation))
      cs= &my_charset_bin;		// Should never happen for STR items
    str->set_charset(cs);
  }
}


unsigned char *in_string::get_value(Item *item)
{
  return (unsigned char*) item->val_str(&tmp);
}

in_row::in_row(uint32_t elements, Item *)
{
  base= (char*) new cmp_item_row[count= elements];
  size= sizeof(cmp_item_row);
  compare= (qsort2_cmp) cmp_row;
  /*
    We need to reset these as otherwise we will call sort() with
    uninitialized (even if not used) elements
  */
  used_count= elements;
  collation= 0;
}

in_row::~in_row()
{
  delete [] (cmp_item_row*) base;
}

unsigned char *in_row::get_value(Item *item)
{
  tmp.store_value(item);
  if (item->is_null())
    return 0;
  return (unsigned char *)&tmp;
}

void in_row::set(uint32_t pos, Item *item)
{
  ((cmp_item_row*) base)[pos].store_value_by_template(&tmp, item);
  return;
}

in_int64_t::in_int64_t(uint32_t elements) :
  in_vector(elements, sizeof(packed_int64_t),(qsort2_cmp) cmp_int64_t, 0)
{}

void in_int64_t::set(uint32_t pos,Item *item)
{
  struct packed_int64_t *buff= &((packed_int64_t*) base)[pos];

  buff->val= item->val_int();
  buff->unsigned_flag= item->unsigned_flag;
}

unsigned char *in_int64_t::get_value(Item *item)
{
  tmp.val= item->val_int();
  if (item->null_value)
    return 0;
  tmp.unsigned_flag= item->unsigned_flag;
  return (unsigned char*) &tmp;
}

in_datetime::in_datetime(Item *warn_item_arg, uint32_t elements) :
  in_int64_t(elements),
  session(current_session),
  warn_item(warn_item_arg),
  lval_cache(0)
{}

void in_datetime::set(uint32_t pos, Item *item)
{
  Item **tmp_item= &item;
  bool is_null;
  struct packed_int64_t *buff= &((packed_int64_t*) base)[pos];

  buff->val= get_datetime_value(session, &tmp_item, 0, warn_item, &is_null);
  buff->unsigned_flag= 1L;
}

unsigned char *in_datetime::get_value(Item *item)
{
  bool is_null;
  Item **tmp_item= lval_cache ? &lval_cache : &item;
  tmp.val= get_datetime_value(session, &tmp_item, &lval_cache, warn_item, &is_null);
  if (item->null_value)
    return 0;
  tmp.unsigned_flag= 1L;
  return (unsigned char*) &tmp;
}

in_double::in_double(uint32_t elements)
  :in_vector(elements,sizeof(double),(qsort2_cmp) cmp_double, 0)
{}

void in_double::set(uint32_t pos,Item *item)
{
  ((double*) base)[pos]= item->val_real();
}

unsigned char *in_double::get_value(Item *item)
{
  tmp= item->val_real();
  if (item->null_value)
    return 0;
  return (unsigned char*) &tmp;
}


in_decimal::in_decimal(uint32_t elements)
  :in_vector(elements, sizeof(type::Decimal),(qsort2_cmp) cmp_decimal, 0)
{}


void in_decimal::set(uint32_t pos, Item *item)
{
  /* as far as 'item' is constant, we can store reference on type::Decimal */
  type::Decimal *dec= ((type::Decimal *)base) + pos;
  dec->len= DECIMAL_BUFF_LENGTH;
  dec->fix_buffer_pointer();
  type::Decimal *res= item->val_decimal(dec);
  /* if item->val_decimal() is evaluated to NULL then res == 0 */
  if (!item->null_value && res != dec)
    class_decimal2decimal(res, dec);
}


unsigned char *in_decimal::get_value(Item *item)
{
  type::Decimal *result= item->val_decimal(&val);
  if (item->null_value)
    return 0;
  return (unsigned char *)result;
}


cmp_item* cmp_item::get_comparator(Item_result type,
                                   const charset_info_st * const cs)
{
  switch (type) {
  case STRING_RESULT:
    return new cmp_item_sort_string(cs);

  case INT_RESULT:
    return new cmp_item_int;

  case REAL_RESULT:
    return new cmp_item_real;

  case ROW_RESULT:
    return new cmp_item_row;

  case DECIMAL_RESULT:
    return new cmp_item_decimal;
  }

  return 0; // to satisfy compiler :)
}


cmp_item* cmp_item_sort_string::make_same()
{
  return new cmp_item_sort_string_in_static(cmp_charset);
}

cmp_item* cmp_item_int::make_same()
{
  return new cmp_item_int();
}

cmp_item* cmp_item_real::make_same()
{
  return new cmp_item_real();
}

cmp_item* cmp_item_row::make_same()
{
  return new cmp_item_row();
}


cmp_item_row::~cmp_item_row()
{
  if (comparators)
  {
    for (uint32_t i= 0; i < n; i++)
    {
      if (comparators[i])
	delete comparators[i];
    }
  }
  return;
}


void cmp_item_row::alloc_comparators()
{
  if (!comparators)
    comparators= (cmp_item **) current_session->mem.calloc(sizeof(cmp_item *)*n);
}


void cmp_item_row::store_value(Item *item)
{
  n= item->cols();
  alloc_comparators();
  if (comparators)
  {
    item->bring_value();
    item->null_value= 0;
    for (uint32_t i=0; i < n; i++)
    {
      if (!comparators[i])
        if (!(comparators[i]=
              cmp_item::get_comparator(item->element_index(i)->result_type(),
                                       item->element_index(i)->collation.collation)))
	  break;					// new failed
      comparators[i]->store_value(item->element_index(i));
      item->null_value|= item->element_index(i)->null_value;
    }
  }
  return;
}


void cmp_item_row::store_value_by_template(cmp_item *t, Item *item)
{
  cmp_item_row *tmpl= (cmp_item_row*) t;
  if (tmpl->n != item->cols())
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), tmpl->n);
    return;
  }
  n= tmpl->n;
  if ((comparators= (cmp_item **) memory::sql_alloc(sizeof(cmp_item *)*n)))
  {
    item->bring_value();
    item->null_value= 0;
    for (uint32_t i=0; i < n; i++)
    {
      if (!(comparators[i]= tmpl->comparators[i]->make_same()))
	break;					// new failed
      comparators[i]->store_value_by_template(tmpl->comparators[i],
					      item->element_index(i));
      item->null_value|= item->element_index(i)->null_value;
    }
  }
}


int cmp_item_row::cmp(Item *arg)
{
  arg->null_value= 0;
  if (arg->cols() != n)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), n);
    return 1;
  }
  bool was_null= 0;
  arg->bring_value();
  for (uint32_t i=0; i < n; i++)
  {
    if (comparators[i]->cmp(arg->element_index(i)))
    {
      if (!arg->element_index(i)->null_value)
	return 1;
      was_null= 1;
    }
  }
  return (arg->null_value= was_null);
}


int cmp_item_row::compare(cmp_item *c)
{
  cmp_item_row *l_cmp= (cmp_item_row *) c;
  for (uint32_t i=0; i < n; i++)
  {
    int res;
    if ((res= comparators[i]->compare(l_cmp->comparators[i])))
      return res;
  }
  return 0;
}


void cmp_item_decimal::store_value(Item *item)
{
  type::Decimal *val= item->val_decimal(&value);
  /* val may be zero if item is nnull */
  if (val && val != &value)
    class_decimal2decimal(val, &value);
}


int cmp_item_decimal::cmp(Item *arg)
{
  type::Decimal tmp_buf, *tmp= arg->val_decimal(&tmp_buf);
  if (arg->null_value)
    return 1;
  return class_decimal_cmp(&value, tmp);
}


int cmp_item_decimal::compare(cmp_item *arg)
{
  cmp_item_decimal *l_cmp= (cmp_item_decimal*) arg;
  return class_decimal_cmp(&value, &l_cmp->value);
}


cmp_item* cmp_item_decimal::make_same()
{
  return new cmp_item_decimal();
}


void cmp_item_datetime::store_value(Item *item)
{
  bool is_null;
  Item **tmp_item= lval_cache ? &lval_cache : &item;
  value= get_datetime_value(session, &tmp_item, &lval_cache, warn_item, &is_null);
}


int cmp_item_datetime::cmp(Item *arg)
{
  bool is_null;
  Item **tmp_item= &arg;
  return value !=
    get_datetime_value(session, &tmp_item, 0, warn_item, &is_null);
}


int cmp_item_datetime::compare(cmp_item *ci)
{
  cmp_item_datetime *l_cmp= (cmp_item_datetime *)ci;
  return (value < l_cmp->value) ? -1 : ((value == l_cmp->value) ? 0 : 1);
}


cmp_item *cmp_item_datetime::make_same()
{
  return new cmp_item_datetime(warn_item);
}


bool Item_func_in::nulls_in_row()
{
  Item **arg,**arg_end;
  for (arg= args+1, arg_end= args+arg_count; arg != arg_end ; arg++)
  {
    if ((*arg)->null_inside())
      return 1;
  }
  return 0;
}


/**
  Perform context analysis of an IN item tree.

    This function performs context analysis (name resolution) and calculates
    various attributes of the item tree with Item_func_in as its root.
    The function saves in ref the pointer to the item or to a newly created
    item that is considered as a replacement for the original one.

  @param session     reference to the global context of the query thread
  @param ref     pointer to Item* variable where pointer to resulting "fixed"
                 item is to be assigned

  @note
    Let T0(e)/T1(e) be the value of not_null_tables(e) when e is used on
    a predicate/function level. Then it's easy to show that:
    @verbatim
      T0(e IN(e1,...,en))     = union(T1(e),intersection(T1(ei)))
      T1(e IN(e1,...,en))     = union(T1(e),intersection(T1(ei)))
      T0(e NOT IN(e1,...,en)) = union(T1(e),union(T1(ei)))
      T1(e NOT IN(e1,...,en)) = union(T1(e),intersection(T1(ei)))
    @endverbatim

  @retval
    0   ok
  @retval
    1   got error
*/

bool
Item_func_in::fix_fields(Session *session, Item **ref)
{
  Item **arg, **arg_end;

  if (Item_func_opt_neg::fix_fields(session, ref))
    return 1;

  /* not_null_tables_cache == union(T1(e),union(T1(ei))) */
  if (pred_level && negated)
    return 0;

  /* not_null_tables_cache = union(T1(e),intersection(T1(ei))) */
  not_null_tables_cache= ~(table_map) 0;
  for (arg= args + 1, arg_end= args + arg_count; arg != arg_end; arg++)
    not_null_tables_cache&= (*arg)->not_null_tables();
  not_null_tables_cache|= (*args)->not_null_tables();
  return 0;
}


static int srtcmp_in(const charset_info_st * const cs, const String *x,const String *y)
{
  return cs->coll->strnncollsp(cs,
                               (unsigned char *) x->ptr(),x->length(),
                               (unsigned char *) y->ptr(),y->length(), 0);
}


void Item_func_in::fix_length_and_dec()
{
  Item **arg, **arg_end;
  bool const_itm= 1;
  bool datetime_found= false;
  /* true <=> arguments values will be compared as DATETIMEs. */
  bool compare_as_datetime= false;
  Item *date_arg= 0;
  uint32_t found_types= 0;
  uint32_t type_cnt= 0;
  Item_result cmp_type= STRING_RESULT;
  left_result_type= args[0]->result_type();
  if (!(found_types= collect_cmp_types(args, arg_count, true)))
    return;

  for (arg= args + 1, arg_end= args + arg_count; arg != arg_end ; arg++)
  {
    if (!arg[0]->const_item())
    {
      const_itm= 0;
      break;
    }
  }
  for (int i= STRING_RESULT; i <= DECIMAL_RESULT; i++)
  {
    if (found_types & 1 << i)
    {
      (type_cnt)++;
      cmp_type= (Item_result) i;
    }
  }

  if (type_cnt == 1)
  {
    if (cmp_type == STRING_RESULT &&
        agg_arg_charsets(cmp_collation, args, arg_count, MY_COLL_CMP_CONV, 1))
      return;
    arg_types_compatible= true;
  }
  if (type_cnt == 1)
  {
    /*
      When comparing rows create the row comparator object beforehand to ease
      the DATETIME comparison detection procedure.
    */
    if (cmp_type == ROW_RESULT)
    {
      cmp_item_row *cmp= 0;
      if (const_itm && !nulls_in_row())
      {
        array= new in_row(arg_count-1, 0);
        cmp= &((in_row*)array)->tmp;
      }
      else
      {
        cmp= new cmp_item_row;
        cmp_items[ROW_RESULT]= cmp;
      }
      cmp->n= args[0]->cols();
      cmp->alloc_comparators();
    }
    /* All DATE/DATETIME fields/functions has the STRING result type. */
    if (cmp_type == STRING_RESULT || cmp_type == ROW_RESULT)
    {
      uint32_t col, num_cols= args[0]->cols();

      for (col= 0; col < num_cols; col++)
      {
        bool skip_column= false;
        /*
          Check that all items to be compared has the STRING result type and at
          least one of them is a DATE/DATETIME item.
        */
        for (arg= args, arg_end= args + arg_count; arg != arg_end ; arg++)
        {
          Item *itm= ((cmp_type == STRING_RESULT) ? arg[0] :
                      arg[0]->element_index(col));
          if (itm->result_type() != STRING_RESULT)
          {
            skip_column= true;
            break;
          }
          else if (itm->is_datetime())
          {
            datetime_found= true;
            /*
              Internally all DATE/DATETIME values are converted to the DATETIME
              type. So try to find a DATETIME item to issue correct warnings.
            */
            if (!date_arg)
              date_arg= itm;
            else if (itm->field_type() == DRIZZLE_TYPE_DATETIME)
            {
              date_arg= itm;
              /* All arguments are already checked to have the STRING result. */
              if (cmp_type == STRING_RESULT)
                break;
            }
          }
        }
        if (skip_column)
          continue;
        if (datetime_found)
        {
          if (cmp_type == ROW_RESULT)
          {
            cmp_item **cmp= 0;
            if (array)
              cmp= ((in_row*)array)->tmp.comparators + col;
            else
              cmp= ((cmp_item_row*)cmp_items[ROW_RESULT])->comparators + col;
            *cmp= new cmp_item_datetime(date_arg);
            /* Reset variables for the next column. */
            date_arg= 0;
            datetime_found= false;
          }
          else
            compare_as_datetime= true;
        }
      }
    }
  }
  /*
    Row item with NULLs inside can return NULL or false =>
    they can't be processed as static
  */
  if (type_cnt == 1 && const_itm && !nulls_in_row())
  {
    if (compare_as_datetime)
    {
      array= new in_datetime(date_arg, arg_count - 1);
    }
    else
    {
      /*
        IN must compare INT columns and constants as int values (the same
        way as equality does).
        So we must check here if the column on the left and all the constant
        values on the right can be compared as integers and adjust the
        comparison type accordingly.
      */
      if (args[0]->real_item()->type() == FIELD_ITEM &&
          cmp_type != INT_RESULT)
      {
        Item_field *field_item= (Item_field*) (args[0]->real_item());
        if (field_item->field->can_be_compared_as_int64_t())
        {
          bool all_converted= true;
          for (arg=args+1, arg_end=args+arg_count; arg != arg_end ; arg++)
          {
            if (!convert_constant_item (&getSession(), field_item, &arg[0]))
              all_converted= false;
          }
          if (all_converted)
            cmp_type= INT_RESULT;
        }
      }

      switch (cmp_type) {
      case STRING_RESULT:
        array=new in_string(arg_count-1,(qsort2_cmp) srtcmp_in,
                            cmp_collation.collation);
        break;

      case INT_RESULT:
        array= new in_int64_t(arg_count-1);
        break;

      case REAL_RESULT:
        array= new in_double(arg_count-1);
        break;

      case ROW_RESULT:
        /*
          The row comparator was created at the beginning but only DATETIME
          items comparators were initialized. Call store_value() to setup
          others.
        */
        ((in_row*)array)->tmp.store_value(args[0]);
        break;

      case DECIMAL_RESULT:
        array= new in_decimal(arg_count - 1);
        break;
      }
    }

    if (array && !(getSession().is_fatal_error))		// If not EOM
    {
      uint32_t j=0;
      for (uint32_t arg_num=1 ; arg_num < arg_count ; arg_num++)
      {
        if (!args[arg_num]->null_value)			// Skip NULL values
        {
          array->set(j,args[arg_num]);
          j++;
        }
        else
          have_null= 1;
      }
      if ((array->used_count= j))
        array->sort();
    }
  }
  else
  {
    if (compare_as_datetime)
      cmp_items[STRING_RESULT]= new cmp_item_datetime(date_arg);
    else
    {
      for (int i= STRING_RESULT; i <= DECIMAL_RESULT; i++)
      {
        if (found_types & (1 << i) && !cmp_items[i])
        {
          if ((Item_result)i == STRING_RESULT &&
              agg_arg_charsets(cmp_collation, args, arg_count,
                               MY_COLL_CMP_CONV, 1))
            return;
          if (!cmp_items[i] && !(cmp_items[i]=
              cmp_item::get_comparator((Item_result)i,
                                       cmp_collation.collation)))
            return;
        }
      }
    }
  }
  max_length= 1;
}


void Item_func_in::print(String *str)
{
  str->append('(');
  args[0]->print(str);
  if (negated)
    str->append(STRING_WITH_LEN(" not"));
  str->append(STRING_WITH_LEN(" in ("));
  print_args(str, 1);
  str->append(STRING_WITH_LEN("))"));
}


/*
  Evaluate the function and return its value.

  SYNOPSIS
    val_int()

  DESCRIPTION
    Evaluate the function and return its value.

  IMPLEMENTATION
    If the array object is defined then the value of the function is
    calculated by means of this array.
    Otherwise several cmp_item objects are used in order to do correct
    comparison of left expression and an expression from the values list.
    One cmp_item object correspond to one used comparison type. Left
    expression can be evaluated up to number of different used comparison
    types. A bit mapped variable value_added_map is used to check whether
    the left expression already was evaluated for a particular result type.
    Result types are mapped to it according to their integer values i.e.
    STRING_RESULT is mapped to bit 0, REAL_RESULT to bit 1, so on.

  RETURN
    Value of the function
*/

int64_t Item_func_in::val_int()
{
  cmp_item *in_item;
  assert(fixed == 1);
  uint32_t value_added_map= 0;
  if (array)
  {
    int tmp=array->find(args[0]);
    null_value=args[0]->null_value || (!tmp && have_null);
    return (int64_t) (!null_value && tmp != negated);
  }

  for (uint32_t i= 1 ; i < arg_count ; i++)
  {
    Item_result cmp_type= item_cmp_type(left_result_type, args[i]->result_type());
    in_item= cmp_items[(uint32_t)cmp_type];
    assert(in_item);
    if (!(value_added_map & (1 << (uint32_t)cmp_type)))
    {
      in_item->store_value(args[0]);
      if ((null_value=args[0]->null_value))
        return 0;
      have_null= 0;
      value_added_map|= 1 << (uint32_t)cmp_type;
    }
    if (!in_item->cmp(args[i]) && !args[i]->null_value)
      return (int64_t) (!negated);
    have_null|= args[i]->null_value;
  }

  null_value= have_null;
  return (int64_t) (!null_value && negated);
}


Item_cond::Item_cond(Session *session, Item_cond *item)
  :item::function::Boolean(session, item),
   abort_on_null(item->abort_on_null),
   and_tables_cache(item->and_tables_cache)
{
  /*
    item->list will be copied by copy_andor_arguments() call
  */
}


void Item_cond::copy_andor_arguments(Session *session, Item_cond *item)
{
  List<Item>::iterator li(item->list.begin());
  while (Item *it= li++)
    list.push_back(it->copy_andor_structure(session));
}


bool
Item_cond::fix_fields(Session *session, Item **)
{
  assert(fixed == 0);
  List<Item>::iterator li(list.begin());
  Item *item;
  void *orig_session_marker= session->session_marker;
  unsigned char buff[sizeof(char*)];			// Max local vars in function
  not_null_tables_cache= used_tables_cache= 0;
  const_item_cache= true;

  if (functype() == COND_OR_FUNC)
    session->session_marker= 0;
  /*
    and_table_cache is the value that Item_cond_or() returns for
    not_null_tables()
  */
  and_tables_cache= ~(table_map) 0;

  if (check_stack_overrun(session, STACK_MIN_SIZE, buff))
    return true;				// Fatal error flag is set!
  /*
    The following optimization reduces the depth of an AND-OR tree.
    E.g. a WHERE clause like
      F1 AND (F2 AND (F2 AND F4))
    is parsed into a tree with the same nested structure as defined
    by braces. This optimization will transform such tree into
      AND (F1, F2, F3, F4).
    Trees of OR items are flattened as well:
      ((F1 OR F2) OR (F3 OR F4))   =>   OR (F1, F2, F3, F4)
    Items for removed AND/OR levels will dangle until the death of the
    entire statement.
    The optimization is currently prepared statements and stored procedures
    friendly as it doesn't allocate any memory and its effects are durable
    (i.e. do not depend on PS/SP arguments).
  */
  while ((item=li++))
  {
    table_map tmp_table_map;
    while (item->type() == Item::COND_ITEM &&
	   ((Item_cond*) item)->functype() == functype() &&
           !((Item_cond*) item)->list.is_empty())
    {						// Identical function
      li.replace(((Item_cond*) item)->list);
      ((Item_cond*) item)->list.clear();
      item= &*li;				// new current item
    }
    if (abort_on_null)
      item->top_level_item();

    // item can be substituted in fix_fields
    if ((!item->fixed &&
	 item->fix_fields(session, li.ref())) ||
	(item= &*li)->check_cols(1))
      return true;
    used_tables_cache|=     item->used_tables();
    if (item->const_item())
      and_tables_cache= (table_map) 0;
    else
    {
      tmp_table_map= item->not_null_tables();
      not_null_tables_cache|= tmp_table_map;
      and_tables_cache&= tmp_table_map;
      const_item_cache= false;
    }
    with_sum_func=	    with_sum_func || item->with_sum_func;
    with_subselect|=        item->with_subselect;
    if (item->maybe_null)
      maybe_null=1;
  }
  session->lex().current_select->cond_count+= list.size();
  session->session_marker= orig_session_marker;
  fix_length_and_dec();
  fixed= 1;
  return false;
}


void Item_cond::fix_after_pullout(Select_Lex *new_parent, Item **)
{
  List<Item>::iterator li(list.begin());
  Item *item;

  used_tables_cache=0;
  const_item_cache= true;

  and_tables_cache= ~(table_map) 0; // Here and below we do as fix_fields does
  not_null_tables_cache= 0;

  while ((item=li++))
  {
    table_map tmp_table_map;
    item->fix_after_pullout(new_parent, li.ref());
    item= &*li;
    used_tables_cache|= item->used_tables();
    const_item_cache&= item->const_item();

    if (item->const_item())
      and_tables_cache= (table_map) 0;
    else
    {
      tmp_table_map= item->not_null_tables();
      not_null_tables_cache|= tmp_table_map;
      and_tables_cache&= tmp_table_map;
      const_item_cache= false;
    }
  }
}


bool Item_cond::walk(Item_processor processor, bool walk_subquery, unsigned char *arg)
{
  List<Item>::iterator li(list.begin());
  Item *item;
  while ((item= li++))
    if (item->walk(processor, walk_subquery, arg))
      return 1;
  return Item_func::walk(processor, walk_subquery, arg);
}


/**
  Transform an Item_cond object with a transformer callback function.

    The function recursively applies the transform method to each
     member item of the condition list.
    If the call of the method for a member item returns a new item
    the old item is substituted for a new one.
    After this the transformer is applied to the root node
    of the Item_cond object.

  @param transformer   the transformer callback function to be applied to
                       the nodes of the tree of the object
  @param arg           parameter to be passed to the transformer

  @return
    Item returned as the result of transformation of the root node
*/

Item *Item_cond::transform(Item_transformer transformer, unsigned char *arg)
{
  List<Item>::iterator li(list.begin());
  Item *item;
  while ((item= li++))
  {
    Item *new_item= item->transform(transformer, arg);
    if (!new_item)
      return 0;
    *li.ref()= new_item;
  }
  return Item_func::transform(transformer, arg);
}


/**
  Compile Item_cond object with a processor and a transformer
  callback functions.

    First the function applies the analyzer to the root node of
    the Item_func object. Then if the analyzer succeeeds (returns true)
    the function recursively applies the compile method to member
    item of the condition list.
    If the call of the method for a member item returns a new item
    the old item is substituted for a new one.
    After this the transformer is applied to the root node
    of the Item_cond object.

  @param analyzer      the analyzer callback function to be applied to the
                       nodes of the tree of the object
  @param[in,out] arg_p parameter to be passed to the analyzer
  @param transformer   the transformer callback function to be applied to the
                       nodes of the tree of the object
  @param arg_t         parameter to be passed to the transformer

  @return
    Item returned as the result of transformation of the root node
*/

Item *Item_cond::compile(Item_analyzer analyzer, unsigned char **arg_p,
                         Item_transformer transformer, unsigned char *arg_t)
{
  if (!(this->*analyzer)(arg_p))
    return 0;

  List<Item>::iterator li(list.begin());
  Item *item;
  while ((item= li++))
  {
    /*
      The same parameter value of arg_p must be passed
      to analyze any argument of the condition formula.
    */
    unsigned char *arg_v= *arg_p;
    Item *new_item= item->compile(analyzer, &arg_v, transformer, arg_t);
    if (new_item && new_item != item)
      li.replace(new_item);
  }
  return Item_func::transform(transformer, arg_t);
}

void Item_cond::traverse_cond(Cond_traverser traverser,
                              void *arg, traverse_order order)
{
  List<Item>::iterator li(list.begin());
  Item *item;

  switch (order) {
  case (T_PREFIX):
    (*traverser)(this, arg);
    while ((item= li++))
    {
      item->traverse_cond(traverser, arg, order);
    }
    (*traverser)(NULL, arg);
    break;
  case (T_POSTFIX):
    while ((item= li++))
    {
      item->traverse_cond(traverser, arg, order);
    }
    (*traverser)(this, arg);
  }
}

/**
  Move SUM items out from item tree and replace with reference.

  The split is done to get an unique item for each SUM function
  so that we can easily find and calculate them.
  (Calculation done by update_sum_func() and copy_sum_funcs() in
  sql_select.cc)

  @param session			Thread handler
  @param ref_pointer_array	Pointer to array of reference fields
  @param fields		All fields in select

  @note
    This function is run on all expression (SELECT list, WHERE, HAVING etc)
    that have or refer (HAVING) to a SUM expression.
*/

void Item_cond::split_sum_func(Session *session, Item **ref_pointer_array, List<Item> &fields)
{
  List<Item>::iterator li(list.begin());
  Item *item;
  while ((item= li++))
    item->split_sum_func(session, ref_pointer_array, fields, li.ref(), true);
}


table_map
Item_cond::used_tables() const
{						// This caches used_tables
  return used_tables_cache;
}


void Item_cond::update_used_tables()
{
  List<Item>::iterator li(list.begin());
  Item *item;

  used_tables_cache=0;
  const_item_cache= true;
  while ((item=li++))
  {
    item->update_used_tables();
    used_tables_cache|= item->used_tables();
    const_item_cache&= item->const_item();
  }
}


void Item_cond::print(String *str)
{
  str->append('(');
  List<Item>::iterator li(list.begin());
  Item *item;
  if ((item=li++))
    item->print(str);
  while ((item=li++))
  {
    str->append(' ');
    str->append(func_name());
    str->append(' ');
    item->print(str);
  }
  str->append(')');
}


void Item_cond::neg_arguments(Session *session)
{
  List<Item>::iterator li(list.begin());
  Item *item;
  while ((item= li++))		/* Apply not transformation to the arguments */
  {
    Item *new_item= item->neg_transformer(session);
    if (!new_item)
      new_item= new Item_func_not(item);
    li.replace(new_item);
  }
}


/**
  Evaluation of AND(expr, expr, expr ...).

  @note
    abort_if_null is set for AND expressions for which we don't care if the
    result is NULL or 0. This is set for:
    - WHERE clause
    - HAVING clause
    - IF(expression)

  @retval
    1  If all expressions are true
  @retval
    0  If all expressions are false or if we find a NULL expression and
       'abort_on_null' is set.
  @retval
    NULL if all expression are either 1 or NULL
*/


int64_t Item_cond_and::val_int()
{
  assert(fixed == 1);
  List<Item>::iterator li(list.begin());
  Item *item;
  null_value= 0;
  while ((item=li++))
  {
    if (!item->val_bool())
    {
      if (abort_on_null || !(null_value= item->null_value))
	return 0;				// return false
    }
  }
  return null_value ? 0 : 1;
}


int64_t Item_cond_or::val_int()
{
  assert(fixed == 1);
  List<Item>::iterator li(list.begin());
  Item *item;
  null_value=0;
  while ((item=li++))
  {
    if (item->val_bool())
    {
      null_value=0;
      return 1;
    }
    if (item->null_value)
      null_value=1;
  }
  return 0;
}

/**
  Create an AND expression from two expressions.

  @param a	expression or NULL
  @param b    	expression.
  @param org_item	Don't modify a if a == *org_item.
                        If a == NULL, org_item is set to point at b,
                        to ensure that future calls will not modify b.

  @note
    This will not modify item pointed to by org_item or b
    The idea is that one can call this in a loop and create and
    'and' over all items without modifying any of the original items.

  @retval
    NULL	Error
  @retval
    Item
*/

Item *and_expressions(Item *a, Item *b, Item **org_item)
{
  if (!a)
    return (*org_item= (Item*) b);
  if (a == *org_item)
  {
    Item_cond *res;
    res= new Item_cond_and(a, (Item*) b);
    res->used_tables_cache= a->used_tables() | b->used_tables();
    res->not_null_tables_cache= a->not_null_tables() | b->not_null_tables();
    return res;
  }
  ((Item_cond_and*) a)->add((Item*) b);
  ((Item_cond_and*) a)->used_tables_cache|= b->used_tables();
  ((Item_cond_and*) a)->not_null_tables_cache|= b->not_null_tables();
  return a;
}


int64_t Item_func_isnull::val_int()
{
  assert(fixed == 1);
  /*
    Handle optimization if the argument can't be null
    This has to be here because of the test in update_used_tables().
  */
  if (!used_tables_cache && !with_subselect)
    return cached_value;
  return args[0]->is_null() ? 1: 0;
}

int64_t Item_is_not_null_test::val_int()
{
  assert(fixed == 1);
  if (!used_tables_cache && !with_subselect)
  {
    owner->was_null|= (!cached_value);
    return(cached_value);
  }
  if (args[0]->is_null())
  {
    owner->was_null|= 1;
    return 0;
  }
  else
    return 1;
}

/**
  Optimize case of not_null_column IS NULL.
*/
void Item_is_not_null_test::update_used_tables()
{
  if (!args[0]->maybe_null)
  {
    used_tables_cache= 0;			/* is always true */
    cached_value= (int64_t) 1;
  }
  else
  {
    args[0]->update_used_tables();
    if (!(used_tables_cache=args[0]->used_tables()) && !with_subselect)
    {
      /* Remember if the value is always NULL or never NULL */
      cached_value= (int64_t) !args[0]->is_null();
    }
  }
}


int64_t Item_func_isnotnull::val_int()
{
  assert(fixed == 1);
  return args[0]->is_null() ? 0 : 1;
}


void Item_func_isnotnull::print(String *str)
{
  str->append('(');
  args[0]->print(str);
  str->append(STRING_WITH_LEN(" is not null)"));
}


int64_t Item_func_like::val_int()
{
  assert(fixed == 1);
  String* res = args[0]->val_str(&tmp_value1);
  if (args[0]->null_value)
  {
    null_value=1;
    return 0;
  }
  String* res2 = args[1]->val_str(&tmp_value2);
  if (args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (canDoTurboBM)
    return turboBM_matches(res->ptr(), res->length()) ? 1 : 0;
  return my_wildcmp(cmp.cmp_collation.collation,
	 	    res->ptr(),res->ptr()+res->length(),
		    res2->ptr(),res2->ptr()+res2->length(),
		    make_escape_code(cmp.cmp_collation.collation, escape),
                    internal::wild_one,internal::wild_many) ? 0 : 1;
}


/**
  We can optimize a where if first character isn't a wildcard
*/

Item_func::optimize_type Item_func_like::select_optimize() const
{
  if (args[1]->const_item())
  {
    String* res2= args[1]->val_str((String *)&tmp_value2);

    if (!res2)
      return OPTIMIZE_NONE;

    if (*res2->ptr() != internal::wild_many)
    {
      if (args[0]->result_type() != STRING_RESULT || *res2->ptr() != internal::wild_one)
	return OPTIMIZE_OP;
    }
  }
  return OPTIMIZE_NONE;
}


bool Item_func_like::fix_fields(Session *session, Item **ref)
{
  assert(fixed == 0);
  if (Item_bool_func2::fix_fields(session, ref) ||
      escape_item->fix_fields(session, &escape_item))
    return true;

  if (!escape_item->const_during_execution())
  {
    my_error(ER_WRONG_ARGUMENTS,MYF(0),"ESCAPE");
    return true;
  }

  if (escape_item->const_item())
  {
    
    /* If we are on execution stage */
    String *escape_str= escape_item->val_str(&tmp_value1);
    if (escape_str)
    {
      escape= (char *)memory::sql_alloc(escape_str->length());
      strcpy(escape, escape_str->ptr()); 
    }
    else
    {
      escape= (char *)memory::sql_alloc(1);
      strcpy(escape, "\\");
    } 
   
    /*
      We could also do boyer-more for non-const items, but as we would have to
      recompute the tables for each row it's not worth it.
    */
    if (args[1]->const_item() && !use_strnxfrm(collation.collation))
    {
      String* res2 = args[1]->val_str(&tmp_value2);
      if (!res2)
        return false;				// Null argument

      const size_t len   = res2->length();
      const char*  first = res2->ptr();
      const char*  last  = first + len - 1;
      /*
        len must be > 2 ('%pattern%')
        heuristic: only do TurboBM for pattern_len > 2
      */

      if (len > MIN_TURBOBM_PATTERN_LEN + 2 &&
          *first == internal::wild_many &&
          *last  == internal::wild_many)
      {
        const char* tmp = first + 1;
        for (; *tmp != internal::wild_many && *tmp != internal::wild_one; tmp++)
        {
          if (escape == tmp)
            break;
        }
  
        canDoTurboBM = (tmp == last) && !use_mb(args[0]->collation.collation);
      }
      if (canDoTurboBM)
      {
        pattern     = first + 1;
        pattern_len = (int) len - 2;
        int *suff = (int*) session->mem.alloc(sizeof(int) * ((pattern_len + 1)*2+ alphabet_size));
        bmGs      = suff + pattern_len + 1;
        bmBc      = bmGs + pattern_len + 1;
        turboBM_compute_good_suffix_shifts(suff);
        turboBM_compute_bad_character_shifts();
      }
    }
  }
  return false;
}

void Item_func_like::cleanup()
{
  canDoTurboBM= false;
  Item_bool_func2::cleanup();
}

static unsigned char likeconv(const charset_info_st *cs, unsigned char a)
{
#ifdef LIKE_CMP_TOUPPER
  return cs->toupper(a);
#else
  return cs->sort_order[a];
#endif
}

/**
  Precomputation dependent only on pattern_len.
*/

void Item_func_like::turboBM_compute_suffixes(int *suff)
{
  const int   plm1 = pattern_len - 1;
  int            f = 0;
  int            g = plm1;
  int *const splm1 = suff + plm1;
  const charset_info_st * const cs= cmp.cmp_collation.collation;

  *splm1 = pattern_len;

  if (!cs->sort_order)
  {
    for (int i = pattern_len - 2; i >= 0; i--)
    {
      int tmp = *(splm1 + i - f);
      if (g < i && tmp < i - g)
        suff[i] = tmp;
      else
      {
        if (i < g)
          g = i;
        f = i;
        while (g >= 0 && pattern[g] == pattern[g + plm1 - f])
          g--;
        suff[i] = f - g;
      }
    }
  }
  else
  {
    for (int i = pattern_len - 2; 0 <= i; --i)
    {
      int tmp = *(splm1 + i - f);
      if (g < i && tmp < i - g)
        suff[i] = tmp;
      else
      {
        if (i < g)
          g = i;
        f = i;
        while (g >= 0 &&
               likeconv(cs, pattern[g]) == likeconv(cs, pattern[g + plm1 - f]))
          g--;
        suff[i] = f - g;
      }
    }
  }
}


/**
  Precomputation dependent only on pattern_len.
*/

void Item_func_like::turboBM_compute_good_suffix_shifts(int *suff)
{
  turboBM_compute_suffixes(suff);

  int *end = bmGs + pattern_len;
  int *k;
  for (k = bmGs; k < end; k++)
    *k = pattern_len;

  int tmp;
  int i;
  int j          = 0;
  const int plm1 = pattern_len - 1;
  for (i = plm1; i > -1; i--)
  {
    if (suff[i] == i + 1)
    {
      for (tmp = plm1 - i; j < tmp; j++)
      {
	int *tmp2 = bmGs + j;
	if (*tmp2 == pattern_len)
	  *tmp2 = tmp;
      }
    }
  }

  int *tmp2;
  for (tmp = plm1 - i; j < tmp; j++)
  {
    tmp2 = bmGs + j;
    if (*tmp2 == pattern_len)
      *tmp2 = tmp;
  }

  tmp2 = bmGs + plm1;
  for (i = 0; i <= pattern_len - 2; i++)
    *(tmp2 - suff[i]) = plm1 - i;
}


/**
   Precomputation dependent on pattern_len.
*/

void Item_func_like::turboBM_compute_bad_character_shifts()
{
  int *i;
  int *end = bmBc + alphabet_size;
  int j;
  const int plm1 = pattern_len - 1;
  const charset_info_st *const cs= cmp.cmp_collation.collation;

  for (i = bmBc; i < end; i++)
    *i = pattern_len;

  if (!cs->sort_order)
  {
    for (j = 0; j < plm1; j++)
      bmBc[(uint32_t) (unsigned char) pattern[j]] = plm1 - j;
  }
  else
  {
    for (j = 0; j < plm1; j++)
      bmBc[(uint32_t) likeconv(cs,pattern[j])] = plm1 - j;
  }
}


/**
  Search for pattern in text.

  @return
    returns true/false for match/no match
*/

bool Item_func_like::turboBM_matches(const char* text, int text_len) const
{
  int bcShift;
  int turboShift;
  int shift = pattern_len;
  int j     = 0;
  int u     = 0;
  const charset_info_st * const cs= cmp.cmp_collation.collation;

  const int plm1=  pattern_len - 1;
  const int tlmpl= text_len - pattern_len;

  /* Searching */
  if (!cs->sort_order)
  {
    while (j <= tlmpl)
    {
      int i= plm1;
      while (i >= 0 && pattern[i] == text[i + j])
      {
	i--;
	if (i == plm1 - shift)
	  i-= u;
      }
      if (i < 0)
	return 1;

      const int v = plm1 - i;
      turboShift = u - v;
      bcShift    = bmBc[(uint32_t) (unsigned char) text[i + j]] - plm1 + i;
      shift      = (turboShift > bcShift) ? turboShift : bcShift;
      shift      = (shift > bmGs[i]) ? shift : bmGs[i];
      if (shift == bmGs[i])
	u = (pattern_len - shift < v) ? pattern_len - shift : v;
      else
      {
        if (turboShift < bcShift)
          shift= max(shift, u + 1);
        u = 0;
      }
      j+= shift;
    }
    return 0;
  }
  else
  {
    while (j <= tlmpl)
    {
      int i = plm1;
      while (i >= 0 && likeconv(cs,pattern[i]) == likeconv(cs,text[i + j]))
      {
        i--;
        if (i == plm1 - shift)
          i-= u;
      }

      if (i < 0)
        return 1;

      const int v= plm1 - i;
      turboShift= u - v;
      bcShift= bmBc[(uint32_t) likeconv(cs, text[i + j])] - plm1 + i;
      shift= (turboShift > bcShift) ? turboShift : bcShift;
      shift= max(shift, bmGs[i]);
      
      if (shift == bmGs[i])
        u= (pattern_len - shift < v) ? pattern_len - shift : v;
      else
      {
        if (turboShift < bcShift)
          shift= max(shift, u + 1);
        u = 0;
      }

      j+= shift;
    }
    return 0;
  }
}


/**
  Make a logical XOR of the arguments.

  If either operator is NULL, return NULL.

  @todo
    (low priority) Change this to be optimized as: @n
    A XOR B   ->  (A) == 1 AND (B) <> 1) OR (A <> 1 AND (B) == 1) @n
    To be able to do this, we would however first have to extend the MySQL
    range optimizer to handle OR better.

  @note
    As we don't do any index optimization on XOR this is not going to be
    very fast to use.
*/

int64_t Item_cond_xor::val_int()
{
  assert(fixed == 1);
  List<Item>::iterator li(list.begin());
  Item *item;
  int result=0;
  null_value=0;
  while ((item=li++))
  {
    result^= (item->val_int() != 0);
    if (item->null_value)
    {
      null_value=1;
      return 0;
    }
  }
  return (int64_t) result;
}

/**
  Apply NOT transformation to the item and return a new one.


    Transform the item using next rules:
    @verbatim
       a AND b AND ...    -> NOT(a) OR NOT(b) OR ...
       a OR b OR ...      -> NOT(a) AND NOT(b) AND ...
       NOT(a)             -> a
       a = b              -> a != b
       a != b             -> a = b
       a < b              -> a >= b
       a >= b             -> a < b
       a > b              -> a <= b
       a <= b             -> a > b
       IS NULL(a)         -> IS NOT NULL(a)
       IS NOT NULL(a)     -> IS NULL(a)
    @endverbatim

  @param session		thread handler

  @return
    New item or
    NULL if we cannot apply NOT transformation (see Item::neg_transformer()).
*/

Item *Item_func_not::neg_transformer(Session *)	/* NOT(x)  ->  x */
{
  return args[0];
}


Item *Item_bool_rowready_func2::neg_transformer(Session *)
{
  Item *item= negated_item();
  return item;
}


/**
  a IS NULL  ->  a IS NOT NULL.
*/
Item *Item_func_isnull::neg_transformer(Session *)
{
  Item *item= new Item_func_isnotnull(args[0]);
  return item;
}


/**
  a IS NOT NULL  ->  a IS NULL.
*/
Item *Item_func_isnotnull::neg_transformer(Session *)
{
  Item *item= new Item_func_isnull(args[0]);
  return item;
}


Item *Item_cond_and::neg_transformer(Session *session)	/* NOT(a AND b AND ...)  -> */
					/* NOT a OR NOT b OR ... */
{
  neg_arguments(session);
  Item *item= new Item_cond_or(list);
  return item;
}


Item *Item_cond_or::neg_transformer(Session *session)	/* NOT(a OR b OR ...)  -> */
					/* NOT a AND NOT b AND ... */
{
  neg_arguments(session);
  Item *item= new Item_cond_and(list);
  return item;
}


Item *Item_func_nop_all::neg_transformer(Session *)
{
  /* "NOT (e $cmp$ ANY (SELECT ...)) -> e $rev_cmp$" ALL (SELECT ...) */
  Item_func_not_all *new_item= new Item_func_not_all(args[0]);
  Item_allany_subselect *allany= (Item_allany_subselect*)args[0];
  allany->func= allany->func_creator(false);
  allany->all= !allany->all;
  allany->upper_item= new_item;
  return new_item;
}

Item *Item_func_not_all::neg_transformer(Session *)
{
  /* "NOT (e $cmp$ ALL (SELECT ...)) -> e $rev_cmp$" ANY (SELECT ...) */
  Item_func_nop_all *new_item= new Item_func_nop_all(args[0]);
  Item_allany_subselect *allany= (Item_allany_subselect*)args[0];
  allany->all= !allany->all;
  allany->func= allany->func_creator(true);
  allany->upper_item= new_item;
  return new_item;
}

Item *Item_func_eq::negated_item()		/* a = b  ->  a != b */
{
  return new Item_func_ne(args[0], args[1]);
}


Item *Item_func_ne::negated_item()		/* a != b  ->  a = b */
{
  return new Item_func_eq(args[0], args[1]);
}


Item *Item_func_lt::negated_item()		/* a < b  ->  a >= b */
{
  return new Item_func_ge(args[0], args[1]);
}


Item *Item_func_ge::negated_item()		/* a >= b  ->  a < b */
{
  return new Item_func_lt(args[0], args[1]);
}


Item *Item_func_gt::negated_item()		/* a > b  ->  a <= b */
{
  return new Item_func_le(args[0], args[1]);
}


Item *Item_func_le::negated_item()		/* a <= b  ->  a > b */
{
  return new Item_func_gt(args[0], args[1]);
}

/**
  just fake method, should never be called.
*/
Item *Item_bool_rowready_func2::negated_item()
{
  assert(0);
  return 0;
}

Item_equal::Item_equal(Item_field *f1, Item_field *f2)
  : item::function::Boolean(), const_item(0), eval_item(0), cond_false(0)
{
  const_item_cache= false;
  fields.push_back(f1);
  fields.push_back(f2);
}

Item_equal::Item_equal(Item *c, Item_field *f)
  : item::function::Boolean(), eval_item(0), cond_false(0)
{
  const_item_cache= false;
  fields.push_back(f);
  const_item= c;
}


Item_equal::Item_equal(Item_equal *item_equal)
  : item::function::Boolean(), eval_item(0), cond_false(0)
{
  const_item_cache= false;
  List<Item_field>::iterator li(item_equal->fields.begin());
  Item_field *item;
  while ((item= li++))
  {
    fields.push_back(item);
  }
  const_item= item_equal->const_item;
  cond_false= item_equal->cond_false;
}

void Item_equal::add(Item *c)
{
  if (cond_false)
    return;
  if (!const_item)
  {
    const_item= c;
    return;
  }
  Item_func_eq *func= new Item_func_eq(c, const_item);
  func->set_cmp_func();
  func->quick_fix_field();
  if ((cond_false= !func->val_int()))
    const_item_cache= true;
}

void Item_equal::add(Item_field *f)
{
  fields.push_back(f);
}

uint32_t Item_equal::members()
{
  return fields.size();
}


/**
  Check whether a field is referred in the multiple equality.

  The function checks whether field is occurred in the Item_equal object .

  @param field   field whose occurrence is to be checked

  @retval
    1       if nultiple equality contains a reference to field
  @retval
    0       otherwise
*/

bool Item_equal::contains(Field *field)
{
  List<Item_field>::iterator it(fields.begin());
  Item_field *item;
  while ((item= it++))
  {
    if (field->eq(item->field))
        return 1;
  }
  return 0;
}


/**
  Join members of another Item_equal object.

    The function actually merges two multiple equalities.
    After this operation the Item_equal object additionally contains
    the field items of another item of the type Item_equal.
    If the optional constant items are not equal the cond_false flag is
    set to 1.
  @param item    multiple equality whose members are to be joined
*/

void Item_equal::merge(Item_equal *item)
{
  fields.concat(&item->fields);
  Item *c= item->const_item;
  if (c)
  {
    /*
      The flag cond_false will be set to 1 after this, if
      the multiple equality already contains a constant and its
      value is  not equal to the value of c.
    */
    add(c);
  }
  cond_false|= item->cond_false;
}


/**
  Order field items in multiple equality according to a sorting criteria.

  The function perform ordering of the field items in the Item_equal
  object according to the criteria determined by the cmp callback parameter.
  If cmp(item_field1,item_field2,arg)<0 than item_field1 must be
  placed after item_fiel2.

  The function sorts field items by the exchange sort algorithm.
  The list of field items is looked through and whenever two neighboring
  members follow in a wrong order they are swapped. This is performed
  again and again until we get all members in a right order.

  @param cmp          function to compare field item
  @param arg          context extra parameter for the cmp function
*/

void Item_equal::sort(Item_field_cmpfunc cmp, void *arg)
{
  bool swap;
  List<Item_field>::iterator it(fields.begin());
  do
  {
    Item_field *item1= it++;
    Item_field **ref1= it.ref();
    Item_field *item2;

    swap= false;
    while ((item2= it++))
    {
      Item_field **ref2= it.ref();
      if (cmp(item1, item2, arg) < 0)
      {
        Item_field *item= *ref1;
        *ref1= *ref2;
        *ref2= item;
        swap= true;
      }
      else
      {
        item1= item2;
        ref1= ref2;
      }
    }
    it= fields.begin();
  } while (swap);
}


/**
  Check appearance of new constant items in the multiple equality object.

  The function checks appearance of new constant items among
  the members of multiple equalities. Each new constant item is
  compared with the designated constant item if there is any in the
  multiple equality. If there is none the first new constant item
  becomes designated.
*/

void Item_equal::update_const()
{
  List<Item_field>::iterator it(fields.begin());
  Item *item;
  while ((item= it++))
  {
    if (item->const_item())
    {
      it.remove();
      add(item);
    }
  }
}

bool Item_equal::fix_fields(Session *, Item **)
{
  List<Item_field>::iterator li(fields.begin());
  Item *item;
  not_null_tables_cache= used_tables_cache= 0;
  const_item_cache= false;
  while ((item= li++))
  {
    table_map tmp_table_map;
    used_tables_cache|= item->used_tables();
    tmp_table_map= item->not_null_tables();
    not_null_tables_cache|= tmp_table_map;
    if (item->maybe_null)
      maybe_null=1;
  }
  fix_length_and_dec();
  fixed= 1;
  return 0;
}

void Item_equal::update_used_tables()
{
  List<Item_field>::iterator li(fields.begin());
  Item *item;
  not_null_tables_cache= used_tables_cache= 0;
  if ((const_item_cache= cond_false))
    return;
  while ((item=li++))
  {
    item->update_used_tables();
    used_tables_cache|= item->used_tables();
    const_item_cache&= item->const_item();
  }
}

int64_t Item_equal::val_int()
{
  Item_field *item_field;
  if (cond_false)
    return 0;
  List<Item_field>::iterator it(fields.begin());
  Item *item= const_item ? const_item : it++;
  eval_item->store_value(item);
  if ((null_value= item->null_value))
    return 0;
  while ((item_field= it++))
  {
    /* Skip fields of non-const tables. They haven't been read yet */
    if (item_field->field->getTable()->const_table)
    {
      if (eval_item->cmp(item_field) || (null_value= item_field->null_value))
        return 0;
    }
  }
  return 1;
}

void Item_equal::fix_length_and_dec()
{
  Item *item= get_first();
  eval_item= cmp_item::get_comparator(item->result_type(),
                                      item->collation.collation);
}

bool Item_equal::walk(Item_processor processor, bool walk_subquery, unsigned char *arg)
{
  List<Item_field>::iterator it(fields.begin());
  Item *item;
  while ((item= it++))
  {
    if (item->walk(processor, walk_subquery, arg))
      return 1;
  }
  return Item_func::walk(processor, walk_subquery, arg);
}

Item *Item_equal::transform(Item_transformer transformer, unsigned char *arg)
{
  List<Item_field>::iterator it(fields.begin());
  Item *item;
  while ((item= it++))
  {
    Item *new_item= item->transform(transformer, arg);
    if (!new_item)
      return 0;
    *(Item **)it.ref()= new_item;
  }
  return Item_func::transform(transformer, arg);
}

void Item_equal::print(String *str)
{
  str->append(func_name());
  str->append('(');
  List<Item_field>::iterator it(fields.begin());
  Item *item;
  if (const_item)
    const_item->print(str);
  else
  {
    item= it++;
    item->print(str);
  }
  while ((item= it++))
  {
    str->append(',');
    str->append(' ');
    item->print(str);
  }
  str->append(')');
}

cmp_item_datetime::cmp_item_datetime(Item *warn_item_arg) :
  session(current_session),
  warn_item(warn_item_arg),
  lval_cache(0)
{}

} /* namespace drizzled */
