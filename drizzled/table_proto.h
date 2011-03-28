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

#pragma once

#include <drizzled/sql_list.h>

namespace drizzled {
/*
  Maximum length of comments.

  These are historical limits that no longer need to apply.
*/
#define TABLE_COMMENT_MAXLEN 2048
#define COLUMN_COMMENT_MAXLEN 1024
#define INDEX_COMMENT_MAXLEN 1024

bool rea_create_table(Session *session,
                      const identifier::Table &identifier,
                      message::Table &table_proto,
                      HA_CREATE_INFO *create_info,
                      List<CreateField> &create_field,
                      uint32_t key_count,KeyInfo *key_info);

} /* namespace drizzled */
