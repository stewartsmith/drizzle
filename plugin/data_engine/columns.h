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

#ifndef PLUGIN_DATA_ENGINE_COLUMNS_H
#define PLUGIN_DATA_ENGINE_COLUMNS_H


class ColumnsTool : public Tool
{
public:

  ColumnsTool();

  class Generator : public Tool::Generator 
  {
    std::set<std::string> schema_names;
    std::set<std::string> table_names;
    std::set<std::string>::iterator schema_iterator;
    std::set<std::string>::iterator table_iterator;
    uint32_t schema_counter;
    int32_t column_iterator;
    drizzled::message::Table table_proto;

    void fetch_proto(void);
    void fetch_tables();

  public:
    Generator();

    const std::string &schema_name()
    {
      return (*schema_iterator);
    }

    const std::string &table_name()
    {
      return (*table_iterator);
    }

    bool populate(Field **fields);
    virtual void fill(Field **fields, const drizzled::message::Table::Field &column);
  };

  Generator *generator()
  {
    return new Generator;
  }
};

#endif // PLUGIN_DATA_ENGINE_COLUMNS_H
