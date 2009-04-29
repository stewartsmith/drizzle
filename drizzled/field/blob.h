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

#ifndef DRIZZLE_SERVER_FIELD_BLOB
#define DRIZZLE_SERVER_FIELD_BLOB

#include <drizzled/field/longstr.h>

#include <string>

class Field_blob :public Field_longstr {
protected:
  uint32_t packlength;
  String value;				// For temporaries
public:

  using Field::store;
  using Field::cmp;
  using Field::pack;
  using Field::unpack;
  using Field::val_int;
  using Field::val_str;


  Field_blob(unsigned char *ptr_arg, unsigned char *null_ptr_arg, unsigned char null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     TableShare *share, uint32_t blob_pack_length, const CHARSET_INFO * const cs);
  Field_blob(uint32_t len_arg, bool maybe_null_arg, const char *field_name_arg,
             const CHARSET_INFO * const cs)
    :Field_longstr((unsigned char*) 0, len_arg, maybe_null_arg ? (unsigned char*) "": 0, 0,
                   NONE, field_name_arg, cs),
    packlength(4)
  {
    flags|= BLOB_FLAG;
  }
  Field_blob(uint32_t len_arg, bool maybe_null_arg, const char *field_name_arg,
	     const CHARSET_INFO * const cs, bool set_packlength)
    :Field_longstr((unsigned char*) 0,len_arg, maybe_null_arg ? (unsigned char*) "": 0, 0,
                   NONE, field_name_arg, cs)
  {
    flags|= BLOB_FLAG;
    packlength= 4;
    if (set_packlength)
    {
      uint32_t l_char_length= len_arg/cs->mbmaxlen;
      packlength= l_char_length <= 255 ? 1 :
                  l_char_length <= 65535 ? 2 :
                  l_char_length <= 16777215 ? 3 : 4;
    }
  }
  Field_blob(uint32_t packlength_arg)
    :Field_longstr((unsigned char*) 0, 0, (unsigned char*) "", 0, NONE, "temp", system_charset_info),
    packlength(packlength_arg) {}
  enum_field_types type() const { return DRIZZLE_TYPE_BLOB;}
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2; }
  int  store(const char *to,uint32_t length,
             const CHARSET_INFO * const charset);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);

  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
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
  { return (uint32_t) (packlength); }
  uint32_t row_pack_length() { return pack_length_no_ptr(); }
  uint32_t sort_length() const;
  virtual uint32_t max_data_length() const
  {
    return (uint32_t) (((uint64_t) 1 << (packlength*8)) -1);
  }
  int reset(void) { memset(ptr, 0, packlength+sizeof(unsigned char*)); return 0; }
  void reset_fields() { memset(&value, 0, sizeof(value)); }
#ifndef WORDS_BIGENDIAN
  static
#endif
  void store_length(unsigned char *i_ptr, uint32_t i_packlength,
                    uint32_t i_number, bool low_byte_first);
  void store_length(unsigned char *i_ptr, uint32_t i_packlength,
                    uint32_t i_number);

  inline void store_length(uint32_t number)
  {
    store_length(ptr, packlength, number);
  }

  /**
     Return the packed length plus the length of the data.

     This is used to determine the size of the data plus the
     packed length portion in the row data.

     @returns The length in the row plus the size of the data.
  */
  uint32_t get_packed_size(const unsigned char *ptr_arg, bool low_byte_first);

  uint32_t get_length(uint32_t row_offset= 0);
  uint32_t get_length(const unsigned char *ptr, uint32_t packlength,
                      bool low_byte_first);
  uint32_t get_length(const unsigned char *ptr_arg);
  void put_length(unsigned char *pos, uint32_t length);
  inline void get_ptr(unsigned char **str)
    {
      memcpy(str,ptr+packlength,sizeof(unsigned char*));
    }
  inline void get_ptr(unsigned char **str, uint32_t row_offset)
    {
      memcpy(str,ptr+packlength+row_offset,sizeof(char*));
    }
  inline void set_ptr(unsigned char *length, unsigned char *data)
    {
      memcpy(ptr,length,packlength);
      memcpy(ptr+packlength,&data,sizeof(char*));
    }
  void set_ptr_offset(my_ptrdiff_t ptr_diff, uint32_t length, unsigned char *data)
    {
      unsigned char *ptr_ofs= ADD_TO_PTR(ptr,ptr_diff,unsigned char*);
      store_length(ptr_ofs, packlength, length);
      memcpy(ptr_ofs+packlength,&data,sizeof(char*));
    }
  inline void set_ptr(uint32_t length, unsigned char *data)
    {
      set_ptr_offset(0, length, data);
    }
  uint32_t get_key_image(unsigned char *buff,uint32_t length, imagetype type);
  uint32_t get_key_image(std::basic_string<unsigned char> &buff,
                        uint32_t length, imagetype type);
  void set_key_image(const unsigned char *buff,uint32_t length);
  void sql_type(String &str) const;
  inline bool copy()
  {
    unsigned char *tmp;
    get_ptr(&tmp);
    if (value.copy((char*) tmp, get_length(), charset()))
    {
      Field_blob::reset();
      return 1;
    }
    tmp=(unsigned char*) value.ptr();
    memcpy(ptr+packlength,&tmp,sizeof(char*));
    return 0;
  }
  virtual unsigned char *pack(unsigned char *to, const unsigned char *from,
                      uint32_t max_length, bool low_byte_first);
  unsigned char *pack_key(unsigned char *to, const unsigned char *from,
                  uint32_t max_length, bool low_byte_first);
  virtual const unsigned char *unpack(unsigned char *to, const unsigned char *from,
                              uint32_t param_data, bool low_byte_first);
  void free() { value.free(); }
  inline void clear_temporary() { memset(&value, 0, sizeof(value)); }
  friend int field_conv(Field *to,Field *from);
  uint32_t size_of() const { return sizeof(*this); }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? false : true; }
  uint32_t max_display_length();
  uint32_t is_equal(Create_field *new_field);
  bool in_read_set();
  bool in_write_set();
private:
  int do_save_field_metadata(unsigned char *first_byte);
};

#endif

