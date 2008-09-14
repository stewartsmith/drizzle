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

#include <libdrizzle/drizzle_field.h>
#include <libdrizzle/drizzle_rows.h>
#include <libdrizzle/drizzle_data.h>
#include <libdrizzle/drizzle_options.h>


enum drizzle_status
{
  DRIZZLE_STATUS_READY,DRIZZLE_STATUS_GET_RESULT,DRIZZLE_STATUS_USE_RESULT
};

struct st_drizzle_methods;
struct st_drizzle_stmt;

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


typedef struct st_drizzle_res {
  uint64_t  row_count;
  DRIZZLE_FIELD  *fields;
  DRIZZLE_DATA  *data;
  DRIZZLE_ROWS  *data_cursor;
  uint32_t *lengths;    /* column lengths of current row */
  DRIZZLE *handle;    /* for unbuffered reads */
  const struct st_drizzle_methods *methods;
  DRIZZLE_ROW  row;      /* If unbuffered read */
  DRIZZLE_ROW  current_row;    /* buffer to current row */
  uint32_t  field_count, current_field;
  bool  eof;      /* Used by drizzle_fetch_row */
  /* drizzle_stmt_close() had to cancel this result */
  bool       unbuffered_fetch_cancelled; 
  void *extension;
} DRIZZLE_RES;


#if !defined(DRIZZLE_SERVER) && !defined(DRIZZLE_CLIENT)
#define DRIZZLE_CLIENT
#endif


typedef struct st_drizzle_parameters
{
  uint32_t *p_max_allowed_packet;
  uint32_t *p_net_buffer_length;
  void *extension;
} DRIZZLE_PARAMETERS;

#if !defined(DRIZZLE_SERVER)
#define max_allowed_packet (*drizzle_get_parameters()->p_max_allowed_packet)
#define net_buffer_length (*drizzle_get_parameters()->p_net_buffer_length)
#endif

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

const DRIZZLE_PARAMETERS * drizzle_get_parameters(void);


/*
  Functions to get information from the DRIZZLE and DRIZZLE_RES structures
  Should definitely be used if one uses shared libraries.
*/

uint64_t drizzle_num_rows(const DRIZZLE_RES *res);
unsigned int drizzle_num_fields(const DRIZZLE_RES *res);
bool drizzle_eof(const DRIZZLE_RES *res);
const DRIZZLE_FIELD * drizzle_fetch_field_direct(const DRIZZLE_RES *res,
                unsigned int fieldnr);
const DRIZZLE_FIELD * drizzle_fetch_fields(const DRIZZLE_RES *res);
DRIZZLE_ROW_OFFSET drizzle_row_tell(const DRIZZLE_RES *res);
DRIZZLE_FIELD_OFFSET drizzle_field_tell(const DRIZZLE_RES *res);

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
int32_t          drizzle_set_character_set(DRIZZLE *drizzle, const char *csname);

DRIZZLE * drizzle_create(DRIZZLE *drizzle);
bool   drizzle_change_user(DRIZZLE *drizzle, const char *user,
            const char *passwd, const char *db);
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

int32_t    drizzle_shutdown(DRIZZLE *drizzle, enum drizzle_enum_shutdown_level shutdown_level);
int32_t    drizzle_dump_debug_info(DRIZZLE *drizzle);
int32_t    drizzle_refresh(DRIZZLE *drizzle, uint32_t refresh_options);
int32_t    drizzle_kill(DRIZZLE *drizzle, uint32_t pid);
int32_t    drizzle_set_server_option(DRIZZLE *drizzle, enum enum_drizzle_set_option option);
int32_t    drizzle_ping(DRIZZLE *drizzle);
const char *  drizzle_stat(DRIZZLE *drizzle);
const char *  drizzle_get_server_info(const DRIZZLE *drizzle);
const char *  drizzle_get_client_info(void);
uint32_t  drizzle_get_client_version(void);
const char *  drizzle_get_host_info(const DRIZZLE *drizzle);
uint32_t  drizzle_get_server_version(const DRIZZLE *drizzle);
uint32_t  drizzle_get_proto_info(const DRIZZLE *drizzle);
DRIZZLE_RES *  drizzle_list_tables(DRIZZLE *drizzle,const char *wild);
DRIZZLE_RES *  drizzle_list_processes(DRIZZLE *drizzle);
int32_t    drizzle_options(DRIZZLE *drizzle,enum drizzle_option option, const void *arg);
void    drizzle_free_result(DRIZZLE_RES *result);
void    drizzle_data_seek(DRIZZLE_RES *result, uint64_t offset);
DRIZZLE_ROW_OFFSET drizzle_row_seek(DRIZZLE_RES *result, DRIZZLE_ROW_OFFSET offset);
DRIZZLE_FIELD_OFFSET drizzle_field_seek(DRIZZLE_RES *result, DRIZZLE_FIELD_OFFSET offset);
DRIZZLE_ROW  drizzle_fetch_row(DRIZZLE_RES *result);
uint32_t * drizzle_fetch_lengths(DRIZZLE_RES *result);
DRIZZLE_FIELD *  drizzle_fetch_field(DRIZZLE_RES *result);
DRIZZLE_RES *     drizzle_list_fields(DRIZZLE *drizzle, const char *table, const char *wild);
uint32_t  drizzle_escape_string(char *to,const char *from, uint32_t from_length);
uint32_t  drizzle_hex_string(char *to,const char *from, uint32_t from_length);
bool         drizzle_read_query_result(DRIZZLE *drizzle);



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
  DRIZZLE_DATA *(*read_rows)(DRIZZLE *drizzle,DRIZZLE_FIELD *drizzle_fields, uint32_t fields);
  DRIZZLE_RES * (*use_result)(DRIZZLE *drizzle);
  void (*fetch_lengths)(uint32_t *to, DRIZZLE_ROW column, uint32_t field_count);
  void (*flush_use_result)(DRIZZLE *drizzle);
  DRIZZLE_FIELD * (*list_fields)(DRIZZLE *drizzle);
  int32_t (*unbuffered_fetch)(DRIZZLE *drizzle, char **row);
  const char *(*read_statistics)(DRIZZLE *drizzle);
  bool (*next_result)(DRIZZLE *drizzle);
  int32_t (*read_change_user_result)(DRIZZLE *drizzle);
} DRIZZLE_METHODS;


bool drizzle_commit(DRIZZLE *drizzle);
bool drizzle_rollback(DRIZZLE *drizzle);
bool drizzle_autocommit(DRIZZLE *drizzle, bool auto_mode);
bool drizzle_more_results(const DRIZZLE *drizzle);
int drizzle_next_result(DRIZZLE *drizzle);
void drizzle_close(DRIZZLE *sock);


/* status return codes */
#define DRIZZLE_NO_DATA        100
#define DRIZZLE_DATA_TRUNCATED 101


#define drizzle_reload(drizzle) drizzle_refresh((drizzle),REFRESH_GRANT)

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
