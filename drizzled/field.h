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


#define DATETIME_DEC                     6
#define DOUBLE_TO_STRING_CONVERSION_BUFFER_SIZE FLOATING_POINT_BUFFER

const uint32_t max_field_size= (uint32_t) 4294967295U;

class Send_field;
class Protocol;
class Create_field;
struct st_cache_field;
int field_conv(Field *to,Field *from);

inline uint32_t get_enum_pack_length(int elements)
{
  return elements < 256 ? 1 : 2;
}

inline uint32_t get_set_pack_length(int elements)
{
  uint32_t len= (elements + 7) / 8;
  return len > 4 ? 8 : len;
}

class virtual_column_info: public Sql_alloc
{
public:
  Item *expr_item;
  LEX_STRING expr_str;
  Item *item_free_list;
  virtual_column_info() 
  : expr_item(0), item_free_list(0),
    field_type(DRIZZLE_TYPE_VIRTUAL),
    is_stored(false), data_inited(false)
  {
    expr_str.str= NULL;
    expr_str.length= 0;
  };
  ~virtual_column_info() {}
  enum_field_types get_real_type()
  {
    assert(data_inited);
    return data_inited ? field_type : DRIZZLE_TYPE_VIRTUAL;
  }
  void set_field_type(enum_field_types fld_type)
  {
    /* Calling this function can only be done once. */
    assert(not data_inited);
    data_inited= true;
    field_type= fld_type;
  }
  bool get_field_stored()
  {
    assert(data_inited);
    return data_inited ? is_stored : true;
  }
  void set_field_stored(bool stored)
  {
    is_stored= stored;
  }
private:
  /*
    The following data is only updated by the parser and read
    when a Create_field object is created/initialized.
  */
  enum_field_types field_type;   /* Real field type*/
  bool is_stored;             /* Indication that the field is 
                                    phisically stored in the database*/
  /*
    This flag is used to prevent other applications from
    reading and using incorrect data.
  */
  bool data_inited; 
};

class Field
{
  Field(const Item &);				/* Prevent use of these */
  void operator=(Field &);
public:
  static void *operator new(size_t size) {return sql_alloc(size); }
  static void operator delete(void *ptr_arg __attribute__((unused)),
                              size_t size __attribute__((unused)))
  { TRASH(ptr_arg, size); }

  unsigned char		*ptr;			// Position to field in record
  unsigned char		*null_ptr;		// Byte where null_bit is
  /*
    Note that you can use table->in_use as replacement for current_thd member 
    only inside of val_*() and store() members (e.g. you can't use it in cons)
  */
  Table *table;		// Pointer for table
  Table *orig_table;		// Pointer to original table
  const char	**table_name, *field_name;
  LEX_STRING	comment;
  /* Field is part of the following keys */
  key_map	key_start, part_of_key, part_of_key_not_clustered;
  key_map       part_of_sortkey;
  /* 
    We use three additional unireg types for TIMESTAMP to overcome limitation 
    of current binary format of .frm file. We'd like to be able to support 
    NOW() as default and on update value for such fields but unable to hold 
    this info anywhere except unireg_check field. This issue will be resolved
    in more clean way with transition to new text based .frm format.
    See also comment for Field_timestamp::Field_timestamp().
  */
  enum utype  { NONE,DATE,SHIELD,NOEMPTY,CASEUP,PNR,BGNR,PGNR,YES,NO,REL,
		CHECK,EMPTY,UNKNOWN_FIELD,CASEDN,NEXT_NUMBER,INTERVAL_FIELD,
                BIT_FIELD, TIMESTAMP_OLD_FIELD, CAPITALIZE, BLOB_FIELD,
                TIMESTAMP_DN_FIELD, TIMESTAMP_UN_FIELD, TIMESTAMP_DNUN_FIELD};
  enum imagetype { itRAW, itMBR};

  utype		unireg_check;
  uint32_t	field_length;		// Length of field
  uint32_t	flags;
  uint16_t        field_index;            // field number in fields array
  unsigned char		null_bit;		// Bit used to test null bit
  /**
     If true, this field was created in create_tmp_field_from_item from a NULL
     value. This means that the type of the field is just a guess, and the type
     may be freely coerced to another type.

     @see create_tmp_field_from_item
     @see Item_type_holder::get_real_type

   */
  bool is_created_from_null_item;

