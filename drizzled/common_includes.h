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
/* Lots of system-wide struct definitions like IO_CACHE,
   prototypes for all my_* functions */
#include <mysys/my_sys.h>
/* Custom C string functions */
#include <mystrings/m_string.h>
/* Defines for the storage engine handler -- i.e. HA_XXX defines */
#include <drizzled/base.h>			                /* Needed by field.h */
/* The <strong>INTERNAL</strong> plugin API - not the external, or public, server plugin API */
#include "sql_plugin.h"
/* Contains system-wide constants and #defines */
#include <drizzled/definitions.h>
/* System-wide common data structures */
#include <drizzled/structs.h>


/**
 * @TODO Move the following into a drizzled.h header?
 *
 * I feel that global variables and functions referencing them directly
 * and that are used only in the server should be separated out into 
 * a drizzled.h header file -- JRP
 */


extern const CHARSET_INFO *system_charset_info, *files_charset_info ;
extern const CHARSET_INFO *national_charset_info, *table_alias_charset;

extern pthread_key_t THR_Session;
inline Session *_current_session(void)
{
  return (Session *)pthread_getspecific(THR_Session);
}
#define current_session _current_session()


/* Drizzle server data type class definitions */
#include <drizzled/field.h>


#include <drizzled/sql_class.h>


#endif /* DRIZZLE_SERVER_COMMON_INCLUDES_H */
