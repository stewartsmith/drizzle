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

bool begin_trans(Session *session);
bool end_active_trans(Session *session);
int end_trans(Session *session, enum enum_mysql_completiontype completion);

bool execute_sqlcom_select(Session *session, TableList *all_tables);
bool multi_update_precheck(Session *session, TableList *tables);
bool multi_delete_precheck(Session *session, TableList *tables);
int mysql_multi_update_prepare(Session *session);
int mysql_multi_delete_prepare(Session *session);
bool mysql_insert_select_prepare(Session *session);
bool update_precheck(Session *session, TableList *tables);
bool delete_precheck(Session *session, TableList *tables);
bool insert_precheck(Session *session, TableList *tables);
bool create_table_precheck(Session *session, TableList *tables,
                           TableList *create_table);
bool parse_sql(Session *session, class Lex_input_stream *lip);

Item *negate_expression(Session *session, Item *expr);

bool test_if_data_home_dir(const char *dir);

bool check_identifier_name(LEX_STRING *str, uint32_t max_char_length,
                           uint32_t err_code, const char *param_for_err_msg);
inline bool check_identifier_name(LEX_STRING *str, uint32_t err_code)
{
  return check_identifier_name(str, NAME_CHAR_LEN, err_code, "");
}
inline bool check_identifier_name(LEX_STRING *str)
{
  return check_identifier_name(str, NAME_CHAR_LEN, 0, "");
}

bool check_string_byte_length(LEX_STRING *str, const char *err_msg,
                              uint32_t max_byte_length);
bool check_string_char_length(LEX_STRING *str, const char *err_msg,
                              uint32_t max_char_length, const CHARSET_INFO * const cs,
                              bool no_error);

#endif /* DRIZZLE_SERVER_SQL_PARSE_H */
