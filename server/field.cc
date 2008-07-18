/* Copyright (C) 2000-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/**
  @file

  @brief
  This file implements classes defined in field.h
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"
#include <m_ctype.h>
#include <errno.h>

// Maximum allowed exponent value for converting string to decimal
#define MAX_EXPONENT 1024

/*****************************************************************************
  Instansiate templates and static variables
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List<Create_field>;
template class List_iterator<Create_field>;
#endif

const char field_separator=',';

/*
  Rules for merging different types of fields in UNION

  NOTE: to avoid 256*256 table, gap in table types numeration is skiped
  following #defines describe that gap and how to canculate number of fields
  and index of field in thia array.
*/
#define FIELDTYPE_TEAR_FROM (MYSQL_TYPE_VARCHAR + 1)
#define FIELDTYPE_TEAR_TO   (MYSQL_TYPE_NEWDECIMAL - 1)
#define FIELDTYPE_NUM (FIELDTYPE_TEAR_FROM + (255 - FIELDTYPE_TEAR_TO))
inline int field_type2index (enum_field_types field_type)
{
  return (field_type < FIELDTYPE_TEAR_FROM ?
          field_type :
          ((int)FIELDTYPE_TEAR_FROM) + (field_type - FIELDTYPE_TEAR_TO) - 1);
}


static enum_field_types field_types_merge_rules [FIELDTYPE_NUM][FIELDTYPE_NUM]=
{
  /* MYSQL_TYPE_DECIMAL -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_NEWDECIMAL,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_NEWDECIMAL,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_NEWDECIMAL,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING       
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_TINY -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_TINY,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_SHORT,       MYSQL_TYPE_LONG,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_TINY,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_LONGLONG,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_TINY,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING       
    MYSQL_TYPE_STRING     
  },
  /* MYSQL_TYPE_SHORT -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_SHORT,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_SHORT,       MYSQL_TYPE_LONG,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_SHORT,       MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_LONGLONG,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_SHORT,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_LONG -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_LONG,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_LONG,        MYSQL_TYPE_LONG,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_LONG,         MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_LONGLONG,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_LONG,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_DOUBLE -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_DOUBLE,      MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_DOUBLE,      MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_DOUBLE,      MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_DOUBLE,      MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_NULL -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_TINY,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_SHORT,       MYSQL_TYPE_LONG,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_NULL,        MYSQL_TYPE_TIMESTAMP,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_LONGLONG,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_TIME,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_YEAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_ENUM,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_SET,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_TIMESTAMP -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_TIMESTAMP,   MYSQL_TYPE_TIMESTAMP,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_DATETIME,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_LONGLONG -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_LONGLONG,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_LONGLONG,    MYSQL_TYPE_LONGLONG,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_LONGLONG,    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_LONGLONG,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_LONGLONG,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_DATE -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_DATETIME,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_DATETIME,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_TIME -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_TIME,        MYSQL_TYPE_DATETIME,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_TIME,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_DATETIME -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_DATETIME,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_DATETIME,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_YEAR -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_TINY,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_SHORT,       MYSQL_TYPE_LONG,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_YEAR,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_LONGLONG,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_YEAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_NEWDATE -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_DATETIME,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_DATETIME,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_DATETIME,    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_NEWDATE,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_VARCHAR -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_VARCHAR
  },
  /* MYSQL_TYPE_NEWDECIMAL -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_NEWDECIMAL,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_NEWDECIMAL,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_DOUBLE,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_NEWDECIMAL,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_NEWDECIMAL,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_NEWDECIMAL,  MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_ENUM -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_ENUM,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_SET -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_SET,         MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  },
  /* MYSQL_TYPE_BLOB -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_BLOB,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_BLOB
  },
  /* MYSQL_TYPE_VAR_STRING -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_VARCHAR,     MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_VARCHAR
  },
  /* MYSQL_TYPE_STRING -> */
  {
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
    MYSQL_TYPE_STRING,      MYSQL_TYPE_STRING,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
    MYSQL_TYPE_STRING,      MYSQL_TYPE_STRING,
  //MYSQL_TYPE_DOUBLE
    MYSQL_TYPE_STRING,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
    MYSQL_TYPE_STRING,      MYSQL_TYPE_STRING,
  //MYSQL_TYPE_LONGLONG
    MYSQL_TYPE_STRING,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
    MYSQL_TYPE_STRING,      MYSQL_TYPE_STRING,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
    MYSQL_TYPE_STRING,      MYSQL_TYPE_STRING,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
    MYSQL_TYPE_STRING,      MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
    MYSQL_TYPE_STRING,      MYSQL_TYPE_STRING,
  //MYSQL_TYPE_SET
    MYSQL_TYPE_STRING,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
    MYSQL_TYPE_BLOB,        MYSQL_TYPE_VARCHAR,
  //MYSQL_TYPE_STRING
    MYSQL_TYPE_STRING
  }
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
  assert(a < FIELDTYPE_TEAR_FROM || a > FIELDTYPE_TEAR_TO);
  assert(b < FIELDTYPE_TEAR_FROM || b > FIELDTYPE_TEAR_TO);
  return field_types_merge_rules[field_type2index(a)]
                                [field_type2index(b)];
}


static Item_result field_types_result_type [FIELDTYPE_NUM]=
{
  //MYSQL_TYPE_DECIMAL      MYSQL_TYPE_TINY
  DECIMAL_RESULT,           INT_RESULT,
  //MYSQL_TYPE_SHORT        MYSQL_TYPE_LONG
  INT_RESULT,               INT_RESULT,
  //MYSQL_TYPE_DOUBLE
  REAL_RESULT,
  //MYSQL_TYPE_NULL         MYSQL_TYPE_TIMESTAMP
  STRING_RESULT,            STRING_RESULT,
  //MYSQL_TYPE_LONGLONG
  INT_RESULT,
  //MYSQL_TYPE_DATE         MYSQL_TYPE_TIME
  STRING_RESULT,            STRING_RESULT,
  //MYSQL_TYPE_DATETIME     MYSQL_TYPE_YEAR
  STRING_RESULT,            INT_RESULT,
  //MYSQL_TYPE_NEWDATE      MYSQL_TYPE_VARCHAR
  STRING_RESULT,            STRING_RESULT,
  //MYSQL_TYPE_NEWDECIMAL   MYSQL_TYPE_ENUM
  DECIMAL_RESULT,           STRING_RESULT,
  //MYSQL_TYPE_SET
  STRING_RESULT,
  //MYSQL_TYPE_BLOB         MYSQL_TYPE_VAR_STRING
  STRING_RESULT,            STRING_RESULT,
  //MYSQL_TYPE_STRING
  STRING_RESULT
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

static bool
test_if_important_data(CHARSET_INFO *cs, const char *str, const char *strend)
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
  assert(field_type < FIELDTYPE_TEAR_FROM || field_type
              > FIELDTYPE_TEAR_TO);
  return field_types_result_type[field_type2index(field_type)];
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
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_STRING:
    return true;
  default:
    return false;
  }
}


/**
  Numeric fields base class constructor.
*/
Field_num::Field_num(uchar *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
                     uchar null_bit_arg, utype unireg_check_arg,
                     const char *field_name_arg,
                     uint8 dec_arg, bool zero_arg, bool unsigned_arg)
  :Field(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
         unireg_check_arg, field_name_arg),
  dec(dec_arg),zerofill(zero_arg),unsigned_flag(unsigned_arg)
{
  if (zerofill)
    flags|=ZEROFILL_FLAG;
  if (unsigned_flag)
    flags|=UNSIGNED_FLAG;
}


