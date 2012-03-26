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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#pragma once

#include <drizzled/charset.h>
#include <drizzled/error.h>
#include <drizzled/foreign_key.h>
#include <drizzled/function/bit/functions.h>
#include <drizzled/function/get_system_var.h>
#include <drizzled/function/locate.h>
#include <drizzled/function/set_user_var.h>
#include <drizzled/function/str/char.h>
#include <drizzled/function/str/collation.h>
#include <drizzled/function/str/concat.h>
#include <drizzled/function/str/insert.h>
#include <drizzled/function/str/left.h>
#include <drizzled/function/str/repeat.h>
#include <drizzled/function/str/replace.h>
#include <drizzled/function/str/right.h>
#include <drizzled/function/str/set_collation.h>
#include <drizzled/function/str/trim.h>
#include <drizzled/function/time/curdate.h>
#include <drizzled/function/time/curtime.h>
#include <drizzled/function/time/date_add_interval.h>
#include <drizzled/function/time/dayofmonth.h>
#include <drizzled/function/time/extract.h>
#include <drizzled/function/time/hour.h>
#include <drizzled/function/time/microsecond.h>
#include <drizzled/function/time/minute.h>
#include <drizzled/function/time/month.h>
#include <drizzled/function/time/now.h>
#include <drizzled/function/time/quarter.h>
#include <drizzled/function/time/second.h>
#include <drizzled/function/time/sysdate_local.h>
#include <drizzled/function/time/timestamp_diff.h>
#include <drizzled/function/time/typecast.h>
#include <drizzled/function/time/year.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/item/boolean.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/copy_string.h>
#include <drizzled/item/create.h>
#include <drizzled/item/default_value.h>
#include <drizzled/item/func.h>
#include <drizzled/item/insert_value.h>
#include <drizzled/item/null.h>
#include <drizzled/item/uint.h>
#include <drizzled/lex_string.h>
#include <drizzled/lex_symbol.h>
#include <drizzled/message/schema.pb.h>
#include <drizzled/message/table.pb.h>
#include <drizzled/nested_join.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/select_dump.h>
#include <drizzled/select_dumpvar.h>
#include <drizzled/select_export.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_parse.h>
#include <drizzled/statement.h>
#include <drizzled/statement/alter_schema.h>
#include <drizzled/statement/alter_table.h>
#include <drizzled/statement/analyze.h>
#include <drizzled/statement/catalog.h>
#include <drizzled/statement/change_schema.h>
#include <drizzled/statement/check.h>
#include <drizzled/statement/commit.h>
#include <drizzled/statement/create_index.h>
#include <drizzled/statement/create_schema.h>
#include <drizzled/statement/create_table.h>
#include <drizzled/statement/delete.h>
#include <drizzled/statement/drop_index.h>
#include <drizzled/statement/drop_schema.h>
#include <drizzled/statement/drop_table.h>
#include <drizzled/statement/empty_query.h>
#include <drizzled/statement/execute.h>
#include <drizzled/statement/flush.h>
#include <drizzled/statement/insert.h>
#include <drizzled/statement/insert_select.h>
#include <drizzled/statement/kill.h>
#include <drizzled/statement/load.h>
#include <drizzled/statement/release_savepoint.h>
#include <drizzled/statement/rename_table.h>
#include <drizzled/statement/replace.h>
#include <drizzled/statement/replace_select.h>
#include <drizzled/statement/rollback.h>
#include <drizzled/statement/rollback_to_savepoint.h>
#include <drizzled/statement/savepoint.h>
#include <drizzled/statement/select.h>
#include <drizzled/statement/set_option.h>
#include <drizzled/statement/show.h>
#include <drizzled/statement/show_errors.h>
#include <drizzled/statement/show_warnings.h>
#include <drizzled/statement/start_transaction.h>
#include <drizzled/statement/truncate.h>
#include <drizzled/statement/unlock_tables.h>
#include <drizzled/statement/update.h>

namespace drizzled {
namespace parser {

Item* handle_sql2003_note184_exception(Session*, Item* left, bool equal, Item *expr);
bool add_select_to_union_list(Session*, LEX*, bool is_union_distinct);
bool setup_select_in_parentheses(Session*, LEX*);
Item* reserved_keyword_function(Session*, const std::string &name, List<Item> *item_list);
void my_parse_error(Lex_input_stream*);
void my_parse_error(const char*);
bool check_reserved_words(str_ref);
void errorOn(Session*, const char *s);


bool buildOrderBy(LEX*);
void buildEngineOption(LEX*, const char *key, str_ref value);
void buildEngineOption(LEX*, const char *key, uint64_t value);
void buildSchemaOption(LEX*, const char *key, str_ref value);
void buildSchemaOption(LEX*, const char *key, uint64_t value);
void buildSchemaDefiner(LEX*, const identifier::User&);
bool checkFieldIdent(LEX*, str_ref schema_name, str_ref table_name);

Item *buildIdent(LEX*, str_ref schema_name, str_ref table_name, str_ref field_name);
Item *buildTableWild(LEX*, str_ref schema_name, str_ref table_name);

void buildCreateFieldIdent(LEX*);
void storeAlterColumnPosition(LEX*, const char *position);

bool buildCollation(LEX*, const charset_info_st *arg);
void buildKey(LEX*, Key::Keytype type_par, str_ref name_arg);
void buildForeignKey(LEX*, str_ref name_arg, Table_ident *table);

enum_field_types buildIntegerColumn(LEX*, enum_field_types final_type, const bool is_unsigned);
enum_field_types buildSerialColumn(LEX*);
enum_field_types buildVarcharColumn(LEX*, const char *length);
enum_field_types buildVarbinaryColumn(LEX*, const char *length);
enum_field_types buildBlobColumn(LEX*);
enum_field_types buildBooleanColumn(LEX*);
enum_field_types buildUuidColumn(LEX*);
enum_field_types buildIPv6Column(LEX*);
enum_field_types buildDoubleColumn(LEX*);
enum_field_types buildTimestampColumn(LEX*, const char *length);
enum_field_types buildDecimalColumn(LEX*);

void buildKeyOnColumn(LEX*);
void buildAutoOnColumn(LEX*);
void buildPrimaryOnColumn(LEX*);
void buildReplicationOption(LEX*, bool arg);
void buildAddAlterDropIndex(LEX*, const char *name, bool is_foreign_key= false);

} // namespace parser
} // namespace drizzled

