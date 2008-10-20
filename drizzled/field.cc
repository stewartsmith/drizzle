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
#include "sql_select.h"
#include <errno.h>
#include <drizzled/drizzled_error_messages.h>


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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_NEWDATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_NEWDATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_NEWDATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_NEWDECIMAL
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_NEWDECIMAL,
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_TIME -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_TIME,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_NEWDATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_NEWDATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_NEWDATE -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_NEWDATE,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_DATETIME,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_NEWDATE,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
  /* DRIZZLE_TYPE_VIRTUAL -> */
  {
    //DRIZZLE_TYPE_TINY
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_LONG
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_DOUBLE
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_NULL
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_TIMESTAMP
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_LONGLONG
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_VIRTUAL,
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_VARCHAR,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
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
    //DRIZZLE_TYPE_TIME
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_DATETIME
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_NEWDATE
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_VARCHAR
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_VIRTUAL
    DRIZZLE_TYPE_VIRTUAL,
    //DRIZZLE_TYPE_NEWDECIMAL
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_ENUM
    DRIZZLE_TYPE_BLOB,
    //DRIZZLE_TYPE_BLOB
    DRIZZLE_TYPE_BLOB,
  },
};

/**
   Return type of which can carry value of both given types in UNION result.

   @param a  type for merging
   @param b  type for merging

   @return
   type of field
*/

enum_field_types Field::field_type_merge(enum_field_types a,
                                         enum_field_types b)
{
  assert(a <= DRIZZLE_TYPE_MAX);
  assert(b <= DRIZZLE_TYPE_MAX);
  return field_types_merge_rules[a][b];
}


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
  //DRIZZLE_TYPE_TIME
  STRING_RESULT,
  //DRIZZLE_TYPE_DATETIME
  STRING_RESULT,
  //DRIZZLE_TYPE_NEWDATE
  STRING_RESULT,
  //DRIZZLE_TYPE_VARCHAR
  STRING_RESULT,
  //DRIZZLE_TYPE_VIRTUAL
  STRING_RESULT,
  //DRIZZLE_TYPE_NEWDECIMAL
  DECIMAL_RESULT,
  //DRIZZLE_TYPE_ENUM
  STRING_RESULT,
  //DRIZZLE_TYPE_BLOB
  STRING_RESULT,
};


/*
  Test if the given string contains important data:
  not spaces for character string,
  or any data for binary string.

  SYNOPSIS
  test_if_important_data()
  cs          Character set
  str         String to test
  strend      String end

  RETURN
  false - If string does not have important data
  true  - If string has some important data
*/

bool
test_if_important_data(const CHARSET_INFO * const cs, const char *str,
                       const char *strend)
{
  if (cs != &my_charset_bin)
    str+= cs->cset->scan(cs, str, strend, MY_SEQ_SPACES);
  return (str < strend);
}


/**
   Detect Item_result by given field type of UNION merge result.

   @param field_type  given field type

   @return
   Item_result (type of internal MySQL expression result)
*/

Item_result Field::result_merge_type(enum_field_types field_type)
{
  assert(field_type <= DRIZZLE_TYPE_MAX);
  return field_types_result_type[field_type];
}

/*****************************************************************************
  Static help functions
*****************************************************************************/


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


/**
   Process decimal library return codes and issue warnings for overflow and
   truncation.

   @param op_result  decimal library return code (E_DEC_* see include/decimal.h)

   @retval
   1  there was overflow
   @retval
   0  no error or some other errors except overflow
*/

int Field::warn_if_overflow(int op_result)
{
  if (op_result == E_DEC_OVERFLOW)
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    return 1;
  }
  if (op_result == E_DEC_TRUNCATED)
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_NOTE, ER_WARN_DATA_TRUNCATED, 1);
    /* We return 0 here as this is not a critical issue */
  }
  return 0;
}


