/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <plugin/session_dictionary/dictionary.h>
#include <drizzled/user_var_entry.h>

#define LARGEST_USER_VARIABLE_NAME 128

namespace session_dictionary {

Variables::Variables() :
  drizzled::plugin::TableFunction("DATA_DICTIONARY", "USER_DEFINED_VARIABLES")
{
  add_field("VARIABLE_NAME", drizzled::plugin::TableFunction::STRING, LARGEST_USER_VARIABLE_NAME, false);
  add_field("VARIABLE_VALUE", drizzled::plugin::TableFunction::STRING, LARGEST_USER_VARIABLE_NAME, true);
}

Variables::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg),
  user_vars(getSession().getUserVariables()),
  iter(user_vars.begin())
{
}

Variables::Generator::~Generator()
{
}

bool Variables::Generator::populate()
{
  while (iter != user_vars.end())
  {
    char buff[LARGEST_USER_VARIABLE_NAME];
    drizzled::String tmp(buff, sizeof(buff), drizzled::system_charset_info);

    bool null_value;
    uint32_t decimals= 4; // arbitrary
    iter->second->val_str(&null_value, &tmp, decimals);


    // VARIABLE_NAME
    push(iter->first);
    
    // VARIABLE_VALUE 
    if (null_value)
      push();
    else
      push(tmp.c_str());

    iter++;

    return true;
  }

  return false;
}

} /* namespace session_dictionary */
