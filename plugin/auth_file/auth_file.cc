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

#include <config.h>

#include <fstream>
#include <map>
#include <string>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <drizzled/item.h>
#include <drizzled/configmake.h>
#include <drizzled/plugin/authentication.h>
#include <drizzled/identifier.h>
#include <drizzled/util/convert.h>
#include <drizzled/algorithm/sha1.h>
#include <drizzled/module/option_map.h>

namespace po= boost::program_options;
namespace fs= boost::filesystem;

using namespace std;
using namespace drizzled;

namespace auth_file {

static const fs::path DEFAULT_USERS_FILE= SYSCONFDIR "/drizzle.users";
typedef std::map<string, string> users_t;
bool updateUsersFile(Session *, set_var *);
bool parseUsersFile(std::string, users_t&);

class AuthFile : public plugin::Authentication
{
public:
  AuthFile(std::string users_file_arg);

  /**
   * Retrieve the last error encountered in the class.
   */
  const string& getError() const;

  std::string& getUsersFile();
  bool setUsersFile(std::string& usersFile);

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
  bool verifyMySQLHash(const string &password,
                       const string &scramble_bytes,
                       const string &scrambled_password);

  string error;
  fs::path users_file;
  std::string sysvar_users_file;

  /**
   * Cache or username:password entries from the file.
   */
  users_t users;
  /**
   * This method is called to update the users cache with the new 
   * username:password pairs given in new users file upon update.
   */
  void setUsers(users_t);
  /**
   * This method is called to delete all the cached username:password pairs
   * when users file is updated.
   */
  void clearUsers();
};
AuthFile *auth_file= NULL;

AuthFile::AuthFile(std::string users_file_arg) :
  plugin::Authentication("auth_file"),
  users_file(users_file_arg), sysvar_users_file(users_file_arg)
{
}

const string& AuthFile::getError() const
{
  return error;
}

std::string& AuthFile::getUsersFile()
{
  return sysvar_users_file;
}

bool AuthFile::setUsersFile(std::string& usersFile)
{
  if (usersFile.empty())
  {
    errmsg_printf(error::ERROR, _("users file cannot be an empty string"));
    return false;  // error
  }
  users_t users_dummy;
  if(parseUsersFile(usersFile, users_dummy))
  {
    this->clearUsers();
    this->setUsers(users_dummy);
    sysvar_users_file= usersFile;
    fs::path newUsersFile(getUsersFile());
    users_file= newUsersFile;
    return true;  //success
  }
  return false;  // error
}

bool AuthFile::verifyMySQLHash(const string &password,
                               const string &scramble_bytes,
                               const string &scrambled_password)
{
  if (scramble_bytes.size() != SHA1_DIGEST_LENGTH || scrambled_password.size() != SHA1_DIGEST_LENGTH)
  {
    return false;
  }

  SHA1_CTX ctx;
  uint8_t local_scrambled_password[SHA1_DIGEST_LENGTH];
  uint8_t temp_hash[SHA1_DIGEST_LENGTH];
  uint8_t scrambled_password_check[SHA1_DIGEST_LENGTH];

  /* Generate the double SHA1 hash for the password stored locally first. */
  SHA1Init(&ctx);
  SHA1Update(&ctx, reinterpret_cast<const uint8_t *>(password.c_str()), password.size());
  SHA1Final(temp_hash, &ctx);

  SHA1Init(&ctx);
  SHA1Update(&ctx, temp_hash, SHA1_DIGEST_LENGTH);
  SHA1Final(local_scrambled_password, &ctx);

  /* Hash the scramble that was sent to client with the local password. */
  SHA1Init(&ctx);
  SHA1Update(&ctx, reinterpret_cast<const uint8_t*>(scramble_bytes.c_str()), SHA1_DIGEST_LENGTH);
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

bool AuthFile::authenticate(const identifier::User &sctx, const string &password)
{
  string* user= find_ptr(users, sctx.username());
  if (not user)
    return false;
  return sctx.getPasswordType() == identifier::User::MYSQL_HASH
    ? verifyMySQLHash(*user, sctx.getPasswordContext(), password)
    : password == *user;
}

void AuthFile::setUsers(users_t users_dummy)
{
  users.insert(users_dummy.begin(), users_dummy.end());
}

void AuthFile::clearUsers()
{
  users.clear();
}

/**
 * This function is called when the value of users file is changed in the system.
 *
 * @return False on success, True on error.
 */
bool updateUsersFile(Session *, set_var* var)
{
  if (not var->value->str_value.empty())
  {
    std::string newUsersFile(var->value->str_value.data());
    if (auth_file->setUsersFile(newUsersFile))
      return false; //success
    else
      return true; // error
  }
  errmsg_printf(error::ERROR, _("auth_file file cannot be NULL"));
  return true; // error
}

/**
 * Parse the users file into a map cache.
 *
 * @return True on success, false on error. If an error is encountered, false is
 * returned with error message printed.
 */
bool parseUsersFile(std::string new_users_file, users_t& users_dummy)
{
  ifstream file(new_users_file.c_str());

  if (!file.is_open())
  {
    string error_msg= "Could not open users file: " + new_users_file;
    errmsg_printf(error::ERROR, _(error_msg.c_str()));
    return false;
  }

  string line;
  try
  {
    while (getline(file, line))
    {
      /* Ignore blank lines and lines starting with '#'. */
      if (line.empty() || line[line.find_first_not_of(" \t")] == '#')
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
  
      if (not users_dummy.insert(pair<string, string>(username, password)).second)
      {
        string error_msg= "Duplicate entry found in users file: " + username;
        errmsg_printf(error::ERROR, _(error_msg.c_str()));
        return false;
      }
    }
    return true;
  }
  catch (const std::exception &e)
  {
    /* On any non-EOF break, unparseable line */
    string error_msg= "Unable to parse users file " + new_users_file + ":" + e.what();
    errmsg_printf(error::ERROR, _(error_msg.c_str()));
    return false;
  }
}

static int init(module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  auth_file= new AuthFile(vm["users"].as<string>());
  if (!auth_file->setUsersFile(auth_file->getUsersFile()))
  {
    errmsg_printf(error::ERROR, _("Could not load auth file: %s\n"), auth_file->getError().c_str());
    delete auth_file;
    return 1;
  }

  context.add(auth_file);
  context.registerVariable(new sys_var_std_string("users", auth_file->getUsersFile(), NULL, &updateUsersFile));

  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("users", 
          po::value<string>()->default_value(DEFAULT_USERS_FILE.string()),
          N_("File to load for usernames and passwords"));
}

} /* namespace auth_file */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "auth_file",
  "0.1",
  "Eric Day",
  N_("Authentication against a plain text file"),
  PLUGIN_LICENSE_GPL,
  auth_file::init,
  NULL,
  auth_file::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
