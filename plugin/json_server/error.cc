 /* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
  * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
  *
  * Copyright (C) 2011 Stewart Smith, Henrik Ingo, Mohit Srivastava
  *       
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *                 
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *                           
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  */
/**
 * @file Implements JsonErrorArea class which handles errors of json server.
 */
#include <plugin/json_server/error.h>
#include <drizzled/error/sql_state.h>

namespace drizzle_plugin
{
namespace json_server
{
  JsonErrorArea::JsonErrorArea()
  {
    reset_jsonerror_area();
  }
  
  void JsonErrorArea::reset_jsonerror_area()
  {
    er_type = ER_EMPTY;
    error_no = drizzled::EE_OK;
    error_msg= "";
    sql_state="00000";
  }

  void JsonErrorArea::set_error(enum_error_type error_type_arg,drizzled::error_t error_no_arg,const char * error_msg_arg)
  {
    if(error_type_arg != ER_EMPTY)
    {
      er_type = error_type_arg;
      error_msg = error_msg_arg;

      if(error_type_arg == ER_SQL)
      {
        error_no = error_no_arg;
        sql_state = drizzled::error::convert_to_sqlstate(error_no_arg);
      }
    }
  }
}
}
