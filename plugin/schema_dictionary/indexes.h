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

class IndexesTool : public TablesTool
{
public:
  IndexesTool();

  IndexesTool(const char *schema_arg, const char *table_arg) :
    TablesTool(schema_arg, table_arg)
  { }

  IndexesTool(const char *table_arg) :
    TablesTool(table_arg)
  { }

  class Generator : public TablesTool::Generator 
  {
    int32_t index_iterator;
    bool is_index_primed;
    drizzled::message::Table::Index index;

    bool nextIndexCore();
    virtual void fill();

  public:
    Generator(drizzled::Field **arg);

    const drizzled::message::Table::Index& getIndex()
    {
      return index;
    }

    bool isIndexesPrimed()
    {
      return is_index_primed;
    }

    bool nextIndex();

    bool populate();
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

