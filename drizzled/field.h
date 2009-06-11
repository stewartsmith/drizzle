/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

/*
  Because of the function new_field() all field classes that have static
  variables must declare the size_of() member function.
*/

#ifndef DRIZZLED_FIELD_H
#define DRIZZLED_FIELD_H

#include "drizzled/sql_error.h"
#include "drizzled/my_decimal.h"
#include "drizzled/key_map.h"
#include "drizzled/sql_bitmap.h"
#include "drizzled/sql_list.h"
#include "drizzled/structs.h"
#include "drizzled/table.h"

#include <string>

#define DATETIME_DEC                     6
#define DOUBLE_TO_STRING_CONVERSION_BUFFER_SIZE FLOATING_POINT_BUFFER

const uint32_t max_field_size= (uint32_t) 4294967295U;

class Send_field;
class Protocol;
class Create_field;
class TableShare;
class Field;
struct st_cache_field;

int field_conv(Field *to,Field *from);

inline uint32_t get_enum_pack_length(int elements)
{
  return elements < 256 ? 1 : 2;
}

/**
 * Class representing a Field in a Table
 *
 * @details
 *
 * The value stored in the Field object is stored in the 
 * unsigned char pointer member variable called ptr.  The
 * val_xxx() methods retrieve this raw byte value and 
 * convert the byte into the appropriate output (int, decimal, etc).
 *
 * The store_xxx() methods take various input and convert
 * the input into the raw bytes stored in the ptr member variable.
 */
class Field
{
  /* Prevent use of these */
  Field(const Item &); 
  void operator=(Field &);
public:
  unsigned char *ptr; /**< Position to field in record. Stores raw field value */
  unsigned char *null_ptr; /**< Byte where null_bit is */

  /**
   * Pointer to the Table object containing this Field 
   *
   * @note You can use table->in_use as replacement for current_session member
   * only inside of val_*() and store() members (e.g. you can't use it in cons)
   */
  Table *table; 
  Table *orig_table; /**< Pointer to the original Table. @TODO What is "the original table"? */
  const char **table_name; /**< Pointer to the name of the table. @TODO This is redundant with Table::table_name. */
  const char *field_name; /**< Name of the field */
  LEX_STRING comment; /**< A comment about the field */

  /** The field is part of the following keys */
  key_map	key_start;
  key_map part_of_key;
  key_map part_of_key_not_clustered;
  key_map part_of_sortkey;

  /*
    We use three additional unireg types for TIMESTAMP to overcome limitation
    of current binary format of .frm file. We'd like to be able to support
    NOW() as default and on update value for such fields but unable to hold
    this info anywhere except unireg_check field. This issue will be resolved
    in more clean way with transition to new text based .frm format.
    See also comment for Field_timestamp::Field_timestamp().
  */
  enum utype
  { 
    NONE,
    NEXT_NUMBER,
    TIMESTAMP_OLD_FIELD,
    TIMESTAMP_DN_FIELD,
    TIMESTAMP_UN_FIELD,
    TIMESTAMP_DNUN_FIELD
  };

  utype	unireg_check;
  uint32_t field_length; /**< Length of this field in bytes */
  uint32_t flags;
  uint16_t field_index; /**< Index of this Field in Table::fields array */
  unsigned char null_bit; /**< Bit used to test null bit */
  /**
     If true, this field was created in create_tmp_field_from_item from a NULL
     value. This means that the type of the field is just a guess, and the type
     may be freely coerced to another type.

     @see create_tmp_field_from_item
     @see Item_type_holder::get_real_type
   */
  bool is_created_from_null_item;

