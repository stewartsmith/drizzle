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

bool mysql_create_db(Session *session, const char *db, HA_CREATE_INFO *create_info);
bool mysql_alter_db(Session *session, const char *db, HA_CREATE_INFO *create);
bool mysql_rm_db(Session *session,char *db, bool if_exists);
bool mysql_change_db(Session *session, const LEX_STRING *new_db_name,
                     bool force_switch);

bool check_db_dir_existence(const char *db_name);
int load_db_opt_by_name(const char *db_name, HA_CREATE_INFO *db_create_info);
const CHARSET_INFO *get_default_db_collation(const char *db_name);

extern int creating_database; // How many database locks are made
extern int creating_table;    // How many mysql_create_table() are running

#endif
