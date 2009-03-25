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

#include CSTDINT_H

/*
  While we have legacy_db_type, we have this array to
  check for dups and to find StorageEngine from legacy_db_type.
  Remove when legacy_db_type is finally gone
*/
st_plugin_int *hton2plugin[MAX_HA];

static StorageEngine *installed_htons[128];

static const LEX_STRING sys_table_aliases[]=
{
  { C_STRING_WITH_LEN("INNOBASE") },  { C_STRING_WITH_LEN("INNODB") },
  { C_STRING_WITH_LEN("HEAP") },      { C_STRING_WITH_LEN("MEMORY") },
  {NULL, 0}
};

/* args: current_session, db, name */
int StorageEngine::table_exists_in_engine(StorageEngine *, Session*,
                                          const char *, const char *)
{
  return HA_ERR_NO_SUCH_TABLE;
}

StorageEngine *ha_resolve_by_legacy_type(Session *session,
                                      enum legacy_db_type db_type)
{
  plugin_ref plugin;
  switch (db_type) {
  case DB_TYPE_DEFAULT:
    return ha_default_handlerton(session);
  default:
    if (db_type > DB_TYPE_UNKNOWN && db_type < DB_TYPE_DEFAULT &&
        (plugin= ha_lock_engine(session, installed_htons[db_type])))
      return plugin_data(plugin, StorageEngine*);
    /* fall through */
  case DB_TYPE_UNKNOWN:
    return NULL;
  }
}


static plugin_ref ha_default_plugin(Session *session)
{
  if (session->variables.table_plugin)
    return session->variables.table_plugin;
  return my_plugin_lock(session, &global_system_variables.table_plugin);
}


/**
  Return the default storage engine StorageEngine for thread

  @param ha_default_handlerton(session)
  @param session         current thread

  @return
    pointer to StorageEngine
*/
StorageEngine *ha_default_handlerton(Session *session)
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

  if ((plugin= my_plugin_lock_by_name(session, name, DRIZZLE_STORAGE_ENGINE_PLUGIN)))
  {
    StorageEngine *engine= plugin_data(plugin, StorageEngine *);
    if (!(engine->flags.test(HTON_BIT_NOT_USER_SELECTABLE)))
      return plugin;

    /*
      unlocking plugin immediately after locking is relatively low cost.
    */
    plugin_unlock(session, plugin);
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


plugin_ref ha_lock_engine(Session *session, StorageEngine *engine)
{
  if (engine)
  {
    st_plugin_int **plugin= hton2plugin + engine->slot;

    return my_plugin_lock(session, &plugin);
  }
  return NULL;
}


/**
  Use other database handler if databasehandler is not compiled in.
*/
StorageEngine *ha_checktype(Session *session, enum legacy_db_type database_type,
                          bool no_substitute, bool report_error)
{
  StorageEngine *engine= ha_resolve_by_legacy_type(session, database_type);
  if (ha_storage_engine_is_enabled(engine))
    return engine;

  if (no_substitute)
  {
    if (report_error)
    {
      const char *engine_name= ha_resolve_storage_engine_name(engine);
      my_error(ER_FEATURE_DISABLED,MYF(0),engine_name,engine_name);
    }
    return NULL;
  }

  return ha_default_handlerton(session);
} /* ha_checktype */


handler *get_new_handler(TABLE_SHARE *share, MEM_ROOT *alloc,
                         StorageEngine *db_type)
{
  handler *file;

  if (db_type && db_type->state == SHOW_OPTION_YES)
  {
    if ((file= db_type->create(db_type, share, alloc)))
      file->init();
    return(file);
  }
  /*
    Try the default table type
    Here the call to current_session() is ok as we call this function a lot of
    times but we enter this branch very seldom.
  */
  return(get_new_handler(share, alloc, ha_default_handlerton(current_session)));
}


int storage_engine_finalizer(st_plugin_int *plugin)
{
  StorageEngine *engine= static_cast<StorageEngine *>(plugin->data);

  switch (engine->state)
  {
  case SHOW_OPTION_NO:
  case SHOW_OPTION_DISABLED:
    break;
  case SHOW_OPTION_YES:
    if (installed_htons[engine->db_type] == engine)
      installed_htons[engine->db_type]= NULL;
    break;
  };

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

  engine->name= plugin->name.str;

  /*
    the switch below and engine->state should be removed when
    command-line options for plugins will be implemented
  */
  switch (engine->state) {
  case SHOW_OPTION_NO:
    break;
  case SHOW_OPTION_YES:
    {
      uint32_t tmp;
      /* now check the db_type for conflict */
      if (engine->db_type <= DB_TYPE_UNKNOWN ||
          engine->db_type >= DB_TYPE_DEFAULT ||
          installed_htons[engine->db_type])
      {
        int idx= (int) DB_TYPE_FIRST_DYNAMIC;

        while (idx < (int) DB_TYPE_DEFAULT && installed_htons[idx])
          idx++;

        if (idx == (int) DB_TYPE_DEFAULT)
        {
          errmsg_printf(ERRMSG_LVL_WARN, _("Too many storage engines!"));
          return(1);
        }
        if (engine->db_type != DB_TYPE_UNKNOWN)
          errmsg_printf(ERRMSG_LVL_WARN,
                        _("Storage engine '%s' has conflicting typecode. Assigning value %d."),
                        plugin->plugin->name, idx);
        engine->db_type= (enum legacy_db_type) idx;
      }
      installed_htons[engine->db_type]= engine;
      tmp= engine->savepoint_offset;
      engine->savepoint_offset= savepoint_alloc_size;
      savepoint_alloc_size+= tmp;
      engine->slot= total_ha++;
      hton2plugin[engine->slot]=plugin;
      if (engine->has_2pc())
        total_ha_2pc++;
      break;
    }
    /* fall through */
  default:
    engine->state= SHOW_OPTION_DISABLED;
    break;
  }

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
  plugin->state= PLUGIN_IS_READY;

  return(0);
}

enum legacy_db_type ha_legacy_type(const StorageEngine *db_type)
{
  return (db_type == NULL) ? DB_TYPE_UNKNOWN : db_type->db_type;
}

const char *ha_resolve_storage_engine_name(const StorageEngine *db_type)
{
  return db_type == NULL ? "UNKNOWN" : hton2plugin[db_type->slot]->name.str;
}

bool ha_check_storage_engine_flag(const StorageEngine *db_type, const engine_flag_bits flag)
{
  return db_type == NULL ? false : db_type->flags.test(static_cast<size_t>(flag));
}

bool ha_storage_engine_is_enabled(const StorageEngine *db_type)
{
  return (db_type) ?
         (db_type->state == SHOW_OPTION_YES) : false;
}

LEX_STRING *ha_storage_engine_name(const StorageEngine *engine)
{
  return &hton2plugin[engine->slot]->name;
}
