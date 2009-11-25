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
 *   Implementation of helper methods for I_S tables.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/sql_base.h"
#include "drizzled/plugin/client.h"
#include "drizzled/join_table.h"

#include "helper_methods.h"

#include <vector>
#include <string>

using namespace std;
using namespace drizzled;

static inline void make_upper(char *buf)
{
  for (; *buf; buf++)
    *buf= my_toupper(system_charset_info, *buf);
}

bool show_status_array(Session *session, 
                       const char *wild,
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
        table->setWriteSet(0);
        table->setWriteSet(1);
        table->setWriteSet(2);
        table->field[0]->store(name_buffer, strlen(name_buffer),
                               system_charset_info);
        table->field[1]->store(pos, (uint32_t) (end - pos), system_charset_info);
        table->field[1]->set_notnull();

        pthread_mutex_unlock(&LOCK_global_system_variables);

        TableList *tmp_tbl_list= table->pos_in_table_list;
        tmp_tbl_list->schema_table->addRow(table->record[0], table->s->reclength);
      }
    }
  }

  return false;
}

void store_key_column_usage(Table *table, 
                            LEX_STRING *db_name,
                            LEX_STRING *table_name, 
                            const char *key_name,
                            uint32_t key_len, 
                            const char *con_type, 
                            uint32_t con_len,
                            int64_t idx)
{
  const CHARSET_INFO * const cs= system_charset_info;
  /* set the appropriate bits in the write bitset */
  table->setWriteSet(1);
  table->setWriteSet(2);
  table->setWriteSet(4);
  table->setWriteSet(5);
  table->setWriteSet(6);
  table->setWriteSet(7);
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(key_name, key_len, cs);
  table->field[4]->store(db_name->str, db_name->length, cs);
  table->field[5]->store(table_name->str, table_name->length, cs);
  table->field[6]->store(con_type, con_len, cs);
  table->field[7]->store((int64_t) idx, true);
}

/*
 * Function object used for deleting the memory allocated
 * for the columns contained with the vector of columns.
 */
class DeleteColumns
{
public:
  template<typename T>
  inline void operator()(const T *ptr) const
  {
    delete ptr;
  }
};

void clearColumns(vector<const drizzled::plugin::ColumnInfo *> &cols)
{
  for_each(cols.begin(), cols.end(), DeleteColumns());
  cols.clear();
}
