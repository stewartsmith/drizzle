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
#include <drizzled/tztime.h>
#include <drizzled/sql_base.h>
#include <drizzled/plugin/client.h>

#include "info_schema_methods.h"

#include <vector>
#include <string>

using namespace std;

static inline void make_upper(char *buf)
{
  for (; *buf; buf++)
    *buf= my_toupper(system_charset_info, *buf);
}

static bool show_status_array(Session *session, const char *wild,
                              SHOW_VAR *variables,
                              enum enum_var_type value_type,
                              struct system_status_var *status_var,
                              const char *prefix, Table *table,
                              bool ucase_names)
{
  MY_ALIGNED_BYTE_ARRAY(buff_data, SHOW_VAR_FUNC_BUFF_SIZE, int64_t);
  char * const buff= (char *) &buff_data;
  char *prefix_end;
  /* the variable name should not be longer than 64 characters */
  char name_buffer[64];
  int len;
  SHOW_VAR tmp, *var;

  prefix_end= strncpy(name_buffer, prefix, sizeof(name_buffer)-1);
  prefix_end+= strlen(prefix);

  if (*prefix)
    *prefix_end++= '_';
  len=name_buffer + sizeof(name_buffer) - prefix_end;

  for (; variables->name; variables++)
  {
    strncpy(prefix_end, variables->name, len);
    name_buffer[sizeof(name_buffer)-1]=0;       /* Safety */
    if (ucase_names)
      make_upper(name_buffer);

    /*
      if var->type is SHOW_FUNC, call the function.
      Repeat as necessary, if new var is again SHOW_FUNC
    */
    for (var=variables; var->type == SHOW_FUNC; var= &tmp)
      ((mysql_show_var_func)((st_show_var_func_container *)var->value)->func)(&tmp, buff);

    SHOW_TYPE show_type=var->type;
    if (show_type == SHOW_ARRAY)
    {
      show_status_array(session, wild, (SHOW_VAR *) var->value, value_type,
                        status_var, name_buffer, table, ucase_names);
    }
    else
    {
      if (!(wild && wild[0] && wild_case_compare(system_charset_info,
                                                 name_buffer, wild)))
      {
        char *value=var->value;
        const char *pos, *end;                  // We assign a lot of const's
        pthread_mutex_lock(&LOCK_global_system_variables);

        if (show_type == SHOW_SYS)
        {
          show_type= ((sys_var*) value)->show_type();
          value= (char*) ((sys_var*) value)->value_ptr(session, value_type,
                                                       &null_lex_str);
        }

        pos= end= buff;
        /*
          note that value may be == buff. All SHOW_xxx code below
          should still work in this case
        */
        switch (show_type) {
        case SHOW_DOUBLE_STATUS:
          value= ((char *) status_var + (ulong) value);
          /* fall through */
        case SHOW_DOUBLE:
          /* 6 is the default precision for '%f' in sprintf() */
          end= buff + my_fcvt(*(double *) value, 6, buff, NULL);
          break;
        case SHOW_LONG_STATUS:
          value= ((char *) status_var + (ulong) value);
          /* fall through */
        case SHOW_LONG:
          end= int10_to_str(*(long*) value, buff, 10);
          break;
        case SHOW_LONGLONG_STATUS:
          value= ((char *) status_var + (uint64_t) value);
          /* fall through */
        case SHOW_LONGLONG:
          end= int64_t10_to_str(*(int64_t*) value, buff, 10);
          break;
        case SHOW_SIZE:
          {
            stringstream ss (stringstream::in);
            ss << *(size_t*) value;

            string str= ss.str();
            strncpy(buff, str.c_str(), str.length());
            end= buff+ str.length();
          }
          break;
        case SHOW_HA_ROWS:
          end= int64_t10_to_str((int64_t) *(ha_rows*) value, buff, 10);
          break;
        case SHOW_BOOL:
          end+= sprintf(buff,"%s", *(bool*) value ? "ON" : "OFF");
          break;
        case SHOW_MY_BOOL:
          end+= sprintf(buff,"%s", *(bool*) value ? "ON" : "OFF");
          break;
        case SHOW_INT:
        case SHOW_INT_NOFLUSH: // the difference lies in refresh_status()
          end= int10_to_str((long) *(uint32_t*) value, buff, 10);
          break;
        case SHOW_HAVE:
        {
          SHOW_COMP_OPTION tmp_option= *(SHOW_COMP_OPTION *)value;
          pos= show_comp_option_name[(int) tmp_option];
          end= strchr(pos, '\0');
          break;
        }
        case SHOW_CHAR:
        {
          if (!(pos= value))
            pos= "";
          end= strchr(pos, '\0');
          break;
        }
       case SHOW_CHAR_PTR:
        {
          if (!(pos= *(char**) value))
            pos= "";
          end= strchr(pos, '\0');
          break;
        }
        case SHOW_KEY_CACHE_LONG:
          value= (char*) dflt_key_cache + (ulong)value;
          end= int10_to_str(*(long*) value, buff, 10);
          break;
        case SHOW_KEY_CACHE_LONGLONG:
          value= (char*) dflt_key_cache + (ulong)value;
	  end= int64_t10_to_str(*(int64_t*) value, buff, 10);
	  break;
        case SHOW_UNDEF:
          break;                                        // Return empty string
        case SHOW_SYS:                                  // Cannot happen
        default:
          assert(0);
          break;
        }
        table->restoreRecordAsDefault();
        table->field[0]->store(name_buffer, strlen(name_buffer),
                               system_charset_info);
        table->field[1]->store(pos, (uint32_t) (end - pos), system_charset_info);
        table->field[1]->set_notnull();

        pthread_mutex_unlock(&LOCK_global_system_variables);

        if (schema_table_store_record(session, table))
          return true;
      }
    }
  }

  return false;
}

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

