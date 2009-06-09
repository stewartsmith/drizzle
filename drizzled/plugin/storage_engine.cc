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
#include <drizzled/unireg.h>
#include <drizzled/data_home.h>
#include <drizzled/plugin_registry.h>
#include <string>

#include CSTDINT_H

using namespace std;

drizzled::Registry<StorageEngine *> all_engines;

void add_storage_engine(StorageEngine *engine)
{
  all_engines.add(engine);
}

void remove_storage_engine(StorageEngine *engine)
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

void StorageEngine::setTransactionReadWrite(Session* session)
{
  Ha_trx_info *ha_info= &session->ha_data[getSlot()].ha_info[0];
  /*
    When a storage engine method is called, the transaction must
    have been started, unless it's a DDL call, for which the
    storage engine starts the transaction internally, and commits
    it internally, without registering in the ha_list.
    Unfortunately here we can't know know for sure if the engine
    has registered the transaction or not, so we must check.
  */
  if (ha_info->is_started())
  {
    /*
      table_share can be NULL in ha_delete_table(). See implementation
      of standalone function ha_delete_table() in sql_base.cc.
    */
//    if (table_share == NULL || table_share->tmp_table == NO_TMP_TABLE)
      ha_info->set_trx_read_write();
  }
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


handler *get_new_handler(TableShare *share, MEM_ROOT *alloc,
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

/**
  Ask handler if the table exists in engine.
  @retval
    HA_ERR_NO_SUCH_TABLE     Table does not exist
  @retval
    HA_ERR_TABLE_EXIST       Table exists
  @retval
    \#                  Error code
*/

class TableExistsInStorageEngine: public unary_function<StorageEngine *,bool>
{
  Session *session;
  const char *db;
  const char *name;
public:
  TableExistsInStorageEngine(Session *session_arg,
                             const char *db_arg, const char *name_arg)
    :session(session_arg), db(db_arg), name(name_arg) {}
  result_type operator() (argument_type engine)
  {
    int ret= engine->table_exists_in_engine(session, db, name);
    return ret == HA_ERR_TABLE_EXIST;
  } 
};

/**
  Call this function in order to give the handler the possiblity
  to ask engine if there are any new tables that should be written to disk
  or any dropped tables that need to be removed from disk
*/
int ha_table_exists_in_engine(Session* session,
                              const char* db, const char* name,
                              StorageEngine **engine_arg)
{
  StorageEngine *engine= NULL;
  bool found= false;

  drizzled::Registry<StorageEngine *>::iterator iter=
    find_if(all_engines.begin(), all_engines.end(),
            TableExistsInStorageEngine(session, db, name));
  if (iter != all_engines.end()) 
  {
    engine= *iter;
    found= true;
  }
  else
  {
    /* Default way of knowing if a table exists. (checking .frm exists) */

    char path[FN_REFLEN];
    size_t length;
    length= build_table_filename(path, sizeof(path),
                                 db, name, false);

    if ((table_proto_exists(path) == EEXIST))
      found= true;

    if (found && engine_arg)
    {
      drizzled::message::Table table;
      strcpy(path + length, ".dfe");
      if (drizzle_read_table_proto(path, &table) == 0)
      {
        LEX_STRING engine_name= { (char*)table.engine().name().c_str(),
                                 strlen(table.engine().name().c_str()) };
        engine= ha_resolve_by_name(session, &engine_name);
      }
    }
  }

  if (found == false)
    return HA_ERR_NO_SUCH_TABLE;

  if (engine_arg)
    *engine_arg= engine;

  return HA_ERR_TABLE_EXIST;
}

int StorageEngine::renameTableImpl(Session *, const char *from, const char *to)
{
  int error= 0;
  for (const char **ext= bas_ext(); *ext ; ext++)
  {
    if (rename_file_ext(from, to, *ext))
    {
      if ((error=my_errno) != ENOENT)
	break;
      error= 0;
    }
  }
  return error;
}


/**
  Delete all files with extension from bas_ext().

  @param name		Base name of table

  @note
    We assume that the handler may return more extensions than
    was actually used for the file.

  @retval
    0   If we successfully deleted at least one file from base_ext and
    didn't get any other errors than ENOENT
  @retval
    !0  Error
*/
int StorageEngine::deleteTableImpl(Session *, const std::string table_path)
{
  int error= 0;
  int enoent_or_zero= ENOENT;                   // Error if no file was deleted
  char buff[FN_REFLEN];

  for (const char **ext=bas_ext(); *ext ; ext++)
  {
    fn_format(buff, table_path.c_str(), "", *ext,
              MY_UNPACK_FILENAME|MY_APPEND_EXT);
    if (my_delete_with_symlink(buff, MYF(0)))
    {
      if ((error= my_errno) != ENOENT)
	break;
    }
    else
      enoent_or_zero= 0;                        // No error for ENOENT
    error= enoent_or_zero;
  }
  return error;
}

static const char *check_lowercase_names(handler *file, const char *path,
                                         char *tmp_path)
{
  if ((file->ha_table_flags() & HA_FILE_BASED))
    return path;

  /* Ensure that table handler get path in lower case */
  if (tmp_path != path)
    strcpy(tmp_path, path);

  /*
    we only should turn into lowercase database/table part
    so start the process after homedirectory
  */
  my_casedn_str(files_charset_info, tmp_path + drizzle_data_home_len);
  return tmp_path;
}

/**
  An interceptor to hijack the text of the error message without
  setting an error in the thread. We need the text to present it
  in the form of a warning to the user.
*/

class Ha_delete_table_error_handler: public Internal_error_handler
{
public:
  Ha_delete_table_error_handler() : Internal_error_handler() {}
  virtual bool handle_error(uint32_t sql_errno,
                            const char *message,
                            DRIZZLE_ERROR::enum_warning_level level,
                            Session *session);
  char buff[DRIZZLE_ERRMSG_SIZE];
};


bool
Ha_delete_table_error_handler::
handle_error(uint32_t ,
             const char *message,
             DRIZZLE_ERROR::enum_warning_level ,
             Session *)
{
  /* Grab the error message */
  strncpy(buff, message, sizeof(buff)-1);
  return true;
}


class DeleteTableStorageEngine
  : public unary_function<StorageEngine *, void>
{
  Session *session;
  const char *path;
  handler **file;
  int *dt_error;
public:
  DeleteTableStorageEngine(Session *session_arg, const char *path_arg,
                           handler **file_arg, int *error_arg)
    : session(session_arg), path(path_arg), file(file_arg), dt_error(error_arg) {}

  result_type operator() (argument_type engine)
  {
    char tmp_path[FN_REFLEN];
    handler *tmp_file;

    if(*dt_error!=ENOENT) /* already deleted table */
      return;

    if (!engine)
      return;

    if (!engine->is_enabled())
      return;

    if ((tmp_file= engine->create(NULL, session->mem_root)))
      tmp_file->init();
    else
      return;

    path= check_lowercase_names(tmp_file, path, tmp_path);
    const std::string table_path(path);
    int tmp_error= engine->deleteTable(session, table_path);

    if(tmp_error!=ENOENT)
    {
      *dt_error= tmp_error;
      if(*file)
        delete *file;
      *file= tmp_file;
      return;
    }
    else
      delete tmp_file;

    return;
  }
};

/**
  This should return ENOENT if the file doesn't exists.
  The .frm file will be deleted only if we return 0 or ENOENT
*/
int ha_delete_table(Session *session, const char *path,
                    const char *db, const char *alias, bool generate_warning)
{
  TableShare dummy_share;
  Table dummy_table;
  memset(&dummy_table, 0, sizeof(dummy_table));
  memset(&dummy_share, 0, sizeof(dummy_share));

  dummy_table.s= &dummy_share;

  int error= ENOENT;
  handler *file= NULL;

  for_each(all_engines.begin(), all_engines.end(),
           DeleteTableStorageEngine(session, path, &file, &error));

  if (error && generate_warning)
  {
    /*
      Because file->print_error() use my_error() to generate the error message
      we use an internal error handler to intercept it and store the text
      in a temporary buffer. Later the message will be presented to user
      as a warning.
    */
    Ha_delete_table_error_handler ha_delete_table_error_handler;

    /* Fill up strucutures that print_error may need */
    dummy_share.path.str= (char*) path;
    dummy_share.path.length= strlen(path);
    dummy_share.db.str= (char*) db;
    dummy_share.db.length= strlen(db);
    dummy_share.table_name.str= (char*) alias;
    dummy_share.table_name.length= strlen(alias);
    dummy_table.alias= alias;


    if(file != NULL)
    {
      file->change_table_ptr(&dummy_table, &dummy_share);

      session->push_internal_handler(&ha_delete_table_error_handler);
      file->print_error(error, 0);

      session->pop_internal_handler();
    }
    else
      error= -1; /* General form of fail. maybe bad FRM */

    /*
      XXX: should we convert *all* errors to warnings here?
      What if the error is fatal?
    */
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR, error,
                 ha_delete_table_error_handler.buff);
  }

  if(file)
    delete file;

  return error;
}

/**
  Initiates table-file and calls appropriate database-creator.

  @retval
   0  ok
  @retval
   1  error
*/
int ha_create_table(Session *session, const char *path,
                    const char *db, const char *table_name,
                    HA_CREATE_INFO *create_info,
                    bool update_create_info)
{
  int error= 1;
  Table table;
  char name_buff[FN_REFLEN];
  const char *name;
  TableShare share(db, 0, table_name, path);

  if (open_table_def(session, &share) ||
      open_table_from_share(session, &share, "", 0, (uint32_t) READ_ALL, 0, &table,
                            OTM_CREATE))
    goto err;

  if (update_create_info)
    table.updateCreateInfo(create_info);

  name= check_lowercase_names(table.file, share.path.str, name_buff);

  error= share.storage_engine->createTable(session, name, &table, create_info);
  table.closefrm(false);
  if (error)
  {
    sprintf(name_buff,"%s.%s",db,table_name);
    my_error(ER_CANT_CREATE_TABLE, MYF(ME_BELL+ME_WAITTANG), name_buff, error);
  }
err:
  share.free_table_share();
  return(error != 0);
}


const string ha_resolve_storage_engine_name(const StorageEngine *engine)
{
  return engine == NULL ? string("UNKNOWN") : engine->getName();
}

