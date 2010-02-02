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

#ifndef PLUGIN_DATA_ENGINE_STATUS_H
#define PLUGIN_DATA_ENGINE_STATUS_H


class StateTool : public Tool
{
  sql_var_t option_type;

public:

  StateTool(const char *arg, bool global);

  virtual drizzle_show_var *getVariables()= 0;

  virtual bool hasStatus()
  {
    return true;
  }

  class Generator : public Tool::Generator 
  {
    sql_var_t option_type;
    bool has_status;
    drizzle_show_var *variables;
    system_status_var status;
    system_status_var *status_ptr;

    void fill(const char *name, char *value, SHOW_TYPE show_type);

    system_status_var *getStatus()
    {
      return status_ptr;
    }

  public:
    Generator(Field **arg, sql_var_t option_arg,
              drizzle_show_var *show_arg,
              bool status_arg);
    ~Generator();

    bool populate();

  };

  Generator *generator(Field **arg)
  {
    return new Generator(arg, option_type, getVariables(), hasStatus());
  }
};

class StatusTool : public StateTool
{
public:
  StatusTool(bool global) :
    StateTool(global ? "GLOBAL_STATUS" : "SESSION_STATUS", global)
  { }

  drizzle_show_var *getVariables()
  {
    return getFrontOfStatusVars();
  }
};


class StatementsTool : public StateTool
{
public:
  StatementsTool(bool global) :
    StateTool(global ? "GLOBAL_STATEMENTS" : "SESSION_STATEMENTS", global)
    { }

  drizzle_show_var *getVariables()
  {
    return getCommandStatusVars();
  }
};
#endif // PLUGIN_DATA_ENGINE_STATUS_H
