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

/**
 * @file
 *
 * Contains function declarations that deal with the SHOW commands.  These
 * will eventually go away, but for now we split these definitions out into
 * their own header file for easier maintenance
 */
#ifndef DRIZZLE_SERVER_SHOW_H
#define DRIZZLE_SERVER_SHOW_H

bool mysqld_show_open_tables(Session *thd,const char *wild);
bool mysqld_show_logs(Session *thd);
void append_identifier(Session *thd, String *packet, const char *name,
		       uint32_t length);
void mysqld_list_fields(Session *thd,TableList *table, const char *wild);
int mysqld_dump_create_info(Session *thd, TableList *table_list, int fd);
bool mysqld_show_create(Session *thd, TableList *table_list);
bool mysqld_show_create_db(Session *thd, char *dbname, HA_CREATE_INFO *create);

void mysqld_list_processes(Session *thd,const char *user,bool verbose);
int mysqld_show_status(Session *thd);
int mysqld_show_variables(Session *thd,const char *wild);
bool mysqld_show_storage_engines(Session *thd);
bool mysqld_show_authors(Session *thd);
bool mysqld_show_contributors(Session *thd);
bool mysqld_show_privileges(Session *thd);
bool mysqld_show_column_types(Session *thd);
bool mysqld_help (Session *thd, const char *text);
void calc_sum_of_all_status(STATUS_VAR *to);

void append_definer(Session *thd, String *buffer, const LEX_STRING *definer_user,
                    const LEX_STRING *definer_host);

int add_status_vars(SHOW_VAR *list);
void remove_status_vars(SHOW_VAR *list);
void init_status_vars();
void free_status_vars();
void reset_status_vars();

#endif /* DRIZZLE_SERVER_SHOW_H */
