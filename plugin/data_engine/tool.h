/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef PLUGIN_DATA_ENGINE_TOOL_H
#define PLUGIN_DATA_ENGINE_TOOL_H

class Tool
{
  drizzled::message::Table proto;
  drizzled::TableIdentifier identifier;

  void setName(); // init name

  void init()
  {
    drizzled::message::Table::StorageEngine *engine;
    drizzled::message::Table::TableOptions *table_options;

    proto.set_name(identifier.getTableName());
    proto.set_type(drizzled::message::Table::FUNCTION);

    table_options= proto.mutable_options();
    table_options->set_collation_id(default_charset_info->number);
    table_options->set_collation(default_charset_info->name);

    engine= proto.mutable_engine();
    engine->set_name(engine_name);
  }

public:
  Tool(const char *schema_arg, const char *table_arg) :
    identifier(schema_arg, table_arg)
  {
    init();
  }

  Tool(const char *table_arg) :
    identifier("data_dictionary", table_arg)
  {
    init();
  }

  enum ColumnType {
    BOOLEAN,
    NUMBER,
    STRING
  };

  virtual ~Tool() {}

  class Generator 
  {
    Field **columns;
    Field **columns_iterator;

  public:
    const CHARSET_INFO *scs;

    Generator(Field **arg) :
      columns(arg)
    {
      scs= system_charset_info;
    }

    virtual ~Generator()
    { }

    /*
      Return type is bool meaning "are there more rows".
    */
    bool sub_populate()
    {
      columns_iterator= columns;
      return populate();
    }

    virtual bool populate()
    {
      return false;
    }

    void push(uint64_t arg)
    {
      (*columns_iterator)->store(static_cast<int64_t>(arg), false);
      columns_iterator++;
    }

    void push(uint32_t arg)
    {
      (*columns_iterator)->store(static_cast<int64_t>(arg), false);
      columns_iterator++;
    }

    void push(int64_t arg)
    {
      (*columns_iterator)->store(arg, false);
      columns_iterator++;
    }

    void push(int32_t arg)
    {
      (*columns_iterator)->store(arg, false);
      columns_iterator++;
    }

    void push(const char *arg, uint32_t length= 0)
    {
      assert(columns_iterator);
      assert(*columns_iterator);
      assert(arg);
      (*columns_iterator)->store(arg, length ? length : strlen(arg), scs);
      columns_iterator++;
    }

    void push(const std::string& arg)
    {
      (*columns_iterator)->store(arg.c_str(), arg.length(), scs);
      columns_iterator++;
    }

    void push(bool arg)
    {
      if (arg)
      {
        (*columns_iterator)->store("TRUE", 4, scs);
      }
      else
      {
        (*columns_iterator)->store("FALSE", 5, scs);
      }

      columns_iterator++;
    }

  };

  void define(drizzled::message::Table &arg)
  { 
    arg.CopyFrom(proto);
  }

  const char *getName()
  { 
    return identifier.getTableName();
  }

  const char *getSchemaHome()
  { 
    return identifier.getSchemaName();
  }

  const char *getPath()
  { 
    return identifier.getPath();
  }

  virtual Generator *generator(Field **arg)
  {
    return new Generator(arg);
  }

  void add_field(const char *label,
                 drizzled::message::Table::Field::FieldType type,
                 uint32_t length= 0);

  void add_field(const char *label,
                 uint32_t field_length= 64);

  void add_field(const char *label,
                 Tool::ColumnType type,
                 bool is_default_null= false);

  void add_field(const char *label,
                 Tool::ColumnType type,
                 uint32_t field_length,
                 bool is_default_null= false);
};

#endif // PLUGIN_DATA_ENGINE_TOOL_H
