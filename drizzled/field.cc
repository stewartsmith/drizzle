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

/**
  @file

  @brief
  This file implements classes defined in field.h
*/
#include <drizzled/server_includes.h>
#include <errno.h>
#include "drizzled/sql_select.h"
#include "drizzled/error.h"
#include "drizzled/field/str.h"
#include "drizzled/field/num.h"
#include "drizzled/field/blob.h"
#include "drizzled/field/enum.h"
#include "drizzled/field/null.h"
#include "drizzled/field/date.h"
#include "drizzled/field/decimal.h"
#include "drizzled/field/real.h"
#include "drizzled/field/double.h"
#include "drizzled/field/long.h"
#include "drizzled/field/int64_t.h"
#include "drizzled/field/num.h"
#include "drizzled/field/timestamp.h"
#include "drizzled/field/datetime.h"
#include "drizzled/field/varstring.h"

/*****************************************************************************
  Instansiate templates and static variables
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List<Create_field>;
template class List_iterator<Create_field>;
#endif

static enum_field_types
field_types_merge_rules [DRIZZLE_TYPE_MAX+1][DRIZZLE_TYPE_MAX+1]=
{
  /* DRIZZLE_TYPE_TINY -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_TINY,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_LONG,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_TINY,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_LONGLONG,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_NEWDECIMAL,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_LONG -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_LONG,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_LONG,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_LONG,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_LONGLONG,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_NEWDECIMAL,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_DOUBLE -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_NULL -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_TINY,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_LONG,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_NULL,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_TIMESTAMP,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_LONGLONG,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_DATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_NEWDECIMAL,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_ENUM,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_TIMESTAMP -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_TIMESTAMP,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_TIMESTAMP,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_DATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_LONGLONG -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_LONGLONG,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_LONGLONG,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_LONGLONG,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_LONGLONG,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_DATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_NEWDECIMAL,
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_DATETIME -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_DATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_DATE -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_DATE,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_DATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_VARCHAR -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_NEWDECIMAL -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_NEWDECIMAL,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_NEWDECIMAL,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_NEWDECIMAL,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_NEWDECIMAL,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_NEWDECIMAL,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_ENUM -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_ENUM,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_BLOB -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
};

static Item_result field_types_result_type [DRIZZLE_TYPE_MAX+1]=
{
  //DRIZZLE_TYPE_TINY
  INT_RESULT,
  //DRIZZLE_TYPE_LONG
  INT_RESULT,
  //DRIZZLE_TYPE_DOUBLE
  REAL_RESULT,
  //DRIZZLE_TYPE_NULL
  STRING_RESULT,
  //DRIZZLE_TYPE_TIMESTAMP
  STRING_RESULT,
  //DRIZZLE_TYPE_LONGLONG
  INT_RESULT,
  //DRIZZLE_TYPE_DATETIME
  STRING_RESULT,
  //DRIZZLE_TYPE_DATE
  STRING_RESULT,
  //DRIZZLE_TYPE_VARCHAR
  STRING_RESULT,
  //DRIZZLE_TYPE_NEWDECIMAL   
  DECIMAL_RESULT,           
  //DRIZZLE_TYPE_ENUM
  STRING_RESULT,
  //DRIZZLE_TYPE_BLOB
  STRING_RESULT,
};

bool test_if_important_data(const CHARSET_INFO * const cs, 
                            const char *str,
                            const char *strend)
{
  if (cs != &my_charset_bin)
    str+= cs->cset->scan(cs, str, strend, MY_SEQ_SPACES);
  return (str < strend);
}

enum_field_types Field::field_type_merge(enum_field_types a,
                                         enum_field_types b)
{
  assert(a <= DRIZZLE_TYPE_MAX);
  assert(b <= DRIZZLE_TYPE_MAX);
  return field_types_merge_rules[a][b];
}

Item_result Field::result_merge_type(enum_field_types field_type)
{
  assert(field_type <= DRIZZLE_TYPE_MAX);
  return field_types_result_type[field_type];
}

bool Field::eq(Field *field)
{
  return (ptr == field->ptr && null_ptr == field->null_ptr &&
          null_bit == field->null_bit);
}

uint32_t Field::pack_length() const
{
  return field_length;
}

uint32_t Field::pack_length_in_rec() const
{
  return pack_length();
}

uint32_t Field::pack_length_from_metadata(uint32_t field_metadata)
{
  return field_metadata;
}

uint32_t Field::row_pack_length()
{
  return 0;
}

int Field::save_field_metadata(unsigned char *first_byte)
{
  return do_save_field_metadata(first_byte);
}

uint32_t Field::data_length()
{
  return pack_length();
}

uint32_t Field::used_length()
{
  return pack_length();
}

uint32_t Field::sort_length() const
{
  return pack_length();
}

uint32_t Field::max_data_length() const
{
  return pack_length();
}

int Field::reset(void)
{
  memset(ptr, 0, pack_length());
  return 0;
}

void Field::reset_fields()
{}

void Field::set_default()
{
  ptrdiff_t l_offset= (ptrdiff_t) (table->getDefaultValues() - table->record[0]);
  memcpy(ptr, ptr + l_offset, pack_length());
  if (null_ptr)
    *null_ptr= ((*null_ptr & (unsigned char) ~null_bit) | (null_ptr[l_offset] & null_bit));

  if (this == table->next_number_field)
    table->auto_increment_field_not_null= false;
}

bool Field::binary() const
{
  return true;
}

bool Field::zero_pack() const
{
  return true;
}

enum ha_base_keytype Field::key_type() const
{
  return HA_KEYTYPE_BINARY;
}

uint32_t Field::key_length() const
{
  return pack_length();
}

enum_field_types Field::real_type() const
{
  return type();
}

int Field::cmp_max(const unsigned char *a, const unsigned char *b, uint32_t)
{
  return cmp(a, b);
}

int Field::cmp_binary(const unsigned char *a,const unsigned char *b, uint32_t)
{
  return memcmp(a,b,pack_length());
}

int Field::cmp_offset(uint32_t row_offset)
{
  return cmp(ptr,ptr+row_offset);
}

int Field::cmp_binary_offset(uint32_t row_offset)
{
  return cmp_binary(ptr, ptr+row_offset);
}

int Field::key_cmp(const unsigned char *a,const unsigned char *b)
{
  return cmp(a, b);
}

int Field::key_cmp(const unsigned char *str, uint32_t)
{
  return cmp(ptr,str);
}

uint32_t Field::decimals() const
{
  return 0;
}

bool Field::is_null(my_ptrdiff_t row_offset)
{
  return null_ptr ?
    (null_ptr[row_offset] & null_bit ? true : false) :
    table->null_row;
}

bool Field::is_real_null(my_ptrdiff_t row_offset)
{
  return null_ptr ? (null_ptr[row_offset] & null_bit ? true : false) : false;
}

bool Field::is_null_in_record(const unsigned char *record)
{
  if (! null_ptr)
    return false;
  return test(record[(uint32_t) (null_ptr -table->record[0])] & null_bit);
}

bool Field::is_null_in_record_with_offset(my_ptrdiff_t with_offset)
{
  if (! null_ptr)
    return false;
  return test(null_ptr[with_offset] & null_bit);
}

void Field::set_null(my_ptrdiff_t row_offset)
{
  if (null_ptr)
    null_ptr[row_offset]|= null_bit;
}

void Field::set_notnull(my_ptrdiff_t row_offset)
{
  if (null_ptr)
    null_ptr[row_offset]&= (unsigned char) ~null_bit;
}

bool Field::maybe_null(void)
{
  return null_ptr != 0 || table->maybe_null;
}

bool Field::real_maybe_null(void)
{
  return null_ptr != 0;
}

bool Field::type_can_have_key_part(enum enum_field_types type)
{
  switch (type) {
  case DRIZZLE_TYPE_VARCHAR:
  case DRIZZLE_TYPE_BLOB:
    return true;
  default:
    return false;
  }
}

int Field::warn_if_overflow(int op_result)
{
  if (op_result == E_DEC_OVERFLOW)
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    return E_DEC_OVERFLOW;
  }
  if (op_result == E_DEC_TRUNCATED)
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
    return E_DEC_TRUNCATED;
  }
  return 0;
}

void Field::init(Table *table_arg)
{
  orig_table= table= table_arg;
  table_name= &table_arg->alias;
}

String *Field::val_int_as_str(String *val_buffer, bool unsigned_val)
{
  const CHARSET_INFO * const cs= &my_charset_bin;
  uint32_t length;
  int64_t value= val_int();

  if (val_buffer->alloc(MY_INT64_NUM_DECIMAL_DIGITS))
    return 0;
  length= (uint32_t) (*cs->cset->int64_t10_to_str)(cs, (char*) val_buffer->ptr(),
                                                MY_INT64_NUM_DECIMAL_DIGITS,
                                                unsigned_val ? 10 : -10,
                                                value);
  val_buffer->length(length);
  return val_buffer;
}

/// This is used as a table name when the table structure is not set up
Field::Field(unsigned char *ptr_arg,
             uint32_t length_arg,
             unsigned char *null_ptr_arg,
             unsigned char null_bit_arg,
             utype unireg_check_arg, 
             const char *field_name_arg)
  :
    ptr(ptr_arg),
    null_ptr(null_ptr_arg),
    table(NULL),
    orig_table(NULL),
    table_name(NULL),
    field_name(field_name_arg),
    key_start(0),
    part_of_key(0),
    part_of_key_not_clustered(0),
    part_of_sortkey(0),
    unireg_check(unireg_check_arg),
    field_length(length_arg),
    null_bit(null_bit_arg),
    is_created_from_null_item(false)
{
  flags= null_ptr ? 0: NOT_NULL_FLAG;
  comment.str= (char*) "";
  comment.length= 0;
  field_index= 0;
}

void Field::hash(uint32_t *nr, uint32_t *nr2)
{
  if (is_null())
  {
    *nr^= (*nr << 1) | 1;
  }
  else
  {
    uint32_t len= pack_length();
    const CHARSET_INFO * const cs= charset();
    cs->coll->hash_sort(cs, ptr, len, nr, nr2);
  }
}

void Field::copy_from_tmp(int row_offset)
{
  memcpy(ptr,ptr+row_offset,pack_length());
  if (null_ptr)
  {
    *null_ptr= (unsigned char) ((null_ptr[0] &
                                 (unsigned char) ~(uint32_t) null_bit) |
                                (null_ptr[row_offset] &
                                 (unsigned char) null_bit));
  }
}

int Field::compatible_field_size(uint32_t field_metadata)
{
  uint32_t const source_size= pack_length_from_metadata(field_metadata);
  uint32_t const destination_size= row_pack_length();
  return (source_size <= destination_size);
}

int Field::store(const char *to, 
                 uint32_t length,
                 const CHARSET_INFO * const cs,
                 enum_check_fields check_level)
{
  int res;
  enum_check_fields old_check_level= table->in_use->count_cuted_fields;
  table->in_use->count_cuted_fields= check_level;
  res= store(to, length, cs);
  table->in_use->count_cuted_fields= old_check_level;
  return res;
}

unsigned char *Field::pack(unsigned char *to, const unsigned char *from, uint32_t max_length, bool)
{
  uint32_t length= pack_length();
  set_if_smaller(length, max_length);
  memcpy(to, from, length);
  return to+length;
}

unsigned char *Field::pack(unsigned char *to, const unsigned char *from)
{
  unsigned char *result= this->pack(to, from, UINT32_MAX, table->s->db_low_byte_first);
  return(result);
}

const unsigned char *Field::unpack(unsigned char* to,
                                   const unsigned char *from, 
                                   uint32_t param_data,
                                   bool)
{
  uint32_t length=pack_length();
  int from_type= 0;
  /*
    If from length is > 255, it has encoded data in the upper bits. Need
    to mask it out.
  */
  if (param_data > 255)
  {
    from_type= (param_data & 0xff00) >> 8U;  // real_type.
    param_data= param_data & 0x00ff;        // length.
  }

  if ((param_data == 0) ||
      (length == param_data) ||
      (from_type != real_type()))
  {
    memcpy(to, from, length);
    return from+length;
  }

  uint32_t len= (param_data && (param_data < length)) ?
            param_data : length;

  memcpy(to, from, param_data > length ? length : len);
  return (from + len);
}

