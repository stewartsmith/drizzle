/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#pragma once
#ifndef create_replication_h
#define create_replication_h

#include "univ.i"
#include "btr0pcur.h"
#include "dict0types.h"
#include "dict0dict.h"
#include "que0types.h"
#include "row0types.h"
#include "mtr0mtr.h"

#include "read_replication.h"

#include <drizzled/message/table.pb.h>
#include <drizzled/table.h>
#include <drizzled/charset.h>

struct read_replication_state_st {
  mtr_t mtr;
  btr_pcur_t pcur;
  dict_table_t *sys_tables;
  dict_index_t *sys_index;
};

UNIV_INTERN ulint dict_create_sys_replication_log(void);

UNIV_INTERN ulint insert_replication_message(const char *message, size_t size,
                                             trx_t *trx, uint64_t trx_id,
                                             uint64_t end_timestamp, bool is_end_segment,
                                             uint32_t seg_id, const char *server_uuid,
                                             bool use_originating_server_uuid,
                                             const char *originating_server_uuid,
                                             uint64_t originating_commit_id);

UNIV_INTERN struct read_replication_state_st *replication_read_init(void);
UNIV_INTERN void replication_read_deinit(struct read_replication_state_st *);
UNIV_INTERN struct read_replication_return_st replication_read_next(struct read_replication_state_st *);

UNIV_INTERN int read_replication_log_table_message(const char* table_name, drizzled::message::Table *table_message);

UNIV_INTERN void convert_to_mysql_format(byte* out, const byte* in, int len);

#endif