  /* Virtual column data */
  virtual_column_info *vcol_info;
  /*
    Indication that the field is phycically stored in tables 
    rather than just generated on SQL queries.
    As of now, false can only be set for generated-only virtual columns.
  */
  bool is_stored;

  Field(unsigned char *ptr_arg,uint32_t length_arg,unsigned char *null_ptr_arg,
        unsigned char null_bit_arg, utype unireg_check_arg,
        const char *field_name_arg);
  virtual ~Field() {}
  /* Store functions returns 1 on overflow and -1 on fatal error */
  virtual int  store(const char *to, uint32_t length, const CHARSET_INFO * const cs)=0;
  virtual int  store(double nr)=0;
  virtual int  store(int64_t nr, bool unsigned_val)=0;
  virtual int  store_decimal(const my_decimal *d)=0;
  virtual int store_time(DRIZZLE_TIME *ltime, enum enum_drizzle_timestamp_type t_type);
  int store(const char *to, uint32_t length, const CHARSET_INFO * const cs,
            enum_check_fields check_level);
  virtual double val_real(void)=0;
  virtual int64_t val_int(void)=0;
  virtual my_decimal *val_decimal(my_decimal *);
  inline String *val_str(String *str) { return val_str(str, str); }
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
  virtual String *val_str(String*,String *)=0;
  String *val_int_as_str(String *val_buffer, bool unsigned_flag);
  /*
   str_needs_quotes() returns true if the value returned by val_str() needs
   to be quoted when used in constructing an SQL query.
  */
  virtual bool str_needs_quotes() { return false; }
  virtual Item_result result_type () const=0;
  virtual Item_result cmp_type () const { return result_type(); }
  virtual Item_result cast_to_int_type () const { return result_type(); }
  static bool type_can_have_key_part(enum_field_types);
  static enum_field_types field_type_merge(enum_field_types, enum_field_types);
  static Item_result result_merge_type(enum_field_types);
  virtual bool eq(Field *field)
  {
    return (ptr == field->ptr && null_ptr == field->null_ptr &&
            null_bit == field->null_bit);
  }
  virtual bool eq_def(Field *field);
  
  /*
    pack_length() returns size (in bytes) used to store field data in memory
    (i.e. it returns the maximum size of the field in a row of the table,
    which is located in RAM).
  */
  virtual uint32_t pack_length() const { return (uint32_t) field_length; }

  /*
    pack_length_in_rec() returns size (in bytes) used to store field data on
    storage (i.e. it returns the maximal size of the field in a row of the
    table, which is located on disk).
  */
  virtual uint32_t pack_length_in_rec() const { return pack_length(); }
  virtual int compatible_field_size(uint32_t field_metadata);
  virtual uint32_t pack_length_from_metadata(uint32_t field_metadata)
  { return field_metadata; }
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
  virtual uint32_t row_pack_length() { return 0; }
  virtual int save_field_metadata(unsigned char *first_byte)
  { return do_save_field_metadata(first_byte); }

  /*
    data_length() return the "real size" of the data in memory.
    For varstrings, this does _not_ include the length bytes.
  */
  virtual uint32_t data_length() { return pack_length(); }
  /*
    used_length() returns the number of bytes actually used to store the data
    of the field. So for a varstring it includes both lenght byte(s) and
    string data, and anything after data_length() bytes are unused.
  */
  virtual uint32_t used_length() { return pack_length(); }
  virtual uint32_t sort_length() const { return pack_length(); }

  /**
     Get the maximum size of the data in packed format.

     @return Maximum data length of the field when packed using the
     Field::pack() function.
   */
  virtual uint32_t max_data_length() const {
    return pack_length();
  };

