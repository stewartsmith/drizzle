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

#include <drizzled/function/set_user_var.h>
#include <drizzled/field/num.h>
#include <drizzled/session.h>
#include <drizzled/plugin/client.h>
#include <drizzled/user_var_entry.h>
#include <drizzled/table.h>

namespace drizzled {

/*
  When a user variable is updated (in a SET command or a query like
  SELECT @a:= ).
*/

bool Item_func_set_user_var::fix_fields(Session *session, Item **ref)
{
  assert(fixed == 0);
  /* fix_fields will call Item_func_set_user_var::fix_length_and_dec */
  if (Item_func::fix_fields(session, ref) ||
      !(entry= session->getVariable(name, true)))
    return true;
  /*
     Remember the last query which updated it, this way a query can later know
     if this variable is a constant item in the query (it is if update_query_id
     is different from query_id).
  */
  entry->update_query_id= session->getQueryId();
  /*
    As it is wrong and confusing to associate any
    character set with NULL, @a should be latin2
    after this query sequence:

      SET @a=_latin2'string';
      SET @a=NULL;

    I.e. the second query should not change the charset
    to the current default value, but should keep the
    original value assigned during the first query.
    In order to do it, we don't copy charset
    from the argument if the argument is NULL
    and the variable has previously been initialized.
  */
  null_item= (args[0]->type() == NULL_ITEM);
  if (!entry->collation.collation || !null_item)
    entry->collation.set(args[0]->collation.collation, DERIVATION_IMPLICIT);
  collation.set(entry->collation.collation, DERIVATION_IMPLICIT);
  cached_result_type= args[0]->result_type();
  return false;
}

void
Item_func_set_user_var::fix_length_and_dec()
{
  maybe_null=args[0]->maybe_null;
  max_length=args[0]->max_length;
  decimals=args[0]->decimals;
  collation.set(args[0]->collation.collation, DERIVATION_IMPLICIT);
}

/*
  Mark field in read_map

  NOTES
    This is used by filesort to register used fields in a a temporary
    column read set or to register used fields in a view
*/

bool Item_func_set_user_var::register_field_in_read_map(unsigned char *arg)
{
  if (result_field)
  {
    Table *table= (Table *) arg;
    if (result_field->getTable() == table || !table)
      result_field->getTable()->setReadSet(result_field->position());
  }
  return 0;
}


void
Item_func_set_user_var::update_hash(void *ptr, uint32_t length,
                                    Item_result res_type,
                                    const charset_info_st * const cs, Derivation dv,
                                    bool unsigned_arg)
{
  /*
    If we set a variable explicitely to NULL then keep the old
    result type of the variable
  */
  if ((null_value= args[0]->null_value) && null_item)
    res_type= entry->type;                      // Don't change type of item
  entry->update_hash((null_value= args[0]->null_value), ptr, length, res_type, cs, dv, unsigned_arg);
}

/**
  This functions is invoked on SET \@variable or
  \@variable:= expression.

  Evaluate (and check expression), store results.

  @note
    For now it always return OK. All problem with value evaluating
    will be caught by session->is_error() check in sql_set_variables().

  @retval
    false OK.
*/

bool
Item_func_set_user_var::check(bool use_result_field)
{
  if (use_result_field && !result_field)
    use_result_field= false;

  switch (cached_result_type) {
  case REAL_RESULT:
    {
      save_result.vreal= use_result_field ? result_field->val_real() :
        args[0]->val_real();
      break;
    }
  case INT_RESULT:
    {
      save_result.vint= use_result_field ? result_field->val_int() :
        args[0]->val_int();
      unsigned_flag= use_result_field ? ((Field_num*)result_field)->unsigned_flag:
        args[0]->unsigned_flag;
      break;
    }
  case STRING_RESULT:
    {
      save_result.vstr= use_result_field ? result_field->val_str_internal(&value) :
        args[0]->val_str(&value);
      break;
    }
  case DECIMAL_RESULT:
    {
      save_result.vdec= use_result_field ?
        result_field->val_decimal(&decimal_buff) :
        args[0]->val_decimal(&decimal_buff);
      break;
    }
  case ROW_RESULT:
    // This case should never be chosen
    assert(0);
    break;
  }

  return false;
}

/**
  This functions is invoked on
  SET \@variable or \@variable:= expression.

  @note
    We have to store the expression as such in the variable, independent of
    the value method used by the user

  @retval
    0   OK
  @retval
    1   EOM Error

*/

void
Item_func_set_user_var::update()
{
  switch (cached_result_type) {
  case REAL_RESULT:
    {
      update_hash((void*) &save_result.vreal,sizeof(save_result.vreal),
                       REAL_RESULT, &my_charset_bin, DERIVATION_IMPLICIT, 0);
      break;
    }

  case INT_RESULT:
    {
      update_hash((void*) &save_result.vint, sizeof(save_result.vint),
                       INT_RESULT, &my_charset_bin, DERIVATION_IMPLICIT,
                       unsigned_flag);
      break;
    }

  case STRING_RESULT:
    {
      if (!save_result.vstr)                                      // Null value
        update_hash((void*) 0, 0, STRING_RESULT, &my_charset_bin,
                         DERIVATION_IMPLICIT, 0);
      else
        update_hash((void*) save_result.vstr->ptr(),
                         save_result.vstr->length(), STRING_RESULT,
                         save_result.vstr->charset(),
                         DERIVATION_IMPLICIT, 0);
      break;
    }

  case DECIMAL_RESULT:
    {
      if (!save_result.vdec)                                      // Null value
        update_hash((void*) 0, 0, DECIMAL_RESULT, &my_charset_bin,
                         DERIVATION_IMPLICIT, 0);
      else
        update_hash((void*) save_result.vdec,
                         sizeof(type::Decimal), DECIMAL_RESULT,
                         &my_charset_bin, DERIVATION_IMPLICIT, 0);
      break;
    }

  case ROW_RESULT:
    // This case should never be chosen
    assert(0);
    break;
  }
}

double Item_func_set_user_var::val_real()
{
  assert(fixed == 1);
  check(0);
  update();                                     // Store expression
  return entry->val_real(&null_value);
}

int64_t Item_func_set_user_var::val_int()
{
  assert(fixed == 1);
  check(0);
  update();                                     // Store expression
  return entry->val_int(&null_value);
}

String *Item_func_set_user_var::val_str(String *str)
{
  assert(fixed == 1);
  check(0);
  update();                                     // Store expression
  return entry->val_str(&null_value, str, decimals);
}


type::Decimal *Item_func_set_user_var::val_decimal(type::Decimal *val)
{
  assert(fixed == 1);
  check(0);
  update();                                     // Store expression
  return entry->val_decimal(&null_value, val);
}

double Item_func_set_user_var::val_result()
{
  assert(fixed == 1);
  check(true);
  update();                                     // Store expression
  return entry->val_real(&null_value);
}

int64_t Item_func_set_user_var::val_int_result()
{
  assert(fixed == 1);
  check(true);
  update();                                     // Store expression
  return entry->val_int(&null_value);
}

String *Item_func_set_user_var::str_result(String *str)
{
  assert(fixed == 1);
  check(true);
  update();                                     // Store expression
  return entry->val_str(&null_value, str, decimals);
}


type::Decimal *Item_func_set_user_var::val_decimal_result(type::Decimal *val)
{
  assert(fixed == 1);
  check(true);
  update();                                     // Store expression
  return entry->val_decimal(&null_value, val);
}

void Item_func_set_user_var::print(String *str)
{
  str->append(STRING_WITH_LEN("(@"));
  str->append(name.str, name.length);
  str->append(STRING_WITH_LEN(":="));
  args[0]->print(str);
  str->append(')');
}

void Item_func_set_user_var::send(plugin::Client *client, String *str_arg)
{
  if (result_field)
  {
    check(1);
    update();
    client->store(result_field);
    return;
  }
  Item::send(client, str_arg);
}

void Item_func_set_user_var::make_field(SendField *tmp_field)
{
  if (result_field)
  {
    result_field->make_field(tmp_field);
    assert(tmp_field->table_name != 0);
    if (Item::name)
      tmp_field->col_name=Item::name;               // Use user supplied name
  }
  else
  {
    Item::make_field(tmp_field);
  }
}

/*
  Save the value of a user variable into a field

  SYNOPSIS
    save_in_field()
      field           target field to save the value to
      no_conversion   flag indicating whether conversions are allowed

  DESCRIPTION
    Save the function value into a field and update the user variable
    accordingly. If a result field is defined and the target field doesn't
    coincide with it then the value from the result field will be used as
    the new value of the user variable.

    The reason to have this method rather than simply using the result
    field in the val_xxx() methods is that the value from the result field
    not always can be used when the result field is defined.
    Let's consider the following cases:
    1) when filling a tmp table the result field is defined but the value of it
    is undefined because it has to be produced yet. Thus we can't use it.
    2) on execution of an INSERT ... SELECT statement the save_in_field()
    function will be called to fill the data in the new record. If the SELECT
    part uses a tmp table then the result field is defined and should be
    used in order to get the correct result.

    The difference between the SET_USER_VAR function and regular functions
    like CONCAT is that the Item_func objects for the regular functions are
    replaced by Item_field objects after the values of these functions have
    been stored in a tmp table. Yet an object of the Item_field class cannot
    be used to update a user variable.
    Due to this we have to handle the result field in a special way here and
    in the Item_func_set_user_var::send() function.

  RETURN VALUES
    false       Ok
    true        Error
*/

int Item_func_set_user_var::save_in_field(Field *field, bool no_conversions,
                                          bool can_use_result_field)
{
  bool use_result_field= (!can_use_result_field ? 0 :
                          (result_field && result_field != field));
  int error;

  /* Update the value of the user variable */
  check(use_result_field);
  update();

  if (result_type() == STRING_RESULT ||
      (result_type() == REAL_RESULT && field->result_type() == STRING_RESULT))
  {
    String *result;
    const charset_info_st * const cs= collation.collation;
    char buff[MAX_FIELD_WIDTH];         // Alloc buffer for small columns
    str_value.set_quick(buff, sizeof(buff), cs);
    result= entry->val_str(&null_value, &str_value, decimals);

    if (null_value)
    {
      str_value.set_quick(0, 0, cs);
      return set_field_to_null_with_conversions(field, no_conversions);
    }

    /* NOTE: If null_value == false, "result" must be not NULL.  */

    field->set_notnull();
    error=field->store(result->ptr(),result->length(),cs);
    str_value.set_quick(0, 0, cs);
  }
  else if (result_type() == REAL_RESULT)
  {
    double nr= entry->val_real(&null_value);
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store(nr);
  }
  else if (result_type() == DECIMAL_RESULT)
  {
    type::Decimal decimal_value;
    type::Decimal *val= entry->val_decimal(&null_value, &decimal_value);
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store_decimal(val);
  }
  else
  {
    int64_t nr= entry->val_int(&null_value);
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store(nr, unsigned_flag);
  }
  return error;
}


} /* namespace drizzled */
