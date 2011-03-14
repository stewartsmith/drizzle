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

#pragma once

class StateTool : public drizzled::plugin::TableFunction
{
  drizzled::sql_var_t option_type;

public:

  StateTool(const char *arg, bool global);

  virtual drizzled::drizzle_show_var *getVariables()= 0;

  class Generator : public drizzled::plugin::TableFunction::Generator 
  {
    drizzled::sql_var_t option_type;
    drizzled::drizzle_show_var *variables;

    void fill(const std::string &name, char *value, drizzled::SHOW_TYPE show_type);

  public:
    Generator(drizzled::Field **arg, drizzled::sql_var_t option_arg,
              drizzled::drizzle_show_var *show_arg);
    ~Generator();

    bool populate();

  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, option_type, getVariables());
  }
};

