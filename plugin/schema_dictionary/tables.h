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

#ifndef PLUGIN_SCHEMA_DICTIONARY_TABLES_H
#define PLUGIN_SCHEMA_DICTIONARY_TABLES_H

class TablesTool : public SchemasTool
{
public:

  TablesTool();

  TablesTool(const char *schema_arg, const char *table_arg) :
    SchemasTool(schema_arg, table_arg)
  { }

  TablesTool(const char *table_arg) :
    SchemasTool(table_arg)
  { }

  class Generator : public SchemasTool::Generator 
  {
    drizzled::message::Table table_proto;
    std::set<std::string> table_names;
    std::set<std::string>::iterator table_iterator;
    bool is_tables_primed;

    virtual void fill();
    bool nextTableCore();

  public:
    Generator(drizzled::Field **arg);

    void pushType(drizzled::message::Table::Field::FieldType type);

    const std::string &table_name()
    {
      return (*table_iterator);
    }

    const drizzled::message::Table& getTableProto()
    {
      return table_proto;
    }

    bool isTablesPrimed()
    {
      return is_tables_primed;
    }

    bool populate();
    bool nextTable();
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }

};

#endif /* PLUGIN_SCHEMA_DICTIONARY_TABLES_H */
