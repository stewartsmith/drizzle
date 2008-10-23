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

/**
 * @file
 * 
 * Contains #includes and definitions that apply to ALL server-related
 * executables, including storage engine plugins.
 *
 * @details
 *
 * Previously, the mysql_priv.h file contained a number of conditional
 * #ifdef DRIZZLE_SERVER blocks which made it very difficult to determine
 * which headers and definitions were actually necessary for plugins to 
 * include.  The file, and NOT mysql_priv.h, should now be the main included
 * header for storage engine plugins, as it contains all definitions and 
 * declarations needed by the plugin and nothing more.
 */
#ifndef DRIZZLE_SERVER_COMMON_INCLUDES_H
#define DRIZZLE_SERVER_COMMON_INCLUDES_H


/* Cross-platform portability code and standard includes */
#include <drizzled/global.h>                    
/* Lots of system-wide struct definitions like IO_CACHE, prototypes for all my_* functions */
#include <mysys/my_sys.h>                       
/* Convenience functions for working with times */
#include <mysys/my_time.h>
/* Custom C string functions */
#include <mystrings/m_string.h>
/* Custom HASH API */
#include <mysys/hash.h>
/* Standard signals API */
#include <signal.h>
/* Deadlock-free table-list lock API */
#include <mysys/thr_lock.h>
/* Defines for the storage engine handler -- i.e. HA_XXX defines */
#include <drizzled/base.h>			                /* Needed by field.h */
/* Custom queue API */
#include <mysys/queues.h>
/* Custom Bitmap API */
#include <drizzled/sql_bitmap.h>
/* Array of pointers to Elem that uses memory from MEM_ROOT */
#include "sql_array.h"
/* The <strong>INTERNAL</strong> plugin API - not the external, or public, server plugin API */
#include "sql_plugin.h"
/* The <strong>connection</strong> thread scheduler API */
#include "scheduler.h"
/* Network database operations (hostent, netent, servent, etc...*/
#include <netdb.h>
/* Definitions that the client and the server have in common */
#include <drizzled/common.h>
/* Contains system-wide constants and #defines */
#include <drizzled/definitions.h>
/* System-wide common data structures */
#include <drizzled/structs.h>
/* Custom continguous-section memory allocator */
#include <drizzled/sql_alloc.h>
/* Definition of the MY_LOCALE struct and some convenience functions */
#include <drizzled/sql_locale.h>

#include <drizzled/korr.h>

#ifdef HAVE_DTRACE
#define _DTRACE_VERSION 1
#else
#undef _DTRACE_VERSION
#endif
#include "probes.h"

/**
  Query type constants.

  QT_ORDINARY -- ordinary SQL query.
  QT_IS -- SQL query to be shown in INFORMATION_SCHEMA (in utf8 and without
  character set introducers).

  @TODO

  Move this out of here once Stew's done with UDF breakout.  The following headers need it:

    sql_lex.h --> included by sql_class.h
    item.h
    table.h
    item_func.h
    item_subselect.h
    item_timefunc.h
    item_sum.h
    item_cmpfunc.h
    item_strfunc.h
*/
enum enum_query_type
{
  QT_ORDINARY,
  QT_IS
};

/** 
 * @TODO convert all these three maps to Bitmap classes 
 *
 * @TODO Move these to a more appropriate header file (maps.h?).  The following files use them:
 *
 *    item_sum.h
 *    item_compfunc.h
 *    item.h
 *    table.h
 *    item_subselect.h
 *    sql_bitmap.h
 *    unireg.h (going bye bye?)
 *    sql_udf.h
 *    item_row.h
 *    handler.cc
 *    sql_insert.cc
 *    opt_range.h
 *    opt_sum.cc
 *    item_strfunc.h
 *    sql_delete.cc
 *    sql_select.h
 *
 *    Since most of these include table.h, I think that would appropriate...
 */
typedef uint64_t table_map;          /* Used for table bits in join */
#if MAX_INDEXES <= 64
typedef Bitmap<64>  key_map;          /* Used for finding keys */
#else
typedef Bitmap<((MAX_INDEXES+7)/8*8)> key_map; /* Used for finding keys */
#endif
typedef uint32_t nesting_map;  /* Used for flags of nesting constructs */

/*
  Used to identify NESTED_JOIN structures within a join (applicable only to
  structures that have not been simplified away and embed more the one
  element)
*/
typedef uint64_t nested_join_map; /* Needed by sql_select.h and table.h */

/* useful constants */#
extern const key_map key_map_empty;
extern key_map key_map_full;          /* Should be threaded as const */
extern const char *primary_key_name;

/**
 * @TODO Move the following into a drizzled.h header?
 *
 * I feel that global variables and functions referencing them directly
 * and that are used only in the server should be separated out into 
 * a drizzled.h header file -- JRP
 */
typedef uint64_t query_id_t;
extern query_id_t global_query_id;

