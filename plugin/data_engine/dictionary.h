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

#ifndef PLUGIN_DATA_ENGINE_DICTIONARY_H
#define PLUGIN_DATA_ENGINE_DICTIONARY_H

#include <plugin/data_engine/tool.h>
#include <plugin/data_engine/processlist.h>
#include <plugin/data_engine/modules.h>
#include <plugin/data_engine/plugins.h>

extern const CHARSET_INFO *default_charset_info;

static const std::string engine_name("DICTIONARY");

static const char *dictionary_exts[] = {
  NULL
};

class Dictionary : public drizzled::plugin::StorageEngine
{
  ProcesslistTool processlist;
  ModulesTool modules;
  PluginsTool plugins;

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
