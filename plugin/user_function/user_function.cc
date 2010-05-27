/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *  Copyright (C) 2010 Stewart Smith
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

#include <drizzled/plugin/function.h>
#include <drizzled/session.h>

using namespace std;
using namespace drizzled;

#include <drizzled/function/str/strfunc.h>

class UserFunction :public Item_str_func
{
protected:
  bool init (const char *user, const char *host);

public:
  UserFunction()
  {
    str_value.set("", 0, system_charset_info);
  }
  String *val_str(String *)
  {
    assert(fixed == 1);
    return (null_value ? 0 : &str_value);
  }
  bool fix_fields(Session *session, Item **ref);
  void fix_length_and_dec()
  {
    max_length= (USERNAME_CHAR_LENGTH + HOSTNAME_LENGTH + 1) *
                system_charset_info->mbmaxlen;
  }
  const char *func_name() const { return "user"; }
  const char *fully_qualified_func_name() const { return "user()"; }
  int save_in_field(Field *field,
                    bool )
  {
    return save_str_value_in_field(field, &str_value);
  }
};

/**
  @todo
  make USER() replicate properly (currently it is replicated to "")
*/
bool UserFunction::init(const char *user, const char *host)
{
  assert(fixed == 1);

  // For system threads (e.g. replication SQL thread) user may be empty
  if (user)
  {
    const CHARSET_INFO * const cs= str_value.charset();
    uint32_t res_length= (strlen(user)+strlen(host)+2) * cs->mbmaxlen;

    if (str_value.alloc(res_length))
    {
      null_value=1;
      return true;
    }

    res_length=cs->cset->snprintf(cs, (char*)str_value.ptr(), res_length,
                                  "%s@%s", user, host);
    str_value.length(res_length);
    str_value.mark_as_const();
  }
  return false;
}


bool UserFunction::fix_fields(Session *session, Item **ref)
{
  return (Item_str_func::fix_fields(session, ref) ||
          init(session->getSecurityContext().getUser().c_str(),
               session->getSecurityContext().getIp().c_str()));
}


plugin::Create_function<UserFunction> *user_function= NULL;

static int initialize(drizzled::module::Context &context)
{
  user_function= new plugin::Create_function<UserFunction>("user");
  context.add(user_function);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "user_function",
  "1.0",
  "Stewart Smith",
  "USER() and CURRENT_USER()",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
