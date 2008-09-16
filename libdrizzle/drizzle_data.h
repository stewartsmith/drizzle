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

#ifndef _libdrizzle_drizzle_data_h
#define _libdrizzle_drizzle_data_h

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <libdrizzle/drizzle_rows.h>

typedef struct st_drizzle_data {
  DRIZZLE_ROWS *data;
  struct embedded_query_result *embedded_info;
  uint64_t rows;
  unsigned int fields;
} DRIZZLE_DATA;


#ifdef  __cplusplus
}
#endif

#endif /* _libdrizzle_drizzle_data_h */
