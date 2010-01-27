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

#include <plugin/data_engine/dictionary.h>
#include <plugin/data_engine/cursor.h>

#include <string>

using namespace std;
using namespace drizzled;

static const string schema_name("data_dictionary");
static const string schema_name_prefix("./data_dictionary/");

Dictionary::Dictionary(const std::string &name_arg) :
  drizzled::plugin::StorageEngine(name_arg,
                                  HTON_ALTER_NOT_SUPPORTED |
                                  HTON_SKIP_STORE_LOCK |
                                  HTON_TEMPORARY_NOT_SUPPORTED)
{

  pair<ToolMap::iterator, bool> ret;

  ret= table_map.insert(make_pair(character_sets.getPath(), &character_sets));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(collation_character_set_applicability.getPath(),
                                  &collation_character_set_applicability));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(collations.getPath(),
                                  &collations));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(columns.getPath(),
                                  &columns));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(key_column_usage.getPath(),
                                  &key_column_usage));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(modules.getPath(),
                                  &modules));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(plugins.getPath(),
                                  &plugins));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(processlist.getPath(),
                                  &processlist));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(referential_constraints.getPath(),
                                  &referential_constraints));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(schemas.getPath(),
                                  &schemas));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(schemas_full.getPath(),
                                  &schemas_full));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(schemata.getPath(),
                                  &schemata));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(statistics.getPath(),
                                  &statistics));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(global_status.getPath(),
                                  &global_status));
  assert(ret.second == true);

#if 0
  ret= table_map.insert(make_pair(session_status.getPath(),
                                  &session_status));
  assert(ret.second == true);
#endif

  ret= table_map.insert(make_pair(tables.getPath(),
                                  &tables));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(table_constraints.getPath(),
                                  &table_constraints));
  assert(ret.second == true);

  ret= table_map.insert(make_pair(global_variables.getPath(),
                                  &global_variables));
  assert(ret.second == true);

#if 0
  ret= table_map.insert(make_pair(session_variables.getPath(),
                                  &session_variables));
  assert(ret.second == true);
#endif
}


Cursor *Dictionary::create(TableShare &table, memory::Root *mem_root)
{
  return new (mem_root) DictionaryCursor(*this, table);
}

Tool *Dictionary::getTool(const char *path)
{
  ToolMap::iterator iter= table_map.find(path);

  if (iter == table_map.end())
  {
    fprintf(stderr, "\n %s\n", path);
    assert(path == NULL);
  }
  return (*iter).second;

}


int Dictionary::doGetTableDefinition(Session &,
                                     const char *path,
                                     const char *,
                                     const char *,
                                     const bool,
                                     message::Table *table_proto)
{
  string tab_name(path);

  if (tab_name.compare(0, schema_name_prefix.length(), schema_name_prefix) != 0)
  {
    return ENOENT;
  }

  ToolMap::iterator iter= table_map.find(path);

  if (iter == table_map.end())
  {
    fprintf(stderr, "\n doGetTableDefinition() %s\n", path);
    return ENOENT;
  }

  if (table_proto)
  {
    Tool *tool= (*iter).second;

    tool->define(*table_proto);
  }

  return EEXIST;
}


void Dictionary::doGetTableNames(drizzled::CachedDirectory&, 
                                        string &db, 
                                        set<string> &set_of_names)
{
  if (db.compare("data_dictionary"))
    return;

  for (ToolMap::iterator it= table_map.begin();
       it != table_map.end();
       it++)
  {
    Tool *tool= (*it).second;
    set_of_names.insert(tool->getName());
  }
}


static plugin::StorageEngine *dictionary_plugin= NULL;

static int init(plugin::Registry &registry)
{
  dictionary_plugin= new(std::nothrow) Dictionary(engine_name);
  if (! dictionary_plugin)
  {
    return 1;
  }

  registry.add(dictionary_plugin);
  
  return 0;
}

static int finalize(plugin::Registry &registry)
{
  registry.remove(dictionary_plugin);
  delete dictionary_plugin;

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "DICTIONARY",
  "1.0",
  "Brian Aker",
  "Dictionary provides the information for data_dictionary.",
  PLUGIN_LICENSE_GPL,
  init,     /* Plugin Init */
  finalize,     /* Plugin Deinit */
  NULL,               /* status variables */
  NULL,               /* system variables */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