  static void *operator new(size_t size) {return sql_alloc(size); }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint32_t) size); }
  static void operator delete(void *, size_t)
  { TRASH(ptr_arg, size); }

  Field(unsigned char *ptr_arg,
        uint32_t length_arg,
        unsigned char *null_ptr_arg,
        unsigned char null_bit_arg,
        utype unireg_check_arg,
        const char *field_name_arg);
  virtual ~Field() {}
  /* Store functions returns 1 on overflow and -1 on fatal error */
  virtual int store(const char *to, 
                    uint32_t length, 
                    const CHARSET_INFO * const cs)=0;
  virtual int store(double nr)=0;
  virtual int store(int64_t nr, bool unsigned_val)=0;
  virtual int store_decimal(const my_decimal *d)=0;
  int store(const char *to, 
            uint32_t length,
            const CHARSET_INFO * const cs,
            enum_check_fields check_level);
  /**
    This is called when storing a date in a string.

    @note
      Needs to be changed if/when we want to support different time formats.
  */
  virtual int store_time(DRIZZLE_TIME *ltime, enum enum_drizzle_timestamp_type t_type);
  virtual double val_real(void)=0;
  virtual int64_t val_int(void)=0;
  virtual my_decimal *val_decimal(my_decimal *);
  inline String *val_str(String *str) 
  {
    return val_str(str, str);
  }
  /*
     val_str(buf1, buf2) gets two buffers and should use them as follows:
     if it needs a temp buffer to convert result to string - use buf1
       example Field_tiny::val_str()
     if the value exists as a string already - use buf2
       example Field_varstring::val_str() (???)
     consequently, buf2 may be created as 'String buf;' - no memory
     will be allocated for it. buf1 will be allocated to hold a
     value if it's too small. Using allocated buffer for buf2 may result in
     an unnecessary free (and later, may be an alloc).
     This trickery is used to decrease a number of malloc calls.
  */
  virtual String *val_str(String*, String *)=0;
  /**
   * Interpret field value as an integer but return the result as a string.
   *
   * This is used for printing bit_fields as numbers while debugging.
   */
  String *val_int_as_str(String *val_buffer, bool unsigned_flag);
  /*
   str_needs_quotes() returns true if the value returned by val_str() needs
   to be quoted when used in constructing an SQL query.
  */
  virtual bool str_needs_quotes() { return false; }
  virtual Item_result result_type () const=0;
  virtual Item_result cmp_type () const { return result_type(); }
  virtual Item_result cast_to_int_type () const { return result_type(); }
  /**
     Check whether a field type can be partially indexed by a key.

     This is a static method, rather than a virtual function, because we need
     to check the type of a non-Field in mysql_alter_table().

     @param type  field type

     @retval
       true  Type can have a prefixed key
     @retval
       false Type can not have a prefixed key
  */
  static bool type_can_have_key_part(enum_field_types);
  static enum_field_types field_type_merge(enum_field_types, enum_field_types);

  /**
     Detect Item_result by given field type of UNION merge result.

     @param field_type  given field type

     @return
       Item_result (type of internal MySQL expression result)
  */
  static Item_result result_merge_type(enum_field_types);

  virtual bool eq(Field *field);
  /**
    @retval
      1  if the fields are equally defined
    @retval
      0  if the fields are unequally defined
  */
  virtual bool eq_def(Field *field);

  /*
    pack_length() returns size (in bytes) used to store field data in memory
    (i.e. it returns the maximum size of the field in a row of the table,
    which is located in RAM).
  */
  virtual uint32_t pack_length() const;

  /*
    pack_length_in_rec() returns size (in bytes) used to store field data on
    storage (i.e. it returns the maximal size of the field in a row of the
    table, which is located on disk).
  */
  virtual uint32_t pack_length_in_rec() const;
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
  virtual int compatible_field_size(uint32_t field_metadata);
  virtual uint32_t pack_length_from_metadata(uint32_t field_metadata);

  /*
    This method is used to return the size of the data in a row-based
    replication row record. The default implementation of returning 0 is
    designed to allow fields that do not use metadata to return true (1)
    from compatible_field_size() which uses this function in the comparison.
    The default value for field metadata for fields that do not have
    metadata is 0. Thus, 0 == 0 means the fields are compatible in size.

    Note: While most classes that override this method return pack_length(),
    the classes Field_varstring, and Field_blob return
    field_length + 1, field_length, and pack_length_no_ptr() respectfully.
  */
  virtual uint32_t row_pack_length();
  virtual int save_field_metadata(unsigned char *first_byte);

  /*
    data_length() return the "real size" of the data in memory.
    For varstrings, this does _not_ include the length bytes.
  */
  virtual uint32_t data_length();
  /*
    used_length() returns the number of bytes actually used to store the data
    of the field. So for a varstring it includes both lenght byte(s) and
    string data, and anything after data_length() bytes are unused.
  */
  virtual uint32_t used_length();
  virtual uint32_t sort_length() const;

  /**
     Get the maximum size of the data in packed format.

     @return Maximum data length of the field when packed using the
     Field::pack() function.
   */
  virtual uint32_t max_data_length() const;
  virtual int reset(void);
  virtual void reset_fields();
  virtual void set_default();
  virtual bool binary() const;
  virtual bool zero_pack() const;
  virtual enum ha_base_keytype key_type() const;
  virtual uint32_t key_length() const;
  virtual enum_field_types type() const =0;
  virtual enum_field_types real_type() const;
  inline  int cmp(const unsigned char *str) { return cmp(ptr,str); }
  virtual int cmp_max(const unsigned char *a, const unsigned char *b,
                      uint32_t max_len);
  virtual int cmp(const unsigned char *,const unsigned char *)=0;
  virtual int cmp_binary(const unsigned char *a,const unsigned char *b,
                         uint32_t max_length=UINT32_MAX);
  virtual int cmp_offset(uint32_t row_offset);
  virtual int cmp_binary_offset(uint32_t row_offset);
  virtual int key_cmp(const unsigned char *a,const unsigned char *b);
  virtual int key_cmp(const unsigned char *str, uint32_t length);
  virtual uint32_t decimals() const;

  /*
    Caller beware: sql_type can change str.Ptr, so check
    ptr() to see if it changed if you are using your own buffer
    in str and restore it with set() if needed
  */
  virtual void sql_type(String &str) const =0;

  // For new field
  virtual uint32_t size_of() const =0;

  bool is_null(my_ptrdiff_t row_offset= 0);
  bool is_real_null(my_ptrdiff_t row_offset= 0);
  bool is_null_in_record(const unsigned char *record);
  bool is_null_in_record_with_offset(my_ptrdiff_t offset);
  void set_null(my_ptrdiff_t row_offset= 0);
  void set_notnull(my_ptrdiff_t row_offset= 0);
  bool maybe_null(void);
  bool real_maybe_null(void);

  virtual void make_field(Send_field *);
  virtual void sort_string(unsigned char *buff,uint32_t length)=0;
  virtual bool optimize_range(uint32_t idx, uint32_t part);
  /*
    This should be true for fields which, when compared with constant
    items, can be casted to int64_t. In this case we will at 'fix_fields'
    stage cast the constant items to int64_ts and at the execution stage
    use field->val_int() for comparison.  Used to optimize clauses like
    'a_column BETWEEN date_const, date_const'.
  */
  virtual bool can_be_compared_as_int64_t() const { return false; }
  virtual void free() {}
  virtual Field *new_field(MEM_ROOT *root, Table *new_table,
                           bool keep_type);
  virtual Field *new_key_field(MEM_ROOT *root, Table *new_table,
                               unsigned char *new_ptr, unsigned char *new_null_ptr,
                               uint32_t new_null_bit);
  /** This is used to generate a field in Table from TableShare */
  Field *clone(MEM_ROOT *mem_root, Table *new_table);
  inline void move_field(unsigned char *ptr_arg,unsigned char *null_ptr_arg,unsigned char null_bit_arg)
  {
    ptr=ptr_arg; null_ptr=null_ptr_arg; null_bit=null_bit_arg;
  }
  inline void move_field(unsigned char *ptr_arg) { ptr=ptr_arg; }
  virtual void move_field_offset(my_ptrdiff_t ptr_diff)
  {
    ptr=ADD_TO_PTR(ptr,ptr_diff, unsigned char*);
    if (null_ptr)
      null_ptr=ADD_TO_PTR(null_ptr,ptr_diff,unsigned char*);
  }
  virtual void get_image(unsigned char *buff, uint32_t length, const CHARSET_INFO * const)
  {
    memcpy(buff,ptr,length);
  }
  virtual void get_image(std::basic_string<unsigned char> &buff, uint32_t length, const CHARSET_INFO * const)
  {
    buff.append(ptr,length);
  }
  virtual void set_image(const unsigned char *buff,uint32_t length, const CHARSET_INFO * const)
  {
    memcpy(ptr,buff,length);
  }


  /*
    Copy a field part into an output buffer.

    SYNOPSIS
      Field::get_key_image()
      buff   [out] output buffer
      length       output buffer size

    DESCRIPTION
      This function makes a copy of field part of size equal to or
      less than "length" parameter value.
      For fields of string types (CHAR, VARCHAR, TEXT) the rest of buffer
      is padded by zero byte.

    NOTES
      For variable length character fields (i.e. UTF-8) the "length"
      parameter means a number of output buffer bytes as if all field
      characters have maximal possible size (mbmaxlen). In the other words,
      "length" parameter is a number of characters multiplied by
      field_charset->mbmaxlen.

    RETURN
      Number of copied bytes (excluding padded zero bytes -- see above).
  */
  virtual uint32_t get_key_image(unsigned char *buff, uint32_t length)
  {
    get_image(buff, length, &my_charset_bin);
    return length;
  }
  virtual uint32_t get_key_image(std::basic_string<unsigned char> &buff, uint32_t length)
  {
    get_image(buff, length, &my_charset_bin);
    return length;
  }
  virtual void set_key_image(const unsigned char *buff,uint32_t length)
  {
    set_image(buff,length, &my_charset_bin);
  }
  inline int64_t val_int_offset(uint32_t row_offset)
  {
    ptr+=row_offset;
    int64_t tmp=val_int();
    ptr-=row_offset;
    return tmp;
  }

  inline int64_t val_int(const unsigned char *new_ptr)
  {
    unsigned char *old_ptr= ptr;
    int64_t return_value;
    ptr= (unsigned char*) new_ptr;
    return_value= val_int();
    ptr= old_ptr;
    return return_value;
  }
  inline String *val_str(String *str, const unsigned char *new_ptr)
  {
    unsigned char *old_ptr= ptr;
    ptr= (unsigned char*) new_ptr;
    val_str(str);
    ptr= old_ptr;
    return str;
  }

  /**
    Pack the field into a format suitable for storage and transfer.

    To implement packing functionality, only the virtual function
    should be overridden. The other functions are just convenience
    functions and hence should not be overridden.

    The value of <code>low_byte_first</code> is dependent on how the
    packed data is going to be used: for local use, e.g., temporary
    store on disk or in memory, use the native format since that is
    faster. For data that is going to be transfered to other machines
    (e.g., when writing data to the binary log), data should always be
    stored in little-endian format.

    @note The default method for packing fields just copy the raw bytes
    of the record into the destination, but never more than
    <code>max_length</code> characters.

    @param to
    Pointer to memory area where representation of field should be put.

    @param from
    Pointer to memory area where record representation of field is
    stored.

    @param max_length
    Maximum length of the field, as given in the column definition. For
    example, for <code>CHAR(1000)</code>, the <code>max_length</code>
    is 1000. This information is sometimes needed to decide how to pack
    the data.

    @param low_byte_first
    @c true if integers should be stored little-endian, @c false if
    native format should be used. Note that for little-endian machines,
    the value of this flag is a moot point since the native format is
    little-endian.
  */
  virtual unsigned char *pack(unsigned char *to,
                              const unsigned char *from,
                              uint32_t max_length,
                              bool low_byte_first);

  unsigned char *pack(unsigned char *to, const unsigned char *from);

  /**
    Unpack a field from row data.

    This method is used to unpack a field from a master whose size of
    the field is less than that of the slave.

    The <code>param_data</code> parameter is a two-byte integer (stored
    in the least significant 16 bits of the unsigned integer) usually
    consisting of two parts: the real type in the most significant byte
    and a original pack length in the least significant byte.

    The exact layout of the <code>param_data</code> field is given by
    the <code>Table_map_log_event::save_field_metadata()</code>.

    This is the default method for unpacking a field. It just copies
    the memory block in byte order (of original pack length bytes or
    length of field, whichever is smaller).

    @param   to         Destination of the data
    @param   from       Source of the data
    @param   param_data Real type and original pack length of the field
                        data

    @param low_byte_first
    If this flag is @c true, all composite entities (e.g., lengths)
    should be unpacked in little-endian format; otherwise, the entities
    are unpacked in native order.

    @return  New pointer into memory based on from + length of the data
  */
  virtual const unsigned char *unpack(unsigned char* to,
                                      const unsigned char *from,
                                      uint32_t param_data,
                                      bool low_byte_first);
  /**
     @overload Field::unpack(unsigned char*, const unsigned char*,
                             uint32_t, bool)
  */
  const unsigned char *unpack(unsigned char* to,
                              const unsigned char *from);

  virtual unsigned char *pack_key(unsigned char* to,
                                  const unsigned char *from,
                                  uint32_t max_length, 
                                  bool low_byte_first)
  {
    return pack(to, from, max_length, low_byte_first);
  }
  virtual unsigned char *pack_key_from_key_image(unsigned char* to,
                                                 const unsigned char *from,
                                                 uint32_t max_length,
                                                 bool low_byte_first)
  {
    return pack(to, from, max_length, low_byte_first);
  }
  virtual const unsigned char *unpack_key(unsigned char* to,
                                          const unsigned char *from,
                                          uint32_t max_length,
                                          bool low_byte_first)
  {
    return unpack(to, from, max_length, low_byte_first);
  }
  virtual uint32_t packed_col_length(const unsigned char *to, uint32_t length);
  virtual uint32_t max_packed_col_length(uint32_t max_length)
  {
    return max_length;
  }

  virtual int pack_cmp(const unsigned char *a,
                       const unsigned char *b,
                       uint32_t key_length_arg,
                       bool insert_or_update);
  virtual int pack_cmp(const unsigned char *b,
                       uint32_t key_length_arg,
                       bool insert_or_update);

  inline uint32_t offset(unsigned char *record)
  {
    return (uint32_t) (ptr - record);
  }
  void copy_from_tmp(int offset);
  uint32_t fill_cache_field(struct st_cache_field *copy);
  virtual bool get_date(DRIZZLE_TIME *ltime,uint32_t fuzzydate);
  virtual bool get_time(DRIZZLE_TIME *ltime);
  virtual const CHARSET_INFO *charset(void) const { return &my_charset_bin; }
  virtual const CHARSET_INFO *sort_charset(void) const { return charset(); }
  virtual bool has_charset(void) const { return false; }
  virtual void set_charset(const CHARSET_INFO * const)
  {}
  virtual enum Derivation derivation(void) const
  {
    return DERIVATION_IMPLICIT;
  }
  virtual void set_derivation(enum Derivation)
  {}
  /**
    Produce warning or note about data saved into field.

    @param level            - level of message (Note/Warning/Error)
    @param code             - error code of message to be produced
    @param cuted_increment  - whenever we should increase cut fields count or not

    @note
      This function won't produce warning and increase cut fields counter
      if count_cuted_fields == CHECK_FIELD_IGNORE for current thread.

      if count_cuted_fields == CHECK_FIELD_IGNORE then we ignore notes.
      This allows us to avoid notes in optimisation, like convert_constant_item().

    @retval
      1 if count_cuted_fields == CHECK_FIELD_IGNORE and error level is not NOTE
    @retval
      0 otherwise
  */
  bool set_warning(DRIZZLE_ERROR::enum_warning_level, 
                   unsigned int code,
                   int cuted_increment);
  /**
    Produce warning or note about datetime string data saved into field.

    @param level            level of message (Note/Warning/Error)
    @param code             error code of message to be produced
    @param str              string value which we tried to save
    @param str_length       length of string which we tried to save
    @param ts_type          type of datetime value (datetime/date/time)
    @param cuted_increment  whenever we should increase cut fields count or not

    @note
      This function will always produce some warning but won't increase cut
      fields counter if count_cuted_fields ==FIELD_CHECK_IGNORE for current
      thread.
  */
  void set_datetime_warning(DRIZZLE_ERROR::enum_warning_level, 
                            uint32_t code,
                            const char *str, 
                            uint32_t str_len,
                            enum enum_drizzle_timestamp_type ts_type, 
                            int cuted_increment);
  /**
    Produce warning or note about integer datetime value saved into field.

    @param level            level of message (Note/Warning/Error)
    @param code             error code of message to be produced
    @param nr               numeric value which we tried to save
    @param ts_type          type of datetime value (datetime/date/time)
    @param cuted_increment  whenever we should increase cut fields count or not

    @note
      This function will always produce some warning but won't increase cut
      fields counter if count_cuted_fields == FIELD_CHECK_IGNORE for current
      thread.
  */
  void set_datetime_warning(DRIZZLE_ERROR::enum_warning_level, 
                            uint32_t code,
                            int64_t nr, 
                            enum enum_drizzle_timestamp_type ts_type,
                            int cuted_increment);
  /**
    Produce warning or note about double datetime data saved into field.

    @param level            level of message (Note/Warning/Error)
    @param code             error code of message to be produced
    @param nr               double value which we tried to save
    @param ts_type          type of datetime value (datetime/date/time)

    @note
      This function will always produce some warning but won't increase cut
      fields counter if count_cuted_fields == FIELD_CHECK_IGNORE for current
      thread.
  */
  void set_datetime_warning(DRIZZLE_ERROR::enum_warning_level, 
                            const uint32_t code,
                            double nr, 
                            enum enum_drizzle_timestamp_type ts_type);
  inline bool check_overflow(int op_result)
  {
    return (op_result == E_DEC_OVERFLOW);
  }
  /**
    Process decimal library return codes and issue warnings for overflow and
    truncation.

    @param op_result  decimal library return code (E_DEC_* see include/decimal.h)

    @retval
      E_DEC_OVERFLOW   there was overflow
      E_DEC_TRUNCATED  there was truncation
    @retval
      0  no error or there was some other error except overflow or truncation
  */
  int warn_if_overflow(int op_result);
  void init(Table *table_arg);

  /* maximum possible display length */
  virtual uint32_t max_display_length()= 0;

  virtual uint32_t is_equal(Create_field *new_field);
  /**
    Conversion from decimal to int64_t with checking overflow and
    setting correct value (min/max) in case of overflow.

    @param val             value which have to be converted
    @param unsigned_flag   type of integer in which we convert val
    @param err             variable to pass error code

    @return
      value converted from val
  */
  int64_t convert_decimal2int64_t(const my_decimal *val, 
                                  bool unsigned_flag,
                                  int *err);
  /* The max. number of characters */
  inline uint32_t char_length() const
  {
    return field_length / charset()->mbmaxlen;
  }

  inline enum column_format_type column_format() const
  {
    return (enum column_format_type)
      ((flags >> COLUMN_FORMAT_FLAGS) & COLUMN_FORMAT_MASK);
  }

  /* Hash value */
  virtual void hash(uint32_t *nr, uint32_t *nr2);
  friend bool reopen_table(Session *,Table *,bool);
  friend class Copy_field;
  friend class Item_avg_field;
  friend class Item_std_field;
  friend class Item_sum_num;
  friend class Item_sum_sum;
  friend class Item_sum_str;
  friend class Item_sum_count;
  friend class Item_sum_avg;
  friend class Item_sum_std;
  friend class Item_sum_min;
  friend class Item_sum_max;
  friend class Item_func_group_concat;

  bool isReadSet();
  bool isWriteSet();

