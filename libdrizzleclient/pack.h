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

#ifndef LIBDRIZZLECLIENT_PACK_H
#define LIBDRIZZLECLIENT_PACK_H

#include <stdint.h>

#define NULL_LENGTH UINT32_MAX

#ifdef __cplusplus
extern "C" {
#endif

  uint32_t drizzleclient_net_field_length(unsigned char **packet);
  uint64_t drizzleclient_drizzleclient_net_field_length_ll(unsigned char **packet);
  unsigned char *drizzleclient_net_store_length(unsigned char *pkg, uint64_t length);

#ifdef __cplusplus
}
#endif

#endif /* LIBDRIZZLECLIENT_PACK_H */
