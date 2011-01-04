/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef DRIZZLED_SQL_PARSE_H
#define DRIZZLED_SQL_PARSE_H

#include "drizzled/definitions.h"
#include "drizzled/common.h"
#include "drizzled/lex_string.h"
#include "drizzled/comp_creator.h"
#include "drizzled/identifier.h"

namespace drizzled
{

class Session;
class TableList;
class Lex_input_stream;
class Item;
class LEX;
class Table_ident;
class Select_Lex;

typedef struct charset_info_st CHARSET_INFO;

extern const LEX_STRING command_name[];

bool execute_sqlcom_select(Session *session, TableList *all_tables);
bool insert_select_prepare(Session *session);
bool update_precheck(Session *session, TableList *tables);
bool delete_precheck(Session *session, TableList *tables);
bool insert_precheck(Session *session, TableList *tables);

Item *negate_expression(Session *session, Item *expr);

bool check_identifier_name(LEX_STRING *str, drizzled_error_code err_code= EE_OK,
                           uint32_t max_char_length= NAME_CHAR_LEN,
                           const char *param_for_err_msg= "");

bool check_string_byte_length(LEX_STRING *str, const char *err_msg,
                              uint32_t max_byte_length);
bool check_string_char_length(LEX_STRING *str, const char *err_msg,
                              uint32_t max_char_length, const CHARSET_INFO * const cs,
                              bool no_error);


bool test_parse_for_slave(Session *session, char *inBuf,
                                uint32_t length);

void reset_session_for_next_command(Session *session);

void create_select_for_variable(const char *var_name);

void init_update_queries(void);

bool dispatch_command(enum enum_server_command command, Session *session,
                      char* packet, uint32_t packet_length);

bool check_simple_select(Session* session);

void init_select(LEX *lex);
bool new_select(LEX *lex, bool move_down);

int prepare_new_schema_table(Session *session, LEX *lex,
                             const std::string& schema_table_name);

Item * all_any_subquery_creator(Item *left_expr,
                                chooser_compare_func_creator cmp,
                                bool all,
                                Select_Lex *select_lex);

char* query_table_status(Session *session,const char *db,const char *table_name);

} /* namespace drizzled */

#endif /* DRIZZLED_SQL_PARSE_H */
