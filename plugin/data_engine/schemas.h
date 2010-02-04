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

#ifndef PLUGIN_DATA_ENGINE_SCHEMAS_H
#define PLUGIN_DATA_ENGINE_SCHEMAS_H

#include "config.h"

#include <set>

#include "drizzled/plugin/table_function.h"
#include "drizzled/current_session.h"
#include "drizzled/plugin/storage_engine.h"
#include "drizzled/message/schema.pb.h"
#include "drizzled/field.h"


class SchemasTool : public drizzled::plugin::TableFunction
{
public:

  SchemasTool();

  SchemasTool(const char *schema_arg, const char *table_arg) :
    drizzled::plugin::TableFunction(schema_arg, table_arg)
  { }

  SchemasTool(const char *table_arg) :
    drizzled::plugin::TableFunction("data_dictionary", table_arg)
  { }

  class Generator : public drizzled::plugin::TableFunction::Generator 
  {
    drizzled::message::Schema schema;
    std::set<std::string> schema_names;
    std::set<std::string>::const_iterator schema_iterator;
    bool is_schema_primed;
    bool is_schema_parsed;

    virtual void fill();
    virtual bool checkSchema();

  public:
    Generator(Field **arg);

    const std::string &schema_name()
    {
      assert(is_schema_primed);
      return is_schema_parsed ? schema.name() : (*schema_iterator);
    }

    bool populate();
    bool nextSchemaCore();
    bool nextSchema();
    bool isSchemaPrimed()
    {
      return is_schema_primed;
    }
  };

  Generator *generator(Field **arg)
  {
    return new Generator(arg);
  }
};

class SchemaNames : public SchemasTool
{
public:
  SchemaNames() :
    SchemasTool("SCHEMA_NAMES")
  {
    add_field("SCHEMA_NAME");
  }

  class Generator : public SchemasTool::Generator 
  {
    void fill()
    {
      /* SCHEMA_NAME */
      push(schema_name());
    }

  public:
    Generator(Field **arg) :
      SchemasTool::Generator(arg)
    { }
  };

  Generator *generator(Field **arg)
  {
    return new Generator(arg);
  }
};

#include "plugin/data_engine/tables.h"
#include "plugin/data_engine/columns.h"
#include "plugin/data_engine/indexes.h"
#include "plugin/data_engine/index_parts.h"
#include "plugin/data_engine/referential_constraints.h"
#include "plugin/data_engine/table_constraints.h"


#endif // PLUGIN_DATA_ENGINE_SCHEMAS_H
