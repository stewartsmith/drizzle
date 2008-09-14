/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef _libdrizzle_drizzle_parameters_h
#define _libdrizzle_drizzle_parameters_h

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct st_drizzle_parameters
{
  uint32_t *p_max_allowed_packet;
  uint32_t *p_net_buffer_length;
  void *extension;
} DRIZZLE_PARAMETERS;

const DRIZZLE_PARAMETERS * drizzle_get_parameters(void);

#if !defined(DRIZZLE_SERVER)
#define max_allowed_packet (*drizzle_get_parameters()->p_max_allowed_packet)
#define net_buffer_length (*drizzle_get_parameters()->p_net_buffer_length)
#endif

#ifdef  __cplusplus
}
#endif

#endif /* _libdrizzle_drizzle_parameters_h */
