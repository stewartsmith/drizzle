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

#pragma once

#include <drizzled/field/str.h>
#include <drizzled/charset.h>
#include <string>
#include <drizzled/visibility.h>

namespace drizzled {

/**
 * Class representing a BLOB data type column
 */
class DRIZZLED_API Field_blob :
  public Field_str
{
protected:
  String value;				// For temporaries
public:

  using Field::store;
  using Field::cmp;
  using Field::pack;
  using Field::unpack;
  using Field::val_int;
  using Field::val_str;

  Field_blob(unsigned char *ptr_arg,
             unsigned char *null_ptr_arg,
             unsigned char null_bit_arg,
             const char *field_name_arg,
             TableShare *share,
             const charset_info_st * const cs);
  Field_blob(uint32_t len_arg,
             bool maybe_null_arg,
             const char *field_name_arg,
             const charset_info_st * const cs)
    :Field_str((unsigned char*) NULL,
               len_arg,
               maybe_null_arg ? (unsigned char *) "": 0,
               0,
               field_name_arg,
               cs)
  {
    flags|= BLOB_FLAG;
  }

  enum_field_types type() const { return DRIZZLE_TYPE_BLOB;}
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2; }
  int  store(const char *to,uint32_t length,
             const charset_info_st * const charset);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);

  double val_real(void) const;
  int64_t val_int(void) const;
  String *val_str(String*,String *) const;
  type::Decimal *val_decimal(type::Decimal *) const;
  int cmp_max(const unsigned char *, const unsigned char *, uint32_t max_length);
  int cmp(const unsigned char *a,const unsigned char *b)
    { return cmp_max(a, b, UINT32_MAX); }
  int cmp(const unsigned char *a, uint32_t a_length, const unsigned char *b, uint32_t b_length);
  int cmp_binary(const unsigned char *a,const unsigned char *b, uint32_t max_length=UINT32_MAX);
  int key_cmp(const unsigned char *,const unsigned char*);
  int key_cmp(const unsigned char *str, uint32_t length);
  uint32_t key_length() const { return 0; }
  void sort_string(unsigned char *buff,uint32_t length);
  uint32_t pack_length() const;


  /**
     Return the packed length without the pointer size added.

     This is used to determine the size of the actual data in the row
     buffer.

     @returns The length of the raw data itself without the pointer.
  */
  uint32_t pack_length_no_ptr() const
  { return (uint32_t) (sizeof(uint32_t)); }

  uint32_t sort_length() const;
  virtual uint32_t max_data_length() const
  {
    return (uint32_t) (((uint64_t) 1 << 32) -1);
  }
  int reset(void) { memset(ptr, 0, sizeof(uint32_t)+sizeof(unsigned char*)); return 0; }
  void reset_fields() { memset(&value, 0, sizeof(value)); }
#ifndef WORDS_BIGENDIAN
  static
#endif
  void store_length(unsigned char *i_ptr, uint32_t i_number, bool low_byte_first);
  void store_length(unsigned char *i_ptr, uint32_t i_number);

  inline void store_length(uint32_t number)
  {
    store_length(ptr, number);
  }

  /**
     Return the packed length plus the length of the data.

     This is used to determine the size of the data plus the
     packed length portion in the row data.

     @returns The length in the row plus the size of the data.
  */
  uint32_t get_packed_size(const unsigned char *ptr_arg, bool low_byte_first);

  DRIZZLED_API uint32_t get_length(uint32_t row_offset= 0) const;
  DRIZZLED_API uint32_t get_length(const unsigned char *ptr, bool low_byte_first) const;
  DRIZZLED_API uint32_t get_length(const unsigned char *ptr_arg) const;
  void put_length(unsigned char *pos, uint32_t length);
  inline unsigned char* get_ptr() const
    {
      unsigned char* str;
      memcpy(&str, ptr + sizeof(uint32_t), sizeof(unsigned char*));
      return str;
    }
  inline void set_ptr(unsigned char *length, unsigned char *data)
    {
      memcpy(ptr,length,sizeof(uint32_t));
      memcpy(ptr+sizeof(uint32_t),&data,sizeof(char*));
    }
  void set_ptr_offset(ptrdiff_t ptr_diff, uint32_t length, unsigned char *data)
    {
      unsigned char *ptr_ofs= ADD_TO_PTR(ptr,ptr_diff,unsigned char*);
      store_length(ptr_ofs, length);
      memcpy(ptr_ofs+sizeof(uint32_t),&data,sizeof(char*));
    }
  inline void set_ptr(uint32_t length, unsigned char *data)
    {
      set_ptr_offset(0, length, data);
    }
  uint32_t get_key_image(unsigned char *buff,uint32_t length);
  uint32_t get_key_image(std::basic_string<unsigned char> &buff, uint32_t length);
  void set_key_image(const unsigned char *buff,uint32_t length);
  inline void copy()
  {
    unsigned char* tmp= get_ptr();
    value.copy((char*) tmp, get_length(), charset());
    tmp=(unsigned char*) value.ptr();
    memcpy(ptr+sizeof(uint32_t),&tmp,sizeof(char*));
  }
  virtual unsigned char *pack(unsigned char *to, const unsigned char *from,
                      uint32_t max_length, bool low_byte_first);
  unsigned char *pack_key(unsigned char *to, const unsigned char *from,
                  uint32_t max_length, bool low_byte_first);
  virtual const unsigned char *unpack(unsigned char *to, const unsigned char *from,
                              uint32_t , bool low_byte_first);
  void free() { value.free(); }
  inline void clear_temporary() { memset(&value, 0, sizeof(value)); }
  friend int field_conv(Field *to,Field *from);
  uint32_t size_of() const { return sizeof(*this); }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? false : true; }
  uint32_t max_display_length();
};

} /* namespace drizzled */


