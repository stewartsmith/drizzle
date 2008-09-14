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

#ifndef _libdrizzle_sql_common_h
#define _libdrizzle_sql_common_h

extern const char	*unknown_sqlstate;
extern const char	*cant_connect_sqlstate;
extern const char	*not_error_sqlstate;

#ifdef	__cplusplus
extern "C" {
#endif

DRIZZLE_FIELD *unpack_fields(DRIZZLE_DATA *data, uint fields,
                             bool default_value);
void free_rows(DRIZZLE_DATA *cur);
void free_old_query(DRIZZLE *drizzle);
void end_server(DRIZZLE *drizzle);
bool drizzle_reconnect(DRIZZLE *drizzle);
bool
cli_advanced_command(DRIZZLE *drizzle, enum enum_server_command command,
		     const unsigned char *header, uint32_t header_length,
		     const unsigned char *arg, uint32_t arg_length, bool skip_check);
uint32_t cli_safe_read(DRIZZLE *drizzle);
void net_clear_error(NET *net);
void set_drizzle_error(DRIZZLE *drizzle, int errcode, const char *sqlstate);
#ifdef	__cplusplus
}
#endif

#endif