const unsigned char *Field::unpack(unsigned char* to, const unsigned char *from)
{
  const unsigned char *result= unpack(to, from, 0U, table->s->db_low_byte_first);
  return(result);
}

uint32_t Field::packed_col_length(const unsigned char *, uint32_t length)
{
  return length;
}

int Field::pack_cmp(const unsigned char *a, const unsigned char *b,
                    uint32_t, bool)
{
  return cmp(a,b);
}

int Field::pack_cmp(const unsigned char *b, uint32_t, bool)
{
  return cmp(ptr,b);
}

my_decimal *Field::val_decimal(my_decimal *)
{
  /* This never have to be called */
  assert(0);
  return 0;
}

void Field::make_field(Send_field *field)
{
  if (orig_table && orig_table->s->db.str && *orig_table->s->db.str)
  {
    field->db_name= orig_table->s->db.str;
    field->org_table_name= orig_table->s->table_name.str;
  }
  else
    field->org_table_name= field->db_name= "";
  if (orig_table)
  {
    field->table_name= orig_table->alias;
    field->org_col_name= field_name;
  }
  else
  {
    field->table_name= "";
    field->org_col_name= "";
  }
  field->col_name= field_name;
  field->charsetnr= charset()->number;
  field->length= field_length;
  field->type= type();
  field->flags= table->maybe_null ? (flags & ~NOT_NULL_FLAG) : flags;
  field->decimals= 0;
}

