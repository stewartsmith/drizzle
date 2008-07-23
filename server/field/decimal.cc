/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "drizzle/server/field/decimal.h"

/****************************************************************************
** Field_new_decimal
****************************************************************************/

Field_new_decimal::Field_new_decimal(uchar *ptr_arg,
                                     uint32_t len_arg, uchar *null_ptr_arg,
                                     uchar null_bit_arg,
                                     enum utype unireg_check_arg,
                                     const char *field_name_arg,
                                     uint8_t dec_arg,bool zero_arg,
                                     bool unsigned_arg)
  :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
             unireg_check_arg, field_name_arg, dec_arg, zero_arg, unsigned_arg)
{
  precision= my_decimal_length_to_precision(len_arg, dec_arg, unsigned_arg);
  set_if_smaller(precision, DECIMAL_MAX_PRECISION);
  assert((precision <= DECIMAL_MAX_PRECISION) &&
              (dec <= DECIMAL_MAX_SCALE));
  bin_size= my_decimal_get_binary_size(precision, dec);
}


Field_new_decimal::Field_new_decimal(uint32_t len_arg,
                                     bool maybe_null_arg,
                                     const char *name,
                                     uint8_t dec_arg,
                                     bool unsigned_arg)
  :Field_num((uchar*) 0, len_arg,
             maybe_null_arg ? (uchar*) "": 0, 0,
             NONE, name, dec_arg, 0, unsigned_arg)
{
  precision= my_decimal_length_to_precision(len_arg, dec_arg, unsigned_arg);
  set_if_smaller(precision, DECIMAL_MAX_PRECISION);
  assert((precision <= DECIMAL_MAX_PRECISION) &&
              (dec <= DECIMAL_MAX_SCALE));
  bin_size= my_decimal_get_binary_size(precision, dec);
}


int Field_new_decimal::reset(void)
{
  store_value(&decimal_zero);
  return 0;
}


/**
  Generate max/min decimal value in case of overflow.

  @param decimal_value     buffer for value
  @param sign              sign of value which caused overflow
*/

void Field_new_decimal::set_value_on_overflow(my_decimal *decimal_value,
                                              bool sign)
{
  max_my_decimal(decimal_value, precision, decimals());
  if (sign)
  {
    if (unsigned_flag)
      my_decimal_set_zero(decimal_value);
    else
      decimal_value->sign(true);
  }
  return;
}


/**
  Store decimal value in the binary buffer.

  Checks if decimal_value fits into field size.
  If it does, stores the decimal in the buffer using binary format.
  Otherwise sets maximal number that can be stored in the field.

  @param decimal_value   my_decimal

  @retval
    0 ok
  @retval
    1 error
*/

bool Field_new_decimal::store_value(const my_decimal *decimal_value)
{
  int error= 0;

  /* check that we do not try to write negative value in unsigned field */
  if (unsigned_flag && decimal_value->sign())
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    error= 1;
    decimal_value= &decimal_zero;
  }

  if (warn_if_overflow(my_decimal2binary(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW,
                                         decimal_value, ptr, precision, dec)))
  {
    my_decimal buff;
    set_value_on_overflow(&buff, decimal_value->sign());
    my_decimal2binary(E_DEC_FATAL_ERROR, &buff, ptr, precision, dec);
    error= 1;
  }
  return(error);
}