void Field_num::prepend_zeros(String *value)
{
  int diff;
  if ((diff= (int) (field_length - value->length())) > 0)
  {
    bmove_upp((uchar*) value->ptr()+field_length,
              (uchar*) value->ptr()+value->length(),
	      value->length());
    bfill((uchar*) value->ptr(),diff,'0');
    value->length(field_length);
    (void) value->c_ptr_quick();		// Avoid warnings in purify
  }
}

/**
  Test if given number is a int.

  @todo
    Make this multi-byte-character safe

  @param str		String to test
  @param length        Length of 'str'
  @param int_end	Pointer to char after last used digit
  @param cs		Character set

  @note
    This is called after one has called strntoull10rnd() function.

  @retval
    0	OK
  @retval
    1	error: empty string or wrong integer.
  @retval
    2   error: garbage at the end of string.
*/

int Field_num::check_int(CHARSET_INFO *cs, const char *str, int length, 
                         const char *int_end, int error)
{
  /* Test if we get an empty string or wrong integer */
  if (str == int_end || error == MY_ERRNO_EDOM)
  {
    char buff[128];
    String tmp(buff, (uint32) sizeof(buff), system_charset_info);
    tmp.copy(str, length, system_charset_info);
    push_warning_printf(table->in_use, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE_FOR_FIELD, 
                        ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                        "integer", tmp.c_ptr(), field_name,
                        (ulong) table->in_use->row_count);
    return 1;
  }
  /* Test if we have garbage at the end of the given string. */
  if (test_if_important_data(cs, int_end, str + length))
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
    return 2;
  }
  return 0;
}


/*
  Conver a string to an integer then check bounds.
  
  SYNOPSIS
    Field_num::get_int
    cs            Character set
    from          String to convert
    len           Length of the string
    rnd           OUT int64_t value
    unsigned_max  max unsigned value
    signed_min    min signed value
    signed_max    max signed value

  DESCRIPTION
    The function calls strntoull10rnd() to get an integer value then
    check bounds and errors returned. In case of any error a warning
    is raised.

  RETURN
    0   ok
    1   error
*/

bool Field_num::get_int(CHARSET_INFO *cs, const char *from, uint len,
                        int64_t *rnd, uint64_t unsigned_max, 
                        int64_t signed_min, int64_t signed_max)
{
  char *end;
  int error;
  
  *rnd= (int64_t) cs->cset->strntoull10rnd(cs, from, len,
                                            unsigned_flag, &end,
                                            &error);
  if (unsigned_flag)
  {

    if ((((uint64_t) *rnd > unsigned_max) && (*rnd= (int64_t) unsigned_max)) ||
        error == MY_ERRNO_ERANGE)
    {
      goto out_of_range;
    }
  }
  else
  {
    if (*rnd < signed_min)
    {
      *rnd= signed_min;
      goto out_of_range;
    }
    else if (*rnd > signed_max)
    {
      *rnd= signed_max;
      goto out_of_range;
    }
  }
  if (table->in_use->count_cuted_fields &&
      check_int(cs, from, len, end, error))
    return 1;
  return 0;

out_of_range:
  set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
  return 1;
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
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    return 1;
  }
  if (op_result == E_DEC_TRUNCATED)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_NOTE, WARN_DATA_TRUNCATED, 1);
    /* We return 0 here as this is not a critical issue */
  }
  return 0;
}


