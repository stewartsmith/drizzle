/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Rackspace
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

#include <string>

#include <drizzled/plugin/authentication.h>
#include <drizzled/identifier.h>
#include <drizzled/util/convert.h>
#include <drizzled/algorithm/sha1.h>

using namespace std;
using namespace drizzled;

namespace auth_test
{

/* This is the result of MYSQL_PASSWORD("scramble_password"). */
static const char *scrambled_password= "2C5A870CEFF02BA3B0A927D7956B3FEB4D59CF21";

class AuthTest: public plugin::Authentication
{
public:
  AuthTest(string name_arg):
    plugin::Authentication(name_arg)
  { }

  virtual bool authenticate(const identifier::User &sctx, const string &password)
  {
    /* The "root" user always succeeds for drizzletest to get in. */
    if (sctx.username() == "root" && password.empty())
      return true;

    /* Any password succeeds. */
    if (sctx.username() == "password_ok" && !password.empty())
      return true;

    /* No password succeeds. */
    if (sctx.username() == "no_password_ok" && password.empty())
      return true;

    /* Check if MySQL password scramble succeeds. */
    if (sctx.username() == "scramble_ok" &&
        sctx.getPasswordType() == identifier::User::MYSQL_HASH &&
        sctx.getPasswordContext().size() == SHA1_DIGEST_LENGTH &&
        password.size() == SHA1_DIGEST_LENGTH)
    {
      SHA1_CTX ctx;
      uint8_t scrambled_password_hash[SHA1_DIGEST_LENGTH];
      uint8_t temp_hash[SHA1_DIGEST_LENGTH];
      uint8_t scrambled_password_check[SHA1_DIGEST_LENGTH];

      /* Get the double-hashed password from the stored hex string. */
      drizzled_hex_to_string(reinterpret_cast<char*>(scrambled_password_hash),
                             scrambled_password, SHA1_DIGEST_LENGTH * 2);

      /* Hash the scramble that was sent to client with the stored password. */
      SHA1Init(&ctx);
      SHA1Update(&ctx, reinterpret_cast<const uint8_t*>(sctx.getPasswordContext().c_str()), SHA1_DIGEST_LENGTH);
      SHA1Update(&ctx, scrambled_password_hash, SHA1_DIGEST_LENGTH);
      SHA1Final(temp_hash, &ctx);

      /* Next, XOR the result with what the client sent to get the original
         single-hashed password. */
      for (int x= 0; x < SHA1_DIGEST_LENGTH; x++)
        temp_hash[x]= temp_hash[x] ^ password[x];

      /* Hash this result once more to get the double-hashed password again. */
      SHA1Init(&ctx);
      SHA1Update(&ctx, temp_hash, SHA1_DIGEST_LENGTH);
      SHA1Final(scrambled_password_check, &ctx);

      /* These should match for a successful auth. */
      return memcmp(scrambled_password_hash, scrambled_password_check, SHA1_DIGEST_LENGTH) == 0;
    }

    return false;
  }
};

AuthTest *auth_test= NULL;

static int init(module::Context &context)
{
  auth_test= new AuthTest("auth_test");
  context.add(auth_test);
  return 0;
}

} /* namespace auth_test */

DRIZZLE_PLUGIN(auth_test::init, NULL, NULL);
