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

/**
 * @file Implementation of the I_S tables.
 */

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/show.h>

#include "info_schema_plugin.h"

#define LIST_PROCESS_HOST_LEN 64

using namespace std;

int ProcessListISMethods::fillTable(Session* session, TableList* tables, COND*)
{
  Table *table= tables->table;
  const CHARSET_INFO * const cs= system_charset_info;
  time_t now= time(NULL);
  size_t length;

  if (now == (time_t)-1)
    return 1;

  pthread_mutex_lock(&LOCK_thread_count);

  if (!session->killed)
  {
    Session* tmp;

    for (vector<Session*>::iterator it= session_list.begin(); it != session_list.end(); ++it)
    {
      tmp= *it;
      Security_context *tmp_sctx= &tmp->security_ctx;
      struct st_my_thread_var *mysys_var;
      const char *val;

      if (! tmp->protocol->isConnected())
        continue;

      table->restoreRecordAsDefault();
      /* ID */
      table->field[0]->store((int64_t) tmp->thread_id, true);
      /* USER */
      val= tmp_sctx->user.c_str() ? tmp_sctx->user.c_str() : "unauthenticated user";
      table->field[1]->store(val, strlen(val), cs);
      /* HOST */
      table->field[2]->store(tmp_sctx->ip.c_str(), strlen(tmp_sctx->ip.c_str()), cs);
      /* DB */
      if (tmp->db)
      {
        table->field[3]->store(tmp->db, strlen(tmp->db), cs);
        table->field[3]->set_notnull();
      }

      if ((mysys_var= tmp->mysys_var))
        pthread_mutex_lock(&mysys_var->mutex);
      /* COMMAND */
      if ((val= (char *) (tmp->killed == Session::KILL_CONNECTION? "Killed" : 0)))
        table->field[4]->store(val, strlen(val), cs);
      else
        table->field[4]->store(command_name[tmp->command].str,
                               command_name[tmp->command].length, cs);
      /* DRIZZLE_TIME */
      table->field[5]->store((uint32_t)(tmp->start_time ?
                                      now - tmp->start_time : 0), true);
      /* STATE */
      val= (char*) (tmp->protocol->isWriting() ?
                    "Writing to net" :
                    tmp->protocol->isReading() ?
                    (tmp->command == COM_SLEEP ?
                     NULL : "Reading from net") :
                    tmp->get_proc_info() ? tmp->get_proc_info() :
                    tmp->mysys_var &&
                    tmp->mysys_var->current_cond ?
                    "Waiting on cond" : NULL);
      if (val)
      {
        table->field[6]->store(val, strlen(val), cs);
        table->field[6]->set_notnull();
      }

      if (mysys_var)
        pthread_mutex_unlock(&mysys_var->mutex);

      length= strlen(tmp->process_list_info);

      if (length)
      {
        table->field[7]->store(tmp->process_list_info, length, cs);
        table->field[7]->set_notnull();
      }

      if (schema_table_store_record(session, table))
      {
        pthread_mutex_unlock(&LOCK_thread_count);
        return(1);
      }
    }
  }

  pthread_mutex_unlock(&LOCK_thread_count);
  return(0);
}

/*
 * The various columns for the PROCESSLIST I_S table.
 */
static ColumnInfo processlist_columns_info[]=
{
  ColumnInfo("ID", 4, DRIZZLE_TYPE_LONGLONG, 
             0, 0, "Id", SKIP_OPEN_TABLE),
  ColumnInfo("USER", 16, DRIZZLE_TYPE_VARCHAR, 
             0, 0, "User", SKIP_OPEN_TABLE),
  ColumnInfo("HOST", LIST_PROCESS_HOST_LEN,  DRIZZLE_TYPE_VARCHAR, 
             0, 0, "Host", SKIP_OPEN_TABLE),
  ColumnInfo("DB", NAME_CHAR_LEN, DRIZZLE_TYPE_VARCHAR, 
             0, 1, "Db", SKIP_OPEN_TABLE),
  ColumnInfo("COMMAND", 16, DRIZZLE_TYPE_VARCHAR, 
             0, 0, "Command", SKIP_OPEN_TABLE),
  ColumnInfo("TIME", 7, DRIZZLE_TYPE_LONGLONG, 
             0, 0, "Time", SKIP_OPEN_TABLE),
  ColumnInfo("STATE", 64, DRIZZLE_TYPE_VARCHAR, 
             0, 1, "State", SKIP_OPEN_TABLE),
  ColumnInfo("INFO", PROCESS_LIST_INFO_WIDTH, DRIZZLE_TYPE_VARCHAR, 
             0, 1, "Info", SKIP_OPEN_TABLE),
  ColumnInfo()
};

/*
 * List of methods for various I_S tables.
 */
static InfoSchemaMethods *processlist_methods= NULL;

/*
 * List of I_S tables.
 */
static InfoSchemaTable *processlist_table= NULL;

/**
 * Initialize the methods for each I_S table.
 *
 * @return false on success; true on failure
 */
bool initTableMethods()
{
  processlist_methods= new ProcessListISMethods();

  return false;
}

/**
 * Delete memory allocated for the I_S table methods.
 */
void cleanupTableMethods()
{
  delete processlist_methods;
}

/**
 * Initialize the I_S tables.
 *
 * @return false on success; true on failure
 */
bool initTables()
{

  processlist_table= new InfoSchemaTable("PROCESSLIST",
                                         processlist_columns_info,
                                         -1, -1, false, false, 0,
                                         processlist_methods);

  return false;
}

/**
 * Delete memory allocated for the I_S tables.
 */
void cleanupTables()
{
  delete processlist_table;
}

/**
 * Initialize the I_S plugin.
 *
 * @param[in] registry the PluginRegistry singleton
 * @return 0 on success; 1 on failure.
 */
int infoSchemaInit(PluginRegistry& registry)
{
  if (initTableMethods())
  {
    return 1;
  }

  if (initTables())
  {
    return 1;
  }

  registry.add(processlist_table);

  return 0;
}

/**
 * Clean up the I_S plugin.
 *
 * @param[in] registry the PluginRegistry singleton
 * @return 0 on success; 1 on failure
 */
int infoSchemaDone(PluginRegistry& registry)
{
  registry.remove(processlist_table);

  cleanupTableMethods();
  cleanupTables();

  return 0;
}

drizzle_declare_plugin(info_schema)
{
  "info_schema",
  "0.1",
  "Padraig O'Sullivan",
  "I_S plugin",
  PLUGIN_LICENSE_GPL,
  infoSchemaInit,
  infoSchemaDone,
  NULL,
  NULL,
  NULL
}
drizzle_declare_plugin_end;
