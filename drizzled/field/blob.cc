/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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
#include <drizzled/field/blob.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <plugin/myisam/myisam.h>
#include <drizzled/system_variables.h>

#include <string>
#include <algorithm>

using namespace std;

namespace drizzled {

static uint32_t blob_pack_length_to_max_length(uint32_t arg)
{
  return max(UINT32_MAX,
             (uint32_t)((INT64_C(1) << min(arg, 4U) * 8) - INT64_C(1)));
}


/****************************************************************************
** blob type
** A blob is saved as a length and a pointer. The length is stored in the
** packlength slot and is sizeof(uint32_t) (4 bytes)
****************************************************************************/

Field_blob::Field_blob(unsigned char *ptr_arg,
                       unsigned char *null_ptr_arg,
                       unsigned char null_bit_arg,
		                   const char *field_name_arg,
                       TableShare *share,
                       const charset_info_st * const cs)
  :Field_str(ptr_arg,
             blob_pack_length_to_max_length(sizeof(uint32_t)),
             null_ptr_arg,
             null_bit_arg,
             field_name_arg,
             cs)
{
  flags|= BLOB_FLAG;
  share->blob_fields++;
  /* TODO: why do not fill table->getShare()->blob_field array here? */
}

void Field_blob::store_length(unsigned char *i_ptr,
                              uint32_t i_number,
                              bool low_byte_first)
{
#ifndef WORDS_BIGENDIAN
  (void)low_byte_first;
#endif

#ifdef WORDS_BIGENDIAN
  if (low_byte_first)
  {
    int4store(i_ptr,i_number);
  }
  else
#endif
    longstore(i_ptr,i_number);
}


void Field_blob::store_length(unsigned char *i_ptr, uint32_t i_number)
{
  store_length(i_ptr, i_number, getTable()->getShare()->db_low_byte_first);
}


uint32_t Field_blob::get_length(const unsigned char *pos,
                                bool low_byte_first) const
{
#ifndef WORDS_BIGENDIAN
  (void)low_byte_first;
#endif
  uint32_t tmp;
#ifdef WORDS_BIGENDIAN
  if (low_byte_first)
    tmp=uint4korr(pos);
  else
#endif
    longget(tmp,pos);
  return (uint32_t) tmp;
}


uint32_t Field_blob::get_packed_size(const unsigned char *ptr_arg,
                                bool low_byte_first)
{
  return sizeof(uint32_t) + get_length(ptr_arg, low_byte_first);
}


uint32_t Field_blob::get_length(uint32_t row_offset) const
{
  return get_length(ptr+row_offset,
                    getTable()->getShare()->db_low_byte_first);
}


uint32_t Field_blob::get_length(const unsigned char *ptr_arg) const
{
  return get_length(ptr_arg, getTable()->getShare()->db_low_byte_first);
}


/**
  Put a blob length field into a record buffer.

  Blob length is always stored in sizeof(uint32_t) (4 bytes)

  @param pos                 Pointer into the record buffer.
  @param length              The length value to put.
*/

void Field_blob::put_length(unsigned char *pos, uint32_t length)
{
    int4store(pos, length);
}


int Field_blob::store(const char *from,uint32_t length, const charset_info_st * const cs)
{
  uint32_t copy_length, new_length;
  const char *well_formed_error_pos;
  const char *cannot_convert_error_pos;
  const char *from_end_pos, *tmp;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmpstr(buff,sizeof(buff), &my_charset_bin);

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if (!length)
  {
    memset(ptr, 0, Field_blob::pack_length());
    return 0;
  }

  if (from == value.ptr())
  {
    size_t dummy_offset;
    if (!String::needs_conversion(length, cs, field_charset, &dummy_offset))
    {
      Field_blob::store_length(length);
      memmove(ptr+sizeof(uint32_t), &from, sizeof(char*));
      return 0;
    }
    tmpstr.copy(from, length, cs);
    from= tmpstr.ptr();
  }

  new_length= min(max_data_length(), field_charset->mbmaxlen * length);
  value.alloc(new_length);

  /*
    "length" is OK as "nchars" argument to well_formed_copy_nchars as this
    is never used to limit the length of the data. The cut of long data
    is done with the new_length value.
  */
  copy_length= well_formed_copy_nchars(field_charset,
                                       (char*) value.ptr(), new_length,
                                       cs, from, length,
                                       length,
                                       &well_formed_error_pos,
                                       &cannot_convert_error_pos,
                                       &from_end_pos);

  Field_blob::store_length(copy_length);
  tmp= value.ptr();
  memmove(ptr+sizeof(uint32_t), &tmp, sizeof(char*));

  if (check_string_copy_error(this, well_formed_error_pos,
                              cannot_convert_error_pos, from + length, cs))
    return 2;

  return report_if_important_data(from_end_pos, from + length);
}

int Field_blob::store(double nr)
{
  const charset_info_st * const cs=charset();
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  value.set_real(nr, NOT_FIXED_DEC, cs);
  return Field_blob::store(value.ptr(),(uint32_t) value.length(), cs);
}

int Field_blob::store(int64_t nr, bool unsigned_val)
{
  const charset_info_st * const cs=charset();
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  value.set_int(nr, unsigned_val, cs);
  return Field_blob::store(value.ptr(), (uint32_t) value.length(), cs);
}


double Field_blob::val_real(void) const
{
  int not_used;
  char *end_not_used, *blob;
  uint32_t length;
  const charset_info_st *cs;

  ASSERT_COLUMN_MARKED_FOR_READ;

  memcpy(&blob,ptr+sizeof(uint32_t),sizeof(char*));
  if (!blob)
    return 0.0;
  length= get_length(ptr);
  cs= charset();
  return my_strntod(cs, blob, length, &end_not_used, &not_used);
}


int64_t Field_blob::val_int(void) const
{
  int not_used;
  char *blob;

  ASSERT_COLUMN_MARKED_FOR_READ;

  memcpy(&blob,ptr+sizeof(uint32_t),sizeof(char*));
  if (!blob)
    return 0;
  uint32_t length= get_length(ptr);
  return my_strntoll(charset(),blob,length,10,NULL,&not_used);
}

String *Field_blob::val_str(String *, String *val_ptr) const
{
  char *blob;

  ASSERT_COLUMN_MARKED_FOR_READ;

  memcpy(&blob,ptr+sizeof(uint32_t),sizeof(char*));
  if (!blob)
    val_ptr->set("",0,charset());	// A bit safer than ->length(0)
  else
    val_ptr->set((const char*) blob,get_length(ptr),charset());
  return val_ptr;
}


type::Decimal *Field_blob::val_decimal(type::Decimal *decimal_value) const
{
  const char *blob;
  size_t length;

  ASSERT_COLUMN_MARKED_FOR_READ;

  memcpy(&blob, ptr+sizeof(uint32_t), sizeof(const unsigned char*));
  if (!blob)
  {
    blob= "";
    length= 0;
  }
  else
  {
    length= get_length(ptr);
  }

  decimal_value->store(E_DEC_FATAL_ERROR, blob, length, charset());

  return decimal_value;
}


int Field_blob::cmp(const unsigned char *a,uint32_t a_length, const unsigned char *b,
		    uint32_t b_length)
{
  return field_charset->coll->strnncollsp(field_charset,
                                          a, a_length, b, b_length,
                                          0);
}


int Field_blob::cmp_max(const unsigned char *a_ptr, const unsigned char *b_ptr,
                        uint32_t max_length)
{
  unsigned char *blob1,*blob2;
  memcpy(&blob1,a_ptr+sizeof(uint32_t),sizeof(char*));
  memcpy(&blob2,b_ptr+sizeof(uint32_t),sizeof(char*));
  uint32_t a_len= get_length(a_ptr), b_len= get_length(b_ptr);
  set_if_smaller(a_len, max_length);
  set_if_smaller(b_len, max_length);
  return Field_blob::cmp(blob1,a_len,blob2,b_len);
}


int Field_blob::cmp_binary(const unsigned char *a_ptr, const unsigned char *b_ptr,
                           uint32_t max_length)
{
  char *a,*b;
  uint32_t diff;
  uint32_t a_length,b_length;
  memcpy(&a,a_ptr+sizeof(uint32_t),sizeof(char*));
  memcpy(&b,b_ptr+sizeof(uint32_t),sizeof(char*));

  a_length= get_length(a_ptr);

  if (a_length > max_length)
    a_length= max_length;

  b_length= get_length(b_ptr);

  if (b_length > max_length)
    b_length= max_length;

  diff= memcmp(a,b,min(a_length,b_length));

  return diff ? diff : (unsigned int) (a_length - b_length);
}

/* The following is used only when comparing a key */
uint32_t Field_blob::get_key_image(unsigned char *buff, uint32_t length)
{
  uint32_t blob_length= get_length(ptr);
  unsigned char *blob= get_ptr();
  uint32_t local_char_length= length / field_charset->mbmaxlen;
  local_char_length= my_charpos(field_charset, blob, blob + blob_length,
                          local_char_length);
  set_if_smaller(blob_length, local_char_length);

  if ((uint32_t) length > blob_length)
  {
    /*
      Must clear this as we do a memcmp in optimizer/range.cc to detect
      identical keys
    */
    memset(buff+HA_KEY_BLOB_LENGTH+blob_length, 0, (length-blob_length));
    length=(uint32_t) blob_length;
  }
  int2store(buff,length);
  memcpy(buff+HA_KEY_BLOB_LENGTH, blob, length);
  return HA_KEY_BLOB_LENGTH+length;
}

uint32_t Field_blob::get_key_image(basic_string<unsigned char> &buff, uint32_t length)
{
  uint32_t blob_length= get_length(ptr);
  unsigned char* blob= get_ptr();
  uint32_t local_char_length= length / field_charset->mbmaxlen;
  local_char_length= my_charpos(field_charset, blob, blob + blob_length,
                                local_char_length);
  set_if_smaller(blob_length, local_char_length);

  unsigned char len_buff[HA_KEY_BLOB_LENGTH];
  int2store(len_buff,length);
  buff.append(len_buff);
  buff.append(blob, blob_length);

  if (length > blob_length)
  {
    /*
      Must clear this as we do a memcmp in optimizer/range.cc to detect
      identical keys
    */

    buff.append(length-blob_length, '0');
  }
  return HA_KEY_BLOB_LENGTH+length;
}

void Field_blob::set_key_image(const unsigned char *buff,uint32_t length)
{
  length= uint2korr(buff);
  (void) Field_blob::store((const char*) buff+HA_KEY_BLOB_LENGTH, length, field_charset);
}

int Field_blob::key_cmp(const unsigned char *key_ptr, uint32_t max_key_length)
{
  unsigned char *blob1;
  uint32_t blob_length=get_length(ptr);
  memcpy(&blob1,ptr+sizeof(uint32_t),sizeof(char*));
  const charset_info_st * const cs= charset();
  uint32_t local_char_length= max_key_length / cs->mbmaxlen;
  local_char_length= my_charpos(cs, blob1, blob1+blob_length,
                                local_char_length);
  set_if_smaller(blob_length, local_char_length);
  return Field_blob::cmp(blob1, blob_length,
			 key_ptr+HA_KEY_BLOB_LENGTH,
			 uint2korr(key_ptr));
}

int Field_blob::key_cmp(const unsigned char *a,const unsigned char *b)
{
  return Field_blob::cmp(a+HA_KEY_BLOB_LENGTH, uint2korr(a),
			 b+HA_KEY_BLOB_LENGTH, uint2korr(b));
}

uint32_t Field_blob::sort_length() const
{
  return (uint32_t) (getTable()->getSession()->variables.max_sort_length +
                     (field_charset == &my_charset_bin ? 0 : sizeof(uint32_t)));
}

void Field_blob::sort_string(unsigned char *to,uint32_t length)
{
  unsigned char *blob;
  uint32_t blob_length=get_length();

  if (!blob_length)
    memset(to, 0, length);
  else
  {
    if (field_charset == &my_charset_bin)
    {
      unsigned char *pos;

      /*
        Store length of blob last in blob to shorter blobs before longer blobs
      */
      length-= sizeof(uint32_t); // size of stored blob length
      pos= to+length;

      mi_int4store(pos, blob_length);
    }
    memcpy(&blob,ptr+sizeof(uint32_t),sizeof(char*));

    blob_length=my_strnxfrm(field_charset,
                            to, length, blob, blob_length);
    assert(blob_length == length);
  }
}

uint32_t Field_blob::pack_length() const
{
  return (uint32_t) (sizeof(uint32_t) + portable_sizeof_char_ptr);
}

unsigned char *Field_blob::pack(unsigned char *to, const unsigned char *from,
                                uint32_t max_length, bool low_byte_first)
{
  unsigned char *save= ptr;
  ptr= (unsigned char*) from;
  uint32_t length= get_length();			// Length of from string

  /*
    Store max length, which will occupy packlength bytes. If the max
    length given is smaller than the actual length of the blob, we
    just store the initial bytes of the blob.
  */
  store_length(to, min(length, max_length), low_byte_first);

  /*
    Store the actual blob data, which will occupy 'length' bytes.
   */
  if (length > 0)
  {
    from= get_ptr();
    memcpy(to+sizeof(uint32_t), from,length);
  }

  ptr= save;					// Restore org row pointer
  return(to+sizeof(uint32_t)+length);
}

/**
   Unpack a blob field from row data.

   This method is used to unpack a blob field from a master whose size of
   the field is less than that of the slave. Note: This method is included
   to satisfy inheritance rules, but is not needed for blob fields. It
   simply is used as a pass-through to the original unpack() method for
   blob fields.

   @param   to         Destination of the data
   @param   from       Source of the data
   @param   param_data @c true if base types should be stored in little-
                       endian format, @c false if native format should
                       be used.

   @return  New pointer into memory based on from + length of the data
*/
const unsigned char *Field_blob::unpack(unsigned char *,
                                        const unsigned char *from,
                                        uint32_t,
                                        bool low_byte_first)
{
  uint32_t const length= get_length(from, low_byte_first);
  getTable()->setWriteSet(position());
  store(reinterpret_cast<const char*>(from) + sizeof(uint32_t),
        length, field_charset);
  return(from + sizeof(uint32_t) + length);
}

/** Create a packed key that will be used for storage from a MySQL row. */

unsigned char *
Field_blob::pack_key(unsigned char *to, const unsigned char *from, uint32_t max_length,
                     bool )
{
  unsigned char *save= ptr;
  ptr= (unsigned char*) from;
  uint32_t length=get_length();        // Length of from string
  uint32_t local_char_length= ((field_charset->mbmaxlen > 1) ?
                           max_length/field_charset->mbmaxlen : max_length);
  if (length)
    from= get_ptr();
  if (length > local_char_length)
    local_char_length= my_charpos(field_charset, from, from+length,
                                  local_char_length);
  set_if_smaller(length, local_char_length);
  *to++= (unsigned char) length;
  if (max_length > 255)				// 2 byte length
    *to++= (unsigned char) (length >> 8);
  memcpy(to, from, length);
  ptr=save;					// Restore org row pointer
  return to+length;
}


/**
  maximum possible display length for blob.

  @return
    length
*/

uint32_t Field_blob::max_display_length()
{
    return (uint32_t) 4294967295U;
}

} /* namespace drizzled */
