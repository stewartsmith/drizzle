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

#ifndef DRIZZLED_SQL_BASE_H
#define DRIZZLED_SQL_BASE_H

#include <stdint.h>

typedef struct st_table_share TABLE_SHARE;

void table_cache_free(void);
bool table_cache_init(void);
bool table_def_init(void);
void table_def_free(void);
void assign_new_table_id(TABLE_SHARE *share);
uint32_t cached_open_tables(void);
uint32_t cached_table_definitions(void);

void kill_drizzle(void);


#endif /* DRIZZLED_SQL_BASE_H */