#ifdef NOT_USED
static bool test_if_real(const char *str,int length, const CHARSET_INFO * const cs)
{
  cs= system_charset_info; // QQ move test_if_real into CHARSET_INFO struct

  while (length && my_isspace(cs,*str))
  {            // Allow start space
    length--; str++;
  }
  if (!length)
    return 0;
  if (*str == '+' || *str == '-')
  {
    length--; str++;
    if (!length || !(my_isdigit(cs,*str) || *str == '.'))
      return 0;
  }
  while (length && my_isdigit(cs,*str))
  {
    length--; str++;
  }
  if (!length)
    return 1;
  if (*str == '.')
  {
    length--; str++;
    while (length && my_isdigit(cs,*str))
    {
      length--; str++;
    }
  }
  if (!length)
    return 1;
  if (*str == 'E' || *str == 'e')
  {
    if (length < 3 || (str[1] != '+' && str[1] != '-') ||
        !my_isdigit(cs,str[2]))
      return 0;
    length-=3;
    str+=3;
    while (length && my_isdigit(cs,*str))
    {
      length--; str++;
    }
  }
  for (; length ; length--, str++)
  {            // Allow end space
    if (!my_isspace(cs,*str))
      return 0;
  }
  return 1;
}
#endif


/**
   Interpret field value as an integer but return the result as a string.

   This is used for printing bit_fields as numbers while debugging.
*/

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
Field::Field(unsigned char *ptr_arg,uint32_t length_arg,unsigned char *null_ptr_arg,
             unsigned char null_bit_arg,
             utype unireg_check_arg, const char *field_name_arg)
  :ptr(ptr_arg), null_ptr(null_ptr_arg),
   table(0), orig_table(0), table_name(0),
   field_name(field_name_arg),
   key_start(0), part_of_key(0), part_of_key_not_clustered(0),
   part_of_sortkey(0), unireg_check(unireg_check_arg),
   field_length(length_arg), null_bit(null_bit_arg),
   is_created_from_null_item(false),
   vcol_info(NULL), is_stored(true)
{
  flags=null_ptr ? 0: NOT_NULL_FLAG;
  comment.str= (char*) "";
  comment.length=0;
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

size_t
Field::do_last_null_byte() const
{
  assert(null_ptr == NULL || null_ptr >= table->record[0]);
  if (null_ptr)
    return (size_t) (null_ptr - table->record[0]) + 1;
  return LAST_NULL_BYTE_UNDEF;
}


void Field::copy_from_tmp(int row_offset)
{
  memcpy(ptr,ptr+row_offset,pack_length());
  if (null_ptr)
  {
    *null_ptr= (unsigned char) ((null_ptr[0] & (unsigned char) ~(uint32_t) null_bit) | (null_ptr[row_offset] & (unsigned char) null_bit));
  }
}


bool Field::send_binary(Protocol *protocol)
{
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),charset());
  val_str(&tmp);
  return protocol->store(tmp.ptr(), tmp.length(), tmp.charset());
}


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
int Field::compatible_field_size(uint32_t field_metadata)
{
  uint32_t const source_size= pack_length_from_metadata(field_metadata);
  uint32_t const destination_size= row_pack_length();
  return (source_size <= destination_size);
}


int Field::store(const char *to, uint32_t length, const CHARSET_INFO * const cs,
                 enum_check_fields check_level)
{
  int res;
  enum_check_fields old_check_level= table->in_use->count_cuted_fields;
  table->in_use->count_cuted_fields= check_level;
  res= store(to, length, cs);
  table->in_use->count_cuted_fields= old_check_level;
  return res;
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
unsigned char *
Field::pack(unsigned char *to, const unsigned char *from, uint32_t max_length,
            bool low_byte_first __attribute__((unused)))
{
  uint32_t length= pack_length();
  set_if_smaller(length, max_length);
  memcpy(to, from, length);
  return to+length;
}

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
const unsigned char *
Field::unpack(unsigned char* to, const unsigned char *from, uint32_t param_data,
              bool low_byte_first __attribute__((unused)))
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
  return from+len;
}


