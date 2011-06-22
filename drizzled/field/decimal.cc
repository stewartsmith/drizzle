/* - mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
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
#include <drizzled/field/decimal.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>

namespace drizzled
{

/****************************************************************************
 ** File_decimal
 ****************************************************************************/

Field_decimal::Field_decimal(unsigned char *ptr_arg,
                             uint32_t len_arg,
                             unsigned char *null_ptr_arg,
                             unsigned char null_bit_arg,
                             enum utype unireg_check_arg,
                             const char *field_name_arg,
                             uint8_t dec_arg) :
  Field_num(ptr_arg,
            len_arg,
            null_ptr_arg,
            null_bit_arg,
            unireg_check_arg,
            field_name_arg,
            dec_arg, false,
            false)
  {
    precision= class_decimal_length_to_precision(len_arg, dec_arg, false);
    set_if_smaller(precision, (uint32_t)DECIMAL_MAX_PRECISION);
    assert((precision <= DECIMAL_MAX_PRECISION) && (dec <= DECIMAL_MAX_SCALE));
    bin_size= class_decimal_get_binary_size(precision, dec);
  }

Field_decimal::Field_decimal(uint32_t len_arg,
                             bool maybe_null_arg,
                             const char *name,
                             uint8_t dec_arg,
                             bool unsigned_arg) :
  Field_num((unsigned char*) 0,
            len_arg,
            maybe_null_arg ? (unsigned char*) "": 0,
            0,
            NONE,
            name,
            dec_arg,
            0,
            unsigned_arg)
{
  precision= class_decimal_length_to_precision(len_arg, dec_arg, unsigned_arg);
  set_if_smaller(precision, (uint32_t)DECIMAL_MAX_PRECISION);
  assert((precision <= DECIMAL_MAX_PRECISION) &&
         (dec <= DECIMAL_MAX_SCALE));
  bin_size= class_decimal_get_binary_size(precision, dec);
}


int Field_decimal::reset(void)
{
  store_value(&decimal_zero);
  return 0;
}


/**
  Generate max/min decimal value in case of overflow.

  @param decimal_value     buffer for value
  @param sign              sign of value which caused overflow
*/

void Field_decimal::set_value_on_overflow(type::Decimal *decimal_value,
                                          bool sign)
{
  max_Decimal(decimal_value, precision, decimals());
  if (sign)
    decimal_value->sign(true);

  return;
}


/**
  Store decimal value in the binary buffer.

  Checks if decimal_value fits into field size.
  If it does, stores the decimal in the buffer using binary format.
  Otherwise sets maximal number that can be stored in the field.

  @param decimal_value   type::Decimal

  @retval
  0 ok
  @retval
  1 error
*/

bool Field_decimal::store_value(const type::Decimal *decimal_value)
{
  int error= decimal_value->val_binary(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW, ptr, precision, dec);

  if (warn_if_overflow(error))
  {
    if (error != E_DEC_TRUNCATED)
    {
      type::Decimal buff;
      set_value_on_overflow(&buff, decimal_value->sign());
      buff.val_binary(E_DEC_FATAL_ERROR, ptr, precision, dec);
    }
    error= 1;
  }

  return(error);
}


int Field_decimal::store(const char *from, uint32_t length,
                         const charset_info_st * const charset_arg)
{
  int err;
  type::Decimal decimal_value;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if ((err= decimal_value.store(E_DEC_FATAL_ERROR &
                           ~(E_DEC_OVERFLOW | E_DEC_BAD_NUM),
                           from, length, charset_arg)) &&
      getTable()->in_use->abortOnWarning())
  {
    /* Because "from" is not NUL-terminated and we use %s in the ER() */
    String from_as_str;
    from_as_str.copy(from, length, &my_charset_bin);

    push_warning_printf(getTable()->in_use, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
                        ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                        "decimal", from_as_str.c_ptr(), field_name,
                        (uint32_t) getTable()->in_use->row_count);

    return(err);
  }

  switch (err) {
  case E_DEC_TRUNCATED:
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
    set_value_on_overflow(&decimal_value, decimal_value.sign());
    break;
  case E_DEC_OVERFLOW:
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    set_value_on_overflow(&decimal_value, decimal_value.sign());
    break;
  case E_DEC_BAD_NUM:
    {
      /* Because "from" is not NUL-terminated and we use %s in the ER() */
      String from_as_str;
      from_as_str.copy(from, length, &my_charset_bin);

      push_warning_printf(getTable()->in_use, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
                          ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                          "decimal", from_as_str.c_ptr(), field_name,
                          (uint32_t) getTable()->in_use->row_count);
      decimal_value.set_zero();

      break;
    }
  }

  store_value(&decimal_value);
  return(err);
}


/**
  @todo
  Fix following when double2_class_decimal when double2decimal
  will return E_DEC_TRUNCATED always correctly
*/

int Field_decimal::store(double nr)
{
  type::Decimal decimal_value;
  int err;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  err= double2_class_decimal(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW, nr,
                         &decimal_value);
  if (err)
  {
    if (check_overflow(err))
      set_value_on_overflow(&decimal_value, decimal_value.sign());
    /* Only issue a warning if store_value doesn't issue an warning */
    getTable()->in_use->got_warning= 0;
  }
  if (store_value(&decimal_value))
    err= 1;
  else if (err && !getTable()->in_use->got_warning)
    err= warn_if_overflow(err);
  return(err);
}


