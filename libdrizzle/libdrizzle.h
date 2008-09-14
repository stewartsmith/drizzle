/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
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
  update the SHAREDLIB_MAJOR_VERSION in configure.ac.

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
#if !defined(DRIZZLE_SERVER) && !defined(DRIZZLE_CLIENT)
#define DRIZZLE_CLIENT
#endif

#include <libdrizzle/drizzle_field.h>
#include <libdrizzle/drizzle_rows.h>
#include <libdrizzle/drizzle_data.h>
#include <libdrizzle/drizzle_options.h>

#include <libdrizzle/drizzle.h>
#include <libdrizzle/drizzle_parameters.h>
#include <libdrizzle/drizzle_methods.h>

/*
  Set up and bring down the server; to ensure that applications will
  work when linked against either the standard client library or the
  embedded server library, these functions should be called.
*/
void drizzle_server_end(void);

/*
  drizzle_server_init/end need to be called when using libdrizzle or
  libdrizzleclient (exactly, drizzle_server_init() is called by drizzle_init() so
  you don't need to call it explicitely; but you need to call
  drizzle_server_end() to free memory). The names are a bit misleading
  (drizzle_SERVER* to be used when using libdrizzleCLIENT). So we add more general
  names which suit well whether you're using libdrizzled or libdrizzleclient. We
  intend to promote these aliases over the drizzle_server* ones.
*/
#define drizzle_library_end drizzle_server_end


const char *  drizzle_get_client_info(void);
uint32_t  drizzle_get_client_version(void);
uint32_t  drizzle_escape_string(char *to,const char *from, uint32_t from_length);
uint32_t  drizzle_hex_string(char *to,const char *from, uint32_t from_length);

/*
  The following functions are mainly exported because of binlog;
  They are not for general usage
*/

#define simple_command(drizzle, command, arg, length, skip_check) \
  (*(drizzle)->methods->advanced_command)(drizzle, command, 0,  \
                                        0, arg, length, skip_check)

#ifdef  __cplusplus
}
#endif

#endif /* _libdrizzle_libdrizzle_h */
