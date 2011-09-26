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

#include <config.h>
#include <drizzled/identifier.h>
#include <drizzled/util/convert.h>
#include <drizzled/algorithm/sha1.h>
#include <drizzled/execute.h>
#include <drizzled/sql/result_set.h>
#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/client.h>
#include <drizzled/catalog/local.h>
#include "auth_schema.h"

using namespace std;
using namespace drizzled;
using namespace plugin;

AuthSchema::AuthSchema() :
  plugin::Authentication("auth_schema")
{
}

bool AuthSchema::setTable(const char *table)
{
  _table= table;
  return false;
}

bool AuthSchema::verifyMySQLPassword(const string &real_password,
                                     const string &scramble_bytes,
                                     const string &client_password)
{
  if (scramble_bytes.size() != SHA1_DIGEST_LENGTH
      || client_password.size() != SHA1_DIGEST_LENGTH)
    return false;

  uint8_t real_password_hash[SHA1_DIGEST_LENGTH];
  drizzled_hex_to_string(
    reinterpret_cast<char *>(real_password_hash),
    real_password.c_str(),
    SHA1_DIGEST_LENGTH * 2);

  /* Hash the scramble that was sent to client with the local password. */
  SHA1_CTX ctx;
  uint8_t temp_hash[SHA1_DIGEST_LENGTH];
  SHA1Init(&ctx);
  SHA1Update(&ctx, reinterpret_cast<const uint8_t*>(scramble_bytes.c_str()), SHA1_DIGEST_LENGTH);
  SHA1Update(&ctx, real_password_hash, SHA1_DIGEST_LENGTH);
  SHA1Final(temp_hash, &ctx);

  /* Next, XOR the result with what the client sent to get the original
     single-hashed password. */
  for (int x= 0; x < SHA1_DIGEST_LENGTH; x++)
    temp_hash[x]= temp_hash[x] ^ client_password[x];

  /* Hash this result once more to get the double-hashed password again. */
  uint8_t client_password_hash[SHA1_DIGEST_LENGTH];
  SHA1Init(&ctx);
  SHA1Update(&ctx, temp_hash, SHA1_DIGEST_LENGTH);
  SHA1Final(client_password_hash, &ctx);

  /* These should match for a successful auth. */
  return memcmp(real_password_hash, client_password_hash, SHA1_DIGEST_LENGTH) == 0;
}

bool AuthSchema::authenticate(const identifier::User &sctx, const string &password)
{
  if (sctx.getPasswordType() != identifier::User::MYSQL_HASH)
    return false;

  string user= sctx.username();
  if (user.empty())
    return false;

  if (!_session) {
    _session= Session::make_shared(Listen::getNullClient(), catalog::local());
    identifier::user::mptr user_id= identifier::User::make_shared();
    user_id->setUser("auth_schema");
    _session->setUser(user_id);
  }
  
  /* Execute wraps the SQL to run within a transaction */ 
  string sql= "SELECT password FROM " + _table +
              " WHERE user='" + user + "'"
              " LIMIT 1;";
  Execute execute(*(_session.get()), true);
  sql::ResultSet result_set(1);
  execute.run(sql, result_set);
  sql::Exception exception= result_set.getException();
  drizzled::error_t err= exception.getErrorCode();
  if ((err != EE_OK) && (err != ER_EMPTY_QUERY))
  {
    errmsg_printf(error::ERROR,
      _("Error querying authentication schema: %s (error code %d)"),
      exception.getErrorMessage().c_str(), exception.getErrorCode());
    return false;
  }

  if (result_set.next() and not result_set.isNull(0))
  {
    string real_password= result_set.getString(0);
    return verifyMySQLPassword(
      real_password,
      sctx.getPasswordContext(),
      password);
  }

  return false;
}
