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

int ha_commit_or_rollback_by_xid(XID *xid, bool commit)
{
  vector<int> results;
  
  if (commit)
    transform(all_engines.begin(), all_engines.end(), results.begin(),
              bind2nd(mem_fun(&StorageEngine::commit_by_xid),xid));
  else
    transform(all_engines.begin(), all_engines.end(), results.begin(),
              bind2nd(mem_fun(&StorageEngine::rollback_by_xid),xid));

  if (find_if(results.begin(), results.end(), bind2nd(equal_to<int>(),0))
         == results.end())
    return 1;
  return 0;
}


/**
  @details
  This function should be called when MySQL sends rows of a SELECT result set
  or the EOF mark to the client. It releases a possible adaptive hash index
  S-latch held by session in InnoDB and also releases a possible InnoDB query
  FIFO ticket to enter InnoDB. To save CPU time, InnoDB allows a session to
  keep them over several calls of the InnoDB handler interface when a join
  is executed. But when we let the control to pass to the client they have
  to be released because if the application program uses mysql_use_result(),
  it may deadlock on the S-latch if the application on another connection
  performs another SQL query. In MySQL-4.1 this is even more important because
  there a connection can have several SELECT queries open at the same time.

  @param session           the thread handle of the current connection

  @return
    always 0
*/
int ha_release_temporary_latches(Session *session)
{
  for_each(all_engines.begin(), all_engines.end(),
           bind2nd(mem_fun(&StorageEngine::release_temporary_latches),session));
  return 0;
}


bool ha_flush_logs(StorageEngine *engine)
{
  if (engine == NULL)
  {
    if (find_if(all_engines.begin(), all_engines.end(),
            mem_fun(&StorageEngine::flush_logs))
          != all_engines.begin())
      return true;
  }
  else
  {
    if ((!engine->is_enabled()) ||
        (engine->flush_logs()))
      return true;
  }
  return false;
}

/**
  recover() step of xa.

  @note
    there are three modes of operation:
    - automatic recover after a crash
    in this case commit_list != 0, tc_heuristic_recover==0
    all xids from commit_list are committed, others are rolled back
    - manual (heuristic) recover
    in this case commit_list==0, tc_heuristic_recover != 0
    DBA has explicitly specified that all prepared transactions should
    be committed (or rolled back).
    - no recovery (MySQL did not detect a crash)
    in this case commit_list==0, tc_heuristic_recover == 0
    there should be no prepared transactions in this case.
*/
class XARecover : unary_function<StorageEngine *, void>
{
  int trans_len, found_foreign_xids, found_my_xids;
  bool result;
  XID *trans_list;
  HASH *commit_list;
  bool dry_run;
public:
  XARecover(XID *trans_list_arg, int trans_len_arg,
            HASH *commit_list_arg, bool dry_run_arg) 
    : trans_len(trans_len_arg), found_foreign_xids(0), found_my_xids(0),
      trans_list(trans_list_arg), commit_list(commit_list_arg),
      dry_run(dry_run_arg)
  {}
  
  int getForeignXIDs()
  {
    return found_foreign_xids; 
  }

  int getMyXIDs()
  {
    return found_my_xids; 
  }

  result_type operator() (argument_type engine)
  {
  
    int got;
  
    if (engine->is_enabled())
    {
      while ((got= engine->recover(trans_list, trans_len)) > 0 )
      {
        errmsg_printf(ERRMSG_LVL_INFO,
                      _("Found %d prepared transaction(s) in %s"),
                      got, engine->getName().c_str());
        for (int i=0; i < got; i ++)
        {
          my_xid x=trans_list[i].get_my_xid();
          if (!x) // not "mine" - that is generated by external TM
          {
            xid_cache_insert(trans_list+i, XA_PREPARED);
            found_foreign_xids++;
            continue;
          }
          if (dry_run)
          {
            found_my_xids++;
            continue;
          }
          // recovery mode
          if (commit_list ?
              hash_search(commit_list, (unsigned char *)&x, sizeof(x)) != 0 :
              tc_heuristic_recover == TC_HEURISTIC_RECOVER_COMMIT)
          {
            engine->commit_by_xid(trans_list+i);
          }
          else
          {
            engine->rollback_by_xid(trans_list+i);
          }
        }
        if (got < trans_len)
          break;
      }
    }
  }

};

int ha_recover(HASH *commit_list)
{
  XID *trans_list= NULL;
  int trans_len= 0;

  bool dry_run= (commit_list==0 && tc_heuristic_recover==0);

  /* commit_list and tc_heuristic_recover cannot be set both */
  assert(commit_list==0 || tc_heuristic_recover==0);

  /* if either is set, total_ha_2pc must be set too */
  if (total_ha_2pc <= 1)
    return 0;


#ifndef WILL_BE_DELETED_LATER

  /*
    for now, only InnoDB supports 2pc. It means we can always safely
    rollback all pending transactions, without risking inconsistent data
  */

  assert(total_ha_2pc == 2); // only InnoDB and binlog
  tc_heuristic_recover= TC_HEURISTIC_RECOVER_ROLLBACK; // forcing ROLLBACK
  dry_run=false;
#endif
  for (trans_len= MAX_XID_LIST_SIZE ;
       trans_list==0 && trans_len > MIN_XID_LIST_SIZE; trans_len/=2)
  {
    trans_list=(XID *)malloc(trans_len*sizeof(XID));
  }
  if (!trans_list)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_OUTOFMEMORY), trans_len*sizeof(XID));
    return(1);
  }

  if (commit_list)
    errmsg_printf(ERRMSG_LVL_INFO, _("Starting crash recovery..."));


  XARecover recover_func(trans_list, trans_len, commit_list, dry_run);
  for_each(all_engines.begin(), all_engines.end(), recover_func);
  free(trans_list);
 
  if (recover_func.getForeignXIDs())
    errmsg_printf(ERRMSG_LVL_WARN,
                  _("Found %d prepared XA transactions"),
                  recover_func.getForeignXIDs());
  if (dry_run && recover_func.getMyXIDs())
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("Found %d prepared transactions! It means that drizzled "
                    "was not shut down properly last time and critical "
                    "recovery information (last binlog or %s file) was "
                    "manually deleted after a crash. You have to start "
                    "drizzled with the --tc-heuristic-recover switch to "
                    "commit or rollback pending transactions."),
                    recover_func.getMyXIDs(), opt_tc_log_file);
    return(1);
  }
  if (commit_list)
    errmsg_printf(ERRMSG_LVL_INFO, _("Crash recovery finished."));
  return(0);
}

int ha_start_consistent_snapshot(Session *session)
{
  for_each(all_engines.begin(), all_engines.end(),
           bind2nd(mem_fun(&StorageEngine::start_consistent_snapshot),session));
  return 0;
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

