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
#include <drizzled/registry.h>

#include <string>

#include CSTDINT_H

using namespace std;

drizzled::Registry<StorageEngine *> all_engines;

static void add_storage_engine(StorageEngine *engine)
{
  all_engines.add(engine);
}

static void remove_storage_engine(StorageEngine *engine)
{
  all_engines.remove(engine);
}

StorageEngine::StorageEngine(const std::string name_arg,
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


/**
  Return the default storage engine StorageEngine for thread

  @param ha_default_storage_engine(session)
  @param session         current thread

  @return
    pointer to StorageEngine
*/
StorageEngine *ha_default_storage_engine(Session *session)
{
  if (session->variables.storage_engine)
    return session->variables.storage_engine;
  return global_system_variables.storage_engine;
}


/**
  Return the storage engine StorageEngine for the supplied name

  @param session         current thread
  @param name        name of storage engine

  @return
    pointer to storage engine plugin handle
*/
StorageEngine *ha_resolve_by_name(Session *session, const LEX_STRING *name)
{

  string find_str(name->str, name->length);
  transform(find_str.begin(), find_str.end(),
            find_str.begin(), ::tolower);
  string default_str("default");
  if (find_str == default_str)
    return ha_default_storage_engine(session);
    

  StorageEngine *engine= all_engines.find(find_str);

  if (engine && engine->is_user_selectable())
    return engine;

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

class StorageEngineCloseConnection
  : public unary_function<StorageEngine *, void>
{
  Session *session;
public:
  StorageEngineCloseConnection(Session *session_arg) : session(session_arg) {}
  /*
    there's no need to rollback here as all transactions must
    be rolled back already
  */
  inline result_type operator() (argument_type engine)
  {
    if (engine->is_enabled() && 
      session_get_ha_data(session, engine))
    engine->close_connection(session);
  }
};

/**
  @note
    don't bother to rollback here, it's done already
*/
void ha_close_connection(Session* session)
{
  for_each(all_engines.begin(), all_engines.end(),
           StorageEngineCloseConnection(session));
}

void ha_drop_database(char* path)
{
  for_each(all_engines.begin(), all_engines.end(),
           bind2nd(mem_fun(&StorageEngine::drop_database),path));
}


int storage_engine_finalizer(st_plugin_int *plugin)
{
  StorageEngine *engine= static_cast<StorageEngine *>(plugin->data);

  remove_storage_engine(engine);

  if (engine && plugin->plugin->deinit)
    (void)plugin->plugin->deinit(engine);

  return(0);
}


int storage_engine_initializer(st_plugin_int *plugin)
{
  StorageEngine *engine= NULL;

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

  if (engine != NULL)
    add_storage_engine(engine);

  plugin->data= engine;
  plugin->isInited= true;

  return 0;
}

const string ha_resolve_storage_engine_name(const StorageEngine *engine)
{
  return engine == NULL ? string("UNKNOWN") : engine->getName();
}