int64_t Field::convert_decimal2int64_t(const my_decimal *val, bool, int *err)
{
  int64_t i;
  if (warn_if_overflow(my_decimal2int(E_DEC_ERROR &
                                      ~E_DEC_OVERFLOW & ~E_DEC_TRUNCATED,
                                      val, false, &i)))
  {
    i= (val->sign() ? INT64_MIN : INT64_MAX);
    *err= 1;
  }
  return i;
}

uint32_t Field::fill_cache_field(CACHE_FIELD *copy)
{
  uint32_t store_length;
  copy->str=ptr;
  copy->length=pack_length();
  copy->blob_field=0;
  if (flags & BLOB_FLAG)
  {
    copy->blob_field=(Field_blob*) this;
    copy->strip=0;
    copy->length-= table->s->blob_ptr_size;
    return copy->length;
  }
  else
  {
    copy->strip=0;
    store_length= 0;
  }
  return copy->length+ store_length;
}

bool Field::get_date(DRIZZLE_TIME *ltime,uint32_t fuzzydate)
{
  char buff[40];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_datetime_with_warn(res->ptr(), res->length(),
                                ltime, fuzzydate) <= DRIZZLE_TIMESTAMP_ERROR)
    return 1;
  return 0;
}

bool Field::get_time(DRIZZLE_TIME *ltime)
{
  char buff[40];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_time_with_warn(res->ptr(), res->length(), ltime))
    return 1;
  return 0;
}

