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

/* This is needed for simple auth, we're not ready for SASL yet. */
#define LDAP_DEPRECATED 1

#include <ldap.h>
#include <pthread.h>
#include <sys/time.h>
#include <map>
#include <string>

#include <drizzled/plugin/authentication.h>
#include <drizzled/identifier.h>
#include <drizzled/util/convert.h>
#include <drizzled/algorithm/sha1.h>

#include <drizzled/module/option_map.h>
#include <boost/program_options.hpp>

namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

namespace auth_ldap
{

std::string uri;
const std::string DEFAULT_URI= "ldap://127.0.0.1/";
std::string bind_dn;
std::string bind_password;
std::string base_dn;
std::string password_attribute;
std::string DEFAULT_PASSWORD_ATTRIBUTE= "userPassword";
std::string mysql_password_attribute;
const std::string DEFAULT_MYSQL_PASSWORD_ATTRIBUTE= "mysqlUserPassword";
static const int DEFAULT_CACHE_TIMEOUT= 600;
typedef constrained_check<int, DEFAULT_CACHE_TIMEOUT, 0, 2147483647> cachetimeout_constraint;
static cachetimeout_constraint cache_timeout= 0;


class AuthLDAP: public plugin::Authentication
{
public:

  AuthLDAP(string name_arg);
  ~AuthLDAP();

  /**
   * Initialize the LDAP connection.
   *
   * @return True on success, false otherwise.
   */
  bool initialize(void);

  /**
   * Connect to the LDAP server.
   *
   * @return True on success, false otherwise.
   */
  bool connect(void);

  /**
   * Retrieve the last error encountered in the class.
   */
  string& getError(void);

private:

  typedef enum
  {
    NOT_FOUND,
    PLAIN_TEXT,
    MYSQL_HASH
  } PasswordType;

  typedef std::pair<PasswordType, std::string> PasswordEntry;
  typedef std::pair<std::string, PasswordEntry> UserEntry;
  typedef std::map<std::string, PasswordEntry> UserCache;

  /**
   * Base class method to check authentication for a user.
   */
  bool authenticate(const identifier::User &sctx, const string &password);

  /**
   * Lookup a user in LDAP.
   *
   * @param[in] Username to lookup.
   */
  void lookupUser(const string& user);

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
  bool verifyMySQLHash(const PasswordEntry &password,
                       const string &scramble_bytes,
                       const string &scrambled_password);