int Field_new_decimal::store(const char *from, uint length,
                             CHARSET_INFO *charset_arg)
{
  int err;
  my_decimal decimal_value;

  if ((err= str2my_decimal(E_DEC_FATAL_ERROR &
                           ~(E_DEC_OVERFLOW | E_DEC_BAD_NUM),
                           from, length, charset_arg,
                           &decimal_value)) &&
      table->in_use->abort_on_warning)
  {
    /* Because "from" is not NUL-terminated and we use %s in the ER() */
    String from_as_str;
    from_as_str.copy(from, length, &my_charset_bin);

    push_warning_printf(table->in_use, MYSQL_ERROR::WARN_LEVEL_ERROR,
                        ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
                        ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                        "decimal", from_as_str.c_ptr(), field_name,
                        (ulong) table->in_use->row_count);

    return(err);
  }

  switch (err) {
  case E_DEC_TRUNCATED:
    set_warning(MYSQL_ERROR::WARN_LEVEL_NOTE, WARN_DATA_TRUNCATED, 1);
    break;
  case E_DEC_OVERFLOW:
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    set_value_on_overflow(&decimal_value, decimal_value.sign());
    break;
  case E_DEC_BAD_NUM:
    {
      /* Because "from" is not NUL-terminated and we use %s in the ER() */
      String from_as_str;
      from_as_str.copy(from, length, &my_charset_bin);

    push_warning_printf(table->in_use, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
                        ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                          "decimal", from_as_str.c_ptr(), field_name,
                        (ulong) table->in_use->row_count);
    my_decimal_set_zero(&decimal_value);

    break;
    }
  }

  store_value(&decimal_value);
  return(err);
}


/**
  @todo
  Fix following when double2my_decimal when double2decimal
  will return E_DEC_TRUNCATED always correctly
*/

int Field_new_decimal::store(double nr)
{
  my_decimal decimal_value;
  int err;

  err= double2my_decimal(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW, nr,
                         &decimal_value);
  if (err)
  {
    if (check_overflow(err))
      set_value_on_overflow(&decimal_value, decimal_value.sign());
    /* Only issue a warning if store_value doesn't issue an warning */
    table->in_use->got_warning= 0;
  }
  if (store_value(&decimal_value))
    err= 1;
  else if (err && !table->in_use->got_warning)
    err= warn_if_overflow(err);
  return(err);
}


int Field_new_decimal::store(int64_t nr, bool unsigned_val)
{
  my_decimal decimal_value;
  int err;

  if ((err= int2my_decimal(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW,
                           nr, unsigned_val, &decimal_value)))
  {
    if (check_overflow(err))
      set_value_on_overflow(&decimal_value, decimal_value.sign());
    /* Only issue a warning if store_value doesn't issue an warning */
    table->in_use->got_warning= 0;
  }
  if (store_value(&decimal_value))
    err= 1;
  else if (err && !table->in_use->got_warning)
    err= warn_if_overflow(err);
  return err;
}


int Field_new_decimal::store_decimal(const my_decimal *decimal_value)
{
  return store_value(decimal_value);
}


int Field_new_decimal::store_time(MYSQL_TIME *ltime,
                                  timestamp_type t_type __attribute__((__unused__)))
{
    my_decimal decimal_value;
    return store_value(date2my_decimal(ltime, &decimal_value));
}


double Field_new_decimal::val_real(void)
{
  double dbl;
  my_decimal decimal_value;
  my_decimal2double(E_DEC_FATAL_ERROR, val_decimal(&decimal_value), &dbl);
  return dbl;
}


int64_t Field_new_decimal::val_int(void)
{
  int64_t i;
  my_decimal decimal_value;
  my_decimal2int(E_DEC_FATAL_ERROR, val_decimal(&decimal_value),
                 unsigned_flag, &i);
  return i;
}


my_decimal* Field_new_decimal::val_decimal(my_decimal *decimal_value)
{
  binary2my_decimal(E_DEC_FATAL_ERROR, ptr, decimal_value,
                    precision, dec);
  return(decimal_value);
}


String *Field_new_decimal::val_str(String *val_buffer,
                                   String *val_ptr __attribute__((unused)))
{
  my_decimal decimal_value;
  uint fixed_precision= zerofill ? precision : 0;
  my_decimal2string(E_DEC_FATAL_ERROR, val_decimal(&decimal_value),
                    fixed_precision, dec, '0', val_buffer);
  return val_buffer;
}


