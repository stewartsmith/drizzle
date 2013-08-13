/* mode: c; c-basic-offset: 2; indent-tabs-mode: nil;
 * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
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
 * @file Implements a class Schema to handle various operations related to schema.Its just a copy of drizzled/schema.cc and its friends.
 */

 #include <config.h>
 
 #include <drizzled/show.h>
 #include <drizzled/session.h>
 #include <drizzled/schema.h>
 #include <drizzled/message.h>
 #include <drizzled/sql_lex.h>
 #include <drizzled/plugin/event_observer.h>
 #include <drizzled/catalog/instance.h>
 #include <plugin/json_server/ddl/schema.h>
 #include <drizzled/plugin/authorization.h>
 #include <drizzled/plugin/storage_engine.h>
 
 #include <string>


 using namespace std;
 using namespace drizzled;

 namespace drizzle_plugin {
 namespace json_server {

    bool Schema::createSchema()
    {
      if (not validateSchemaOptions())
        return true;
      
      if (session().inTransaction())
      {
        my_error(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, MYF(0));
        return true;
      }

      drizzled::identifier::Schema schema_identifier(session().catalog().identifier(),_db_name);

      if (not check(schema_identifier))
        return false;

      drizzled::message::schema::init(schema_message, schema_identifier);
      message::set_definer(schema_message, *session().user());
        
      bool res =false;
      std::string path = schema_identifier.getSQLPath();
         
        if (unlikely(plugin::EventObserver::beforeCreateDatabase(session(), path)))
        {
          my_error(ER_EVENT_OBSERVER_PLUGIN, MYF(0), path.c_str());
        }
        else
        {
          res= schema::create(session(), schema_message, false);
          if (unlikely(plugin::EventObserver::afterCreateDatabase(session(), path, res)))
          {
            my_error(ER_EVENT_OBSERVER_PLUGIN, schema_identifier);
            res = false;
          }                      
        }
        return not res; 
    }

    bool Schema::dropSchema()
    {
      if (session().inTransaction())
      {
        my_error(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, MYF(0));
        return true;
      }
      
      drizzled::identifier::Schema schema_identifier(session().catalog().identifier(),_db_name);

      if (not schema::check(session(),schema_identifier))
      {
        my_error(ER_WRONG_DB_NAME, schema_identifier);
        return false;
      }
      
      if (session().inTransaction())
      {
        my_message(ER_LOCK_OR_ACTIVE_TRANSACTION, ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
        return true;
      }

      bool res = true;
      std::string path = schema_identifier.getSQLPath();
      if (unlikely(plugin::EventObserver::beforeDropDatabase(session(), path)))
      {
        my_error(ER_EVENT_OBSERVER_PLUGIN, schema_identifier);      
      }
      else
      {
        res= schema::drop(session(), schema_identifier, false);
        if (unlikely(plugin::EventObserver::afterDropDatabase(session(), path, res)))
        {
          my_error(ER_EVENT_OBSERVER_PLUGIN, MYF(0), path.c_str());
          res = false;
        }                                 
      }

      return res;
    
    }

    bool Schema::validateSchemaOptions()
    {
      size_t num_engine_options= schema_message.engine().options_size();
      bool rc= num_engine_options ? false : true;
          
      for (size_t y= 0; y < num_engine_options; ++y)
      {
        my_error(ER_UNKNOWN_SCHEMA_OPTION, MYF(0),schema_message.engine().options(y).name().c_str(),schema_message.engine().options(y).state().c_str());
        rc= false;
      }
      return rc;

    }

    bool Schema::check(const identifier::Schema &identifier)
    {
      if (not identifier.isValid())
        return false;
      
      if (not plugin::Authorization::isAuthorized(*session().user(), identifier))
        return false;

      if (plugin::StorageEngine::doesSchemaExist(identifier))
      {
        my_error(ER_DB_CREATE_EXISTS, identifier);
        return false;
      }

      return true;
    }
}
}
 
