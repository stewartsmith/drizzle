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
 * @file Declare a class SQLExecutor that executes given sql string.
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
   * Execute given sql string.
   */
  class SQLExecutor
  {
    public:
    /**
     * Constructor
     * 
     * @param user a constant string.
     * @param schema a constant string.
     */
    SQLExecutor(const string &schema);
    
    /**
     * set the error state as true.
     */
    void setErrorState()
    {
      _in_error_state= true;
    }
    
    /**
     * set the error state as false.
     */
    void clearErrorState()
    {
      _in_error_state= false;
    }
    
    /**
     * used to get error execption which occurs while executing a sql.
     * @return a constant sql exception object.
     */
    const sql::Exception getException() const
    {
      return _exception;
    }

    /**
     * used to get resultset object.
     * @return a resultset.
     */
    sql::ResultSet* getResultSet() const  
    {
      return _result_set;
    }
    
    /**
     * used to get sql string.
     * @return a constant sql string.
     */
    const string& getSql() const
    {
      return _sql;
    }
    
    /**
     * used to get error.
     * @return a constant error_t object.
     */
    const drizzled::error_t& getErr() const
    {
      return _err;
    }
  
    /**
     * execute a batch of SQL statements.
     * @param sql Batch of SQL statements to execute.
     * @return true Success
     * @return false Failure
     */
    bool executeSQL(string &sql);

  protected:
    /**
     * stores session.
     */
    drizzled::Session::shared_ptr _session;

  private:
    /**
     * Stores whether an error has happened. 
     */
    bool _in_error_state;
    /**
     * Stores execption if one occurs while executing a sql query.
     */
    sql::Exception _exception;
    /**
     * Stores the error state of executing a sql.
     */
    drizzled::error_t _err;
    /**
     * Stores resultset of a sql transaction.
     */
    sql::ResultSet* _result_set;
    /**
     * Stores a sql string.
     */
    string _sql;
  };

}
}

