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
 * Contains function declarations that deal with connecting to the server,
 * creating a new connection, logging in, etc
 */
#ifndef DRIZZLE_SERVER_CONNECT_H
#define DRIZZLE_SERVER_CONNECT_H

int check_user(THD *thd, const char *passwd, uint32_t passwd_len, const char *db, bool check_count);
pthread_handler_t handle_one_connection(void *arg);
bool init_new_connection_handler_thread();
void time_out_user_resource_limits(THD *thd, USER_CONN *uc);
void decrease_user_connections(USER_CONN *uc);
void thd_init_client_charset(THD *thd, uint32_t cs_number);
bool setup_connection_thread_globals(THD *thd);
bool login_connection(THD *thd);
void prepare_new_connection_state(THD* thd);
void end_connection(THD *thd);

#endif /* DRIZZLE_SERVER_CONNECT_H */
