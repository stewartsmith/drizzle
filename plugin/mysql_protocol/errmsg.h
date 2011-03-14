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

#pragma once

namespace drizzle_plugin
{

/* Error messages for MySQL clients */
enum CR_CLIENT_ERRORS {
  CR_ERROR_FIRST    =2000, /*Copy first error nr.*/
  CR_UNKNOWN_ERROR  =2000,
  CR_SOCKET_CREATE_ERROR  =2001,
  CR_CONNECTION_ERROR  =2002,
  CR_CONN_HOST_ERROR  =2003,
  CR_IPSOCK_ERROR    =2004,
  CR_UNKNOWN_HOST    =2005,
  CR_SERVER_GONE_ERROR  =2006,
  CR_VERSION_ERROR  =2007,
  CR_OUT_OF_MEMORY  =2008,
  CR_WRONG_HOST_INFO  =2009,
  CR_LOCALHOST_CONNECTION =2010,
  CR_TCP_CONNECTION  =2011,
  CR_SERVER_HANDSHAKE_ERR =2012,
  CR_SERVER_LOST    =2013,
  CR_COMMANDS_OUT_OF_SYNC =2014,
  CR_NAMEDPIPE_CONNECTION =2015,
  CR_NAMEDPIPEWAIT_ERROR  =2016,
  CR_NAMEDPIPEOPEN_ERROR  =2017,
  CR_NAMEDPIPESETSTATE_ERROR =2018,
  CR_CANT_READ_CHARSET  =2019,
  CR_NET_PACKET_TOO_LARGE =2020,
  CR_EMBEDDED_CONNECTION  =2021,
  CR_PROBE_SLAVE_STATUS   =2022,
  CR_PROBE_SLAVE_HOSTS    =2023,
  CR_PROBE_SLAVE_CONNECT  =2024,
  CR_PROBE_MASTER_CONNECT =2025,
  CR_SSL_CONNECTION_ERROR =2026,
  CR_MALFORMED_PACKET     =2027,

  CR_NULL_POINTER    =2029,
  CR_NO_PREPARE_STMT  =2030,
  CR_PARAMS_NOT_BOUND  =2031,
  CR_DATA_TRUNCATED  =2032,
  CR_NO_PARAMETERS_EXISTS =2033,
  CR_INVALID_PARAMETER_NO =2034,
  CR_INVALID_BUFFER_USE  =2035,
  CR_UNSUPPORTED_PARAM_TYPE =2036,

  CR_CONN_UNKNOW_PROTOCOL     =2047,
  CR_INVALID_CONN_HANDLE      =2048,
  CR_SECURE_AUTH                          =2049,
  CR_FETCH_CANCELED                       =2050,
  CR_NO_DATA                              =2051,
  CR_NO_STMT_METADATA                     =2052,
  CR_NO_RESULT_SET                        =2053,
  CR_NOT_IMPLEMENTED                      =2054,
  CR_SERVER_LOST_INITIAL_COMM_WAIT  =2055,
  CR_SERVER_LOST_INITIAL_COMM_READ  =2056,
  CR_SERVER_LOST_SEND_AUTH    =2057,
  CR_SERVER_LOST_READ_AUTH    =2058,
  CR_SERVER_LOST_SETTING_DB    =2059,

  CR_STMT_CLOSED  =2060,

  CR_NET_UNCOMPRESS_ERROR= 2061,
  CR_NET_READ_ERROR= 2062,
  CR_NET_READ_INTERRUPTED= 2063,
  CR_NET_ERROR_ON_WRITE= 2064,
  CR_NET_WRITE_INTERRUPTED= 2065,

  /* Add error numbers before CR_ERROR_LAST and change it accordingly. */
  CR_ERROR_LAST    =2065 /*Copy last error nr:*/
};

} /* namespace drizzle_plugin */