my_decimal *Field::val_decimal(my_decimal *decimal __attribute__((unused)))
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
  field->length=field_length;
  field->type=type();
  field->flags=table->maybe_null ? (flags & ~NOT_NULL_FLAG) : flags;
  field->decimals= 0;
}


/**
   Conversion from decimal to int64_t with checking overflow and
   setting correct value (min/max) in case of overflow.

   @param val             value which have to be converted
   @param unsigned_flag   type of integer in which we convert val
   @param err             variable to pass error code

   @return
   value converted from val
*/
int64_t Field::convert_decimal2int64_t(const my_decimal *val,
                                       bool unsigned_flag __attribute__((unused)), int *err)
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

/**
   This is called when storing a date in a string.

   @note
   Needs to be changed if/when we want to support different time formats.
*/

int Field::store_time(DRIZZLE_TIME *ltime,
                      enum enum_drizzle_timestamp_type type_arg __attribute__((unused)))
{
  char buff[MAX_DATE_STRING_REP_LENGTH];
  uint32_t length= (uint32_t) my_TIME_to_str(ltime, buff);
  return store(buff, length, &my_charset_bin);
}


bool Field::optimize_range(uint32_t idx, uint32_t part)
{
  return test(table->file->index_flags(idx, part, 1) & HA_READ_RANGE);
}


Field *Field::new_field(MEM_ROOT *root, Table *new_table,
                        bool keep_type __attribute__((unused)))
{
  Field *tmp;
  if (!(tmp= (Field*) memdup_root(root,(char*) this,size_of())))
    return 0;

  if (tmp->table->maybe_null)
    tmp->flags&= ~NOT_NULL_FLAG;
  tmp->table= new_table;
  tmp->key_start.init(0);
  tmp->part_of_key.init(0);
  tmp->part_of_sortkey.init(0);
  tmp->unireg_check= Field::NONE;
  tmp->flags&= (NOT_NULL_FLAG | BLOB_FLAG | UNSIGNED_FLAG | BINARY_FLAG | ENUM_FLAG | SET_FLAG);
  tmp->reset_fields();
  return tmp;
}


Field *Field::new_key_field(MEM_ROOT *root, Table *new_table,
                            unsigned char *new_ptr, unsigned char *new_null_ptr,
                            uint32_t new_null_bit)
{
  Field *tmp;
  if ((tmp= new_field(root, new_table, table == new_table)))
  {
    tmp->ptr=      new_ptr;
    tmp->null_ptr= new_null_ptr;
    tmp->null_bit= new_null_bit;
  }
  return tmp;
}


/* This is used to generate a field in Table from TABLE_SHARE */

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


/*
  Report "not well formed" or "cannot convert" error
  after storing a character string info a field.

  SYNOPSIS
  check_string_copy_error()
  field                    - Field
  well_formed_error_pos    - where not well formed data was first met
  cannot_convert_error_pos - where a not-convertable character was first met
  end                      - end of the string
  cs                       - character set of the string

  NOTES
  As of version 5.0 both cases return the same error:

  "Invalid string value: 'xxx' for column 't' at row 1"

  Future versions will possibly introduce a new error message:

  "Cannot convert character string: 'xxx' for column 't' at row 1"

  RETURN
  false - If errors didn't happen
  true  - If an error happened
*/

