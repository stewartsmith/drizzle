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


#include <thr_alarm.h>

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG |	  \
                             CLIENT_SECURE_CONNECTION | CLIENT_TRANSACTIONS | \
			                 CLIENT_SECURE_CONNECTION)

#define init_sigpipe_variables
#define set_sigpipe(mysql)
#define reset_sigpipe(mysql)
#define read_user_name(A) {}
#define mysql_rpl_query_type(A,B) DRIZZLE_RPL_ADMIN
#define mysql_master_send_query(A, B, C) 1
#define mysql_slave_send_query(A, B, C) 1
#define mysql_rpl_probe(mysql) 0
#undef HAVE_SMEM
#undef _CUSTOMCONFIG_

#define mysql_server_init(a,b,c) 0

DRIZZLE_DATA * cli_read_rows(MYSQL *mysql,DRIZZLE_FIELD *mysql_fields,
                           uint32_t fields);
extern int mysql_init_character_set(MYSQL *mysql);

