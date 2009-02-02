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

#ifndef LIBDRIZZLECLIENT_DRIZZLE_METHODS_H
#define LIBDRIZZLECLIENT_DRIZZLE_METHODS_h

#include "drizzle.h"
#include "drizzle_data.h"
#include "drizzle_res.h"
#include "drizzle_field.h"

#include <stdint.h>
#if !defined(__cplusplus)
# include <stdbool.h>
#endif

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct st_drizzle_methods
{
  bool (*read_query_result)(DRIZZLE *drizzle);
  bool (*advanced_command)(DRIZZLE *drizzle,
                           enum enum_server_command command,
                           const unsigned char *header,
                           uint32_t header_length,
                           const unsigned char *arg,
                           uint32_t arg_length,
                           bool skip_check);
  DRIZZLE_DATA *(*read_rows)(DRIZZLE *drizzle, DRIZZLE_FIELD *drizzle_fields,
                             uint32_t fields);
  DRIZZLE_RES * (*use_result)(DRIZZLE *drizzle);
  void (*fetch_lengths)(uint32_t *to, DRIZZLE_ROW column,
                        uint32_t field_count);
  void (*flush_use_result)(DRIZZLE *drizzle);
  DRIZZLE_FIELD * (*list_fields)(DRIZZLE *drizzle);
  int32_t (*unbuffered_fetch)(DRIZZLE *drizzle, char **row);
  const char *(*read_statistics)(DRIZZLE *drizzle);
  bool (*next_result)(DRIZZLE *drizzle);
  int32_t (*read_change_user_result)(DRIZZLE *drizzle);
} DRIZZLE_METHODS;

#define simple_command(drizzle, command, arg, length, skip_check) \
  (*(drizzle)->methods->advanced_command)(drizzle, command, 0,          \
                                          0, arg, length, skip_check)

#ifdef  __cplusplus
}
#endif

#endif /* LIBDRIZZLECLIENT_DRIZZLE_METHODS_H */
