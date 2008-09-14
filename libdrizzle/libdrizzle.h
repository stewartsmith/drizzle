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

/*
  This file defines the client API to DRIZZLE and also the ABI of the
  dynamically linked libdrizzleclient.

  In case the file is changed so the ABI is broken, you must also
  update the SHARED_LIB_MAJOR_VERSION in configure.ac.

*/

#ifndef _libdrizzle_libdrizzle_h
#define _libdrizzle_libdrizzle_h

#ifdef  __cplusplus
extern "C" {
#endif

#include <libdrizzle/drizzle_com.h>

extern unsigned int drizzle_port;

#define CLIENT_NET_READ_TIMEOUT    365*24*3600  /* Timeout on read */
#define CLIENT_NET_WRITE_TIMEOUT  365*24*3600  /* Timeout on write */

#include <stdint.h>
#include <libdrizzle/drizzle_field.h>
#include <libdrizzle/drizzle_rows.h>
#include <libdrizzle/drizzle_data.h>
#include <libdrizzle/drizzle_options.h>

#include <libdrizzle/drizzle.h>
#include <libdrizzle/drizzle_parameters.h>
#include <libdrizzle/drizzle_methods.h>

const char * drizzle_get_client_info(void);
uint32_t drizzle_get_client_version(void);
uint32_t drizzle_escape_string(char *to,const char *from,
                               uint32_t from_length);
uint32_t drizzle_hex_string(char *to,const char *from,
                            uint32_t from_length);

/*
  Set up and bring down the server; to ensure that applications will
  work when linked against either the standard client library or the
  embedded server library, these functions should be called.
*/
void drizzle_server_end(void);


#ifdef  __cplusplus
}
#endif

#endif /* _libdrizzle_libdrizzle_h */
