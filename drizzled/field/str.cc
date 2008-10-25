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


#include <drizzled/server_includes.h>
#include <drizzled/field/str.h>
#include <drizzled/error.h>

Field_str::Field_str(unsigned char *ptr_arg,uint32_t len_arg, unsigned char *null_ptr_arg,
                     unsigned char null_bit_arg, utype unireg_check_arg,
                     const char *field_name_arg, const CHARSET_INFO * const charset_arg)
  :Field(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
         unireg_check_arg, field_name_arg)
{
  field_charset= charset_arg;
  if (charset_arg->state & MY_CS_BINSORT)
    flags|=BINARY_FLAG;
  field_derivation= DERIVATION_IMPLICIT;
}

/**
  Decimal representation of Field_str.

  @param d         value for storing

  @note
    Field_str is the base class for fields like Field_enum,
    Field_date and some similar. Some dates use fraction and also
    string value should be converted to floating point value according
    our rules, so we use double to store value of decimal in string.

  @todo
    use decimal2string?

  @retval
    0     OK
  @retval
    !=0  error
*/

int Field_str::store_decimal(const my_decimal *d)
{
  double val;
  /* TODO: use decimal2string? */
  int err= warn_if_overflow(my_decimal2double(E_DEC_FATAL_ERROR &
                                            ~E_DEC_OVERFLOW, d, &val));
  return err | store(val);
}

my_decimal *Field_str::val_decimal(my_decimal *decimal_value)
{
  int64_t nr= val_int();
  int2my_decimal(E_DEC_FATAL_ERROR, nr, 0, decimal_value);
  return decimal_value;
}

/**
  Store double value in Field_varstring.

  Pretty prints double number into field_length characters buffer.

  @param nr            number
*/

int Field_str::store(double nr)
{
  char buff[DOUBLE_TO_STRING_CONVERSION_BUFFER_SIZE];
  uint32_t local_char_length= field_length / charset()->mbmaxlen;
  size_t length;
  bool error;

  length= my_gcvt(nr, MY_GCVT_ARG_DOUBLE, local_char_length, buff, &error);
  if (error)
  {
    if (table->in_use->abort_on_warning)
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_ERROR, ER_DATA_TOO_LONG, 1);
    else
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
  }
  return store(buff, length, charset());
}

/* If one of the fields is binary and the other one isn't return 1 else 0 */

bool Field_str::compare_str_field_flags(Create_field *new_field, uint32_t flag_arg)
{
  return (((new_field->flags & (BINCMP_FLAG | BINARY_FLAG)) &&
          !(flag_arg & (BINCMP_FLAG | BINARY_FLAG))) ||
         (!(new_field->flags & (BINCMP_FLAG | BINARY_FLAG)) &&
          (flag_arg & (BINCMP_FLAG | BINARY_FLAG))));
}


uint32_t Field_str::is_equal(Create_field *new_field)
{
  if (compare_str_field_flags(new_field, flags))
    return 0;

  return ((new_field->sql_type == real_type()) &&
          new_field->charset == field_charset &&
          new_field->length == max_display_length());
}