  virtual int reset(void) { memset(ptr, 0, pack_length()); return 0; }
  virtual void reset_fields() {}
  virtual void set_default()
  {
    my_ptrdiff_t l_offset= (my_ptrdiff_t) (table->getDefaultValues() - table->record[0]);
    memcpy(ptr, ptr + l_offset, pack_length());
    if (null_ptr)
      *null_ptr= ((*null_ptr & (unsigned char) ~null_bit) | (null_ptr[l_offset] & null_bit));
  }
  virtual bool binary() const { return 1; }
  virtual bool zero_pack() const { return 1; }
  virtual enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  virtual uint32_t key_length() const { return pack_length(); }
  virtual enum_field_types type() const =0;
  virtual enum_field_types real_type() const { return type(); }
  inline  int cmp(const unsigned char *str) { return cmp(ptr,str); }
  virtual int cmp_max(const unsigned char *a, const unsigned char *b,
                      uint32_t max_len __attribute__((unused)))
    { return cmp(a, b); }
  virtual int cmp(const unsigned char *,const unsigned char *)=0;
  virtual int cmp_binary(const unsigned char *a,const unsigned char *b,
                         uint32_t  __attribute__((unused)) max_length=UINT32_MAX)
  { return memcmp(a,b,pack_length()); }
  virtual int cmp_offset(uint32_t row_offset)
  { return cmp(ptr,ptr+row_offset); }
  virtual int cmp_binary_offset(uint32_t row_offset)
  { return cmp_binary(ptr, ptr+row_offset); };
  virtual int key_cmp(const unsigned char *a,const unsigned char *b)
  { return cmp(a, b); }
  virtual int key_cmp(const unsigned char *str, uint32_t length __attribute__((unused)))
  { return cmp(ptr,str); }
  virtual uint32_t decimals() const { return 0; }
  /*
    Caller beware: sql_type can change str.Ptr, so check
    ptr() to see if it changed if you are using your own buffer
    in str and restore it with set() if needed
  */
  virtual void sql_type(String &str) const =0;
  virtual uint32_t size_of() const =0;		// For new field
  inline bool is_null(my_ptrdiff_t row_offset= 0)
  { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : table->null_row; }
  inline bool is_real_null(my_ptrdiff_t row_offset= 0)
    { return null_ptr ? (null_ptr[row_offset] & null_bit ? 1 : 0) : 0; }
  inline bool is_null_in_record(const unsigned char *record)
  {
    if (!null_ptr)
      return 0;
    return test(record[(uint32_t) (null_ptr -table->record[0])] &
		null_bit);
  }
  inline bool is_null_in_record_with_offset(my_ptrdiff_t offset)
  {
    if (!null_ptr)
      return 0;
    return test(null_ptr[offset] & null_bit);
  }
  inline void set_null(my_ptrdiff_t row_offset= 0)
    { if (null_ptr) null_ptr[row_offset]|= null_bit; }
  inline void set_notnull(my_ptrdiff_t row_offset= 0)
    { if (null_ptr) null_ptr[row_offset]&= (unsigned char) ~null_bit; }
  inline bool maybe_null(void) { return null_ptr != 0 || table->maybe_null; }
  inline bool real_maybe_null(void) { return null_ptr != 0; }

  enum {
    LAST_NULL_BYTE_UNDEF= 0
  };

  /*
    Find the position of the last null byte for the field.

    SYNOPSIS
      last_null_byte()

    DESCRIPTION
      Return a pointer to the last byte of the null bytes where the
      field conceptually is placed.

    RETURN VALUE
      The position of the last null byte relative to the beginning of
      the record. If the field does not use any bits of the null
      bytes, the value 0 (LAST_NULL_BYTE_UNDEF) is returned.
   */
  size_t last_null_byte() const {
    size_t bytes= do_last_null_byte();
    assert(bytes <= table->getNullBytes());
    return bytes;
  }

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
  virtual void get_image(unsigned char *buff, uint32_t length,
                         const CHARSET_INFO * const cs __attribute__((unused)))
    { memcpy(buff,ptr,length); }
  virtual void set_image(const unsigned char *buff,uint32_t length,
                         const CHARSET_INFO * const cs __attribute__((unused)))
    { memcpy(ptr,buff,length); }


