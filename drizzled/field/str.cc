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
#include <drizzled/field/str.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/internal/m_string.h>

namespace drizzled {

namespace internal
{
  extern char _dig_vec_upper[];
}

Field_str::Field_str(unsigned char *ptr_arg,
                     uint32_t len_arg,
                     unsigned char *null_ptr_arg,
                     unsigned char null_bit_arg,
                     const char *field_name_arg,
                     const charset_info_st * const charset_arg)
  :Field(ptr_arg, len_arg,
         null_ptr_arg,
         null_bit_arg,
         Field::NONE,
         field_name_arg)
{
  field_charset= charset_arg;
  if (charset_arg->state & MY_CS_BINSORT)
    flags|= BINARY_FLAG;
  field_derivation= DERIVATION_IMPLICIT;
}

/*
  Check if we lost any important data and send a truncation error/warning

  SYNOPSIS
    Field_str::report_if_important_data()
    ptr                      - Truncated rest of string
    end                      - End of truncated string

  RETURN VALUES
    0   - None was truncated (or we don't count cut fields)
    2   - Some bytes was truncated

  NOTE
    Check if we lost any important data (anything in a binary string,
    or any non-space in others). If only trailing spaces was lost,
    send a truncation note, otherwise send a truncation error.
*/

int Field_str::report_if_important_data(const char *field_ptr, const char *end)
{
  if (field_ptr < end && getTable()->in_use->count_cuted_fields)
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_ERROR, ER_DATA_TOO_LONG, 1);
    return 2;
  }
  return 0;
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

int Field_str::store_decimal(const type::Decimal *d)
{
  char buff[DECIMAL_MAX_STR_LENGTH+1];
  String str(buff, sizeof(buff), &my_charset_bin);
  class_decimal2string(d, 0, &str);
  return store(str.ptr(), str.length(), str.charset());
}

type::Decimal *Field_str::val_decimal(type::Decimal *decimal_value) const
{
  int2_class_decimal(E_DEC_FATAL_ERROR, val_int(), 0, decimal_value);
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
  bool error;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  size_t length= internal::my_gcvt(nr, internal::MY_GCVT_ARG_DOUBLE, local_char_length, buff, &error);
  if (error)
  {
    if (getTable()->getSession()->abortOnWarning())
    {
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_ERROR, ER_DATA_TOO_LONG, 1);
    }
    else
    {
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
    }
  }
  return store(buff, length, charset());
}


bool check_string_copy_error(Field_str *field,
                             const char *well_formed_error_pos,
                             const char *cannot_convert_error_pos,
                             const char *end,
                             const charset_info_st * const cs)
{
  const char* pos= well_formed_error_pos;
  if (not pos && not (pos= cannot_convert_error_pos))
    return false;

  const char* end_orig= end;
  set_if_smaller(end, pos + 6);

  char tmp[64];
  char* t= tmp;
  for (; pos < end; pos++)
  {
    /*
      If the source string is ASCII compatible (mbminlen==1)
      and the source character is in ASCII printable range (0x20..0x7F),
      then display the character as is.

      Otherwise, if the source string is not ASCII compatible (e.g. UCS2),
      or the source character is not in the printable range,
      then print the character using HEX notation.
    */
    if (((unsigned char) *pos) >= 0x20 && ((unsigned char) *pos) <= 0x7F && cs->mbminlen == 1)
    {
      *t++= *pos;
    }
    else
    {
      *t++= '\\';
      *t++= 'x';
      *t++= internal::_dig_vec_upper[((unsigned char) *pos) >> 4];
      *t++= internal::_dig_vec_upper[((unsigned char) *pos) & 15];
    }
  }
  if (end_orig > end)
  {
    *t++= '.';
    *t++= '.';
    *t++= '.';
  }
  *t= '\0';
  push_warning_printf(field->getTable()->in_use,
                      field->getTable()->in_use->abortOnWarning() ? DRIZZLE_ERROR::WARN_LEVEL_ERROR : DRIZZLE_ERROR::WARN_LEVEL_WARN,
                      ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
                      ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                      "string", tmp, field->field_name,
                      (uint32_t) field->getTable()->in_use->row_count);
  return true;
}

uint32_t Field_str::max_data_length() const
{
  return field_length + (field_length > 255 ? 2 : 1);
}

} /* namespace drizzled */