#ifdef NOT_USED
static bool test_if_real(const char *str,int length, CHARSET_INFO *cs)
{
  cs= system_charset_info; // QQ move test_if_real into CHARSET_INFO struct

  while (length && my_isspace(cs,*str))
  {						// Allow start space
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
  {						// Allow end space
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

String *Field::val_int_as_str(String *val_buffer, my_bool unsigned_val)
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  int64_t value= val_int();

  if (val_buffer->alloc(MY_INT64_NUM_DECIMAL_DIGITS))
    return 0;
  length= (uint) (*cs->cset->int64_t10_to_str)(cs, (char*) val_buffer->ptr(),
                                                MY_INT64_NUM_DECIMAL_DIGITS,
                                                unsigned_val ? 10 : -10,
                                                value);
  val_buffer->length(length);
  return val_buffer;
}


/// This is used as a table name when the table structure is not set up
Field::Field(uchar *ptr_arg,uint32 length_arg,uchar *null_ptr_arg,
	     uchar null_bit_arg,
	     utype unireg_check_arg, const char *field_name_arg)
  :ptr(ptr_arg), null_ptr(null_ptr_arg),
   table(0), orig_table(0), table_name(0),
   field_name(field_name_arg),
   key_start(0), part_of_key(0), part_of_key_not_clustered(0),
   part_of_sortkey(0), unireg_check(unireg_check_arg),
   field_length(length_arg), null_bit(null_bit_arg), 
   is_created_from_null_item(false)
{
  flags=null_ptr ? 0: NOT_NULL_FLAG;
  comment.str= (char*) "";
  comment.length=0;
  field_index= 0;
}


void Field::hash(ulong *nr, ulong *nr2)
{
  if (is_null())
  {
    *nr^= (*nr << 1) | 1;
  }
  else
  {
    uint len= pack_length();
    CHARSET_INFO *cs= charset();
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
    *null_ptr= (uchar) ((null_ptr[0] & (uchar) ~(uint) null_bit) | (null_ptr[row_offset] & (uchar) null_bit));
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
int Field::compatible_field_size(uint field_metadata)
{
  uint const source_size= pack_length_from_metadata(field_metadata);
  uint const destination_size= row_pack_length();
  return (source_size <= destination_size);
}


int Field::store(const char *to, uint length, CHARSET_INFO *cs,
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
uchar *
Field::pack(uchar *to, const uchar *from, uint max_length,
            bool low_byte_first __attribute__((unused)))
{
  uint32 length= pack_length();
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
const uchar *
Field::unpack(uchar* to, const uchar *from, uint param_data,
              bool low_byte_first __attribute__((unused)))
{
  uint length=pack_length();
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

  uint len= (param_data && (param_data < length)) ?
            param_data : length;

  memcpy(to, from, param_data > length ? length : len);
  return from+len;
}


my_decimal *Field::val_decimal(my_decimal *decimal __attribute__((__unused__)))
{
  /* This never have to be called */
  assert(0);
  return 0;
}


void Field_num::add_zerofill_and_unsigned(String &res) const
{
  if (unsigned_flag)
    res.append(STRING_WITH_LEN(" unsigned"));
  if (zerofill)
    res.append(STRING_WITH_LEN(" zerofill"));
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
                                         bool unsigned_flag, int *err)
{
  int64_t i;
  if (unsigned_flag)
  {
    if (val->sign())
    {
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      i= 0;
      *err= 1;
    }
    else if (warn_if_overflow(my_decimal2int(E_DEC_ERROR &
                                           ~E_DEC_OVERFLOW & ~E_DEC_TRUNCATED,
                                           val, true, &i)))
    {
      i= ~(int64_t) 0;
      *err= 1;
    }
  }
  else if (warn_if_overflow(my_decimal2int(E_DEC_ERROR &
                                         ~E_DEC_OVERFLOW & ~E_DEC_TRUNCATED,
                                         val, false, &i)))
  {
    i= (val->sign() ? INT64_MIN : INT64_MAX);
    *err= 1;
  }
  return i;
}


/**
  Storing decimal in integer fields.

  @param val       value for storing

  @note
    This method is used by all integer fields, real/decimal redefine it

  @retval
    0     OK
  @retval
    !=0  error
*/

int Field_num::store_decimal(const my_decimal *val)
{
  int err= 0;
  int64_t i= convert_decimal2int64_t(val, unsigned_flag, &err);
  return test(err | store(i, unsigned_flag));
}


/**
  Return decimal value of integer field.

  @param decimal_value     buffer for storing decimal value

  @note
    This method is used by all integer fields, real/decimal redefine it.
    All int64_t values fit in our decimal buffer which cal store 8*9=72
    digits of integer number

  @return
    pointer to decimal buffer with value of field
*/

my_decimal* Field_num::val_decimal(my_decimal *decimal_value)
{
  assert(result_type() == INT_RESULT);
  int64_t nr= val_int();
  int2my_decimal(E_DEC_FATAL_ERROR, nr, unsigned_flag, decimal_value);
  return decimal_value;
}


Field_str::Field_str(uchar *ptr_arg,uint32 len_arg, uchar *null_ptr_arg,
                     uchar null_bit_arg, utype unireg_check_arg,
                     const char *field_name_arg, CHARSET_INFO *charset_arg)
  :Field(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
         unireg_check_arg, field_name_arg)
{
  field_charset= charset_arg;
  if (charset_arg->state & MY_CS_BINSORT)
    flags|=BINARY_FLAG;
  field_derivation= DERIVATION_IMPLICIT;
}


void Field_num::make_field(Send_field *field)
{
  Field::make_field(field);
  field->decimals= dec;
}

/**
  Decimal representation of Field_str.

  @param d         value for storing

  @note
    Field_str is the base class for fields like Field_enum,
    Field_date and some similar. Some dates use fraction and also
    string value should be converted to floating point value according
    our rules, so we use double to store value of decimal in string.

  @todo
    use decimal2string?

  @retval
    0     OK
  @retval
    !=0  error
*/

int Field_str::store_decimal(const my_decimal *d)
{
  double val;
  /* TODO: use decimal2string? */
  int err= warn_if_overflow(my_decimal2double(E_DEC_FATAL_ERROR &
                                            ~E_DEC_OVERFLOW, d, &val));
  return err | store(val);
}


my_decimal *Field_str::val_decimal(my_decimal *decimal_value)
{
  int64_t nr= val_int();
  int2my_decimal(E_DEC_FATAL_ERROR, nr, 0, decimal_value);
  return decimal_value;
}


uint Field::fill_cache_field(CACHE_FIELD *copy)
{
  uint store_length;
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
  else if (!zero_pack() &&
           (type() == MYSQL_TYPE_STRING && copy->length >= 4 &&
            copy->length < 256))
  {
    copy->strip=1;				/* Remove end space */
    store_length= 2;
  }
  else
  {
    copy->strip=0;
    store_length= 0;
  }
  return copy->length+ store_length;
}


bool Field::get_date(MYSQL_TIME *ltime,uint fuzzydate)
{
  char buff[40];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_datetime_with_warn(res->ptr(), res->length(),
                                ltime, fuzzydate) <= MYSQL_TIMESTAMP_ERROR)
    return 1;
  return 0;
}

bool Field::get_time(MYSQL_TIME *ltime)
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

int Field::store_time(MYSQL_TIME *ltime,
                      timestamp_type type_arg __attribute__((__unused__)))
{
  char buff[MAX_DATE_STRING_REP_LENGTH];
  uint length= (uint) my_TIME_to_str(ltime, buff);
  return store(buff, length, &my_charset_bin);
}


bool Field::optimize_range(uint idx, uint part)
{
  return test(table->file->index_flags(idx, part, 1) & HA_READ_RANGE);
}


Field *Field::new_field(MEM_ROOT *root, struct st_table *new_table,
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
  tmp->flags&= (NOT_NULL_FLAG | BLOB_FLAG | UNSIGNED_FLAG |
                ZEROFILL_FLAG | BINARY_FLAG | ENUM_FLAG | SET_FLAG);
  tmp->reset_fields();
  return tmp;
}


Field *Field::new_key_field(MEM_ROOT *root, struct st_table *new_table,
                            uchar *new_ptr, uchar *new_null_ptr,
                            uint new_null_bit)
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


/* This is used to generate a field in TABLE from TABLE_SHARE */

Field *Field::clone(MEM_ROOT *root, struct st_table *new_table)
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


/****************************************************************************
** tiny int
****************************************************************************/

int Field_tiny::store(const char *from,uint len,CHARSET_INFO *cs)
{
  int error;
  int64_t rnd;
  
  error= get_int(cs, from, len, &rnd, 255, -128, 127);
  ptr[0]= unsigned_flag ? (char) (uint64_t) rnd : (char) rnd;
  return error;
}


int Field_tiny::store(double nr)
{
  int error= 0;
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0.0)
    {
      *ptr=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if (nr > 255.0)
    {
      *ptr=(char) 255;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      *ptr=(char) nr;
  }
  else
  {
    if (nr < -128.0)
    {
      *ptr= (char) -128;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if (nr > 127.0)
    {
      *ptr=127;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      *ptr=(char) (int) nr;
  }
  return error;
}


int Field_tiny::store(int64_t nr, bool unsigned_val)
{
  int error= 0;

  if (unsigned_flag)
  {
    if (nr < 0 && !unsigned_val)
    {
      *ptr= 0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if ((uint64_t) nr > (uint64_t) 255)
    {
      *ptr= (char) 255;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      *ptr=(char) nr;
  }
  else
  {
    if (nr < 0 && unsigned_val)
      nr= 256;                                    // Generate overflow
    if (nr < -128)
    {
      *ptr= (char) -128;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if (nr > 127)
    {
      *ptr=127;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      *ptr=(char) nr;
  }
  return error;
}


double Field_tiny::val_real(void)
{
  int tmp= unsigned_flag ? (int) ptr[0] :
    (int) ((signed char*) ptr)[0];
  return (double) tmp;
}


int64_t Field_tiny::val_int(void)
{
  int tmp= unsigned_flag ? (int) ptr[0] :
    (int) ((signed char*) ptr)[0];
  return (int64_t) tmp;
}


String *Field_tiny::val_str(String *val_buffer,
			    String *val_ptr __attribute__((unused)))
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,5*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();

  if (unsigned_flag)
    length= (uint) cs->cset->long10_to_str(cs,to,mlength, 10,
					   (long) *ptr);
  else
    length= (uint) cs->cset->long10_to_str(cs,to,mlength,-10,
					   (long) *((signed char*) ptr));
  
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}

bool Field_tiny::send_binary(Protocol *protocol)
{
  return protocol->store_tiny((int64_t) (int8) ptr[0]);
}

int Field_tiny::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  signed char a,b;
  a=(signed char) a_ptr[0]; b= (signed char) b_ptr[0];
  if (unsigned_flag)
    return ((uchar) a < (uchar) b) ? -1 : ((uchar) a > (uchar) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_tiny::sort_string(uchar *to,uint length __attribute__((unused)))
{
  if (unsigned_flag)
    *to= *ptr;
  else
    to[0] = (char) (ptr[0] ^ (uchar) 128);	/* Revers signbit */
}

void Field_tiny::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			  "tinyint(%d)",(int) field_length));
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
 Field type short int (2 byte)
****************************************************************************/

int Field_short::store(const char *from,uint len,CHARSET_INFO *cs)
{
  int store_tmp;
  int error;
  int64_t rnd;
  
  error= get_int(cs, from, len, &rnd, UINT16_MAX, INT16_MIN, INT16_MAX);
  store_tmp= unsigned_flag ? (int) (uint64_t) rnd : (int) rnd;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int2store(ptr, store_tmp);
  }
  else
#endif
    shortstore(ptr, (short) store_tmp);
  return error;
}


int Field_short::store(double nr)
{
  int error= 0;
  int16 res;
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      res=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if (nr > (double) UINT16_MAX)
    {
      res=(int16) UINT16_MAX;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      res=(int16) (uint16) nr;
  }
  else
  {
    if (nr < (double) INT16_MIN)
    {
      res=INT16_MIN;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if (nr > (double) INT16_MAX)
    {
      res=INT16_MAX;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      res=(int16) (int) nr;
  }
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int2store(ptr,res);
  }
  else
#endif
    shortstore(ptr,res);
  return error;
}


int Field_short::store(int64_t nr, bool unsigned_val)
{
  int error= 0;
  int16 res;

  if (unsigned_flag)
  {
    if (nr < 0L && !unsigned_val)
    {
      res=0;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if ((uint64_t) nr > (uint64_t) UINT16_MAX)
    {
      res=(int16) UINT16_MAX;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      res=(int16) (uint16) nr;
  }
  else
  {
    if (nr < 0 && unsigned_val)
      nr= UINT16_MAX+1;                         // Generate overflow

    if (nr < INT16_MIN)
    {
      res=INT16_MIN;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if (nr > (int64_t) INT16_MAX)
    {
      res=INT16_MAX;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      res=(int16) nr;
  }
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int2store(ptr,res);
  }
  else
#endif
    shortstore(ptr,res);
  return error;
}


double Field_short::val_real(void)
{
  short j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint2korr(ptr);
  else
#endif
    shortget(j,ptr);
  return unsigned_flag ? (double) (unsigned short) j : (double) j;
}

int64_t Field_short::val_int(void)
{
  short j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint2korr(ptr);
  else
#endif
    shortget(j,ptr);
  return unsigned_flag ? (int64_t) (unsigned short) j : (int64_t) j;
}


String *Field_short::val_str(String *val_buffer,
			     String *val_ptr __attribute__((unused)))
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,7*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();
  short j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint2korr(ptr);
  else
#endif
    shortget(j,ptr);

  if (unsigned_flag)
    length=(uint) cs->cset->long10_to_str(cs, to, mlength, 10, 
					  (long) (uint16) j);
  else
    length=(uint) cs->cset->long10_to_str(cs, to, mlength,-10, (long) j);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


bool Field_short::send_binary(Protocol *protocol)
{
  return protocol->store_short(Field_short::val_int());
}


int Field_short::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  short a,b;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    a=sint2korr(a_ptr);
    b=sint2korr(b_ptr);
  }
  else
#endif
  {
    shortget(a,a_ptr);
    shortget(b,b_ptr);
  }

  if (unsigned_flag)
    return ((unsigned short) a < (unsigned short) b) ? -1 :
    ((unsigned short) a > (unsigned short) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_short::sort_string(uchar *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->s->db_low_byte_first)
  {
    if (unsigned_flag)
      to[0] = ptr[0];
    else
      to[0] = (char) (ptr[0] ^ 128);		/* Revers signbit */
    to[1]   = ptr[1];
  }
  else
#endif
  {
    if (unsigned_flag)
      to[0] = ptr[1];
    else
      to[0] = (char) (ptr[1] ^ 128);		/* Revers signbit */
    to[1]   = ptr[0];
  }
}

void Field_short::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			  "smallint(%d)",(int) field_length));
  add_zerofill_and_unsigned(res);
}


/****************************************************************************
 Field type int64_t int (8 bytes)
****************************************************************************/

int Field_int64_t::store(const char *from,uint len,CHARSET_INFO *cs)
{
  int error= 0;
  char *end;
  uint64_t tmp;

  tmp= cs->cset->strntoull10rnd(cs,from,len,unsigned_flag,&end,&error);
  if (error == MY_ERRNO_ERANGE)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    error= 1;
  }
  else if (table->in_use->count_cuted_fields && 
           check_int(cs, from, len, end, error))
    error= 1;
  else
    error= 0;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int8store(ptr,tmp);
  }
  else
#endif
    int64_tstore(ptr,tmp);
  return error;
}


int Field_int64_t::store(double nr)
{
  int error= 0;
  int64_t res;

  nr= rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      res=0;
      error= 1;
    }
    else if (nr >= (double) UINT64_MAX)
    {
      res= ~(int64_t) 0;
      error= 1;
    }
    else
      res=(int64_t) (uint64_t) nr;
  }
  else
  {
    if (nr <= (double) INT64_MIN)
    {
      res= INT64_MIN;
      error= (nr < (double) INT64_MIN);
    }
    else if (nr >= (double) (uint64_t) INT64_MAX)
    {
      res= INT64_MAX;
      error= (nr > (double) INT64_MAX);
    }
    else
      res=(int64_t) nr;
  }
  if (error)
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);

#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int8store(ptr,res);
  }
  else
#endif
    int64_tstore(ptr,res);
  return error;
}


int Field_int64_t::store(int64_t nr, bool unsigned_val)
{
  int error= 0;

  if (nr < 0)                                   // Only possible error
  {
    /*
      if field is unsigned and value is signed (< 0) or
      if field is signed and value is unsigned we have an overflow
    */
    if (unsigned_flag != unsigned_val)
    {
      nr= unsigned_flag ? (uint64_t) 0 : (uint64_t) INT64_MAX;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
  }

#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int8store(ptr,nr);
  }
  else
#endif
    int64_tstore(ptr,nr);
  return error;
}


double Field_int64_t::val_real(void)
{
  int64_t j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    j=sint8korr(ptr);
  }
  else
#endif
    int64_tget(j,ptr);
  /* The following is open coded to avoid a bug in gcc 3.3 */
  if (unsigned_flag)
  {
    uint64_t tmp= (uint64_t) j;
    return uint64_t2double(tmp);
  }
  return (double) j;
}


int64_t Field_int64_t::val_int(void)
{
  int64_t j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint8korr(ptr);
  else
#endif
    int64_tget(j,ptr);
  return j;
}


String *Field_int64_t::val_str(String *val_buffer,
				String *val_ptr __attribute__((unused)))
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,22*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();
  int64_t j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint8korr(ptr);
  else
#endif
    int64_tget(j,ptr);

  length=(uint) (cs->cset->int64_t10_to_str)(cs,to,mlength,
					unsigned_flag ? 10 : -10, j);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


bool Field_int64_t::send_binary(Protocol *protocol)
{
  return protocol->store_int64_t(Field_int64_t::val_int(), unsigned_flag);
}


int Field_int64_t::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  int64_t a,b;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    a=sint8korr(a_ptr);
    b=sint8korr(b_ptr);
  }
  else
