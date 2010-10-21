/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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


#ifndef PLUGIN_UTILITY_FUNCTIONS_USER_H
#define PLUGIN_UTILITY_FUNCTIONS_USER_H

#include <drizzled/function/str/strfunc.h>

namespace drizzled
{

namespace utility_functions
{

class User :public Item_str_func
{
protected:
  bool init (const char *user, const char *host);

public:
  User()
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

} /* namespace utility_functions */
} /* namespace drizzled */

#endif /* PLUGIN_UTILITY_FUNCTIONS_USER_H */
