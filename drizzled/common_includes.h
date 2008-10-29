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
/* Contains system-wide constants and #defines */
#include <drizzled/definitions.h>
/* System-wide common data structures */
#include <drizzled/structs.h>
/* Custom continguous-section memory allocator */
#include <drizzled/sql_alloc.h>

#include "probes.h"


/**
 * @TODO Move the following into a drizzled.h header?
 *
 * I feel that global variables and functions referencing them directly
 * and that are used only in the server should be separated out into 
 * a drizzled.h header file -- JRP
 */


extern const CHARSET_INFO *system_charset_info, *files_charset_info ;
extern const CHARSET_INFO *national_charset_info, *table_alias_charset;

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
extern uint32_t server_id;

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
int get_quote_char_for_identifier(Session *session, const char *name,
                                  uint32_t length);
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
