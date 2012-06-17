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
 * @file Executes the sql command
 */


#include <config.h>
#include <plugin/json_server/sql_executor.h>
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
namespace json_server
{
/**
 * Constructor
 */
SQLExecutor::SQLExecutor(const string &user, const string &schema)
  : _in_error_state(false)
{
  /* setup a Session object */
  _session= Session::make_shared(plugin::Listen::getNullClient(), catalog::local());
  identifier::user::mptr user_id= identifier::User::make_shared();
  user_id->setUser(user);
  _session->setUser(user_id);
  _session->set_schema(schema);
  _result_set= new sql::ResultSet(1);
  _sql="";
}

/**
 * Function to execute a sql string
 */
bool SQLExecutor::executeSQL(string &sql)
{
  _sql=sql;
  if (not _in_error_state)
    _error_message.clear();

  Execute execute(*(_session.get()), true);

  /* Execute wraps the SQL to run within a transaction */
  execute.run(_sql, *_result_set);

  _exception= _result_set->getException();

  _err= _exception.getErrorCode();

  _sql_state= _exception.getSQLState();

  if ((_err != EE_OK) && (_err != ER_EMPTY_QUERY))
  {
    /* avoid recursive errors */
    if (_in_error_state)
    {
      return true;
    }

    _in_error_state= true; 
    _error_type= "sql error";
    _error_message= _exception.getErrorMessage();
    _error_code= _exception.getErrorCode();
    _internal_sql_query=sql;

    return false;
  }

  return true;
}
} /* namespace json_server */
}
