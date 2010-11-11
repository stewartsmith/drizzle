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

#include "config.h"

#include "drizzled/statement/execute.h"
#include "drizzled/session.h"
#include "drizzled/user_var_entry.h"
#include "drizzled/plugin/listen.h"
#include "drizzled/plugin/client.h"

namespace drizzled
{

void mysql_parse(drizzled::Session *session, const char *inBuf, uint32_t length);

namespace statement
{

Execute::Execute(Session *in_session, drizzled::execute_string_t to_execute_arg, bool is_quiet_arg) :
  Statement(in_session),
  is_quiet(is_quiet_arg),
  to_execute(to_execute_arg)
{
}
  

bool statement::Execute::parseVariable()
{
  if (to_execute.isVariable())
  {
    user_var_entry *var= getSession()->getVariable(to_execute, false);

    if (var && var->length && var->value && var->type == STRING_RESULT)
    {
      LEX_STRING tmp_for_var;
      tmp_for_var.str= var->value; 
      tmp_for_var.length= var->length; 
      to_execute= tmp_for_var;
      return true;
    }
  }

  return false;
}

bool statement::Execute::execute()
{
  if (to_execute.length == 0)
  {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "Invalid Variable");
    return false;
  }
  if (to_execute.isVariable())
  {
    if (not parseVariable())
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "Invalid Variable");
      return false;
    }
  }

  if (is_quiet)
  {
    plugin::Client *temp= getSession()->getClient();
    plugin::Client *null_client= plugin::Listen::getNullClient();

    getSession()->setClient(null_client);

    mysql_parse(getSession(), to_execute.str, to_execute.length);

    getSession()->setClient(temp);
    if (getSession()->is_error())
    {
      getSession()->clear_error(true);
      getSession()->my_ok();
    }
    null_client->close();
    delete null_client;
  }
  else
  {
    mysql_parse(getSession(), to_execute.str, to_execute.length);
  }


  // We have to restore ourselves at the top for delete() to work.
  getSession()->getLex()->statement= this;

  return true;
}

} /* namespace statement */
} /* namespace drizzled */

