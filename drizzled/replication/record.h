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

#ifndef DRIZZLED_RPL_RECORD_H
#define DRIZZLED_RPL_RECORD_H

#include <drizzled/replication/reporting.h>

size_t pack_row(Table* table, MY_BITMAP const* cols,
                unsigned char *row_data, const unsigned char *data);

int unpack_row(Relay_log_info const *rli,
               Table *table, uint32_t const colcnt,
               unsigned char const *const row_data, MY_BITMAP const *cols,
               unsigned char const **const row_end, ulong *const master_reclength);

// Fill table's record[0] with default values.
int prepare_record(Table *const, const MY_BITMAP *cols, uint32_t width, const bool);

#endif /* DRIZZLED_RPL_RECORD_H */
