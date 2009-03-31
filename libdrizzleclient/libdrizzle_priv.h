/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef LIBDRIZZLECLIENT_LIBDRIZZLE_PRIV_H
#define LIBDRIZZLECLIENT_LIBDRIZZLE_PRIV_H

#include <drizzled/global.h>

#include "drizzle.h"
#include <drizzled/korr.h>

#include <sys/socket.h>

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_SECURE_CONNECTION)


const char * drizzleclient_sqlstate_get_unknown(void);
const char * drizzleclient_sqlstate_get_not_error(void);
const char * drizzleclient_sqlstate_get_cant_connect(void);

void drizzleclient_set_default_port(unsigned int port);
void drizzleclient_set_error(DRIZZLE *drizzle, int errcode, const char *sqlstate);
void drizzleclient_set_extended_error(DRIZZLE *drizzle, int errcode,
                                const char *sqlstate,
                                const char *format, ...);
void drizzleclient_free_old_query(DRIZZLE *drizzle);

int drizzleclient_connect_with_timeout(int fd, const struct sockaddr *name,
                         unsigned int namelen, int32_t timeout);

void drizzleclient_close_free_options(DRIZZLE *drizzle);
void drizzleclient_close_free(DRIZZLE *drizzle);

/* Hook Methods */
bool drizzleclient_cli_read_query_result(DRIZZLE *drizzle);
DRIZZLE_RES *drizzleclient_cli_use_result(DRIZZLE *drizzle);
void drizzleclient_cli_fetch_lengths(uint32_t *to, DRIZZLE_ROW column,
                       uint32_t field_count);
void drizzleclient_cli_flush_use_result(DRIZZLE *drizzle);

#endif
