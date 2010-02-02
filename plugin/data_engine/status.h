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


class StatusTool : public Tool
{
  bool scope;

public:

  StatusTool(const char *arg, bool scope_arg);

  virtual drizzle_show_var *getVariables()= 0;

  class Generator : public Tool::Generator 
  {
    bool scope;
    drizzle_show_var *variables;
    system_status_var status;

    void fill(const char *name, char *value, SHOW_TYPE show_type);
    system_status_var *getStatus()
    {
      Session *session= current_session;
      return scope ? &status :  &session->status_var;
    }


  public:
    Generator(Field **arg, bool scope_arg, drizzle_show_var *);
    ~Generator();

    bool populate();

  };

  Generator *generator(Field **arg)
  {
    return new Generator(arg, scope, getVariables());
  }
};

class GlobalStatusTool : public StatusTool
{
public:
  GlobalStatusTool() :
    StatusTool("GLOBAL_STATUS", true)
  { }

  drizzle_show_var *getVariables()
  {
    return getFrontOfStatusVars();
  }
};


class SessionStatusTool : public StatusTool
{
public:
  SessionStatusTool() :
    StatusTool("SESSION_STATUS", false)
  { }

  drizzle_show_var *getVariables()
  {
    return getFrontOfStatusVars();
  }
};


class GlobalStatementsTool : public StatusTool
{
public:
  GlobalStatementsTool() :
    StatusTool("GLOBAL_STATEMENTS", true)
  { }

  drizzle_show_var *getVariables()
  {
    return getCommandStatusVars();
  }
};


class SessionStatementsTool : public StatusTool
{
public:
  SessionStatementsTool() :
    StatusTool("SESSION_STATEMENTS", false)
  { }

  drizzle_show_var *getVariables()
  {
    return getCommandStatusVars();
  }
};

#endif // PLUGIN_DATA_ENGINE_STATUS_H
