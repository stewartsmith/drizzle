/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#include "drizzled/server_includes.h"
#include "drizzled/plugin_registry.h"

#include "drizzled/plugin.h"
#include "drizzled/show.h"
#include "drizzled/handler.h"
#include "drizzled/errmsg.h"
#include "drizzled/authentication.h"
#include "drizzled/qcache.h"
#include "drizzled/scheduling.h"
#include "drizzled/logging.h"
#include "drizzled/sql_udf.h"
#include "drizzled/protocol.h"
#include "drizzled/transaction_services.h"

#include <string>
#include <vector>
#include <map>

using namespace std;

static Plugin_registry the_registry;
Plugin_registry& Plugin_registry::get_plugin_registry()
{
  return the_registry;
}

st_plugin_int *Plugin_registry::find(const LEX_STRING *name, int type)
{
  uint32_t i;
  string find_str(name->str,name->length);
  transform(find_str.begin(), find_str.end(), find_str.begin(), ::tolower);

  map<string, st_plugin_int *>::iterator map_iter;
  if (type == DRIZZLE_ANY_PLUGIN)
  {
    for (i= 0; i < DRIZZLE_MAX_PLUGIN_TYPE_NUM; i++)
    {
      map_iter= plugin_map[i].find(find_str);
      if (map_iter != plugin_map[i].end())
        return (*map_iter).second;
    }
  }
  else
  {
    map_iter= plugin_map[type].find(find_str);
    if (map_iter != plugin_map[type].end())
      return (*map_iter).second;
  }
  return(0);
}

void Plugin_registry::add(st_mysql_plugin *handle, st_plugin_int *plugin)
{
  string add_str(plugin->name.str);
  transform(add_str.begin(), add_str.end(),
            add_str.begin(), ::tolower);

  plugin_map[handle->type][add_str]= plugin;
}


void Plugin_registry::get_list(uint32_t type,
                                    vector<st_plugin_int *> &plugins,
                                    bool active)
{
  st_plugin_int *plugin= NULL;
  plugins.reserve(plugin_map[type].size());
  map<string, st_plugin_int *>::iterator map_iter;

  for (map_iter= plugin_map[type].begin();
       map_iter != plugin_map[type].end();
       map_iter++)
  {
    plugin= (*map_iter).second;
    if (active)
      plugins.push_back(plugin);
    else if (plugin->isInited)
      plugins.push_back(plugin);
  }
}

void Plugin_registry::registerPlugin(StorageEngine *engine)
{
  add_storage_engine(engine);
}

void Plugin_registry::registerPlugin(ST_SCHEMA_TABLE *schema_table)
{
  add_infoschema_table(schema_table);
}

void Plugin_registry::registerPlugin(Function_builder *udf)
{
  add_udf(udf);
}

void Plugin_registry::registerPlugin(Logging_handler *handler)
{
  add_logger(handler);
}

void Plugin_registry::registerPlugin(Error_message_handler *handler)
{
  add_errmsg_handler(handler);
}

void Plugin_registry::registerPlugin(Authentication *auth)
{
  add_authentication(auth);
}

void Plugin_registry::registerPlugin(QueryCache *qcache)
{
  add_query_cache(qcache);
}

void Plugin_registry::registerPlugin(SchedulerFactory *factory)
{
  add_scheduler_factory(factory);
}

void Plugin_registry::registerPlugin(ProtocolFactory *factory)
{
  add_protocol_factory(factory);
}

void Plugin_registry::registerPlugin(drizzled::plugin::Replicator *repl)
{
  add_replicator(repl);
}