  /*
    Copy a field part into an output buffer.

    SYNOPSIS
      Field::get_key_image()
      buff   [out] output buffer
      length       output buffer size
      type         itMBR for geometry blobs, otherwise itRAW

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

  virtual uint32_t get_key_image(unsigned char *buff, uint32_t length,
                             imagetype type __attribute__((unused)))
  {
    get_image(buff, length, &my_charset_bin);
    return length;
  }
  virtual void set_key_image(const unsigned char *buff,uint32_t length)
    { set_image(buff,length, &my_charset_bin); }
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
  virtual bool send_binary(Protocol *protocol);

  virtual unsigned char *pack(unsigned char *to, const unsigned char *from,
                      uint32_t max_length, bool low_byte_first);
  /**
     @overload Field::pack(unsigned char*, const unsigned char*, uint32_t, bool)
  */
  unsigned char *pack(unsigned char *to, const unsigned char *from)
  {
    unsigned char *result= this->pack(to, from, UINT_MAX, table->s->db_low_byte_first);
    return(result);
  }

  virtual const unsigned char *unpack(unsigned char* to, const unsigned char *from,
                              uint32_t param_data, bool low_byte_first);
  /**
     @overload Field::unpack(unsigned char*, const unsigned char*, uint32_t, bool)
  */
  const unsigned char *unpack(unsigned char* to, const unsigned char *from)
  {
    const unsigned char *result= unpack(to, from, 0U, table->s->db_low_byte_first);
    return(result);
  }

  virtual unsigned char *pack_key(unsigned char* to, const unsigned char *from,
                          uint32_t max_length, bool low_byte_first)
  {
    return pack(to, from, max_length, low_byte_first);
  }
  virtual unsigned char *pack_key_from_key_image(unsigned char* to, const unsigned char *from,
					uint32_t max_length, bool low_byte_first)
  {
    return pack(to, from, max_length, low_byte_first);
  }
  virtual const unsigned char *unpack_key(unsigned char* to, const unsigned char *from,
                                  uint32_t max_length, bool low_byte_first)
  {
    return unpack(to, from, max_length, low_byte_first);
  }
  virtual uint32_t packed_col_length(const unsigned char *to __attribute__((unused)),
                                 uint32_t length)
  { return length;}
  virtual uint32_t max_packed_col_length(uint32_t max_length)
  { return max_length;}

  virtual int pack_cmp(const unsigned char *a,const unsigned char *b,
                       uint32_t key_length_arg __attribute__((unused)),
                       bool insert_or_update __attribute__((unused)))
  { return cmp(a,b); }
  virtual int pack_cmp(const unsigned char *b,
                       uint32_t key_length_arg __attribute__((unused)),
                       bool insert_or_update __attribute__((unused)))
  { return cmp(ptr,b); }
  uint32_t offset(unsigned char *record)
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
  virtual void set_charset(const CHARSET_INFO * const charset_arg __attribute__((unused)))
  { }
  virtual enum Derivation derivation(void) const
  { return DERIVATION_IMPLICIT; }
  virtual void set_derivation(enum Derivation derivation_arg __attribute__((unused)))
  { }
  bool set_warning(DRIZZLE_ERROR::enum_warning_level, unsigned int code,
                   int cuted_increment);
  void set_datetime_warning(DRIZZLE_ERROR::enum_warning_level, uint32_t code, 
                            const char *str, uint32_t str_len,
                            enum enum_drizzle_timestamp_type ts_type, int cuted_increment);
  void set_datetime_warning(DRIZZLE_ERROR::enum_warning_level, uint32_t code, 
                            int64_t nr, enum enum_drizzle_timestamp_type ts_type,
                            int cuted_increment);
  void set_datetime_warning(DRIZZLE_ERROR::enum_warning_level, const uint32_t code, 
                            double nr, enum enum_drizzle_timestamp_type ts_type);
  inline bool check_overflow(int op_result)
  {
    return (op_result == E_DEC_OVERFLOW);
  }
  int warn_if_overflow(int op_result);
  void init(Table *table_arg)
  {
    orig_table= table= table_arg;
    table_name= &table_arg->alias;
  }

  /* maximum possible display length */
  virtual uint32_t max_display_length()= 0;