int Field_decimal::store(int64_t nr, bool unsigned_val)
{
  type::Decimal decimal_value;
  int err;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if ((err= int2_class_decimal(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW,
                           nr, unsigned_val, &decimal_value)))
  {
    if (check_overflow(err))
      set_value_on_overflow(&decimal_value, decimal_value.sign());
    /* Only issue a warning if store_value doesn't issue an warning */
    getTable()->in_use->got_warning= 0;
  }
  if (store_value(&decimal_value))
    err= 1;
  else if (err && not getTable()->in_use->got_warning)
    err= warn_if_overflow(err);
  return err;
}


int Field_decimal::store_decimal(const type::Decimal *decimal_value)
{
  return store_value(decimal_value);
}


int Field_decimal::store_time(type::Time &ltime,
                              type::timestamp_t )
{
  type::Decimal decimal_value;
  return store_value(date2_class_decimal(&ltime, &decimal_value));
}


double Field_decimal::val_real(void) const
{
  double dbl;
  type::Decimal decimal_value;

  ASSERT_COLUMN_MARKED_FOR_READ;

  class_decimal2double(E_DEC_FATAL_ERROR, val_decimal(&decimal_value), &dbl);

  return dbl;
}


int64_t Field_decimal::val_int(void) const
{
  int64_t i;
  type::Decimal decimal_value;

  ASSERT_COLUMN_MARKED_FOR_READ;

  val_decimal(&decimal_value)->val_int32(E_DEC_FATAL_ERROR, false, &i);

  return i;
}


type::Decimal* Field_decimal::val_decimal(type::Decimal *decimal_value) const
{
  ASSERT_COLUMN_MARKED_FOR_READ;

  binary2_class_decimal(E_DEC_FATAL_ERROR, ptr, decimal_value,
                    precision, dec);
  return(decimal_value);
}


String *Field_decimal::val_str(String *val_buffer, String *) const
{
  type::Decimal decimal_value;

  ASSERT_COLUMN_MARKED_FOR_READ;

  class_decimal2string(val_decimal(&decimal_value),
                       dec, val_buffer);
  return val_buffer;
}


int Field_decimal::cmp(const unsigned char *a,const unsigned char*b)
{
  return memcmp(a, b, bin_size);
}


void Field_decimal::sort_string(unsigned char *buff, uint32_t)
{
  memcpy(buff, ptr, bin_size);
}

/**
  Returns the number of bytes field uses in row-based replication
  row packed size.

  This method is used in row-based replication to determine the number
  of bytes that the field consumes in the row record format. This is
  used to skip fields in the master that do not exist on the slave.

  @param   field_metadata   Encoded size in field metadata

  @returns The size of the field based on the field metadata.
*/
uint32_t Field_decimal::pack_length_from_metadata(uint32_t field_metadata)
{
  uint32_t const source_precision= (field_metadata >> 8U) & 0x00ff;
  uint32_t const source_decimal= field_metadata & 0x00ff;
  uint32_t const source_size= class_decimal_get_binary_size(source_precision,
                                                         source_decimal);
  return (source_size);
}


uint32_t Field_decimal::is_equal(CreateField *new_field_ptr)
{
  return ((new_field_ptr->sql_type == real_type()) &&
          ((new_field_ptr->flags & UNSIGNED_FLAG) ==
           (uint32_t) (flags & UNSIGNED_FLAG)) &&
          ((new_field_ptr->flags & AUTO_INCREMENT_FLAG) ==
           (uint32_t) (flags & AUTO_INCREMENT_FLAG)) &&
          (new_field_ptr->length == max_display_length()) &&
          (new_field_ptr->decimals == dec));
}


/**
  Unpack a decimal field from row data.

  This method is used to unpack a decimal or numeric field from a master
  whose size of the field is less than that of the slave.

  @param   to         Destination of the data
  @param   from       Source of the data
  @param   param_data Precision (upper) and decimal (lower) values

  @return  New pointer into memory based on from + length of the data
*/
const unsigned char *
Field_decimal::unpack(unsigned char* to,
                      const unsigned char *from,
                      uint32_t param_data,
                      bool low_byte_first)
{
  if (param_data == 0)
    return Field::unpack(to, from, param_data, low_byte_first);

  uint32_t from_precision= (param_data & 0xff00) >> 8U;
  uint32_t from_decimal= param_data & 0x00ff;
  uint32_t length=pack_length();
  uint32_t from_pack_len= class_decimal_get_binary_size(from_precision, from_decimal);
  uint32_t len= (param_data && (from_pack_len < length)) ?
    from_pack_len : length;
  if ((from_pack_len && (from_pack_len < length)) ||
      (from_precision < precision) ||
      (from_decimal < decimals()))
  {
    /*
      If the master's data is smaller than the slave, we need to convert
      the binary to decimal then resize the decimal converting it back to
      a decimal and write that to the raw data buffer.
    */
    decimal_digit_t dec_buf[DECIMAL_MAX_PRECISION];
    decimal_t conv_dec;
    conv_dec.len= from_precision;
    conv_dec.buf= dec_buf;
    /*
Note: bin2decimal does not change the length of the field. So it is
just the first step the resizing operation. The second step does the
resizing using the precision and decimals from the slave.
    */
    bin2decimal((unsigned char *)from, &conv_dec, from_precision, from_decimal);
    decimal2bin(&conv_dec, to, precision, decimals());
  }
  else
    memcpy(to, from, len); // Sizes are the same, just copy the data.
  return from+len;
}

} /* namespace drizzled */
