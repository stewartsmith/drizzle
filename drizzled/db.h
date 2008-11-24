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


#ifndef DRIZZLE_SERVER_DB_H
#define DRIZZLE_SERVER_DB_H

int mysql_create_db(Session *session, char *db, HA_CREATE_INFO *create, bool silent);
bool mysql_alter_db(Session *session, const char *db, HA_CREATE_INFO *create);
bool mysql_rm_db(Session *session,char *db,bool if_exists, bool silent);
bool mysql_change_db(Session *session, const LEX_STRING *new_db_name,
                     bool force_switch);
bool mysql_opt_change_db(Session *session,
                         const LEX_STRING *new_db_name,
                         LEX_STRING *saved_db_name,
                         bool force_switch,
                         bool *cur_db_changed);


#define MY_DB_OPT_FILE "db.opt"
bool my_database_names_init(void);
void my_database_names_free(void);
bool check_db_dir_existence(const char *db_name);
bool load_db_opt(Session *session, const char *path, HA_CREATE_INFO *create);
bool load_db_opt_by_name(Session *session, const char *db_name,
                         HA_CREATE_INFO *db_create_info);
const CHARSET_INFO *get_default_db_collation(Session *session, const char *db_name);
bool my_dbopt_init(void);
void my_dbopt_cleanup(void);
extern int creating_database; // How many database locks are made
extern int creating_table;    // How many mysql_create_table() are running

#endif
