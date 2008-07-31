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

#ifndef _libdrizzle_drizzle_h
#define _libdrizzle_drizzle_h

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _global_h        /* If not standard header */
#include <sys/types.h>
typedef char my_bool;
#define STDCALL

#ifndef my_socket_defined
typedef int my_socket;
#endif /* my_socket_defined */
#endif /* _global_h */

#include <drizzled/version.h>
#include "drizzle_com.h"
#include "drizzle_time.h"

#include <mysys/my_list.h> /* for LISTs used in 'MYSQL' */

extern unsigned int drizzle_port;
extern char *drizzle_unix_port;

#define CLIENT_NET_READ_TIMEOUT    365*24*3600  /* Timeout on read */
#define CLIENT_NET_WRITE_TIMEOUT  365*24*3600  /* Timeout on write */

#define IS_PRI_KEY(n)  ((n) & PRI_KEY_FLAG)
#define IS_NOT_NULL(n)  ((n) & NOT_NULL_FLAG)
#define IS_BLOB(n)  ((n) & BLOB_FLAG)
#define IS_NUM(t)  ((t) <= DRIZZLE_TYPE_LONGLONG || (t) == DRIZZLE_TYPE_NEWDECIMAL)
#define IS_NUM_FIELD(f)   ((f)->flags & NUM_FLAG)
#define INTERNAL_NUM_FIELD(f) (((f)->type <= DRIZZLE_TYPE_LONGLONG && ((f)->type != DRIZZLE_TYPE_TIMESTAMP || (f)->length == 14 || (f)->length == 8)))
#define IS_LONGDATA(t) ((t) >= DRIZZLE_TYPE_TINY_BLOB && (t) <= DRIZZLE_TYPE_STRING)


typedef struct st_drizzle_field {
  char *name;                 /* Name of column */
  char *org_name;             /* Original column name, if an alias */
  char *table;                /* Table of column if column was a field */
  char *org_table;            /* Org table name, if table was an alias */
  char *db;                   /* Database for table */
  char *catalog;        /* Catalog for table */
  char *def;                  /* Default value (set by drizzle_list_fields) */
  unsigned long length;       /* Width of column (create length) */
  unsigned long max_length;   /* Max width for selected set */
  unsigned int name_length;
  unsigned int org_name_length;
  unsigned int table_length;
  unsigned int org_table_length;
  unsigned int db_length;
  unsigned int catalog_length;
  unsigned int def_length;
  unsigned int flags;         /* Div flags */
  unsigned int decimals;      /* Number of decimals in field */
  unsigned int charsetnr;     /* Character set */
  enum enum_field_types type; /* Type of field. See drizzle_com.h for types */
  void *extension;
} DRIZZLE_FIELD;

typedef char **DRIZZLE_ROW;    /* return data as array of strings */
typedef unsigned int DRIZZLE_FIELD_OFFSET; /* offset to current field */

#include <mysys/typelib.h>

#define DRIZZLE_COUNT_ERROR (~(uint64_t) 0)

typedef struct st_drizzle_rows {
  struct st_drizzle_rows *next;    /* list of rows */
  DRIZZLE_ROW data;
  unsigned long length;
} DRIZZLE_ROWS;

typedef DRIZZLE_ROWS *DRIZZLE_ROW_OFFSET;  /* offset to current row */

#include <mysys/my_alloc.h>

typedef struct embedded_query_result EMBEDDED_QUERY_RESULT;
typedef struct st_drizzle_data {
  DRIZZLE_ROWS *data;
  struct embedded_query_result *embedded_info;
  MEM_ROOT alloc;
  uint64_t rows;
  unsigned int fields;
  /* extra info for embedded library */
  void *extension;
} DRIZZLE_DATA;

