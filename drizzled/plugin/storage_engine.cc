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

#include <drizzled/server_includes.h>
#include <drizzled/definitions.h>
#include <drizzled/base.h>
#include <drizzled/handler.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <vector>
#include <string>

#include CSTDINT_H

using namespace std;

vector<StorageEngine *> all_engines;

st_plugin_int *engine2plugin[MAX_HA];

static const LEX_STRING sys_table_aliases[]=
{
  { C_STRING_WITH_LEN("INNOBASE") },  { C_STRING_WITH_LEN("INNODB") },
  { C_STRING_WITH_LEN("HEAP") },      { C_STRING_WITH_LEN("MEMORY") },
  {NULL, 0}
};

StorageEngine::StorageEngine(const std::string &name_arg,
                             const std::bitset<HTON_BIT_SIZE> &flags_arg,
                             size_t savepoint_offset_arg,
                             bool support_2pc)
    : name(name_arg), two_phase_commit(support_2pc), enabled(true),
      flags(flags_arg),
      savepoint_offset(savepoint_alloc_size),
      orig_savepoint_offset(savepoint_offset_arg),
      slot(0)
{
  if (enabled)
  {
    savepoint_alloc_size+= orig_savepoint_offset;
    slot= total_ha++;
    if (two_phase_commit)
        total_ha_2pc++;
  }
}


StorageEngine::~StorageEngine()
{
  savepoint_alloc_size-= orig_savepoint_offset;
}


/* args: current_session, db, name */
int StorageEngine::table_exists_in_engine(Session*, const char *, const char *)
{
  return HA_ERR_NO_SUCH_TABLE;
}

static plugin_ref ha_default_plugin(Session *session)
{
  if (session->variables.table_plugin)
    return session->variables.table_plugin;
  return global_system_variables.table_plugin;
}


/**
  Return the default storage engine StorageEngine for thread

  @param ha_default_storage_engine(session)
  @param session         current thread

  @return
    pointer to StorageEngine
*/
StorageEngine *ha_default_storage_engine(Session *session)
{
  plugin_ref plugin= ha_default_plugin(session);
  assert(plugin);
  StorageEngine *engine= plugin_data(plugin, StorageEngine*);
  assert(engine);
  return engine;
}


/**
  Return the storage engine StorageEngine for the supplied name

  @param session         current thread
  @param name        name of storage engine

  @return
    pointer to storage engine plugin handle
*/
plugin_ref ha_resolve_by_name(Session *session, const LEX_STRING *name)
{
  const LEX_STRING *table_alias;
  plugin_ref plugin;

redo:
  /* my_strnncoll is a macro and gcc doesn't do early expansion of macro */
  if (session && !my_charset_utf8_general_ci.coll->strnncoll(&my_charset_utf8_general_ci,
                           (const unsigned char *)name->str, name->length,
                           (const unsigned char *)STRING_WITH_LEN("DEFAULT"), 0))
    return ha_default_plugin(session);

  if ((plugin= plugin_lock_by_name(session, name, DRIZZLE_STORAGE_ENGINE_PLUGIN)))
  {
    StorageEngine *engine= plugin_data(plugin, StorageEngine *);
    if (engine->is_user_selectable())
      return plugin;
  }

  /*
    We check for the historical aliases.
  */
  for (table_alias= sys_table_aliases; table_alias->str; table_alias+= 2)
  {
    if (!my_strnncoll(&my_charset_utf8_general_ci,
                      (const unsigned char *)name->str, name->length,
                      (const unsigned char *)table_alias->str,
                      table_alias->length))
    {
      name= table_alias + 1;
      goto redo;
    }
  }

  return NULL;
}


plugin_ref ha_lock_engine(Session *, StorageEngine *engine)
{
  if (engine)
  {
    st_plugin_int **plugin= engine2plugin + engine->slot;

    return plugin;
  }

  return NULL;
}


handler *get_new_handler(TABLE_SHARE *share, MEM_ROOT *alloc,
                         StorageEngine *engine)
{
  handler *file;

  if (engine && engine->is_enabled())
  {
    if ((file= engine->create(share, alloc)))
      file->init();
    return(file);
  }
  /*
    Try the default table type
    Here the call to current_session() is ok as we call this function a lot of
    times but we enter this branch very seldom.
  */
  return(get_new_handler(share, alloc, ha_default_storage_engine(current_session)));
}


int storage_engine_finalizer(st_plugin_int *plugin)
{
  StorageEngine *engine= static_cast<StorageEngine *>(plugin->data);

  all_engines.remove(engine);

  if (engine && plugin->plugin->deinit)
    (void)plugin->plugin->deinit(engine);

  return(0);
}


int storage_engine_initializer(st_plugin_int *plugin)
{
  StorageEngine *engine;


  if (plugin->plugin->init)
  {
    if (plugin->plugin->init(&engine))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Plugin '%s' init function returned error."),
                    plugin->name.str);
      return 1;
    }
  }

  if (engine->is_enabled())
    engine2plugin[engine->slot]=plugin;

  /*
    This is entirely for legacy. We will create a new "disk based" engine and a
    "memory" engine which will be configurable longterm. We should be able to
    remove partition and myisammrg.
  */
  if (strcmp(plugin->plugin->name, "MEMORY") == 0)
    heap_engine= engine;

  if (strcmp(plugin->plugin->name, "MyISAM") == 0)
    myisam_engine= engine;

  plugin->data= engine;
  plugin->isInited= true;
  all_engines.push_back(engine);

  return 0;
}

const char *ha_resolve_storage_engine_name(const StorageEngine *engine)
{
  return engine == NULL ? "UNKNOWN" : engine->get_name()->c_str();
}

