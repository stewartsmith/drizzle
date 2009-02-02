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

#ifndef LIBDRIZZLECLIENT_DRIZZLE_ROWS_H
#define LIBDRIZZLECLIENT_DRIZZLE_ROWS_H

#ifdef  __cplusplus
extern "C" {
#endif

typedef char **DRIZZLE_ROW;    /* return data as array of strings */

typedef struct st_drizzle_rows {
  struct st_drizzle_rows *next;    /* list of rows */
  DRIZZLE_ROW data;
  unsigned long length;
} DRIZZLE_ROWS;

typedef DRIZZLE_ROWS *DRIZZLE_ROW_OFFSET;  /* offset to current row */


#ifdef  __cplusplus
}
#endif

#endif /* LIBDRIZZLECLIENT_DRIZZLE_ROWS_H */