enum drizzle_option
{
  DRIZZLE_OPT_CONNECT_TIMEOUT, DRIZZLE_OPT_COMPRESS, DRIZZLE_OPT_NAMED_PIPE,
  DRIZZLE_INIT_COMMAND, DRIZZLE_READ_DEFAULT_FILE, DRIZZLE_READ_DEFAULT_GROUP,
  DRIZZLE_SET_CHARSET_DIR, DRIZZLE_SET_CHARSET_NAME, DRIZZLE_OPT_LOCAL_INFILE,
  DRIZZLE_OPT_PROTOCOL, DRIZZLE_SHARED_MEMORY_BASE_NAME, DRIZZLE_OPT_READ_TIMEOUT,
  DRIZZLE_OPT_WRITE_TIMEOUT, DRIZZLE_OPT_USE_RESULT,
  DRIZZLE_OPT_USE_REMOTE_CONNECTION, DRIZZLE_OPT_USE_EMBEDDED_CONNECTION,
  DRIZZLE_OPT_GUESS_CONNECTION, DRIZZLE_SET_CLIENT_IP, DRIZZLE_SECURE_AUTH,
  DRIZZLE_REPORT_DATA_TRUNCATION, DRIZZLE_OPT_RECONNECT,
  DRIZZLE_OPT_SSL_VERIFY_SERVER_CERT
};

struct st_drizzle_options {
  unsigned int connect_timeout, read_timeout, write_timeout;
  unsigned int port, protocol;
  unsigned long client_flag;
  char *host,*user,*password,*unix_socket,*db;
  struct st_dynamic_array *init_commands;
  char *my_cnf_file,*my_cnf_group, *charset_dir, *charset_name;
  char *ssl_key;        /* PEM key file */
  char *ssl_cert;        /* PEM cert file */
  char *ssl_ca;          /* PEM CA file */
  char *ssl_capath;        /* PEM directory of CA-s? */
  char *ssl_cipher;        /* cipher to use */
  char *shared_memory_base_name;
  unsigned long max_allowed_packet;
  my_bool use_ssl;        /* if to use SSL or not */
  my_bool compress,named_pipe;
  my_bool unused1;
  my_bool unused2;
  my_bool unused3;
  my_bool unused4;
  enum drizzle_option methods_to_use;
  char *client_ip;
  /* Refuse client connecting to server if it uses old (pre-4.1.1) protocol */
  my_bool secure_auth;
  /* 0 - never report, 1 - always report (default) */
  my_bool report_data_truncation;

  /* function pointers for local infile support */
  int (*local_infile_init)(void **, const char *, void *);
  int (*local_infile_read)(void *, char *, unsigned int);
  void (*local_infile_end)(void *);
  int (*local_infile_error)(void *, char *, unsigned int);
  void *local_infile_userdata;
  void *extension;
};

enum drizzle_status
{
  DRIZZLE_STATUS_READY,DRIZZLE_STATUS_GET_RESULT,DRIZZLE_STATUS_USE_RESULT
};

enum drizzle_protocol_type
{
  DRIZZLE_PROTOCOL_DEFAULT, DRIZZLE_PROTOCOL_TCP, DRIZZLE_PROTOCOL_SOCKET,
  DRIZZLE_PROTOCOL_PIPE, DRIZZLE_PROTOCOL_MEMORY
};

typedef struct character_set
{
  unsigned int      number;     /* character set number              */
  unsigned int      state;      /* character set state               */
  const char        *csname;    /* collation name                    */
  const char        *name;      /* character set name                */
  const char        *comment;   /* comment                           */
  const char        *dir;       /* character set directory           */
  unsigned int      mbminlen;   /* min. length for multibyte strings */
  unsigned int      mbmaxlen;   /* max. length for multibyte strings */
} MY_CHARSET_INFO;

struct st_drizzle_methods;
struct st_drizzle_stmt;

typedef struct st_drizzle
{
  NET    net;      /* Communication parameters */
  unsigned char  *connector_fd;    /* ConnectorFd for SSL */
  char    *host,*user,*passwd,*unix_socket,*server_version,*host_info;
  char          *info, *db;
  struct charset_info_st *charset;
  DRIZZLE_FIELD  *fields;
  MEM_ROOT  field_alloc;
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

  LIST  *stmts;                     /* list of all statements */
  const struct st_drizzle_methods *methods;
  void *thd;
  /*
    Points to boolean flag in DRIZZLE_RES  or MYSQL_STMT. We set this flag
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
  MEM_ROOT  field_alloc;
  uint32_t  field_count, current_field;
  bool  eof;      /* Used by drizzle_fetch_row */
  /* drizzle_stmt_close() had to cancel this result */
  bool       unbuffered_fetch_cancelled; 
  void *extension;
} DRIZZLE_RES;