int Field::store_time(DRIZZLE_TIME *ltime, enum enum_drizzle_timestamp_type)
{
  char buff[MAX_DATE_STRING_REP_LENGTH];
  uint32_t length= (uint32_t) my_TIME_to_str(ltime, buff);
  return store(buff, length, &my_charset_bin);
}

bool Field::optimize_range(uint32_t idx, uint32_t part)
{
  return test(table->file->index_flags(idx, part, 1) & HA_READ_RANGE);
}

Field *Field::new_field(MEM_ROOT *root, Table *new_table, bool)
{
  Field *tmp;
  if (!(tmp= (Field*) memdup_root(root,(char*) this,size_of())))
    return 0;

  if (tmp->table->maybe_null)
    tmp->flags&= ~NOT_NULL_FLAG;
  tmp->table= new_table;
  tmp->key_start.reset();
  tmp->part_of_key.reset();
  tmp->part_of_sortkey.reset();
  tmp->unireg_check= Field::NONE;
  tmp->flags&= (NOT_NULL_FLAG | BLOB_FLAG | UNSIGNED_FLAG | BINARY_FLAG | ENUM_FLAG | SET_FLAG);
  tmp->reset_fields();
  return tmp;
}

Field *Field::new_key_field(MEM_ROOT *root, Table *new_table,
                            unsigned char *new_ptr,
                            unsigned char *new_null_ptr,
                            uint32_t new_null_bit)
{
  Field *tmp;
  if ((tmp= new_field(root, new_table, table == new_table)))
  {
    tmp->ptr= new_ptr;
    tmp->null_ptr= new_null_ptr;
    tmp->null_bit= new_null_bit;
  }
  return tmp;
}

Field *Field::clone(MEM_ROOT *root, Table *new_table)
{
  Field *tmp;
  if ((tmp= (Field*) memdup_root(root,(char*) this,size_of())))
  {
    tmp->init(new_table);
    tmp->move_field_offset((my_ptrdiff_t) (new_table->record[0] -
                                           new_table->s->default_values));
  }
  return tmp;
}

uint32_t Field::is_equal(Create_field *new_field_ptr)
{
  return (new_field_ptr->sql_type == real_type());
}

bool Field::eq_def(Field *field)
{
  if (real_type() != field->real_type() || charset() != field->charset() ||
      pack_length() != field->pack_length())
    return 0;
  return 1;
}

bool Field_enum::eq_def(Field *field)
{
  if (!Field::eq_def(field))
    return 0;
  TYPELIB *from_lib=((Field_enum*) field)->typelib;

  if (typelib->count < from_lib->count)
    return 0;
  for (uint32_t i=0 ; i < from_lib->count ; i++)
    if (my_strnncoll(field_charset,
                     (const unsigned char*)typelib->type_names[i],
                     strlen(typelib->type_names[i]),
                     (const unsigned char*)from_lib->type_names[i],
                     strlen(from_lib->type_names[i])))
      return 0;
  return 1;
}

/*****************************************************************************
  Handling of field and Create_field
*****************************************************************************/

/**
  Convert create_field::length from number of characters to number of bytes.
*/
void Create_field::create_length_to_internal_length(void)
{
  switch (sql_type) {
  case DRIZZLE_TYPE_BLOB:
  case DRIZZLE_TYPE_VARCHAR:
    length*= charset->mbmaxlen;
    key_length= length;
    pack_length= calc_pack_length(sql_type, length);
    break;
  case DRIZZLE_TYPE_ENUM:
    /* Pack_length already calculated in ::init() */
    length*= charset->mbmaxlen;
    key_length= pack_length;
    break;
  case DRIZZLE_TYPE_NEWDECIMAL:
    key_length= pack_length=
      my_decimal_get_binary_size(my_decimal_length_to_precision(length,
								decimals,
								flags &
								UNSIGNED_FLAG),
				 decimals);
    break;
  default:
    key_length= pack_length= calc_pack_length(sql_type, length);
    break;
  }
}

