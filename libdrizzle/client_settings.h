/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef _libdrizzle_client_settings_h
#define _libdrizzle_client_settings_h

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG |	  \
                             CLIENT_TRANSACTIONS | \
			     			 CLIENT_SECURE_CONNECTION)

sig_handler my_pipe_sig_handler(int sig);
bool handle_local_infile(DRIZZLE *drizzle, const char *net_filename);


/* TODO: Do we still need these now that there's not non-threaded stuff? */
#define init_sigpipe_variables
#define set_sigpipe(drizzle)
#define reset_sigpipe(drizzle)

void mysql_detach_stmt_list(LIST **stmt_list, const char *func_name);
DRIZZLE *
cli_drizzle_connect(DRIZZLE *drizzle,const char *host, const char *user,
		       const char *passwd, const char *db,
		       uint port, const char *unix_socket,ulong client_flag);

void cli_drizzle_close(DRIZZLE *drizzle);

DRIZZLE_FIELD * cli_list_fields(DRIZZLE *drizzle);
DRIZZLE_DATA * cli_read_rows(DRIZZLE *drizzle,DRIZZLE_FIELD *drizzle_fields,
				   uint fields);
int cli_unbuffered_fetch(DRIZZLE *drizzle, char **row);
const char * cli_read_statistics(DRIZZLE *drizzle);
int cli_read_change_user_result(DRIZZLE *drizzle);

C_MODE_START
extern int drizzle_init_character_set(DRIZZLE *drizzle);
C_MODE_END

#endif
