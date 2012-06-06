/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 David Shrewsbury
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

#pragma once

#include <string>
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


namespace drizzle_plugin
{
namespace json_server {

class SQLExecutor
{
public:

  SQLExecutor(const string &user, const string &schema);

  void markInErrorState();

  void clearErrorState();

  const string &getErrorMessage() const;

  const string &getErrorType() const;

  const string &getErrorCode() const;

  const string &getInternalSqlQuery() const;

  const string &getSqlState() const;
 
  sql::ResultSet* getResultSet() const;

  /**
   * Execute a batch of SQL statements.
   *
   * @param sql Batch of SQL statements to execute.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool executeSQL(string &sql);

protected:
  drizzled::Session::shared_ptr _session;

private:
  bool _in_error_state;
  string _error_message;
  string _error_type;
  string _error_code;
  string _internal_sql_query;
  string _sql_state;
  sql::Exception _exception;
  drizzled::error_t _err;
  sql::ResultSet* _result_set;

};

} /* namespace slave */
}

