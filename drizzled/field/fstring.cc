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


#include <drizzled/server_includes.h>
#include <drizzled/field/fstring.h>
#include <drizzled/drizzled_error_messages.h>

#define LONGLONG_TO_STRING_CONVERSION_BUFFER_SIZE 128
#define DECIMAL_TO_STRING_CONVERSION_BUFFER_SIZE 128

/****************************************************************************
** string type
** A string may be varchar or binary
****************************************************************************/

/* Copy a string and fill with space */
int Field_string::store(const char *from,uint32_t length, const CHARSET_INFO * const cs)
{
  uint32_t copy_length;
  const char *well_formed_error_pos;
  const char *cannot_convert_error_pos;
  const char *from_end_pos;

  /* See the comment for Field_long::store(int64_t) */
  assert(table->in_use == current_thd);

  copy_length= well_formed_copy_nchars(field_charset,
                                       (char*) ptr, field_length,
                                       cs, from, length,
                                       field_length / field_charset->mbmaxlen,
                                       &well_formed_error_pos,
                                       &cannot_convert_error_pos,
                                       &from_end_pos);

  /* Append spaces if the string was shorter than the field. */
  if (copy_length < field_length)
    field_charset->cset->fill(field_charset,(char*) ptr+copy_length,
                              field_length-copy_length,
                              field_charset->pad_char);

  if (check_string_copy_error(this, well_formed_error_pos,
                              cannot_convert_error_pos, from + length, cs))
    return 2;

  return report_if_important_data(from_end_pos, from + length);
}


int Field_string::store(int64_t nr, bool unsigned_val)
{
  char buff[64];
  int  l;
  const CHARSET_INFO * const cs= charset();
  l= (cs->cset->int64_t10_to_str)(cs,buff,sizeof(buff),
                                   unsigned_val ? 10 : -10, nr);
  return Field_string::store(buff,(uint)l,cs);
}


double Field_string::val_real(void)
{
  int error;
  char *end;
  const CHARSET_INFO * const cs= charset();
  double result;
  
  result=  my_strntod(cs,(char*) ptr,field_length,&end,&error);
  if (!table->in_use->no_errors &&
      (error || (field_length != (uint32_t)(end - (char*) ptr) && 
                 !check_if_only_end_space(cs, end,
                                          (char*) ptr + field_length))))
  {
    char buf[DOUBLE_TO_STRING_CONVERSION_BUFFER_SIZE];
    String tmp(buf, sizeof(buf), cs);
    tmp.copy((char*) ptr, field_length, cs);
    push_warning_printf(current_thd, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE, 
                        ER(ER_TRUNCATED_WRONG_VALUE),
                        "DOUBLE", tmp.c_ptr());
  }
  return result;
}


int64_t Field_string::val_int(void)
{
  int error;
  char *end;
  const CHARSET_INFO * const cs= charset();
  int64_t result;

  result= my_strntoll(cs, (char*) ptr,field_length,10,&end,&error);
  if (!table->in_use->no_errors &&
      (error || (field_length != (uint32_t)(end - (char*) ptr) && 
                 !check_if_only_end_space(cs, end,
                                          (char*) ptr + field_length))))
  {
    char buf[LONGLONG_TO_STRING_CONVERSION_BUFFER_SIZE];
    String tmp(buf, sizeof(buf), cs);
    tmp.copy((char*) ptr, field_length, cs);
    push_warning_printf(current_thd, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE, 
                        ER(ER_TRUNCATED_WRONG_VALUE),
                        "INTEGER", tmp.c_ptr());
  }
  return result;
}


String *Field_string::val_str(String *val_buffer __attribute__((unused)),
			      String *val_ptr)
{
  /* See the comment for Field_long::store(int64_t) */
  assert(table->in_use == current_thd);
  uint32_t length;

  length= field_charset->cset->lengthsp(field_charset, (const char*) ptr, field_length);
  val_ptr->set((const char*) ptr, length, field_charset);

  return val_ptr;
}


my_decimal *Field_string::val_decimal(my_decimal *decimal_value)
{
  int err= str2my_decimal(E_DEC_FATAL_ERROR, (char*) ptr, field_length,
                          charset(), decimal_value);
  if (!table->in_use->no_errors && err)
  {
    char buf[DECIMAL_TO_STRING_CONVERSION_BUFFER_SIZE];
    const CHARSET_INFO * const cs= charset();
    String tmp(buf, sizeof(buf), cs);
    tmp.copy((char*) ptr, field_length, cs);
    push_warning_printf(current_thd, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE, 
                        ER(ER_TRUNCATED_WRONG_VALUE),
                        "DECIMAL", tmp.c_ptr());
  }

  return decimal_value;
}


int Field_string::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  uint32_t a_len, b_len;

  if (field_charset->mbmaxlen != 1)
  {
    uint32_t char_len= field_length/field_charset->mbmaxlen;
    a_len= my_charpos(field_charset, a_ptr, a_ptr + field_length, char_len);
    b_len= my_charpos(field_charset, b_ptr, b_ptr + field_length, char_len);
  }
  else
    a_len= b_len= field_length;
  /*
    We have to remove end space to be able to compare multi-byte-characters
    like in latin_de 'ae' and 0xe4
  */
  return field_charset->coll->strnncollsp(field_charset,
                                          a_ptr, a_len,
                                          b_ptr, b_len,
                                          0);
}


void Field_string::sort_string(unsigned char *to,uint32_t length)
{
  uint32_t tmp= my_strnxfrm(field_charset,
                                 to, length,
                                 ptr, field_length);
  assert(tmp == length);
}


