/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright 2011 Daniel Nichter
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

#pragma once
#include <drizzled/session.h>
#include <drizzled/plugin/authentication.h>

using namespace std;
using namespace drizzled;

namespace drizzle_plugin {
namespace auth_schema {

class AuthSchema : public drizzled::plugin::Authentication
{
public:
  AuthSchema();

  /**
   * @brief
   *   Set/change the authentication table.
   *
   * @param[in] table Schema-qualified table name.
   *
   * @returns false Always returns false currently because table isn't checked.
   */
  bool setTable(const char *table);

  /**
   * These are the query_log system variables.  So sysvar_enabled is
   * auth_schema_enabled in SHOW VARIABLES, etc.  They are all global
   * and dynamic.
   */
  bool sysvar_enabled;
  string sysvar_table;

private:
  /**
   * Base class method to check authentication for a user.
   */
  bool authenticate(const identifier::User &sctx, const string &password);

  /**
   * @brief
   *   Verify that the client password matches the real password.
   *
   * @details
   *   This method compares two MySQL hashed passwords: one from the
   *   client who is trying to authenticate, and the other from an
   *   auth table with the real password.  The client's password is
   *   hashed with the scramble bytes that Drizzle sent when the client
   *   connected, so we hash the real password with these bytes, too.
   *   This method is a modified copy of auth_file::verifyMySQLHash(),
   *   written by Eric Day, so credit the credit is his for the algos.
   *
   * @param[in] real_password   Real password, double-hashed but not yet
   *                            scrambled with the scramble bytes.
   * @param[in] scramble_bytes  Random bytes sent by Drizzle to client.
   * @param[in] client_password Password sent by client, double-hashed and
   *                            scrambled with the scramble bytes.
   *
   * @return True if the passwords match, else false.
   */
  bool verifyMySQLPassword(const string &real_password,
                           const string &scramble_bytes,
                           const string &client_password);

  Session::shared_ptr _session; ///< Internal session for querying auth table
};

} /* end namespace drizzle_plugin::auth_schema */
} /* end namespace drizzle_plugin */
