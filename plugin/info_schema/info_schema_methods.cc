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
 *   Implementation of the methods for I_S tables.
 */

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/show.h>

#include "info_schema_methods.h"

using namespace std;

int CharSetISMethods::fillTable(Session *session, TableList *tables, COND *)
{
  CHARSET_INFO **cs;
  const char *wild= session->lex->wild ? session->lex->wild->ptr() : NULL;
  Table *table= tables->table;
  const CHARSET_INFO * const scs= system_charset_info;

  for (cs= all_charsets ; cs < all_charsets+255 ; cs++)
  {
    const CHARSET_INFO * const tmp_cs= cs[0];
    if (tmp_cs && (tmp_cs->state & MY_CS_PRIMARY) &&
        (tmp_cs->state & MY_CS_AVAILABLE) &&
        !(tmp_cs->state & MY_CS_HIDDEN) &&
        !(wild && wild[0] &&
          wild_case_compare(scs, tmp_cs->csname,wild)))
    {
      const char *comment;
      table->restoreRecordAsDefault();
      table->field[0]->store(tmp_cs->csname, strlen(tmp_cs->csname), scs);
      table->field[1]->store(tmp_cs->name, strlen(tmp_cs->name), scs);
      comment= tmp_cs->comment ? tmp_cs->comment : "";
      table->field[2]->store(comment, strlen(comment), scs);
      table->field[3]->store((int64_t) tmp_cs->mbmaxlen, true);
      if (schema_table_store_record(session, table))
        return 1;
    }
  }
  return 0;
}

int CharSetISMethods::oldFormat(Session *session, InfoSchemaTable *schema_table)
  const
{
  int fields_arr[]= {0, 2, 1, 3, -1};
  int *field_num= fields_arr;
  const ColumnInfo *column;
  Name_resolution_context *context= &session->lex->select_lex.context;

  for (; *field_num >= 0; field_num++)
  {
    column= schema_table->getSpecificColumn(*field_num);
    Item_field *field= new Item_field(context,
                                      NULL, NULL, column->getName());
    if (field)
    {
      field->set_name(column->getOldName(),
                      strlen(column->getOldName()),
                      system_charset_info);
      if (session->add_item_to_list(field))
        return 1;
    }
  }
  return 0;
}

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

