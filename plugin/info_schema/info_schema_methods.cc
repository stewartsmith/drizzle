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
  const InfoSchemaTable::Columns columns= schema_table->getColumns();
  const ColumnInfo *column;
  Name_resolution_context *context= &session->lex->select_lex.context;

  for (; *field_num >= 0; field_num++)
  {
    column= columns[*field_num];
    Item_field *field= new Item_field(context,
                                      NULL, NULL, column->getName().c_str());
    if (field)
    {
      field->set_name(column->getOldName().c_str(),
                      column->getOldName().length(),
                      system_charset_info);
      if (session->add_item_to_list(field))
        return 1;
    }
  }
  return 0;
}

int CollationISMethods::fillTable(Session *session, TableList *tables, COND *)
{
  CHARSET_INFO **cs;
  const char *wild= session->lex->wild ? session->lex->wild->ptr() : NULL;
  Table *table= tables->table;
  const CHARSET_INFO * const scs= system_charset_info;
  for (cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    CHARSET_INFO **cl;
    const CHARSET_INFO *tmp_cs= cs[0];
    if (!tmp_cs || !(tmp_cs->state & MY_CS_AVAILABLE) ||
         (tmp_cs->state & MY_CS_HIDDEN) ||
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for (cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      const CHARSET_INFO *tmp_cl= cl[0];
      if (!tmp_cl || !(tmp_cl->state & MY_CS_AVAILABLE) ||
          !my_charset_same(tmp_cs, tmp_cl))
        continue;
      if (!(wild && wild[0] &&
          wild_case_compare(scs, tmp_cl->name,wild)))
      {
        const char *tmp_buff;
        table->restoreRecordAsDefault();
        table->field[0]->store(tmp_cl->name, strlen(tmp_cl->name), scs);
        table->field[1]->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
        table->field[2]->store((int64_t) tmp_cl->number, true);
        tmp_buff= (tmp_cl->state & MY_CS_PRIMARY) ? "Yes" : "";
        table->field[3]->store(tmp_buff, strlen(tmp_buff), scs);
        tmp_buff= (tmp_cl->state & MY_CS_COMPILED)? "Yes" : "";
        table->field[4]->store(tmp_buff, strlen(tmp_buff), scs);
        table->field[5]->store((int64_t) tmp_cl->strxfrm_multiply, true);
        if (schema_table_store_record(session, table))
          return 1;
      }
    }
  }
  return 0;
}

int CollCharISMethods::fillTable(Session *session, TableList *tables, COND *)
{
  CHARSET_INFO **cs;
  Table *table= tables->table;
  const CHARSET_INFO * const scs= system_charset_info;
  for (cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    CHARSET_INFO **cl;
    const CHARSET_INFO *tmp_cs= cs[0];
    if (!tmp_cs || !(tmp_cs->state & MY_CS_AVAILABLE) ||
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for (cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      const CHARSET_INFO *tmp_cl= cl[0];
      if (!tmp_cl || !(tmp_cl->state & MY_CS_AVAILABLE) ||
          !my_charset_same(tmp_cs,tmp_cl))
        continue;
      table->restoreRecordAsDefault();
      table->field[0]->store(tmp_cl->name, strlen(tmp_cl->name), scs);
      table->field[1]->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
      if (schema_table_store_record(session, table))
        return 1;
    }
  }
  return 0;
}

static void store_key_column_usage(Table *table, LEX_STRING *db_name,
                                   LEX_STRING *table_name, const char *key_name,
                                   uint32_t key_len, const char *con_type, uint32_t con_len,
                                   int64_t idx)
{
  const CHARSET_INFO * const cs= system_charset_info;
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(key_name, key_len, cs);
  table->field[4]->store(db_name->str, db_name->length, cs);
  table->field[5]->store(table_name->str, table_name->length, cs);
  table->field[6]->store(con_type, con_len, cs);
  table->field[7]->store((int64_t) idx, true);
}

int KeyColUsageISMethods::processTable(Session *session,
                                              TableList *tables,
                                              Table *table, bool res,
                                              LEX_STRING *db_name,
                                              LEX_STRING *table_name) const
{
  if (res)
  {
    if (session->is_error())
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                   session->main_da.sql_errno(), session->main_da.message());
    session->clear_error();
    return(0);
  }
  else
  {
    List<FOREIGN_KEY_INFO> f_key_list;
    Table *show_table= tables->table;
    KEY *key_info=show_table->key_info;
    uint32_t primary_key= show_table->s->primary_key;
    show_table->file->info(HA_STATUS_VARIABLE |
                           HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);
    for (uint32_t i=0 ; i < show_table->s->keys ; i++, key_info++)
    {
      if (i != primary_key && !(key_info->flags & HA_NOSAME))
      {
        continue;
      }
      uint32_t f_idx= 0;
      KEY_PART_INFO *key_part= key_info->key_part;
      for (uint32_t j=0 ; j < key_info->key_parts ; j++,key_part++)
      {
        if (key_part->field)
        {
          f_idx++;
          table->restoreRecordAsDefault();
          store_key_column_usage(table, db_name, table_name,
                                 key_info->name,
                                 strlen(key_info->name),
                                 key_part->field->field_name,
                                 strlen(key_part->field->field_name),
                                 (int64_t) f_idx);
          if (schema_table_store_record(session, table))
          {
            return (1);
          }
        }
      }
    }

    show_table->file->get_foreign_key_list(session, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> fkey_it(f_key_list);
    while ((f_key_info= fkey_it++))
    {
      LEX_STRING *f_info;
      LEX_STRING *r_info;
      List_iterator_fast<LEX_STRING> it(f_key_info->foreign_fields),
        it1(f_key_info->referenced_fields);
      uint32_t f_idx= 0;
      while ((f_info= it++))
      {
        r_info= it1++;
        f_idx++;
        table->restoreRecordAsDefault();
        store_key_column_usage(table, db_name, table_name,
                               f_key_info->forein_id->str,
                               f_key_info->forein_id->length,
                               f_info->str, f_info->length,
                               (int64_t) f_idx);
        table->field[8]->store((int64_t) f_idx, true);
        table->field[8]->set_notnull();
        table->field[9]->store(f_key_info->referenced_db->str,
                               f_key_info->referenced_db->length,
                               system_charset_info);
        table->field[9]->set_notnull();
        table->field[10]->store(f_key_info->referenced_table->str,
                                f_key_info->referenced_table->length,
                                system_charset_info);
        table->field[10]->set_notnull();
        table->field[11]->store(r_info->str, r_info->length,
                                system_charset_info);
        table->field[11]->set_notnull();
        if (schema_table_store_record(session, table))
        {
          return (1);
        }
      }
    }
  }
  return (res);
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

int
RefConstraintsISMethods::processTable(Session *session, TableList *tables,
                                      Table *table, bool res,
                                      LEX_STRING *db_name, LEX_STRING *table_name)
  const
{
  const CHARSET_INFO * const cs= system_charset_info;

  if (res)
  {
    if (session->is_error())
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                   session->main_da.sql_errno(), session->main_da.message());
    session->clear_error();
    return(0);
  }

  {
    List<FOREIGN_KEY_INFO> f_key_list;
    Table *show_table= tables->table;
    show_table->file->info(HA_STATUS_VARIABLE |
                           HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);

    show_table->file->get_foreign_key_list(session, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info= it++))
    {
      table->restoreRecordAsDefault();
      table->field[1]->store(db_name->str, db_name->length, cs);
      table->field[9]->store(table_name->str, table_name->length, cs);
      table->field[2]->store(f_key_info->forein_id->str,
                             f_key_info->forein_id->length, cs);
      table->field[4]->store(f_key_info->referenced_db->str,
                             f_key_info->referenced_db->length, cs);
      table->field[10]->store(f_key_info->referenced_table->str,
                             f_key_info->referenced_table->length, cs);
      if (f_key_info->referenced_key_name)
      {
        table->field[5]->store(f_key_info->referenced_key_name->str,
                               f_key_info->referenced_key_name->length, cs);
        table->field[5]->set_notnull();
      }
      else
      {
        table->field[5]->set_null();
      }
      table->field[6]->store(STRING_WITH_LEN("NONE"), cs);
      table->field[7]->store(f_key_info->update_method->str,
                             f_key_info->update_method->length, cs);
      table->field[8]->store(f_key_info->delete_method->str,
                             f_key_info->delete_method->length, cs);
      if (schema_table_store_record(session, table))
      {
        return (1);
      }
    }
  }
  return (0);
}

