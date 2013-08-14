/** - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2011-2013 Stewart Smith, Henrik Ingo, Mohit Srivastava
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
 * @file Declare a class Schema to perform various operations related to schema. 
 */ 
#include <drizzled/session.h>
#include <drizzled/statement.h>
#include <drizzled/message/schema.pb.h>
#include <uuid/uuid.h>
#include <drizzled/definitions.h>
#include <drizzled/error.h>
#include <drizzled/sql_parse.h>
#include <drizzled/sql_base.h>
#include <string>
using namespace std;
using namespace drizzled; 
namespace drizzle_plugin {
namespace json_server {
  /**
   * a class.
   *
   * To perform various operations related to schema.
   */
  class Schema
    {
      public:
        /**
         * Constructor.
         *
         * @param in_session a session object.
         * @param db_name a schema name string.
         */
      Schema(Session *in_session,string db_name) : 
        _session(*in_session),_db_name(db_name)
      {}
      /**
       * Stores whether schema exist or not.
       */
      bool is_if_not_exists;
      /**
       * Stores schema message.
       */
      message::Schema schema_message;
      /**
       * create a new schema if it not exists.
       *
       * @return false Success.
       * @return true Failure.
       */
      bool createSchema();
      /**
       * drop a schema if it exists.
       *
       * @return false Success.
       * @reutrn true Failure.
       */
      bool dropSchema();
      /**
       * Get a session object.
       *
       * @return a session object.
       */
      Session& session() const{
        return _session;
      }

      private:
      /**
       * Validates various schema options.
       *
       * @return false Success.
       * @return true Failure.
       */
      bool validateSchemaOptions();
      /**
       * Checks whether schema exists or not already.
       *
       * @return false Success.
       * @return true Failure.
       */ 
      bool check(const identifier::Schema &identifier);
      /**
       * Stores a session object.
       */
      Session& _session;
      /**
       * Stores a schema name.
       */
      string _db_name;
    };

}
}
