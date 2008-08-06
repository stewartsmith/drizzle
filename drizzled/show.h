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
 * @file
 *
 * Contains function declarations that deal with the SHOW commands.  These
 * will eventually go away, but for now we split these definitions out into
 * their own header file for easier maintenance
 */
#ifndef DRIZZLE_SERVER_SHOW_H
#define DRIZZLE_SERVER_SHOW_H

bool mysqld_show_open_tables(THD *thd,const char *wild);
bool mysqld_show_logs(THD *thd);
void append_identifier(THD *thd, String *packet, const char *name,
		       uint length);
void mysqld_list_fields(THD *thd,TABLE_LIST *table, const char *wild);
int mysqld_dump_create_info(THD *thd, TABLE_LIST *table_list, int fd);
bool mysqld_show_create(THD *thd, TABLE_LIST *table_list);
bool mysqld_show_create_db(THD *thd, char *dbname, HA_CREATE_INFO *create);

void mysqld_list_processes(THD *thd,const char *user,bool verbose);
int mysqld_show_status(THD *thd);
int mysqld_show_variables(THD *thd,const char *wild);
bool mysqld_show_storage_engines(THD *thd);
bool mysqld_show_authors(THD *thd);
bool mysqld_show_contributors(THD *thd);
bool mysqld_show_privileges(THD *thd);
bool mysqld_show_column_types(THD *thd);
bool mysqld_help (THD *thd, const char *text);
void calc_sum_of_all_status(STATUS_VAR *to);

void append_definer(THD *thd, String *buffer, const LEX_STRING *definer_user,
                    const LEX_STRING *definer_host);

int add_status_vars(SHOW_VAR *list);
void remove_status_vars(SHOW_VAR *list);
void init_status_vars();
void free_status_vars();
void reset_status_vars();

#endif /* DRIZZLE_SERVER_SHOW_H */
