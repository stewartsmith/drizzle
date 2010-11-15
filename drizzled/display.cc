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

#include "config.h"

#include "drizzled/display.h"

#include <assert.h>

namespace drizzled {
namespace display {

static const std::string PROGRAM_ERROR("PROGRAM_ERROR");

static const std::string COM_SLEEP("COM_SLEEP"); 
static const std::string COM_QUIT("COM_QUIT"); 
static const std::string COM_INIT_DB("COM_INIT_DB"); 
static const std::string COM_QUERY("COM_QUERY"); 
static const std::string COM_SHUTDOWN("COM_SHUTDOWN"); 
static const std::string COM_CONNECT("COM_CONNECT"); 
static const std::string COM_PING("COM_PING"); 
static const std::string COM_END("COM_END"); 

static const std::string DRIZZLE_TYPE_LONG("DRIZZLE_TYPE_LONG"); 
static const std::string DRIZZLE_TYPE_DOUBLE("DRIZZLE_TYPE_DOUBLE"); 
static const std::string DRIZZLE_TYPE_NULL("DRIZZLE_TYPE_NULL"); 
static const std::string DRIZZLE_TYPE_TIMESTAMP("DRIZZLE_TYPE_TIMESTAMP"); 
static const std::string DRIZZLE_TYPE_LONGLONG("DRIZZLE_TYPE_LONGLONG"); 
static const std::string DRIZZLE_TYPE_DATETIME("DRIZZLE_TYPE_DATETIME"); 
static const std::string DRIZZLE_TYPE_DATE("DRIZZLE_TYPE_DATE"); 
static const std::string DRIZZLE_TYPE_VARCHAR("DRIZZLE_TYPE_VARCHAR"); 
static const std::string DRIZZLE_TYPE_DECIMAL("DRIZZLE_TYPE_DECIMAL"); 
static const std::string DRIZZLE_TYPE_ENUM("DRIZZLE_TYPE_ENUM"); 
static const std::string DRIZZLE_TYPE_BLOB("DRIZZLE_TYPE_BLOB"); 
static const std::string DRIZZLE_TYPE_MAX("DRIZZLE_TYPE_MAX"); 

static const std::string YES("YES");
static const std::string NO("NO");


const std::string &type(drizzled::enum_server_command type)
{
  switch (type)
  {
  case drizzled::COM_SLEEP : 
    return COM_SLEEP;
  case drizzled::COM_QUIT : 
    return COM_QUIT;
  case drizzled::COM_INIT_DB : 
    return COM_INIT_DB;
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
  case drizzled::DRIZZLE_TYPE_TIMESTAMP : 
    return DRIZZLE_TYPE_TIMESTAMP;
  case drizzled::DRIZZLE_TYPE_LONGLONG : 
    return DRIZZLE_TYPE_LONGLONG;
  case drizzled::DRIZZLE_TYPE_DATETIME : 
    return DRIZZLE_TYPE_DATETIME;
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
  }

  assert(0);
  return PROGRAM_ERROR;
}

} /* namespace display */
} /* namespace drizzled */