#endif
  {
    int64_tget(a,a_ptr);
    int64_tget(b,b_ptr);
  }
  if (unsigned_flag)
    return ((uint64_t) a < (uint64_t) b) ? -1 :
    ((uint64_t) a > (uint64_t) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_int64_t::sort_string(uchar *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->s->db_low_byte_first)
  {
    if (unsigned_flag)
      to[0] = ptr[0];
    else
      to[0] = (char) (ptr[0] ^ 128);		/* Revers signbit */
    to[1]   = ptr[1];
    to[2]   = ptr[2];
    to[3]   = ptr[3];
    to[4]   = ptr[4];
    to[5]   = ptr[5];
    to[6]   = ptr[6];
    to[7]   = ptr[7];
  }
  else
#endif
  {
    if (unsigned_flag)
      to[0] = ptr[7];
    else
      to[0] = (char) (ptr[7] ^ 128);		/* Revers signbit */
    to[1]   = ptr[6];
    to[2]   = ptr[5];
    to[3]   = ptr[4];
    to[4]   = ptr[3];
    to[5]   = ptr[2];
    to[6]   = ptr[1];
    to[7]   = ptr[0];
  }
}


void Field_int64_t::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			  "bigint(%d)",(int) field_length));
  add_zerofill_and_unsigned(res);
}


/*
  Floating-point numbers
 */

