/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems
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

#include "plugin/status_dictionary/dictionary.h"

#include <drizzled/pthread_globals.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/definitions.h>

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
  add_field("VARIABLE_VALUE", 16300);
}

StateTool::Generator::Generator(Field **arg, sql_var_t option_arg,
                                drizzle_show_var *variables_args,
                                bool status_arg) :
  plugin::TableFunction::Generator(arg),
  option_type(option_arg),
  has_status(status_arg),
  variables(variables_args)
{
  if (not has_status)
  {
    status_ptr= NULL;
    pthread_rwlock_rdlock(&LOCK_system_variables_hash);
  }
  else if (option_type == OPT_GLOBAL  && has_status)
  {
    status_ptr= &status;
    pthread_mutex_lock(&LOCK_status);
    calc_sum_of_all_status(&status);
  }
  else
  {
    Session *session= current_session;
    status_ptr= &session->status_var;
  }
}

StateTool::Generator::~Generator()
{
  if (not has_status)
  {
    pthread_rwlock_unlock(&LOCK_system_variables_hash);
  }
  else if (option_type == OPT_GLOBAL)
  {
    pthread_mutex_unlock(&LOCK_status);
  }
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
        ((mysql_show_var_func)((st_show_var_func_container *)var->value)->func)(&tmp, buff);
    }

    if (isWild(variables->name))
    {
      variables++;
      continue;
    }
    else if (var->type != SHOW_ARRAY)
    {
      fill(variables->name, var->value, var->type);
    }
    else
    {
      variables++;
      continue;
    }

    variables++;

    return true;
  }

  return false;
}


void StateTool::Generator::fill(const char *name, char *value, SHOW_TYPE show_type)
{
  MY_ALIGNED_BYTE_ARRAY(buff_data, SHOW_VAR_FUNC_BUFF_SIZE, int64_t);
  char * const buff= (char *) &buff_data;
  Session *session= current_session;
  const char *pos, *end;                  // We assign a lot of const's
  struct system_status_var *status_var;

  /* Scope represents if the status should be session or global */
  status_var= getStatus();

  pthread_mutex_lock(&LOCK_global_system_variables);

  if (show_type == SHOW_SYS)
  {
    show_type= ((sys_var*) value)->show_type();
    value= (char*) ((sys_var*) value)->value_ptr(session, option_type,
                                                 &null_lex_str);
  }

  pos= end= buff;
  /*
    note that value may be == buff. All SHOW_xxx code below
    should still work in this case
  */
  switch (show_type) {
  case SHOW_DOUBLE_STATUS:
    value= ((char *) status_var + (ulong) value);
    /* fall through */
  case SHOW_DOUBLE:
    /* 6 is the default precision for '%f' in sprintf() */
    end= buff + internal::my_fcvt(*(double *) value, 6, buff, NULL);
    break;
  case SHOW_LONG_STATUS:
    value= ((char *) status_var + (ulong) value);
    /* fall through */
  case SHOW_LONG:
    end= internal::int10_to_str(*(long*) value, buff, 10);
    break;
  case SHOW_LONGLONG_STATUS:
    value= ((char *) status_var + (uint64_t) value);
    /* fall through */
  case SHOW_LONGLONG:
    end= internal::int64_t10_to_str(*(int64_t*) value, buff, 10);
    break;
  case SHOW_SIZE:
    {
      stringstream ss (stringstream::in);
      ss << *(size_t*) value;

      string str= ss.str();
      strncpy(buff, str.c_str(), str.length());
      end= buff+ str.length();
    }
    break;
  case SHOW_HA_ROWS:
    end= internal::int64_t10_to_str((int64_t) *(ha_rows*) value, buff, 10);
    break;
  case SHOW_BOOL:
    end+= sprintf(buff,"%s", *(bool*) value ? "ON" : "OFF");
    break;
  case SHOW_MY_BOOL:
    end+= sprintf(buff,"%s", *(bool*) value ? "ON" : "OFF");
    break;
  case SHOW_INT:
  case SHOW_INT_NOFLUSH: // the difference lies in refresh_status()
    end= internal::int10_to_str((long) *(uint32_t*) value, buff, 10);
    break;
  case SHOW_CHAR:
    {
      if (!(pos= value))
        pos= "";
      end= strchr(pos, '\0');
      break;
    }
  case SHOW_CHAR_PTR:
    {
      if (!(pos= *(char**) value))
        pos= "";
      end= strchr(pos, '\0');
      break;
    }
  case SHOW_KEY_CACHE_LONG:
  case SHOW_KEY_CACHE_LONGLONG:
    pos= "not supported";
    end= pos + sizeof("not supported");
  case SHOW_UNDEF:
    break;                                        // Return empty string
  case SHOW_SYS:                                  // Cannot happen
  default:
    assert(0);
    break;
  }
  pthread_mutex_unlock(&LOCK_global_system_variables);
  push(name);
  if (end - pos)
    push(pos, (uint32_t) (end - pos));
  else 
    push();
}