/**
  Init for a tmp table field. To be extended if need be.
*/
void Create_field::init_for_tmp_table(enum_field_types sql_type_arg,
                                      uint32_t length_arg, uint32_t decimals_arg,
                                      bool maybe_null, bool is_unsigned)
{
  field_name= "";
  sql_type= sql_type_arg;
  char_length= length= length_arg;;
  unireg_check= Field::NONE;
  interval= 0;
  charset= &my_charset_bin;
  pack_flag= (FIELDFLAG_NUMBER |
              ((decimals_arg & FIELDFLAG_MAX_DEC) << FIELDFLAG_DEC_SHIFT) |
              (maybe_null ? FIELDFLAG_MAYBE_NULL : 0) |
              (is_unsigned ? 0 : FIELDFLAG_DECIMAL));
}

bool Create_field::init(Session *,
                        char *fld_name,
                        enum_field_types fld_type,
                        char *fld_length,
                        char *fld_decimals,
                        uint32_t fld_type_modifier,
                        Item *fld_default_value,
                        Item *fld_on_update_value,
                        LEX_STRING *fld_comment,
                        char *fld_change,
                        List<String> *fld_interval_list,
                        const CHARSET_INFO * const fld_charset,
                        uint32_t,
                        enum column_format_type column_format_in)
                        
{
  uint32_t sign_len, allowed_type_modifier= 0;
  uint32_t max_field_charlength= MAX_FIELD_CHARLENGTH;

  field= 0;
  field_name= fld_name;
  def= fld_default_value;
  flags= fld_type_modifier;
  flags|= (((uint32_t)column_format_in & COLUMN_FORMAT_MASK) << COLUMN_FORMAT_FLAGS);
  unireg_check= (fld_type_modifier & AUTO_INCREMENT_FLAG ?
                 Field::NEXT_NUMBER : Field::NONE);
  decimals= fld_decimals ? (uint32_t)atoi(fld_decimals) : 0;
  if (decimals >= NOT_FIXED_DEC)
  {
    my_error(ER_TOO_BIG_SCALE, MYF(0), decimals, fld_name,
             NOT_FIXED_DEC-1);
    return(true);
  }

  sql_type= fld_type;
  length= 0;
  change= fld_change;
  interval= 0;
  pack_length= key_length= 0;
  charset= fld_charset;
  interval_list.empty();

  comment= *fld_comment;

  /*
    Set NO_DEFAULT_VALUE_FLAG if this field doesn't have a default value and
    it is NOT NULL, not an AUTO_INCREMENT field and not a TIMESTAMP.
  */
  if (!fld_default_value && !(fld_type_modifier & AUTO_INCREMENT_FLAG) &&
      (fld_type_modifier & NOT_NULL_FLAG) && fld_type != DRIZZLE_TYPE_TIMESTAMP)
    flags|= NO_DEFAULT_VALUE_FLAG;

  if (fld_length && !(length= (uint32_t) atoi(fld_length)))
    fld_length= 0; /* purecov: inspected */
  sign_len= fld_type_modifier & UNSIGNED_FLAG ? 0 : 1;

