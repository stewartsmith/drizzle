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

#ifndef LIBDRIZZLECLIENT_DRIZZLE_H
#define LIBDRIZZLECLIENT_DRIZZLE_H

#include <drizzled/common.h>
#include "drizzle_field.h"
#include "drizzle_options.h"
#include "drizzle_res.h"
#include "net_serv.h"

#include <stdint.h>
#if !defined(__cplusplus)
# include <stdbool.h>
#endif

#ifdef  __cplusplus
extern "C" {
#endif

struct st_drizzle_methods;

enum drizzle_status
{
  DRIZZLE_STATUS_READY,DRIZZLE_STATUS_GET_RESULT,DRIZZLE_STATUS_USE_RESULT
};

typedef struct st_drizzle
{
  NET    net;      /* Communication parameters */
  unsigned char  *connector_fd;    /* ConnectorFd for SSL */
  char    *host,*user,*passwd,*unix_socket,*server_version,*host_info;
  char          *info, *db;
  DRIZZLE_FIELD  *fields;
  uint64_t affected_rows;
  uint64_t insert_id;    /* id if insert on table with NEXTNR */
  uint64_t extra_info;    /* Not used */
  uint32_t thread_id;    /* Id for connection in server */
  uint32_t packet_length;
  uint32_t  port;
  uint32_t client_flag,server_capabilities;
  uint32_t  protocol_version;
  uint32_t  field_count;
  uint32_t  server_status;
  uint32_t  server_language;
  uint32_t  warning_count;
  struct st_drizzle_options options;
  enum drizzle_status status;
  bool  free_me;    /* If free in drizzle_close */
  bool  reconnect;    /* set to 1 if automatic reconnect */

  /* session-wide random string */
  char          scramble[SCRAMBLE_LENGTH+1];
  bool unused1;
  void *unused2, *unused3, *unused4, *unused5;

  const struct st_drizzle_methods *methods;
  void *thd;
  /*
    Points to boolean flag in DRIZZLE_RES  or DRIZZLE_STMT. We set this flag
    from drizzle_stmt_close if close had to cancel result set of this object.
  */
  bool *unbuffered_fetch_owner;
  /* needed for embedded server - no net buffer to store the 'info' */
  char *info_buffer;
  void *extension;
} DRIZZLE;

DRIZZLE * drizzle_create(DRIZZLE *drizzle);
DRIZZLE * drizzle_connect(DRIZZLE *drizzle, const char *host,
             const char *user,
             const char *passwd,
             const char *db,
             uint32_t port,
             const char *unix_socket,
             uint32_t clientflag);

int32_t    drizzle_select_db(DRIZZLE *drizzle, const char *db);
int32_t    drizzle_query(DRIZZLE *drizzle, const char *q);
int32_t    drizzle_send_query(DRIZZLE *drizzle, const char *q, uint32_t length);
int32_t    drizzle_real_query(DRIZZLE *drizzle, const char *q, uint32_t length);

DRIZZLE_RES * drizzle_store_result(DRIZZLE *drizzle);
DRIZZLE_RES * drizzle_use_result(DRIZZLE *drizzle);

uint32_t drizzle_field_count(const DRIZZLE *drizzle);
uint64_t drizzle_affected_rows(const DRIZZLE *drizzle);
uint64_t drizzle_insert_id(const DRIZZLE *drizzle);
uint32_t drizzle_errno(const DRIZZLE *drizzle);
const char * drizzle_error(const DRIZZLE *drizzle);
const char * drizzle_sqlstate(const DRIZZLE *drizzle);
uint32_t drizzle_warning_count(const DRIZZLE *drizzle);
const char * drizzle_info(const DRIZZLE *drizzle);
uint32_t drizzle_thread_id(const DRIZZLE *drizzle);
const char * drizzle_character_set_name(const DRIZZLE *drizzle);

bool   drizzle_change_user(DRIZZLE *drizzle, const char *user,
                           const char *passwd, const char *db);

/* local infile support */

#define LOCAL_INFILE_ERROR_LEN 512

void
drizzle_set_local_infile_handler(DRIZZLE *drizzle,
        int (*local_infile_init)(void **, const char *, void *),
        int (*local_infile_read)(void *, char *, unsigned int),
        void (*local_infile_end)(void *),int (*local_infile_error)
        (void *, char*, unsigned int), void *);

void
drizzle_set_local_infile_default(DRIZZLE *drizzle);

int32_t    drizzle_shutdown(DRIZZLE *drizzle);
int32_t    drizzle_dump_debug_info(DRIZZLE *drizzle);
int32_t    drizzle_refresh(DRIZZLE *drizzle, uint32_t refresh_options);
int32_t    drizzle_kill(DRIZZLE *drizzle, uint32_t pid);
int32_t    drizzle_set_server_option(DRIZZLE *drizzle,
                                     enum enum_drizzle_set_option option);
int32_t    drizzle_ping(DRIZZLE *drizzle);
const char *  drizzle_stat(DRIZZLE *drizzle);
const char *  drizzle_get_server_info(const DRIZZLE *drizzle);

const char *  drizzle_get_host_info(const DRIZZLE *drizzle);
uint32_t  drizzle_get_server_version(const DRIZZLE *drizzle);
uint32_t  drizzle_get_proto_info(const DRIZZLE *drizzle);
DRIZZLE_RES *  drizzle_list_tables(DRIZZLE *drizzle,const char *wild);
DRIZZLE_RES *  drizzle_list_processes(DRIZZLE *drizzle);
int32_t    drizzle_options(DRIZZLE *drizzle, enum drizzle_option option,
                           const void *arg);
DRIZZLE_RES *     drizzle_list_fields(DRIZZLE *drizzle, const char *table,
                                      const char *wild);
bool         drizzle_read_query_result(DRIZZLE *drizzle);

bool drizzle_commit(DRIZZLE *drizzle);
bool drizzle_rollback(DRIZZLE *drizzle);
bool drizzle_autocommit(DRIZZLE *drizzle, bool auto_mode);
bool drizzle_more_results(const DRIZZLE *drizzle);
int drizzle_next_result(DRIZZLE *drizzle);
void drizzle_close(DRIZZLE *drizzle);
bool drizzle_reconnect(DRIZZLE *drizzle);
void drizzle_disconnect(DRIZZLE *drizzle);

bool
drizzleclient_cli_advanced_command(DRIZZLE *drizzle, enum enum_server_command command,
                     const unsigned char *header, uint32_t header_length,
                     const unsigned char *arg, uint32_t arg_length,
                     bool skip_check);
uint32_t drizzleclient_cli_safe_read(DRIZZLE *drizzle);
DRIZZLE_FIELD * drizzleclient_cli_list_fields(DRIZZLE *drizzle);
DRIZZLE_DATA * drizzleclient_cli_read_rows(DRIZZLE *drizzle,DRIZZLE_FIELD *drizzle_fields,
                             unsigned int fields);
int drizzleclient_cli_unbuffered_fetch(DRIZZLE *drizzle, char **row);
const char * drizzleclient_cli_read_statistics(DRIZZLE *drizzle);
int drizzleclient_cli_read_change_user_result(DRIZZLE *drizzle);

typedef bool (*safe_read_error_hook_func)(NET *net);
extern safe_read_error_hook_func safe_read_error_hook;

#ifdef  __cplusplus
}
#endif

#endif /* LIBDRIZZLECLIENT_DRIZZLE_H */
