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

#ifndef _libdrizzle_libdrizzle_priv_h_
#define _libdrizzle_libdrizzle_priv_h_

extern unsigned int drizzle_port;

extern const char  *unknown_sqlstate;
extern const char  *cant_connect_sqlstate;
extern const char  *not_error_sqlstate;


void drizzle_set_default_port(unsigned int port);
void drizzle_set_error(DRIZZLE *drizzle, int errcode, const char *sqlstate);
void drizzle_set_extended_error(DRIZZLE *drizzle, int errcode,
                                const char *sqlstate,
                                const char *format, ...);
void free_old_query(DRIZZLE *drizzle);

#endif
