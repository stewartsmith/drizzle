/*
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
 * Various server-wide declarations and variables.
 */

#ifndef DRIZZLED_SERVER_INCLUDES_H
#define DRIZZLED_SERVER_INCLUDES_H

/* Cross-platform portability code and standard includes */
#include "config.h"
/* Contains system-wide constants and #defines */
#include <drizzled/definitions.h>

/* Lots of system-wide struct definitions like IO_CACHE,
   prototypes for all my_* functions */
#include <mysys/my_sys.h>
/* Custom C string functions */
#include <mystrings/m_string.h>
#include <drizzled/sql_string.h>

/* Routines for dropping, repairing, checking schema tables */
#include <drizzled/sql_table.h>

/* Routines for printing error messages */
#include <drizzled/errmsg_print.h>

#include "drizzled/global_charset_info.h"

typedef struct drizzle_lex_string LEX_STRING;
typedef struct st_typelib TYPELIB;


namespace drizzled { namespace plugin { class StorageEngine; } }
class TableList;
class TableShare;
class DRIZZLE_ERROR;
class Session;
class Table;


extern char *drizzle_tmpdir;
extern const LEX_STRING command_name[];
extern const char *first_keyword;
extern const char *binary_keyword;
extern const char *in_left_expr_name;
extern const char *in_additional_cond;
extern const char *in_having_cond;
extern char language[FN_REFLEN];
extern char glob_hostname[FN_REFLEN];
extern char drizzle_home[FN_REFLEN];
extern char pidfile_name[FN_REFLEN];
extern char system_time_zone[30];
extern char *opt_tc_log_file;
extern const double log_10[309];
extern uint64_t session_startup_options;
extern uint32_t global_thread_id;
extern uint64_t aborted_threads;
extern uint64_t aborted_connects;
extern uint64_t table_cache_size;
extern size_t table_def_size;
extern uint64_t max_connect_errors;
extern uint32_t back_log;
extern pid_t current_pid;
extern uint32_t ha_open_options;
extern char *drizzled_bind_host;
extern uint32_t drizzled_bind_timeout;
extern uint32_t dropping_tables;
extern bool opt_endinfo;
extern bool locked_in_memory;
extern bool volatile abort_loop;
extern bool volatile shutdown_in_progress;
extern uint32_t volatile thread_running;
extern uint32_t volatile global_read_lock;
extern bool opt_readonly;
extern char* opt_secure_file_priv;
extern char *default_tz_name;

extern FILE *stderror_file;
extern pthread_mutex_t LOCK_create_db;
extern pthread_mutex_t LOCK_open;
extern pthread_mutex_t LOCK_thread_count;
extern pthread_mutex_t LOCK_status;
extern pthread_mutex_t LOCK_global_read_lock;
extern pthread_mutex_t LOCK_global_system_variables;

extern pthread_rwlock_t LOCK_system_variables_hash;
extern pthread_cond_t COND_refresh;
extern pthread_cond_t COND_thread_count;
extern pthread_cond_t COND_global_read_lock;
extern pthread_attr_t connection_attrib;
extern struct system_variables max_system_variables;
extern struct system_status_var global_status_var;

extern Table *unused_tables;
extern struct my_option my_long_options[];


extern drizzled::plugin::StorageEngine *myisam_engine;
extern drizzled::plugin::StorageEngine *heap_engine;

extern SHOW_COMP_OPTION have_symlink;

extern pthread_t signal_thread;




#endif /* DRIZZLED_SERVER_INCLUDES_H */
