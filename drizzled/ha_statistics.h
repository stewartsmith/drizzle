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

#include <drizzled/base.h>
#include <time.h>

namespace drizzled
{

class ha_statistics
{
public:
  uint64_t data_file_length;		/* Length off data file */
  uint64_t max_data_file_length;	/* Length off data file */
  uint64_t index_file_length;
  uint64_t max_index_file_length;
  uint64_t delete_length;		/* Free bytes */
  uint64_t auto_increment_value;
  /*
    The number of records in the table.
      0    - means the table has exactly 0 rows
    other  - if (table_flags() & HA_STATS_RECORDS_IS_EXACT)
               the value is the exact number of records in the table
             else
               it is an estimate
  */
  ha_rows records;
  ha_rows deleted;			/* Deleted records */
  uint32_t mean_rec_length;		/* physical reclength */
  uint32_t block_size;			/* index block size */
  time_t create_time;			/* When table was created */
  time_t check_time;
  time_t update_time;

  ha_statistics():
    data_file_length(0), max_data_file_length(0),
    index_file_length(0), delete_length(0), auto_increment_value(0),
    records(0), deleted(0), mean_rec_length(0), block_size(0),
    create_time(0), check_time(0), update_time(0)
  {}
};

} /* namespace drizzled */

