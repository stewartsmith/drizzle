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
 * @file Implements the various functions of class SQLExecutor
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
SQLExecutor::SQLExecutor(const string &schema)
  : _in_error_state(false)
{
  /* setup a Session object */
  _session= Session::make_shared(plugin::Listen::getNullClient(), catalog::local());
  identifier::user::mptr user_id= identifier::User::make_shared();
  user_id->setUser("");
  _session->setUser(user_id);
  _session->set_schema(schema);
  _sql="";
  _result_set=NULL;
}

bool SQLExecutor::executeSQL(string &sql)
{
  _sql=sql;
  if(_result_set)
  {
    delete(_result_set);
  }
  _result_set= new sql::ResultSet(1);
  Execute execute(*(_session.get()), true);
  /*wraps the SQL to run within a transaction */
  execute.run(_sql, *_result_set);

  _exception= _result_set->getException();

  _err= _exception.getErrorCode();

  if ((_err != EE_OK) && (_err != ER_EMPTY_QUERY))
  {
    /* avoid recursive errors */
    if (_in_error_state)
    {
      return true;
    }

    return false;
  }

  return true;
}
} /* namespace json_server*/ 
}     
