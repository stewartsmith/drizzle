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

#ifndef PLUGIN_DATA_ENGINE_DICTIONARY_H
#define PLUGIN_DATA_ENGINE_DICTIONARY_H

#include <plugin/data_engine/tool.h>

#include <plugin/data_engine/character_sets.h>
#include <plugin/data_engine/collation_character_set_applicability.h>
#include <plugin/data_engine/collations.h>
#include <plugin/data_engine/columns.h>
#include <plugin/data_engine/key_column_usage.h>
#include <plugin/data_engine/modules.h>
#include <plugin/data_engine/plugins.h>
#include <plugin/data_engine/processlist.h>
#include <plugin/data_engine/referential_constraints.h>
#include <plugin/data_engine/schemata.h>
#include <plugin/data_engine/statistics.h>
#include <plugin/data_engine/status.h>
#include <plugin/data_engine/table_constraints.h>
#include <plugin/data_engine/tables.h>
#include <plugin/data_engine/variables.h>

extern const CHARSET_INFO *default_charset_info;

static const std::string engine_name("DICTIONARY");

static const char *dictionary_exts[] = {
  NULL
};

class Dictionary : public drizzled::plugin::StorageEngine
{
  typedef drizzled::hash_map<std::string, Tool *> ToolMap;
  typedef std::pair<std::string, Tool&> ToolMapPair;

  ToolMap table_map;

  CharacterSetsTool character_sets;
  CollationCharacterSetApplicabilityTool collation_character_set_applicability;
  CollationsTool collations;
  ColumnsTool columns;
  KeyColumnUsageTool key_column_usage;
  ModulesTool modules;
  PluginsTool plugins;
  ProcesslistTool processlist;
  ReferentialConstraintsTool referential_constraints;
  SchemataTool schemata;
  StatisticsTool statistics;
  StatusTool global_status;
  StatusTool session_status;
  TablesTool tables;
  TableConstraintsTool table_constraints;
  VariablesTool global_variables;
  VariablesTool session_variables;


public:
  Dictionary(const std::string &name_arg);

  ~Dictionary()
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
    return dictionary_exts;
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

#endif /* PLUGIN_DATA_ENGINE_DICTIONARY_H */