int CharSetISMethods::oldFormat(Session *session, drizzled::plugin::InfoSchemaTable *schema_table)
  const
{
  int fields_arr[]= {0, 2, 1, 3, -1};
  int *field_num= fields_arr;
  const drizzled::plugin::InfoSchemaTable::Columns columns= schema_table->getColumns();
  const drizzled::plugin::ColumnInfo *column;
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
    if (! tmp_cs || ! (tmp_cs->state & MY_CS_AVAILABLE) ||
         (tmp_cs->state & MY_CS_HIDDEN) ||
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for (cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      const CHARSET_INFO *tmp_cl= cl[0];
      if (! tmp_cl || ! (tmp_cl->state & MY_CS_AVAILABLE) ||
          !my_charset_same(tmp_cs, tmp_cl))
        continue;
      if (! (wild && wild[0] &&
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
    if (! tmp_cs || ! (tmp_cs->state & MY_CS_AVAILABLE) ||
        ! (tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for (cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      const CHARSET_INFO *tmp_cl= cl[0];
      if (! tmp_cl || ! (tmp_cl->state & MY_CS_AVAILABLE) ||
          ! my_charset_same(tmp_cs,tmp_cl))
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

int ColumnsISMethods::oldFormat(Session *session, drizzled::plugin::InfoSchemaTable *schema_table)
  const
{
  int fields_arr[]= {3, 14, 13, 6, 15, 5, 16, 17, 18, -1};
  int *field_num= fields_arr;
  const drizzled::plugin::InfoSchemaTable::Columns columns= schema_table->getColumns();
  const drizzled::plugin::ColumnInfo *column;
  Name_resolution_context *context= &session->lex->select_lex.context;

  for (; *field_num >= 0; field_num++)
  {
    column= columns[*field_num];
    if (! session->lex->verbose && (*field_num == 13 ||
                                    *field_num == 17 ||
                                    *field_num == 18))
    {
      continue;
    }
    Item_field *field= new Item_field(context,
                                      NULL, NULL, column->getName().c_str());
    if (field)
    {
      field->set_name(column->getOldName().c_str(),
                      column->getOldName().length(),
                      system_charset_info);
      if (session->add_item_to_list(field))
      {
        return 1;
      }
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

inline bool open_list_store(Table *table, open_table_list_st& open_list);
inline bool open_list_store(Table *table, open_table_list_st& open_list)
{
  table->restoreRecordAsDefault();
  table->field[0]->store(open_list.db.c_str(), open_list.db.length(), system_charset_info);
  table->field[1]->store(open_list.table.c_str(), open_list.table.length(), system_charset_info);
  table->field[2]->store((int64_t) open_list.in_use, true);
  table->field[3]->store((int64_t) open_list.locked, true);
  if (schema_table_store_record(table->in_use, table))
    return true;

  return false;
}

int OpenTablesISMethods::fillTable(Session *session, TableList *tables, COND *)
{
  const char *wild= session->lex->wild ? session->lex->wild->ptr() : NULL;

  if ((list_open_tables(session->lex->select_lex.db, wild, open_list_store, tables->table) == true) && session->is_fatal_error)
    return 1;

  return 0;
}

class ShowPlugins : public unary_function<drizzled::plugin::Handle *, bool>
{
  Session *session;
  Table *table;
public:
  ShowPlugins(Session *session_arg, Table *table_arg)
    : session(session_arg), table(table_arg) {}

  result_type operator() (argument_type plugin)
  {
    const drizzled::plugin::Manifest &manifest= plugin->getManifest();
    const CHARSET_INFO * const cs= system_charset_info;

    table->restoreRecordAsDefault();

    table->field[0]->store(plugin->getName().c_str(),
                           plugin->getName().size(), cs);

    if (manifest.version)
    {
      table->field[1]->store(manifest.version, strlen(manifest.version), cs);
      table->field[1]->set_notnull();
    }
    else
      table->field[1]->set_null();

    if (plugin->isInited)
    {
      table->field[2]->store(STRING_WITH_LEN("ACTIVE"), cs);
    }
    else
    {
      table->field[2]->store(STRING_WITH_LEN("INACTIVE"), cs);
    }

    if (manifest.author)
    {
      table->field[3]->store(manifest.author, strlen(manifest.author), cs);
      table->field[3]->set_notnull();
    }
    else
    {
      table->field[3]->set_null();
    }

    if (manifest.descr)
    {
      table->field[4]->store(manifest.descr, strlen(manifest.descr), cs);
      table->field[4]->set_notnull();
    }
    else
    {
      table->field[4]->set_null();
    }

    switch (manifest.license) {
    case PLUGIN_LICENSE_GPL:
      table->field[5]->store(drizzled::plugin::LICENSE_GPL_STRING.c_str(),
                             drizzled::plugin::LICENSE_GPL_STRING.size(), cs);
      break;
    case PLUGIN_LICENSE_BSD:
      table->field[5]->store(drizzled::plugin::LICENSE_BSD_STRING.c_str(),
                             drizzled::plugin::LICENSE_BSD_STRING.size(), cs);
      break;
    case PLUGIN_LICENSE_LGPL:
      table->field[5]->store(drizzled::plugin::LICENSE_LGPL_STRING.c_str(),
                             drizzled::plugin::LICENSE_LGPL_STRING.size(), cs);
      break;
    default:
      table->field[5]->store(drizzled::plugin::LICENSE_PROPRIETARY_STRING.c_str(),
                             drizzled::plugin::LICENSE_PROPRIETARY_STRING.size(),
                             cs);
      break;
    }
    table->field[5]->set_notnull();

    return schema_table_store_record(session, table);
  }
};

int PluginsISMethods::fillTable(Session *session, TableList *tables, COND *)
{
  Table *table= tables->table;

  drizzled::plugin::Registry &registry= drizzled::plugin::Registry::singleton();
  vector<drizzled::plugin::Handle *> plugins= registry.getList(true);
  vector<drizzled::plugin::Handle *>::iterator iter=
    find_if(plugins.begin(), plugins.end(), ShowPlugins(session, table));
  if (iter != plugins.end())
  {
    return 1;
  }
  return (0);
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

  if (! session->killed)
  {
    Session* tmp;

    for (vector<Session*>::iterator it= session_list.begin(); it != session_list.end(); ++it)
    {
      tmp= *it;
      Security_context *tmp_sctx= &tmp->security_ctx;
      struct st_my_thread_var *mysys_var;
      const char *val;

      if (! tmp->client->isConnected())
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

static bool store_schema_schemata(Session* session, Table *table, LEX_STRING *db_name,
                                  const CHARSET_INFO * const cs)
{
  table->restoreRecordAsDefault();
  table->field[1]->store(db_name->str, db_name->length, system_charset_info);
  table->field[2]->store(cs->csname, strlen(cs->csname), system_charset_info);
  table->field[3]->store(cs->name, strlen(cs->name), system_charset_info);
  return schema_table_store_record(session, table);
}

int SchemataISMethods::fillTable(Session *session, TableList *tables, COND *cond)
{
  /*
    TODO: fill_schema_shemata() is called when new client is connected.
    Returning error status in this case leads to client hangup.
  */

  LOOKUP_FIELD_VALUES lookup_field_vals;
  vector<LEX_STRING*> db_names;
  bool with_i_schema;
  Table *table= tables->table;

  if (get_lookup_field_values(session, cond, tables, &lookup_field_vals))
    return(0);
  if (make_db_list(session, db_names, &lookup_field_vals,
                   &with_i_schema))
    return(1);

  /*
    If we have lookup db value we should check that the database exists
  */
  if(lookup_field_vals.db_value.str && !lookup_field_vals.wild_db_value &&
     !with_i_schema)
  {
    char path[FN_REFLEN+16];
    uint32_t path_len;
    struct stat stat_info;
    if (!lookup_field_vals.db_value.str[0])
      return(0);
    path_len= build_table_filename(path, sizeof(path),
                                   lookup_field_vals.db_value.str, "", false);
    path[path_len-1]= 0;
    if (stat(path,&stat_info))
      return(0);
  }

  vector<LEX_STRING*>::iterator db_name= db_names.begin();
  while (db_name != db_names.end())
  {
    if (with_i_schema)       // information schema name is always first in list
    {
      if (store_schema_schemata(session, table, *db_name, system_charset_info))
        return(1);

      with_i_schema= 0;
    }
    else
    {
      const CHARSET_INFO *cs= get_default_db_collation((*db_name)->str);

      if (store_schema_schemata(session, table, *db_name, cs))
        return(1);
    }

    ++db_name;
  }
  return(0);
}

int SchemataISMethods::oldFormat(Session *session, drizzled::plugin::InfoSchemaTable *schema_table)
  const
{
  char tmp[128];
  LEX *lex= session->lex;
  Select_Lex *sel= lex->current_select;
  Name_resolution_context *context= &sel->context;
  const drizzled::plugin::InfoSchemaTable::Columns columns= schema_table->getColumns();

  if (!sel->item_list.elements)
  {
    const drizzled::plugin::ColumnInfo *column= columns[1];
    String buffer(tmp,sizeof(tmp), system_charset_info);
    Item_field *field= new Item_field(context,
                                      NULL, NULL, column->getName().c_str());
    if (!field || session->add_item_to_list(field))
      return 1;
    buffer.length(0);
    buffer.append(column->getOldName().c_str());
    if (lex->wild && lex->wild->ptr())
    {
      buffer.append(STRING_WITH_LEN(" ("));
      buffer.append(lex->wild->ptr());
      buffer.append(')');
    }
    field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
  }
  return 0;
}

int StatsISMethods::processTable(Session *session, TableList *tables,
                                  Table *table, bool res,
                                  LEX_STRING *db_name,
                                  LEX_STRING *table_name) const
{
  const CHARSET_INFO * const cs= system_charset_info;
  if (res)
  {
    if (session->lex->sql_command != SQLCOM_SHOW_KEYS)
    {
      /*
        I.e. we are in SELECT FROM INFORMATION_SCHEMA.STATISTICS
        rather than in SHOW KEYS
      */
      if (session->is_error())
      {
        push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                     session->main_da.sql_errno(), session->main_da.message());
      }
      session->clear_error();
      res= 0;
    }
    return (res);
  }
  else
  {
    Table *show_table= tables->table;
    KEY *key_info=show_table->s->key_info;
    if (show_table->file)
    {
      show_table->file->info(HA_STATUS_VARIABLE |
                             HA_STATUS_NO_LOCK |
                             HA_STATUS_TIME);
    }
    for (uint32_t i=0 ; i < show_table->s->keys ; i++,key_info++)
    {
      KEY_PART_INFO *key_part= key_info->key_part;
      const char *str;
      for (uint32_t j=0 ; j < key_info->key_parts ; j++,key_part++)
      {
        table->restoreRecordAsDefault();
        table->field[1]->store(db_name->str, db_name->length, cs);
        table->field[2]->store(table_name->str, table_name->length, cs);
        table->field[3]->store((int64_t) ((key_info->flags &
                                            HA_NOSAME) ? 0 : 1), true);
        table->field[4]->store(db_name->str, db_name->length, cs);
        table->field[5]->store(key_info->name, strlen(key_info->name), cs);
        table->field[6]->store((int64_t) (j+1), true);
        str=(key_part->field ? key_part->field->field_name :
             "?unknown field?");
        table->field[7]->store(str, strlen(str), cs);
        if (show_table->file)
        {
          if (show_table->file->index_flags(i, j, 0) & HA_READ_ORDER)
          {
            table->field[8]->store(((key_part->key_part_flag &
                                     HA_REVERSE_SORT) ?
                                    "D" : "A"), 1, cs);
            table->field[8]->set_notnull();
          }
          KEY *key=show_table->key_info+i;
          if (key->rec_per_key[j])
          {
            ha_rows records=(show_table->file->stats.records /
                             key->rec_per_key[j]);
            table->field[9]->store((int64_t) records, true);
            table->field[9]->set_notnull();
          }
          str= show_table->file->index_type(i);
          table->field[13]->store(str, strlen(str), cs);
        }
        if ((key_part->field &&
             key_part->length !=
             show_table->s->field[key_part->fieldnr-1]->key_length()))
        {
          table->field[10]->store((int64_t) key_part->length /
                                  key_part->field->charset()->mbmaxlen, true);
          table->field[10]->set_notnull();
        }
        uint32_t flags= key_part->field ? key_part->field->flags : 0;
        const char *pos=(char*) ((flags & NOT_NULL_FLAG) ? "" : "YES");
        table->field[12]->store(pos, strlen(pos), cs);
        if (!show_table->s->keys_in_use.test(i))
        {
          table->field[14]->store(STRING_WITH_LEN("disabled"), cs);
        }
        else
        {
          table->field[14]->store("", 0, cs);
        }
        table->field[14]->set_notnull();
        assert(test(key_info->flags & HA_USES_COMMENT) ==
                   (key_info->comment.length > 0));
        if (key_info->flags & HA_USES_COMMENT)
        {
          table->field[15]->store(key_info->comment.str,
                                  key_info->comment.length, cs);
        }
        if (schema_table_store_record(session, table))
        {
          return (1);
        }
      }
    }
  }
  return(res);
}

int StatusISMethods::fillTable(Session *session, TableList *tables, COND *)
{
  LEX *lex= session->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NULL;
  int res= 0;
  STATUS_VAR *tmp1, tmp;
  const string schema_table_name= tables->schema_table->getTableName();
  enum enum_var_type option_type;
  bool upper_case_names= (schema_table_name.compare("STATUS") != 0);

  if (schema_table_name.compare("STATUS") == 0)
  {
    option_type= lex->option_type;
    if (option_type == OPT_GLOBAL)
      tmp1= &tmp;
    else
      tmp1= session->initial_status_var;
  }
  else if (schema_table_name.compare("GLOBAL_STATUS") == 0)
  {
    option_type= OPT_GLOBAL;
    tmp1= &tmp;
  }
  else
  {
    option_type= OPT_SESSION;
    tmp1= &session->status_var;
  }

  pthread_mutex_lock(&LOCK_status);
  if (option_type == OPT_GLOBAL)
    calc_sum_of_all_status(&tmp);
  res= show_status_array(session, wild,
                         getFrontOfStatusVars(),
                         option_type, tmp1, "", tables->table,
                         upper_case_names);
  pthread_mutex_unlock(&LOCK_status);
  return(res);
}

static bool store_constraints(Session *session, Table *table, LEX_STRING *db_name,
                              LEX_STRING *table_name, const char *key_name,
                              uint32_t key_len, const char *con_type, uint32_t con_len)
{
  const CHARSET_INFO * const cs= system_charset_info;
  table->restoreRecordAsDefault();
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(key_name, key_len, cs);
  table->field[3]->store(db_name->str, db_name->length, cs);
  table->field[4]->store(table_name->str, table_name->length, cs);
  table->field[5]->store(con_type, con_len, cs);
  return schema_table_store_record(session, table);
}

int TabConstraintsISMethods::processTable(Session *session, TableList *tables,
                                          Table *table, bool res,
                                          LEX_STRING *db_name,
                                          LEX_STRING *table_name) const
{
  if (res)
  {
    if (session->is_error())
    {
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                   session->main_da.sql_errno(), session->main_da.message());
    }
    session->clear_error();
    return (0);
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

      if (i == primary_key && is_primary_key(key_info))
      {
        if (store_constraints(session, table, db_name, table_name, key_info->name,
                              strlen(key_info->name),
                              STRING_WITH_LEN("PRIMARY KEY")))
        {
          return (1);
        }
      }
      else if (key_info->flags & HA_NOSAME)
      {
        if (store_constraints(session, table, db_name, table_name, key_info->name,
                              strlen(key_info->name),
                              STRING_WITH_LEN("UNIQUE")))
        {
          return (1);
        }
      }
    }

    show_table->file->get_foreign_key_list(session, &f_key_list);
    FOREIGN_KEY_INFO *f_key_info;
    List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);
    while ((f_key_info=it++))
    {
      if (store_constraints(session, table, db_name, table_name,
                            f_key_info->forein_id->str,
                            strlen(f_key_info->forein_id->str),
                            "FOREIGN KEY", 11))
      {
        return (1);
      }
    }
  }
  return (res);
}

int TablesISMethods::processTable(Session *session, TableList *tables,
                                    Table *table, bool res,
                                    LEX_STRING *db_name,
                                    LEX_STRING *table_name) const
{
  const char *tmp_buff;
  DRIZZLE_TIME time;
  const CHARSET_INFO * const cs= system_charset_info;

  table->restoreRecordAsDefault();
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(table_name->str, table_name->length, cs);
  if (res)
  {
    /*
      there was errors during opening tables
    */
    const char *error= session->is_error() ? session->main_da.message() : "";
    if (tables->schema_table)
    {
      table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"), cs);
    }
    else
    {
      table->field[3]->store(STRING_WITH_LEN("BASE Table"), cs);
    }
    table->field[20]->store(error, strlen(error), cs);
    session->clear_error();
  }
  else
  {
    char option_buff[400],*ptr;
    Table *show_table= tables->table;
    TableShare *share= show_table->s;
    handler *file= show_table->file;
    drizzled::plugin::StorageEngine *tmp_db_type= share->db_type();

    if (share->tmp_table == SYSTEM_TMP_TABLE)
    {
      table->field[3]->store(STRING_WITH_LEN("SYSTEM VIEW"), cs);
    }
    else if (share->tmp_table)
    {
      table->field[3]->store(STRING_WITH_LEN("LOCAL TEMPORARY"), cs);
    }
    else
    {
      table->field[3]->store(STRING_WITH_LEN("BASE Table"), cs);
    }

    for (int i= 4; i < 20; i++)
    {
      if (i == 7 || (i > 12 && i < 17) || i == 18)
      {
        continue;
      }
      table->field[i]->set_notnull();
    }
    const string &engine_name= drizzled::plugin::StorageEngine::resolveName(tmp_db_type);
    table->field[4]->store(engine_name.c_str(), engine_name.size(), cs);
    table->field[5]->store((int64_t) 0, true);

    ptr=option_buff;

    if (share->db_create_options & HA_OPTION_PACK_KEYS)
    {
      ptr= strcpy(ptr," pack_keys=1")+12;
    }
    if (share->db_create_options & HA_OPTION_NO_PACK_KEYS)
    {
      ptr= strcpy(ptr," pack_keys=0")+12;
    }
    if (share->row_type != ROW_TYPE_DEFAULT)
    {
      ptr+= sprintf(ptr, " row_format=%s", ha_row_type[(uint32_t)share->row_type]);
    }
    if (share->block_size)
    {
      ptr= strcpy(ptr, " block_size=")+12;
      ptr= int64_t10_to_str(share->block_size, ptr, 10);
    }

    table->field[19]->store(option_buff+1,
                            (ptr == option_buff ? 0 :
                             (uint32_t) (ptr-option_buff)-1), cs);

    tmp_buff= (share->table_charset ?
               share->table_charset->name : "default");
    table->field[17]->store(tmp_buff, strlen(tmp_buff), cs);

    if (share->hasComment())
      table->field[20]->store(share->getComment(),
                              share->getCommentLength(), cs);

    if(file)
    {
      file->info(HA_STATUS_VARIABLE | HA_STATUS_TIME | HA_STATUS_AUTO |
                 HA_STATUS_NO_LOCK);
      enum row_type row_type = file->get_row_type();
      switch (row_type) {
      case ROW_TYPE_NOT_USED:
      case ROW_TYPE_DEFAULT:
        tmp_buff= ((share->db_options_in_use &
                    HA_OPTION_COMPRESS_RECORD) ? "Compressed" :
                   (share->db_options_in_use & HA_OPTION_PACK_RECORD) ?
                   "Dynamic" : "Fixed");
        break;
      case ROW_TYPE_FIXED:
        tmp_buff= "Fixed";
        break;
      case ROW_TYPE_DYNAMIC:
        tmp_buff= "Dynamic";
        break;
      case ROW_TYPE_COMPRESSED:
        tmp_buff= "Compressed";
        break;
      case ROW_TYPE_REDUNDANT:
        tmp_buff= "Redundant";
        break;
      case ROW_TYPE_COMPACT:
        tmp_buff= "Compact";
        break;
      case ROW_TYPE_PAGE:
        tmp_buff= "Paged";
        break;
      }
      table->field[6]->store(tmp_buff, strlen(tmp_buff), cs);
      if (! tables->schema_table)
      {
        table->field[7]->store((int64_t) file->stats.records, true);
        table->field[7]->set_notnull();
      }
      table->field[8]->store((int64_t) file->stats.mean_rec_length, true);
      table->field[9]->store((int64_t) file->stats.data_file_length, true);
      if (file->stats.max_data_file_length)
      {
        table->field[10]->store((int64_t) file->stats.max_data_file_length,
                                true);
      }
      table->field[11]->store((int64_t) file->stats.index_file_length, true);
      table->field[12]->store((int64_t) file->stats.delete_length, true);
      if (show_table->found_next_number_field)
      {
        table->field[13]->store((int64_t) file->stats.auto_increment_value,
                                true);
        table->field[13]->set_notnull();
      }
      if (file->stats.create_time)
      {
        session->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (time_t) file->stats.create_time);
        table->field[14]->store_time(&time, DRIZZLE_TIMESTAMP_DATETIME);
        table->field[14]->set_notnull();
      }
      if (file->stats.update_time)
      {
        session->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (time_t) file->stats.update_time);
        table->field[15]->store_time(&time, DRIZZLE_TIMESTAMP_DATETIME);
        table->field[15]->set_notnull();
      }
      if (file->stats.check_time)
      {
        session->variables.time_zone->gmt_sec_to_TIME(&time,
                                                  (time_t) file->stats.check_time);
        table->field[16]->store_time(&time, DRIZZLE_TIMESTAMP_DATETIME);
        table->field[16]->set_notnull();
      }
      if (file->ha_table_flags() & (ulong) HA_HAS_CHECKSUM)
      {
        table->field[18]->store((int64_t) file->checksum(), true);
        table->field[18]->set_notnull();
      }
    }
  }
  return (schema_table_store_record(session, table));
}


int TabNamesISMethods::oldFormat(Session *session, drizzled::plugin::InfoSchemaTable *schema_table)
  const
{
  char tmp[128];
  String buffer(tmp,sizeof(tmp), session->charset());
  LEX *lex= session->lex;
  Name_resolution_context *context= &lex->select_lex.context;
  const drizzled::plugin::InfoSchemaTable::Columns columns= schema_table->getColumns();

  const drizzled::plugin::ColumnInfo *column= columns[2];
  buffer.length(0);
  buffer.append(column->getOldName().c_str());
  buffer.append(lex->select_lex.db);
  if (lex->wild && lex->wild->ptr())
  {
    buffer.append(STRING_WITH_LEN(" ("));
    buffer.append(lex->wild->ptr());
    buffer.append(')');
  }
  Item_field *field= new Item_field(context,
                                    NULL, NULL, column->getName().c_str());
  if (session->add_item_to_list(field))
  {
    return 1;
  }
  field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
  if (session->lex->verbose)
  {
    field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
    column= columns[3];
    field= new Item_field(context, NULL, NULL, column->getName().c_str());
    if (session->add_item_to_list(field))
    {
      return 1;
    }
    field->set_name(column->getOldName().c_str(),
                    column->getOldName().length(),
                    system_charset_info);
  }
  return 0;
}

int VariablesISMethods::fillTable(Session *session, TableList *tables, COND *)
{
  int res= 0;
  LEX *lex= session->lex;
  const char *wild= lex->wild ? lex->wild->ptr() : NULL;
  const string schema_table_name= tables->schema_table->getTableName();
  enum enum_var_type option_type= OPT_SESSION;
  bool upper_case_names= (schema_table_name.compare("VARIABLES") != 0);
  bool sorted_vars= (schema_table_name.compare("VARIABLES") == 0);

  if (lex->option_type == OPT_GLOBAL ||
      schema_table_name.compare("GLOBAL_VARIABLES") == 0)
  {
    option_type= OPT_GLOBAL;
  }

  pthread_rwlock_rdlock(&LOCK_system_variables_hash);
  res= show_status_array(session, wild, enumerate_sys_vars(session, sorted_vars),
                         option_type, NULL, "", tables->table, upper_case_names);
  pthread_rwlock_unlock(&LOCK_system_variables_hash);
  return(res);
}

