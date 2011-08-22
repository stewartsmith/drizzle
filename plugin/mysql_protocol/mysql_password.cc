/* Copyright (C) 2010 Rackspace

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>
#include "mysql_password.h"
#include <drizzled/algorithm/sha1.h>
#include <drizzled/util/convert.h>

using namespace std;

namespace drizzle_plugin {

const char* MySQLPasswordName = "mysql_password";

const char *MySQLPassword::func_name() const
{
  return MySQLPasswordName;
}
  
void MySQLPassword::fix_length_and_dec()
{
  max_length= args[0]->max_length;
}

bool MySQLPassword::check_argument_count(int n)
{
  return n == 1;
}

drizzled::String *MySQLPassword::val_str(drizzled::String *str)
{
  uint8_t hash_tmp1[SHA1_DIGEST_LENGTH];
  uint8_t hash_tmp2[SHA1_DIGEST_LENGTH];

  drizzled::String argument;
  drizzled::do_sha1(*args[0]->val_str(&argument), hash_tmp1);
  drizzled::do_sha1(data_ref(hash_tmp1, SHA1_DIGEST_LENGTH), hash_tmp2);

  str->realloc(SHA1_DIGEST_LENGTH * 2);
  drizzled::drizzled_string_to_hex(str->ptr(), reinterpret_cast<const char*>(hash_tmp2), SHA1_DIGEST_LENGTH);
  str->length(SHA1_DIGEST_LENGTH * 2);

  return str;
}

} /* namespace drizzle_plugin */
