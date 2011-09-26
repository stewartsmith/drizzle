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

namespace drizzled {
namespace plugin {

class AuthSchema : public drizzled::plugin::Authentication
{
public:
  AuthSchema();
  bool setTable(const char *table);
  string _table;

private:
  /**
   * Base class method to check authentication for a user.
   */
  bool authenticate(const identifier::User &sctx, const string &password);

  /**
   * Verify the local and remote scrambled password match using the MySQL
   * hashing algorithm.
   *
   * @param[in] password Plain text password that is stored locally.
   * @param[in] scramble_bytes The random bytes that the server sent to the
   *  client for scrambling the password.
   * @param[in] scrambled_password The result of the client scrambling the
   *  password remotely.
   * @return True if the password matched, false if not.
   */
  bool verifyMySQLPassword(const string &real_password,
                           const string &scramble_bytes,
                           const string &client_password);

  Session::shared_ptr _session;
};

} // namespace plugin
} // namesapce drizzled
