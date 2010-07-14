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

/* This is needed for simple auth, we're not ready for SASL yet. */
#define LDAP_DEPRECATED 1

#include <ldap.h>
#include <pthread.h>
#include <sys/time.h>
#include <map>
#include <string>

#include "drizzled/plugin/authentication.h"
#include "drizzled/security_context.h"
#include "drizzled/util/convert.h"
#include "drizzled/algorithm/sha1.h"

using namespace std;
using namespace drizzled;

namespace auth_ldap
{

static char *uri= NULL;
static const char DEFAULT_URI[]= "ldap://127.0.0.1/";
static char *bind_dn= NULL;
static char *bind_password= NULL;
static char *base_dn= NULL;
static char *password_attribute= NULL;
static const char DEFAULT_PASSWORD_ATTRIBUTE[]= "userPassword";
static char *mysql_password_attribute= NULL;
static const char DEFAULT_MYSQL_PASSWORD_ATTRIBUTE[]= "mysqlUserPassword";
static int cache_timeout= 0;
static const int DEFAULT_CACHE_TIMEOUT= 600;

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

  typedef pair<PasswordType, string> PasswordEntry;
  typedef pair<string, PasswordEntry> UserEntry;
  typedef map<string, PasswordEntry> UserCache;

  /**
   * Base class method to check authentication for a user.
   */
  bool authenticate(const SecurityContext &sctx, const string &password);

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
  int return_code= ldap_initialize(&ldap, uri);
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

  if (bind_dn != NULL)
  {
    return_code= ldap_simple_bind_s(ldap, bind_dn, bind_password);
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

bool AuthLDAP::authenticate(const SecurityContext &sctx, const string &password)
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

  AuthLDAP::UserCache::const_iterator user= users.find(sctx.getUser());
  if (user == users.end())
  {
    pthread_rwlock_unlock(&lock);

    pthread_rwlock_wrlock(&lock);

    /* Make sure the user was not added while we unlocked. */
    user= users.find(sctx.getUser());
    if (user == users.end())
      lookupUser(sctx.getUser());

    pthread_rwlock_unlock(&lock);

    pthread_rwlock_rdlock(&lock);

    /* Get user again because map may have changed while unlocked. */
    user= users.find(sctx.getUser());
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

  if (sctx.getPasswordType() == SecurityContext::MYSQL_HASH)
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
    password_attribute,
    mysql_password_attribute,
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
        errmsg_printf(ERRMSG_LVL_ERROR, _("Reconnect failed: %s\n"),
                      getError().c_str());
        return;
      }
    }

    int return_code= ldap_search_ext_s(ldap,
                                       base_dn,
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
      errmsg_printf(ERRMSG_LVL_ERROR, _("ldap_search_ext_s failed: %s\n"),
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
    char **values= ldap_get_values(ldap, entry, mysql_password_attribute);
    if (values == NULL)
    {
      values= ldap_get_values(ldap, entry, password_attribute);
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
    errmsg_printf(ERRMSG_LVL_ERROR, _("Could not load auth ldap: %s\n"),
                  auth_ldap->getError().c_str());
    delete auth_ldap;
    return 1;
  }

  context.add(auth_ldap);
  return 0;
}

static DRIZZLE_SYSVAR_STR(uri,
                          uri,
                          PLUGIN_VAR_READONLY,
                          N_("URI of the LDAP server to contact"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          DEFAULT_URI);

static DRIZZLE_SYSVAR_STR(bind_dn,
                          bind_dn,
                          PLUGIN_VAR_READONLY,
                          N_("DN to use when binding to the LDAP server"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          NULL); /* default value */

static DRIZZLE_SYSVAR_STR(bind_password,
                          bind_password,
                          PLUGIN_VAR_READONLY,
                          N_("Password to use when binding the DN"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          NULL); /* default value */

static DRIZZLE_SYSVAR_STR(base_dn,
                          base_dn,
                          PLUGIN_VAR_READONLY,
                          N_("DN to use when searching"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          NULL); /* default value */

static DRIZZLE_SYSVAR_STR(password_attribute,
                          password_attribute,
                          PLUGIN_VAR_READONLY,
                          N_("Attribute in LDAP with plain text password"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          DEFAULT_PASSWORD_ATTRIBUTE);

static DRIZZLE_SYSVAR_STR(mysql_password_attribute,
                          mysql_password_attribute,
                          PLUGIN_VAR_READONLY,
                          N_("Attribute in LDAP with MySQL hashed password"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          DEFAULT_MYSQL_PASSWORD_ATTRIBUTE);

static DRIZZLE_SYSVAR_INT(cache_timeout,
                          cache_timeout,
                          PLUGIN_VAR_READONLY,
                          N_("How often to empty the users cache, 0 to disable"),
                          NULL, /* check func */
                          NULL, /* update func */
                          DEFAULT_CACHE_TIMEOUT,
                          0,
                          2147483647,
                          0);

static drizzle_sys_var* sys_variables[]=
{
  DRIZZLE_SYSVAR(uri),
  DRIZZLE_SYSVAR(bind_dn),
  DRIZZLE_SYSVAR(bind_password),
  DRIZZLE_SYSVAR(base_dn),
  DRIZZLE_SYSVAR(password_attribute),
  DRIZZLE_SYSVAR(mysql_password_attribute),
  DRIZZLE_SYSVAR(cache_timeout),
  NULL
};

} /* namespace auth_ldap */

DRIZZLE_PLUGIN(auth_ldap::init, auth_ldap::sys_variables, NULL);
