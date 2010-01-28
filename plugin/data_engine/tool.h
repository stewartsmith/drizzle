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
  Tool() {};

  drizzled::message::Table schema;
  std::string name;
  std::string path;

public:
  Tool(const char *arg)
  {
    drizzled::message::Table::StorageEngine *engine;
    drizzled::message::Table::TableOptions *table_options;

    setName(arg);
    schema.set_name(name.c_str());
    schema.set_type(drizzled::message::Table::FUNCTION);

    table_options= schema.mutable_options();
    table_options->set_collation_id(default_charset_info->number);
    table_options->set_collation(default_charset_info->name);

    engine= schema.mutable_engine();
    engine->set_name(engine_name);
  }

  virtual ~Tool() {}

  class Generator 
  {

  public:
    virtual ~Generator()
    { }

    /*
      Return type is bool meaning "are there more rows".
    */
    virtual bool populate(Field **)
    {
      return false;
    }
  };

  void define(drizzled::message::Table &proto)
  { 
    proto.CopyFrom(schema);
  }

  std::string &getName()
  { 
    return name;
  }

  std::string &getPath()
  { 
    return path;
  }

  void setName(const char *arg)
  { 
    path.clear();
    name= arg;

    path.append("./data_dictionary/");
    path.append(name);
    transform(path.begin(), path.end(),
              path.begin(), ::tolower);
  }

  virtual Generator *generator()
  {
    return new Generator;
  }

  void add_field(const char *label,
                 drizzled::message::Table::Field::FieldType type,
                 uint32_t length= 0)
  {
    drizzled::message::Table::Field *field;
    drizzled::message::Table::Field::FieldOptions *field_options;
    drizzled::message::Table::Field::FieldConstraints *field_constraints;

    field= schema.add_field();
    field->set_name(label);
    field->set_type(type);

    field_options= field->mutable_options();
    field_constraints= field->mutable_constraints();
    field_options->set_default_null(false);
    field_constraints->set_is_nullable(false);

    switch (type) 
    {
    case drizzled::message::Table::Field::VARCHAR:
      {
        drizzled::message::Table::Field::StringFieldOptions *string_field_options;

        string_field_options= field->mutable_string_options();
        string_field_options->set_length(length);

        break;
      }
    default:
      break;
    }
  }
};

#endif // PLUGIN_DATA_ENGINE_TOOL_H
