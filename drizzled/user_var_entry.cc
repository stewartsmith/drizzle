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

#include <drizzled/session.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/user_var_entry.h>
#include <drizzled/type/decimal.h>
#include <drizzled/charset.h>

namespace drizzled {

/** Get the value of a variable as a double. */

double user_var_entry::val_real(bool *null_value)
{
  if ((*null_value= (value == 0)))
    return 0.0;

  switch (type) {
  case REAL_RESULT:
    return *(double*) value;

  case INT_RESULT:
    return (double) *(int64_t*) value;

  case DECIMAL_RESULT:
    {
      double result;
      class_decimal2double(E_DEC_FATAL_ERROR, (type::Decimal *)value, &result);
      return result;
    }

  case STRING_RESULT:
    return internal::my_atof(value);                      // This is null terminated

  case ROW_RESULT:
    assert(1);				// Impossible
    break;
  }
  return 0.0;					// Impossible
}


/** Get the value of a variable as an integer. */

int64_t user_var_entry::val_int(bool *null_value) const
{
  if ((*null_value= (value == 0)))
    return 0L;

  switch (type) {
  case REAL_RESULT:
    return (int64_t) *(double*) value;

  case INT_RESULT:
    return *(int64_t*) value;

  case DECIMAL_RESULT:
    {
      int64_t result;
      ((type::Decimal *)(value))->val_int32(E_DEC_FATAL_ERROR, 0, &result);
      return result;
    }

  case STRING_RESULT:
    {
      int error;
      return internal::my_strtoll10(value, (char**) 0, &error);// String is null terminated
    }

  case ROW_RESULT:
    assert(1);				// Impossible
    break;
  }

  return 0L;					// Impossible
}


/** Get the value of a variable as a string. */

String *user_var_entry::val_str(bool *null_value, String *str,
                                uint32_t decimals)
{
  if ((*null_value= (value == 0)))
    return (String*) 0;

  switch (type) {
  case REAL_RESULT:
    str->set_real(*(double*) value, decimals, &my_charset_bin);
    break;

  case INT_RESULT:
    if (!unsigned_flag)
      str->set(*(int64_t*) value, &my_charset_bin);
    else
      str->set(*(uint64_t*) value, &my_charset_bin);
    break;

  case DECIMAL_RESULT:
    class_decimal2string((type::Decimal *)value, 0, str);
    break;

  case STRING_RESULT:
    str->copy(value, length, collation.collation);
    // break missing?

  case ROW_RESULT:
    assert(1);				// Impossible
    break;
  }

  return(str);
}

/** Get the value of a variable as a decimal. */

type::Decimal *user_var_entry::val_decimal(bool *null_value, type::Decimal *val)
{
  if ((*null_value= (value == 0)))
    return 0;

  switch (type) {
  case REAL_RESULT:
    double2_class_decimal(E_DEC_FATAL_ERROR, *(double*) value, val);
    break;

  case INT_RESULT:
    int2_class_decimal(E_DEC_FATAL_ERROR, *(int64_t*) value, 0, val);
    break;

  case DECIMAL_RESULT:
    val= (type::Decimal *)value;
    break;

  case STRING_RESULT:
    val->store(E_DEC_FATAL_ERROR, value, length, collation.collation);
    break;

  case ROW_RESULT:
    assert(1);				// Impossible
    break;
  }

  return(val);
}

/**
  Set value to user variable.

  @param entry          pointer to structure representing variable
  @param set_null       should we set NULL value ?
  @param ptr            pointer to buffer with new value
  @param length         length of new value
  @param type           type of new value
  @param cs             charset info for new value
  @param dv             derivation for new value
  @param unsigned_arg   indiates if a value of type INT_RESULT is unsigned

  @note Sets error and fatal error if allocation fails.

  @retval
    false   success
  @retval
    true    failure
*/

void user_var_entry::update_hash(bool set_null, void *ptr, uint32_t arg_length,
                                 Item_result arg_type, const charset_info_st * const cs, Derivation dv,
                                 bool unsigned_arg)
{
  if (set_null)
  {
    if (value)
    {
      assert(length && size);
      free(value);
      value= NULL;
      length= 0;
      size= 0;
    }
  }
  else
  {
    size_t needed_size= arg_length + ((arg_type == STRING_RESULT) ? 1 : 0);

    if (needed_size > size)
    {
			value= (char *)realloc(value, needed_size);
      size= needed_size;
    }

    if (arg_type == STRING_RESULT)
      value[arg_length]= 0;			// Store end \0

    memcpy(value, ptr, arg_length);
    if (arg_type == DECIMAL_RESULT)
      ((type::Decimal*)value)->fix_buffer_pointer();
    length= arg_length;
    collation.set(cs, dv);
    unsigned_flag= unsigned_arg;
  }
  type= arg_type;
}

} /* namespace drizzled */
