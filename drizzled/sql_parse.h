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

#ifndef DRIZZLE_SERVER_SQL_PARSE_H
#define DRIZZLE_SERVER_SQL_PARSE_H

bool begin_trans(THD *thd);
bool end_active_trans(THD *thd);
int end_trans(THD *thd, enum enum_mysql_completiontype completion);

bool execute_sqlcom_select(THD *thd, TableList *all_tables);
bool multi_update_precheck(THD *thd, TableList *tables);
bool multi_delete_precheck(THD *thd, TableList *tables);
int mysql_multi_update_prepare(THD *thd);
int mysql_multi_delete_prepare(THD *thd);
bool mysql_insert_select_prepare(THD *thd);
bool update_precheck(THD *thd, TableList *tables);
bool delete_precheck(THD *thd, TableList *tables);
bool insert_precheck(THD *thd, TableList *tables);
bool create_table_precheck(THD *thd, TableList *tables,
                           TableList *create_table);
bool parse_sql(THD *thd, class Lex_input_stream *lip);

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