  switch (fld_type) {
  case DRIZZLE_TYPE_TINY:
    if (!fld_length)
      length= MAX_TINYINT_WIDTH+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case DRIZZLE_TYPE_LONG:
    if (!fld_length)
      length= MAX_INT_WIDTH+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case DRIZZLE_TYPE_LONGLONG:
    if (!fld_length)
      length= MAX_BIGINT_WIDTH;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case DRIZZLE_TYPE_NULL:
    break;
  case DRIZZLE_TYPE_NEWDECIMAL:
    my_decimal_trim(&length, &decimals);
    if (length > DECIMAL_MAX_PRECISION)
    {
      my_error(ER_TOO_BIG_PRECISION, MYF(0), length, fld_name,
               DECIMAL_MAX_PRECISION);
      return(true);
    }
    if (length < decimals)
    {
      my_error(ER_M_BIGGER_THAN_D, MYF(0), fld_name);
      return(true);
    }
    length=
      my_decimal_precision_to_length(length, decimals,
                                     fld_type_modifier & UNSIGNED_FLAG);
    pack_length=
      my_decimal_get_binary_size(length, decimals);
    break;
  case DRIZZLE_TYPE_VARCHAR:
    /*
      Long VARCHAR's are automaticly converted to blobs in mysql_prepare_table
      if they don't have a default value
    */
    max_field_charlength= MAX_FIELD_VARCHARLENGTH;
    break;
  case DRIZZLE_TYPE_BLOB:
    if (fld_default_value)
    {
      /* Allow empty as default value. */
      String str,*res;
      res= fld_default_value->val_str(&str);
      if (res->length())
      {
        my_error(ER_BLOB_CANT_HAVE_DEFAULT, MYF(0),
                 fld_name);
        return(true);
      }

    }
    flags|= BLOB_FLAG;
    break;
  case DRIZZLE_TYPE_DOUBLE:
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    if (!fld_length && !fld_decimals)
    {
      length= DBL_DIG+7;
      decimals= NOT_FIXED_DEC;
    }
    if (length < decimals &&
        decimals != NOT_FIXED_DEC)
    {
      my_error(ER_M_BIGGER_THAN_D, MYF(0), fld_name);
      return(true);
    }
    break;
  case DRIZZLE_TYPE_TIMESTAMP:
    if (!fld_length)
    {
      /* Compressed date YYYYMMDDHHMMSS */
      length= MAX_DATETIME_COMPRESSED_WIDTH;
    }
    else if (length != MAX_DATETIME_WIDTH)
    {
      /*
        We support only even TIMESTAMP lengths less or equal than 14
        and 19 as length of 4.1 compatible representation.
      */
      length= ((length+1)/2)*2; /* purecov: inspected */
      length= cmin(length, (uint32_t)MAX_DATETIME_COMPRESSED_WIDTH); /* purecov: inspected */
    }
    flags|= UNSIGNED_FLAG;
    if (fld_default_value)
    {
      /* Grammar allows only NOW() value for ON UPDATE clause */
      if (fld_default_value->type() == Item::FUNC_ITEM &&
          ((Item_func*)fld_default_value)->functype() == Item_func::NOW_FUNC)
      {
        unireg_check= (fld_on_update_value ? Field::TIMESTAMP_DNUN_FIELD:
                                             Field::TIMESTAMP_DN_FIELD);
        /*
          We don't need default value any longer moreover it is dangerous.
          Everything handled by unireg_check further.
        */
        def= 0;
      }
      else
        unireg_check= (fld_on_update_value ? Field::TIMESTAMP_UN_FIELD:
                                             Field::NONE);
    }
    else
    {
      /*
        If we have default TIMESTAMP NOT NULL column without explicit DEFAULT
        or ON UPDATE values then for the sake of compatiblity we should treat
        this column as having DEFAULT NOW() ON UPDATE NOW() (when we don't
        have another TIMESTAMP column with auto-set option before this one)
        or DEFAULT 0 (in other cases).
        So here we are setting TIMESTAMP_OLD_FIELD only temporary, and will
        replace this value by TIMESTAMP_DNUN_FIELD or NONE later when
        information about all TIMESTAMP fields in table will be availiable.

        If we have TIMESTAMP NULL column without explicit DEFAULT value
        we treat it as having DEFAULT NULL attribute.
      */
      unireg_check= (fld_on_update_value ? Field::TIMESTAMP_UN_FIELD :
                     (flags & NOT_NULL_FLAG ? Field::TIMESTAMP_OLD_FIELD :
                                              Field::NONE));
    }
    break;
  case DRIZZLE_TYPE_DATE:
    length= 10;
    break;
  case DRIZZLE_TYPE_DATETIME:
    length= MAX_DATETIME_WIDTH;
    break;
  case DRIZZLE_TYPE_ENUM:
    {
      /* Should be safe. */
      pack_length= get_enum_pack_length(fld_interval_list->elements);

      List_iterator<String> it(*fld_interval_list);
      String *tmp;
      while ((tmp= it++))
        interval_list.push_back(tmp);
      length= 1; /* See comment for DRIZZLE_TYPE_SET above. */
      break;
   }
  }
  /* Remember the value of length */
  char_length= length;

  if (!(flags & BLOB_FLAG) &&
      ((length > max_field_charlength &&
        fld_type != DRIZZLE_TYPE_ENUM &&
        (fld_type != DRIZZLE_TYPE_VARCHAR || fld_default_value)) ||
       (!length && fld_type != DRIZZLE_TYPE_VARCHAR)))
  {
    my_error((fld_type == DRIZZLE_TYPE_VARCHAR) ?  ER_TOO_BIG_FIELDLENGTH : ER_TOO_BIG_DISPLAYWIDTH,
              MYF(0),
              fld_name, max_field_charlength); /* purecov: inspected */
    return(true);
  }
  fld_type_modifier&= AUTO_INCREMENT_FLAG;
  if ((~allowed_type_modifier) & fld_type_modifier)
  {
    my_error(ER_WRONG_FIELD_SPEC, MYF(0), fld_name);
    return(true);
  }

  return(false); /* success */
}

/*
  Make a field from the .frm file info
*/
uint32_t calc_pack_length(enum_field_types type,uint32_t length)
{
  switch (type) {
  case DRIZZLE_TYPE_VARCHAR:     return (length + (length < 256 ? 1: 2));
  case DRIZZLE_TYPE_TINY	: return 1;
  case DRIZZLE_TYPE_DATE: return 3;
  case DRIZZLE_TYPE_TIMESTAMP:
  case DRIZZLE_TYPE_LONG	: return 4;
  case DRIZZLE_TYPE_DOUBLE: return sizeof(double);
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_LONGLONG: return 8;	/* Don't crash if no int64_t */
  case DRIZZLE_TYPE_NULL	: return 0;
  case DRIZZLE_TYPE_BLOB:		return 4+portable_sizeof_char_ptr;
  case DRIZZLE_TYPE_ENUM:
  case DRIZZLE_TYPE_NEWDECIMAL:
    abort();
  default:
    return 0;
  }
}

