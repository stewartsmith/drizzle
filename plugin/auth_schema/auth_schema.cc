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

namespace drizzle_plugin {
namespace auth_schema {

AuthSchema::AuthSchema(bool enabled) :
  plugin::Authentication("auth_schema"),
    sysvar_enabled(enabled)
{
  const char *error;
  int erroffset;
  _ident_re= pcre_compile(
    "^`[^`]+`",   /* the pattern */
    0,            /* default options */
    &error,       /* for error message */
    &erroffset,   /* for error offset */
    NULL);        /* use default character tables */
}

bool AuthSchema::setTable(const string &table)
{
  if (table.empty())
  {
    errmsg_printf(error::ERROR, _("auth_schema table cannot be an empty string"));
    return true;  // error
  }

  if (table.find(".") == string::npos)
  {
    errmsg_printf(error::ERROR, _("auth_schema must be schema-qualified"));
    return true;  // error
  }

  sysvar_table= escapeQuoteAuthTable(table);

  return false;  // success
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
  // If plugin is disabled, deny everyone.
  if (not sysvar_enabled)
    return false;

  // Plugin only works with MySQL hash passwords, so client needs
  // to connect with --protocol mysql-plugin-auth.
  if (sctx.getPasswordType() != identifier::User::MYSQL_HASH)
    return false;

  // Anonymous users are not allowed.
  string user= escapeString(sctx.username());
  if (user.empty())
    return false;

  // Create an internal session for ourself the first time we're called.
  // I don't know why but doing this in the constructor crashes Drizzle
  if (not _session)
  {
    _session= Session::make_shared(plugin::Listen::getNullClient(), catalog::local());
    identifier::user::mptr user_id= identifier::User::make_shared();
    user_id->setUser("auth_schema");
    _session->setUser(user_id);
  }

  // Create an execute a SQL statement to select the user from the auth table.
  // Execute wraps the SQL to run within a transaction.
  string sql= "SELECT password FROM " + sysvar_table + " WHERE user='" + user + "' LIMIT 1;";
  Execute execute(*(_session.get()), true);
  sql::ResultSet result_set(1);
  execute.run(sql, result_set);
  sql::Exception exception= result_set.getException();
  drizzled::error_t err= exception.getErrorCode();
  if ((err != EE_OK) && (err != ER_EMPTY_QUERY))
  {
    errmsg_printf(error::ERROR,
      _("Error querying authentication schema: %s (error code %d.  Query: %s"),
      exception.getErrorMessage().c_str(), exception.getErrorCode(), sql.c_str());
    return false;
  }

  // If there's a result and it's not null, verify the password from
  // the client against the real password from the auth table.
  if (result_set.next() and not result_set.isNull(0))
  {
    string real_password= result_set.getString(0);
    // Return true if auth succeeds, else return false.
    return verifyMySQLPassword(
      real_password,
      sctx.getPasswordContext(),
      password);
  }

  // User doesn't exist in auth table; auth fails.
  return false;
}

string AuthSchema::escapeQuoteAuthTable(const string &table)
{
  int pos= table.find(".");
  string quoted_schema= escapeQuoteIdentifier(table.substr(0, pos));
  string quoted_table= escapeQuoteIdentifier(table.substr(pos + 1, table.length() - pos));
  return quoted_schema + "." + quoted_table;
}

string AuthSchema::escapeQuoteIdentifier(const string &input)
{
  if (input.empty())
    return "``";

  /**
   * The input may already be a quoted ident with no extra backticks.
   * If so, return it.
   */
  int match_result= pcre_exec(
    _ident_re, NULL, input.c_str(), input.length(), 0, 0, NULL, 0);
  if (match_result >= 0)
    return input;

  const char *pos= input.c_str();
  const char *end= input.c_str()+input.length();
  string ident= "`";

  for (; pos != end ; pos++)
  {
    switch (*pos) {
    case '`':
      ident.push_back('\\');
      ident.push_back('`');
      break;
    case '\\':
      ident.push_back('\\');
      ident.push_back('\\');
      break;
    default:
      ident.push_back(*pos);
      break;
    }
  }

  ident.push_back('`');

  return ident;
}

string AuthSchema::escapeString(const string &input)
{
  if (input.empty())
    return input;

  const char *pos= input.c_str();
  const char *end= input.c_str()+input.length();
  string res;

  for (; pos != end ; pos++)
  {
    switch (*pos) {
    case 0:
      res.push_back('\\');
      res.push_back('0');
      break;
    case '\n':
      res.push_back('\\');
      res.push_back('n');
      break;
    case '\r':
      res.push_back('\\');
      res.push_back('r');
      break;
    case '\\':
      res.push_back('\\');
      res.push_back('\\');
      break;
    case '\'':
      res.push_back('\\');
      res.push_back('\'');
      break;
    default:
      res.push_back(*pos);
      break;
    }
  }

  return res;
}

AuthSchema::~AuthSchema()
{
  if (_ident_re != NULL)
    pcre_free(_ident_re);
}

} /* end namespace drizzle_plugin::auth_schema */
} /* end namespace drizzle_plugin */
