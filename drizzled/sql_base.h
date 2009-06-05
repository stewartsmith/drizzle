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

#ifndef DRIZZLED_SQL_BASE_H
#define DRIZZLED_SQL_BASE_H

#include <stdint.h>
#include <drizzled/table.h>

class TableShare;

void table_cache_free(void);
bool table_cache_init(void);
bool table_def_init(void);
void table_def_free(void);
void assign_new_table_id(TableShare *share);
uint32_t cached_open_tables(void);
uint32_t cached_table_definitions(void);

void kill_drizzle(void);

/* sql_base.cc */
void set_item_name(Item *item,char *pos,uint32_t length);
bool add_field_to_list(Session *session, LEX_STRING *field_name, enum enum_field_types type,
		       char *length, char *decimal,
		       uint32_t type_modifier,
           enum column_format_type column_format,
		       Item *default_value, Item *on_update_value,
		       LEX_STRING *comment,
		       char *change, List<String> *interval_list,
		       const CHARSET_INFO * const cs);
CreateField * new_create_field(Session *session, char *field_name, enum_field_types type,
				char *length, char *decimals,
				uint32_t type_modifier,
				Item *default_value, Item *on_update_value,
				LEX_STRING *comment, char *change,
				List<String> *interval_list, CHARSET_INFO *cs);
void store_position_for_column(const char *name);
bool add_to_list(Session *session, SQL_LIST &list,Item *group,bool asc);
bool push_new_name_resolution_context(Session *session,
                                      TableList *left_op,
                                      TableList *right_op);
void add_join_on(TableList *b,Item *expr);
void add_join_natural(TableList *a,TableList *b,List<String> *using_fields,
                      Select_Lex *lex);
void unlink_open_table(Session *session, Table *find, bool unlock);
void drop_open_table(Session *session, Table *table, const char *db_name,
                     const char *table_name);
void update_non_unique_table_error(TableList *update,
                                   const char *operation,
                                   TableList *duplicate);

SQL_SELECT *make_select(Table *head, table_map const_tables,
			table_map read_tables, COND *conds,
                        bool allow_null_cond,  int *error);
extern Item **not_found_item;

/*
  A set of constants used for checking non aggregated fields and sum
  functions mixture in the ONLY_FULL_GROUP_BY_MODE.
*/
#define NON_AGG_FIELD_USED  1
#define SUM_FUNC_USED       2

/*
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
Item ** find_item_in_list(Item *item, List<Item> &items, uint32_t *counter,
                          find_item_error_report_type report_error,
                          enum_resolution_type *resolution);
bool get_key_map_from_key_list(key_map *map, Table *table,
                               List<String> *index_list);
bool insert_fields(Session *session, Name_resolution_context *context,
		   const char *db_name, const char *table_name,
                   List_iterator<Item> *it, bool any_privileges);
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
int setup_ftfuncs(Select_Lex* select);
int init_ftfuncs(Session *session, Select_Lex* select, bool no_order);
void wait_for_condition(Session *session, pthread_mutex_t *mutex,
                        pthread_cond_t *cond);
int open_tables(Session *session, TableList **tables, uint32_t *counter, uint32_t flags);
/* open_and_lock_tables with optional derived handling */
int open_and_lock_tables_derived(Session *session, TableList *tables, bool derived);
/* simple open_and_lock_tables without derived handling */
inline int simple_open_n_lock_tables(Session *session, TableList *tables)
{
  return open_and_lock_tables_derived(session, tables, false);
}
/* open_and_lock_tables with derived handling */
inline int open_and_lock_tables(Session *session, TableList *tables)
{
  return open_and_lock_tables_derived(session, tables, true);
}
bool open_normal_and_derived_tables(Session *session, TableList *tables, uint32_t flags);
int lock_tables(Session *session, TableList *tables, uint32_t counter, bool *need_reopen);
int decide_logging_format(Session *session);
Table *open_temporary_table(Session *session, const char *path, const char *db,
                            const char *table_name, bool link_in_list,
                            open_table_mode open_mode);
bool rm_temporary_table(StorageEngine *base, char *path);
void free_io_cache(Table *entry);
void intern_close_table(Table *entry);
void close_temporary_tables(Session *session);
void close_tables_for_reopen(Session *session, TableList **tables);
TableList *find_table_in_list(TableList *table,
                               TableList *TableList::*link,
                               const char *db_name,
                               const char *table_name);
TableList *unique_table(Session *session, TableList *table, TableList *table_list,
                         bool check_alias);
Table *find_temporary_table(Session *session, const char *db, const char *table_name);
Table *find_temporary_table(Session *session, TableList *table_list);
int drop_temporary_table(Session *session, TableList *table_list);
void close_temporary_table(Session *session, Table *table, bool free_share,
                           bool delete_table);
void close_temporary(Table *table, bool free_share, bool delete_table);
bool rename_temporary_table(Table *table, const char *new_db, const char *table_name);
void remove_db_from_cache(const char *db);
bool is_equal(const LEX_STRING *a, const LEX_STRING *b);

/* bits for last argument to remove_table_from_cache() */
#define RTFC_NO_FLAG                0x0000
#define RTFC_OWNED_BY_Session_FLAG      0x0001
#define RTFC_WAIT_OTHER_THREAD_FLAG 0x0002
#define RTFC_CHECK_KILLED_FLAG      0x0004
bool remove_table_from_cache(Session *session, const char *db, const char *table,
                             uint32_t flags);

#define NORMAL_PART_NAME 0
#define TEMP_PART_NAME 1
#define RENAMED_PART_NAME 2

void mem_alloc_error(size_t size);

#define WFRM_WRITE_SHADOW 1
#define WFRM_INSTALL_SHADOW 2
#define WFRM_PACK_FRM 4
#define WFRM_KEEP_SHARE 8

bool close_cached_tables(Session *session, TableList *tables,
                         bool wait_for_refresh, bool wait_for_placeholders);
void copy_field_from_tmp_record(Field *field,int offset);
bool fill_record(Session * session, List<Item> &fields, List<Item> &values, bool ignore_errors);
bool fill_record(Session *session, Field **field, List<Item> &values, bool ignore_errors);
OPEN_TableList *list_open_tables(const char *db, const char *wild);

inline TableList *find_table_in_global_list(TableList *table,
                                             const char *db_name,
                                             const char *table_name)
{
  return find_table_in_list(table, &TableList::next_global,
                            db_name, table_name);
}

inline TableList *find_table_in_local_list(TableList *table,
                                            const char *db_name,
                                            const char *table_name)
{
  return find_table_in_list(table, &TableList::next_local,
                            db_name, table_name);
}
#endif /* DRIZZLED_SQL_BASE_H */
