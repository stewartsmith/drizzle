/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Joseph Daly <skinny.moey@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
#include "transaction_log_connection.h"
#include <iostream>

using namespace std;
using namespace drizzled;

TransactionLogConnection::TransactionLogConnection(string &host, uint16_t port,
                                                   string &username, string &password,
                                                   bool drizzle_protocol)
  :
    hostName(host),
    drizzleProtocol(drizzle_protocol)
{
  drizzle_return_t ret;

  if (host.empty())
    host= "localhost";

  drizzle_create(&drizzle);
  drizzle_con_create(&drizzle, &connection);
  drizzle_con_set_tcp(&connection, (char *)host.c_str(), port);
  drizzle_con_set_auth(&connection, (char *)username.c_str(),
    (char *)password.c_str());
  drizzle_con_add_options(&connection,
    drizzle_protocol ? DRIZZLE_CON_EXPERIMENTAL : DRIZZLE_CON_MYSQL);
  ret= drizzle_con_connect(&connection);
  if (ret != DRIZZLE_RETURN_OK)
  {
    cerr << "Error trying to connect" << endl;
    throw 1;
  }
}

drizzle_result_st* TransactionLogConnection::query(std::string &str_query)
{
  drizzle_return_t ret;
  drizzle_result_st* result= new drizzle_result_st;
  if (drizzle_query_str(&connection, result, str_query.c_str(), &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      cerr << "Error executing query" << endl;
      drizzle_result_free(result);
    }
    else
    {
      cerr << "Error executing query" << endl;
    }
    return NULL;
  }

  if (drizzle_result_buffer(result) != DRIZZLE_RETURN_OK)
  {
    cerr << "Could not buffer result" << endl;
    return NULL;
  }
  return result;
}
