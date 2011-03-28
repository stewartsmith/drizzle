/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <drizzled/field/num.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/util/test.h>

namespace drizzled {

/**
  Numeric fields base class constructor.
*/
Field_num::Field_num(unsigned char *ptr_arg,
                     uint32_t len_arg,
                     unsigned char *null_ptr_arg,
                     unsigned char null_bit_arg,
                     utype unireg_check_arg,
                     const char *field_name_arg,
                     uint8_t dec_arg,
                     bool zero_arg,
                     bool unsigned_arg) :
  Field(ptr_arg,
        len_arg,
        null_ptr_arg,
        null_bit_arg,
        unireg_check_arg,
        field_name_arg),
  dec(dec_arg),
  decimal_precision(zero_arg),
  unsigned_flag(unsigned_arg)
  {
}


/**
  Test if given number is a int.

  @todo
    Make this multi-byte-character safe

  @param str            String to test
  @param length        Length of 'str'
  @param int_end        Pointer to char after last used digit
  @param cs             Character set

  @note
    This is called after one has called strntoull10rnd() function.

  @retval
    0   OK
  @retval
    1   error: empty string or wrong integer.
  @retval
    2   error: garbage at the end of string.
*/

int Field_num::check_int(const charset_info_st * const cs, const char *str, int length,
                         const char *int_end, int error)
{
  /* Test if we get an empty string or wrong integer */
  if (str == int_end || error == EDOM)
  {
    char buff[128];
    String tmp(buff, (uint32_t) sizeof(buff), system_charset_info);
    tmp.copy(str, length, system_charset_info);
    push_warning_printf(getTable()->in_use, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
                        ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                        "integer", tmp.c_ptr(), field_name,
                        (uint32_t) getTable()->in_use->row_count);
    return 1;
  }
  /* Test if we have garbage at the end of the given string. */
  if (test_if_important_data(cs, int_end, str + length))
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
    return 2;
  }
  return 0;
}

/*
  Conver a string to an integer then check bounds.

  SYNOPSIS
    Field_num::get_int
    cs            Character set
    from          String to convert
    len           Length of the string
    rnd           OUT int64_t value
    unsigned_max  max unsigned value
    signed_min    min signed value
    signed_max    max signed value

  DESCRIPTION
    The function calls strntoull10rnd() to get an integer value then
    check bounds and errors returned. In case of any error a warning
    is raised.

  RETURN
    0   ok
    1   error
*/

bool Field_num::get_int(const charset_info_st * const cs, const char *from, uint32_t len,
                        int64_t *rnd, uint64_t ,
                        int64_t signed_min, int64_t signed_max)
{
  char *end;
  int error;

  *rnd= (int64_t) cs->cset->strntoull10rnd(cs, from, len, false, &end, &error);
  if (*rnd < signed_min)
  {
    *rnd= signed_min;
    goto out_of_range;
  }
  else if (*rnd > signed_max)
  {
    *rnd= signed_max;
    goto out_of_range;
  }

  if (getTable()->in_use->count_cuted_fields &&
      check_int(cs, from, len, end, error))
    return 1;

  return 0;

out_of_range:
  set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
  return 1;
}

/**
  Storing decimal in integer fields.

  @param val       value for storing

  @note
    This method is used by all integer fields, real/decimal redefine it

  @retval
    0     OK
  @retval
    !=0  error
*/

int Field_num::store_decimal(const type::Decimal *val)
{
  int err= 0;
  int64_t i= convert_decimal2int64_t(val, false, &err);
  return test(err | store(i, false));
}

/**
  Return decimal value of integer field.

  @param decimal_value     buffer for storing decimal value

  @note
    This method is used by all integer fields, real/decimal redefine it.
    All int64_t values fit in our decimal buffer which cal store 8*9=72
    digits of integer number

  @return
    pointer to decimal buffer with value of field
*/

type::Decimal* Field_num::val_decimal(type::Decimal *decimal_value) const
{
  assert(result_type() == INT_RESULT);

  int64_t nr= val_int();
  int2_class_decimal(E_DEC_FATAL_ERROR, nr, false, decimal_value);
  return decimal_value;
}


void Field_num::make_field(SendField *field)
{
  Field::make_field(field);
  field->decimals= dec;
}

/**
  @return
  returns 1 if the fields are equally defined
*/
bool Field_num::eq_def(Field *field)
{
  if (!Field::eq_def(field))
    return 0;
  Field_num *from_num= (Field_num*) field;

  if (dec != from_num->dec)
    return 0;
  return 1;
}

uint32_t Field_num::is_equal(CreateField *new_field_ptr)
{
  return ((new_field_ptr->sql_type == real_type()) &&
          ((new_field_ptr->flags & UNSIGNED_FLAG) ==
           (uint32_t) (flags & UNSIGNED_FLAG)) &&
          ((new_field_ptr->flags & AUTO_INCREMENT_FLAG) ==
           (uint32_t) (flags & AUTO_INCREMENT_FLAG)) &&
          (new_field_ptr->length <= max_display_length()));
}


} /* namespace drizzled */
