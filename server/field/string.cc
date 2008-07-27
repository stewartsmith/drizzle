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

#include "drizzle/server/field/string.h"

#define LONGLONG_TO_STRING_CONVERSION_BUFFER_SIZE 128
#define DECIMAL_TO_STRING_CONVERSION_BUFFER_SIZE 128

/****************************************************************************
** string type
** A string may be varchar or binary
****************************************************************************/

/* Copy a string and fill with space */
int Field_string::store(const char *from,uint length,CHARSET_INFO *cs)
{
  uint copy_length;
  const char *well_formed_error_pos;
  const char *cannot_convert_error_pos;
  const char *from_end_pos;

  /* See the comment for Field_long::store(long long) */
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
  CHARSET_INFO *cs=charset();
  l= (cs->cset->int64_t10_to_str)(cs,buff,sizeof(buff),
                                   unsigned_val ? 10 : -10, nr);
  return Field_string::store(buff,(uint)l,cs);
}


double Field_string::val_real(void)
{
  int error;
  char *end;
  CHARSET_INFO *cs= charset();
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
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
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
  CHARSET_INFO *cs= charset();
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
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE, 
                        ER(ER_TRUNCATED_WRONG_VALUE),
                        "INTEGER", tmp.c_ptr());
  }
  return result;
}


String *Field_string::val_str(String *val_buffer __attribute__((unused)),
			      String *val_ptr)
{
  /* See the comment for Field_long::store(long long) */
  assert(table->in_use == current_thd);
  uint length;
  if (table->in_use->variables.sql_mode &
      MODE_PAD_CHAR_TO_FULL_LENGTH)
    length= my_charpos(field_charset, ptr, ptr + field_length, field_length);
  else
    length= field_charset->cset->lengthsp(field_charset, (const char*) ptr,
                                          field_length);
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
    CHARSET_INFO *cs= charset();
    String tmp(buf, sizeof(buf), cs);
    tmp.copy((char*) ptr, field_length, cs);
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE, 
                        ER(ER_TRUNCATED_WRONG_VALUE),
                        "DECIMAL", tmp.c_ptr());
  }

  return decimal_value;
}


int Field_string::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  uint a_len, b_len;

  if (field_charset->mbmaxlen != 1)
  {
    uint char_len= field_length/field_charset->mbmaxlen;
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


void Field_string::sort_string(uchar *to,uint length)
{
  uint tmp= my_strnxfrm(field_charset,
                                 to, length,
                                 ptr, field_length);
  assert(tmp == length);
}


void Field_string::sql_type(String &res) const
{
  THD *thd= table->in_use;
  CHARSET_INFO *cs=res.charset();
  ulong length;

  length= cs->cset->snprintf(cs,(char*) res.ptr(),
                             res.alloced_length(), "%s(%d)",
                             ((type() == FIELD_TYPE_VAR_STRING &&
                               !thd->variables.new_mode) ?
                              (has_charset() ? "varchar" : "varbinary") :
			      (has_charset() ? "char" : "binary")),
                             (int) field_length / charset()->mbmaxlen);
  res.length(length);
  if ((thd->variables.sql_mode & (MODE_MYSQL323 | MODE_MYSQL40)) &&
      has_charset() && (charset()->state & MY_CS_BINSORT))
    res.append(STRING_WITH_LEN(" binary"));
}


uchar *Field_string::pack(uchar *to, const uchar *from,
                          uint max_length,
                          bool low_byte_first __attribute__((unused)))
{
  uint length=      min(field_length,max_length);
  uint local_char_length= max_length/field_charset->mbmaxlen;
  if (length > local_char_length)
    local_char_length= my_charpos(field_charset, from, from+length,
                                  local_char_length);
  set_if_smaller(length, local_char_length);
  while (length && from[length-1] == field_charset->pad_char)
    length--;

  // Length always stored little-endian
  *to++= (uchar) length;
  if (field_length > 255)
    *to++= (uchar) (length >> 8);

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
const uchar *
Field_string::unpack(uchar *to,
                     const uchar *from,
                     uint param_data,
                     bool low_byte_first __attribute__((unused)))
{
  uint from_length=
    param_data ? min(param_data & 0x00ff, field_length) : field_length;
  uint length;

  if (from_length > 255)
  {
    length= uint2korr(from);
    from+= 2;
  }
  else
    length= (uint) *from++;

  memcpy(to, from, length);
  // Pad the string with the pad character of the fields charset
  bfill(to + length, field_length - length, field_charset->pad_char);
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
int Field_string::do_save_field_metadata(uchar *metadata_ptr)
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

int Field_string::pack_cmp(const uchar *a, const uchar *b, uint length,
                           my_bool insert_or_update)
{
  uint a_length, b_length;
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

int Field_string::pack_cmp(const uchar *key, uint length,
                           my_bool insert_or_update)
{
  uint row_length, local_key_length;
  uchar *end;
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


uint Field_string::packed_col_length(const uchar *data_ptr, uint length)
{
  if (length > 255)
    return uint2korr(data_ptr)+2;
  return (uint) *data_ptr + 1;
}


uint Field_string::max_packed_col_length(uint max_length)
{
  return (max_length > 255 ? 2 : 1)+max_length;
}


uint Field_string::get_key_image(uchar *buff,
                                 uint length,
                                 imagetype type_arg __attribute__((unused)))
{
  uint bytes = my_charpos(field_charset, (char*) ptr,
                          (char*) ptr + field_length,
                          length / field_charset->mbmaxlen);
  memcpy(buff, ptr, bytes);
  if (bytes < length)
    field_charset->cset->fill(field_charset, (char*) buff + bytes,
                              length - bytes, field_charset->pad_char);
  return bytes;
}


Field *Field_string::new_field(MEM_ROOT *root, struct st_table *new_table,
                               bool keep_type)
{
  Field *field;
  if (type() != FIELD_TYPE_VAR_STRING || keep_type)
    field= Field::new_field(root, new_table, keep_type);
  else if ((field= new Field_varstring(field_length, maybe_null(), field_name,
                                       new_table->s, charset())))
  {
    /*
      Old VARCHAR field which should be modified to a VARCHAR on copy
      This is done to ensure that ALTER TABLE will convert old VARCHAR fields
      to now VARCHAR fields.
    */
    field->init(new_table);
    /*
      Normally orig_table is different from table only if field was created
      via ::new_field.  Here we alter the type of field, so ::new_field is
      not applicable. But we still need to preserve the original field
      metadata for the client-server protocol.
    */
    field->orig_table= orig_table;
  }
  return field;
}