uchar *
Field_real::pack(uchar *to, const uchar *from,
                 uint max_length, bool low_byte_first)
{
  assert(max_length >= pack_length());
#ifdef WORDS_BIGENDIAN
  if (low_byte_first != table->s->db_low_byte_first)
  {
    const uchar *dptr= from + pack_length();
    while (dptr-- > from)
      *to++ = *dptr;
    return(to);
  }
  else
#endif
    return(Field::pack(to, from, max_length, low_byte_first));
}

const uchar *
Field_real::unpack(uchar *to, const uchar *from,
                   uint param_data, bool low_byte_first)
{
#ifdef WORDS_BIGENDIAN
  if (low_byte_first != table->s->db_low_byte_first)
  {
    const uchar *dptr= from + pack_length();
    while (dptr-- > from)
      *to++ = *dptr;
    return(from + pack_length());
  }
  else
#endif
    return(Field::unpack(to, from, param_data, low_byte_first));
}

/****************************************************************************
** string type
** A string may be varchar or binary
****************************************************************************/

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
                        CHARSET_INFO *cs)
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
                      MYSQL_ERROR::WARN_LEVEL_ERROR :
                      MYSQL_ERROR::WARN_LEVEL_WARN,
                      ER_TRUNCATED_WRONG_VALUE_FOR_FIELD, 
                      ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                      "string", tmp, field->field_name,
                      (ulong) field->table->in_use->row_count);
  return true;
}


/*
  Check if we lost any important data and send a truncation error/warning

  SYNOPSIS
    Field_longstr::report_if_important_data()
    ptr                      - Truncated rest of string
    end                      - End of truncated string

  RETURN VALUES
    0   - None was truncated (or we don't count cut fields)
    2   - Some bytes was truncated

  NOTE
    Check if we lost any important data (anything in a binary string,
    or any non-space in others). If only trailing spaces was lost,
    send a truncation note, otherwise send a truncation error.
*/

int
Field_longstr::report_if_important_data(const char *ptr, const char *end)
{
  if ((ptr < end) && table->in_use->count_cuted_fields)
  {
    if (test_if_important_data(field_charset, ptr, end))
    {
      if (table->in_use->abort_on_warning)
        set_warning(MYSQL_ERROR::WARN_LEVEL_ERROR, ER_DATA_TOO_LONG, 1);
      else
        set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
    }
    else /* If we lost only spaces then produce a NOTE, not a WARNING */
      set_warning(MYSQL_ERROR::WARN_LEVEL_NOTE, WARN_DATA_TRUNCATED, 1);
    return 2;
  }
  return 0;
}


/**
  Store double value in Field_string or Field_varstring.

  Pretty prints double number into field_length characters buffer.

  @param nr            number
*/

int Field_str::store(double nr)
{
  char buff[DOUBLE_TO_STRING_CONVERSION_BUFFER_SIZE];
  uint local_char_length= field_length / charset()->mbmaxlen;
  size_t length;
  my_bool error;

  length= my_gcvt(nr, MY_GCVT_ARG_DOUBLE, local_char_length, buff, &error);
  if (error)
  {
    if (table->in_use->abort_on_warning)
      set_warning(MYSQL_ERROR::WARN_LEVEL_ERROR, ER_DATA_TOO_LONG, 1);
    else
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
  }
  return store(buff, length, charset());
}


uint Field::is_equal(Create_field *new_field)
{
  return (new_field->sql_type == real_type());
}


/* If one of the fields is binary and the other one isn't return 1 else 0 */

bool Field_str::compare_str_field_flags(Create_field *new_field, uint32 flag_arg)
{
  return (((new_field->flags & (BINCMP_FLAG | BINARY_FLAG)) &&
          !(flag_arg & (BINCMP_FLAG | BINARY_FLAG))) ||
         (!(new_field->flags & (BINCMP_FLAG | BINARY_FLAG)) &&
          (flag_arg & (BINCMP_FLAG | BINARY_FLAG))));
}


uint Field_str::is_equal(Create_field *new_field)
{
  if (compare_str_field_flags(new_field, flags))
    return 0;

  return ((new_field->sql_type == real_type()) &&
	  new_field->charset == field_charset &&
	  new_field->length == max_display_length());
}


int Field_longstr::store_decimal(const my_decimal *d)
{
  char buff[DECIMAL_MAX_STR_LENGTH+1];
  String str(buff, sizeof(buff), &my_charset_bin);
  my_decimal2string(E_DEC_FATAL_ERROR, d, 0, 0, 0, &str);
  return store(str.ptr(), str.length(), str.charset());
}

uint32 Field_longstr::max_data_length() const
{
  return field_length + (field_length > 255 ? 2 : 1);
}


/****************************************************************************
** enum type.
** This is a string which only can have a selection of different values.
** If one uses this string in a number context one gets the type number.
****************************************************************************/

enum ha_base_keytype Field_enum::key_type() const
{
  switch (packlength) {
  default: return HA_KEYTYPE_BINARY;
  case 2: return HA_KEYTYPE_USHORT_INT;
  case 3: return HA_KEYTYPE_UINT24;
  case 4: return HA_KEYTYPE_ULONG_INT;
  case 8: return HA_KEYTYPE_ULONGLONG;
  }
}

void Field_enum::store_type(uint64_t value)
{
  switch (packlength) {
  case 1: ptr[0]= (uchar) value;  break;
  case 2:
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int2store(ptr,(unsigned short) value);
  }
  else
#endif
    shortstore(ptr,(unsigned short) value);
  break;
  case 3: int3store(ptr,(long) value); break;
  case 4:
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int4store(ptr,value);
  }
  else
#endif
    longstore(ptr,(long) value);
  break;
  case 8:
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int8store(ptr,value);
  }
  else
#endif
    int64_tstore(ptr,value); break;
  }
}


/**
  @note
    Storing a empty string in a enum field gives a warning
    (if there isn't a empty value in the enum)
*/

int Field_enum::store(const char *from,uint length,CHARSET_INFO *cs)
{
  int err= 0;
  uint32 not_used;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmpstr(buff,sizeof(buff), &my_charset_bin);

  /* Convert character set if necessary */
  if (String::needs_conversion(length, cs, field_charset, &not_used))
  { 
    uint dummy_errors;
    tmpstr.copy(from, length, cs, field_charset, &dummy_errors);
    from= tmpstr.ptr();
    length=  tmpstr.length();
  }

  /* Remove end space */
  length= field_charset->cset->lengthsp(field_charset, from, length);
  uint tmp=find_type2(typelib, from, length, field_charset);
  if (!tmp)
  {
    if (length < 6) // Can't be more than 99999 enums
    {
      /* This is for reading numbers with LOAD DATA INFILE */
      char *end;
      tmp=(uint) my_strntoul(cs,from,length,10,&end,&err);
      if (err || end != from+length || tmp > typelib->count)
      {
	tmp=0;
	set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
      }
      if (!table->in_use->count_cuted_fields)
        err= 0;
    }
    else
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
  }
  store_type((uint64_t) tmp);
  return err;
}


int Field_enum::store(double nr)
{
  return Field_enum::store((int64_t) nr, false);
}


int Field_enum::store(int64_t nr,
                      bool unsigned_val __attribute__((__unused__)))
{
  int error= 0;
  if ((uint64_t) nr > typelib->count || nr == 0)
  {
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
    if (nr != 0 || table->in_use->count_cuted_fields)
    {
      nr= 0;
      error= 1;
    }
  }
  store_type((uint64_t) (uint) nr);
  return error;
}


double Field_enum::val_real(void)
{
  return (double) Field_enum::val_int();
}


