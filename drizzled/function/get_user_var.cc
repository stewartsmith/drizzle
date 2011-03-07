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

#include <float.h>

#include <drizzled/function/get_user_var.h>
#include <drizzled/item/null.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <drizzled/user_var_entry.h>

namespace drizzled
{

String *Item_func_get_user_var::val_str(String *str)
{
  assert(fixed == 1);
  if (!var_entry)
    return((String*) 0);                        // No such variable
  return(var_entry->val_str(&null_value, str, decimals));
}


double Item_func_get_user_var::val_real()
{
  assert(fixed == 1);
  if (!var_entry)
    return 0.0;                                 // No such variable
  return (var_entry->val_real(&null_value));
}


type::Decimal *Item_func_get_user_var::val_decimal(type::Decimal *dec)
{
  assert(fixed == 1);
  if (!var_entry)
    return 0;
  return var_entry->val_decimal(&null_value, dec);
}

int64_t Item_func_get_user_var::val_int()
{
  assert(fixed == 1);
  if (!var_entry)
    return 0L;                          // No such variable
  return (var_entry->val_int(&null_value));
}

void Item_func_get_user_var::fix_length_and_dec()
{
  maybe_null=1;
  decimals=NOT_FIXED_DEC;
  max_length=MAX_BLOB_WIDTH;

  var_entry= session.getVariable(name, false);

  /*
    If the variable didn't exist it has been created as a STRING-type.
    'var_entry' is NULL only if there occured an error during the call to
    get_var_with_binlog.
  */
  if (var_entry)
  {
    m_cached_result_type= var_entry->type;
    unsigned_flag= var_entry->unsigned_flag;
    max_length= var_entry->length;

    collation.set(var_entry->collation);
    switch(m_cached_result_type) 
    {
    case REAL_RESULT:
      max_length= DBL_DIG + 8;
      break;

    case INT_RESULT:
      max_length= MAX_BIGINT_WIDTH;
      decimals=0;
      break;
    case STRING_RESULT:
      max_length= MAX_BLOB_WIDTH;
      break;

    case DECIMAL_RESULT:
      max_length= DECIMAL_MAX_STR_LENGTH;
      decimals= DECIMAL_MAX_SCALE;
      break;

    case ROW_RESULT:                            // Keep compiler happy
      assert(0);
      break;
    }
  }
  else
  {
    collation.set(&my_charset_bin, DERIVATION_IMPLICIT);
    null_value= 1;
    m_cached_result_type= STRING_RESULT;
    max_length= MAX_BLOB_WIDTH;
  }
}


bool Item_func_get_user_var::const_item() const
{
  return (!var_entry || session.getQueryId() != var_entry->update_query_id);
}


enum Item_result Item_func_get_user_var::result_type() const
{
  return m_cached_result_type;
}


void Item_func_get_user_var::print(String *str)
{
  str->append(STRING_WITH_LEN("(@"));
  str->append(name.str,name.length);
  str->append(')');
}


bool Item_func_get_user_var::eq(const Item *item,
                                bool ) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;					// Same item is same.
  /* Check if other type is also a get_user_var() object */
  if (item->type() != FUNC_ITEM ||
      ((Item_func*) item)->functype() != functype())
    return 0;
  Item_func_get_user_var *other=(Item_func_get_user_var*) item;
  return (name.length == other->name.length &&
	  !memcmp(name.str, other->name.str, name.length));
}

} /* namespace drizzled */