int Field_new_decimal::cmp(const uchar *a,const uchar*b)
{
  return memcmp(a, b, bin_size);
}


void Field_new_decimal::sort_string(uchar *buff,
                                    uint length __attribute__((unused)))
{
  memcpy(buff, ptr, bin_size);
}


void Field_new_decimal::sql_type(String &str) const
{
  CHARSET_INFO *cs= str.charset();
  str.length(cs->cset->snprintf(cs, (char*) str.ptr(), str.alloced_length(),
                                "decimal(%d,%d)", precision, (int)dec));
  add_zerofill_and_unsigned(str);
}


/**
   Save the field metadata for new decimal fields.

   Saves the precision in the first byte and decimals() in the second
   byte of the field metadata array at index of *metadata_ptr and 
   *(metadata_ptr + 1).

   @param   metadata_ptr   First byte of field metadata

   @returns number of bytes written to metadata_ptr
*/
int Field_new_decimal::do_save_field_metadata(uchar *metadata_ptr)
{
  *metadata_ptr= precision;
  *(metadata_ptr + 1)= decimals();
  return 2;
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
uint Field_new_decimal::pack_length_from_metadata(uint field_metadata)
{
  uint const source_precision= (field_metadata >> 8U) & 0x00ff;
  uint const source_decimal= field_metadata & 0x00ff; 
  uint const source_size= my_decimal_get_binary_size(source_precision, 
                                                     source_decimal);
  return (source_size);
}


/**
   Check to see if field size is compatible with destination.

   This method is used in row-based replication to verify that the slave's
   field size is less than or equal to the master's field size. The 
   encoded field metadata (from the master or source) is decoded and compared
   to the size of this field (the slave or destination). 

   @param   field_metadata   Encoded size in field metadata

   @retval 0 if this field's size is < the source field's size
   @retval 1 if this field's size is >= the source field's size
*/
int Field_new_decimal::compatible_field_size(uint field_metadata)
{
  int compatible= 0;
  uint const source_precision= (field_metadata >> 8U) & 0x00ff;
  uint const source_decimal= field_metadata & 0x00ff; 
  uint const source_size= my_decimal_get_binary_size(source_precision, 
                                                     source_decimal);
  uint const destination_size= row_pack_length();
  compatible= (source_size <= destination_size);
  if (compatible)
    compatible= (source_precision <= precision) &&
                (source_decimal <= decimals());
  return (compatible);
}


uint Field_new_decimal::is_equal(Create_field *new_field)
{
  return ((new_field->sql_type == real_type()) &&
          ((new_field->flags & UNSIGNED_FLAG) == 
           (uint) (flags & UNSIGNED_FLAG)) &&
          ((new_field->flags & AUTO_INCREMENT_FLAG) ==
           (uint) (flags & AUTO_INCREMENT_FLAG)) &&
          (new_field->length == max_display_length()) &&
          (new_field->decimals == dec));
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
const uchar *
Field_new_decimal::unpack(uchar* to,
                          const uchar *from,
                          uint param_data,
                          bool low_byte_first)
{
  if (param_data == 0)
    return Field::unpack(to, from, param_data, low_byte_first);

  uint from_precision= (param_data & 0xff00) >> 8U;
  uint from_decimal= param_data & 0x00ff;
  uint length=pack_length();
  uint from_pack_len= my_decimal_get_binary_size(from_precision, from_decimal);
  uint len= (param_data && (from_pack_len < length)) ?
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
    decimal_t dec;
    dec.len= from_precision;
    dec.buf= dec_buf;
    /*
      Note: bin2decimal does not change the length of the field. So it is
      just the first step the resizing operation. The second step does the
      resizing using the precision and decimals from the slave.
    */
    bin2decimal((uchar *)from, &dec, from_precision, from_decimal);
    decimal2bin(&dec, to, precision, decimals());
  }
  else
    memcpy(to, from, len); // Sizes are the same, just copy the data.
  return from+len;
}

