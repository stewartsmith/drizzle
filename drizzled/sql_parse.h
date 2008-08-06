/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DRIZZLE_SERVER_SQL_PARSE_H
#define DRIZZLE_SERVER_SQL_PARSE_H

bool begin_trans(THD *thd);
bool end_active_trans(THD *thd);
int end_trans(THD *thd, enum enum_mysql_completiontype completion);

bool execute_sqlcom_select(THD *thd, TABLE_LIST *all_tables);
bool multi_update_precheck(THD *thd, TABLE_LIST *tables);
bool multi_delete_precheck(THD *thd, TABLE_LIST *tables);
int mysql_multi_update_prepare(THD *thd);
int mysql_multi_delete_prepare(THD *thd);
bool mysql_insert_select_prepare(THD *thd);
bool update_precheck(THD *thd, TABLE_LIST *tables);
bool delete_precheck(THD *thd, TABLE_LIST *tables);
bool insert_precheck(THD *thd, TABLE_LIST *tables);
bool create_table_precheck(THD *thd, TABLE_LIST *tables,
                           TABLE_LIST *create_table);
bool parse_sql(THD *thd,
               class Lex_input_stream *lip,
               class Object_creation_ctx *creation_ctx);

Item *negate_expression(THD *thd, Item *expr);

bool test_if_data_home_dir(const char *dir);

bool check_identifier_name(LEX_STRING *str, uint max_char_length,
                           uint err_code, const char *param_for_err_msg);
inline bool check_identifier_name(LEX_STRING *str, uint err_code)
{
  return check_identifier_name(str, NAME_CHAR_LEN, err_code, "");
}
inline bool check_identifier_name(LEX_STRING *str)
{
  return check_identifier_name(str, NAME_CHAR_LEN, 0, "");
}

bool check_string_byte_length(LEX_STRING *str, const char *err_msg,
                              uint max_byte_length);
bool check_string_char_length(LEX_STRING *str, const char *err_msg,
                              uint max_char_length, const CHARSET_INFO * const cs,
                              bool no_error);

#endif /* DRIZZLE_SERVER_SQL_PARSE_H */