  virtual uint32_t is_equal(Create_field *new_field);
  /* convert decimal to int64_t with overflow check */
  int64_t convert_decimal2int64_t(const my_decimal *val, bool unsigned_flag,
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
  friend bool reopen_table(THD *,Table *,bool);
  friend int cre_myisam(char * name, register Table *form, uint32_t options,
			uint64_t auto_increment_value);
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

private:
  /*
    Primitive for implementing last_null_byte().

    SYNOPSIS
      do_last_null_byte()

    DESCRIPTION
      Primitive for the implementation of the last_null_byte()
      function. This represents the inheritance interface and can be
      overridden by subclasses.
   */
  virtual size_t do_last_null_byte() const;

/**
   Retrieve the field metadata for fields.

   This default implementation returns 0 and saves 0 in the metadata_ptr
   value.

   @param   metadata_ptr   First byte of field metadata

   @returns 0 no bytes written.
*/
  virtual int do_save_field_metadata(unsigned char *metadata_ptr __attribute__((unused)))
  { return 0; }
};

/*
  Create field class for CREATE TABLE
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
  TYPELIB *save_interval;               // Temporary copy for the above
                                        // Used only for UCS2 intervals
  List<String> interval_list;
  const CHARSET_INFO *charset;
  Field *field;				// For alter table

  uint8_t row,col,sc_length,interval_id;	// For rea_create_table
  uint32_t	offset,pack_flag;

  /* Virtual column expression statement */
  virtual_column_info *vcol_info;
  /*
    Indication that the field is phycically stored in tables 
    rather than just generated on SQL queries.
    As of now, FALSE can only be set for generated-only virtual columns.
  */
  bool is_stored;

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

  bool init(THD *thd, char *field_name, enum_field_types type, char *length,
            char *decimals, uint32_t type_modifier, Item *default_value,
            Item *on_update_value, LEX_STRING *comment, char *change,
            List<String> *interval_list, const CHARSET_INFO * const cs,
            uint32_t uint_geom_type,
            enum column_format_type column_format,
            virtual_column_info *vcol_info);
};


/*
  A class for sending info to the client
*/

class Send_field {
 public:
  const char *db_name;
  const char *table_name,*org_table_name;
  const char *col_name,*org_col_name;
  uint32_t length;
  uint32_t charsetnr, flags, decimals;
  enum_field_types type;
  Send_field() {}
};


/*
  A class for quick copying data to fields
*/

