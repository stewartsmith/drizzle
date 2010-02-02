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

#include <plugin/data_engine/function.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/definitions.h>

#include <vector>
#include <string>
#include <sstream>

using namespace std;
using namespace drizzled;

StatusTool::StatusTool(const char *arg, bool scope_arg) :
  Tool(arg),
  scope(scope_arg)
{
  add_field("VARIABLE_NAME");
  add_field("VARIABLE_VALUE", 16300);
}

StatusTool::Generator::Generator(Field **arg, bool scope_arg, drizzle_show_var *variables_args) :
  Tool::Generator(arg),
  scope(scope_arg),
  variables(variables_args)
{
  pthread_mutex_lock(&LOCK_status);
  if (scope)
  {
    calc_sum_of_all_status(getStatus());
    pthread_mutex_lock(&LOCK_global_system_variables);
  }
}

StatusTool::Generator::~Generator()
{
  if (scope)
    pthread_mutex_unlock(&LOCK_global_system_variables);
  pthread_mutex_unlock(&LOCK_status);
}

bool StatusTool::Generator::populate()
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

    if (var->type != SHOW_ARRAY)
    {
      fill(variables->name, var->value, var->type);
    }

    variables++;

    return true;
  }

  return false;
}
void StatusTool::Generator::fill(const char *name, char *value, SHOW_TYPE show_type)
{
  MY_ALIGNED_BYTE_ARRAY(buff_data, SHOW_VAR_FUNC_BUFF_SIZE, int64_t);
  char * const buff= (char *) &buff_data;
  Session *session= current_session;
  const char *pos, *end;                  // We assign a lot of const's
  enum enum_var_type option_type= OPT_GLOBAL;
  struct system_status_var *status_var;

  /* Scope represents if the status should be session or global */
  status_var= getStatus();

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
    end= buff + my_fcvt(*(double *) value, 6, buff, NULL);
    break;
  case SHOW_LONG_STATUS:
    value= ((char *) status_var + (ulong) value);
    /* fall through */
  case SHOW_LONG:
    end= int10_to_str(*(long*) value, buff, 10);
    break;
  case SHOW_LONGLONG_STATUS:
    value= ((char *) status_var + (uint64_t) value);
    /* fall through */
  case SHOW_LONGLONG:
    end= int64_t10_to_str(*(int64_t*) value, buff, 10);
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
    end= int64_t10_to_str((int64_t) *(ha_rows*) value, buff, 10);
    break;
  case SHOW_BOOL:
    end+= sprintf(buff,"%s", *(bool*) value ? "ON" : "OFF");
    break;
  case SHOW_MY_BOOL:
    end+= sprintf(buff,"%s", *(bool*) value ? "ON" : "OFF");
    break;
  case SHOW_INT:
  case SHOW_INT_NOFLUSH: // the difference lies in refresh_status()
    end= int10_to_str((long) *(uint32_t*) value, buff, 10);
    break;
  case SHOW_HAVE:
    {
      SHOW_COMP_OPTION tmp_option= *(SHOW_COMP_OPTION *)value;
      pos= show_comp_option_name[(int) tmp_option];
      end= strchr(pos, '\0');
      break;
    }
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
  push(name);
  push(pos, (uint32_t) (end - pos));
}
