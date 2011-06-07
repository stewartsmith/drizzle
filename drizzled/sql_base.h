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



#pragma once

#include <drizzled/table.h>
#include <drizzled/table_list.h>
#include <drizzled/table/concurrent.h>

#include <drizzled/visibility.h>

namespace drizzled {

void table_cache_free();

table::Cache &get_open_cache();

DRIZZLED_API void kill_drizzle();

/* sql_base.cc */
void set_item_name(Item *item,char *pos,uint32_t length);
bool add_field_to_list(Session *session, LEX_STRING *field_name, enum enum_field_types type,
                       char *length, char *decimal,
                       uint32_t type_modifier,
                       enum column_format_type column_format,
                       Item *default_value, Item *on_update_value,
                       LEX_STRING *comment,
                       char *change, List<String> *interval_list,
                       const charset_info_st * const cs);
CreateField * new_create_field(Session *session, char *field_name, enum_field_types type,
                               char *length, char *decimals,
                               uint32_t type_modifier,
                               Item *default_value, Item *on_update_value,
                               LEX_STRING *comment, char *change,
                               List<String> *interval_list, charset_info_st *cs);
void push_new_name_resolution_context(Session&, TableList& left_op, TableList& right_op);
void add_join_on(TableList *b,Item *expr);
void add_join_natural(TableList *a,TableList *b,List<String> *using_fields,
                      Select_Lex *lex);
extern Item **not_found_item;

/**
  A set of constants used for checking non aggregated fields and sum
  functions mixture in the ONLY_FULL_GROUP_BY_MODE.
*/
enum enum_group_by_mode_type
{
  NON_AGG_FIELD_USED= 0,
  SUM_FUNC_USED
};

/**
  This enumeration type is used only by the function find_item_in_list
  to return the info on how an item has been resolved against a list
  of possibly aliased items.
  The item can be resolved:
   - against an alias name of the list's element (RESOLVED_AGAINST_ALIAS)
   - against non-aliased field name of the list  (RESOLVED_WITH_NO_ALIAS)
   - against an aliased field name of the list   (RESOLVED_BEHIND_ALIAS)
   - ignoring the alias name in cases when SQL requires to ignore aliases
     (e.g. when the resolved field reference contains a table name or
     when the resolved item is an expression)   (RESOLVED_IGNORING_ALIAS)
*/
enum enum_resolution_type {
  NOT_RESOLVED=0,
  RESOLVED_IGNORING_ALIAS,
  RESOLVED_BEHIND_ALIAS,
  RESOLVED_WITH_NO_ALIAS,
  RESOLVED_AGAINST_ALIAS
};
Item ** find_item_in_list(Session *session,
                          Item *item, List<Item> &items, uint32_t *counter,
                          find_item_error_report_type report_error,
                          enum_resolution_type *resolution);
bool insert_fields(Session *session, Name_resolution_context *context,
                   const char *db_name, const char *table_name,
                   List<Item>::iterator *it, bool any_privileges);
bool setup_tables(Session *session, Name_resolution_context *context,
                  List<TableList> *from_clause, TableList *tables,
                  TableList **leaves, bool select_insert);
bool setup_tables_and_check_access(Session *session,
                                   Name_resolution_context *context,
                                   List<TableList> *from_clause,
                                   TableList *tables,
                                   TableList **leaves,
                                   bool select_insert);
int setup_wild(Session *session, List<Item> &fields,
               List<Item> *sum_func_list,
               uint32_t wild_num);
bool setup_fields(Session *session, Item** ref_pointer_array,
                  List<Item> &item, enum_mark_columns mark_used_columns,
                  List<Item> *sum_func_list, bool allow_sum_func);
inline bool setup_fields_with_no_wrap(Session *session, Item **ref_pointer_array,
                                      List<Item> &item,
                                      enum_mark_columns mark_used_columns,
                                      List<Item> *sum_func_list,
                                      bool allow_sum_func)
{
  bool res;
  res= setup_fields(session, ref_pointer_array, item, mark_used_columns, sum_func_list,
                    allow_sum_func);
  return res;
}
int setup_conds(Session *session, TableList *leaves, COND **conds);
/* open_and_lock_tables with optional derived handling */
TableList *find_table_in_list(TableList *table,
                               TableList *TableList::*link,
                               const char *db_name,
                               const char *table_name);
TableList *unique_table(TableList *table, TableList *table_list,
                        bool check_alias= false);

/* bits for last argument to table::Cache::removeTable() */
#define RTFC_NO_FLAG                0x0000
#define RTFC_OWNED_BY_Session_FLAG      0x0001
#define RTFC_WAIT_OTHER_THREAD_FLAG 0x0002
#define RTFC_CHECK_KILLED_FLAG      0x0004

void mem_alloc_error(size_t size);

bool fill_record(Session* session, List<Item> &fields, List<Item> &values, bool ignore_errors= false);
bool fill_record(Session *session, Field **field, List<Item> &values, bool ignore_errors= false);
inline TableList *find_table_in_global_list(TableList *table,
                                             const char *db_name,
                                             const char *table_name)
{
  return find_table_in_list(table, &TableList::next_global,
                            db_name, table_name);
}

void drizzle_rm_tmp_tables();

} /* namespace drizzled */

