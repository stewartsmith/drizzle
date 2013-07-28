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
 * @file Declare a class to handle errors in json server.
 */ 
#include <config.h>

#include <drizzled/error_t.h>
#include <string> 
namespace drizzle_plugin
{
namespace json_server
{
  /**
   * a class
   * used to handle various errors of json server.
   */ 
  class JsonErrorArea
  {
    public:
      /**
       * an enumerated data type for different type of error in json server.
       */
      enum enum_error_type
      {
        ER_EMPTY=0,
        ER_SQL,
        ER_JSON,
        ER_HTTP,
        ER_UNKNOWN
      };
      /**
       *  Constructor.
       */  
      JsonErrorArea();
      /**
       * Reset the memeber values.
       */ 
      void reset_jsonerror_area();
      /**
       * Set an error with details.
       */ 
      void set_error(enum_error_type type,drizzled::error_t sql_errno_arg,const char *message_arg);
      /**
       * Check whether error or not.
       *
       * @return true Success.
       * @return false Failure.
       */
      bool is_error(){return er_type!= ER_EMPTY;}
      /**
       * Check whether sql error or not.
       *
       * @return true Success.
       * @return false Failure.
       */
      bool is_sqlerror(){return er_type == ER_SQL;}
      /**
       * Check whether json error or not.
       *
       * @return true Success.
       * @return false Failure.
       */ 
      bool is_jsonerror(){return er_type == ER_JSON;}
      /**
       * Check whether http error or not.
       *
       * @return true Success.
       * @return false Failure.
       */ 
      bool is_httperror(){return er_type == ER_HTTP;}
      /**
       * Check whether unknown error or not.
       *
       * @return true Success.
       * @return false Failure.
       */ 
      bool is_unknownerror(){return er_type == ER_UNKNOWN;}
      /**
       * Get an error number.
       *
       * @return a error number.
       */ 
      drizzled::error_t get_error_no() const { return error_no;}
      /**
       * Get an error message.
       *
       * @return a const error message string.
       */ 
      const char* get_error_msg() const { return error_msg;}
      /**
       * Get sql state.
       *
       * @return a const sql state string.
       */
      const char* get_sql_state() const { return sql_state;}
      /**
       * Get error type.
       *
       * @return a error type.
       */
      enum_error_type get_error_type() const { return er_type;}
      /**
       * Get error type string.
       *
       * @return a error type string.
       */
      std::string get_error_type_string() const {
        std::string error_str;
        switch(er_type)
        {
          case ER_EMPTY: {error_str="NO ERROR"; break;}
          case ER_SQL: {error_str="SQL ERROR"; break;}
          case ER_JSON: {error_str="JSON ERROR"; break;}
          case ER_HTTP: {error_str="HTTP ERROR"; break;}
          case ER_UNKNOWN: {error_str="UNKNOWN ERROR"; break;}              
        }
        return error_str;
      }

    private:
      /**
       * Stores error type.
       */
      enum_error_type er_type;
      /**
       * Stores error number.
       */
      drizzled::error_t error_no;
      /**
       * Stores error message.
       */
      const char *error_msg;
      /**
       * Stores sql state.
       */
      const char *sql_state;

  };
}
}
