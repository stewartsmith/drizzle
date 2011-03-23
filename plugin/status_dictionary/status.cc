/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems, Inc.
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

#include <plugin/status_dictionary/dictionary.h>

#include <drizzled/pthread_globals.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/definitions.h>
#include <drizzled/status_helper.h>
#include <drizzled/sql_lex.h>
#include <drizzled/catalog/instance.h>

#include <vector>
#include <string>
#include <sstream>

using namespace std;
using namespace drizzled;

StateTool::StateTool(const char *arg, bool global) :
  plugin::TableFunction("DATA_DICTIONARY", arg),
  option_type(global ? OPT_GLOBAL : OPT_SESSION)
{
  add_field("VARIABLE_NAME");
  add_field("VARIABLE_VALUE", 1024);
}

StateTool::Generator::Generator(Field **arg, sql_var_t option_arg,
                                drizzle_show_var *variables_args)
                                :
  plugin::TableFunction::Generator(arg),
  option_type(option_arg),
  variables(variables_args)
{
}

StateTool::Generator::~Generator()
{
}

bool StateTool::Generator::populate()
{
  while (variables && variables->name)
  {
    drizzle_show_var *var;
    MY_ALIGNED_BYTE_ARRAY(buff_data, SHOW_VAR_FUNC_BUFF_SIZE, int64_t);
    char * const buff= (char *) &buff_data;

    /*
      if var->type is SHOW_FUNC, call the function.
      Repeat as necessary, if new var is again SHOW_FUNC
    */
    {
      drizzle_show_var tmp;

      for (var= variables; var->type == SHOW_FUNC; var= &tmp)
        ((drizzle_show_var_func)((st_show_var_func_container *)var->value)->func)(&tmp, buff);
    }

    if (isWild(variables->name))
    {
      variables++;
      continue;
    }

    fill(variables->name, var->value, var->type);

    variables++;

    return true;
  }

  return false;
}

void StateTool::Generator::fill(const std::string &name, char *value, SHOW_TYPE show_type)
{
  std::ostringstream oss;
  std::string return_value;

  {
    boost::mutex::scoped_lock scopedLock(getSession().catalog().systemVariableLock());

    if (show_type == SHOW_SYS)
    {
      show_type= ((sys_var*) value)->show_type();
      value= (char*) ((sys_var*) value)->value_ptr(&(getSession()), option_type,
                                                   &null_lex_str);
    }

    return_value= StatusHelper::fillHelper(NULL, value, show_type); 
  }
  push(name);

  if (return_value.length())
    push(return_value);
  else 
    push(" ");
}
