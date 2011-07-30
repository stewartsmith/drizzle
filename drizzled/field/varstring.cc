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
#include <drizzled/field/varstring.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <plugin/myisam/myisam.h>

#include <string>

using namespace std;

namespace drizzled
{

/****************************************************************************
  VARCHAR type
  Data in field->ptr is stored as:
    1 or 2 bytes length-prefix-header  (from Field_varstring::length_bytes)
    data

  NOTE:
  When VARCHAR is stored in a key (for handler::index_read() etc) it's always
  stored with a 2 byte prefix. (Just like blob keys).

  Normally length_bytes is calculated as (field_length < 256 : 1 ? 2)
  The exception is if there is a prefix key field that is part of a long
  VARCHAR, in which case field_length for this may be 1 but the length_bytes
  is 2.
****************************************************************************/

const uint32_t Field_varstring::MAX_SIZE= UINT16_MAX;

Field_varstring::Field_varstring(unsigned char *ptr_arg,
                                 uint32_t len_arg,
                                 uint32_t length_bytes_arg,
                                 unsigned char *null_ptr_arg,
                                 unsigned char null_bit_arg,
                                 const char *field_name_arg,
                                 const charset_info_st * const cs) :
  Field_str(ptr_arg,
            len_arg,
            null_ptr_arg,
            null_bit_arg,
            field_name_arg, cs),
length_bytes(length_bytes_arg)
{
}

Field_varstring::Field_varstring(uint32_t len_arg,
                                 bool maybe_null_arg,
                                 const char *field_name_arg,
                                 const charset_info_st * const cs) :
  Field_str((unsigned char*) 0,
            len_arg,
            maybe_null_arg ? (unsigned char*) "": 0,
            0,
            field_name_arg,
            cs),
  length_bytes(len_arg < 256 ? 1 :2)
{
}

int Field_varstring::store(const char *from,uint32_t length, const charset_info_st * const cs)
{
  uint32_t copy_length;
  const char *well_formed_error_pos;
  const char *cannot_convert_error_pos;
  const char *from_end_pos;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  copy_length= well_formed_copy_nchars(field_charset,
                                       (char*) ptr + length_bytes,
                                       field_length,
                                       cs, from, length,
                                       field_length / field_charset->mbmaxlen,
                                       &well_formed_error_pos,
                                       &cannot_convert_error_pos,
                                       &from_end_pos);

  if (length_bytes == 1)
    *ptr= (unsigned char) copy_length;
  else
    int2store(ptr, copy_length);

  if (check_string_copy_error(this, well_formed_error_pos,
                              cannot_convert_error_pos, from + length, cs))
    return 2;

  return report_if_important_data(from_end_pos, from + length);
}


int Field_varstring::store(int64_t nr, bool unsigned_val)
{
  char buff[64];
  uint32_t  length;
  length= (uint32_t) (field_charset->cset->int64_t10_to_str)(field_charset,
                                                             buff,
                                                             sizeof(buff),
                                                             (unsigned_val ? 10: -10),
                                                             nr);
  return Field_varstring::store(buff, length, field_charset);
}


double Field_varstring::val_real(void) const
{
  int not_used;
  char *end_not_used;

  ASSERT_COLUMN_MARKED_FOR_READ;

  uint32_t length= length_bytes == 1 ? (uint32_t) *ptr : uint2korr(ptr);

  return my_strntod(field_charset, (char*) ptr+length_bytes, length,
                    &end_not_used, &not_used);
}


int64_t Field_varstring::val_int(void) const
{
  int not_used;
  char *end_not_used;
  uint32_t length;

  ASSERT_COLUMN_MARKED_FOR_READ;

  length= length_bytes == 1 ? (uint32_t) *ptr : uint2korr(ptr);

  return my_strntoll(field_charset, (char*) ptr+length_bytes, length, 10,
                     &end_not_used, &not_used);
}

String *Field_varstring::val_str(String *, String *val_ptr) const
{
  uint32_t length=  length_bytes == 1 ? (uint32_t) *ptr : uint2korr(ptr);

  ASSERT_COLUMN_MARKED_FOR_READ;

  val_ptr->set((const char*) ptr+length_bytes, length, field_charset);

  return val_ptr;
}


type::Decimal *Field_varstring::val_decimal(type::Decimal *decimal_value) const
{
  uint32_t length;

  ASSERT_COLUMN_MARKED_FOR_READ;

  length= length_bytes == 1 ? (uint32_t) *ptr : uint2korr(ptr);

  decimal_value->store(E_DEC_FATAL_ERROR, (char*) ptr+length_bytes, length, charset());

  return decimal_value;
}


int Field_varstring::cmp_max(const unsigned char *a_ptr, const unsigned char *b_ptr,
                             uint32_t max_len)
{
  uint32_t a_length, b_length;
  int diff;

  if (length_bytes == 1)
  {
    a_length= (uint32_t) *a_ptr;
    b_length= (uint32_t) *b_ptr;
  }
  else
  {
    a_length= uint2korr(a_ptr);
    b_length= uint2korr(b_ptr);
  }
  set_if_smaller(a_length, max_len);
  set_if_smaller(b_length, max_len);
  diff= field_charset->coll->strnncollsp(field_charset,
                                         a_ptr+
                                         length_bytes,
                                         a_length,
                                         b_ptr+
                                         length_bytes,
                                         b_length,0);
  return diff;
}


/**
  @note
    varstring and blob keys are ALWAYS stored with a 2 byte length prefix
*/

int Field_varstring::key_cmp(const unsigned char *key_ptr, uint32_t max_key_length)
{
  uint32_t length=  length_bytes == 1 ? (uint32_t) *ptr : uint2korr(ptr);
  uint32_t local_char_length= max_key_length / field_charset->mbmaxlen;

  local_char_length= my_charpos(field_charset, ptr + length_bytes,
                                ptr + length_bytes + length, local_char_length);
  set_if_smaller(length, local_char_length);
  return field_charset->coll->strnncollsp(field_charset,
                                          ptr + length_bytes,
                                          length,
                                          key_ptr+
                                          HA_KEY_BLOB_LENGTH,
                                          uint2korr(key_ptr), 0);
}


/**
  Compare to key segments (always 2 byte length prefix).

  @note
    This is used only to compare key segments created for index_read().
    (keys are created and compared in key.cc)
*/

int Field_varstring::key_cmp(const unsigned char *a,const unsigned char *b)
{
  return field_charset->coll->strnncollsp(field_charset,
                                          a + HA_KEY_BLOB_LENGTH,
                                          uint2korr(a),
                                          b + HA_KEY_BLOB_LENGTH,
                                          uint2korr(b),
                                          0);
}


void Field_varstring::sort_string(unsigned char *to,uint32_t length)
{
  uint32_t tot_length=  length_bytes == 1 ? (uint32_t) *ptr : uint2korr(ptr);

  if (field_charset == &my_charset_bin)
  {
    /* Store length last in high-byte order to sort longer strings first */
    if (length_bytes == 1)
      to[length-1]= tot_length;
    else
      mi_int2store(to+length-2, tot_length);
    length-= length_bytes;
  }

  tot_length= my_strnxfrm(field_charset,
                          to, length, ptr + length_bytes,
                          tot_length);
  assert(tot_length == length);
}


enum ha_base_keytype Field_varstring::key_type() const
{
  enum ha_base_keytype res;

