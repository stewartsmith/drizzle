/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Eric Day
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

#include <fstream>
#include <map>
#include <string>

#include "drizzled/configmake.h"
#include "drizzled/plugin/authentication.h"
#include "drizzled/security_context.h"
#include "drizzled/util/convert.h"
#include "drizzled/algorithm/sha1.h"
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <drizzled/set_var.h>
#include <iostream>

namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

namespace auth_file
{

static const char DEFAULT_USERS_FILE[]= SYSCONFDIR "/drizzle.users";

class AuthFile: public plugin::Authentication
{
  const std::string users_file;

public:

  AuthFile(string name_arg, string users_file_arg);

  /**
   * Retrieve the last error encountered in the class.
   */
  string& getError(void);

  /**
   * Load the users file into a map cache.
   *
   * @return True on success, false on error. If false is returned an error
   *  is set and can be retrieved with getError().
   */
  bool loadFile(void);

private:

  /**
   * Base class method to check authentication for a user.
   */
  bool authenticate(const SecurityContext &sctx, const string &password);

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
  bool verifyMySQLHash(const string &password,
                       const string &scramble_bytes,
                       const string &scrambled_password);

  string error;

  /**
   * Cache or username:password entries from the file.
   */
  map<string, string> users;
};

AuthFile::AuthFile(string name_arg, string users_file_arg):
  plugin::Authentication(name_arg),
  users_file(users_file_arg),
  error(),
  users()
{
}

string& AuthFile::getError(void)
{
  return error;
}

bool AuthFile::loadFile(void)
{
  ifstream file(users_file.c_str());

  if (!file.is_open())
  {
    error = "Could not open users file: ";
    error += users_file;
    return false;
  }

  while (!file.eof())
  {
    string line;
    getline(file, line);

    /* Ignore blank lines and lines starting with '#'. */
    if (line == "" || line[line.find_first_not_of(" \t")] == '#')
      continue;

    string username;
    string password;
    size_t password_offset = line.find(":");
    if (password_offset == string::npos)
      username = line;
    else
    {
      username = string(line, 0, password_offset);
      password = string(line, password_offset + 1);
    }

    pair<map<string, string>::iterator, bool> result;
    result = users.insert(pair<string, string>(username, password));
    if (result.second == false)
    {
      error = "Duplicate entry found in users file: ";
      error += username;
      file.close();
      return false;
    }
  }

  file.close();
  return true;
}

bool AuthFile::verifyMySQLHash(const string &password,
                               const string &scramble_bytes,
                               const string &scrambled_password)
{
  if (scramble_bytes.size() != SHA1_DIGEST_LENGTH ||
      scrambled_password.size() != SHA1_DIGEST_LENGTH)
  {
    return false;
  }

  SHA1_CTX ctx;
  uint8_t local_scrambled_password[SHA1_DIGEST_LENGTH];
  uint8_t temp_hash[SHA1_DIGEST_LENGTH];
  uint8_t scrambled_password_check[SHA1_DIGEST_LENGTH];

  /* Generate the double SHA1 hash for the password stored locally first. */
  SHA1Init(&ctx);
  SHA1Update(&ctx, reinterpret_cast<const uint8_t *>(password.c_str()),
             password.size());
  SHA1Final(temp_hash, &ctx);

  SHA1Init(&ctx);
  SHA1Update(&ctx, temp_hash, SHA1_DIGEST_LENGTH);
  SHA1Final(local_scrambled_password, &ctx);

  /* Hash the scramble that was sent to client with the local password. */
  SHA1Init(&ctx);
  SHA1Update(&ctx, reinterpret_cast<const uint8_t*>(scramble_bytes.c_str()),
             SHA1_DIGEST_LENGTH);
  SHA1Update(&ctx, local_scrambled_password, SHA1_DIGEST_LENGTH);
  SHA1Final(temp_hash, &ctx);

  /* Next, XOR the result with what the client sent to get the original
     single-hashed password. */
  for (int x= 0; x < SHA1_DIGEST_LENGTH; x++)
    temp_hash[x]= temp_hash[x] ^ scrambled_password[x];

  /* Hash this result once more to get the double-hashed password again. */
  SHA1Init(&ctx);
  SHA1Update(&ctx, temp_hash, SHA1_DIGEST_LENGTH);
  SHA1Final(scrambled_password_check, &ctx);

  /* These should match for a successful auth. */
  return memcmp(local_scrambled_password, scrambled_password_check, SHA1_DIGEST_LENGTH) == 0;
}

bool AuthFile::authenticate(const SecurityContext &sctx, const string &password)
{
  map<string, string>::const_iterator user = users.find(sctx.getUser());
  if (user == users.end())
    return false;

  if (sctx.getPasswordType() == SecurityContext::MYSQL_HASH)
    return verifyMySQLHash(user->second, sctx.getPasswordContext(), password);

  if (password == user->second)
    return true;

  return false;
}

static int init(module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  AuthFile *auth_file = new AuthFile("auth_file", vm["users"].as<string>());
  if (!auth_file->loadFile())
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Could not load auth file: %s\n"),
                  auth_file->getError().c_str());
    delete auth_file;
    return 1;
  }

  context.add(auth_file);
  context.registerVariable(new sys_var_const_string_val("users", vm["users"].as<string>()));
  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("users", 
          po::value<string>()->default_value(DEFAULT_USERS_FILE),
          N_("File to load for usernames and passwords"));
}

} /* namespace auth_file */

DRIZZLE_PLUGIN(auth_file::init, NULL, auth_file::init_options);