bool
check_string_copy_error(Field_str *field,
                        const char *well_formed_error_pos,
                        const char *cannot_convert_error_pos,
                        const char *end,
                        const CHARSET_INFO * const cs)
{
  const char *pos, *end_orig;
  char tmp[64], *t;

  if (!(pos= well_formed_error_pos) &&
      !(pos= cannot_convert_error_pos))
    return false;

  end_orig= end;
  set_if_smaller(end, pos + 6);

  for (t= tmp; pos < end; pos++)
  {
    /*
      If the source string is ASCII compatible (mbminlen==1)
      and the source character is in ASCII printable range (0x20..0x7F),
      then display the character as is.

      Otherwise, if the source string is not ASCII compatible (e.g. UCS2),
      or the source character is not in the printable range,
      then print the character using HEX notation.
    */
    if (((unsigned char) *pos) >= 0x20 &&
        ((unsigned char) *pos) <= 0x7F &&
        cs->mbminlen == 1)
    {
      *t++= *pos;
    }
    else
    {
      *t++= '\\';
      *t++= 'x';
      *t++= _dig_vec_upper[((unsigned char) *pos) >> 4];
      *t++= _dig_vec_upper[((unsigned char) *pos) & 15];
    }
  }
  if (end_orig > end)
  {
    *t++= '.';
    *t++= '.';
    *t++= '.';
  }
  *t= '\0';
  push_warning_printf(field->table->in_use,
                      field->table->in_use->abort_on_warning ?
                      DRIZZLE_ERROR::WARN_LEVEL_ERROR :
                      DRIZZLE_ERROR::WARN_LEVEL_WARN,
                      ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
                      ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                      "string", tmp, field->field_name,
                      (uint32_t) field->table->in_use->row_count);
  return true;
}

uint32_t Field::is_equal(Create_field *new_field)
{
  return (new_field->sql_type == real_type());
}

/**
   @retval
   1  if the fields are equally defined
   @retval
   0  if the fields are unequally defined
*/

bool Field::eq_def(Field *field)
{
  if (real_type() != field->real_type() || charset() != field->charset() ||
      pack_length() != field->pack_length())
    return 0;
  return 1;
}

/**
   @return
   returns 1 if the fields are equally defined
*/
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
    /* Pack_length already calculated in sql_parse.cc */
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
  vcol_info= NULL;
  is_stored= true;
}


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
   @param fld_vcol_info         Virtual column data

   @retval
   false on success
   @retval
   true  on error
*/

