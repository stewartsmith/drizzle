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

/*
  This file defines the client API to DRIZZLE and also the ABI of the
  dynamically linked libdrizzleclient.

  In case the file is changed so the ABI is broken, you must also
  update the SHARED_LIB_MAJOR_VERSION in configure.ac.

*/

#ifndef LIBDRIZZLECLIENT_LIBDRIZZLE_H
#define LIBDRIZZLECLIENT_LIBDRIZZLE_H

#include <drizzled/common.h>

#define CLIENT_NET_READ_TIMEOUT    365*24*3600  /* Timeout on read */
#define CLIENT_NET_WRITE_TIMEOUT  365*24*3600  /* Timeout on write */

#define CLIENT_LONG_PASSWORD    1       /* new more secure passwords */
#define CLIENT_FOUND_ROWS       2       /* Found instead of affected rows */
#define CLIENT_LONG_FLAG        4       /* Get all column flags */
#define CLIENT_CONNECT_WITH_DB  8       /* One can specify db on connect */
#define CLIENT_NO_SCHEMA        16      /* Don't allow database.table.column */
#define CLIENT_COMPRESS         32      /* Can use compression protocol */
#define CLIENT_ODBC             64      /* Odbc client */
#define CLIENT_IGNORE_SPACE     256     /* Ignore spaces before '(' */
#define UNUSED_CLIENT_PROTOCOL_41       512     /* New 4.1 protocol */
#define CLIENT_SSL              2048    /* Switch to SSL after handshake */
#define CLIENT_IGNORE_SIGPIPE   4096    /* IGNORE sigpipes */
#define CLIENT_RESERVED         16384   /* Old flag for 4.1 protocol  */
#define CLIENT_SECURE_CONNECTION 32768  /* New 4.1 authentication */
#define CLIENT_MULTI_STATEMENTS (1UL << 16) /* Enable/disable multi-stmt support */
#define CLIENT_MULTI_RESULTS    (1UL << 17) /* Enable/disable multi-results */

#define CLIENT_SSL_VERIFY_SERVER_CERT (1UL << 30)
#define CLIENT_REMEMBER_OPTIONS (1UL << 31)

/* Gather all possible capabilites (flags) supported by the server */
#define CLIENT_ALL_FLAGS  (CLIENT_LONG_PASSWORD | \
                           CLIENT_FOUND_ROWS | \
                           CLIENT_LONG_FLAG | \
                           CLIENT_CONNECT_WITH_DB | \
                           CLIENT_NO_SCHEMA | \
                           CLIENT_COMPRESS | \
                           CLIENT_ODBC | \
                           CLIENT_IGNORE_SPACE | \
                           CLIENT_SSL | \
                           CLIENT_IGNORE_SIGPIPE | \
                           CLIENT_RESERVED | \
                           CLIENT_SECURE_CONNECTION | \
                           CLIENT_MULTI_STATEMENTS | \
                           CLIENT_MULTI_RESULTS | \
                           CLIENT_SSL_VERIFY_SERVER_CERT | \
                           CLIENT_REMEMBER_OPTIONS)

/*
  Switch off the flags that are optional and depending on build flags
  If any of the optional flags is supported by the build it will be switched
  on before sending to the client during the connection handshake.
*/
#define CLIENT_BASIC_FLAGS (((CLIENT_ALL_FLAGS & ~CLIENT_SSL) \
                                               & ~CLIENT_COMPRESS) \
                                               & ~CLIENT_SSL_VERIFY_SERVER_CERT)


#include "drizzle_field.h"
#include "drizzle_rows.h"
#include "drizzle_data.h"
#include "options.h"

#include "drizzle.h"
#include "drizzle_methods.h"

#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

  unsigned int drizzleclient_get_default_port(void);

#ifdef  __cplusplus
}
#endif

#endif /* LIBDRIZZLECLIENT_LIBDRIZZLE_H */
