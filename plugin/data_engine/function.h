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

#include "config.h"
#include <assert.h>
#include <drizzled/session.h>
#include <drizzled/plugin/storage_engine.h>
#include "drizzled/hash.h"

#ifndef PLUGIN_DATA_ENGINE_FUNCTION_H
#define PLUGIN_DATA_ENGINE_FUNCTION_H

static const std::string engine_name("FUNCTION");

#include <plugin/data_engine/tool.h>

#include <plugin/data_engine/schemas.h>
#include <plugin/data_engine/tables.h>
#include <plugin/data_engine/columns.h>
#include <plugin/data_engine/indexes.h>
#include <plugin/data_engine/index_parts.h>
#include <plugin/data_engine/referential_constraints.h>
#include <plugin/data_engine/table_constraints.h>

#include <plugin/data_engine/character_sets.h>
#include <plugin/data_engine/collations.h>

#include <plugin/data_engine/modules.h>
#include <plugin/data_engine/plugins.h>
#include <plugin/data_engine/processlist.h>
#include <plugin/data_engine/status.h>
#include <plugin/data_engine/variables.h>

extern const CHARSET_INFO *default_charset_info;

static const char *function_exts[] = {
  NULL
};

class Function : public drizzled::plugin::StorageEngine
{
  typedef drizzled::hash_map<std::string, Tool *> ToolMap;
  typedef std::pair<std::string, Tool&> ToolMapPair;

  ToolMap table_map;

  CharacterSetsTool character_sets;
  CollationsTool collations;
  ColumnsTool columns;
  IndexPartsTool index_parts;
  IndexesTool indexes;
  ModulesTool modules;
  PluginsTool plugins;
  ProcesslistTool processlist;
  ReferentialConstraintsTool referential_constraints;
  SchemasTool schemas;
  StatementsTool global_statements;
  StatementsTool session_statements;
  StatusTool global_status;
  StatusTool session_status;
  TableConstraintsTool table_constraints;
  TablesTool tables;
  VariablesTool global_variables;
  VariablesTool session_variables;

  void addTool(Tool& tool)
  {
    std::pair<ToolMap::iterator, bool> ret;
    std::string schema= tool.getSchemaHome();
    std::string path= tool.getPath();

    transform(path.begin(), path.end(),
              path.begin(), ::tolower);

    transform(schema.begin(), schema.end(),
              schema.begin(), ::tolower);

    ret= table_map.insert(std::make_pair(path, &tool));
    assert(ret.second == true);
  }


public:
  Function(const std::string &name_arg);

  ~Function()
  {
  }

  Tool *getTool(const char *name_arg);

  int doCreateTable(Session *,
                    const char *,
                    Table&,
                    drizzled::message::Table&)
  {
    return EPERM;
  }

  int doDropTable(Session&, const std::string) 
  { 
    return EPERM; 
  }

  virtual Cursor *create(TableShare &table, drizzled::memory::Root *mem_root);

  const char **bas_ext() const 
  {
    return function_exts;
  }

  void doGetTableNames(drizzled::CachedDirectory&, 
                       std::string &db, 
                       std::set<std::string> &set_of_names);

  int doGetTableDefinition(Session &session,
                           const char *path,
                           const char *db,
                           const char *table_name,
                           const bool is_tmp,
                           drizzled::message::Table *table_proto);
};

#endif /* PLUGIN_DATA_ENGINE_FUNCTION_H */
