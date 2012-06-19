/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2012 Mohit Srivastava
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
 * @file Declare a class SQLExecutor that helps to execute a sql.
 */
 

#pragma once

#include <vector>
#include <drizzled/session.h>
#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/client.h>
#include <drizzled/catalog/local.h>
#include <drizzled/execute.h>
#include <drizzled/sql/result_set.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/plugin.h>


using namespace std;
using namespace drizzled;

/**
 *  Drizzle Plugin Namespace
 */
namespace drizzle_plugin
{
/**
  * Json Server Plugin namespace
  */
namespace json_server 
{
  /**
   * a class.
   * used to execute sql.
   */
  class SQLExecutor
  {
    public:
    /**
     * a constructor.
     * set the session for executing sql with user and schema.
     * intializes the member variables.
     * @param user a constant string.
     * @param schema a constant string.
     */
    SQLExecutor(const string &user, const string &schema);
    
    /**
     * a function variable.
     * set the error state as true.
     */
    void setErrorState()
    {
      _in_error_state= true;
    }
    
    /**
     * a function variable.
     * set the error state as false.
     */
    void clearErrorState()
    {
      _in_error_state= false;
    }
    
    /**
     * a constant function variable.
     * used to get error message which occurs while executing a sql.
     * @return a constant error message string.
     */
    const string& getErrorMessage() const
    {
      return _error_message;
    }
    
    /**
     * a constant function variable.
     * used to get error type which occurs while executing a sql.
     * @return a constant error type string.
     */
    const string& getErrorType() const
    {
      return _error_type;
    }
    
    /**
     * a constant function variable.
     * used to get error code which occurs while executing a sql.
     * @return a constant error code string.
     */
    const string& getErrorCode() const
    {
      return _error_code;
    }
    
    /**
     * a constant function variable.
     * used to get internal sql query.
     * @return a constant sql query string.
     */
    const string& getInternalSqlQuery() const
    {
      return _internal_sql_query;	
    }
    
    /**
     * a constant function variable.
     * used to get sql state.
     * @return a constant sql state string.
     */
    const string& getSqlState() const
    {
      return _sql_state;
    }
    
    /**
     * a constant function variable.
     * used to get resultset object.
     * @return a resultset.
     */
    sql::ResultSet* getResultSet() const  
    {
      return _result_set;
    }
    
    /**
     * a constant function variable.
     * used to get sql string.
     * @return a constant sql string.
     */
    const string& getSql() const
    {
      return _sql;
    }
    
    /**
     * a constant function variable.
     * used to get error.
     * @return a constant error_t object.
     */
    const drizzled::error_t& getErr() const
    {
      return _err;
    }
  
    /**
     * a function variable.
     * execute a batch of SQL statements.
     * @param sql Batch of SQL statements to execute.
     * @return true Success
     * @return false Failure
     */
    bool executeSQL(string &sql);

  protected:
    /**
     * a protected variable.
     * stores session.
     */
    drizzled::Session::shared_ptr _session;

  private:
    /**
     * a private variable
     * stores an error state. 
     */
    bool _in_error_state;
    /**
     * a private variable.
     * stores an error message occurs while executing a sql.
     */
    string _error_message;
    /**
     * a private variable.
     * stores an error type occurs while executing a sql.
     */
    string _error_type;
    /**
     * a private variable.
     * stores an error code occurs while executing a sql.
     */
    string _error_code;
    /**
     * a private variable.
     * stores an internal sql query.
     */
    string _internal_sql_query;
    /**
     * a private variable.
     * stores sql state.
     */
    string _sql_state;
    /**
     * a private variable.
     * stores execption occurs while executing a sql query.
     */
    sql::Exception _exception;
    /**
     * a private variable.
     * stores an error occurs while executing a sql.
     */
    drizzled::error_t _err;
    /**
     * a private variable.
     * stores resultset of a sql transaction.
     */
    sql::ResultSet* _result_set;
    /**
     * a private variable.
     * stores a sql string.
     */
    string _sql;
  };

}
}

