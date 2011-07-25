/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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
 * @file This file implements the Field class and API
 */

#include <config.h>
#include <cstdio>
#include <errno.h>
#include <float.h>
#include <drizzled/sql_select.h>
#include <drizzled/error.h>
#include <drizzled/field/str.h>
#include <drizzled/field/num.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/boolean.h>
#include <drizzled/field/enum.h>
#include <drizzled/field/null.h>
#include <drizzled/field/date.h>
#include <drizzled/field/decimal.h>
#include <drizzled/field/real.h>
#include <drizzled/field/double.h>
#include <drizzled/field/int32.h>
#include <drizzled/field/int64.h>
#include <drizzled/field/num.h>
#include <drizzled/field/time.h>
#include <drizzled/field/epoch.h>
#include <drizzled/field/datetime.h>
#include <drizzled/field/microtime.h>
#include <drizzled/field/varstring.h>
#include <drizzled/field/uuid.h>
#include <drizzled/time_functions.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/table.h>
#include <drizzled/util/test.h>
#include <drizzled/session.h>
#include <drizzled/current_session.h>
#include <drizzled/display.h>
#include <drizzled/typelib.h>

namespace drizzled {

/*****************************************************************************
  Instansiate templates and static variables
*****************************************************************************/

static enum_field_types
field_types_merge_rules [enum_field_types_size][enum_field_types_size]=
{
  /* DRIZZLE_TYPE_LONG -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_DECIMAL,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_DOUBLE -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_NULL -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_DECIMAL,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_ENUM,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_BOOLEAN,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_UUID,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_MICROTIME,
  },
  /* DRIZZLE_TYPE_TIMESTAMP -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_LONGLONG -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_DECIMAL,
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_DATETIME -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_DATE -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_VARCHAR -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_DECIMAL -> */
  {
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_DECIMAL,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_DOUBLE,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_DECIMAL,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_DECIMAL,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_DECIMAL,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_ENUM -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
   },
  /* DRIZZLE_TYPE_BLOB -> */
  {
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_TIME -> */
  {
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_TIME,
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR,
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_UUID,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_BOOLEAN -> */
  {
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_BOOLEAN,
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR,
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_BOOLEAN,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_UUID -> */
  {
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_UUID,
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR,
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_UUID,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_VARCHAR,
  },
  /* DRIZZLE_TYPE_MICROTIME -> */
  {
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_MICROTIME,
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
    //DRIZZLE_TYPE_DECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR,
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_BOOLEAN
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_UUID
    DRIZZLE_TYPE_UUID,
    //DRIZZLE_TYPE_MICROTIME
    DRIZZLE_TYPE_MICROTIME,
  },
};

static Item_result field_types_result_type [enum_field_types_size]=
{
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
  //DRIZZLE_TYPE_DECIMAL   
  DECIMAL_RESULT,           
  //DRIZZLE_TYPE_ENUM
  STRING_RESULT,
  //DRIZZLE_TYPE_BLOB
  STRING_RESULT,
  //DRIZZLE_TYPE_TIME
  STRING_RESULT,
  //DRIZZLE_TYPE_BOOLEAN
  STRING_RESULT,
  //DRIZZLE_TYPE_UUID
  STRING_RESULT,
  //DRIZZLE_TYPE_MICROTIME
  STRING_RESULT,
};

bool test_if_important_data(const charset_info_st * const cs, 
                            const char *str,
                            const char *strend)
{
  if (cs != &my_charset_bin)
    str+= cs->cset->scan(cs, str, strend, MY_SEQ_SPACES);
  return (str < strend);
}

void *Field::operator new(size_t size)
{
  return memory::sql_alloc(size);
}

void *Field::operator new(size_t size, memory::Root *mem_root)
{
  return mem_root->alloc(size);
}

enum_field_types Field::field_type_merge(enum_field_types a,
                                         enum_field_types b)
{
  assert(a < enum_field_types_size);
  assert(b < enum_field_types_size);
  return field_types_merge_rules[a][b];
}

Item_result Field::result_merge_type(enum_field_types field_type)
{
  assert(field_type < enum_field_types_size);
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
  ptrdiff_t l_offset= (ptrdiff_t) (table->getDefaultValues() - table->getInsertRecord());
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

bool Field::is_null(ptrdiff_t row_offset) const
{
  return null_ptr ? (null_ptr[row_offset] & null_bit ? true : false) : table->null_row;
}

bool Field::is_real_null(ptrdiff_t row_offset) const
{
  return null_ptr && (null_ptr[row_offset] & null_bit);
}

bool Field::is_null_in_record(const unsigned char *record) const
{
  return null_ptr && test(record[(uint32_t) (null_ptr -table->getInsertRecord())] & null_bit);
}

bool Field::is_null_in_record_with_offset(ptrdiff_t with_offset) const
{
  return null_ptr && test(null_ptr[with_offset] & null_bit);
}

void Field::set_null(ptrdiff_t row_offset)
{
  if (null_ptr)
    null_ptr[row_offset]|= null_bit;
}

void Field::set_notnull(ptrdiff_t row_offset)
{
  if (null_ptr)
    null_ptr[row_offset]&= (unsigned char) ~null_bit;
}

bool Field::maybe_null(void) const
{
  return null_ptr != 0 || table->maybe_null;
}

bool Field::real_maybe_null(void) const
{
  return null_ptr != 0;
}

bool Field::type_can_have_key_part(enum enum_field_types type)
{
  switch (type) 
  {
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
}

/// This is used as a table name when the table structure is not set up
Field::Field(unsigned char *ptr_arg,
             uint32_t length_arg,
             unsigned char *null_ptr_arg,
             unsigned char null_bit_arg,
             utype unireg_check_arg, 
             const char *field_name_arg) :
    ptr(ptr_arg),
    null_ptr(null_ptr_arg),
    table(NULL),
    orig_table(NULL),
    field_name(field_name_arg),
    comment(NULL_LEX_STRING),
    key_start(0),
    part_of_key(0),
    part_of_key_not_clustered(0),
    part_of_sortkey(0),
    unireg_check(unireg_check_arg),
    field_length(length_arg),
    flags(null_ptr ? 0: NOT_NULL_FLAG),
    field_index(0),
    null_bit(null_bit_arg),
    is_created_from_null_item(false)
{
}

void Field::hash(uint32_t *nr, uint32_t *nr2) const
{
  if (is_null())
  {
    *nr^= (*nr << 1) | 1;
  }
  else
  {
    uint32_t len= pack_length();
    const charset_info_st * const cs= charset();
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

int Field::store_and_check(enum_check_fields check_level,
                           const char *to, 
                           uint32_t length,
                           const charset_info_st * const cs)

{
  enum_check_fields old_check_level= table->in_use->count_cuted_fields;
  table->in_use->count_cuted_fields= check_level;
  int res= store(to, length, cs);
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
  return this->pack(to, from, UINT32_MAX, table->getShare()->db_low_byte_first);
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
  return unpack(to, from, 0, table->getShare()->db_low_byte_first);
}

type::Decimal *Field::val_decimal(type::Decimal *) const
{
  /* This never have to be called */
  assert(false);
  return 0;
}


void Field::make_field(SendField *field)
{
  if (orig_table && orig_table->getShare()->getSchemaName() && *orig_table->getShare()->getSchemaName())
  {
    field->db_name= orig_table->getShare()->getSchemaName();
    field->org_table_name= orig_table->getShare()->getTableName();
  }
  else
    field->org_table_name= field->db_name= "";
  if (orig_table)
  {
    field->table_name= orig_table->getAlias();
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

int64_t Field::convert_decimal2int64_t(const type::Decimal *val, bool, int *err)
{
  int64_t i;
  if (warn_if_overflow(val->val_int32(E_DEC_ERROR &
                                      ~E_DEC_OVERFLOW & ~E_DEC_TRUNCATED,
                                      false, &i)))
  {
    i= (val->sign() ? INT64_MIN : INT64_MAX);
    *err= 1;
  }
  return i;
}

uint32_t Field::fill_cache_field(CacheField *copy)
{
  uint32_t store_length;
  copy->str=ptr;
  copy->length=pack_length();
  copy->blob_field=0;
  if (flags & BLOB_FLAG)
  {
    copy->blob_field=(Field_blob*) this;
    copy->strip=0;
    copy->length-= table->getShare()->sizeBlobPtr();
    return copy->length;
  }
  else
  {
    copy->strip=0;
    store_length= 0;
  }
  return copy->length+ store_length;
}

bool Field::get_date(type::Time &ltime, uint32_t fuzzydate) const
{
  char buff[type::Time::MAX_STRING_LENGTH];
  String tmp(buff,sizeof(buff),&my_charset_bin);

  assert(getTable() and getTable()->getSession());

  String* res= val_str_internal(&tmp);
  return not res or str_to_datetime_with_warn(getTable()->getSession(), res->ptr(), res->length(), &ltime, fuzzydate) <= type::DRIZZLE_TIMESTAMP_ERROR;
}

bool Field::get_time(type::Time &ltime) const
{
  char buff[type::Time::MAX_STRING_LENGTH];
  String tmp(buff,sizeof(buff),&my_charset_bin);

  String* res= val_str_internal(&tmp);
  return not res or str_to_time_with_warn(getTable()->getSession(), res->ptr(), res->length(), &ltime);
}

int Field::store_time(type::Time &ltime, type::timestamp_t)
{
  String tmp;
  ltime.convert(tmp);
  return store(tmp.ptr(), tmp.length(), &my_charset_bin);
}

bool Field::optimize_range(uint32_t idx, uint32_t)
{
  return test(table->index_flags(idx) & HA_READ_RANGE);
}

Field *Field::new_field(memory::Root *root, Table *new_table, bool)
{
  Field* tmp= (Field*) root->memdup(this,size_of());
  if (tmp->table->maybe_null)
    tmp->flags&= ~NOT_NULL_FLAG;
  tmp->table= new_table;
  tmp->key_start.reset();
  tmp->part_of_key.reset();
  tmp->part_of_sortkey.reset();
  tmp->unireg_check= Field::NONE;
  tmp->flags&= (NOT_NULL_FLAG | BLOB_FLAG | UNSIGNED_FLAG | BINARY_FLAG | ENUM_FLAG);
  tmp->reset_fields();
  return tmp;
}

Field *Field::new_key_field(memory::Root *root, Table *new_table,
                            unsigned char *new_ptr,
                            unsigned char *new_null_ptr,
                            uint32_t new_null_bit)
{
  Field *tmp= new_field(root, new_table, table == new_table);
  tmp->ptr= new_ptr;
  tmp->null_ptr= new_null_ptr;
  tmp->null_bit= new_null_bit;
  return tmp;
}

Field *Field::clone(memory::Root *root, Table *new_table)
{
  Field *tmp= (Field*) root->memdup(this,size_of());
  tmp->init(new_table);
  tmp->move_field_offset((ptrdiff_t) (new_table->getInsertRecord() - new_table->getDefaultValues()));
  return tmp;
}

uint32_t Field::is_equal(CreateField *new_field_ptr)
{
  return new_field_ptr->sql_type == real_type();
}

bool Field::eq_def(Field *field)
{
  return real_type() == field->real_type() && charset() == field->charset() && pack_length() == field->pack_length();
}

bool Field_enum::eq_def(Field *field)
{
  if (!Field::eq_def(field))
    return 0;

  TYPELIB *from_lib=((Field_enum*) field)->typelib;

  if (typelib->count < from_lib->count)
    return 0;

  for (uint32_t i=0 ; i < from_lib->count ; i++)
  {
    if (my_strnncoll(field_charset,
                     (const unsigned char*)typelib->type_names[i], strlen(typelib->type_names[i]),
                     (const unsigned char*)from_lib->type_names[i], strlen(from_lib->type_names[i])))
      return 0;
  }

  return 1;
}

uint32_t calc_pack_length(enum_field_types type,uint32_t length)
{
  switch (type) 
  {
  case DRIZZLE_TYPE_VARCHAR: return (length + (length < 256 ? 1: 2));
  case DRIZZLE_TYPE_UUID: return field::Uuid::max_string_length();
  case DRIZZLE_TYPE_MICROTIME: return field::Microtime::max_string_length();
  case DRIZZLE_TYPE_TIMESTAMP: return field::Epoch::max_string_length();
  case DRIZZLE_TYPE_BOOLEAN: return field::Boolean::max_string_length();
  case DRIZZLE_TYPE_DATE:
  case DRIZZLE_TYPE_ENUM:
  case DRIZZLE_TYPE_LONG: return 4;
  case DRIZZLE_TYPE_DOUBLE: return sizeof(double);
  case DRIZZLE_TYPE_TIME:
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_LONGLONG: return 8;	/* Don't crash if no int64_t */
  case DRIZZLE_TYPE_NULL: return 0;
  case DRIZZLE_TYPE_BLOB: return 4 + portable_sizeof_char_ptr;
  case DRIZZLE_TYPE_DECIMAL:
                          break;
  }

  assert(0);
  abort();
}

uint32_t pack_length_to_packflag(uint32_t type)
{
  switch (type) 
  {
  case 1: return 1 << FIELDFLAG_PACK_SHIFT;
  case 2: assert(0);
  case 3: assert(0);
  case 4: return f_settype(DRIZZLE_TYPE_LONG);
  case 8: return f_settype(DRIZZLE_TYPE_LONGLONG);
  }
  assert(false);
  return 0;					// This shouldn't happen
}

/*****************************************************************************
 Warning handling
*****************************************************************************/

bool Field::set_warning(DRIZZLE_ERROR::enum_warning_level level,
                        drizzled::error_t code,
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
                                 drizzled::error_t code,
                                 const char *str, 
                                 uint32_t str_length,
                                 type::timestamp_t ts_type, 
                                 int cuted_increment)
{
  Session *session= (getTable() and getTable()->getSession()) ? getTable()->getSession() : current_session;

  if ((session->abortOnWarning() and
       level >= DRIZZLE_ERROR::WARN_LEVEL_WARN) ||
      set_warning(level, code, cuted_increment))
    make_truncated_value_warning(session, level, str, str_length, ts_type,
                                 field_name);
}

void Field::set_datetime_warning(DRIZZLE_ERROR::enum_warning_level level, 
                                 drizzled::error_t code,
                                 int64_t nr, 
                                 type::timestamp_t ts_type,
                                 int cuted_increment)
{
  Session *session= (getTable() and getTable()->getSession()) ? getTable()->getSession() : current_session;

  if (session->abortOnWarning() or
      set_warning(level, code, cuted_increment))
  {
    char str_nr[DECIMAL_LONGLONG_DIGITS];
    char *str_end= internal::int64_t10_to_str(nr, str_nr, -10);
    make_truncated_value_warning(session, level, str_nr, (uint32_t) (str_end - str_nr),
                                 ts_type, field_name);
  }
}

void Field::set_datetime_warning(DRIZZLE_ERROR::enum_warning_level level,
                                 const drizzled::error_t code,
                                 double nr, 
                                 type::timestamp_t ts_type)
{
  Session *session= (getTable() and getTable()->getSession()) ? getTable()->getSession() : current_session;

  if (session->abortOnWarning() or
      set_warning(level, code, 1))
  {
    /* DBL_DIG is enough to print '-[digits].E+###' */
    char str_nr[DBL_DIG + 8];
    uint32_t str_len= snprintf(str_nr, sizeof(str_nr), "%g", nr);
    make_truncated_value_warning(session, level, str_nr, str_len, ts_type,
                                 field_name);
  }
}

bool Field::isReadSet() const 
{ 
  return table->isReadSet(field_index); 
}

bool Field::isWriteSet()
{ 
  return table->isWriteSet(field_index); 
}

void Field::setReadSet(bool arg)
{
  if (arg)
    table->setReadSet(field_index);
  else
    table->clearReadSet(field_index);
}

void Field::setWriteSet(bool arg)
{
  if (arg)
    table->setWriteSet(field_index);
  else
    table->clearWriteSet(field_index);
}

void Field::pack_num(uint64_t arg, unsigned char *destination)
{
  if (not destination)
    destination= ptr;

  int64_tstore(destination, arg);
}

void Field::pack_num(uint32_t arg, unsigned char *destination)
{
  if (not destination)
    destination= ptr;

  longstore(destination, arg);
}

uint64_t Field::unpack_num(uint64_t &destination, const unsigned char *arg) const
{
  if (not arg)
    arg= ptr;

  int64_tget(destination, arg);

  return destination;
}

uint32_t Field::unpack_num(uint32_t &destination, const unsigned char *arg) const
{
  if (not arg)
    arg= ptr;

  longget(destination, arg);

  return destination;
}

std::ostream& operator<<(std::ostream& output, const Field &field)
{
  output << "Field:(";
  output <<  field.field_name;
  output << ", ";
  output << drizzled::display::type(field.real_type());
  output << ", { ";

  if (field.flags & NOT_NULL_FLAG)
    output << " NOT_NULL";

  if (field.flags & PRI_KEY_FLAG)
    output << ", PRIMARY KEY";

  if (field.flags & UNIQUE_KEY_FLAG)
    output << ", UNIQUE KEY";

  if (field.flags & MULTIPLE_KEY_FLAG)
    output << ", MULTIPLE KEY";

  if (field.flags & BLOB_FLAG)
    output << ", BLOB";

  if (field.flags & UNSIGNED_FLAG)
    output << ", UNSIGNED";

  if (field.flags & BINARY_FLAG)
    output << ", BINARY";
  output << "}, ";
  output << ")";

  return output;  // for multiple << operators.
}

} /* namespace drizzled */