  time_t next_cache_expiration;
  LDAP *ldap;
  string error;
  UserCache users;
  pthread_rwlock_t lock;
};

AuthLDAP::AuthLDAP(string name_arg):
  plugin::Authentication(name_arg),
  next_cache_expiration(),
  ldap(),
  error(),
  users()
{
}

AuthLDAP::~AuthLDAP()
{
  pthread_rwlock_destroy(&lock);
  if (ldap != NULL)
    ldap_unbind(ldap);
}

bool AuthLDAP::initialize(void)
{
  int return_code= pthread_rwlock_init(&lock, NULL);
  if (return_code != 0)
  {
    error= "pthread_rwlock_init failed";
    return false;
  }

  return connect();
}

bool AuthLDAP::connect(void)
{
  int return_code= ldap_initialize(&ldap, (char *)uri.c_str());
  if (return_code != LDAP_SUCCESS)
  {
    error= "ldap_initialize failed: ";
    error+= ldap_err2string(return_code);
    return false;
  }

  int version= 3;
  return_code= ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
  if (return_code != LDAP_SUCCESS)
  {
    ldap_unbind(ldap);
    ldap= NULL;
    error= "ldap_set_option failed: ";
    error+= ldap_err2string(return_code);
    return false;
  }

  if (not bind_dn.empty())
  {
    return_code= ldap_simple_bind_s(ldap, (char *)bind_dn.c_str(), (char *)bind_password.c_str());
    if (return_code != LDAP_SUCCESS)
    {
      ldap_unbind(ldap);
      ldap= NULL;
      error= "ldap_simple_bind_s failed: ";
      error+= ldap_err2string(return_code);
      return false;
    }
  }

  return true;
}

string& AuthLDAP::getError(void)
{
  return error;
}

bool AuthLDAP::authenticate(const identifier::User &sctx, const string &password)
{
  /* See if cache should be emptied. */
  if (cache_timeout > 0)
  {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    if (current_time.tv_sec > next_cache_expiration)
    {
      pthread_rwlock_wrlock(&lock);
      /* Make sure another thread didn't already clear it. */
      if (current_time.tv_sec > next_cache_expiration)
      {
        users.clear();
        next_cache_expiration= current_time.tv_sec + cache_timeout;
      }
      pthread_rwlock_unlock(&lock);
    }
  }

  pthread_rwlock_rdlock(&lock);

  AuthLDAP::UserCache::const_iterator user= users.find(sctx.username());
  if (user == users.end())
  {
    pthread_rwlock_unlock(&lock);

    pthread_rwlock_wrlock(&lock);

    /* Make sure the user was not added while we unlocked. */
    user= users.find(sctx.username());
    if (user == users.end())
      lookupUser(sctx.username());

    pthread_rwlock_unlock(&lock);

    pthread_rwlock_rdlock(&lock);

    /* Get user again because map may have changed while unlocked. */
    user= users.find(sctx.username());
    if (user == users.end())
    {
      pthread_rwlock_unlock(&lock);
      return false;
    }
  }

  if (user->second.first == NOT_FOUND)
  {
    pthread_rwlock_unlock(&lock);
    return false;
  }

  if (sctx.getPasswordType() == identifier::User::MYSQL_HASH)
  {
    bool allow= verifyMySQLHash(user->second, sctx.getPasswordContext(), password);
    pthread_rwlock_unlock(&lock);
    return allow;
  }

  if (user->second.first == PLAIN_TEXT && password == user->second.second)
  {
    pthread_rwlock_unlock(&lock);
    return true;
  }

  pthread_rwlock_unlock(&lock);
  return false;
}

void AuthLDAP::lookupUser(const string& user)
{
  string filter("(cn=" + user + ")");
  const char *attributes[3]=
  {
    (char *)password_attribute.c_str(),
    (char *)mysql_password_attribute.c_str(),
    NULL
  };
  LDAPMessage *result;
  bool try_reconnect= true;

  while (true)
  {
    if (ldap == NULL)
    {
      if (! connect())
      {
        errmsg_printf(error::ERROR, _("Reconnect failed: %s\n"),
                      getError().c_str());
        return;
      }
    }

    int return_code= ldap_search_ext_s(ldap,
                                       (char *)base_dn.c_str(),
                                       LDAP_SCOPE_ONELEVEL,
                                       filter.c_str(),
                                       const_cast<char **>(attributes),
                                       0,
                                       NULL,
                                       NULL,
                                       NULL,
                                       1,
                                       &result);
    if (return_code != LDAP_SUCCESS)
    {
      errmsg_printf(error::ERROR, _("ldap_search_ext_s failed: %s\n"),
                    ldap_err2string(return_code));

      /* Only try one reconnect per request. */
      if (try_reconnect)
      {
        try_reconnect= false;
        ldap_unbind(ldap);
        ldap= NULL;
        continue;
      }

      return;
    }

    break;
  }

  LDAPMessage *entry= ldap_first_entry(ldap, result);
  AuthLDAP::PasswordEntry new_password;
  if (entry == NULL)
    new_password= AuthLDAP::PasswordEntry(NOT_FOUND, "");
  else
  {
    char **values= ldap_get_values(ldap, entry, (char *)mysql_password_attribute.c_str());
    if (values == NULL)
    {
      values= ldap_get_values(ldap, entry, (char *)password_attribute.c_str());
      if (values == NULL)
        new_password= AuthLDAP::PasswordEntry(NOT_FOUND, "");
      else
      {
        new_password= AuthLDAP::PasswordEntry(PLAIN_TEXT, values[0]);
        ldap_value_free(values);
      }
    }
    else
    {
      new_password= AuthLDAP::PasswordEntry(MYSQL_HASH, values[0]);
      ldap_value_free(values);
    }
  }

  users.insert(AuthLDAP::UserEntry(user, new_password));
}

bool AuthLDAP::verifyMySQLHash(const PasswordEntry &password,
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

  if (password.first == MYSQL_HASH)
  {
    /* Get the double-hashed password from the given hex string. */
    drizzled_hex_to_string(reinterpret_cast<char*>(local_scrambled_password),
                           password.second.c_str(), SHA1_DIGEST_LENGTH * 2);
  }
  else
  {
    /* Generate the double SHA1 hash for the password stored locally first. */
    SHA1Init(&ctx);
    SHA1Update(&ctx, reinterpret_cast<const uint8_t *>(password.second.c_str()),
               password.second.size());
    SHA1Final(temp_hash, &ctx);

    SHA1Init(&ctx);
    SHA1Update(&ctx, temp_hash, SHA1_DIGEST_LENGTH);
    SHA1Final(local_scrambled_password, &ctx);
  }

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

static int init(module::Context &context)
{
  AuthLDAP *auth_ldap= new AuthLDAP("auth_ldap");
  if (! auth_ldap->initialize())
  {
    errmsg_printf(error::ERROR, _("Could not load auth ldap: %s\n"),
                  auth_ldap->getError().c_str());
    delete auth_ldap;
    return 1;
  }

  context.registerVariable(new sys_var_const_string_val("uri", uri));
  context.registerVariable(new sys_var_const_string_val("bind-dn", bind_dn));
  context.registerVariable(new sys_var_const_string_val("bind-password", bind_password));
  context.registerVariable(new sys_var_const_string_val("base-dn", base_dn));
  context.registerVariable(new sys_var_const_string_val("password-attribute",password_attribute));
  context.registerVariable(new sys_var_const_string_val("mysql-password-attribute", mysql_password_attribute));
  context.registerVariable(new sys_var_constrained_value_readonly<int>("cache-timeout", cache_timeout));

  context.add(auth_ldap);
  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("uri", po::value<string>(&uri)->default_value(DEFAULT_URI),
          N_("URI of the LDAP server to contact"));
  context("bind-db", po::value<string>(&bind_dn)->default_value(""),
          N_("DN to use when binding to the LDAP server"));
  context("bind-password", po::value<string>(&bind_password)->default_value(""),
          N_("Password to use when binding the DN"));
  context("base-dn", po::value<string>(&base_dn)->default_value(""),
          N_("DN to use when searching"));
  context("password-attribute", po::value<string>(&password_attribute)->default_value(DEFAULT_PASSWORD_ATTRIBUTE),
          N_("Attribute in LDAP with plain text password"));
  context("mysql-password-attribute", po::value<string>(&mysql_password_attribute)->default_value(DEFAULT_MYSQL_PASSWORD_ATTRIBUTE),
          N_("Attribute in LDAP with MySQL hashed password"));
  context("cache-timeout", po::value<cachetimeout_constraint>(&cache_timeout)->default_value(DEFAULT_CACHE_TIMEOUT),
          N_("How often to empty the users cache, 0 to disable"));
}

} /* namespace auth_ldap */

DRIZZLE_PLUGIN(auth_ldap::init, NULL, auth_ldap::init_options);