private:

  /**
    Retrieve the field metadata for fields.

    This default implementation returns 0 and saves 0 in the metadata_ptr
    value.

    @param   metadata_ptr   First byte of field metadata

    @returns 0 no bytes written.
  */
  virtual int do_save_field_metadata(unsigned char *)
  {
    return 0;
  }
};

/**
 * Class representing a field in a CREATE TABLE statement
 */
class Create_field :public Sql_alloc
{
public:
  const char *field_name;
  const char *change;			// If done with alter table
  const char *after;			// Put column after this one
  LEX_STRING comment;			// Comment for field
  Item	*def;				// Default value
  enum	enum_field_types sql_type;
  /*
    At various stages in execution this can be length of field in bytes or
    max number of characters.
  */
  uint32_t length;
  /*
    The value of `length' as set by parser: is the number of characters
    for most of the types, or of bytes for BLOBs or numeric types.
  */
  uint32_t char_length;
  uint32_t  decimals, flags, pack_length, key_length;
  Field::utype unireg_check;
  TYPELIB *interval;			// Which interval to use
  List<String> interval_list;
  const CHARSET_INFO *charset;
  Field *field;				// For alter table

  uint8_t       interval_id;	// For rea_create_table
  uint32_t	offset,pack_flag;

  Create_field() :after(0) {}
  Create_field(Field *field, Field *orig_field);
  /* Used to make a clone of this object for ALTER/CREATE TABLE */
  Create_field *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Create_field(*this); }
  void create_length_to_internal_length(void);

  inline enum column_format_type column_format() const
  {
    return (enum column_format_type)
      ((flags >> COLUMN_FORMAT_FLAGS) & COLUMN_FORMAT_MASK);
  }

  /* Init for a tmp table field. To be extended if need be. */
  void init_for_tmp_table(enum_field_types sql_type_arg,
                          uint32_t max_length, uint32_t decimals,
                          bool maybe_null, bool is_unsigned);

  /**
    Initialize field definition for create.

    @param session                   Thread handle
    @param fld_name              Field name
    @param fld_type              Field type
    @param fld_length            Field length
    @param fld_decimals          Decimal (if any)
    @param fld_type_modifier     Additional type information
    @param fld_default_value     Field default value (if any)
    @param fld_on_update_value   The value of ON UPDATE clause
    @param fld_comment           Field comment
    @param fld_change            Field change
    @param fld_interval_list     Interval list (if any)
    @param fld_charset           Field charset

    @retval
      false on success
    @retval
      true  on error
  */
  bool init(Session *session,
            char *field_name,
            enum_field_types type,
            char *length,
            char *decimals,
            uint32_t type_modifier,
            Item *default_value,
            Item *on_update_value,
            LEX_STRING *comment,
            char *change,
            List<String> *interval_list,
            const CHARSET_INFO * const cs,
            uint32_t uint_geom_type,
            enum column_format_type column_format);
};

