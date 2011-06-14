/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Original author: Paul McCullagh (H&G2JCtL)
 * Continued development: Barry Leslie
 *
 * 2007-07-04
 *
 * Global definitions.
 *
 */

#pragma once
#ifndef __DEFS_MS_H__
#define __DEFS_MS_H__

#include "cslib/CSDefs.h"

#define MS_IDENTIFIER_CHAR_COUNT	64

#define MS_IDENTIFIER_NAME_SIZE		((MS_IDENTIFIER_CHAR_COUNT * 3) + 1)	// The identifier length as UTF-8

#define MS_TABLE_NAME_SIZE			MS_IDENTIFIER_NAME_SIZE					// The maximum length of a table name 
#define MS_DATABASE_NAME_SIZE		MS_IDENTIFIER_NAME_SIZE

#define MS_TABLE_URL_SIZE			(MS_DATABASE_NAME_SIZE + MS_TABLE_NAME_SIZE)

#define MS_CONNECTION_THREAD		1000
#define MS_TEMP_LOG_THREAD			1001
#define MS_COMPACTOR_THREAD			1002

#ifdef DEBUG
#define MS_DEFAULT_TEMP_LOG_WAIT	(200*60)

// Set MS_DEFAULT_TEMP_LOG_WAIT high to prevent 
// BLOBs from being deleted while walking through 
// the code in the debugger.
//#define MS_DEFAULT_TEMP_LOG_WAIT	(200*60) 
#else
#define MS_DEFAULT_TEMP_LOG_WAIT	(10*60)
#endif

/* Default compactor wait time in seconds! */
#define MS_COMPACTOR_POLLS
#ifdef MS_COMPACTOR_POLLS
#ifdef DEBUG
#define MS_COMPACTOR_POLL_FREQ		1000		// milli-seconds
#else
#define MS_COMPACTOR_POLL_FREQ		3000
#endif
#else
#ifdef DEBUG
#define MS_DEFAULT_COMPACTOR_WAIT	120
#else
#define MS_DEFAULT_COMPACTOR_WAIT	30
#endif
#endif

#ifdef DEBUG
//#define MS_DEFAULT_GARBAGE_LEVEL	1
#define MS_DEFAULT_GARBAGE_LEVEL	10
#else
#define MS_DEFAULT_GARBAGE_LEVEL	50
#endif

#ifdef DEBUG
#define MS_REPO_THRESHOLD_DEF		"20MB"
//#define MS_REPO_THRESHOLD_DEF		"32K"
#else
#define MS_REPO_THRESHOLD_DEF		"128MB"
#endif

#ifdef DEBUG
#define MS_TEMP_LOG_THRESHOLD_DEF	"32K"
#else
#define MS_TEMP_LOG_THRESHOLD_DEF	"32MB"
#endif

#define MS_HTTP_METADATA_HEADERS_DEF "Content-Type"

#ifdef DEBUG
#define MS_COMPACTOR_BUFFER_SIZE	(4*1024)
#else
#define MS_COMPACTOR_BUFFER_SIZE	(64*1024)
#endif

#define MS_BACKUP_BUFFER_SIZE MS_COMPACTOR_BUFFER_SIZE

/*
 * The time (in seconds) that a connection thread will stay alive, when it is idle:
 */
#ifdef DEBUG
#define MS_IDLE_THREAD_TIMEOUT		(10)
#else
#define MS_IDLE_THREAD_TIMEOUT		(40)
#endif

/*
 * The timeout, in milli-seconds, before the HTTP server will close an inactive HTTP connection.
*/
#define MS_DEFAULT_KEEP_ALIVE		(10) 

#ifdef DRIZZLED
#include <boost/dynamic_bitset.hpp>
/* Drizzle is stuck at this level: */
#define MYSQL_VERSION_ID					60005

#define TABLE_LIST							TableList
#define TABLE								drizzled::Table
#define Field								drizzled::Field
//#define enum_field_types					drizzled::enum_field_types

#define my_charset_bin						drizzled::my_charset_bin
#define THR_LOCK							drizzled::THR_LOCK

#define TABLE_SHARE							TableShare
#define THD									drizzled::Session
#define MYSQL_THD							Session *
#define THR_THD								THR_Session
#define STRUCT_TABLE						class Table
#define MY_BITMAP							boost::dynamic_bitset<>

#define MYSQL_TYPE_TIMESTAMP				DRIZZLE_TYPE_TIMESTAMP
#define MYSQL_TYPE_LONG						DRIZZLE_TYPE_LONG
#define MYSQL_TYPE_SHORT					DRIZZLE_TYPE_LONG
#define MYSQL_TYPE_STRING					DRIZZLE_TYPE_VARCHAR
#define MYSQL_TYPE_VARCHAR					DRIZZLE_TYPE_VARCHAR
#define MYSQL_TYPE_LONGLONG					DRIZZLE_TYPE_LONGLONG
#define MYSQL_TYPE_BLOB						DRIZZLE_TYPE_BLOB
#define MYSQL_TYPE_LONG_BLOB				DRIZZLE_TYPE_BLOB
#define MYSQL_TYPE_ENUM						DRIZZLE_TYPE_ENUM
#define MYSQL_PLUGIN_VAR_HEADER				DRIZZLE_PLUGIN_VAR_HEADER
#define MYSQL_STORAGE_ENGINE_PLUGIN			DRIZZLE_STORAGE_ENGINE_PLUGIN
#define MYSQL_INFORMATION_SCHEMA_PLUGIN		DRIZZLE_INFORMATION_SCHEMA_PLUGIN
#define memcpy_fixed						memcpy
#define bfill(m, len, ch)					memset(m, ch, len)