int64_t Field_enum::val_int(void)
{
  switch (packlength) {
  case 1:
    return (int64_t) ptr[0];
  case 2:
  {
    uint16 tmp;
#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      tmp=sint2korr(ptr);
    else
#endif
      shortget(tmp,ptr);
    return (int64_t) tmp;
  }
  case 3:
    return (int64_t) uint3korr(ptr);
  case 4:
  {
    uint32 tmp;
#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      tmp=uint4korr(ptr);
    else
#endif
      longget(tmp,ptr);
    return (int64_t) tmp;
  }
  case 8:
  {
    int64_t tmp;
#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      tmp=sint8korr(ptr);
    else
#endif
      int64_tget(tmp,ptr);
    return tmp;
  }
  }
  return 0;					// impossible
}


/**
   Save the field metadata for enum fields.

   Saves the real type in the first byte and the pack length in the 
   second byte of the field metadata array at index of *metadata_ptr and
   *(metadata_ptr + 1).

   @param   metadata_ptr   First byte of field metadata

   @returns number of bytes written to metadata_ptr
*/
int Field_enum::do_save_field_metadata(uchar *metadata_ptr)
{
  *metadata_ptr= real_type();
  *(metadata_ptr + 1)= pack_length();
  return 2;
}


String *Field_enum::val_str(String *val_buffer __attribute__((unused)),
			    String *val_ptr)
{
  uint tmp=(uint) Field_enum::val_int();
  if (!tmp || tmp > typelib->count)
    val_ptr->set("", 0, field_charset);
  else
    val_ptr->set((const char*) typelib->type_names[tmp-1],
		 typelib->type_lengths[tmp-1],
		 field_charset);
  return val_ptr;
}

int Field_enum::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  uchar *old= ptr;
  ptr= (uchar*) a_ptr;
  uint64_t a=Field_enum::val_int();
  ptr= (uchar*) b_ptr;
  uint64_t b=Field_enum::val_int();
  ptr= old;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_enum::sort_string(uchar *to,uint length __attribute__((unused)))
{
  uint64_t value=Field_enum::val_int();
  to+=packlength-1;
  for (uint i=0 ; i < packlength ; i++)
  {
    *to-- = (uchar) (value & 255);
    value>>=8;
  }
}


void Field_enum::sql_type(String &res) const
{
  char buffer[255];
  String enum_item(buffer, sizeof(buffer), res.charset());

  res.length(0);
  res.append(STRING_WITH_LEN("enum("));

  bool flag=0;
  uint *len= typelib->type_lengths;
  for (const char **pos= typelib->type_names; *pos; pos++, len++)
  {
    uint dummy_errors;
    if (flag)
      res.append(',');
    /* convert to res.charset() == utf8, then quote */
    enum_item.copy(*pos, *len, charset(), res.charset(), &dummy_errors);
    append_unescaped(&res, enum_item.ptr(), enum_item.length());
    flag= 1;
  }
  res.append(')');
}


Field *Field_enum::new_field(MEM_ROOT *root, struct st_table *new_table,
                             bool keep_type)
{
  Field_enum *res= (Field_enum*) Field::new_field(root, new_table, keep_type);
  if (res)
    res->typelib= copy_typelib(root, typelib);
  return res;
}


/*
   set type.
   This is a string which can have a collection of different values.
   Each string value is separated with a ','.
   For example "One,two,five"
   If one uses this string in a number context one gets the bits as a int64_t
   number.
*/


int Field_set::store(const char *from,uint length,CHARSET_INFO *cs)
{
  bool got_warning= 0;
  int err= 0;
  char *not_used;
  uint not_used2;
  uint32 not_used_offset;
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmpstr(buff,sizeof(buff), &my_charset_bin);

  /* Convert character set if necessary */
  if (String::needs_conversion(length, cs, field_charset, &not_used_offset))
  { 
    uint dummy_errors;
    tmpstr.copy(from, length, cs, field_charset, &dummy_errors);
    from= tmpstr.ptr();
    length=  tmpstr.length();
  }
  uint64_t tmp= find_set(typelib, from, length, field_charset,
                          &not_used, &not_used2, &got_warning);
  if (!tmp && length && length < 22)
  {
    /* This is for reading numbers with LOAD DATA INFILE */
    char *end;
    tmp=my_strntoull(cs,from,length,10,&end,&err);
    if (err || end != from+length ||
	tmp > (uint64_t) (((int64_t) 1 << typelib->count) - (int64_t) 1))
    {
      tmp=0;      
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
    }
  }
  else if (got_warning)
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
  store_type(tmp);
  return err;
}


int Field_set::store(int64_t nr,
                     bool unsigned_val __attribute__((__unused__)))
{
  int error= 0;
  uint64_t max_nr= set_bits(uint64_t, typelib->count);
  if ((uint64_t) nr > max_nr)
  {
    nr&= max_nr;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, WARN_DATA_TRUNCATED, 1);
    error=1;
  }
  store_type((uint64_t) nr);
  return error;
}


String *Field_set::val_str(String *val_buffer,
			   String *val_ptr __attribute__((unused)))
{
  uint64_t tmp=(uint64_t) Field_enum::val_int();
  uint bitnr=0;

  val_buffer->length(0);
  val_buffer->set_charset(field_charset);
  while (tmp && bitnr < (uint) typelib->count)
  {
    if (tmp & 1)
    {
      if (val_buffer->length())
	val_buffer->append(&field_separator, 1, &my_charset_latin1);
      String str(typelib->type_names[bitnr],
		 typelib->type_lengths[bitnr],
		 field_charset);
      val_buffer->append(str);
    }
    tmp>>=1;
    bitnr++;
  }
  return val_buffer;
}


void Field_set::sql_type(String &res) const
{
  char buffer[255];
  String set_item(buffer, sizeof(buffer), res.charset());

  res.length(0);
  res.append(STRING_WITH_LEN("set("));

  bool flag=0;
  uint *len= typelib->type_lengths;
  for (const char **pos= typelib->type_names; *pos; pos++, len++)
  {
    uint dummy_errors;
    if (flag)
      res.append(',');
    /* convert to res.charset() == utf8, then quote */
    set_item.copy(*pos, *len, charset(), res.charset(), &dummy_errors);
    append_unescaped(&res, set_item.ptr(), set_item.length());
    flag= 1;
  }
  res.append(')');
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
  for (uint i=0 ; i < from_lib->count ; i++)
    if (my_strnncoll(field_charset,
                     (const uchar*)typelib->type_names[i],
                     strlen(typelib->type_names[i]),
                     (const uchar*)from_lib->type_names[i],
                     strlen(from_lib->type_names[i])))
      return 0;
  return 1;
}

/**
  @return
  returns 1 if the fields are equally defined
*/
bool Field_num::eq_def(Field *field)
{
  if (!Field::eq_def(field))
    return 0;
  Field_num *from_num= (Field_num*) field;

  if (unsigned_flag != from_num->unsigned_flag ||
      (zerofill && !from_num->zerofill && !zero_pack()) ||
      dec != from_num->dec)
    return 0;
  return 1;
}