uint32_t pack_length_to_packflag(uint32_t type)
{
  switch (type) {
    case 1: return 1 << FIELDFLAG_PACK_SHIFT;
    case 2: assert(1);
    case 3: assert(1);
    case 4: return f_settype((uint32_t) DRIZZLE_TYPE_LONG);
    case 8: return f_settype((uint32_t) DRIZZLE_TYPE_LONGLONG);
  }
  return 0;					// This shouldn't happen
}

Field *make_field(TableShare *share,
                  MEM_ROOT *root,
                  unsigned char *ptr,
                  uint32_t field_length,
                  unsigned char *null_pos,
                  unsigned char null_bit,
                  uint32_t pack_flag,
                  enum_field_types field_type,
                  const CHARSET_INFO * field_charset,
                  Field::utype unireg_check,
                  TYPELIB *interval,
                  const char *field_name)
{
  if(!root)
    root= current_mem_root();

  if (!f_maybe_null(pack_flag))
  {
    null_pos=0;
    null_bit=0;
  }
  else
  {
    null_bit= ((unsigned char) 1) << null_bit;
  }

  switch (field_type) {
  case DRIZZLE_TYPE_DATE:
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_TIMESTAMP:
    field_charset= &my_charset_bin;
  default: break;
  }

  if (f_is_alpha(pack_flag))
  {
    if (!f_is_packed(pack_flag))
    {
      if (field_type == DRIZZLE_TYPE_VARCHAR)
        return new (root) Field_varstring(ptr,field_length,
                                   HA_VARCHAR_PACKLENGTH(field_length),
                                   null_pos,null_bit,
                                   unireg_check, field_name,
                                   share,
                                   field_charset);
      return 0;                                 // Error
    }

    uint32_t pack_length=calc_pack_length((enum_field_types)
				      f_packtype(pack_flag),
				      field_length);

    if (f_is_blob(pack_flag))
      return new (root) Field_blob(ptr,null_pos,null_bit,
			    unireg_check, field_name, share,
			    pack_length, field_charset);
    if (interval)
    {
      if (f_is_enum(pack_flag))
      {
        return new (root) Field_enum(ptr,field_length,null_pos,null_bit,
				  unireg_check, field_name,
				  get_enum_pack_length(interval->count),
                                  interval, field_charset);
      }
    }
  }

  switch (field_type) {
  case DRIZZLE_TYPE_NEWDECIMAL:
    return new (root) Field_new_decimal(ptr,field_length,null_pos,null_bit,
                                 unireg_check, field_name,
                                 f_decimals(pack_flag),
                                 f_is_decimal_precision(pack_flag) != 0,
                                 f_is_dec(pack_flag) == 0);
  case DRIZZLE_TYPE_DOUBLE:
    return new (root) Field_double(ptr,field_length,null_pos,null_bit,
			    unireg_check, field_name,
			    f_decimals(pack_flag),
			    false,
			    f_is_dec(pack_flag)== 0);
  case DRIZZLE_TYPE_TINY:
    assert(0);
  case DRIZZLE_TYPE_LONG:
    return new (root) Field_long(ptr,field_length,null_pos,null_bit,
			   unireg_check, field_name,
                           false,
			   f_is_dec(pack_flag) == 0);
  case DRIZZLE_TYPE_LONGLONG:
    return new (root) Field_int64_t(ptr,field_length,null_pos,null_bit,
			      unireg_check, field_name,
                              false,
			      f_is_dec(pack_flag) == 0);
  case DRIZZLE_TYPE_TIMESTAMP:
    return new (root) Field_timestamp(ptr,field_length, null_pos, null_bit,
                               unireg_check, field_name, share,
                               field_charset);
  case DRIZZLE_TYPE_DATE:
    return new (root) Field_date(ptr,null_pos,null_bit,
			     unireg_check, field_name, field_charset);
  case DRIZZLE_TYPE_DATETIME:
    return new (root) Field_datetime(ptr,null_pos,null_bit,
			      unireg_check, field_name, field_charset);
  case DRIZZLE_TYPE_NULL:
    return new (root) Field_null(ptr, field_length, unireg_check, field_name,
                          field_charset);
  default:					// Impossible (Wrong version)
    break;
  }
  return 0;
}

