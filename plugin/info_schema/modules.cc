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

/**
 * @file 
 *   Modules I_S table methods.
 */

#include "drizzled/server_includes.h"

#include <vector>

#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/plugin/library.h"

#include "helper_methods.h"
#include "modules.h"


using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the modules I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the modules I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * modules I_S table.
 */
static plugin::InfoSchemaTable *mods_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *ModulesIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*columns);
  }

  columns->push_back(new plugin::ColumnInfo("MODULE_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Name",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("MODULE_VERSION",
                                            20,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("MODULE_AUTHOR",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("IS_BUILTIN",
                                            3,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("MODULE_LIBRARY",
                                            65535,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("MODULE_DESCRIPTION",
                                            65535,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("MODULE_LICENSE",
                                            80,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "License",
                                            SKIP_OPEN_TABLE));
  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *ModulesIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new ModulesISMethods();
  }

  if (mods_table == NULL)
  {
    mods_table= new plugin::InfoSchemaTable("MODULES",
                                            *columns,
                                            -1, -1, false, false, 0,
                                            methods);
  }

  return mods_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void ModulesIS::cleanup()
{
  clearColumns(*columns);
  delete mods_table;
  delete methods;
  delete columns;
}

class ShowModules : public unary_function<drizzled::plugin::Module *, bool>
{
  Session *session;
  Table *table;

  const string LICENSE_GPL_STRING;
  const string LICENSE_BSD_STRING;
  const string LICENSE_LGPL_STRING;
  const string LICENSE_PROPRIETARY_STRING;

public:
  ShowModules(Session *session_arg, Table *table_arg)
    : session(session_arg), table(table_arg), 
      LICENSE_GPL_STRING("GPL"),
      LICENSE_BSD_STRING("BSD"),
      LICENSE_LGPL_STRING("LGPL"),
      LICENSE_PROPRIETARY_STRING("PROPRIETARY")
  {}

  result_type operator() (argument_type module)
  {
    const drizzled::plugin::Manifest &manifest= module->getManifest();
    const CHARSET_INFO * const cs= system_charset_info;

    table->restoreRecordAsDefault();

    table->field[0]->store(module->getName().c_str(),
                           module->getName().size(), cs);

    if (manifest.version)
    {
      table->field[1]->store(manifest.version, strlen(manifest.version), cs);
      table->field[1]->set_notnull();
    }
    else
      table->field[1]->set_null();

    if (manifest.author)
    {
      table->field[2]->store(manifest.author, strlen(manifest.author), cs);
      table->field[2]->set_notnull();
    }
    else
    {
      table->field[2]->set_null();
    }

    if (module->plugin_dl == NULL)
    {
      table->field[3]->store(STRING_WITH_LEN("YES"),cs);
      table->field[4]->set_null();
    }
    else
    {
      table->field[3]->store(STRING_WITH_LEN("NO"),cs);
      table->field[4]->store(module->plugin_dl->getName().c_str(),
                             module->plugin_dl->getName().size(), cs);
    }

    if (manifest.descr)
    {
      table->field[5]->store(manifest.descr, strlen(manifest.descr), cs);
      table->field[5]->set_notnull();
    }
    else
    {
      table->field[5]->set_null();
    }

    switch (manifest.license) {
    case PLUGIN_LICENSE_GPL:
      table->field[6]->store(LICENSE_GPL_STRING.c_str(),
                             LICENSE_GPL_STRING.size(), cs);
      break;
    case PLUGIN_LICENSE_BSD:
      table->field[6]->store(LICENSE_BSD_STRING.c_str(),
                             LICENSE_BSD_STRING.size(), cs);
      break;
    case PLUGIN_LICENSE_LGPL:
      table->field[6]->store(LICENSE_LGPL_STRING.c_str(),
                             LICENSE_LGPL_STRING.size(), cs);
      break;
    default:
      table->field[6]->store(LICENSE_PROPRIETARY_STRING.c_str(),
                             LICENSE_PROPRIETARY_STRING.size(),
                             cs);
      break;
    }
    table->field[6]->set_notnull();

    return schema_table_store_record(session, table);
  }
};

int ModulesISMethods::fillTable(Session *session, TableList *tables)
{
  Table *table= tables->table;

  drizzled::plugin::Registry &registry= drizzled::plugin::Registry::singleton();
  vector<drizzled::plugin::Module *> modules= registry.getList(true);
  vector<drizzled::plugin::Module *>::iterator iter=
    find_if(modules.begin(), modules.end(), ShowModules(session, table));
  if (iter != modules.end())
  {
    return 1;
  }
  return 0;
}
