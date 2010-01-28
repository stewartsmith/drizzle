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
 * @file 
 *   proceslist I_S table methods.
 */

#include "config.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/plugin/client.h"
#include "drizzled/session_list.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/internal/my_sys.h"

#include "helper_methods.h"
#include "processlist.h"

#include <vector>

#define LIST_PROCESS_HOST_LEN 64

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the processlist I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the processlist I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * processlist I_S table.
 */
static plugin::InfoSchemaTable *pl_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *ProcessListIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*columns);
  }

  /*
   * Create each column for the PROCESSLIST table.
   */
  columns->push_back(new plugin::ColumnInfo("ID", 
                                            4,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            0,
                                            "Id"));

  columns->push_back(new plugin::ColumnInfo("USER",
                                            16,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "User"));

  columns->push_back(new plugin::ColumnInfo("HOST",
                                            LIST_PROCESS_HOST_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Host"));

  columns->push_back(new plugin::ColumnInfo("DB",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Db"));

  columns->push_back(new plugin::ColumnInfo("COMMAND",
                                            16,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Command"));

  columns->push_back(new plugin::ColumnInfo("TIME",
                                            7,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            0,
                                            "Time"));

  columns->push_back(new plugin::ColumnInfo("STATE",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "State"));

  columns->push_back(new plugin::ColumnInfo("INFO",
                                            16383,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            1,
                                            "Info"));

  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *ProcessListIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new ProcessListISMethods();
  }

  if (pl_table == NULL)
  {
    pl_table= new plugin::InfoSchemaTable("PROCESSLIST",
                                          *columns,
                                          -1, -1, false, false, 0,
                                          methods);
  }

  return pl_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void ProcessListIS::cleanup()
{
  clearColumns(*columns);
  delete pl_table;
  delete methods;
  delete columns;
}

int ProcessListISMethods::fillTable(Session* session, 
                                    Table *table,
                                    plugin::InfoSchemaTable *schema_table)
{
  const CHARSET_INFO * const cs= system_charset_info;
  time_t now= time(NULL);
  size_t length;

  if (now == (time_t)-1)
    return 1;

  pthread_mutex_lock(&LOCK_thread_count);

  if (! session->killed)
  {
    Session* tmp;

    for (vector<Session*>::iterator it= getSessionList().begin(); it != getSessionList().end(); ++it)
    {
      tmp= *it;
      SecurityContext *tmp_sctx= &tmp->security_ctx;
      struct st_my_thread_var *mysys_var;
      const char *val;

      if (! tmp->client->isConnected())
        continue;

      table->restoreRecordAsDefault();
      table->setWriteSet(0);
      table->setWriteSet(1);
      table->setWriteSet(2);
      table->setWriteSet(3);
      table->setWriteSet(4);
      table->setWriteSet(5);
      table->setWriteSet(6);
      table->setWriteSet(7);
      /* ID */
      table->field[0]->store((int64_t) tmp->thread_id, true);
      /* USER */
      val= tmp_sctx->getUser().c_str() ? tmp_sctx->getUser().c_str() : "unauthenticated user";
      table->field[1]->store(val, strlen(val), cs);
      /* HOST */
      table->field[2]->store(tmp_sctx->getIp().c_str(), strlen(tmp_sctx->getIp().c_str()), cs);
      /* DB */
      if (! tmp->db.empty())
      {
        table->field[3]->store(tmp->db.c_str(), tmp->db.length(), cs);
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
      val= (char*) (tmp->client->isWriting() ?
                    "Writing to net" :
                    tmp->client->isReading() ?
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

      schema_table->addRow(table->record[0], table->s->reclength);
    }
  }

  pthread_mutex_unlock(&LOCK_thread_count);
  return 0;
}