#if !defined(MYSQL_SERVER) && !defined(MYSQL_CLIENT)
#define MYSQL_CLIENT
#endif


typedef struct st_drizzle_parameters
{
  uint32_t *p_max_allowed_packet;
  uint32_t *p_net_buffer_length;
  void *extension;
} DRIZZLE_PARAMETERS;

#if !defined(MYSQL_SERVER)
#define max_allowed_packet (*drizzle_get_parameters()->p_max_allowed_packet)
#define net_buffer_length (*drizzle_get_parameters()->p_net_buffer_length)
#endif

/*
  Set up and bring down the server; to ensure that applications will
  work when linked against either the standard client library or the
  embedded server library, these functions should be called.
*/
void STDCALL drizzle_server_end(void);

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

const DRIZZLE_PARAMETERS *STDCALL drizzle_get_parameters(void);

/*
  Set up and bring down a thread; these function should be called
  for each thread in an application which opens at least one MySQL
  connection.  All uses of the connection(s) should be between these
  function calls.
*/
bool STDCALL drizzle_thread_init(void);
void STDCALL drizzle_thread_end(void);

/*
  Functions to get information from the DRIZZLE and DRIZZLE_RES structures
  Should definitely be used if one uses shared libraries.
*/

uint64_t STDCALL drizzle_num_rows(const DRIZZLE_RES *res);
unsigned int STDCALL drizzle_num_fields(const DRIZZLE_RES *res);
bool STDCALL drizzle_eof(const DRIZZLE_RES *res);
const DRIZZLE_FIELD *STDCALL drizzle_fetch_field_direct(const DRIZZLE_RES *res,
                unsigned int fieldnr);
const DRIZZLE_FIELD * STDCALL drizzle_fetch_fields(const DRIZZLE_RES *res);
DRIZZLE_ROW_OFFSET STDCALL drizzle_row_tell(const DRIZZLE_RES *res);
DRIZZLE_FIELD_OFFSET STDCALL drizzle_field_tell(const DRIZZLE_RES *res);

uint32_t STDCALL drizzle_field_count(const DRIZZLE *drizzle);
uint64_t STDCALL drizzle_affected_rows(const DRIZZLE *drizzle);
uint64_t STDCALL drizzle_insert_id(const DRIZZLE *drizzle);
uint32_t STDCALL drizzle_errno(const DRIZZLE *drizzle);
const char * STDCALL drizzle_error(const DRIZZLE *drizzle);
const char *STDCALL drizzle_sqlstate(const DRIZZLE *drizzle);
uint32_t STDCALL drizzle_warning_count(const DRIZZLE *drizzle);
const char * STDCALL drizzle_info(const DRIZZLE *drizzle);
uint32_t STDCALL drizzle_thread_id(const DRIZZLE *drizzle);
const char * STDCALL drizzle_character_set_name(const DRIZZLE *drizzle);
int32_t          STDCALL drizzle_set_character_set(DRIZZLE *drizzle, const char *csname);

DRIZZLE * STDCALL drizzle_create(DRIZZLE *drizzle);
bool   STDCALL drizzle_change_user(DRIZZLE *drizzle, const char *user,
            const char *passwd, const char *db);
DRIZZLE * STDCALL drizzle_connect(DRIZZLE *drizzle, const char *host,
             const char *user,
             const char *passwd,
             const char *db,
             uint32_t port,
             const char *unix_socket,
             uint32_t clientflag);
int32_t    STDCALL drizzle_select_db(DRIZZLE *drizzle, const char *db);
int32_t    STDCALL drizzle_query(DRIZZLE *drizzle, const char *q);
int32_t    STDCALL drizzle_send_query(DRIZZLE *drizzle, const char *q, uint32_t length);
int32_t    STDCALL drizzle_real_query(DRIZZLE *drizzle, const char *q, uint32_t length);
DRIZZLE_RES * STDCALL drizzle_store_result(DRIZZLE *drizzle);
DRIZZLE_RES * STDCALL drizzle_use_result(DRIZZLE *drizzle);