void Field_string::sql_type(String &res) const
{
  THD *thd= table->in_use;
  const CHARSET_INFO * const cs= res.charset();
  uint32_t length;

  length= cs->cset->snprintf(cs,(char*) res.ptr(),
                             res.alloced_length(), "%s(%d)",
                             ((type() == DRIZZLE_TYPE_VARCHAR &&
                               !thd->variables.new_mode) ?
                              (has_charset() ? "varchar" : "varbinary") :
			      (has_charset() ? "char" : "binary")),
                             (int) field_length / charset()->mbmaxlen);
  res.length(length);
}


unsigned char *Field_string::pack(unsigned char *to, const unsigned char *from,
                          uint32_t max_length,
                          bool low_byte_first __attribute__((unused)))
{
  uint32_t length=      cmin(field_length,max_length);
  uint32_t local_char_length= max_length/field_charset->mbmaxlen;
  if (length > local_char_length)
    local_char_length= my_charpos(field_charset, from, from+length,
                                  local_char_length);
  set_if_smaller(length, local_char_length);
  while (length && from[length-1] == field_charset->pad_char)
    length--;

  // Length always stored little-endian
  *to++= (unsigned char) length;
  if (field_length > 255)
    *to++= (unsigned char) (length >> 8);

  // Store the actual bytes of the string
  memcpy(to, from, length);
  return to+length;
}


/**
   Unpack a string field from row data.

   This method is used to unpack a string field from a master whose size 
   of the field is less than that of the slave. Note that there can be a
   variety of field types represented with this class. Certain types like
   ENUM or SET are processed differently. Hence, the upper byte of the 
   @c param_data argument contains the result of field->real_type() from
   the master.

   @param   to         Destination of the data
   @param   from       Source of the data
   @param   param_data Real type (upper) and length (lower) values

   @return  New pointer into memory based on from + length of the data
*/
const unsigned char *
Field_string::unpack(unsigned char *to,
                     const unsigned char *from,
                     uint32_t param_data,
                     bool low_byte_first __attribute__((unused)))
{
  uint32_t from_length=
    param_data ? cmin(param_data & 0x00ff, field_length) : field_length;
  uint32_t length;

  if (from_length > 255)
  {
    length= uint2korr(from);
    from+= 2;
  }
  else
    length= (uint) *from++;

  memcpy(to, from, length);
  // Pad the string with the pad character of the fields charset
  memset(to + length, field_charset->pad_char, field_length - length);
  return from+length;
}


/**
   Save the field metadata for string fields.

   Saves the real type in the first byte and the field length in the 
   second byte of the field metadata array at index of *metadata_ptr and
   *(metadata_ptr + 1).

   @param   metadata_ptr   First byte of field metadata

   @returns number of bytes written to metadata_ptr
*/
int Field_string::do_save_field_metadata(unsigned char *metadata_ptr)
{
  *metadata_ptr= real_type();
  *(metadata_ptr + 1)= field_length;
  return 2;
}


/*
  Compare two packed keys

  SYNOPSIS
    pack_cmp()
     a			New key
     b			Original key
     length		Key length
     insert_or_update	1 if this is an insert or update

  RETURN
    < 0	  a < b
    0	  a = b
    > 0   a > b
*/

int Field_string::pack_cmp(const unsigned char *a, const unsigned char *b, uint32_t length,
                           bool insert_or_update)
{
  uint32_t a_length, b_length;
  if (length > 255)
  {
    a_length= uint2korr(a);
    b_length= uint2korr(b);
    a+= 2;
    b+= 2;
  }
  else
  {
    a_length= (uint) *a++;
    b_length= (uint) *b++;
  }
  return field_charset->coll->strnncollsp(field_charset,
                                          a, a_length,
                                          b, b_length,
                                          insert_or_update);
}


/**
  Compare a packed key against row.

  @param key		        Original key
  @param length		Key length. (May be less than field length)
  @param insert_or_update	1 if this is an insert or update

  @return
    < 0	  row < key
  @return
    0	  row = key
  @return
    > 0   row > key
*/

int Field_string::pack_cmp(const unsigned char *key, uint32_t length,
                           bool insert_or_update)
{
  uint32_t row_length, local_key_length;
  unsigned char *end;
  if (length > 255)
  {
    local_key_length= uint2korr(key);
    key+= 2;
  }
  else
    local_key_length= (uint) *key++;
  
  /* Only use 'length' of key, not field_length */
  end= ptr + length;
  while (end > ptr && end[-1] == ' ')
    end--;
  row_length= (uint) (end - ptr);

  return field_charset->coll->strnncollsp(field_charset,
                                          ptr, row_length,
                                          key, local_key_length,
                                          insert_or_update);
}


uint32_t Field_string::packed_col_length(const unsigned char *data_ptr, uint32_t length)
{
  if (length > 255)
    return uint2korr(data_ptr)+2;
  return (uint) *data_ptr + 1;
}


uint32_t Field_string::max_packed_col_length(uint32_t max_length)
{
  return (max_length > 255 ? 2 : 1)+max_length;
}


uint32_t Field_string::get_key_image(unsigned char *buff,
                                 uint32_t length,
                                 imagetype type_arg __attribute__((unused)))
{
  uint32_t bytes = my_charpos(field_charset, (char*) ptr,
                          (char*) ptr + field_length,
                          length / field_charset->mbmaxlen);
  memcpy(buff, ptr, bytes);
  if (bytes < length)
    field_charset->cset->fill(field_charset, (char*) buff + bytes,
                              length - bytes, field_charset->pad_char);
  return bytes;
}


Field *Field_string::new_field(MEM_ROOT *root, Table *new_table, bool keep_type)
{
  Field *field;
  field= Field::new_field(root, new_table, keep_type);
  return field;
}