#define mx_tmp_use_all_columns(x, y)		(x)->use_all_columns(y)
#define mx_tmp_restore_column_map(x, y)		(x)->restore_column_map(y)

#define MX_TABLE_TYPES_T					handler::Table_flags
#define MX_UINT8_T							uint8_t
#define MX_ULONG_T							uint32_t
#define MX_ULONGLONG_T						uint64_t
#define MX_LONGLONG_T						uint64_t
#define MX_CHARSET_INFO						struct charset_info_st
#define MX_CONST_CHARSET_INFO				const struct charset_info_st			
#define MX_CONST							const
#define my_bool								bool
#define int16								int16_t
#define int32								int32_t
#define uint16								uint16_t
#define uint32								uint32_t
#define uchar								unsigned char
#define longlong							int64_t
#define ulonglong							uint64_t

#define HAVE_LONG_LONG

#define my_malloc(x, y)						malloc(x)
#define my_free(x, y)						free(x)

#define HA_CAN_SQL_HANDLER					0
#define HA_CAN_INSERT_DELAYED				0
#define HA_BINLOG_ROW_CAPABLE				0

#define max									cmax
#define min									cmin

#define NullS								NULL

#define current_thd							current_session
#define thd_charset							session_charset
#define thd_query							session_query
#define thd_slave_thread					session_slave_thread
#define thd_non_transactional_update		session_non_transactional_update
#define thd_binlog_format					session_binlog_format
#define thd_mark_transaction_to_rollback	session_mark_transaction_to_rollback
#define current_thd							current_session
#define thd_sql_command(x)						((x)->getSqlCommand())
#define thd_test_options					session_test_options
#define thd_killed							session_killed
#define thd_tx_isolation(x)					((x)->getTxIsolation())
#define thd_in_lock_tables					session_in_lock_tables
#define thd_tablespace_op(x)					((x)->doingTablespaceOperation())
#define thd_alloc							session_alloc
#define thd_make_lex_string					session_make_lex_string

#define my_pthread_setspecific_ptr(T, V)	pthread_setspecific(T, (void*) (V))

#define mysql_real_data_home				drizzle_real_data_home

#define mi_int4store(T,A)   { uint32_t def_temp= (uint32_t) (A);\
                              ((unsigned char*) (T))[3]= (unsigned char) (def_temp);\
                              ((unsigned char*) (T))[2]= (unsigned char) (def_temp >> 8);\
                              ((unsigned char*) (T))[1]= (unsigned char) (def_temp >> 16);\
                              ((unsigned char*) (T))[0]= (unsigned char) (def_temp >> 24); }

#define mi_uint4korr(A) ((uint32_t) (((uint32_t) (((const unsigned char*) (A))[3])) +\
                                   (((uint32_t) (((const unsigned char*) (A))[2])) << 8) +\
                                   (((uint32_t) (((const unsigned char*) (A))[1])) << 16) +\
                                   (((uint32_t) (((const unsigned char*) (A))[0])) << 24)))
								   
#define mi_int8store(T,A)   { uint64_t def_temp= (uint64_t) (A);\
                              ((unsigned char*) (T))[7]= (unsigned char) (def_temp);\
                              ((unsigned char*) (T))[6]= (unsigned char) (def_temp >> 8);\
                              ((unsigned char*) (T))[5]= (unsigned char) (def_temp >> 16);\
                              ((unsigned char*) (T))[4]= (unsigned char) (def_temp >> 24);\
                              ((unsigned char*) (T))[3]= (unsigned char) (def_temp >> 32);\
                              ((unsigned char*) (T))[2]= (unsigned char) (def_temp >> 40);\
                              ((unsigned char*) (T))[1]= (unsigned char) (def_temp >> 48);\
                              ((unsigned char*) (T))[0]= (unsigned char) (def_temp >> 56); }

#define mi_uint8korr(A) ((uint64_t) (((uint64_t) (((const unsigned char*) (A))[7])) +\
                                   (((uint64_t) (((const unsigned char*) (A))[6])) << 8) +\
                                   (((uint64_t) (((const unsigned char*) (A))[5])) << 16) +\
                                   (((uint64_t) (((const unsigned char*) (A))[4])) << 24) +\
                                   (((uint64_t) (((const unsigned char*) (A))[3])) << 32) +\
                                   (((uint64_t) (((const unsigned char*) (A))[2])) << 40) +\
                                   (((uint64_t) (((const unsigned char*) (A))[1])) << 48) +\
                                   (((uint64_t) (((const unsigned char*) (A))[0])) << 56)))
								   
#else // DRIZZLED
/* The MySQL case: */
#define STRUCT_TABLE						struct st_table

#define mx_tmp_use_all_columns				dbug_tmp_use_all_columns
#define mx_tmp_restore_column_map(x, y)		dbug_tmp_restore_column_map((x)->read_set, y)

#define MX_TABLE_TYPES_T					ulonglong
#define MX_UINT8_T							uint8
#define MX_ULONG_T							ulong
#define MX_ULONGLONG_T						ulonglong
#define MX_LONGLONG_T						longlong
#define MX_CHARSET_INFO						CHARSET_INFO
#define MX_CONST_CHARSET_INFO				struct charset_info_st			
#define MX_CONST							

#endif // DRIZZLED

#endif
