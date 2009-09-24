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
#include <drizzled/plugin/registry.h>
#include <string>

#include <drizzled/table_proto.h>

#include <mysys/my_dir.h>

#include CSTDINT_H

using namespace std;
using namespace drizzled;

plugin::StorageEngine::StorageEngine(const std::string name_arg,
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


plugin::StorageEngine::~StorageEngine()
{
  savepoint_alloc_size-= orig_savepoint_offset;
}

void plugin::StorageEngine::setTransactionReadWrite(Session* session)
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
    ha_info->set_trx_read_write();
  }
}


/**
  Return the default storage engine plugin::StorageEngine for thread

  @param ha_default_storage_engine(session)
  @param session         current thread

  @return
    pointer to plugin::StorageEngine
*/
plugin::StorageEngine *ha_default_storage_engine(Session *session)
{
  if (session->variables.storage_engine)
    return session->variables.storage_engine;
  return global_system_variables.storage_engine;
}


handler *get_new_handler(TableShare *share, MEM_ROOT *alloc,
                         plugin::StorageEngine *engine)
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




int plugin::StorageEngine::renameTableImplementation(Session *, const char *from, const char *to)
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
int plugin::StorageEngine::deleteTableImplementation(Session *, const std::string table_path)
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
                    bool update_create_info,
                    message::Table *table_proto)
{
  int error= 1;
  Table table;
  TableShare share(db, 0, table_name, path);
  message::Table tmp_proto;

  if (table_proto)
  {
    if (parse_table_proto(session, *table_proto, &share))
      goto err;
  }
  else
  {
    table_proto= &tmp_proto;
    if (open_table_def(session, &share))
      goto err;
  }

  if (open_table_from_share(session, &share, "", 0, (uint32_t) READ_ALL, 0,
                            &table, OTM_CREATE))
    goto err;

  if (update_create_info)
    table.updateCreateInfo(create_info, table_proto);

  error= share.storage_engine->createTable(session, path, &table,
                                           create_info, table_proto);
  table.closefrm(false);
  if (error)
  {
    char name_buff[FN_REFLEN];
    sprintf(name_buff,"%s.%s",db,table_name);
    my_error(ER_CANT_CREATE_TABLE, MYF(ME_BELL+ME_WAITTANG), name_buff, error);
  }
err:
  share.free_table_share();
  return(error != 0);
}


const string ha_resolve_storage_engine_name(const plugin::StorageEngine *engine)
{
  return engine == NULL ? string("UNKNOWN") : engine->getName();
}

const char *plugin::StorageEngine::checkLowercaseNames(const char *path, char *tmp_path)
{
  if (flags.test(HTON_BIT_FILE_BASED))
    return path;

  /* Ensure that table handler get path in lower case */
  if (tmp_path != path)
    strcpy(tmp_path, path);

  /*
    we only should turn into lowercase database/table part
    so start the process after homedirectory
  */
  if (strstr(tmp_path, drizzle_tmpdir) == tmp_path)
    my_casedn_str(files_charset_info, tmp_path + strlen(drizzle_tmpdir));
  else
    my_casedn_str(files_charset_info, tmp_path + drizzle_data_home_len);

  return tmp_path;
}