/** Create a field suitable for create of table. */
Create_field::Create_field(Field *old_field, Field *orig_field)
{
  field= old_field;
  field_name= change= old_field->field_name;
  length= old_field->field_length;
  flags= old_field->flags;
  unireg_check= old_field->unireg_check;
  pack_length= old_field->pack_length();
  key_length= old_field->key_length();
  sql_type= old_field->real_type();
  charset= old_field->charset();		// May be NULL ptr
  comment= old_field->comment;
  decimals= old_field->decimals();

  /* Fix if the original table had 4 byte pointer blobs */
  if (flags & BLOB_FLAG)
    pack_length= (pack_length- old_field->table->s->blob_ptr_size +
		  portable_sizeof_char_ptr);

  switch (sql_type) {
  case DRIZZLE_TYPE_BLOB:
    sql_type= DRIZZLE_TYPE_BLOB;
    length/= charset->mbmaxlen;
    key_length/= charset->mbmaxlen;
    break;
    /* Change CHAR -> VARCHAR if dynamic record length */
  case DRIZZLE_TYPE_ENUM:
  case DRIZZLE_TYPE_VARCHAR:
    /* This is corrected in create_length_to_internal_length */
    length= (length+charset->mbmaxlen-1) / charset->mbmaxlen;
    break;
  default:
    break;
  }

  if (flags & (ENUM_FLAG | SET_FLAG))
    interval= ((Field_enum*) old_field)->typelib;
  else
    interval= 0;
  def= 0;
  char_length= length;

  if (!(flags & (NO_DEFAULT_VALUE_FLAG )) &&
      old_field->ptr && orig_field &&
      (sql_type != DRIZZLE_TYPE_TIMESTAMP ||                /* set def only if */
       old_field->table->timestamp_field != old_field ||  /* timestamp field */
       unireg_check == Field::TIMESTAMP_UN_FIELD))        /* has default val */
  {
    ptrdiff_t diff;

    /* Get the value from default_values */
    diff= (ptrdiff_t) (orig_field->table->s->default_values - orig_field->table->record[0]);
    orig_field->move_field_offset(diff);	// Points now at default_values
    if (! orig_field->is_real_null())
    {
      char buff[MAX_FIELD_WIDTH], *pos;
      String tmp(buff, sizeof(buff), charset), *res;
      res= orig_field->val_str(&tmp);
      pos= (char*) sql_strmake(res->ptr(), res->length());
      def= new Item_string(pos, res->length(), charset);
    }
    orig_field->move_field_offset(-diff);	// Back to record[0]
  }
}

/*****************************************************************************
 Warning handling
*****************************************************************************/

bool Field::set_warning(DRIZZLE_ERROR::enum_warning_level level,
                        uint32_t code,
                        int cuted_increment)
{
  /*
    If this field was created only for type conversion purposes it
    will have table == NULL.
  */
  Session *session= table ? table->in_use : current_session;
  if (session->count_cuted_fields)
  {
    session->cuted_fields+= cuted_increment;
    push_warning_printf(session, level, code, ER(code), field_name,
                        session->row_count);
    return 0;
  }
  return level >= DRIZZLE_ERROR::WARN_LEVEL_WARN;
}


void Field::set_datetime_warning(DRIZZLE_ERROR::enum_warning_level level,
                                 unsigned int code,
                                 const char *str, 
                                 uint32_t str_length,
                                 enum enum_drizzle_timestamp_type ts_type, 
                                 int cuted_increment)
{
  Session *session= table ? table->in_use : current_session;
  if ((session->really_abort_on_warning() &&
       level >= DRIZZLE_ERROR::WARN_LEVEL_WARN) ||
      set_warning(level, code, cuted_increment))
    make_truncated_value_warning(session, level, str, str_length, ts_type,
                                 field_name);
}

void Field::set_datetime_warning(DRIZZLE_ERROR::enum_warning_level level, 
                                 uint32_t code,
                                 int64_t nr, 
                                 enum enum_drizzle_timestamp_type ts_type,
                                 int cuted_increment)
{
  Session *session= table ? table->in_use : current_session;
  if (session->really_abort_on_warning() ||
      set_warning(level, code, cuted_increment))
  {
    char str_nr[22];
    char *str_end= int64_t10_to_str(nr, str_nr, -10);
    make_truncated_value_warning(session, level, str_nr, (uint32_t) (str_end - str_nr),
                                 ts_type, field_name);
  }
}

void Field::set_datetime_warning(DRIZZLE_ERROR::enum_warning_level level,
                                 const uint32_t code,
                                 double nr, 
                                 enum enum_drizzle_timestamp_type ts_type)
{
  Session *session= table ? table->in_use : current_session;
  if (session->really_abort_on_warning() ||
      set_warning(level, code, 1))
  {
    /* DBL_DIG is enough to print '-[digits].E+###' */
    char str_nr[DBL_DIG + 8];
    uint32_t str_len= sprintf(str_nr, "%g", nr);
    make_truncated_value_warning(session, level, str_nr, str_len, ts_type,
                                 field_name);
  }
}

bool Field::isReadSet() 
{ 
  return table->isReadSet(field_index); 
}

bool Field::isWriteSet()
{ 
  return table->isWriteSet(field_index); 
}