  if (binary())
    res= length_bytes == 1 ? HA_KEYTYPE_VARBINARY1 : HA_KEYTYPE_VARBINARY2;
  else
    res= length_bytes == 1 ? HA_KEYTYPE_VARTEXT1 : HA_KEYTYPE_VARTEXT2;
  return res;
}


uint32_t Field_varstring::used_length()
{
  return length_bytes == 1 ? 1 + (uint32_t) (unsigned char) *ptr : 2 + uint2korr(ptr);
}

/*
  Functions to create a packed row.
  Here the number of length bytes are depending on the given max_length
*/

unsigned char *Field_varstring::pack(unsigned char *to, const unsigned char *from,
                             uint32_t max_length,
                             bool )
{
  uint32_t length= length_bytes == 1 ? (uint32_t) *from : uint2korr(from);
  set_if_smaller(max_length, field_length);
  if (length > max_length)
    length=max_length;

  /* Length always stored little-endian */
  *to++= length & 0xFF;
  if (max_length > 255)
    *to++= (length >> 8) & 0xFF;

  /* Store bytes of string */
  if (length > 0)
    memcpy(to, from+length_bytes, length);
  return to+length;
}


/**
   Unpack a varstring field from row data.

   This method is used to unpack a varstring field from a master
   whose size of the field is less than that of the slave.

   @note
   The string length is always packed little-endian.

   @param   to         Destination of the data
   @param   from       Source of the data
   @param   param_data Length bytes from the master's field data

   @return  New pointer into memory based on from + length of the data
*/
const unsigned char *
Field_varstring::unpack(unsigned char *to, const unsigned char *from,
                        uint32_t param_data,
                        bool )
{
  uint32_t length;
  uint32_t l_bytes= (param_data && (param_data < field_length)) ?
                (param_data <= 255) ? 1 : 2 : length_bytes;
  if (l_bytes == 1)
  {
    to[0]= *from++;
    length= to[0];
    if (length_bytes == 2)
      to[1]= 0;
  }
  else /* l_bytes == 2 */
  {
    length= uint2korr(from);
    to[0]= *from++;
    to[1]= *from++;
  }
  if (length)
    memcpy(to+ length_bytes, from, length);
  return from+length;
}


uint32_t Field_varstring::max_packed_col_length(uint32_t max_length)
{
  return (max_length > 255 ? 2 : 1)+max_length;
}

uint32_t Field_varstring::get_key_image(basic_string<unsigned char> &buff, uint32_t length)
{
  /* Key is always stored with 2 bytes */
  const uint32_t key_len= 2;
  uint32_t f_length=  length_bytes == 1 ? (uint32_t) *ptr : uint2korr(ptr);
  uint32_t local_char_length= length / field_charset->mbmaxlen;
  unsigned char *pos= ptr+length_bytes;
  local_char_length= my_charpos(field_charset, pos, pos + f_length,
                                local_char_length);
  set_if_smaller(f_length, local_char_length);
  unsigned char len_buff[key_len];
  int2store(len_buff,f_length);
  buff.append(len_buff);
  buff.append(pos, f_length);
  if (f_length < length)
  {
    /*
      Must clear this as we do a memcmp in optimizer/range.cc to detect
      identical keys
    */
    buff.append(length-f_length, 0);
  }
  return key_len+f_length;
}


uint32_t Field_varstring::get_key_image(unsigned char *buff, uint32_t length)
{
  uint32_t f_length=  length_bytes == 1 ? (uint32_t) *ptr : uint2korr(ptr);
  uint32_t local_char_length= length / field_charset->mbmaxlen;
  unsigned char *pos= ptr+length_bytes;
  local_char_length= my_charpos(field_charset, pos, pos + f_length,
                                local_char_length);
  set_if_smaller(f_length, local_char_length);
  /* Key is always stored with 2 bytes */
  int2store(buff,f_length);
  memcpy(buff+HA_KEY_BLOB_LENGTH, pos, f_length);
  if (f_length < length)
  {
    /*
      Must clear this as we do a memcmp in optimizer/range.cc to detect
      identical keys
    */
    memset(buff+HA_KEY_BLOB_LENGTH+f_length, 0, (length-f_length));
  }
  return HA_KEY_BLOB_LENGTH+f_length;
}

void Field_varstring::set_key_image(const unsigned char *buff, uint32_t length)
{
  length= uint2korr(buff);			// Real length is here
  (void) Field_varstring::store((const char*) buff+HA_KEY_BLOB_LENGTH, length, field_charset);
}

int Field_varstring::cmp_binary(const unsigned char *a_ptr,
                                const unsigned char *b_ptr,
                                uint32_t max_length)
{
  uint32_t a_length,b_length;

  if (length_bytes == 1)
  {
    a_length= (uint32_t) *a_ptr;
    b_length= (uint32_t) *b_ptr;
  }
  else
  {
    a_length= uint2korr(a_ptr);
    b_length= uint2korr(b_ptr);
  }
  set_if_smaller(a_length, max_length);
  set_if_smaller(b_length, max_length);
  if (a_length != b_length)
    return 1;
  return memcmp(a_ptr+length_bytes, b_ptr+length_bytes, a_length);
}


Field *Field_varstring::new_field(memory::Root *root, Table *new_table, bool keep_type)
{
  Field_varstring *res= (Field_varstring*) Field::new_field(root, new_table,
                                                            keep_type);
  if (res)
    res->length_bytes= length_bytes;
  return res;
}


Field *Field_varstring::new_key_field(memory::Root *root,
                                      Table *new_table,
                                      unsigned char *new_ptr, unsigned char *new_null_ptr,
                                      uint32_t new_null_bit)
{
  Field_varstring *res;
  if ((res= (Field_varstring*) Field::new_key_field(root,
                                                    new_table,
                                                    new_ptr,
                                                    new_null_ptr,
                                                    new_null_bit)))
  {
    /* Keys length prefixes are always packed with 2 bytes */
    res->length_bytes= 2;
  }
  return res;
}

} /* namespace drizzled */