class Copy_field :public Sql_alloc {
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


Field *make_field(TABLE_SHARE *share, unsigned char *ptr, uint32_t field_length,
		  unsigned char *null_pos, unsigned char null_bit,
		  uint32_t pack_flag, enum_field_types field_type,
		  const CHARSET_INFO * cs,
		  Field::utype unireg_check,
		  TYPELIB *interval, const char *field_name);
uint32_t pack_length_to_packflag(uint32_t type);
enum_field_types get_blob_type_from_length(uint32_t length);
uint32_t calc_pack_length(enum_field_types type,uint32_t length);
int set_field_to_null(Field *field);
int set_field_to_null_with_conversions(Field *field, bool no_conversions);


bool
test_if_important_data(const CHARSET_INFO * const cs, 
		       const char *str,
                       const char *strend);

/*
  Field subclasses
 */
#include <drizzled/field/str.h>
#include <drizzled/field/longstr.h>
#include <drizzled/field/num.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/enum.h>
#include <drizzled/field/null.h>
#include <drizzled/field/date.h>
#include <drizzled/field/fdecimal.h>
#include <drizzled/field/real.h>
#include <drizzled/field/double.h>
#include <drizzled/field/long.h>
#include <drizzled/field/int64_t.h>
#include <drizzled/field/num.h>
#include <drizzled/field/timetype.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/field/datetime.h>
#include <drizzled/field/fstring.h>
#include <drizzled/field/varstring.h>

/*
  The following are for the interface with the .frm file
*/

#define FIELDFLAG_DECIMAL		1
#define FIELDFLAG_BINARY		1	// Shares same flag
#define FIELDFLAG_NUMBER		2
#define FIELDFLAG_DECIMAL_POSITION      4
#define FIELDFLAG_PACK			120	// Bits used for packing
#define FIELDFLAG_INTERVAL		256     // mangled with decimals!
#define FIELDFLAG_BITFIELD		512	// mangled with decimals!
#define FIELDFLAG_BLOB			1024	// mangled with decimals!
#define FIELDFLAG_GEOM			2048    // mangled with decimals!

#define FIELDFLAG_TREAT_BIT_AS_CHAR     4096    /* use Field_bit_as_char */

#define FIELDFLAG_LEFT_FULLSCREEN	8192
#define FIELDFLAG_RIGHT_FULLSCREEN	16384
#define FIELDFLAG_FORMAT_NUMBER		16384	// predit: ###,,## in output
#define FIELDFLAG_NO_DEFAULT		16384   /* sql */
#define FIELDFLAG_SUM			((uint32_t) 32768)// predit: +#fieldflag
#define FIELDFLAG_MAYBE_NULL		((uint32_t) 32768)// sql
#define FIELDFLAG_HEX_ESCAPE		((uint32_t) 0x10000)
#define FIELDFLAG_PACK_SHIFT		3
#define FIELDFLAG_DEC_SHIFT		8
#define FIELDFLAG_MAX_DEC		31
#define FIELDFLAG_NUM_SCREEN_TYPE	0x7F01
#define FIELDFLAG_ALFA_SCREEN_TYPE	0x7800

#define MTYP_TYPENR(type) (type & 127)	/* Remove bits from type */

#define f_is_dec(x)		((x) & FIELDFLAG_DECIMAL)
#define f_is_num(x)		((x) & FIELDFLAG_NUMBER)
#define f_is_decimal_precision(x)	((x) & FIELDFLAG_DECIMAL_POSITION)
#define f_is_packed(x)		((x) & FIELDFLAG_PACK)
#define f_packtype(x)		(((x) >> FIELDFLAG_PACK_SHIFT) & 15)
#define f_decimals(x)		((uint8_t) (((x) >> FIELDFLAG_DEC_SHIFT) & FIELDFLAG_MAX_DEC))
#define f_is_alpha(x)		(!f_is_num(x))
#define f_is_binary(x)          ((x) & FIELDFLAG_BINARY) // 4.0- compatibility
#define f_is_enum(x)            (((x) & (FIELDFLAG_INTERVAL | FIELDFLAG_NUMBER)) == FIELDFLAG_INTERVAL)
#define f_is_bitfield(x)        (((x) & (FIELDFLAG_BITFIELD | FIELDFLAG_NUMBER)) == FIELDFLAG_BITFIELD)
#define f_is_blob(x)		(((x) & (FIELDFLAG_BLOB | FIELDFLAG_NUMBER)) == FIELDFLAG_BLOB)
#define f_is_equ(x)		((x) & (1+2+FIELDFLAG_PACK+31*256))
#define f_settype(x)		(((int) x) << FIELDFLAG_PACK_SHIFT)
#define f_maybe_null(x)		(x & FIELDFLAG_MAYBE_NULL)
#define f_no_default(x)		(x & FIELDFLAG_NO_DEFAULT)
#define f_bit_as_char(x)        ((x) & FIELDFLAG_TREAT_BIT_AS_CHAR)
#define f_is_hex_escape(x)      ((x) & FIELDFLAG_HEX_ESCAPE)

bool
check_string_copy_error(Field_str *field,
                        const char *well_formed_error_pos,
                        const char *cannot_convert_error_pos,
                        const char *end,
                        const CHARSET_INFO * const cs);


class Field_tiny :public Field_num {
public:
  Field_tiny(unsigned char *ptr_arg, uint32_t len_arg, unsigned char *null_ptr_arg,
	     unsigned char null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg,
	       0, zero_arg,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return DRIZZLE_TYPE_TINY;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_BINARY : HA_KEYTYPE_INT8; }
  int store(const char *to,uint32_t length, const CHARSET_INFO * const charset);
  int store(double nr);
  int store(int64_t nr, bool unsigned_val);
  int reset(void) { ptr[0]=0; return 0; }
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const unsigned char *,const unsigned char *);
  void sort_string(unsigned char *buff,uint32_t length);
  uint32_t pack_length() const { return 1; }
  void sql_type(String &str) const;
  uint32_t max_display_length() { return 4; }

  virtual unsigned char *pack(unsigned char* to, const unsigned char *from,
                      uint32_t max_length __attribute__((unused)),
                      bool low_byte_first __attribute__((unused)))
  {
    *to= *from;
    return to + 1;
  }

  virtual const unsigned char *unpack(unsigned char* to, const unsigned char *from,
                              uint32_t param_data __attribute__((unused)),
                              bool low_byte_first __attribute__((unused)))
  {
    *to= *from;
    return from + 1;
  }
};
