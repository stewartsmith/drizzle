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

/**
  @file

  Routines to drop, repair, optimize, analyze, and check a schema table

*/
#ifndef DRIZZLE_SERVER_SQL_TABLE_H
#define DRIZZLE_SERVER_SQL_TABLE_H

bool mysql_rm_table(THD *thd,TABLE_LIST *tables, bool if_exists,
                    bool drop_temporary);
int mysql_rm_table_part2(THD *thd, TABLE_LIST *tables, bool if_exists,
                         bool drop_temporary, bool drop_view, bool log_query);
bool quick_rm_table(handlerton *base,const char *db,
                    const char *table_name, uint flags);
void close_cached_table(THD *thd, TABLE *table);

void wait_while_table_is_used(THD *thd, TABLE *table,
                              enum ha_extra_function function);
bool mysql_checksum_table(THD* thd, TABLE_LIST* table_list,
                          HA_CHECK_OPT* check_opt);
bool mysql_check_table(THD* thd, TABLE_LIST* table_list,
                       HA_CHECK_OPT* check_opt);
bool mysql_repair_table(THD* thd, TABLE_LIST* table_list,
                        HA_CHECK_OPT* check_opt);
bool mysql_analyze_table(THD* thd, TABLE_LIST* table_list,
                         HA_CHECK_OPT* check_opt);
bool mysql_optimize_table(THD* thd, TABLE_LIST* table_list,
                          HA_CHECK_OPT* check_opt);

bool mysql_assign_to_keycache(THD* thd, TABLE_LIST* table_list,
                              LEX_STRING *key_cache_name);
bool mysql_preload_keys(THD* thd, TABLE_LIST* table_list);
int reassign_keycache_tables(THD* thd, KEY_CACHE *src_cache,
                             KEY_CACHE *dst_cache);

#endif /* DRIZZLE_SERVER_SQL_TABLE_H */