uint Field_num::is_equal(Create_field *new_field)
{
  return ((new_field->sql_type == real_type()) &&
	  ((new_field->flags & UNSIGNED_FLAG) == (uint) (flags &
							 UNSIGNED_FLAG)) &&
	  ((new_field->flags & AUTO_INCREMENT_FLAG) ==
	   (uint) (flags & AUTO_INCREMENT_FLAG)) &&
	  (new_field->length <= max_display_length()));
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
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VARCHAR:
    length*= charset->mbmaxlen;
    key_length= length;
    pack_length= calc_pack_length(sql_type, length);
    break;
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
    /* Pack_length already calculated in sql_parse.cc */
    length*= charset->mbmaxlen;
    key_length= pack_length;
    break;
  case MYSQL_TYPE_NEWDECIMAL:
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
                                      uint32 length_arg, uint32 decimals_arg,
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


/**
  Initialize field definition for create.

  @param thd                   Thread handle
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

bool Create_field::init(THD *thd, char *fld_name, enum_field_types fld_type,
                        char *fld_length, char *fld_decimals,
                        uint fld_type_modifier, Item *fld_default_value,
                        Item *fld_on_update_value, LEX_STRING *fld_comment,
                        char *fld_change, List<String> *fld_interval_list,
                        CHARSET_INFO *fld_charset,
                        uint fld_geom_type __attribute__((__unused__)),
                        enum column_format_type column_format)
{
  uint sign_len, allowed_type_modifier= 0;
  ulong max_field_charlength= MAX_FIELD_CHARLENGTH;

  field= 0;
  field_name= fld_name;
  def= fld_default_value;
  flags= fld_type_modifier;
  flags|= (((uint)column_format & COLUMN_FORMAT_MASK) << COLUMN_FORMAT_FLAGS);
  unireg_check= (fld_type_modifier & AUTO_INCREMENT_FLAG ?
                 Field::NEXT_NUMBER : Field::NONE);
  decimals= fld_decimals ? (uint)atoi(fld_decimals) : 0;
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
      (fld_type_modifier & NOT_NULL_FLAG) && fld_type != MYSQL_TYPE_TIMESTAMP)
    flags|= NO_DEFAULT_VALUE_FLAG;

  if (fld_length && !(length= (uint) atoi(fld_length)))
    fld_length= 0; /* purecov: inspected */
  sign_len= fld_type_modifier & UNSIGNED_FLAG ? 0 : 1;

  switch (fld_type) {
  case MYSQL_TYPE_TINY:
    if (!fld_length)
      length= MAX_TINYINT_WIDTH+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case MYSQL_TYPE_SHORT:
    if (!fld_length)
      length= MAX_SMALLINT_WIDTH+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case MYSQL_TYPE_LONG:
    if (!fld_length)
      length= MAX_INT_WIDTH+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case MYSQL_TYPE_LONGLONG:
    if (!fld_length)
      length= MAX_BIGINT_WIDTH;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case MYSQL_TYPE_NULL:
    break;
  case MYSQL_TYPE_NEWDECIMAL:
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
  case MYSQL_TYPE_VARCHAR:
    /*
      Long VARCHAR's are automaticly converted to blobs in mysql_prepare_table
      if they don't have a default value
    */
    max_field_charlength= MAX_FIELD_VARCHARLENGTH;
    break;
  case MYSQL_TYPE_STRING:
    break;
  case MYSQL_TYPE_BLOB:
    if (fld_default_value)
    {
      /* Allow empty as default value. */
      String str,*res;
      res= fld_default_value->val_str(&str);
      /*
        A default other than '' is always an error, and any non-NULL
        specified default is an error in strict mode.
      */
      if (res->length() || (thd->variables.sql_mode &
                            (MODE_STRICT_TRANS_TABLES |
                             MODE_STRICT_ALL_TABLES)))
      {
        my_error(ER_BLOB_CANT_HAVE_DEFAULT, MYF(0),
                 fld_name); /* purecov: inspected */
        return(true);
      }
      else
      {
        /*
          Otherwise a default of '' is just a warning.
        */
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_BLOB_CANT_HAVE_DEFAULT,
                            ER(ER_BLOB_CANT_HAVE_DEFAULT),
                            fld_name);
      }
      def= 0;
    }
    flags|= BLOB_FLAG;
    break;
  case MYSQL_TYPE_YEAR:
    if (!fld_length || length != 2)
      length= 4; /* Default length */
    flags|= ZEROFILL_FLAG | UNSIGNED_FLAG;
    break;
  case MYSQL_TYPE_DOUBLE:
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
  case MYSQL_TYPE_TIMESTAMP:
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
      length= min(length, MAX_DATETIME_COMPRESSED_WIDTH); /* purecov: inspected */
    }
    flags|= ZEROFILL_FLAG | UNSIGNED_FLAG;
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
  case MYSQL_TYPE_DATE:
    /* Old date type. */
    sql_type= MYSQL_TYPE_NEWDATE;
    /* fall trough */
  case MYSQL_TYPE_NEWDATE:
    length= 10;
    break;
  case MYSQL_TYPE_TIME:
    length= 10;
    break;
  case MYSQL_TYPE_DATETIME:
    length= MAX_DATETIME_WIDTH;
    break;
  case MYSQL_TYPE_SET:
    {
      pack_length= get_set_pack_length(fld_interval_list->elements);

      List_iterator<String> it(*fld_interval_list);
      String *tmp;
      while ((tmp= it++))
        interval_list.push_back(tmp);
      /*
        Set fake length to 1 to pass the below conditions.
        Real length will be set in mysql_prepare_table()
        when we know the character set of the column
      */
      length= 1;
      break;
    }
  case MYSQL_TYPE_ENUM:
    {
      /* Should be safe. */
      pack_length= get_enum_pack_length(fld_interval_list->elements);

      List_iterator<String> it(*fld_interval_list);
      String *tmp;
      while ((tmp= it++))
        interval_list.push_back(tmp);
      length= 1; /* See comment for MYSQL_TYPE_SET above. */
      break;
   }
  case MYSQL_TYPE_VAR_STRING:
    assert(0);  /* Impossible, we killed it */
    break;
  }
  /* Remember the value of length */
  char_length= length;

  if (!(flags & BLOB_FLAG) &&
      ((length > max_field_charlength && fld_type != MYSQL_TYPE_SET &&
        fld_type != MYSQL_TYPE_ENUM &&
        (fld_type != MYSQL_TYPE_VARCHAR || fld_default_value)) ||
       (!length &&
        fld_type != MYSQL_TYPE_STRING &&
        fld_type != MYSQL_TYPE_VARCHAR)))
  {
    my_error((fld_type == MYSQL_TYPE_VAR_STRING ||
              fld_type == MYSQL_TYPE_VARCHAR ||
              fld_type == MYSQL_TYPE_STRING) ?  ER_TOO_BIG_FIELDLENGTH :
                                                ER_TOO_BIG_DISPLAYWIDTH,
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


enum_field_types get_blob_type_from_length(ulong length __attribute__((__unused__)))
{
  enum_field_types type;

  type= MYSQL_TYPE_BLOB;

  return type;
}


/*
  Make a field from the .frm file info
*/

uint32 calc_pack_length(enum_field_types type,uint32 length)
{
  switch (type) {
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VARCHAR:     return (length + (length < 256 ? 1: 2));
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_TINY	: return 1;
  case MYSQL_TYPE_SHORT : return 2;
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_TIME:   return 3;
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_LONG	: return 4;
  case MYSQL_TYPE_DOUBLE: return sizeof(double);
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_LONGLONG: return 8;	/* Don't crash if no int64_t */
  case MYSQL_TYPE_NULL	: return 0;
  case MYSQL_TYPE_BLOB:		return 4+portable_sizeof_char_ptr;
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_NEWDECIMAL:
    abort(); return 0;                          // This shouldn't happen
  default:
    return 0;
  }
}


uint pack_length_to_packflag(uint type)
{
  switch (type) {
    case 1: return f_settype((uint) MYSQL_TYPE_TINY);
    case 2: return f_settype((uint) MYSQL_TYPE_SHORT);
    case 3: assert(1);
    case 4: return f_settype((uint) MYSQL_TYPE_LONG);
    case 8: return f_settype((uint) MYSQL_TYPE_LONGLONG);
  }
  return 0;					// This shouldn't happen
}


Field *make_field(TABLE_SHARE *share, uchar *ptr, uint32 field_length,
		  uchar *null_pos, uchar null_bit,
		  uint pack_flag,
		  enum_field_types field_type,
		  CHARSET_INFO *field_charset,
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
    null_bit= ((uchar) 1) << null_bit;
  }

  switch (field_type) {
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    field_charset= &my_charset_bin;
  default: break;
  }

  if (f_is_alpha(pack_flag))
  {
    if (!f_is_packed(pack_flag))
    {
      if (field_type == MYSQL_TYPE_STRING ||
          field_type == MYSQL_TYPE_VAR_STRING)
        return new Field_string(ptr,field_length,null_pos,null_bit,
                                unireg_check, field_name,
                                field_charset);
      if (field_type == MYSQL_TYPE_VARCHAR)
        return new Field_varstring(ptr,field_length,
                                   HA_VARCHAR_PACKLENGTH(field_length),
                                   null_pos,null_bit,
                                   unireg_check, field_name,
                                   share,
                                   field_charset);
      return 0;                                 // Error
    }

    uint pack_length=calc_pack_length((enum_field_types)
				      f_packtype(pack_flag),
				      field_length);

    if (f_is_blob(pack_flag))
      return new Field_blob(ptr,null_pos,null_bit,
			    unireg_check, field_name, share,
			    pack_length, field_charset);
    if (interval)
    {
      if (f_is_enum(pack_flag))
	return new Field_enum(ptr,field_length,null_pos,null_bit,
				  unireg_check, field_name,
				  pack_length, interval, field_charset);
      else
	return new Field_set(ptr,field_length,null_pos,null_bit,
			     unireg_check, field_name,
			     pack_length, interval, field_charset);
    }
  }

  switch (field_type) {
  case MYSQL_TYPE_NEWDECIMAL:
    return new Field_new_decimal(ptr,field_length,null_pos,null_bit,
                                 unireg_check, field_name,
                                 f_decimals(pack_flag),
                                 f_is_zerofill(pack_flag) != 0,
                                 f_is_dec(pack_flag) == 0);
  case MYSQL_TYPE_DOUBLE:
    return new Field_double(ptr,field_length,null_pos,null_bit,
			    unireg_check, field_name,
			    f_decimals(pack_flag),
			    f_is_zerofill(pack_flag) != 0,
			    f_is_dec(pack_flag)== 0);
  case MYSQL_TYPE_TINY:
    return new Field_tiny(ptr,field_length,null_pos,null_bit,
			  unireg_check, field_name,
			  f_is_zerofill(pack_flag) != 0,
			  f_is_dec(pack_flag) == 0);
  case MYSQL_TYPE_SHORT:
    return new Field_short(ptr,field_length,null_pos,null_bit,
			   unireg_check, field_name,
			   f_is_zerofill(pack_flag) != 0,
			   f_is_dec(pack_flag) == 0);
  case MYSQL_TYPE_LONG:
    return new Field_long(ptr,field_length,null_pos,null_bit,
			   unireg_check, field_name,
			   f_is_zerofill(pack_flag) != 0,
			   f_is_dec(pack_flag) == 0);
  case MYSQL_TYPE_LONGLONG:
    return new Field_int64_t(ptr,field_length,null_pos,null_bit,
			      unireg_check, field_name,
			      f_is_zerofill(pack_flag) != 0,
			      f_is_dec(pack_flag) == 0);
  case MYSQL_TYPE_TIMESTAMP:
    return new Field_timestamp(ptr,field_length, null_pos, null_bit,
                               unireg_check, field_name, share,
                               field_charset);
  case MYSQL_TYPE_YEAR:
    return new Field_year(ptr,field_length,null_pos,null_bit,
			  unireg_check, field_name);
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_NEWDATE:
    return new Field_newdate(ptr,null_pos,null_bit,
			     unireg_check, field_name, field_charset);
  case MYSQL_TYPE_TIME:
    return new Field_time(ptr,null_pos,null_bit,
			  unireg_check, field_name, field_charset);
  case MYSQL_TYPE_DATETIME:
    return new Field_datetime(ptr,null_pos,null_bit,
			      unireg_check, field_name, field_charset);
  case MYSQL_TYPE_NULL:
    return new Field_null(ptr, field_length, unireg_check, field_name,
                          field_charset);
  default:					// Impossible (Wrong version)
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
  charset=    old_field->charset();		// May be NULL ptr
  comment=    old_field->comment;
  decimals=   old_field->decimals();

  /* Fix if the original table had 4 byte pointer blobs */
  if (flags & BLOB_FLAG)
    pack_length= (pack_length- old_field->table->s->blob_ptr_size +
		  portable_sizeof_char_ptr);

  switch (sql_type) {
  case MYSQL_TYPE_BLOB:
    sql_type= MYSQL_TYPE_BLOB;
    length/= charset->mbmaxlen;
    key_length/= charset->mbmaxlen;
    break;
  case MYSQL_TYPE_STRING:
    /* Change CHAR -> VARCHAR if dynamic record length */
    if (old_field->type() == MYSQL_TYPE_VAR_STRING)
      sql_type= MYSQL_TYPE_VARCHAR;
    /* fall through */

  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_VAR_STRING:
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
      (sql_type != MYSQL_TYPE_TIMESTAMP ||                /* set def only if */
       old_field->table->timestamp_field != old_field ||  /* timestamp field */ 
       unireg_check == Field::TIMESTAMP_UN_FIELD))        /* has default val */
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff), charset);
    my_ptrdiff_t diff;

    /* Get the value from default_values */
    diff= (my_ptrdiff_t) (orig_field->table->s->default_values-
                          orig_field->table->record[0]);
    orig_field->move_field_offset(diff);	// Points now at default_values
    if (!orig_field->is_real_null())
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
Field::set_warning(MYSQL_ERROR::enum_warning_level level, uint code,
                   int cuted_increment)
{
  /*
    If this field was created only for type conversion purposes it
    will have table == NULL.
  */
  THD *thd= table ? table->in_use : current_thd;
  if (thd->count_cuted_fields)
  {
    thd->cuted_fields+= cuted_increment;
    push_warning_printf(thd, level, code, ER(code), field_name,
                        thd->row_count);
    return 0;
  }
  return level >= MYSQL_ERROR::WARN_LEVEL_WARN;
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
Field::set_datetime_warning(MYSQL_ERROR::enum_warning_level level, uint code, 
                            const char *str, uint str_length, 
                            timestamp_type ts_type, int cuted_increment)
{
  THD *thd= table ? table->in_use : current_thd;
  if ((thd->really_abort_on_warning() &&
       level >= MYSQL_ERROR::WARN_LEVEL_WARN) ||
      set_warning(level, code, cuted_increment))
    make_truncated_value_warning(thd, level, str, str_length, ts_type,
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
Field::set_datetime_warning(MYSQL_ERROR::enum_warning_level level, uint code, 
                            int64_t nr, timestamp_type ts_type,
                            int cuted_increment)
{
  THD *thd= table ? table->in_use : current_thd;
  if (thd->really_abort_on_warning() ||
      set_warning(level, code, cuted_increment))
  {
    char str_nr[22];
    char *str_end= int64_t10_to_str(nr, str_nr, -10);
    make_truncated_value_warning(thd, level, str_nr, (uint) (str_end - str_nr), 
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
Field::set_datetime_warning(MYSQL_ERROR::enum_warning_level level, uint code, 
                            double nr, timestamp_type ts_type)
{
  THD *thd= table ? table->in_use : current_thd;
  if (thd->really_abort_on_warning() ||
      set_warning(level, code, 1))
  {
    /* DBL_DIG is enough to print '-[digits].E+###' */
    char str_nr[DBL_DIG + 8];
    uint str_len= my_sprintf(str_nr, (str_nr, "%g", nr));
    make_truncated_value_warning(thd, level, str_nr, str_len, ts_type,
                                 field_name);
  }
}
