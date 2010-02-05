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

#ifndef PLUGIN_DATA_ENGINE_TABLES_H
#define PLUGIN_DATA_ENGINE_TABLES_H

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
    Generator(Field **arg);

    void pushRow(drizzled::message::Table::TableOptions::RowType type);

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
    bool checkTableName();
  };

  Generator *generator(Field **arg)
  {
    return new Generator(arg);
  }

};

class TableNames : public TablesTool
{
public:
  TableNames(const char *table_arg) :
    TablesTool(table_arg)
  { }

  TableNames() :
    TablesTool("LOCAL_TABLE_NAMES")
  {
    add_field("TABLE_NAME");
  }

  class Generator : public TablesTool::Generator 
  {
    void fill()
    {
      /* TABLE_NAME */
      push(table_name());
    }

    bool checkSchema();

  public:
    Generator(Field **arg) :
      TablesTool::Generator(arg)
    { }
  };

  Generator *generator(Field **arg)
  {
    return new Generator(arg);
  }
};

class TableStatus : public TableNames
{
public:
  TableStatus() :
    TableNames("LOCAL_TABLE_STATUS")
  {
    add_field("Name");
    add_field("Engine");
    add_field("Version");
    add_field("Row_format");
    add_field("Rows");
    add_field("Avg_row_length");
    add_field("Data_length");
    add_field("Max_data_length");
    add_field("Index_length");
    add_field("Data_free");
    add_field("Auto_increment");
    add_field("Create_time");
    add_field("Update_time");
    add_field("Check_time");
    add_field("Collation");
    add_field("Checksum");
    add_field("Create_options");
    add_field("Comment");
  }

  class Generator : public TableNames::Generator 
  {
    void fill()
    {
      /* Name */
      push(table_name());

      /* Engine */
      push(getTableProto().engine().name());

      /* Version */
      push(0);

      /* Row_format */
      pushRow(getTableProto().options().row_type());

      /* Rows */
      push(0);

      /* Avg_row_length */
      push(0);

      /* Data_length */
      push(0);

      /* Max_data_length */
      push(0);

      /* Index_length */
      push(0);

      /* Data_free */
      push(0);

      /* Auto_increment */
      push(0);

      /* Create_time */
      push(0);

      /* Update_time */
      push(0);

      /* Check_time */
      push(0);

      /* Collation */
      push(getTableProto().options().collation());

      /* Checksum */
      push(0);

      /* Create_options */
      push("");

      /* Comment */
      push(getTableProto().options().comment());
    }

  public:
    Generator(Field **arg) :
      TableNames::Generator(arg)
    { }
  };

  Generator *generator(Field **arg)
  {
    return new Generator(arg);
  }
};

#endif // PLUGIN_DATA_ENGINE_TABLES_H
