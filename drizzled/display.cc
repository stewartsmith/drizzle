/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <drizzled/display.h>
#include <drizzled/item.h>

#include <cassert>
#include <iostream>
#include <sstream>

namespace drizzled {
namespace display {

static const std::string PROGRAM_ERROR("PROGRAM_ERROR");

static const std::string COM_SLEEP("COM_SLEEP"); 
static const std::string COM_QUIT("COM_QUIT"); 
static const std::string COM_USE_SCHEMA("COM_USE_SCHEMA"); 
static const std::string COM_QUERY("COM_QUERY"); 
static const std::string COM_SHUTDOWN("COM_SHUTDOWN"); 
static const std::string COM_CONNECT("COM_CONNECT"); 
static const std::string COM_PING("COM_PING"); 
static const std::string COM_END("COM_END"); 

static const std::string DRIZZLE_TYPE_LONG("DRIZZLE_TYPE_LONG"); 
static const std::string DRIZZLE_TYPE_DOUBLE("DRIZZLE_TYPE_DOUBLE"); 
static const std::string DRIZZLE_TYPE_NULL("DRIZZLE_TYPE_NULL"); 
static const std::string DRIZZLE_TYPE_TIMESTAMP("DRIZZLE_TYPE_TIMESTAMP"); 
static const std::string DRIZZLE_TYPE_MICROTIME("DRIZZLE_TYPE_MICROTIME"); 
static const std::string DRIZZLE_TYPE_LONGLONG("DRIZZLE_TYPE_LONGLONG"); 
static const std::string DRIZZLE_TYPE_DATETIME("DRIZZLE_TYPE_DATETIME"); 
static const std::string DRIZZLE_TYPE_TIME("DRIZZLE_TYPE_TIME"); 
static const std::string DRIZZLE_TYPE_DATE("DRIZZLE_TYPE_DATE"); 
static const std::string DRIZZLE_TYPE_VARCHAR("DRIZZLE_TYPE_VARCHAR"); 
static const std::string DRIZZLE_TYPE_DECIMAL("DRIZZLE_TYPE_DECIMAL"); 
static const std::string DRIZZLE_TYPE_ENUM("DRIZZLE_TYPE_ENUM"); 
static const std::string DRIZZLE_TYPE_BLOB("DRIZZLE_TYPE_BLOB"); 
static const std::string DRIZZLE_TYPE_UUID("DRIZZLE_TYPE_UUID"); 
static const std::string DRIZZLE_TYPE_BOOLEAN("DRIZZLE_TYPE_BOOLEAN"); 

static const std::string FIELD_ITEM("FIELD_ITEM");
static const std::string FUNC_ITEM("FUNC_ITEM");
static const std::string SUM_FUNC_ITEM("SUM_FUNC_ITEM");
static const std::string STRING_ITEM("STRING_ITEM");
static const std::string INT_ITEM("INT_ITEM");
static const std::string REAL_ITEM("REAL_ITEM");
static const std::string NULL_ITEM("NULL_ITEM");
static const std::string VARBIN_ITEM("VARBIN_ITEM");
static const std::string COPY_STR_ITEM("COPY_STR_ITEM");
static const std::string FIELD_AVG_ITEM("FIELD_AVG_ITEM");
static const std::string DEFAULT_VALUE_ITEM("DEFAULT_VALUE_ITEM");
static const std::string PROC_ITEM("PROC_ITEM");
static const std::string COND_ITEM("COND_ITEM");
static const std::string REF_ITEM("REF_ITEM");
static const std::string FIELD_STD_ITEM("FIELD_STD_ITEM");
static const std::string FIELD_VARIANCE_ITEM("FIELD_VARIANCE_ITEM");
static const std::string INSERT_VALUE_ITEM("INSERT_VALUE_ITEM");
static const std::string SUBSELECT_ITEM("SUBSELECT_ITEM");
static const std::string ROW_ITEM("ROW_ITEM");
static const std::string CACHE_ITEM("CACHE_ITEM");
static const std::string TYPE_HOLDER("TYPE_HOLDER");
static const std::string PARAM_ITEM("PARAM_ITEM");
static const std::string BOOLEAN_ITEM("BOOLEAN_ITEM");
static const std::string DECIMAL_ITEM("DECIMAL_ITEM");

static const std::string ITEM_CAST_SIGNED("ITEM_CAST_SIGNED");
static const std::string ITEM_CAST_UNSIGNED("ITEM_CAST_UNSIGNED");
static const std::string ITEM_CAST_BINARY("ITEM_CAST_BINARY");
static const std::string ITEM_CAST_BOOLEAN("ITEM_CAST_BOOLEAN");
static const std::string ITEM_CAST_DATE("ITEM_CAST_DATE");
static const std::string ITEM_CAST_TIME("ITEM_CAST_TIME");
static const std::string ITEM_CAST_DATETIME("ITEM_CAST_DATETIME");
static const std::string ITEM_CAST_CHAR("ITEM_CAST_CHAR");
static const std::string ITEM_CAST_DECIMAL("ITEM_CAST_DECIMAL");

static const std::string STRING_RESULT_STRING("STRING");
static const std::string REAL_RESULT_STRING("REAL");
static const std::string INT_RESULT_STRING("INTEGER");
static const std::string ROW_RESULT_STRING("ROW");
static const std::string DECIMAL_RESULT_STRING("DECIMAL");

static const std::string YES("YES");
static const std::string NO("NO");

const std::string &type(drizzled::Cast_target type)
{
  switch (type)
  {
  case drizzled::ITEM_CAST_SIGNED:
    return ITEM_CAST_SIGNED;
  case drizzled::ITEM_CAST_UNSIGNED:
    return ITEM_CAST_UNSIGNED;
  case drizzled::ITEM_CAST_BINARY:
    return ITEM_CAST_BINARY;
  case drizzled::ITEM_CAST_BOOLEAN:
    return ITEM_CAST_BOOLEAN;
  case drizzled::ITEM_CAST_DATE:
    return ITEM_CAST_DATE;
  case drizzled::ITEM_CAST_TIME:
    return ITEM_CAST_TIME;
  case drizzled::ITEM_CAST_DATETIME:
    return ITEM_CAST_DATETIME;
  case drizzled::ITEM_CAST_CHAR:
    return ITEM_CAST_CHAR;
  case drizzled::ITEM_CAST_DECIMAL:
    return ITEM_CAST_DECIMAL;
  }

  abort();
}

const std::string &type(drizzled::enum_server_command type)
{
  switch (type)
  {
  case drizzled::COM_SLEEP : 
    return COM_SLEEP;

  case drizzled::COM_KILL : 
    {
      static std::string COM_KILL("COM_KILL");
      return COM_KILL;
    }

  case drizzled::COM_QUIT : 
    return COM_QUIT;

  case drizzled::COM_USE_SCHEMA : 
    return COM_USE_SCHEMA;

  case drizzled::COM_QUERY : 
    return COM_QUERY;

  case drizzled::COM_SHUTDOWN : 
    return COM_SHUTDOWN;

  case drizzled::COM_CONNECT : 
    return COM_CONNECT;

  case drizzled::COM_PING : 
    return COM_PING;

  case drizzled::COM_END : 
    return COM_END;
  }

  assert(0);
  return PROGRAM_ERROR;
}

const std::string &type(drizzled::Item::Type type)
{
  switch (type)
  {
  case drizzled::Item::FIELD_ITEM :
    return FIELD_ITEM;
  case drizzled::Item::FUNC_ITEM :
    return FUNC_ITEM;
  case drizzled::Item::SUM_FUNC_ITEM :
    return SUM_FUNC_ITEM;
  case drizzled::Item::STRING_ITEM :
    return STRING_ITEM;
  case drizzled::Item::INT_ITEM :
    return INT_ITEM;
  case drizzled::Item::REAL_ITEM :
    return REAL_ITEM;
  case drizzled::Item::NULL_ITEM :
    return NULL_ITEM;
  case drizzled::Item::VARBIN_ITEM :
    return VARBIN_ITEM;
  case drizzled::Item::COPY_STR_ITEM :
    return COPY_STR_ITEM;
  case drizzled::Item::FIELD_AVG_ITEM :
    return FIELD_AVG_ITEM;
  case drizzled::Item::DEFAULT_VALUE_ITEM :
    return DEFAULT_VALUE_ITEM;
  case drizzled::Item::PROC_ITEM :
    return PROC_ITEM;
  case drizzled::Item::COND_ITEM :
    return COND_ITEM;
  case drizzled::Item::REF_ITEM :
    return REF_ITEM;
  case drizzled::Item::FIELD_STD_ITEM :
    return FIELD_STD_ITEM;
  case drizzled::Item::FIELD_VARIANCE_ITEM :
    return FIELD_VARIANCE_ITEM;
  case drizzled::Item::INSERT_VALUE_ITEM :
    return INSERT_VALUE_ITEM;
  case drizzled::Item::SUBSELECT_ITEM :
    return SUBSELECT_ITEM;
  case drizzled::Item::ROW_ITEM:
    return ROW_ITEM;
  case drizzled::Item::CACHE_ITEM :
    return CACHE_ITEM;
  case drizzled::Item::TYPE_HOLDER :
    return TYPE_HOLDER;
  case drizzled::Item::PARAM_ITEM :
    return PARAM_ITEM;
  case drizzled::Item::BOOLEAN_ITEM :
    return BOOLEAN_ITEM;
  case drizzled::Item::DECIMAL_ITEM :
    return DECIMAL_ITEM;
  }

  assert(0);
  return PROGRAM_ERROR;
}

const std::string &type(Item_result type)
{
  switch (type)
  {
  case STRING_RESULT:
    return STRING_RESULT_STRING;
  case REAL_RESULT:
    return REAL_RESULT_STRING;
  case INT_RESULT:
    return INT_RESULT_STRING;
  case ROW_RESULT:
    return ROW_RESULT_STRING;
  case DECIMAL_RESULT:
    return DECIMAL_RESULT_STRING;
  }

  assert(0);
  return PROGRAM_ERROR;
}

const std::string &type(drizzled::enum_field_types type)
{
  switch (type)
  {
  case drizzled::DRIZZLE_TYPE_LONG : 
    return DRIZZLE_TYPE_LONG;
  case drizzled::DRIZZLE_TYPE_DOUBLE : 
    return DRIZZLE_TYPE_DOUBLE;
  case drizzled::DRIZZLE_TYPE_NULL : 
    return DRIZZLE_TYPE_NULL;
  case drizzled::DRIZZLE_TYPE_MICROTIME : 
    return DRIZZLE_TYPE_MICROTIME;
  case drizzled::DRIZZLE_TYPE_TIMESTAMP : 
    return DRIZZLE_TYPE_TIMESTAMP;
  case drizzled::DRIZZLE_TYPE_LONGLONG : 
    return DRIZZLE_TYPE_LONGLONG;
  case drizzled::DRIZZLE_TYPE_DATETIME : 
    return DRIZZLE_TYPE_DATETIME;
  case drizzled::DRIZZLE_TYPE_TIME : 
    return DRIZZLE_TYPE_TIME;
  case drizzled::DRIZZLE_TYPE_DATE : 
    return DRIZZLE_TYPE_DATE;
  case drizzled::DRIZZLE_TYPE_VARCHAR : 
    return DRIZZLE_TYPE_VARCHAR;
  case drizzled::DRIZZLE_TYPE_DECIMAL : 
    return DRIZZLE_TYPE_DECIMAL;
  case drizzled::DRIZZLE_TYPE_ENUM : 
    return DRIZZLE_TYPE_ENUM;
  case drizzled::DRIZZLE_TYPE_BLOB : 
    return DRIZZLE_TYPE_BLOB;
  case drizzled::DRIZZLE_TYPE_UUID : 
    return DRIZZLE_TYPE_UUID;
  case drizzled::DRIZZLE_TYPE_BOOLEAN : 
    return DRIZZLE_TYPE_BOOLEAN;
  }

  assert(0);
  return PROGRAM_ERROR;
}

std::string hexdump(const unsigned char *str, size_t length)
{
  static const char hexval[16] = { '0', '1', '2', '3', 
    '4', '5', '6', '7', 
    '8', '9', 'a', 'b', 
    'c', 'd', 'e', 'f' };
  unsigned max_cols = 16; 
  std::ostringstream buf;
  std::ostringstream raw_buf;

  const unsigned char *e= str + length;
  for (const unsigned char *i= str; i != e;)
  {
    raw_buf.str("");
    for (unsigned col = 0; col < max_cols; ++col)
    {
      if (i != e)
      {
        buf << hexval[ ( (*i >> 4) & 0xF ) ]
          << hexval[ ( *i & 0x0F ) ]
          << ' ';
        raw_buf << (isprint(*i) ? *i : '.');
        ++i;
      }
      else
      {
        buf << "   ";
      }
    }
  }

  return buf.str();
}

} /* namespace display */
} /* namespace drizzled */
