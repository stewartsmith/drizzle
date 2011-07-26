/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

/**
  @file

  Routines to drop, repair, optimize, analyze, and check a schema table

*/
#pragma once

#include <drizzled/base.h>

namespace drizzled {

typedef struct st_ha_create_information HA_CREATE_INFO;

int rm_table_part2(Session *session, TableList *tables, bool if_exists,
                         bool drop_temporary);
void close_cached_table(Session *session, Table *table);

void wait_while_table_is_used(Session *session, Table *table,
                              enum ha_extra_function function);

bool check_table(Session* session, TableList* table_list);
bool analyze_table(Session* session, TableList* table_list);
bool optimize_table(Session* session, TableList* table_list);

bool is_primary_key(KeyInfo *key_info);
const char* is_primary_key_name(const char* key_name);
bool check_engine(Session *, const char *, message::Table *, HA_CREATE_INFO *);
void set_table_default_charset(HA_CREATE_INFO *create_info, const char *db);
} /* namespace drizzled */