bool Create_field::init(Session *session __attribute__((unused)), char *fld_name, enum_field_types fld_type,
                        char *fld_length, char *fld_decimals,
                        uint32_t fld_type_modifier, Item *fld_default_value,
                        Item *fld_on_update_value, LEX_STRING *fld_comment,
                        char *fld_change, List<String> *fld_interval_list,
                        const CHARSET_INFO * const fld_charset,
                        uint32_t fld_geom_type __attribute__((unused)),
                        enum column_format_type column_format,
                        virtual_column_info *fld_vcol_info)
{
  uint32_t sign_len, allowed_type_modifier= 0;
  uint32_t max_field_charlength= MAX_FIELD_CHARLENGTH;

  field= 0;
  field_name= fld_name;
  def= fld_default_value;
  flags= fld_type_modifier;
  flags|= (((uint32_t)column_format & COLUMN_FORMAT_MASK) << COLUMN_FORMAT_FLAGS);
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
  vcol_info= fld_vcol_info;
  is_stored= true;

  /* Initialize data for a virtual field */
  if (fld_type == DRIZZLE_TYPE_VIRTUAL)
  {
    assert(vcol_info && vcol_info->expr_item);
    is_stored= vcol_info->get_field_stored();
    /*
      Perform per item-type checks to determine if the expression is
      allowed for a virtual column.
      Note that validation of the specific function is done later in
      procedures open_table_from_share and fix_fields_vcol_func
    */
    switch (vcol_info->expr_item->type()) {
    case Item::FUNC_ITEM:
      if (((Item_func *)vcol_info->expr_item)->functype() == Item_func::FUNC_SP)
      {
        my_error(ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED, MYF(0), field_name);
        return(true);
      }
      break;
    case Item::COPY_STR_ITEM:
    case Item::FIELD_AVG_ITEM:
    case Item::PROC_ITEM:
    case Item::REF_ITEM:
    case Item::FIELD_STD_ITEM:
    case Item::FIELD_VARIANCE_ITEM:
    case Item::INSERT_VALUE_ITEM:
    case Item::SUBSELECT_ITEM:
    case Item::CACHE_ITEM:
    case Item::TYPE_HOLDER:
    case Item::PARAM_ITEM:
    case Item::VIEW_FIXER_ITEM:
      my_error(ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED, MYF(0), field_name);
      return true;
      break;
    default:
      // Continue with the field creation
      break;
    }
    /*
      Make a field created for the real type.
      Note that "real" and virtual fields differ from each other
      only by Field::vcol_info, which is always 0 for normal columns.
      vcol_info is updated for fields later in procedure open_binary_frm.
    */
    sql_type= fld_type= vcol_info->get_real_type();
  }

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
  case DRIZZLE_TYPE_NEWDATE:
    length= 10;
    break;
  case DRIZZLE_TYPE_TIME:
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
  case DRIZZLE_TYPE_VIRTUAL: // Must not happen
    assert(0);
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


enum_field_types get_blob_type_from_length(uint32_t length __attribute__((unused)))
{
  enum_field_types type;

  type= DRIZZLE_TYPE_BLOB;

  return type;
}


/*
  Make a field from the .frm file info
*/

uint32_t calc_pack_length(enum_field_types type,uint32_t length)
{
  switch (type) {
  case DRIZZLE_TYPE_VARCHAR:     return (length + (length < 256 ? 1: 2));
  case DRIZZLE_TYPE_TINY  : return 1;
  case DRIZZLE_TYPE_NEWDATE:
  case DRIZZLE_TYPE_TIME:   return 3;
  case DRIZZLE_TYPE_TIMESTAMP:
  case DRIZZLE_TYPE_LONG  : return 4;
  case DRIZZLE_TYPE_DOUBLE: return sizeof(double);
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_LONGLONG: return 8;  /* Don't crash if no int64_t */
  case DRIZZLE_TYPE_NULL  : return 0;
  case DRIZZLE_TYPE_BLOB:    return 4+portable_sizeof_char_ptr;
  case DRIZZLE_TYPE_ENUM:
  case DRIZZLE_TYPE_NEWDECIMAL:
    abort(); return 0;                          // This shouldn't happen
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
  return 0;          // This shouldn't happen
}


Field *make_field(TABLE_SHARE *share, unsigned char *ptr, uint32_t field_length,
                  unsigned char *null_pos, unsigned char null_bit,
                  uint32_t pack_flag,
                  enum_field_types field_type,
                  const CHARSET_INFO * field_charset,
                  Field::utype unireg_check,
                  TYPELIB *interval,
                  const char *field_name)
{
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
  case DRIZZLE_TYPE_NEWDATE:
  case DRIZZLE_TYPE_TIME:
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
        return new Field_varstring(ptr,field_length,
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
      return new Field_blob(ptr,null_pos,null_bit,
                            unireg_check, field_name, share,
                            pack_length, field_charset);
    if (interval)
    {
      if (f_is_enum(pack_flag))
      {
        return new Field_enum(ptr,field_length,null_pos,null_bit,
                              unireg_check, field_name,
                              get_enum_pack_length(interval->count),
                              interval, field_charset);
      }
    }
  }

  switch (field_type) {
  case DRIZZLE_TYPE_NEWDECIMAL:
    return new Field_new_decimal(ptr,field_length,null_pos,null_bit,
                                 unireg_check, field_name,
                                 f_decimals(pack_flag),
                                 f_is_decimal_precision(pack_flag) != 0,
                                 f_is_dec(pack_flag) == 0);
  case DRIZZLE_TYPE_DOUBLE:
    return new Field_double(ptr,field_length,null_pos,null_bit,
                            unireg_check, field_name,
                            f_decimals(pack_flag),
                            false,
                            f_is_dec(pack_flag)== 0);
  case DRIZZLE_TYPE_TINY:
    assert(0);
  case DRIZZLE_TYPE_LONG:
    return new Field_long(ptr,field_length,null_pos,null_bit,
                          unireg_check, field_name,
                          false,
                          f_is_dec(pack_flag) == 0);
  case DRIZZLE_TYPE_LONGLONG:
    return new Field_int64_t(ptr,field_length,null_pos,null_bit,
                             unireg_check, field_name,
                             false,
                             f_is_dec(pack_flag) == 0);
  case DRIZZLE_TYPE_TIMESTAMP:
    return new Field_timestamp(ptr,field_length, null_pos, null_bit,
                               unireg_check, field_name, share,
                               field_charset);
  case DRIZZLE_TYPE_NEWDATE:
    return new Field_newdate(ptr,null_pos,null_bit,
                             unireg_check, field_name, field_charset);
  case DRIZZLE_TYPE_TIME:
    return new Field_time(ptr,null_pos,null_bit,
                          unireg_check, field_name, field_charset);
  case DRIZZLE_TYPE_DATETIME:
    return new Field_datetime(ptr,null_pos,null_bit,
                              unireg_check, field_name, field_charset);
  case DRIZZLE_TYPE_NULL:
    return new Field_null(ptr, field_length, unireg_check, field_name,
                          field_charset);
  case DRIZZLE_TYPE_VIRTUAL:                    // Must not happen
    assert(0);
  default:          // Impossible (Wrong version)
    break;
  }
  return 0;
}


/** Create a field suitable for create of table. */

Create_field::Create_field(Field *old_field,Field *orig_field)
{
  field=      old_field;
  field_name=change=old_field->field_name;
  length=     old_field->field_length;
  flags=      old_field->flags;
  unireg_check=old_field->unireg_check;
  pack_length=old_field->pack_length();
  key_length= old_field->key_length();
  sql_type=   old_field->real_type();
  charset=    old_field->charset();    // May be NULL ptr
  comment=    old_field->comment;
  decimals=   old_field->decimals();
  vcol_info=  old_field->vcol_info;
  is_stored= old_field->is_stored;

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
    interval=0;
  def=0;
  char_length= length;

  if (!(flags & (NO_DEFAULT_VALUE_FLAG | BLOB_FLAG)) &&
      old_field->ptr && orig_field &&
      (sql_type != DRIZZLE_TYPE_TIMESTAMP ||                /* set def only if */
       old_field->table->timestamp_field != old_field ||  /* timestamp field */
       unireg_check == Field::TIMESTAMP_UN_FIELD))        /* has default val */
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff), charset);
    my_ptrdiff_t diff;

    /* Get the value from default_values */
    diff= (my_ptrdiff_t) (orig_field->table->s->default_values-
                          orig_field->table->record[0]);
    orig_field->move_field_offset(diff);  // Points now at default_values
    if (!orig_field->is_real_null())
    {
      char buff[MAX_FIELD_WIDTH], *pos;
      String tmp(buff, sizeof(buff), charset), *res;
      res= orig_field->val_str(&tmp);
      pos= (char*) sql_strmake(res->ptr(), res->length());
      def= new Item_string(pos, res->length(), charset);
    }
    orig_field->move_field_offset(-diff);  // Back to record[0]
  }
}


/*****************************************************************************
 Warning handling
*****************************************************************************/

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

bool
Field::set_warning(DRIZZLE_ERROR::enum_warning_level level, uint32_t code,
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

void
Field::set_datetime_warning(DRIZZLE_ERROR::enum_warning_level level,
                            unsigned int code,
                            const char *str, uint32_t str_length,
                            enum enum_drizzle_timestamp_type ts_type, int cuted_increment)
{
  Session *session= table ? table->in_use : current_session;
  if ((session->really_abort_on_warning() &&
       level >= DRIZZLE_ERROR::WARN_LEVEL_WARN) ||
      set_warning(level, code, cuted_increment))
    make_truncated_value_warning(session, level, str, str_length, ts_type,
                                 field_name);
}


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

void
Field::set_datetime_warning(DRIZZLE_ERROR::enum_warning_level level, uint32_t code,
                            int64_t nr, enum enum_drizzle_timestamp_type ts_type,
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

void
Field::set_datetime_warning(DRIZZLE_ERROR::enum_warning_level level, uint32_t code,
                            double nr, enum enum_drizzle_timestamp_type ts_type)
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