/* increment query_id and return it.  */
inline query_id_t next_query_id() { return global_query_id++; }

extern const CHARSET_INFO *system_charset_info, *files_charset_info ;
extern const CHARSET_INFO *national_charset_info, *table_alias_charset;

/**
 * @TODO Move to a separate header?
 *
 * It's needed by item.h and field.h, which are both inter-dependent
 * and contain forward declarations of many structs/classes in the
 * other header file.
 *
 * What is needed is a separate header file that is included
 * by *both* item.h and field.h to resolve inter-dependencies
 *
 * But, probably want to hold off on this until Stew finished the UDF cleanup
 */
enum Derivation
{
  DERIVATION_IGNORABLE= 5,
  DERIVATION_COERCIBLE= 4,
  DERIVATION_SYSCONST= 3,
  DERIVATION_IMPLICIT= 2,
  DERIVATION_NONE= 1,
  DERIVATION_EXPLICIT= 0
};

/**
 * Opening modes for open_temporary_table and open_table_from_share
 *
 * @TODO Put this into an appropriate header. It is only needed in:
 *
 *    table.cc
 *    sql_base.cc
 */
enum open_table_mode
{
  OTM_OPEN= 0,
  OTM_CREATE= 1,
  OTM_ALTER= 2
};

enum enum_parsing_place
{
  NO_MATTER
  , IN_HAVING
  , SELECT_LIST
  , IN_WHERE
  , IN_ON
};

enum enum_mysql_completiontype {
  ROLLBACK_RELEASE= -2
  , ROLLBACK= 1
  , ROLLBACK_AND_CHAIN= 7
  , COMMIT_RELEASE= -1
  , COMMIT= 0
  , COMMIT_AND_CHAIN= 6
};

enum enum_check_fields
{
  CHECK_FIELD_IGNORE
  , CHECK_FIELD_WARN
  , CHECK_FIELD_ERROR_FOR_NULL
};

enum enum_var_type
{
  OPT_DEFAULT= 0
  , OPT_SESSION
  , OPT_GLOBAL
};

/* Forward declarations */

class TableList;
class String;
class Table;
class Session;
class user_var_entry;
class Security_context;

extern pthread_key(Session*, THR_Session);
inline Session *_current_session(void)
{
  return (Session *)pthread_getspecific(THR_Session);
}
#define current_session _current_session()

extern "C" void set_session_proc_info(Session *session, const char *info);
extern "C" const char *get_session_proc_info(Session *session);

/*
  External variables
*/
extern ulong server_id;

/* Custom C++-style String class API */
#include <drizzled/sql_string.h>
/* Custom singly-linked list lite struct and full-blown type-safe, templatized class */
#include <drizzled/sql_list.h>
#include "my_decimal.h"
#include "handler.h"
#include <drizzled/table_list.h>
#include "sql_error.h"
/* Drizzle server data type class definitions */
#include <drizzled/field.h>
#include "protocol.h"
#include "item.h"

extern my_decimal decimal_zero;

/** @TODO Find a good header to put this guy... */
void close_thread_tables(Session *session);

#include <drizzled/sql_parse.h>

#include "sql_class.h"
#include "slave.h" // for tables_ok(), rpl_filter
#include "tztime.h"

void sql_perror(const char *message);

bool fn_format_relative_to_data_home(char * to, const char *name,
				     const char *dir, const char *extension);

/**
 * @TODO
 *
 * This is much better than the previous situation of a crap-ton
 * of conditional defines all over mysql_priv.h, but this still
 * is hackish.  Put these things into a separate header?  Or fix
 * InnoDB?  Or does the InnoDB plugin already fix this stuff?
 */
#if defined DRIZZLE_SERVER || defined INNODB_COMPATIBILITY_HOOKS
bool check_global_access(Session *session, ulong want_access);
int get_quote_char_for_identifier(Session *session, const char *name, uint32_t length);
extern struct system_variables global_system_variables;
extern uint32_t mysql_data_home_len;
extern char *mysql_data_home,server_version[SERVER_VERSION_LENGTH],
            mysql_real_data_home[], mysql_unpacked_real_data_home[];
extern const CHARSET_INFO *character_set_filesystem;
extern char reg_ext[FN_EXTLEN];
extern uint32_t reg_ext_length;
extern ulong specialflag;
extern uint32_t lower_case_table_names;
uint32_t strconvert(const CHARSET_INFO *from_cs, const char *from,
                const CHARSET_INFO *to_cs, char *to, uint32_t to_length,
                uint32_t *errors);
uint32_t filename_to_tablename(const char *from, char *to, uint32_t to_length);
uint32_t tablename_to_filename(const char *from, char *to, uint32_t to_length);
#endif /* DRIZZLE_SERVER || INNODB_COMPATIBILITY_HOOKS */


#endif /* DRIZZLE_SERVER_COMMON_INCLUDES_H */
