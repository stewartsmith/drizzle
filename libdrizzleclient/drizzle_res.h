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

#ifndef LIBDRIZZLECLIENT_DRIZZLE_RES_H
#define LIBDRIZZLECLIENT_DRIZZLE_RES_H

#include "drizzle_field.h"
#include "drizzle_data.h"
#include "drizzle_rows.h"
#include "drizzle.h"

#include <stdint.h>
#if !defined(__cplusplus)
# include <stdbool.h>
#endif

#ifdef  __cplusplus
extern "C" {
#endif

  struct st_drizzle;

  typedef struct st_drizzle_res {
  uint64_t  row_count;
  DRIZZLE_FIELD  *fields;
  DRIZZLE_DATA  *data;
  DRIZZLE_ROWS  *data_cursor;
  uint32_t *lengths;    /* column lengths of current row */
  struct st_drizzle *handle;    /* for unbuffered reads */
  const struct st_drizzle_methods *methods;
  DRIZZLE_ROW  row;      /* If unbuffered read */
  DRIZZLE_ROW  current_row;    /* buffer to current row */
  uint32_t  field_count, current_field;
  bool  eof;      /* Used by drizzleclient_fetch_row */
  /* drizzle_stmt_close() had to cancel this result */
  bool       unbuffered_fetch_cancelled;
  void *extension;
} DRIZZLE_RES;


/*
  Functions to get information from the DRIZZLE and DRIZZLE_RES structures
  Should definitely be used if one uses shared libraries.
*/

uint64_t drizzleclient_num_rows(const DRIZZLE_RES *res);
unsigned int drizzleclient_num_fields(const DRIZZLE_RES *res);
bool drizzleclient_eof(const DRIZZLE_RES *res);
const DRIZZLE_FIELD * drizzleclient_fetch_field_direct(const DRIZZLE_RES *res,
                unsigned int fieldnr);
const DRIZZLE_FIELD * drizzleclient_fetch_fields(const DRIZZLE_RES *res);
DRIZZLE_ROW_OFFSET drizzleclient_row_tell(const DRIZZLE_RES *res);
DRIZZLE_FIELD_OFFSET drizzleclient_field_tell(const DRIZZLE_RES *res);

void    drizzleclient_free_result(DRIZZLE_RES *result);
void    drizzleclient_data_seek(DRIZZLE_RES *result, uint64_t offset);
DRIZZLE_ROW_OFFSET drizzleclient_row_seek(DRIZZLE_RES *result, DRIZZLE_ROW_OFFSET offset);
DRIZZLE_FIELD_OFFSET drizzleclient_field_seek(DRIZZLE_RES *result, DRIZZLE_FIELD_OFFSET offset);
DRIZZLE_ROW  drizzleclient_fetch_row(DRIZZLE_RES *result);
uint32_t * drizzleclient_fetch_lengths(DRIZZLE_RES *result);
DRIZZLE_FIELD *  drizzleclient_fetch_field(DRIZZLE_RES *result);

#ifdef  __cplusplus
}
#endif

#endif /* LIBDRIZZLECLIENT_DRIZZLE_RES_H */
