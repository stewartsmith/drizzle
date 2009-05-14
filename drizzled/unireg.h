/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

/*  Extra functions used by unireg library */

#ifndef DRIZZLED_UNIREG_H
#define DRIZZLED_UNIREG_H

#include <drizzled/structs.h>				/* All structs we need */
#include <drizzled/message/table.pb.h>
int drizzle_read_table_proto(const char* path, drizzled::message::Table* table);
int table_proto_exists(const char *path);
int copy_table_proto_file(const char *from, const char* to);

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef NO_ALARM_LOOP
#define NO_ALARM_LOOP		/* lib5 and popen can't use alarm */
#endif

void unireg_end(void) __attribute__((noreturn));
void unireg_abort(int exit_code) __attribute__((noreturn));

int rea_create_table(Session *session, const char *path,
                     const char *db, const char *table_name,
                     HA_CREATE_INFO *create_info,
                     List<Create_field> &create_field,
                     uint32_t key_count,KEY *key_info,
                     handler *file, bool is_like);


#if defined(__cplusplus)
}
#endif


#endif /* DRIZZLED_UNIREG_H */