void        STDCALL drizzle_get_character_set_info(const DRIZZLE *drizzle,
                                                   MY_CHARSET_INFO *charset);

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

int32_t    STDCALL drizzle_shutdown(DRIZZLE *drizzle, enum drizzle_enum_shutdown_level shutdown_level);
int32_t    STDCALL drizzle_dump_debug_info(DRIZZLE *drizzle);
int32_t    STDCALL drizzle_refresh(DRIZZLE *drizzle, uint32_t refresh_options);
int32_t    STDCALL drizzle_kill(DRIZZLE *drizzle, uint32_t pid);
int32_t    STDCALL drizzle_set_server_option(DRIZZLE *drizzle, enum enum_drizzle_set_option option);
int32_t    STDCALL drizzle_ping(DRIZZLE *drizzle);
const char *  STDCALL drizzle_stat(DRIZZLE *drizzle);
const char *  STDCALL drizzle_get_server_info(const DRIZZLE *drizzle);
const char *  STDCALL drizzle_get_client_info(void);
uint32_t  STDCALL drizzle_get_client_version(void);
const char *  STDCALL drizzle_get_host_info(const DRIZZLE *drizzle);
uint32_t  STDCALL drizzle_get_server_version(const DRIZZLE *drizzle);
uint32_t  STDCALL drizzle_get_proto_info(const DRIZZLE *drizzle);
DRIZZLE_RES *  STDCALL drizzle_list_dbs(DRIZZLE *drizzle,const char *wild);
DRIZZLE_RES *  STDCALL drizzle_list_tables(DRIZZLE *drizzle,const char *wild);
DRIZZLE_RES *  STDCALL drizzle_list_processes(DRIZZLE *drizzle);
int32_t    STDCALL drizzle_options(DRIZZLE *drizzle,enum drizzle_option option, const void *arg);
void    STDCALL drizzle_free_result(DRIZZLE_RES *result);
void    STDCALL drizzle_data_seek(DRIZZLE_RES *result, uint64_t offset);
DRIZZLE_ROW_OFFSET STDCALL drizzle_row_seek(DRIZZLE_RES *result, DRIZZLE_ROW_OFFSET offset);
DRIZZLE_FIELD_OFFSET STDCALL drizzle_field_seek(DRIZZLE_RES *result, DRIZZLE_FIELD_OFFSET offset);
DRIZZLE_ROW  STDCALL drizzle_fetch_row(DRIZZLE_RES *result);
uint32_t * STDCALL drizzle_fetch_lengths(DRIZZLE_RES *result);
DRIZZLE_FIELD *  STDCALL drizzle_fetch_field(DRIZZLE_RES *result);
DRIZZLE_RES *     STDCALL drizzle_list_fields(DRIZZLE *drizzle, const char *table, const char *wild);
uint32_t  STDCALL drizzle_escape_string(char *to,const char *from, uint32_t from_length);
uint32_t  STDCALL drizzle_hex_string(char *to,const char *from, uint32_t from_length);
uint32_t        STDCALL drizzle_real_escape_string(DRIZZLE *drizzle, char *to, const char *from, uint32_t length);
void    STDCALL myodbc_remove_escape(const DRIZZLE *drizzle, char *name);
uint32_t  STDCALL drizzle_thread_safe(void);
bool    STDCALL drizzle_embedded(void);
bool         STDCALL drizzle_read_query_result(DRIZZLE *drizzle);



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


bool STDCALL drizzle_commit(DRIZZLE *drizzle);
bool STDCALL drizzle_rollback(DRIZZLE *drizzle);
bool STDCALL drizzle_autocommit(DRIZZLE *drizzle, bool auto_mode);
bool STDCALL drizzle_more_results(const DRIZZLE *drizzle);
int STDCALL drizzle_next_result(DRIZZLE *drizzle);
void STDCALL drizzle_close(DRIZZLE *sock);


/* status return codes */
#define DRIZZLE_NO_DATA        100
#define DRIZZLE_DATA_TRUNCATED 101


#define DRIZZLE_PROTOCOL_NO_MORE_DATA 0xFE

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

#endif /* _libdrizzle_drizzle_h */
