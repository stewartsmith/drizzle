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

#ifndef LIBDRIZZLECLIENT_DRIZZLE_OPTIONS_H
#define LIBDRIZZLECLIENT_DRIZZLE_OPTIONS_H

#ifdef  __cplusplus
extern "C" {
#endif

enum drizzle_option
{
  DRIZZLE_OPT_CONNECT_TIMEOUT, DRIZZLE_OPT_COMPRESS, DRIZZLE_OPT_NAMED_PIPE,
  DRIZZLE_INIT_COMMAND, DRIZZLE_READ_DEFAULT_FILE, DRIZZLE_READ_DEFAULT_GROUP,
  DRIZZLE_OPT_PROTOCOL, DRIZZLE_SHARED_MEMORY_BASE_NAME, DRIZZLE_OPT_READ_TIMEOUT,
  DRIZZLE_OPT_WRITE_TIMEOUT, DRIZZLE_OPT_USE_RESULT,
  DRIZZLE_OPT_USE_REMOTE_CONNECTION,
  DRIZZLE_OPT_GUESS_CONNECTION, DRIZZLE_SET_CLIENT_IP, DRIZZLE_SECURE_AUTH,
  DRIZZLE_REPORT_DATA_TRUNCATION, DRIZZLE_OPT_RECONNECT,
  DRIZZLE_OPT_SSL_VERIFY_SERVER_CERT
};

struct st_drizzleclient_options {
  unsigned int connect_timeout, read_timeout, write_timeout;
  unsigned int port;
  unsigned long client_flag;
  char *host,*user,*password,*db;
  char *my_cnf_file,*my_cnf_group;
  char *ssl_key;        /* PEM key file */
  char *ssl_cert;        /* PEM cert file */
  char *ssl_ca;          /* PEM CA file */
  char *ssl_capath;        /* PEM directory of CA-s? */
  char *ssl_cipher;        /* cipher to use */
  char *shared_memory_base_name;
  unsigned long max_allowed_packet;
  bool use_ssl;        /* if to use SSL or not */
  bool compress,named_pipe;
  bool unused1;
  bool unused2;
  bool unused3;
  bool unused4;
  enum drizzle_option methods_to_use;
  char *client_ip;
  /* Refuse client connecting to server if it uses old (pre-4.1.1) protocol */
  bool secure_auth;
  /* 0 - never report, 1 - always report (default) */
  bool report_data_truncation;

  /* function pointers for local infile support */
  int (*local_infile_init)(void **, const char *, void *);
  int (*local_infile_read)(void *, char *, unsigned int);
  void (*local_infile_end)(void *);
  int (*local_infile_error)(void *, char *, unsigned int);
  void *local_infile_userdata;
  void *extension;
};


#ifdef  __cplusplus
}
#endif

#endif /* LIBDRIZZLECLIENT_DRIZZLE_OPTIONS_H */