/**
 * A class for sending field information to the client
 */
class Send_field 
{
public:
  const char *db_name;
  const char *table_name,*org_table_name;
  const char *col_name,*org_col_name;
  uint32_t length;
  uint32_t charsetnr, flags, decimals;
  enum_field_types type;
  Send_field() {}
};

/**
 * A class for quick copying data to fields
 */
class Copy_field :public Sql_alloc
{
  /**
    Convenience definition of a copy function returned by
    get_copy_func.
  */
  typedef void Copy_func(Copy_field*);
  Copy_func *get_copy_func(Field *to, Field *from);
public:
  unsigned char *from_ptr,*to_ptr;
  unsigned char *from_null_ptr,*to_null_ptr;
  bool *null_row;
  uint32_t	from_bit,to_bit;
  uint32_t from_length,to_length;
  Field *from_field,*to_field;
  String tmp;					// For items

  Copy_field() {}
  ~Copy_field() {}
  void set(Field *to,Field *from,bool save);	// Field to field
  void set(unsigned char *to,Field *from);		// Field to string
  void (*do_copy)(Copy_field *);
  void (*do_copy2)(Copy_field *);		// Used to handle null values
};

Field *make_field(TableShare *share,
                  MEM_ROOT *root,
                  unsigned char *ptr,
                  uint32_t field_length,
                  unsigned char *null_pos,
                  unsigned char null_bit,
                  uint32_t pack_flag,
                  enum_field_types field_type,
                  const CHARSET_INFO * cs,
                  Field::utype unireg_check,
                  TYPELIB *interval,
                  const char *field_name);

uint32_t pack_length_to_packflag(uint32_t type);
enum_field_types get_blob_type_from_length(uint32_t length);
uint32_t calc_pack_length(enum_field_types type,uint32_t length);
int set_field_to_null(Field *field);
int set_field_to_null_with_conversions(Field *field, bool no_conversions);


bool test_if_important_data(const CHARSET_INFO * const cs,
                            const char *str,
                            const char *strend);


#endif /* DRIZZLED_FIELD_H */
