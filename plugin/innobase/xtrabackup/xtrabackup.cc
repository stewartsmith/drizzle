/******************************************************
XtraBackup: The another hot backup tool for InnoDB
(c) 2009 Percona Inc.
(C) 2011 Stewart Smith
Created 3/3/2009 Yasufumi Kinoshita

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#ifndef XTRABACKUP_VERSION
#define XTRABACKUP_VERSION "undefined"
#endif
#ifndef XTRABACKUP_REVISION
#define XTRABACKUP_REVISION "undefined"
#endif

#include <config.h>
#include <string>
#include <drizzled/internal/my_sys.h>
#include <drizzled/charset.h>
#include <drizzled/gettext.h>
#include <drizzled/constrained_value.h>
#include <drizzled/configmake.h>
#include <drizzled/error/level_t.h>

#include "ha_prototypes.h"
 //#define XTRABACKUP_TARGET_IS_PLUGIN
#include <boost/program_options.hpp>
#include <boost/scoped_ptr.hpp>

typedef drizzled::charset_info_st CHARSET_INFO;

#define my_progname "xtrabackup"

#define MYSQL_VERSION_ID 50507 /* Drizzle is much greater */

#include <univ.i>
#include <os0file.h>
#include <os0thread.h>
#include <srv0start.h>
#include <srv0srv.h>
#include <trx0roll.h>
#include <trx0trx.h>
#include <trx0sys.h>
#include <mtr0mtr.h>
#include <row0ins.h>
#include <row0mysql.h>
#include <row0sel.h>
#include <row0upd.h>
#include <log0log.h>
#include <log0recv.h>
#include <lock0lock.h>
#include <dict0crea.h>
#include <btr0cur.h>
#include <btr0btr.h>
#include <btr0sea.h>
#include <fsp0fsp.h>
#include <sync0sync.h>
#include <fil0fil.h>
#include <trx0xa.h>

#ifdef INNODB_VERSION_SHORT
#include <ibuf0ibuf.h>
#else
#error ENOCOOL
#endif

#define IB_INT64 ib_int64_t
#define LSN64 ib_uint64_t
#define MACH_READ_64 mach_read_from_8
#define MACH_WRITE_64 mach_write_to_8
#define OS_MUTEX_CREATE() os_mutex_create()
#define ut_dulint_zero 0
#define ut_dulint_cmp(A, B) (A > B ? 1 : (A == B ? 0 : -1))
#define ut_dulint_add(A, B) (A + B)
#define ut_dulint_minus(A, B) (A - B)
#define ut_dulint_align_down(A, B) (A & ~((ib_int64_t)B - 1))
#define ut_dulint_align_up(A, B) ((A + B - 1) & ~((ib_int64_t)B - 1))

#ifdef __WIN__
#define SRV_PATH_SEPARATOR	'\\'
#define SRV_PATH_SEPARATOR_STR	"\\"	
#else
#define SRV_PATH_SEPARATOR	'/'
#define SRV_PATH_SEPARATOR_STR	"/"
#endif

#ifndef UNIV_PAGE_SIZE_MAX
#define UNIV_PAGE_SIZE_MAX UNIV_PAGE_SIZE
#endif
#ifndef UNIV_PAGE_SIZE_SHIFT_MAX
#define UNIV_PAGE_SIZE_SHIFT_MAX UNIV_PAGE_SIZE_SHIFT
#endif

using namespace drizzled;
namespace po=boost::program_options;

namespace drizzled {
  bool errmsg_printf (error::level_t, char const *format, ...);

  bool errmsg_printf (error::level_t, char const *format, ...)
  {
    bool rv;
    va_list args;
    va_start(args, format);
    rv= vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    return rv;
  }

}

#include "xtrabackup_api.h"

 /* prototypes for static functions in original */

 ulint
 recv_find_max_checkpoint(
 /*=====================*/
                                         /* out: error code or DB_SUCCESS */
         log_group_t**	max_group,	/* out: max group */
         ulint*		max_field);	/* out: LOG_CHECKPOINT_1 or
                                         LOG_CHECKPOINT_2 */


 void
 os_file_set_nocache(
 /*================*/
         int		fd,		/* in: file descriptor to alter */
         const char*	file_name,	/* in: used in the diagnostic message */
         const char*	operation_name);	/* in: used in the diagnostic message,
                                         we call os_file_set_nocache()
                                         immediately after opening or creating
                                         a file, so this is either "open" or
                                         "create" */

 #include <fcntl.h>
 #include <regex.h>

 #ifdef POSIX_FADV_NORMAL
 #define USE_POSIX_FADVISE
 #endif

 /* ==start === definition at fil0fil.c === */
 // ##################################################################
 // NOTE: We should check the following definitions fit to the source.
 // ##################################################################

 //Plugin ?
 /** File node of a tablespace or the log data space */
 struct fil_node_struct {
         fil_space_t*	space;	/*!< backpointer to the space where this node
                                 belongs */
         char*		name;	/*!< path to the file */
         ibool		open;	/*!< TRUE if file open */
         os_file_t	handle;	/*!< OS handle to the file, if file open */
         ibool		is_raw_disk;/*!< TRUE if the 'file' is actually a raw
                                 device or a raw disk partition */
         ulint		size;	/*!< size of the file in database pages, 0 if
                                 not known yet; the possible last incomplete
                                 megabyte may be ignored if space == 0 */
         ulint		n_pending;
                                 /*!< count of pending i/o's on this file;
                                 closing of the file is not allowed if
                                 this is > 0 */
         ulint		n_pending_flushes;
                                 /*!< count of pending flushes on this file;
                                 closing of the file is not allowed if
                                 this is > 0 */
         ib_int64_t	modification_counter;/*!< when we write to the file we
                                 increment this by one */
         ib_int64_t	flush_counter;/*!< up to what
                                 modification_counter value we have
                                 flushed the modifications to disk */
         UT_LIST_NODE_T(fil_node_t) chain;
                                 /*!< link field for the file chain */
         UT_LIST_NODE_T(fil_node_t) LRU;
                                 /*!< link field for the LRU list */
         ulint		magic_n;/*!< FIL_NODE_MAGIC_N */
 };

 struct fil_space_struct {
         char*		name;	/*!< space name = the path to the first file in
                                 it */
         ulint		id;	/*!< space id */
         ib_int64_t	tablespace_version;
                                 /*!< in DISCARD/IMPORT this timestamp
                                 is used to check if we should ignore
                                 an insert buffer merge request for a
                                 page because it actually was for the
                                 previous incarnation of the space */
         ibool		mark;	/*!< this is set to TRUE at database startup if
                                 the space corresponds to a table in the InnoDB
                                 data dictionary; so we can print a warning of
                                 orphaned tablespaces */
         ibool		stop_ios;/*!< TRUE if we want to rename the
                                 .ibd file of tablespace and want to
                                 stop temporarily posting of new i/o
                                 requests on the file */
         ibool		stop_ibuf_merges;
                                 /*!< we set this TRUE when we start
                                 deleting a single-table tablespace */
         ibool		is_being_deleted;
                                 /*!< this is set to TRUE when we start
                                 deleting a single-table tablespace and its
                                 file; when this flag is set no further i/o
                                 or flush requests can be placed on this space,
                                 though there may be such requests still being
                                 processed on this space */
         ulint		purpose;/*!< FIL_TABLESPACE, FIL_LOG, or
                                 FIL_ARCH_LOG */
         UT_LIST_BASE_NODE_T(fil_node_t) chain;
                                 /*!< base node for the file chain */
         ulint		size;	/*!< space size in pages; 0 if a single-table
                                 tablespace whose size we do not know yet;
                                 last incomplete megabytes in data files may be
                                 ignored if space == 0 */
         ulint		flags;	/*!< compressed page size and file format, or 0 */
         ulint		n_reserved_extents;
                                 /*!< number of reserved free extents for
                                 ongoing operations like B-tree page split */
         ulint		n_pending_flushes; /*!< this is positive when flushing
                                 the tablespace to disk; dropping of the
                                 tablespace is forbidden if this is positive */
         ulint		n_pending_ibuf_merges;/*!< this is positive
                                 when merging insert buffer entries to
                                 a page so that we may need to access
                                 the ibuf bitmap page in the
                                 tablespade: dropping of the tablespace
                                 is forbidden if this is positive */
         hash_node_t	hash;	/*!< hash chain node */
         hash_node_t	name_hash;/*!< hash chain the name_hash table */
 #ifndef UNIV_HOTBACKUP
         rw_lock_t	latch;	/*!< latch protecting the file space storage
                                 allocation */
 #endif /* !UNIV_HOTBACKUP */
         UT_LIST_NODE_T(fil_space_t) unflushed_spaces;
                                 /*!< list of spaces with at least one unflushed
                                 file we have written to */
         ibool		is_in_unflushed_spaces; /*!< TRUE if this space is
                                 currently in unflushed_spaces */
 #ifdef XTRADB_BASED
         ibool		is_corrupt;
 #endif
         UT_LIST_NODE_T(fil_space_t) space_list;
                                 /*!< list of all spaces */
         ulint		magic_n;/*!< FIL_SPACE_MAGIC_N */
 };

 typedef	struct fil_system_struct	fil_system_t;

 struct fil_system_struct {
 #ifndef UNIV_HOTBACKUP
         mutex_t		mutex;		/*!< The mutex protecting the cache */
 #ifdef XTRADB55
         mutex_t		file_extend_mutex;
 #endif
 #endif /* !UNIV_HOTBACKUP */
         hash_table_t*	spaces;		/*!< The hash table of spaces in the
                                         system; they are hashed on the space
                                         id */
         hash_table_t*	name_hash;	/*!< hash table based on the space
                                         name */
         UT_LIST_BASE_NODE_T(fil_node_t) LRU;
                                         /*!< base node for the LRU list of the
                                         most recently used open files with no
                                         pending i/o's; if we start an i/o on
                                         the file, we first remove it from this
                                         list, and return it to the start of
                                         the list when the i/o ends;
                                         log files and the system tablespace are
                                         not put to this list: they are opened
                                         after the startup, and kept open until
                                         shutdown */
         UT_LIST_BASE_NODE_T(fil_space_t) unflushed_spaces;
                                         /*!< base node for the list of those
                                         tablespaces whose files contain
                                         unflushed writes; those spaces have
                                         at least one file node where
                                         modification_counter > flush_counter */
         ulint		n_open;		/*!< number of files currently open */
         ulint		max_n_open;	/*!< n_open is not allowed to exceed
                                         this */
         ib_int64_t	modification_counter;/*!< when we write to a file we
                                         increment this by one */
         ulint		max_assigned_id;/*!< maximum space id in the existing
                                         tables, or assigned during the time
                                         mysqld has been up; at an InnoDB
                                         startup we scan the data dictionary
                                         and set here the maximum of the
                                         space id's of the tables there */
         ib_int64_t	tablespace_version;
                                         /*!< a counter which is incremented for
                                         every space object memory creation;
                                         every space mem object gets a
                                         'timestamp' from this; in DISCARD/
                                         IMPORT this is used to check if we
                                         should ignore an insert buffer merge
                                         request */
         UT_LIST_BASE_NODE_T(fil_space_t) space_list;
                                         /*!< list of all file spaces */
 };

 typedef struct {
         ulint	page_size;
 } xb_delta_info_t;

 extern fil_system_t*   fil_system;

 /* ==end=== definition  at fil0fil.c === */


 bool innodb_inited= 0;

 /* === xtrabackup specific options === */
 char xtrabackup_real_target_dir[FN_REFLEN] = "./xtrabackup_backupfiles/";
 const char *xtrabackup_target_dir= xtrabackup_real_target_dir;
 bool xtrabackup_backup = false;
 bool xtrabackup_stats = false;
 bool xtrabackup_prepare = false;
 bool xtrabackup_print_param = false;

 bool xtrabackup_export = false;
 bool xtrabackup_apply_log_only = false;

 bool xtrabackup_suspend_at_end = false;
 uint64_t xtrabackup_use_memory = 100*1024*1024L;
 bool xtrabackup_create_ib_logfile = false;

 long xtrabackup_throttle = 0; /* 0:unlimited */
 lint io_ticket;
 os_event_t wait_throttle = NULL;

 bool xtrabackup_stream = false;
const char *xtrabackup_incremental = NULL;
 LSN64 incremental_lsn;
 LSN64 incremental_to_lsn;
 LSN64 incremental_last_lsn;
 byte* incremental_buffer = NULL;
 byte* incremental_buffer_base = NULL;

const char *xtrabackup_incremental_basedir = NULL; /* for --backup */
const char *xtrabackup_extra_lsndir = NULL; /* for --backup with --extra-lsndir */
char *xtrabackup_incremental_dir = NULL; /* for --prepare */

 char *xtrabackup_tables = NULL;
 int tables_regex_num;
 regex_t *tables_regex;
 regmatch_t tables_regmatch[1];

const char *xtrabackup_tables_file = NULL;
 hash_table_t* tables_hash;

 struct xtrabackup_tables_struct{
         char*		name;
         hash_node_t	name_hash;
 };
 typedef struct xtrabackup_tables_struct	xtrabackup_tables_t;

 #ifdef XTRADB_BASED
 static ulint		thread_nr[SRV_MAX_N_IO_THREADS + 6 + 64];
 static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 6 + 64];
 #else
 static ulint		thread_nr[SRV_MAX_N_IO_THREADS + 6];
 static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 6];
 #endif

 LSN64 checkpoint_lsn_start;
 LSN64 checkpoint_no_start;
 LSN64 log_copy_scanned_lsn;
 IB_INT64 log_copy_offset = 0;
 ibool log_copying = TRUE;
 ibool log_copying_running = FALSE;
 ibool log_copying_succeed = FALSE;

 ibool xtrabackup_logfile_is_renamed = FALSE;

 uint parallel;

 /* === metadata of backup === */
 #define XTRABACKUP_METADATA_FILENAME "xtrabackup_checkpoints"
 char metadata_type[30] = ""; /*[full-backuped|full-prepared|incremental]*/

 ib_uint64_t metadata_from_lsn = 0;
 ib_uint64_t metadata_to_lsn = 0;
 ib_uint64_t metadata_last_lsn = 0;

 #define XB_DELTA_INFO_SUFFIX ".meta"

 /* === sharing with thread === */
 os_file_t       dst_log = -1;
 char            dst_log_path[FN_REFLEN];

 /* === some variables from mysqld === */
 char mysql_real_data_home[FN_REFLEN] = "./";
 char *mysql_data_home= mysql_real_data_home;
std::string mysql_data_home_arg;
 static char mysql_data_home_buff[2];

const char *opt_mysql_tmpdir = NULL;

/* === static parameters in ha_innodb.cc */

#define HA_INNOBASE_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_INNOBASE_RANGE_COUNT	  100

ulong 	innobase_large_page_size = 0;

/* The default values for the following, type long or longlong, start-up
parameters are declared in mysqld.cc: */

long innobase_additional_mem_pool_size = 1*1024*1024L;
long innobase_buffer_pool_awe_mem_mb = 0;
long innobase_file_io_threads = 4;
long innobase_read_io_threads = 4;
long innobase_write_io_threads = 4;
long innobase_force_recovery = 0;
long innobase_lock_wait_timeout = 50;
long innobase_log_buffer_size = 1024*1024L;
long innobase_log_files_in_group = 2;
long innobase_log_files_in_group_backup;
long innobase_mirrored_log_groups = 1;
long innobase_open_files = 300L;

long innobase_page_size = (1 << 14); /* 16KB */
static ulong innobase_log_block_size = 512;
bool innobase_fast_checksum = false;
bool	innobase_extra_undoslots = false;
char*	innobase_doublewrite_file = NULL;

uint64_t innobase_buffer_pool_size = 8*1024*1024L;

typedef constrained_check<int64_t, INT64_MAX, 1024*1024, 1024*1024> log_file_constraint;
static log_file_constraint innobase_log_file_size;

uint64_t innobase_log_file_size_backup;

/* The default values for the following char* start-up parameters
are determined in innobase_init below: */

char*	innobase_data_home_dir			= NULL;
const char* innobase_data_file_path 		= NULL;
char*   innobase_log_group_home_dir		= NULL;
char*	innobase_log_group_home_dir_backup	= NULL;
char*	innobase_log_arch_dir			= NULL;/* unused */
/* The following has a misleading name: starting from 4.0.5, this also
affects Windows: */
char*	innobase_unix_file_flush_method		= NULL;

/* Below we have boolean-valued start-up parameters, and their default
values */

ulong	innobase_fast_shutdown			= 1;
bool innobase_log_archive			= FALSE;/* unused */
bool innobase_use_doublewrite    = TRUE;
bool innobase_use_checksums      = TRUE;
bool innobase_use_large_pages    = FALSE;
bool	innobase_file_per_table			= FALSE;
bool innobase_locks_unsafe_for_binlog        = FALSE;
bool innobase_rollback_on_timeout		= FALSE;
bool innobase_create_status_file		= FALSE;
bool innobase_adaptive_hash_index		= TRUE;

static char *internal_innobase_data_file_path	= NULL;

/* The following counter is used to convey information to InnoDB
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL	32
ulong	innobase_active_counter	= 0;

UNIV_INTERN
bool
innobase_isspace(
  const void *cs,
  char char_to_test)
{
  return my_isspace(static_cast<const CHARSET_INFO *>(cs), char_to_test);
}

UNIV_INTERN
void
innobase_rec_to_mysql(
	Table*			,
	const rec_t*		,
	const dict_index_t*	,
	const ulint*		);

UNIV_INTERN
void
innobase_rec_to_mysql(
/*==================*/
	Table*			,		/*!< in/out: MySQL table */
	const rec_t*		,		/*!< in: record */
	const dict_index_t*	,		/*!< in: index */
	const ulint*		)	/*!< in: rec_get_offsets(
						rec, index, ...) */
{
  fprintf(stderr, "ERROR: innobase_rec_to_mysql called\n");
  return;
}

UNIV_INTERN
void
innobase_rec_reset(Table*);

UNIV_INTERN
void
innobase_rec_reset(
/*===============*/
	Table*			)		/*!< in/out: MySQL table */
{
  fprintf(stderr, "ERROR: innobase_rec_reset called\n");
  return;
}

UNIV_INTERN
void
thd_set_lock_wait_time(
/*===================*/
	drizzled::Session*	,	/*!< in: thread handle (THD*) */
	ulint	);	/*!< in: time waited for the lock */

UNIV_INTERN
void
thd_set_lock_wait_time(
/*===================*/
	drizzled::Session*	,	/*!< in: thread handle (THD*) */
	ulint	)	/*!< in: time waited for the lock */
{
  return;
}



/* ======== Datafiles iterator ======== */
typedef struct {
	fil_system_t *system;
	fil_space_t  *space;
	fil_node_t   *node;
	ibool        started;
	os_mutex_t   mutex;
} datafiles_iter_t;

static
datafiles_iter_t *
datafiles_iter_new(fil_system_t *f_system)
{
	datafiles_iter_t *it;

	it = (datafiles_iter_t*) ut_malloc(sizeof(datafiles_iter_t));
	it->mutex = OS_MUTEX_CREATE();

	it->system = f_system;
	it->space = NULL;
	it->node = NULL;
	it->started = FALSE;

	return it;
}

static
fil_node_t *
datafiles_iter_next(datafiles_iter_t *it, ibool *space_changed)
{
	os_mutex_enter(it->mutex);

	*space_changed = FALSE;

	if (it->node == NULL) {
		if (it->started)
			goto end;
		it->started = TRUE;
	} else {
		it->node = UT_LIST_GET_NEXT(chain, it->node);
		if (it->node != NULL)
			goto end;
	}

	it->space = (it->space == NULL) ?
		UT_LIST_GET_FIRST(it->system->space_list) :
		UT_LIST_GET_NEXT(space_list, it->space);

	while (it->space != NULL &&
	       (it->space->purpose != FIL_TABLESPACE ||
		UT_LIST_GET_LEN(it->space->chain) == 0))
		it->space = UT_LIST_GET_NEXT(space_list, it->space);
	if (it->space == NULL)
		goto end;
	*space_changed = TRUE;

	it->node = UT_LIST_GET_FIRST(it->space->chain);

end:
	os_mutex_exit(it->mutex);

	return it->node;
}

static
void
datafiles_iter_free(datafiles_iter_t *it)
{
	os_mutex_free(it->mutex);
	ut_free(it);
}

/* ======== Date copying thread context ======== */

typedef struct {
	datafiles_iter_t 	*it;
	uint			num;
	uint			*count;
	os_mutex_t		count_mutex;
	os_thread_id_t		id;
} data_thread_ctxt_t;

static void print_version(void)
{
  printf("%s  Ver %s Rev %s for %s %s (%s)\n" ,my_progname,
	  XTRABACKUP_VERSION, XTRABACKUP_REVISION, "Drizzle7",
         TARGET_OS, TARGET_CPU);
}

static void usage(void)
{
  puts("Open source backup tool for InnoDB and XtraDB\n\
\n\
Copyright (C) 2009 Percona Inc.\n\
\n\
This program is free software; you can redistribute it and/or\n\
modify it under the terms of the GNU General Public License\n\
as published by the Free Software Foundation version 2\n\
of the License.\n\
\n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n\
\n\
You can download full text of the license on http://www.gnu.org/licenses/gpl-2.0.txt\n");

  printf("Usage: [%s [--defaults-file=#] --backup | %s [--defaults-file=#] --prepare] [OPTIONS]\n",my_progname,my_progname);
  // FIXME: print what variables we have
}

/* ================ Dummys =================== */

UNIV_INTERN
ibool
thd_is_replication_slave_thread(
/*============================*/
  drizzled::Session* ) /*!< in: thread handle (Session*) */
{
	fprintf(stderr, "xtrabackup: thd_is_replication_slave_thread() is called\n");
	return(FALSE);
}

UNIV_INTERN
ibool
thd_has_edited_nontrans_tables(
/*===========================*/
  drizzled::Session *)  /*!< in: thread handle (Session*) */
{
	fprintf(stderr, "xtrabackup: thd_has_edited_nontrans_tables() is called\n");
	return(FALSE);
}

UNIV_INTERN
ibool
thd_is_select(
/*==========*/
  const drizzled::Session *)  /*!< in: thread handle (Session*) */
{
	fprintf(stderr, "xtrabackup: thd_is_select() is called\n");
        return(false);
}

UNIV_INTERN
void
innobase_mysql_print_thd(
	FILE*,
	drizzled::Session*,
	uint)
{
	fprintf(stderr, "xtrabackup: innobase_mysql_print_thd() is called\n");
}

void
innobase_get_cset_width(
	ulint	cset,
	ulint*	mbminlen,
	ulint*	mbmaxlen)
{
	CHARSET_INFO*	cs;
	ut_ad(cset < 256);
	ut_ad(mbminlen);
	ut_ad(mbmaxlen);

	cs = all_charsets[cset];
	if (cs) {
		*mbminlen = cs->mbminlen;
		*mbmaxlen = cs->mbmaxlen;
	} else {
		ut_a(cset == 0);
		*mbminlen = *mbmaxlen = 0;
	}
}

UNIV_INTERN
void
innobase_convert_from_table_id(
/*===========================*/
  const void*,      /*!< in: the 'from' character set */
  char*     , /*!< out: converted identifier */
  const char* , /*!< in: identifier to convert */
  ulint     )  /*!< in: length of 'to', in bytes */
{
	fprintf(stderr, "xtrabackup: innobase_convert_from_table_id() is called\n");
}

UNIV_INTERN
void
innobase_convert_from_id(
/*=====================*/
  const void*,      /*!< in: the 'from' character set */
  char*     , /*!< out: converted identifier */
  const char*   , /*!< in: identifier to convert */
  ulint     )  /*!< in: length of 'to', in bytes */
{
	fprintf(stderr, "xtrabackup: innobase_convert_from_id() is called\n");
}

int
innobase_strcasecmp(
	const char*	a,
	const char*	b)
{
	return(my_strcasecmp(&my_charset_utf8_general_ci, a, b));
}

void
innobase_casedn_str(
	char*	a)
{
	my_casedn_str(&my_charset_utf8_general_ci, a);
}

UNIV_INTERN
const char*
innobase_get_stmt(
/*==============*/
       drizzled::Session *,      /*!< in: MySQL thread handle */
       size_t* )         /*!< out: length of the SQL statement */
{
	fprintf(stderr, "xtrabackup: innobase_get_stmt() is called\n");
	return("nothing");
}

int
innobase_mysql_tmpfile(void)
{
	char	filename[FN_REFLEN];
	int	fd2 = -1;
	int	fd = internal::create_temp_file(filename, opt_mysql_tmpdir,
                                                "ib",
#ifdef __WIN__
				O_BINARY | O_TRUNC | O_SEQUENTIAL |
				O_TEMPORARY | O_SHORT_LIVED |
#endif /* __WIN__ */
				O_CREAT | O_EXCL | O_RDWR);
	if (fd >= 0) {
#ifndef __WIN__
		/* On Windows, open files cannot be removed, but files can be
		created with the O_TEMPORARY flag to the same effect
		("delete on close"). */
		unlink(filename);
#endif /* !__WIN__ */
		/* Copy the file descriptor, so that the additional resources
		allocated by create_temp_file() can be freed by invoking
		my_close().

		Because the file descriptor returned by this function
		will be passed to fdopen(), it will be closed by invoking
		fclose(), which in turn will invoke close() instead of
		my_close(). */
		fd2 = dup(fd);
		if (fd2 < 0) {
			fprintf(stderr, "xtrabackup: Got error %d on dup\n",fd2);
                }
		close(fd);
	}
	return(fd2);
}

void
innobase_invalidate_query_cache(
	trx_t*	,
	const char*	,
	ulint	)
{
	/* do nothing */
}

/*****************************************************************//**
Convert an SQL identifier to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return	pointer to the end of buf */
static
char*
innobase_convert_identifier(
/*========================*/
	char*		buf,	/*!< out: buffer for converted identifier */
	ulint		buflen,	/*!< in: length of buf, in bytes */
	const char*	id,	/*!< in: identifier to convert */
	ulint		idlen,	/*!< in: length of id, in bytes */
	void*		,	/*!< in: MySQL connection thread, or NULL */
	ibool		)/*!< in: TRUE=id is a table or database name;
				FALSE=id is an UTF-8 string */
{
	const char*	s	= id;
	int		q;

	/* See if the identifier needs to be quoted. */
	q = '"';

	if (q == EOF) {
		if (UNIV_UNLIKELY(idlen > buflen)) {
			idlen = buflen;
		}
		memcpy(buf, s, idlen);
		return(buf + idlen);
	}

	/* Quote the identifier. */
	if (buflen < 2) {
		return(buf);
	}

	*buf++ = q;
	buflen--;

	for (; idlen; idlen--) {
		int	c = *s++;
		if (UNIV_UNLIKELY(c == q)) {
			if (UNIV_UNLIKELY(buflen < 3)) {
				break;
			}

			*buf++ = c;
			*buf++ = c;
			buflen -= 2;
		} else {
			if (UNIV_UNLIKELY(buflen < 2)) {
				break;
			}

			*buf++ = c;
			buflen--;
		}
	}

	*buf++ = q;
	return(buf);
}

/*****************************************************************//**
Convert a table or index name to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return	pointer to the end of buf */
UNIV_INTERN
char*
innobase_convert_name(
/*==================*/
  char*   buf,  /*!< out: buffer for converted identifier */
  ulint   buflen, /*!< in: length of buf, in bytes */
  const char* id, /*!< in: identifier to convert */
  ulint   idlen,  /*!< in: length of id, in bytes */
  drizzled::Session *session,/*!< in: MySQL connection thread, or NULL */
  ibool   table_id)/*!< in: TRUE=id is a table or database name;
        FALSE=id is an index name */
{
	char*		s	= buf;
	const char*	bufend	= buf + buflen;

	if (table_id) {
		const char*	slash = (const char*) memchr(id, '/', idlen);
		if (!slash) {

			goto no_db_name;
		}

		/* Print the database name and table name separately. */
		s = innobase_convert_identifier(s, bufend - s, id, slash - id,
						session, TRUE);
		if (UNIV_LIKELY(s < bufend)) {
			*s++ = '.';
			s = innobase_convert_identifier(s, bufend - s,
							slash + 1, idlen
							- (slash - id) - 1,
							session, TRUE);
		}
	} else if (UNIV_UNLIKELY(*id == TEMP_INDEX_PREFIX)) {
		/* Temporary index name (smart ALTER TABLE) */
		const char temp_index_suffix[]= "--temporary--";

		s = innobase_convert_identifier(buf, buflen, id + 1, idlen - 1,
						session, FALSE);
		if (s - buf + (sizeof temp_index_suffix - 1) < buflen) {
			memcpy(s, temp_index_suffix,
			       sizeof temp_index_suffix - 1);
			s += sizeof temp_index_suffix - 1;
		}
	} else {
no_db_name:
		s = innobase_convert_identifier(buf, buflen, id, idlen,
						session, table_id);
	}

	return(s);

}

ibool
trx_is_interrupted(
	trx_t*	)
{
	/* There are no mysql_thd */
	return(FALSE);
}

UNIV_INTERN int
innobase_mysql_cmp(
/*===============*/
  int   mysql_type, /*!< in: MySQL type */
  uint    charset_number, /*!< in: number of the charset */
  const unsigned char* a,   /*!< in: data field */
  unsigned int  a_length, /*!< in: data field length,
          not UNIV_SQL_NULL */
  const unsigned char* b,   /* in: data field */
  unsigned int  b_length);  /* in: data field length,
          not UNIV_SQL_NULL */

int
innobase_mysql_cmp(
/*===============*/
          /* out: 1, 0, -1, if a is greater, equal, less than b, respectively */
  int   mysql_type, /* in: MySQL type */
  uint    charset_number, /* in: number of the charset */
  const unsigned char* a,   /* in: data field */
  unsigned int  a_length, /* in: data field length, not UNIV_SQL_NULL */
  const unsigned char* b,   /* in: data field */
  unsigned int  b_length) /* in: data field length, not UNIV_SQL_NULL */
{
  const CHARSET_INFO* charset;
  enum_field_types  mysql_tp;
  int     ret;

  assert(a_length != UNIV_SQL_NULL);
  assert(b_length != UNIV_SQL_NULL);

  mysql_tp = (enum_field_types) mysql_type;

  switch (mysql_tp) {

  case DRIZZLE_TYPE_BLOB:
  case DRIZZLE_TYPE_VARCHAR:
    /* Use the charset number to pick the right charset struct for
      the comparison. Since the MySQL function get_charset may be
      slow before Bar removes the mutex operation there, we first
      look at 2 common charsets directly. */

    if (charset_number == default_charset_info->number) {
      charset = default_charset_info;
    } else {
      charset = get_charset(charset_number);

      if (charset == NULL) {
        fprintf(stderr, "xtrabackup needs charset %lu for doing "
                "a comparison, but MySQL cannot "
                "find that charset.",
                (ulong) charset_number);
        ut_a(0);
      }
    }

    /* Starting from 4.1.3, we use strnncollsp() in comparisons of
      non-latin1_swedish_ci strings. NOTE that the collation order
      changes then: 'b\0\0...' is ordered BEFORE 'b  ...'. Users
      having indexes on such data need to rebuild their tables! */

    ret = charset->coll->strnncollsp(charset,
                                     a, a_length,
                                     b, b_length, 0);
    if (ret < 0) {
      return(-1);
    } else if (ret > 0) {
      return(1);
    } else {
      return(0);
    }
  default:
    ut_error;
  }

  return(0);
}

ulint
innobase_get_at_most_n_mbchars(
	ulint charset_id,
	ulint prefix_len,
	ulint data_len,
	const char* str)
{
	ulint char_length;	/* character length in bytes */
	ulint n_chars;		/* number of characters in prefix */
	const CHARSET_INFO* charset;	/* charset used in the field */

	charset = get_charset((uint) charset_id);

	ut_ad(charset);
	ut_ad(charset->mbmaxlen);

	/* Calculate how many characters at most the prefix index contains */

	n_chars = prefix_len / charset->mbmaxlen;

	/* If the charset is multi-byte, then we must find the length of the
	first at most n chars in the string. If the string contains less
	characters than n, then we return the length to the end of the last
	character. */

	if (charset->mbmaxlen > 1) {
		/* my_charpos() returns the byte length of the first n_chars
		characters, or a value bigger than the length of str, if
		there were not enough full characters in str.

		Why does the code below work:
		Suppose that we are looking for n UTF-8 characters.

		1) If the string is long enough, then the prefix contains at
		least n complete UTF-8 characters + maybe some extra
		characters + an incomplete UTF-8 character. No problem in
		this case. The function returns the pointer to the
		end of the nth character.

		2) If the string is not long enough, then the string contains
		the complete value of a column, that is, only complete UTF-8
		characters, and we can store in the column prefix index the
		whole string. */

		char_length = my_charpos(charset, str,
						str + data_len, (int) n_chars);
		if (char_length > data_len) {
			char_length = data_len;
		}
	} else {
		if (data_len < prefix_len) {
			char_length = data_len;
		} else {
			char_length = prefix_len;
		}
	}

	return(char_length);
}

UNIV_INTERN
ulint
innobase_raw_format(
/*================*/
  const char* ,   /*!< in: raw data */
  ulint   , /*!< in: raw data length
          in bytes */
  ulint   ,   /*!< in: charset collation */
  char*   ,    /*!< out: output buffer */
  ulint   ) /*!< in: output buffer size
          in bytes */
{
	fprintf(stderr, "xtrabackup: innobase_raw_format() is called\n");
	return(0);
}

UNIV_INTERN
ulong
thd_lock_wait_timeout(
/*==================*/
  drizzled::Session*)  /*!< in: thread handle (Session*), or NULL to query
      the global innodb_lock_wait_timeout */
{
	return(innobase_lock_wait_timeout);
}

UNIV_INTERN
ibool
thd_supports_xa(
/*============*/
  drizzled::Session* )  /*!< in: thread handle (Session*), or NULL to query
        the global innodb_supports_xa */
{
	return(FALSE);
}

ibool
trx_is_strict(
/*==========*/
	trx_t*)	/*!< in: transaction */
{
	return(FALSE);
}

#ifdef XTRADB_BASED
trx_t*
innobase_get_trx()
{
	return(NULL);
}

ibool
innobase_get_slow_log()
{
	return(FALSE);
}
#endif

/***********************************************************************
Computes bit shift for a given value. If the argument is not a power
of 2, returns 0.*/
UNIV_INLINE
ulint
get_bit_shift(ulint value)
{
	ulint shift;

	if (value == 0)
		return 0;

	for (shift = 0; !(value & 1UL); shift++) {
		value >>= 1;
	}
	return (value >> 1) ? 0 : shift;
}

static bool
innodb_init_param(void)
{
	/* innobase_init */
	static char	current_dir[3];		/* Set if using current lib */
	bool		ret;
	char		*default_path;

	/* dummy for initialize all_charsets[] */
	get_charset_name(0);

#ifdef XTRADB_BASED
	srv_page_size = 0;
	srv_page_size_shift = 0;

	if (innobase_page_size != (1 << 14)) {
		int n_shift = get_bit_shift(innobase_page_size);

		if (n_shift >= 12 && n_shift <= UNIV_PAGE_SIZE_SHIFT_MAX) {
			fprintf(stderr,
				"InnoDB: Warning: innodb_page_size has been "
				"changed from default value 16384.\n");
			srv_page_size_shift = n_shift;
			srv_page_size = 1 << n_shift;
			fprintf(stderr,
				"InnoDB: The universal page size of the "
				"database is set to %lu.\n", srv_page_size);
		} else {
			fprintf(stderr, "InnoDB: Error: invalid value of "
			       "innobase_page_size: %lu", innobase_page_size);
			exit(EXIT_FAILURE);
		}
	} else {
		srv_page_size_shift = 14;
		srv_page_size = (1 << srv_page_size_shift);
	}

	srv_log_block_size = 0;
	if (innobase_log_block_size != 512) {
		uint	n_shift = get_bit_shift(innobase_log_block_size);;

		fprintf(stderr,
			"InnoDB: Warning: innodb_log_block_size has "
			"been changed from its default value. "
			"(###EXPERIMENTAL### operation)\n");
		if (n_shift > 0) {
			srv_log_block_size = (1 << n_shift);
			fprintf(stderr,
				"InnoDB: The log block size is set to %lu.\n",
				srv_log_block_size);
		}
	} else {
		srv_log_block_size = 512;
	}

	if (!srv_log_block_size) {
		fprintf(stderr,
			"InnoDB: Error: %lu is not valid value for "
			"innodb_log_block_size.\n", innobase_log_block_size);
		goto error;
	}

	srv_fast_checksum = (ibool) innobase_fast_checksum;
#endif

	/* Check that values don't overflow on 32-bit systems. */
	if (sizeof(ulint) == 4) {
		if (xtrabackup_use_memory > UINT32_MAX) {
			fprintf(stderr,
				"xtrabackup: use-memory can't be over 4GB"
				" on 32-bit systems\n");
		}

		if (innobase_buffer_pool_size > UINT32_MAX) {
			fprintf(stderr,
				"xtrabackup: innobase_buffer_pool_size can't be over 4GB"
				" on 32-bit systems\n");

			goto error;
		}

		if (innobase_log_file_size > UINT32_MAX) {
			fprintf(stderr,
				"xtrabackup: innobase_log_file_size can't be over 4GB"
				" on 32-bit systemsi\n");

			goto error;
		}
	}

  	os_innodb_umask = (ulint)0664;

	/* First calculate the default path for innodb_data_home_dir etc.,
	in case the user has not given any value.

	Note that when using the embedded server, the datadirectory is not
	necessarily the current directory of this program. */

	  	/* It's better to use current lib, to keep paths short */
	  	current_dir[0] = FN_CURLIB;
	  	current_dir[1] = FN_LIBCHAR;
	  	current_dir[2] = 0;
	  	default_path = current_dir;

	ut_a(default_path);

	/* Set InnoDB initialization parameters according to the values
	read from MySQL .cnf file */

	if (xtrabackup_backup || xtrabackup_stats) {
		fprintf(stderr, "xtrabackup: Target instance is assumed as followings.\n");
	} else {
		fprintf(stderr, "xtrabackup: Temporary instance for recovery is set as followings.\n");
	}

	/*--------------- Data files -------------------------*/

	/* The default dir for data files is the datadir of MySQL */

	srv_data_home = ((xtrabackup_backup || xtrabackup_stats) && innobase_data_home_dir
			 ? innobase_data_home_dir : default_path);
	fprintf(stderr, "xtrabackup:   innodb_data_home_dir = %s\n", srv_data_home);

	/* Set default InnoDB data file size to 10 MB and let it be
  	auto-extending. Thus users can use InnoDB in >= 4.0 without having
	to specify any startup options. */

	if (!innobase_data_file_path) {
  		innobase_data_file_path = (char*) "ibdata1:10M:autoextend";
	}
	fprintf(stderr, "xtrabackup:   innodb_data_file_path = %s\n",
		innobase_data_file_path);

	/* Since InnoDB edits the argument in the next call, we make another
	copy of it: */

	internal_innobase_data_file_path = strdup(innobase_data_file_path);

	ret = (bool) srv_parse_data_file_paths_and_sizes(
			internal_innobase_data_file_path);
	if (ret == FALSE) {
	  	fprintf(stderr,
			"xtrabackup: syntax error in innodb_data_file_path\n");
mem_free_and_error:
	  	free(internal_innobase_data_file_path);
                goto error;
	}

	if (xtrabackup_prepare) {
		/* "--prepare" needs filenames only */
		ulint i;

		for (i=0; i < srv_n_data_files; i++) {
			char *p;

			p = srv_data_file_names[i];
			while ((p = strstr(p, SRV_PATH_SEPARATOR_STR)) != NULL)
			{
				p++;
				srv_data_file_names[i] = p;
			}
		}
	}

#ifdef XTRADB_BASED
	srv_doublewrite_file = innobase_doublewrite_file;
#ifndef XTRADB55
	srv_extra_undoslots = (ibool) innobase_extra_undoslots;
#endif
#endif

	/* -------------- Log files ---------------------------*/

	/* The default dir for log files is the datadir of MySQL */

	if (!((xtrabackup_backup || xtrabackup_stats) && innobase_log_group_home_dir)) {
	  	innobase_log_group_home_dir = default_path;
	}
	if (xtrabackup_prepare && xtrabackup_incremental_dir) {
		innobase_log_group_home_dir = xtrabackup_incremental_dir;
	}
	fprintf(stderr, "xtrabackup:   innodb_log_group_home_dir = %s\n",
		innobase_log_group_home_dir);

#ifdef UNIV_LOG_ARCHIVE
	/* Since innodb_log_arch_dir has no relevance under MySQL,
	starting from 4.0.6 we always set it the same as
	innodb_log_group_home_dir: */

	innobase_log_arch_dir = innobase_log_group_home_dir;

	srv_arch_dir = innobase_log_arch_dir;
#endif /* UNIG_LOG_ARCHIVE */

	ret = (bool)
		srv_parse_log_group_home_dirs(innobase_log_group_home_dir);

	if (ret == FALSE || innobase_mirrored_log_groups != 1) {
	  fprintf(stderr, "xtrabackup: syntax error in innodb_log_group_home_dir, or a "
			  "wrong number of mirrored log groups\n");

                goto mem_free_and_error;
	}

	srv_adaptive_flushing = FALSE;
	srv_use_sys_malloc = TRUE;
	srv_file_format = 1; /* Barracuda */
	srv_max_file_format_at_startup = DICT_TF_FORMAT_51; /* on */

	/* --------------------------------------------------*/

	srv_file_flush_method_str = innobase_unix_file_flush_method;

	srv_n_log_groups = (ulint) innobase_mirrored_log_groups;
	srv_n_log_files = (ulint) innobase_log_files_in_group;
	srv_log_file_size = (ulint) innobase_log_file_size;
	fprintf(stderr, "xtrabackup:   innodb_log_files_in_group = %ld\n",
		srv_n_log_files);
	fprintf(stderr, "xtrabackup:   innodb_log_file_size = %ld\n",
		srv_log_file_size);

#ifdef UNIV_LOG_ARCHIVE
	srv_log_archive_on = (ulint) innobase_log_archive;
#endif /* UNIV_LOG_ARCHIVE */
	srv_log_buffer_size = (ulint) innobase_log_buffer_size;

        /* We set srv_pool_size here in units of 1 kB. InnoDB internally
        changes the value so that it becomes the number of database pages. */

	//srv_buf_pool_size = (ulint) innobase_buffer_pool_size;
	srv_buf_pool_size = (ulint) xtrabackup_use_memory;

	srv_mem_pool_size = (ulint) innobase_additional_mem_pool_size;

	srv_n_file_io_threads = (ulint) innobase_file_io_threads;

	srv_n_read_io_threads = (ulint) innobase_read_io_threads;
	srv_n_write_io_threads = (ulint) innobase_write_io_threads;

	srv_force_recovery = (ulint) innobase_force_recovery;

	srv_use_doublewrite_buf = (ibool) innobase_use_doublewrite;
	srv_use_checksums = (ibool) innobase_use_checksums;

	btr_search_enabled = innobase_adaptive_hash_index ? true : false;

	os_use_large_pages = (ibool) innobase_use_large_pages;
	os_large_page_size = (ulint) innobase_large_page_size;

	row_rollback_on_timeout = (ibool) innobase_rollback_on_timeout;

	srv_file_per_table = innobase_file_per_table ? true : false;

        srv_locks_unsafe_for_binlog = (ibool) innobase_locks_unsafe_for_binlog;

	srv_max_n_open_files = (ulint) innobase_open_files;
	srv_innodb_status = (ibool) innobase_create_status_file;

	srv_print_verbose_log = 1;

	/* Store the default charset-collation number of this MySQL
	installation */

	/* We cannot treat characterset here for now!! */
	data_mysql_default_charset_coll = (ulint)default_charset_info->number;

	ut_a(DATA_MYSQL_BINARY_CHARSET_COLL == my_charset_bin.number);

	//innobase_commit_concurrency_init_default();

	/* Since we in this module access directly the fields of a trx
        struct, and due to different headers and flags it might happen that
	mutex_t has a different size in this module and in InnoDB
	modules, we check at run time that the size is the same in
	these compilation modules. */


	/* On 5.5 srv_use_native_aio is TRUE by default. It is later reset
	if it is not supported by the platform in
	innobase_start_or_create_for_mysql(). As we don't call it in xtrabackup,
	we have to duplicate checks from that function here. */

#ifdef __WIN__
	switch (os_get_os_version()) {
	case OS_WIN95:
	case OS_WIN31:
	case OS_WINNT:
		/* On Win 95, 98, ME, Win32 subsystem for Windows 3.1,
		and NT use simulated aio. In NT Windows provides async i/o,
		but when run in conjunction with InnoDB Hot Backup, it seemed
		to corrupt the data files. */

		srv_use_native_aio = FALSE;
		break;

	case OS_WIN2000:
	case OS_WINXP:
		/* On 2000 and XP, async IO is available. */
		srv_use_native_aio = TRUE;
		break;

	default:
		/* Vista and later have both async IO and condition variables */
		srv_use_native_aio = TRUE;
		srv_use_native_conditions = TRUE;
		break;
	}

#elif defined(LINUX_NATIVE_AIO)

	if (srv_use_native_aio) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" InnoDB: Using Linux native AIO\n");
	}
#else
	/* Currently native AIO is supported only on windows and linux
	and that also when the support is compiled in. In all other
	cases, we ignore the setting of innodb_use_native_aio. */
	srv_use_native_aio = FALSE;

#endif

	return(FALSE);

error:
	fprintf(stderr, "xtrabackup: innodb_init_param(): Error occured.\n");
	return(TRUE);
}

static bool
innodb_init(void)
{
	int	err;

	err = innobase_start_or_create_for_mysql();

	if (err != DB_SUCCESS) {
	  	free(internal_innobase_data_file_path);
                goto error;
	}

	/* They may not be needed for now */
//	(void) hash_init(&innobase_open_tables,system_charset_info, 32, 0, 0,
//			 		(hash_get_key) innobase_get_key, 0, 0);
//        pthread_mutex_init(&innobase_share_mutex, MY_MUTEX_INIT_FAST);
//        pthread_mutex_init(&prepare_commit_mutex, MY_MUTEX_INIT_FAST);
//        pthread_mutex_init(&commit_threads_m, MY_MUTEX_INIT_FAST);
//        pthread_mutex_init(&commit_cond_m, MY_MUTEX_INIT_FAST);
//        pthread_cond_init(&commit_cond, NULL);

	innodb_inited= 1;

	return(FALSE);

error:
	fprintf(stderr, "xtrabackup: innodb_init(): Error occured.\n");
	return(TRUE);
}

static bool
innodb_end(void)
{
	srv_fast_shutdown = (ulint) innobase_fast_shutdown;
	innodb_inited = 0;

	fprintf(stderr, "xtrabackup: starting shutdown with innodb_fast_shutdown = %lu\n",
		srv_fast_shutdown);

	if (innobase_shutdown_for_mysql() != DB_SUCCESS) {
		goto error;
	}
	free(internal_innobase_data_file_path);

	/* They may not be needed for now */
//	hash_free(&innobase_open_tables);
//	pthread_mutex_destroy(&innobase_share_mutex);
//	pthread_mutex_destroy(&prepare_commit_mutex);
//	pthread_mutex_destroy(&commit_threads_m);
//	pthread_mutex_destroy(&commit_cond_m);
//	pthread_cond_destroy(&commit_cond);

	return(FALSE);

error:
	fprintf(stderr, "xtrabackup: innodb_end(): Error occured.\n");
	return(TRUE);
}

/* ================= common ================= */
static bool
xtrabackup_read_metadata(char *filename)
{
	FILE *fp;

	fp = fopen(filename,"r");
	if(!fp) {
		fprintf(stderr, "xtrabackup: Error: cannot open %s\n", filename);
		return(TRUE);
	}

	if (fscanf(fp, "backup_type = %29s\n", metadata_type)
			!= 1)
		return(TRUE);
	if (fscanf(fp, "from_lsn = %"PRIu64"\n", &metadata_from_lsn)
			!= 1)
		return(TRUE);
	if (fscanf(fp, "to_lsn = %"PRIu64"\n", &metadata_to_lsn)
			!= 1)
		return(TRUE);
	if (fscanf(fp, "last_lsn = %"PRIu64"\n", &metadata_last_lsn)
			!= 1) {
		metadata_last_lsn = 0;
	}

	fclose(fp);

	return(FALSE);
}

static bool
xtrabackup_write_metadata(char *filename)
{
	FILE *fp;

	fp = fopen(filename,"w");
	if(!fp) {
		fprintf(stderr, "xtrabackup: Error: cannot open %s\n", filename);
		return(TRUE);
	}

	if (fprintf(fp, "backup_type = %s\n", metadata_type)
			< 0)
		return(TRUE);
	if (fprintf(fp, "from_lsn = %"PRIu64"\n", metadata_from_lsn)
			< 0)
		return(TRUE);
	if (fprintf(fp, "to_lsn = %"PRIu64"\n", metadata_to_lsn)
			< 0)
		return(TRUE);
	if (fprintf(fp, "last_lsn = %"PRIu64"\n", metadata_last_lsn)
			< 0)
		return(TRUE);

	fclose(fp);

	return(FALSE);
}

/***********************************************************************
Read meta info for an incremental delta.
@return TRUE on success, FALSE on failure. */
static bool
xb_read_delta_metadata(const char *filepath, xb_delta_info_t *info)
{
	FILE *fp;

	memset(info, 0, sizeof(xb_delta_info_t));

	fp = fopen(filepath, "r");
	if (!fp) {
		/* Meta files for incremental deltas are optional */
		return(TRUE);
	}

	if (fscanf(fp, "page_size = %lu\n", &info->page_size) != 1)
		return(FALSE);

	fclose(fp);

	return(TRUE);
}

/***********************************************************************
Write meta info for an incremental delta.
@return TRUE on success, FALSE on failure. */
static bool
xb_write_delta_metadata(const char *filepath, const xb_delta_info_t *info)
{
	FILE *fp;

	fp = fopen(filepath, "w");
	if (!fp) {
		fprintf(stderr, "xtrabackup: Error: cannot open %s\n", filepath);
		return(FALSE);
	}

	if (fprintf(fp, "page_size = %lu\n", info->page_size) < 0)
		return(FALSE);

	fclose(fp);

	return(TRUE);
}

/* ================= backup ================= */
static void
xtrabackup_io_throttling(void)
{
	if (xtrabackup_throttle && (io_ticket--) < 0) {
		os_event_reset(wait_throttle);
		os_event_wait(wait_throttle);
	}
}


/* TODO: We may tune the behavior (e.g. by fil_aio)*/
#define COPY_CHUNK 64

static bool
xtrabackup_copy_datafile(fil_node_t* node, uint thread_n)
{
	os_file_t	src_file = -1;
	os_file_t	dst_file = -1;
	char		dst_path[FN_REFLEN];
	char		meta_path[FN_REFLEN];
	ibool		success;
	byte*		page;
	byte*		buf2 = NULL;
	IB_INT64	file_size;
	IB_INT64	offset;
	ulint		page_in_buffer= 0;
	ulint		incremental_buffers = 0;
	ulint		page_size;
	ulint		page_size_shift;
	ulint		zip_size;
	xb_delta_info_t info;

	info.page_size = 0;

#ifdef XTRADB_BASED
	if (xtrabackup_tables && (!trx_sys_sys_space(node->space->id)))
#else
	if (xtrabackup_tables && (node->space->id != 0))
#endif
	{ /* must backup id==0 */
		char *p;
		int p_len, regres= 0;
		char *next, *prev;
		char tmp;
		int i;

		p = node->name;
		prev = NULL;
		while ((next = strstr(p, SRV_PATH_SEPARATOR_STR)) != NULL)
		{
			prev = p;
			p = next + 1;
		}
		p_len = strlen(p) - strlen(".ibd");

		if (p_len < 1) {
			/* unknown situation: skip filtering */
			goto skip_filter;
		}

		/* TODO: Fix this lazy implementation... */
		tmp = p[p_len];
		p[p_len] = 0;
		*(p - 1) = '.';

		for (i = 0; i < tables_regex_num; i++) {
			regres = regexec(&tables_regex[i], prev, 1, tables_regmatch, 0);
			if (regres != REG_NOMATCH)
				break;
		}

		p[p_len] = tmp;
		*(p - 1) = SRV_PATH_SEPARATOR;

		if ( regres == REG_NOMATCH ) {
			printf("[%02u] Copying %s is skipped.\n",
			       thread_n, node->name);
			return(FALSE);
		}
	}

#ifdef XTRADB_BASED
	if (xtrabackup_tables_file && (!trx_sys_sys_space(node->space->id)))
#else
	if (xtrabackup_tables_file && (node->space->id != 0))
#endif
	{ /* must backup id==0 */
		xtrabackup_tables_t* table;
		char *p;
		int p_len;
		char *next, *prev;
		char tmp;

		p = node->name;
		prev = NULL;
		while ((next = strstr(p, SRV_PATH_SEPARATOR_STR)) != NULL)
		{
			prev = p;
			p = next + 1;
		}
		p_len = strlen(p) - strlen(".ibd");

		if (p_len < 1) {
			/* unknown situation: skip filtering */
			goto skip_filter;
		}

		/* TODO: Fix this lazy implementation... */
		tmp = p[p_len];
		p[p_len] = 0;

		HASH_SEARCH(name_hash, tables_hash, ut_fold_string(prev),
			    xtrabackup_tables_t*,
			    table,
			    ut_ad(table->name),
	    		    !strcmp(table->name, prev));

		p[p_len] = tmp;

		if (!table) {
			printf("[%02u] Copying %s is skipped.\n",
			       thread_n, node->name);
			return(FALSE);
		}
	}

skip_filter:
	zip_size = fil_space_get_zip_size(node->space->id);
	if (zip_size == ULINT_UNDEFINED) {
		fprintf(stderr, "[%02u] xtrabackup: Warning: "
			"Failed to determine page size for %s.\n"
			"[%02u] xtrabackup: Warning: We assume the table was "
			"dropped during xtrabackup execution and ignore the "
			"file.\n", thread_n, node->name, thread_n);
		goto skip;
	} else if (zip_size) {
		page_size = zip_size;
		page_size_shift = get_bit_shift(page_size);
		fprintf(stderr, "[%02u] %s is compressed with page size = "
			"%lu bytes\n", thread_n, node->name, page_size);
		if (page_size_shift < 10 || page_size_shift > 14) {
			fprintf(stderr, "[%02u] xtrabackup: Error: Invalid "
				"page size.\n", thread_n);
			ut_error;
		}
	} else {
		page_size = UNIV_PAGE_SIZE;
		page_size_shift = UNIV_PAGE_SIZE_SHIFT;
	}

#ifdef XTRADB_BASED
	if (trx_sys_sys_space(node->space->id))
#else
	if (node->space->id == 0)
#endif
	{
		char *next, *p;
		/* system datafile "/fullpath/datafilename.ibd" or "./datafilename.ibd" */
		p = node->name;
		while ((next = strstr(p, SRV_PATH_SEPARATOR_STR)) != NULL)
		{
			p = next + 1;
		}
		sprintf(dst_path, "%s/%s", xtrabackup_target_dir, p);
	} else {
		/* file per table style "./database/table.ibd" */
		sprintf(dst_path, "%s%s", xtrabackup_target_dir, strstr(node->name, SRV_PATH_SEPARATOR_STR));
	}

	if (xtrabackup_incremental) {
		snprintf(meta_path, sizeof(meta_path),
			 "%s%s", dst_path, XB_DELTA_INFO_SUFFIX);
		strcat(dst_path, ".delta");

		/* clear buffer */
		bzero(incremental_buffer, (page_size/4) * page_size);
		page_in_buffer = 0;
		mach_write_to_4(incremental_buffer, 0x78747261UL);/*"xtra"*/
		page_in_buffer++;

		info.page_size = page_size;
	}

	/* open src_file*/
	if (!node->open) {
		src_file = os_file_create_simple_no_error_handling(
						0 /* dummy of innodb_file_data_key */,
						node->name, OS_FILE_OPEN,
						OS_FILE_READ_ONLY, &success);
		if (!success) {
			/* The following call prints an error message */
			os_file_get_last_error(TRUE);

			fprintf(stderr,
				"[%02u] xtrabackup: Warning: cannot open %s\n"
				"[%02u] xtrabackup: Warning: We assume the "
				"table was dropped during xtrabackup execution "
				"and ignore the file.\n",
				thread_n, node->name, thread_n);
			goto skip;
		}

		if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
			os_file_set_nocache(src_file, node->name, "OPEN");
		}
	} else {
		src_file = node->handle;
	}

#ifdef USE_POSIX_FADVISE
	posix_fadvise(src_file, 0, 0, POSIX_FADV_SEQUENTIAL);
	posix_fadvise(src_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	/* open dst_file */
	/* os_file_create reads srv_unix_file_flush_method */
	dst_file = os_file_create(
			0 /* dummy of innodb_file_data_key */,
			dst_path, OS_FILE_CREATE,
			OS_FILE_NORMAL, OS_DATA_FILE, &success);
                if (!success) {
                        /* The following call prints an error message */
                        os_file_get_last_error(TRUE);

			fprintf(stderr,"[%02u] xtrabackup: error: "
				"cannot open %s\n", thread_n, dst_path);
                        goto error;
                }

#ifdef USE_POSIX_FADVISE
	posix_fadvise(dst_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	/* copy : TODO: tune later */
	printf("[%02u] Copying %s \n     to %s\n", thread_n,
	       node->name, dst_path);

	buf2 = (unsigned char*) ut_malloc(COPY_CHUNK * page_size + UNIV_PAGE_SIZE);
	page = (unsigned char*) ut_align(buf2, UNIV_PAGE_SIZE);

	success = os_file_read(src_file, page, 0, 0, UNIV_PAGE_SIZE);
	if (!success) {
		goto error;
	}

	file_size = os_file_get_size_as_iblonglong(src_file);

	for (offset = 0; offset < file_size; offset += COPY_CHUNK * page_size) {
		ulint chunk;
		ulint chunk_offset;
		ulint retry_count = 10;
//copy_loop:
		if ((ulint)(file_size - offset) > COPY_CHUNK * page_size) {
			chunk = COPY_CHUNK * page_size;
		} else {
			chunk = (ulint)(file_size - offset);
		}

read_retry:
		xtrabackup_io_throttling();

		success = os_file_read(src_file, page,
				(ulint)(offset & 0xFFFFFFFFUL),
				(ulint)(offset >> 32), chunk);
		if (!success) {
			goto error;
		}

		/* check corruption and retry */
		for (chunk_offset = 0; chunk_offset < chunk; chunk_offset += page_size) {
			if (buf_page_is_corrupted(page + chunk_offset, zip_size))
			{
				if (
#ifdef XTRADB_BASED
				    trx_sys_sys_space(node->space->id)
#else
				    node->space->id == 0
#endif
				    && ((offset + (IB_INT64)chunk_offset) >> page_size_shift)
				       >= FSP_EXTENT_SIZE
				    && ((offset + (IB_INT64)chunk_offset) >> page_size_shift)
				       < FSP_EXTENT_SIZE * 3) {
					/* double write buffer may have old data in the end
					   or it may contain the other format page like COMPRESSED.
 					   So, we can pass the check of double write buffer.*/
					ut_a(page_size == UNIV_PAGE_SIZE);
					fprintf(stderr, "[%02u] xtrabackup: "
						"Page %lu seems double write "
						"buffer. passing the check.\n",
						thread_n,
						(ulint)((offset +
							 (IB_INT64)chunk_offset) >>
							page_size_shift));
				} else {
					retry_count--;
					if (retry_count == 0) {
						fprintf(stderr,
							"[%02u] xtrabackup: "
							"Error: 10 retries "
							"resulted in fail. This"
							"file seems to be "
							"corrupted.\n",
							thread_n);
						goto error;
					}
					fprintf(stderr, "[%02u] xtrabackup: "
						"Database page corruption "
						"detected at page %lu. "
						"retrying...\n",
						thread_n,
						(ulint)((offset +
							 (IB_INT64)chunk_offset)
							>> page_size_shift));
					goto read_retry;
				}
			}
		}

		if (xtrabackup_incremental) {
			for (chunk_offset = 0; chunk_offset < chunk; chunk_offset += page_size) {
				/* newer page */
				/* This condition may be OK for header, ibuf and fsp */
				if (ut_dulint_cmp(incremental_lsn,
					MACH_READ_64(page + chunk_offset + FIL_PAGE_LSN)) < 0) {
	/* ========================================= */
	IB_INT64 offset_on_page;

	if (page_in_buffer == page_size/4) {
		/* flush buffer */
		success = os_file_write(dst_path, dst_file, incremental_buffer,
			((incremental_buffers * (page_size/4))
				<< page_size_shift) & 0xFFFFFFFFUL,
			(incremental_buffers * (page_size/4))
				>> (32 - page_size_shift),
			page_in_buffer * page_size);
		if (!success) {
			goto error;
		}

		incremental_buffers++;

		/* clear buffer */
		bzero(incremental_buffer, (page_size/4) * page_size);
		page_in_buffer = 0;
		mach_write_to_4(incremental_buffer, 0x78747261UL);/*"xtra"*/
		page_in_buffer++;
	}

	offset_on_page = ((offset + (IB_INT64)chunk_offset) >> page_size_shift);
	ut_a(offset_on_page >> 32 == 0);

	mach_write_to_4(incremental_buffer + page_in_buffer * 4, (ulint)offset_on_page);
	memcpy(incremental_buffer + page_in_buffer * page_size,
	       page + chunk_offset, page_size);

	page_in_buffer++;
	/* ========================================= */
				}
			}
		} else {
			success = os_file_write(dst_path, dst_file, page,
				(ulint)(offset & 0xFFFFFFFFUL),
				(ulint)(offset >> 32), chunk);
			if (!success) {
				goto error;
			}
		}

	}

	if (xtrabackup_incremental) {
		/* termination */
		if (page_in_buffer != page_size/4) {
			mach_write_to_4(incremental_buffer + page_in_buffer * 4, 0xFFFFFFFFUL);
		}

		mach_write_to_4(incremental_buffer, 0x58545241UL);/*"XTRA"*/

		/* flush buffer */
		success = os_file_write(dst_path, dst_file, incremental_buffer,
			((incremental_buffers * (page_size/4))
				<< page_size_shift) & 0xFFFFFFFFUL,
			(incremental_buffers * (page_size/4))
				>> (32 - page_size_shift),
			page_in_buffer * page_size);
		if (!success) {
			goto error;
		}
		if (!xb_write_delta_metadata(meta_path, &info)) {
			fprintf(stderr, "[%02u] xtrabackup: Error: "
				"failed to write meta info for %s\n",
				thread_n, dst_path);
			goto error;
		}
	}

	success = os_file_flush(dst_file);
	if (!success) {
		goto error;
	}


	/* check size again */
	/* TODO: but is it needed ?? */
//	if (file_size < os_file_get_size_as_iblonglong(src_file)) {
//		offset -= COPY_CHUNK * page_size;
//		file_size = os_file_get_size_as_iblonglong(src_file);
//		goto copy_loop;
//	}

	/* TODO: How should we treat double_write_buffer here? */
	/* (currently, don't care about. Because,
	    the blocks is newer than the last checkpoint anyway.) */

	/* close */
	printf("[%02u]        ...done\n", thread_n);
	if (!node->open) {
		os_file_close(src_file);
	}
	os_file_close(dst_file);
	ut_free(buf2);
	return(FALSE);
error:
	if (src_file != -1 && !node->open)
		os_file_close(src_file);
	if (dst_file != -1)
		os_file_close(dst_file);
	if (buf2)
		ut_free(buf2);
	fprintf(stderr, "[%02u] xtrabackup: Error: "
		"xtrabackup_copy_datafile() failed.\n", thread_n);
	return(TRUE); /*ERROR*/

skip:
	if (src_file != -1 && !node->open)
		os_file_close(src_file);
	if (dst_file != -1)
		os_file_close(dst_file);
	if (buf2)
		ut_free(buf2);
	fprintf(stderr, "[%02u] xtrabackup: Warning: skipping file %s.\n",
		thread_n, node->name);
	return(FALSE);
}

static bool
xtrabackup_copy_logfile(LSN64 from_lsn, bool is_last)
{
	/* definition from recv_recovery_from_checkpoint_start() */
	log_group_t*	group;
	LSN64		group_scanned_lsn;
	LSN64		contiguous_lsn;

	ibool		success;

	if (!xtrabackup_stream)
		ut_a(dst_log != -1);

	/* read from checkpoint_lsn_start to current */
	contiguous_lsn = ut_dulint_align_down(from_lsn,
						OS_FILE_LOG_BLOCK_SIZE);

	/* TODO: We must check the contiguous_lsn still exists in log file.. */

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		ibool	finished;
		LSN64	start_lsn;
		LSN64	end_lsn;


		/* reference recv_group_scan_log_recs() */
	finished = FALSE;

	start_lsn = contiguous_lsn;
		
	while (!finished) {			
		end_lsn = ut_dulint_add(start_lsn, RECV_SCAN_SIZE);

		xtrabackup_io_throttling();

		log_group_read_log_seg(LOG_RECOVER, log_sys->buf,
						group, start_lsn, end_lsn);

		//printf("log read from (%lu %lu) to (%lu %lu)\n",
		//	start_lsn.high, start_lsn.low, end_lsn.high, end_lsn.low);

		/* reference recv_scan_log_recs() */
		{
	byte*	log_block;
	ulint	no;
	LSN64	scanned_lsn;
	ulint	data_len;

	ulint	scanned_checkpoint_no = 0;

	finished = FALSE;
	
	log_block = log_sys->buf;
	scanned_lsn = start_lsn;

	while (log_block < log_sys->buf + RECV_SCAN_SIZE && !finished) {

		no = log_block_get_hdr_no(log_block);

		if (no != log_block_convert_lsn_to_no(scanned_lsn)
		    || !log_block_checksum_is_ok_or_old_format(log_block)) {

			if (no > log_block_convert_lsn_to_no(scanned_lsn)
			    && log_block_checksum_is_ok_or_old_format(log_block)) {
				fprintf(stderr,
">> ###Warning###: The copying transaction log migh be overtaken already by the target.\n"
">>              : Waiting log block no %lu, but the bumber is already %lu.\n"
">>              : If the number equals %lu + n * %lu, it should be overtaken already.\n",
					(ulong) log_block_convert_lsn_to_no(scanned_lsn),
					(ulong) no,
					(ulong) log_block_convert_lsn_to_no(scanned_lsn),
					(ulong) (log_block_convert_lsn_to_no(
							 log_group_get_capacity(group)
									  ) - 1));

			} else if (no == log_block_convert_lsn_to_no(scanned_lsn)
			    && !log_block_checksum_is_ok_or_old_format(
								log_block)) {
				fprintf(stderr,
"xtrabackup: Log block no %lu at lsn %"PRIu64" has\n"
"xtrabackup: ok header, but checksum field contains %lu, should be %lu\n",
				(ulong) no,
				scanned_lsn,
				(ulong) log_block_get_checksum(log_block),
				(ulong) log_block_calc_checksum(log_block));
			}

			/* Garbage or an incompletely written log block */

			finished = TRUE;

			break;
		}

		if (log_block_get_flush_bit(log_block)) {
			/* This block was a start of a log flush operation:
			we know that the previous flush operation must have
			been completed for all log groups before this block
			can have been flushed to any of the groups. Therefore,
			we know that log data is contiguous up to scanned_lsn
			in all non-corrupt log groups. */

			if (ut_dulint_cmp(scanned_lsn, contiguous_lsn) > 0) {
				contiguous_lsn = scanned_lsn;
			}
		}

		data_len = log_block_get_data_len(log_block);

		if (
		    (scanned_checkpoint_no > 0)
		    && (log_block_get_checkpoint_no(log_block)
		       < scanned_checkpoint_no)
		    && (scanned_checkpoint_no
			- log_block_get_checkpoint_no(log_block)
			> 0x80000000UL)) {

			/* Garbage from a log buffer flush which was made
			before the most recent database recovery */

			finished = TRUE;
			break;
		}		    

		scanned_lsn = ut_dulint_add(scanned_lsn, data_len);
		scanned_checkpoint_no = log_block_get_checkpoint_no(log_block);

		if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
			/* Log data for this group ends here */

			finished = TRUE;
		} else {
			log_block += OS_FILE_LOG_BLOCK_SIZE;
		}
	} /* while (log_block < log_sys->buf + RECV_SCAN_SIZE && !finished) */

	group_scanned_lsn = scanned_lsn;



		}

		/* ===== write log to 'xtrabackup_logfile' ====== */
		{
		ulint write_size;

		if (!finished) {
			write_size = RECV_SCAN_SIZE;
		} else {
			write_size = ut_dulint_minus(
					ut_dulint_align_up(group_scanned_lsn, OS_FILE_LOG_BLOCK_SIZE),
					start_lsn);
		}

		//printf("Wrinting offset= %lld, size= %lu\n", log_copy_offset, write_size);

		if (!xtrabackup_stream) {
			success = os_file_write(dst_log_path, dst_log, log_sys->buf,
					(ulint)(log_copy_offset & 0xFFFFFFFFUL),
					(ulint)(log_copy_offset >> 32), write_size);
		} else {
			ulint ret;
			ulint stdout_write_size = write_size;
			if (finished && !is_last
			    && group_scanned_lsn % OS_FILE_LOG_BLOCK_SIZE
			   )
				stdout_write_size -= OS_FILE_LOG_BLOCK_SIZE;
			if (stdout_write_size) {
				ret = write(fileno(stdout), log_sys->buf, stdout_write_size);
				if (ret == stdout_write_size) {
					success = TRUE;
				} else {
					fprintf(stderr, "write: %lu > %lu\n", stdout_write_size, ret);
					success = FALSE;
				}
			} else {
				success = TRUE; /* do nothing */
			}
		}

		log_copy_offset += write_size;

		if (finished && group_scanned_lsn % OS_FILE_LOG_BLOCK_SIZE)
		{
			/* if continue, it will start from align_down(group_scanned_lsn) */
			log_copy_offset -= OS_FILE_LOG_BLOCK_SIZE;
		}

		if(!success) {
			if (!xtrabackup_stream) {
				fprintf(stderr, "xtrabackup: Error: os_file_write to %s\n", dst_log_path);
			} else {
				fprintf(stderr, "xtrabackup: Error: write to stdout\n");
			}
			goto error;
		}


		}





		start_lsn = end_lsn;
	}



		group->scanned_lsn = group_scanned_lsn;
		

		fprintf(stderr, ">> log scanned up to (%"PRIu64")\n",group->scanned_lsn);

		group = UT_LIST_GET_NEXT(log_groups, group);

		/* update global variable*/
		log_copy_scanned_lsn = group_scanned_lsn;

		/* innodb_mirrored_log_groups must be 1, no other groups */
		ut_a(group == NULL);
	}


	if (!xtrabackup_stream) {
		success = os_file_flush(dst_log);
	} else {
		fflush(stdout);
		success = TRUE;
	}

	if(!success) {
		goto error;
	}

	return(FALSE);

error:
	if (!xtrabackup_stream)
		os_file_close(dst_log);
	fprintf(stderr, "xtrabackup: Error: xtrabackup_copy_logfile() failed.\n");
	return(TRUE);
}

/* copying logfile in background */
#define SLEEPING_PERIOD 5

static
#ifndef __WIN__
void*
#else
ulint
#endif
log_copying_thread(
	void*	)
{
	ulint	counter = 0;

	if (!xtrabackup_stream)
		ut_a(dst_log != -1);

	log_copying_running = TRUE;

	while(log_copying) {
		os_thread_sleep(200000); /*0.2 sec*/

		counter++;
		if(counter >= SLEEPING_PERIOD * 5) {
			if(xtrabackup_copy_logfile(log_copy_scanned_lsn, FALSE))
				goto end;
			counter = 0;
		}
	}

	/* last copying */
	if(xtrabackup_copy_logfile(log_copy_scanned_lsn, TRUE))
		goto end;

	log_copying_succeed = TRUE;
end:
	log_copying_running = FALSE;
	os_thread_exit(NULL);

	return(0);
}

/* io throttle watching (rough) */
static
#ifndef __WIN__
void*
#else
ulint
#endif
io_watching_thread(
	void*	)
{
	/* currently, for --backup only */
	ut_a(xtrabackup_backup);

	while (log_copying) {
		os_thread_sleep(1000000); /*1 sec*/

		//for DEBUG
		//if (io_ticket == xtrabackup_throttle) {
		//	fprintf(stderr, "There seem to be no IO...?\n");
		//}

		io_ticket = xtrabackup_throttle;
		os_event_set(wait_throttle);
	}

	/* stop io throttle */
	xtrabackup_throttle = 0;
	os_event_set(wait_throttle);

	os_thread_exit(NULL);

	return(0);
}

/************************************************************************
I/o-handler thread function. */
static

#ifndef __WIN__
void*
#else
ulint
#endif
io_handler_thread(
/*==============*/
	void*	arg)
{
	ulint	segment;
	ulint	i;
	
	segment = *((ulint*)arg);

	for (i = 0;; i++) {
		fil_aio_wait(segment);
	}

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit.
	The thread actually never comes here because it is exited in an
	os_event_wait(). */

	os_thread_exit(NULL);

#ifndef __WIN__
	return(NULL);				/* Not reached */
#else
	return(0);
#endif
}

/***************************************************************************
Creates an output directory for a given tablespace, if it does not exist */
static
int
xtrabackup_create_output_dir(
/*==========================*/
				/* out: 0 if succes, -1 if failure */
	fil_space_t *space)	/* in: tablespace */
{
	char	path[FN_REFLEN];
	char	*ptr1, *ptr2;

	/* mkdir if not exist */
	ptr1 = strstr(space->name, SRV_PATH_SEPARATOR_STR);
	if (ptr1) {
		ptr2 = strstr(ptr1 + 1, SRV_PATH_SEPARATOR_STR);
	} else {
		ptr2 = NULL;
	}
#ifdef XTRADB_BASED
	if(!trx_sys_sys_space(space->id) && ptr2)
#else
	if(space->id && ptr2)
#endif
	{
		/* single table space */
		*ptr2 = 0; /* temporary (it's my lazy..)*/
		snprintf(path, sizeof(path), "%s%s", xtrabackup_target_dir,
			 ptr1);
		*ptr2 = SRV_PATH_SEPARATOR;

		if (mkdir(path, 0777) != 0 && errno != EEXIST) {
			fprintf(stderr,
				"xtrabackup: Error: cannot mkdir %d: %s\n",
				errno, path);
			return -1;
		}
	}
	return 0;
}

/**************************************************************************
Datafiles copying thread.*/
static
os_thread_ret_t
data_copy_thread_func(
/*==================*/
	void *arg) /* thread context */
{
	data_thread_ctxt_t	*ctxt = (data_thread_ctxt_t *) arg;
	uint			num = ctxt->num;
	fil_space_t*		space;
	ibool			space_changed;
	fil_node_t*     	node;

	while ((node = datafiles_iter_next(ctxt->it, &space_changed)) != NULL) {
		space = node->space;

		if (space_changed && xtrabackup_create_output_dir(space))
			exit(EXIT_FAILURE);

		/* copy the datafile */
		if(xtrabackup_copy_datafile(node, num)) {
			fprintf(stderr, "[%02u] xtrabackup: Error: "
				"failed to copy datafile.\n",
				num);
			exit(EXIT_FAILURE);
		}
	}

	os_mutex_enter(ctxt->count_mutex);
	(*ctxt->count)--;
	os_mutex_exit(ctxt->count_mutex);

	os_thread_exit(NULL);
	OS_THREAD_DUMMY_RETURN;
}

/* CAUTION(?): Don't rename file_per_table during backup */
static void
xtrabackup_backup_func(void)
{
	struct stat stat_info;
	LSN64 latest_cp;

#ifdef USE_POSIX_FADVISE
	fprintf(stderr, "xtrabackup: uses posix_fadvise().\n");
#endif

	/* cd to datadir */

	if (chdir(mysql_real_data_home) != 0)
	{
		fprintf(stderr, "xtrabackup: cannot my_setwd %s\n", mysql_real_data_home);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "xtrabackup: cd to %s\n", mysql_real_data_home);

	mysql_data_home= mysql_data_home_buff;
	mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
	mysql_data_home[1]=0;

	/* set read only */
	srv_read_only = TRUE;

	/* initialize components */
        if(innodb_init_param())
                exit(EXIT_FAILURE);

        if (srv_file_flush_method_str == NULL) {
        	/* These are the default options */
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;

		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
#ifndef __WIN__        
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "fsync")) {
		srv_unix_file_flush_method = SRV_UNIX_FSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DSYNC")) {
	  	srv_unix_file_flush_method = SRV_UNIX_O_DSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DIRECT")) {
	  	srv_unix_file_flush_method = SRV_UNIX_O_DIRECT;
		fprintf(stderr,"xtrabackup: use O_DIRECT\n");
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "littlesync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_LITTLESYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "nosync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_NOSYNC;
#else
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "normal")) {
	  	srv_win_file_flush_method = SRV_WIN_IO_NORMAL;
	  	os_aio_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "unbuffered")) {
	  	srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
	  	os_aio_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str,
							"async_unbuffered")) {
	  	srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;	
#endif
	} else {
	  	fprintf(stderr, 
          	"xtrabackup: Unrecognized value %s for innodb_flush_method\n",
          				srv_file_flush_method_str);
	  	exit(EXIT_FAILURE);
	}

	if (srv_buf_pool_size >= 1000 * 1024 * 1024) {
                                  /* Here we still have srv_pool_size counted
                                  in kilobytes (in 4.0 this was in bytes)
				  srv_boot() converts the value to
                                  pages; if buffer pool is less than 1000 MB,
                                  assume fewer threads. */
                srv_max_n_threads = 50000;


	} else if (srv_buf_pool_size >= 8 * 1024 * 1024) {

                srv_max_n_threads = 10000;
        } else {
		srv_max_n_threads = 1000;       /* saves several MB of memory,
                                                especially in 64-bit
                                                computers */
        }

	{
	ulint	nr;
	ulint	i;

	nr = srv_n_data_files;
	
	for (i = 0; i < nr; i++) {
		srv_data_file_sizes[i] = srv_data_file_sizes[i]
					* ((1024 * 1024) / UNIV_PAGE_SIZE);
	}		

	srv_last_file_size_max = srv_last_file_size_max
					* ((1024 * 1024) / UNIV_PAGE_SIZE);
		
	srv_log_file_size = srv_log_file_size / UNIV_PAGE_SIZE;

	srv_log_buffer_size = srv_log_buffer_size / UNIV_PAGE_SIZE;

	srv_lock_table_size = 5 * (srv_buf_pool_size / UNIV_PAGE_SIZE);
	}

	os_sync_mutex = NULL;
	srv_general_init();

	{
	ibool	create_new_db;
#ifdef XTRADB_BASED
	ibool	create_new_doublewrite_file;
#endif
	ibool	log_file_created;
	ibool	log_created	= FALSE;
	ibool	log_opened	= FALSE;
	LSN64	min_flushed_lsn;
	LSN64	max_flushed_lsn;
	ulint   sum_of_new_sizes;
	ulint	err;
	ulint	i;




#define SRV_N_PENDING_IOS_PER_THREAD 	OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS	100

	srv_n_file_io_threads = 2 + srv_n_read_io_threads + srv_n_write_io_threads;

	os_aio_init(8 * SRV_N_PENDING_IOS_PER_THREAD,
		    srv_n_read_io_threads,
		    srv_n_write_io_threads,
		    SRV_MAX_N_PENDING_SYNC_IOS);

	fil_init(srv_file_per_table ? 50000 : 5000,
		 srv_max_n_open_files);

	fsp_init();
	log_init();

	lock_sys_create(srv_lock_table_size);

	for (i = 0; i < srv_n_file_io_threads; i++) {
		thread_nr[i] = i;

		os_thread_create(io_handler_thread, thread_nr + i, thread_ids + i);
    	}

	os_thread_sleep(200000); /*0.2 sec*/

	err = open_or_create_data_files(&create_new_db,
#ifdef XTRADB_BASED
					&create_new_doublewrite_file,
#endif
					&min_flushed_lsn, &max_flushed_lsn,
					&sum_of_new_sizes);
	if (err != DB_SUCCESS) {
	        fprintf(stderr,
"xtrabackup: Could not open or create data files.\n"
"xtrabackup: If you tried to add new data files, and it failed here,\n"
"xtrabackup: you should now edit innodb_data_file_path in my.cnf back\n"
"xtrabackup: to what it was, and remove the new ibdata files InnoDB created\n"
"xtrabackup: in this failed attempt. InnoDB only wrote those files full of\n"
"xtrabackup: zeros, but did not yet use them in any way. But be careful: do not\n"
"xtrabackup: remove old data files which contain your precious data!\n");

		//return((int) err);
		exit(EXIT_FAILURE);
	}

	/* create_new_db must not be TRUE.. */
	if (create_new_db) {
		fprintf(stderr, "xtrabackup: Something wrong with source files...\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < srv_n_log_files; i++) {
		err = open_or_create_log_file(create_new_db, &log_file_created,
							     log_opened, 0, i);
		if (err != DB_SUCCESS) {

			//return((int) err);
			exit(EXIT_FAILURE);
		}

		if (log_file_created) {
			log_created = TRUE;
		} else {
			log_opened = TRUE;
		}
		if ((log_opened && create_new_db)
			    		|| (log_opened && log_created)) {
			fprintf(stderr, 
	"xtrabackup: Error: all log files must be created at the same time.\n"
	"xtrabackup: All log files must be created also in database creation.\n"
	"xtrabackup: If you want bigger or smaller log files, shut down the\n"
	"xtrabackup: database and make sure there were no errors in shutdown.\n"
	"xtrabackup: Then delete the existing log files. Edit the .cnf file\n"
	"xtrabackup: and start the database again.\n");

			//return(DB_ERROR);
			exit(EXIT_FAILURE);
		}
	}

	/* log_file_created must not be TRUE, if online */
	if (log_file_created) {
		fprintf(stderr, "xtrabackup: Something wrong with source files...\n");
		exit(EXIT_FAILURE);
	}

	fil_load_single_table_tablespaces();

	}

	/* create extra LSN dir if it does not exist. */
	if (xtrabackup_extra_lsndir
            && (stat(xtrabackup_extra_lsndir,&stat_info) != 0)
            && (mkdir(xtrabackup_extra_lsndir,0777) != 0)){
		fprintf(stderr,"xtrabackup: Error: cannot mkdir %d: %s\n",errno,xtrabackup_extra_lsndir);
		exit(EXIT_FAILURE);
	}


	if (!xtrabackup_stream) {

	/* create target dir if not exist */
	if (stat(xtrabackup_target_dir,&stat_info) != 0
            && (mkdir(xtrabackup_target_dir,0777) != 0)){
		fprintf(stderr,"xtrabackup: Error: cannot mkdir %d: %s\n",errno,xtrabackup_target_dir);
		exit(EXIT_FAILURE);
	}

	} else {
		fprintf(stderr,"xtrabackup: Stream mode.\n");
		/* stdout can treat binary at Linux */
		//setmode(fileno(stdout), O_BINARY);
	}

        {
        fil_system_t*   f_system = fil_system;

	/* definition from recv_recovery_from_checkpoint_start() */
	log_group_t*	max_cp_group;
	ulint		max_cp_field;
	byte*		buf;
	byte		log_hdr_buf_[LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE];
	byte*		log_hdr_buf;
	ulint		err;

	ibool		success;

	/* start back ground thread to copy newer log */
	os_thread_id_t log_copying_thread_id;
	datafiles_iter_t *it;

	log_hdr_buf = (unsigned char*)ut_align(log_hdr_buf_, OS_FILE_LOG_BLOCK_SIZE);

	/* log space */
	//space = UT_LIST_GET_NEXT(space_list, UT_LIST_GET_FIRST(f_system->space_list));
	//printf("space: name=%s, id=%d, purpose=%d, size=%d\n",
	//	space->name, space->id, space->purpose, space->size);

	/* get current checkpoint_lsn */
	/* Look for the latest checkpoint from any of the log groups */
	
	err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

	if (err != DB_SUCCESS) {

		exit(EXIT_FAILURE);
	}
		
	log_group_read_checkpoint_info(max_cp_group, max_cp_field);
	buf = log_sys->checkpoint_buf;

	checkpoint_lsn_start = MACH_READ_64(buf + LOG_CHECKPOINT_LSN);
	checkpoint_no_start = MACH_READ_64(buf + LOG_CHECKPOINT_NO);

reread_log_header:
	fil_io(OS_FILE_READ | OS_FILE_LOG, TRUE, max_cp_group->space_id,
				0,
				0, 0, LOG_FILE_HDR_SIZE,
				log_hdr_buf, max_cp_group);

	/* check consistency of log file header to copy */
        err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

        if (err != DB_SUCCESS) {

                exit(EXIT_FAILURE);
        }

        log_group_read_checkpoint_info(max_cp_group, max_cp_field);
        buf = log_sys->checkpoint_buf;

	if(ut_dulint_cmp(checkpoint_no_start,
			MACH_READ_64(buf + LOG_CHECKPOINT_NO)) != 0) {
		checkpoint_lsn_start = MACH_READ_64(buf + LOG_CHECKPOINT_LSN);
		checkpoint_no_start = MACH_READ_64(buf + LOG_CHECKPOINT_NO);
		goto reread_log_header;
	}

	if (!xtrabackup_stream) {

		/* open 'xtrabackup_logfile' */
		sprintf(dst_log_path, "%s%s", xtrabackup_target_dir, "/xtrabackup_logfile");
		srv_normalize_path_for_win(dst_log_path);
		/* os_file_create reads srv_unix_file_flush_method for OS_DATA_FILE*/
		dst_log = os_file_create(
				0 /* dummy of innodb_file_data_key */,
				dst_log_path, OS_FILE_CREATE,
				OS_FILE_NORMAL, OS_DATA_FILE, &success);

                if (!success) {
                        /* The following call prints an error message */
                        os_file_get_last_error(TRUE);

                        fprintf(stderr,
"xtrabackup: error: cannot open %s\n",
                                dst_log_path);
                        exit(EXIT_FAILURE);
                }

#ifdef USE_POSIX_FADVISE
		posix_fadvise(dst_log, 0, 0, POSIX_FADV_DONTNEED);
#endif

	}

	/* label it */
	strcpy((char*) log_hdr_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
		"xtrabkup ");
	ut_sprintf_timestamp(
		(char*) log_hdr_buf + (LOG_FILE_WAS_CREATED_BY_HOT_BACKUP
				+ (sizeof "xtrabkup ") - 1));

	if (!xtrabackup_stream) {
		success = os_file_write(dst_log_path, dst_log, log_hdr_buf,
				0, 0, LOG_FILE_HDR_SIZE);
	} else {
		/* Stream */
		if (write(fileno(stdout), log_hdr_buf, LOG_FILE_HDR_SIZE)
				== LOG_FILE_HDR_SIZE) {
			success = TRUE;
		} else {
			success = FALSE;
		}
	}

	log_copy_offset += LOG_FILE_HDR_SIZE;
	if (!success) {
		if (dst_log != -1)
			os_file_close(dst_log);
		exit(EXIT_FAILURE);
	}

	/* start flag */
	log_copying = TRUE;

	/* start io throttle */
	if(xtrabackup_throttle) {
		os_thread_id_t io_watching_thread_id;

		io_ticket = xtrabackup_throttle;
		wait_throttle = os_event_create(NULL);

		os_thread_create(io_watching_thread, NULL, &io_watching_thread_id);
	}


	/* copy log file by current position */
	if(xtrabackup_copy_logfile(checkpoint_lsn_start, FALSE))
		exit(EXIT_FAILURE);


	os_thread_create(log_copying_thread, NULL, &log_copying_thread_id);



	if (!xtrabackup_stream) { /* stream mode is transaction log only */
		uint			i;
		uint			count;
		os_mutex_t		count_mutex;
		data_thread_ctxt_t 	*data_threads;

		ut_a(parallel > 0);

		if (parallel > 1)
			printf("xtrabackup: Starting %u threads for parallel "
			       "data files transfer\n", parallel);

		it = datafiles_iter_new(f_system);
		if (it == NULL) {
			fprintf(stderr,
				"xtrabackup: Error: "
				"datafiles_iter_new() failed.\n");
			exit(EXIT_FAILURE);
		}

		/* Create data copying threads */
		ut_a(parallel > 0);

		data_threads = (data_thread_ctxt_t *)
			ut_malloc(sizeof(data_thread_ctxt_t) * parallel);
		count = parallel;
		count_mutex = OS_MUTEX_CREATE();

		for (i = 0; i < parallel; i++) {
			data_threads[i].it = it;
			data_threads[i].num = i+1;
			data_threads[i].count = &count;
			data_threads[i].count_mutex = count_mutex;
			os_thread_create(data_copy_thread_func,
					 data_threads + i,
					 &data_threads[i].id);
		}

		/* Wait for threads to exit */
		while (1) {
			os_thread_sleep(1000000);
			os_mutex_enter(count_mutex);
			if (count == 0) {
				os_mutex_exit(count_mutex);
				break;
			}
			os_mutex_exit(count_mutex);
		}
		/* NOTE: It may not needed at "--backup" for now */
		/* mutex_enter(&(f_system->mutex)); */

		os_mutex_free(count_mutex);
		datafiles_iter_free(it);

	} //if (!xtrabackup_stream)

        //mutex_exit(&(f_system->mutex));
        }


	/* suspend-at-end */
	if (xtrabackup_suspend_at_end) {
		os_file_t	suspend_file = -1;
		char	suspend_path[FN_REFLEN];
		ibool	success, exists;
		os_file_type_t	type;

		sprintf(suspend_path, "%s%s", xtrabackup_target_dir,
			"/xtrabackup_suspended");

		srv_normalize_path_for_win(suspend_path);
		/* os_file_create reads srv_unix_file_flush_method */
		suspend_file = os_file_create(
					0 /* dummy of innodb_file_data_key */,
					suspend_path, OS_FILE_OVERWRITE,
					OS_FILE_NORMAL, OS_DATA_FILE, &success);

		if (!success) {
			fprintf(stderr, "xtrabackup: Error: failed to create file 'xtrabackup_suspended'\n");
		}

		if (suspend_file != -1)
			os_file_close(suspend_file);

		exists = TRUE;
		while (exists) {
			os_thread_sleep(200000); /*0.2 sec*/
			success = os_file_status(suspend_path, &exists, &type);
			if (!success)
				break;
		}
		xtrabackup_suspend_at_end = FALSE; /* suspend is 1 time */
	}

	/* read the latest checkpoint lsn */
	latest_cp = ut_dulint_zero;
	{
		log_group_t*	max_cp_group;
		ulint	max_cp_field;
		ulint	err;

		err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

		if (err != DB_SUCCESS) {
			fprintf(stderr, "xtrabackup: Error: recv_find_max_checkpoint() failed.\n");
			goto skip_last_cp;
		}

		log_group_read_checkpoint_info(max_cp_group, max_cp_field);

		latest_cp = MACH_READ_64(log_sys->checkpoint_buf + LOG_CHECKPOINT_LSN);

		if (!xtrabackup_stream) {
			printf("xtrabackup: The latest check point (for incremental): '%"PRIu64"'\n",
				latest_cp);
		} else {
			fprintf(stderr, "xtrabackup: The latest check point (for incremental): '%"PRIu64"'\n",
				latest_cp);
		}
	}
skip_last_cp:
	/* stop log_copying_thread */
	log_copying = FALSE;
	if (!xtrabackup_stream) {
		printf("xtrabackup: Stopping log copying thread");
		while (log_copying_running) {
			printf(".");
			os_thread_sleep(200000); /*0.2 sec*/
		}
		printf("\n");
	} else {
		while (log_copying_running)
			os_thread_sleep(200000); /*0.2 sec*/
	}

	/* output to metadata file */
	{
		char	filename[FN_REFLEN];

		if(!xtrabackup_incremental) {
			strcpy(metadata_type, "full-backuped");
			metadata_from_lsn = ut_dulint_zero;
		} else {
			strcpy(metadata_type, "incremental");
			metadata_from_lsn = incremental_lsn;
		}
		metadata_to_lsn = latest_cp;
		metadata_last_lsn = log_copy_scanned_lsn;

		sprintf(filename, "%s/%s", xtrabackup_target_dir, XTRABACKUP_METADATA_FILENAME);
		if (xtrabackup_write_metadata(filename))
			fprintf(stderr, "xtrabackup: error: xtrabackup_write_metadata(xtrabackup_target_dir)\n");

		if(xtrabackup_extra_lsndir) {
			sprintf(filename, "%s/%s", xtrabackup_extra_lsndir, XTRABACKUP_METADATA_FILENAME);
			if (xtrabackup_write_metadata(filename))
				fprintf(stderr, "xtrabackup: error: xtrabackup_write_metadata(xtrabackup_extra_lsndir)\n");
		}
	}

	if (!log_copying_succeed) {
		fprintf(stderr, "xtrabackup: Error: log_copying_thread failed.\n");
		exit(EXIT_FAILURE);
	}

	if (!xtrabackup_stream)
	        os_file_close(dst_log);

	if (wait_throttle)
		os_event_free(wait_throttle);

	if (!xtrabackup_stream) {
		printf("xtrabackup: Transaction log of lsn (%"PRIu64") to (%"PRIu64") was copied.\n",
			checkpoint_lsn_start, log_copy_scanned_lsn);
	} else {
		fprintf(stderr, "xtrabackup: Transaction log of lsn (%"PRIu64") to (%"PRIu64") was copied.\n",
			checkpoint_lsn_start, log_copy_scanned_lsn);
		if(xtrabackup_extra_lsndir) {
			char	filename[FN_REFLEN];
			sprintf(filename, "%s/%s", xtrabackup_extra_lsndir, XTRABACKUP_METADATA_FILENAME);
			if (xtrabackup_write_metadata(filename))
				fprintf(stderr, "xtrabackup: error: xtrabackup_write_metadata(xtrabackup_extra_lsndir)\n");
		}
	}
}

/* ================= stats ================= */
static bool
xtrabackup_stats_level(
	dict_index_t*	index,
	ulint		level)
{
	ulint	space;
	page_t*	page;

	rec_t*	node_ptr;

	ulint	right_page_no;

	page_cur_t	cursor;

	mtr_t	mtr;
	mem_heap_t*	heap	= mem_heap_create(256);

	ulint*	offsets = NULL;

	uint64_t n_pages, n_pages_extern;
	uint64_t sum_data, sum_data_extern;
	uint64_t n_recs;
	ulint	page_size;

	n_pages = sum_data = n_recs = 0;
	n_pages_extern = sum_data_extern = 0;

	buf_block_t*	block;
	ulint	zip_size;

	if (level == 0)
		fprintf(stdout, "        leaf pages: ");
	else
		fprintf(stdout, "     level %lu pages: ", level);

	mtr_start(&mtr);

	mtr_x_lock(&(index->lock), &mtr);
	block = btr_root_block_get(index, &mtr);
	page = buf_block_get_frame(block);

	space = page_get_space_id(page);
	zip_size = fil_space_get_zip_size(space);

	while (level != btr_page_get_level(page, &mtr)) {

		ut_a(space == buf_block_get_space(block));
		ut_a(space == page_get_space_id(page));
		ut_a(!page_is_leaf(page));

		page_cur_set_before_first(block, &cursor);
		page_cur_move_to_next(&cursor);

		node_ptr = page_cur_get_rec(&cursor);
		offsets = rec_get_offsets(node_ptr, index, offsets,
					ULINT_UNDEFINED, &heap);

		block = btr_node_ptr_get_child(node_ptr, index, offsets, &mtr);
		page = buf_block_get_frame(block);
	}

loop:
	mem_heap_empty(heap);
	offsets = NULL;
	mtr_x_lock(&(index->lock), &mtr);

	right_page_no = btr_page_get_next(page, &mtr);


	/*=================================*/
	//fprintf(stdout, "%lu ", (ulint) buf_frame_get_page_no(page));

	n_pages++;
	sum_data += page_get_data_size(page);
	n_recs += page_get_n_recs(page);


	if (level == 0) {
		page_cur_t	cur;
		ulint	n_fields;
		ulint	i;
		mem_heap_t*	local_heap	= NULL;
		ulint	offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*	local_offsets	= offsets_;

		*offsets_ = (sizeof offsets_) / sizeof *offsets_;

		page_cur_set_before_first(block, &cur);

		page_cur_move_to_next(&cur);

		for (;;) {
			if (page_cur_is_after_last(&cur)) {
				break;
			}

			local_offsets = rec_get_offsets(cur.rec, index, local_offsets,
						ULINT_UNDEFINED, &local_heap);
			n_fields = rec_offs_n_fields(local_offsets);

			for (i = 0; i < n_fields; i++) {
				if (rec_offs_nth_extern(local_offsets, i)) {
					page_t*	local_page;
					ulint	space_id;
					ulint	page_no;
					ulint	offset;
					byte*	blob_header;
					ulint	part_len;
					mtr_t	local_mtr;
					ulint	local_len;
					byte*	data;
					buf_block_t*	local_block;

					data = rec_get_nth_field(cur.rec, local_offsets, i, &local_len);

					ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);
					local_len -= BTR_EXTERN_FIELD_REF_SIZE;

					space_id = mach_read_from_4(data + local_len + BTR_EXTERN_SPACE_ID);
					page_no = mach_read_from_4(data + local_len + BTR_EXTERN_PAGE_NO);
					offset = mach_read_from_4(data + local_len + BTR_EXTERN_OFFSET);

					if (offset != FIL_PAGE_DATA)
						fprintf(stderr, "\nWarning: several record may share same external page.\n");

					for (;;) {
						mtr_start(&local_mtr);

						local_block = btr_block_get(space_id, zip_size, page_no, RW_S_LATCH, &local_mtr);
						local_page = buf_block_get_frame(local_block);

						blob_header = local_page + offset;
#define BTR_BLOB_HDR_PART_LEN		0
#define BTR_BLOB_HDR_NEXT_PAGE_NO	4
						//part_len = btr_blob_get_part_len(blob_header);
						part_len = mach_read_from_4(blob_header + BTR_BLOB_HDR_PART_LEN);

						//page_no = btr_blob_get_next_page_no(blob_header);
						page_no = mach_read_from_4(blob_header + BTR_BLOB_HDR_NEXT_PAGE_NO);

						offset = FIL_PAGE_DATA;




						/*=================================*/
						//fprintf(stdout, "[%lu] ", (ulint) buf_frame_get_page_no(page));

						n_pages_extern++;
						sum_data_extern += part_len;


						mtr_commit(&local_mtr);

						if (page_no == FIL_NULL)
							break;
					}
				}
			}

			page_cur_move_to_next(&cur);
		}
	}




	mtr_commit(&mtr);
	if (right_page_no != FIL_NULL) {
		mtr_start(&mtr);
		block = btr_block_get(space, zip_size, right_page_no, RW_X_LATCH, &mtr);
		page = buf_block_get_frame(block);
		goto loop;
	}
	mem_heap_free(heap);

	if (zip_size) {
		page_size = zip_size;
	} else {
		page_size = UNIV_PAGE_SIZE;
	}

	if (level == 0)
		fprintf(stdout, "recs=%"PRIu64", ", n_recs);

	fprintf(stdout, "pages=%"PRIu64", data=%"PRIu64" bytes, data/pages=%"PRIu64"%%",
		n_pages, sum_data,
		((sum_data * 100)/ page_size)/n_pages);


	if (level == 0 && n_pages_extern) {
		putc('\n', stdout);
		/* also scan blob pages*/
		fprintf(stdout, "    external pages: ");

		fprintf(stdout, "pages=%"PRIu64", data=%"PRIu64" bytes, data/pages=%"PRIu64"%%",
			n_pages_extern, sum_data_extern,
			((sum_data_extern * 100)/ page_size)/n_pages_extern);
	}

	putc('\n', stdout);

	if (level > 0) {
		xtrabackup_stats_level(index, level - 1);
	}

	return(TRUE);
}

static void
xtrabackup_stats_func(void)
{
	ulint n;

	/* cd to datadir */

	if (chdir(mysql_real_data_home) != 0)
	{
		fprintf(stderr, "xtrabackup: cannot my_setwd %s\n", mysql_real_data_home);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "xtrabackup: cd to %s\n", mysql_real_data_home);

	mysql_data_home= mysql_data_home_buff;
	mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
	mysql_data_home[1]=0;

	/* set read only */
	srv_read_only = TRUE;
	srv_fake_write = TRUE;

	/* initialize components */
	if(innodb_init_param())
		exit(EXIT_FAILURE);

	/* Check if the log files have been created, otherwise innodb_init()
	will crash when called with srv_read_only == TRUE */
	for (n = 0; n < srv_n_log_files; n++) {
		char		logname[FN_REFLEN];
		ibool		exists;
		os_file_type_t	type;

		sprintf(logname, "ib_logfile%lu", (ulong) n);
		if (!os_file_status(logname, &exists, &type) || !exists ||
		    type != OS_FILE_TYPE_FILE) {
			fprintf(stderr, "xtrabackup: Error: "
				"Cannot find log file %s.\n", logname);
			fprintf(stderr, "xtrabackup: Error: "
				"to use the statistics feature, you need a "
				"clean copy of the database including "
				"correctly sized log files, so you need to "
				"execute with --prepare twice to use this "
				"functionality on a backup.\n");
			exit(EXIT_FAILURE);
		}
	}

	fprintf(stderr, "xtrabackup: Starting 'read-only' InnoDB instance to gather index statistics.\n"
		"xtrabackup: Using %"PRIu64" bytes for buffer pool (set by --use-memory parameter)\n",
		xtrabackup_use_memory);

	if(innodb_init())
		exit(EXIT_FAILURE);

	fprintf(stdout, "\n\n<INDEX STATISTICS>\n");

	/* gather stats */

	{
	dict_table_t*	sys_tables;
	dict_index_t*	sys_index;
	dict_table_t*	table;
	btr_pcur_t	pcur;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	mtr_t		mtr;
	
	/* Enlarge the fatal semaphore wait timeout during the InnoDB table
	monitor printout */

	mutex_enter(&kernel_mutex);
	srv_fatal_semaphore_wait_threshold += 72000; /* 20 hours */
	mutex_exit(&kernel_mutex);

	mutex_enter(&(dict_sys->mutex));

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);

	btr_pcur_open_at_index_side(TRUE, sys_index, BTR_SEARCH_LEAF, &pcur,
								TRUE, &mtr);
loop:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur))
	{
		/* end of index */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		
		mutex_exit(&(dict_sys->mutex));

		/* Restore the fatal semaphore wait timeout */

		mutex_enter(&kernel_mutex);
		srv_fatal_semaphore_wait_threshold -= 72000; /* 20 hours */
		mutex_exit(&kernel_mutex);

		goto end;
	}	

	field = rec_get_nth_field_old(rec, 0, &len);

	if (!rec_get_deleted_flag(rec, 0))
	{

		/* We found one */

                char*	table_name = mem_strdupl((char*) field, len);

		btr_pcur_store_position(&pcur, &mtr);

		mtr_commit(&mtr);

		table = dict_table_get_low(table_name);
		mem_free(table_name);


		if (xtrabackup_tables) {
			char *p;
			int regres= 0;
			int i;

			p = strstr(table->name, SRV_PATH_SEPARATOR_STR);

			if (p)
				*p = '.';

			for (i = 0; i < tables_regex_num; i++) {
				regres = regexec(&tables_regex[i], table->name, 1, tables_regmatch, 0);
				if (regres != REG_NOMATCH)
					break;
			}

			if (p)
				*p = SRV_PATH_SEPARATOR;

			if ( regres == REG_NOMATCH )
				goto skip;
		}

		if (xtrabackup_tables_file) {
			xtrabackup_tables_t*	xtable;

			HASH_SEARCH(name_hash, tables_hash, ut_fold_string(table->name),
				    xtrabackup_tables_t*,
				    xtable,
				    ut_ad(xtable->name),
				    !strcmp(xtable->name, table->name));

			if (!xtable)
				goto skip;
		}


		if (table == NULL) {
			fputs("InnoDB: Failed to load table ", stderr);
			ut_print_namel(stderr, NULL, TRUE, (char*) field, len);
			putc('\n', stderr);
		} else {
			dict_index_t*	index;

			/* The table definition was corrupt if there
			is no index */

			if (dict_table_get_first_index(table)) {
#ifdef XTRADB_BASED
				dict_update_statistics(table, TRUE, FALSE);
#elif defined(INNODB_VERSION_SHORT)
				dict_update_statistics(table, TRUE);
#else
				dict_update_statistics_low(table, TRUE);
#endif
			}

			//dict_table_print_low(table);

			index = UT_LIST_GET_FIRST(table->indexes);
			while (index != NULL) {
{
	IB_INT64	n_vals;

	if (index->n_user_defined_cols > 0) {
		n_vals = index->stat_n_diff_key_vals[
					index->n_user_defined_cols];
	} else {
		n_vals = index->stat_n_diff_key_vals[1];
	}

	fprintf(stdout,
		"  table: %s, index: %s, space id: %lu, root page: %lu"
		", zip size: %lu"
		"\n  estimated statistics in dictionary:\n"
		"    key vals: %lu, leaf pages: %lu, size pages: %lu\n"
		"  real statistics:\n",
		table->name, index->name,
		(ulong) index->space,
		(ulong) index->page,
		(ulong) fil_space_get_zip_size(index->space),
		(ulong) n_vals,
		(ulong) index->stat_n_leaf_pages,
		(ulong) index->stat_index_size);

	{
		mtr_t	local_mtr;
		page_t*	root;
		ulint	page_level;

		mtr_start(&local_mtr);

		mtr_x_lock(&(index->lock), &local_mtr);
		root = btr_root_get(index, &local_mtr);

		page_level = btr_page_get_level(root, &local_mtr);

		xtrabackup_stats_level(index, page_level);

		mtr_commit(&local_mtr);
	}

	putc('\n', stdout);
}
				index = UT_LIST_GET_NEXT(indexes, index);
			}
		}

skip:
		mtr_start(&mtr);

		btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
	}

	goto loop;
	}

end:
	putc('\n', stdout);

	/* shutdown InnoDB */
	if(innodb_end())
		exit(EXIT_FAILURE);
}

/* ================= prepare ================= */

static bool
xtrabackup_init_temp_log(void)
{
	os_file_t	src_file = -1;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	ibool	success;

	ulint	field;
	byte*	log_buf;
	byte*	log_buf_ = NULL;

	IB_INT64	file_size;

	LSN64	max_no;
	LSN64	max_lsn= 0;
	LSN64	checkpoint_no;

	ulint	fold;

	max_no = ut_dulint_zero;

	if(!xtrabackup_incremental_dir) {
		sprintf(dst_path, "%s%s", xtrabackup_target_dir, "/ib_logfile0");
		sprintf(src_path, "%s%s", xtrabackup_target_dir, "/xtrabackup_logfile");
	} else {
		sprintf(dst_path, "%s%s", xtrabackup_incremental_dir, "/ib_logfile0");
		sprintf(src_path, "%s%s", xtrabackup_incremental_dir, "/xtrabackup_logfile");
	}

	srv_normalize_path_for_win(dst_path);
	srv_normalize_path_for_win(src_path);
retry:
	src_file = os_file_create_simple_no_error_handling(
					0 /* dummy of innodb_file_data_key */,
					src_path, OS_FILE_OPEN,
					OS_FILE_READ_WRITE /* OS_FILE_READ_ONLY */, &success);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(TRUE);

		fprintf(stderr,
"xtrabackup: Warning: cannot open %s. will try to find.\n",
			src_path);

		/* check if ib_logfile0 may be xtrabackup_logfile */
		src_file = os_file_create_simple_no_error_handling(
				0 /* dummy of innodb_file_data_key */,
				dst_path, OS_FILE_OPEN,
				OS_FILE_READ_WRITE /* OS_FILE_READ_ONLY */, &success);
		if (!success) {
			os_file_get_last_error(TRUE);
			fprintf(stderr,
"  xtrabackup: Fatal error: cannot find %s.\n",
			src_path);

			goto error;
		}

		log_buf_ = (unsigned char*) ut_malloc(LOG_FILE_HDR_SIZE * 2);
		log_buf = (unsigned char*) ut_align(log_buf_, LOG_FILE_HDR_SIZE);

		success = os_file_read(src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
		if (!success) {
			goto error;
		}

		if ( ut_memcmp(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
				(byte*)"xtrabkup", (sizeof "xtrabkup") - 1) == 0) {
			fprintf(stderr,
"  xtrabackup: 'ib_logfile0' seems to be 'xtrabackup_logfile'. will retry.\n");

			ut_free(log_buf_);
			log_buf_ = NULL;

			os_file_close(src_file);
			src_file = -1;

			/* rename and try again */
			success = os_file_rename(
					0 /* dummy of innodb_file_data_key */,
					dst_path, src_path);
			if (!success) {
				goto error;
			}

			goto retry;
		}

		fprintf(stderr,
"  xtrabackup: Fatal error: cannot find %s.\n",
		src_path);

		ut_free(log_buf_);
		log_buf_ = NULL;

		os_file_close(src_file);
		src_file = -1;

		goto error;
	}

#ifdef USE_POSIX_FADVISE
	posix_fadvise(src_file, 0, 0, POSIX_FADV_SEQUENTIAL);
	posix_fadvise(src_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		os_file_set_nocache(src_file, src_path, "OPEN");
	}

	file_size = os_file_get_size_as_iblonglong(src_file);


	/* TODO: We should skip the following modifies, if it is not the first time. */
	log_buf_ = (unsigned char*) ut_malloc(UNIV_PAGE_SIZE * 129);
	log_buf = (unsigned char*) ut_align(log_buf_, UNIV_PAGE_SIZE);

	/* read log file header */
	success = os_file_read(src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	if ( ut_memcmp(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
			(byte*)"xtrabkup", (sizeof "xtrabkup") - 1) != 0 ) {
		printf("xtrabackup: notice: xtrabackup_logfile was already used to '--prepare'.\n");
		goto skip_modify;
	} else {
		/* clear it later */
		//memset(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
		//		' ', 4);
	}

	/* read last checkpoint lsn */
	for (field = LOG_CHECKPOINT_1; field <= LOG_CHECKPOINT_2;
			field += LOG_CHECKPOINT_2 - LOG_CHECKPOINT_1) {
		if (!recv_check_cp_is_consistent(log_buf + field))
			goto not_consistent;

		checkpoint_no = MACH_READ_64(log_buf + field + LOG_CHECKPOINT_NO);

		if (ut_dulint_cmp(checkpoint_no, max_no) >= 0) {
			max_no = checkpoint_no;
			max_lsn = MACH_READ_64(log_buf + field + LOG_CHECKPOINT_LSN);
/*
			mach_write_to_4(log_buf + field + LOG_CHECKPOINT_OFFSET,
					LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
					ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE)));

			ulint	fold;
			fold = ut_fold_binary(log_buf + field, LOG_CHECKPOINT_CHECKSUM_1);
			mach_write_to_4(log_buf + field + LOG_CHECKPOINT_CHECKSUM_1, fold);

			fold = ut_fold_binary(log_buf + field + LOG_CHECKPOINT_LSN,
				LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
			mach_write_to_4(log_buf + field + LOG_CHECKPOINT_CHECKSUM_2, fold);
*/
		}
not_consistent:
		;
	}

	if (ut_dulint_cmp(max_no, ut_dulint_zero) == 0) {
		fprintf(stderr, "xtrabackup: No valid checkpoint found.\n");
		goto error;
	}


	/* It seems to be needed to overwrite the both checkpoint area. */
	MACH_WRITE_64(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN, max_lsn);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_OFFSET,
			LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
			ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE)));
#ifdef XTRADB_BASED
	MACH_WRITE_64(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_ARCHIVED_LSN,
			(ib_uint64_t)(LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
					ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE))));
#endif
	fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_1, LOG_CHECKPOINT_CHECKSUM_1);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_CHECKSUM_1, fold);

	fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_LSN,
		LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
	mach_write_to_4(log_buf + LOG_CHECKPOINT_1 + LOG_CHECKPOINT_CHECKSUM_2, fold);

	MACH_WRITE_64(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN, max_lsn);
        mach_write_to_4(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_OFFSET,
                        LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
                        ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE)));
#ifdef XTRADB_BASED
	MACH_WRITE_64(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_ARCHIVED_LSN,
			(ib_uint64_t)(LOG_FILE_HDR_SIZE + ut_dulint_minus(max_lsn,
					ut_dulint_align_down(max_lsn,OS_FILE_LOG_BLOCK_SIZE))));
#endif
        fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_2, LOG_CHECKPOINT_CHECKSUM_1);
        mach_write_to_4(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_CHECKSUM_1, fold);

        fold = ut_fold_binary(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_LSN,
                LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);
        mach_write_to_4(log_buf + LOG_CHECKPOINT_2 + LOG_CHECKPOINT_CHECKSUM_2, fold);


	success = os_file_write(src_path, src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	/* expand file size (9/8) and align to UNIV_PAGE_SIZE */

	if (file_size % UNIV_PAGE_SIZE) {
		memset(log_buf, 0, UNIV_PAGE_SIZE);
		success = os_file_write(src_path, src_file, log_buf,
				(ulint)(file_size & 0xFFFFFFFFUL),
				(ulint)(file_size >> 32),
				UNIV_PAGE_SIZE - (file_size % UNIV_PAGE_SIZE));
		if (!success) {
			goto error;
		}

		file_size = os_file_get_size_as_iblonglong(src_file);
	}

	/* TODO: We should judge whether the file is already expanded or not... */
	{
		ulint	expand;

		memset(log_buf, 0, UNIV_PAGE_SIZE * 128);
		expand = file_size / UNIV_PAGE_SIZE / 8;

		for (; expand > 128; expand -= 128) {
			success = os_file_write(src_path, src_file, log_buf,
					(ulint)(file_size & 0xFFFFFFFFUL),
					(ulint)(file_size >> 32),
					UNIV_PAGE_SIZE * 128);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE * 128;
		}

		if (expand) {
			success = os_file_write(src_path, src_file, log_buf,
					(ulint)(file_size & 0xFFFFFFFFUL),
					(ulint)(file_size >> 32),
					expand * UNIV_PAGE_SIZE);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE * expand;
		}
	}

	/* make larger than 2MB */
	if (file_size < 2*1024*1024L) {
		memset(log_buf, 0, UNIV_PAGE_SIZE);
		while (file_size < 2*1024*1024L) {
			success = os_file_write(src_path, src_file, log_buf,
				(ulint)(file_size & 0xFFFFFFFFUL),
				(ulint)(file_size >> 32),
				UNIV_PAGE_SIZE);
			if (!success) {
				goto error;
			}
			file_size += UNIV_PAGE_SIZE;
		}
		file_size = os_file_get_size_as_iblonglong(src_file);
	}

	printf("xtrabackup: xtrabackup_logfile detected: size=%"PRIu64", start_lsn=(%"PRIu64")\n",
		file_size, max_lsn);

	os_file_close(src_file);
	src_file = -1;

	/* Backup log parameters */
	innobase_log_group_home_dir_backup = innobase_log_group_home_dir;
	innobase_log_file_size_backup      = innobase_log_file_size;
	innobase_log_files_in_group_backup = innobase_log_files_in_group;

	/* fake InnoDB */
	innobase_log_group_home_dir = NULL;
	innobase_log_file_size      = file_size;
	innobase_log_files_in_group = 1;

	srv_thread_concurrency = 0;

	/* rename 'xtrabackup_logfile' to 'ib_logfile0' */
	success = os_file_rename(
			0 /* dummy of innodb_file_data_key */,
			src_path, dst_path);
	if (!success) {
		goto error;
	}
	xtrabackup_logfile_is_renamed = TRUE;

	ut_free(log_buf_);

	return(FALSE);

skip_modify:
	os_file_close(src_file);
	src_file = -1;
	ut_free(log_buf_);
	return(FALSE);

error:
	if (src_file != -1)
		os_file_close(src_file);
	if (log_buf_)
		ut_free(log_buf_);
	fprintf(stderr, "xtrabackup: Error: xtrabackup_init_temp_log() failed.\n");
	return(TRUE); /*ERROR*/
}

/***********************************************************************
Generates path to the meta file path from a given path to an incremental .delta
by replacing trailing ".delta" with ".meta", or returns error if 'delta_path'
does not end with the ".delta" character sequence.
@return TRUE on success, FALSE on error. */
static
ibool
get_meta_path(
	const char	*delta_path,	/* in: path to a .delta file */
	char 		*meta_path)	/* out: path to the corresponding .meta
					file */
{
	size_t		len = strlen(delta_path);

	if (len <= 6 || strcmp(delta_path + len - 6, ".delta")) {
		return FALSE;
	}
	memcpy(meta_path, delta_path, len - 6);
	strcpy(meta_path + len - 6, XB_DELTA_INFO_SUFFIX);

	return TRUE;
}

static void
xtrabackup_apply_delta(
	const char*	dirname,	/* in: dir name of incremental */
	const char*	dbname,		/* in: database name (ibdata: NULL) */
	const char*	filename,	/* in: file name (not a path),
					including the .delta extension */
	bool )
{
	os_file_t	src_file = -1;
	os_file_t	dst_file = -1;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	char	meta_path[FN_REFLEN];
	ibool	success;

	ibool	last_buffer = FALSE;
	ulint	page_in_buffer;
	ulint	incremental_buffers = 0;

	xb_delta_info_t info;
	ulint		page_size;
	ulint		page_size_shift;

	ut_a(xtrabackup_incremental);

	if (dbname) {
		snprintf(src_path, sizeof(src_path), "%s/%s/%s",
			 dirname, dbname, filename);
		snprintf(dst_path, sizeof(dst_path), "%s/%s/%s",
			 xtrabackup_real_target_dir, dbname, filename);
	} else {
		snprintf(src_path, sizeof(src_path), "%s/%s",
			 dirname, filename);
		snprintf(dst_path, sizeof(dst_path), "%s/%s",
			 xtrabackup_real_target_dir, filename);
	}
	dst_path[strlen(dst_path) - 6] = '\0';

	if (!get_meta_path(src_path, meta_path)) {
		goto error;
	}

	srv_normalize_path_for_win(dst_path);
	srv_normalize_path_for_win(src_path);
	srv_normalize_path_for_win(meta_path);

	if (!xb_read_delta_metadata(meta_path, &info)) {
		goto error;
	}

	page_size = info.page_size;
	page_size_shift = get_bit_shift(page_size);
	fprintf(stderr, "xtrabackup: page size for %s is %lu bytes\n",
		src_path, page_size);
	if (page_size_shift < 10 ||
	    page_size_shift > UNIV_PAGE_SIZE_SHIFT_MAX) {
		fprintf(stderr,
			"xtrabackup: error: invalid value of page_size "
			"(%lu bytes) read from %s\n", page_size, meta_path);
		goto error;
	}
	
	src_file = os_file_create_simple_no_error_handling(
			0 /* dummy of innodb_file_data_key */,
			src_path, OS_FILE_OPEN, OS_FILE_READ_WRITE, &success);
	if (!success) {
		os_file_get_last_error(TRUE);
		fprintf(stderr,
			"xtrabackup: error: cannot open %s\n",
			src_path);
		goto error;
	}

#ifdef USE_POSIX_FADVISE
	posix_fadvise(src_file, 0, 0, POSIX_FADV_SEQUENTIAL);
	posix_fadvise(src_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		os_file_set_nocache(src_file, src_path, "OPEN");
	}

	dst_file = os_file_create_simple_no_error_handling(
			0 /* dummy of innodb_file_data_key */,
			dst_path, OS_FILE_OPEN, OS_FILE_READ_WRITE, &success);
	if (!success) {
		os_file_get_last_error(TRUE);
		fprintf(stderr,
			"xtrabackup: error: cannot open %s\n",
			dst_path);
		goto error;
	}

#ifdef USE_POSIX_FADVISE
	posix_fadvise(dst_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		os_file_set_nocache(dst_file, dst_path, "OPEN");
	}

	printf("Applying %s ...\n", src_path);

	while (!last_buffer) {
		ulint cluster_header;

		/* read to buffer */
		/* first block of block cluster */
		success = os_file_read(src_file, incremental_buffer,
				       ((incremental_buffers * (page_size / 4))
					<< page_size_shift) & 0xFFFFFFFFUL,
				       (incremental_buffers * (page_size / 4))
				       >> (32 - page_size_shift),
				       page_size);
		if (!success) {
			goto error;
		}

		cluster_header = mach_read_from_4(incremental_buffer);
		switch(cluster_header) {
			case 0x78747261UL: /*"xtra"*/
				break;
			case 0x58545241UL: /*"XTRA"*/
				last_buffer = TRUE;
				break;
			default:
				fprintf(stderr,
					"xtrabackup: error: %s seems not .delta file.\n",
					src_path);
				goto error;
		}

		for (page_in_buffer = 1; page_in_buffer < page_size / 4;
		     page_in_buffer++) {
			if (mach_read_from_4(incremental_buffer + page_in_buffer * 4)
			    == 0xFFFFFFFFUL)
				break;
		}

		ut_a(last_buffer || page_in_buffer == page_size / 4);

		/* read whole of the cluster */
		success = os_file_read(src_file, incremental_buffer,
				       ((incremental_buffers * (page_size / 4))
					<< page_size_shift) & 0xFFFFFFFFUL,
				       (incremental_buffers * (page_size / 4))
				       >> (32 - page_size_shift),
				       page_in_buffer * page_size);
		if (!success) {
			goto error;
		}

		for (page_in_buffer = 1; page_in_buffer < page_size / 4;
		     page_in_buffer++) {
			ulint offset_on_page;

			offset_on_page = mach_read_from_4(incremental_buffer + page_in_buffer * 4);

			if (offset_on_page == 0xFFFFFFFFUL)
				break;

			/* apply blocks in the cluster */
//			if (ut_dulint_cmp(incremental_lsn,
//				MACH_READ_64(incremental_buffer
//						 + page_in_buffer * page_size
//						 + FIL_PAGE_LSN)) >= 0)
//				continue;

			success = os_file_write(dst_path, dst_file,
					incremental_buffer +
						page_in_buffer * page_size,
					(offset_on_page << page_size_shift) &
						0xFFFFFFFFUL,
					offset_on_page >> (32 - page_size_shift),
					page_size);
			if (!success) {
				goto error;
			}
		}

		incremental_buffers++;
	}

	if (src_file != -1)
		os_file_close(src_file);
	if (dst_file != -1)
		os_file_close(dst_file);
	return;

error:
	if (src_file != -1)
		os_file_close(src_file);
	if (dst_file != -1)
		os_file_close(dst_file);
	fprintf(stderr, "xtrabackup: Error: xtrabackup_apply_delta() failed.\n");
	return;
}

static void
xtrabackup_apply_deltas(bool check_newer)
{
	int		ret;
	char		dbpath[FN_REFLEN];
	os_file_dir_t	dir;
	os_file_dir_t	dbdir;
	os_file_stat_t	dbinfo;
	os_file_stat_t	fileinfo;
	ulint		err 		= DB_SUCCESS;
	static char	current_dir[2];

	current_dir[0] = FN_CURLIB;
	current_dir[1] = 0;
	srv_data_home = current_dir;

	/* datafile */
	dbdir = os_file_opendir(xtrabackup_incremental_dir, FALSE);

	if (dbdir != NULL) {
		ret = fil_file_readdir_next_file(&err, xtrabackup_incremental_dir, dbdir,
							&fileinfo);
		while (ret == 0) {
			if (fileinfo.type == OS_FILE_TYPE_DIR) {
				goto next_file_item_1;
			}

			if (strlen(fileinfo.name) > 6
			    && 0 == strcmp(fileinfo.name + 
					strlen(fileinfo.name) - 6,
					".delta")) {
				xtrabackup_apply_delta(
					xtrabackup_incremental_dir, NULL,
					fileinfo.name, check_newer);
			}
next_file_item_1:
			ret = fil_file_readdir_next_file(&err,
							xtrabackup_incremental_dir, dbdir,
							&fileinfo);
		}

		os_file_closedir(dbdir);
	} else {
		fprintf(stderr, "xtrabackup: Cannot open dir %s\n", xtrabackup_incremental_dir);
	}

	/* single table tablespaces */
	dir = os_file_opendir(xtrabackup_incremental_dir, FALSE);

	if (dir == NULL) {
		fprintf(stderr, "xtrabackup: Cannot open dir %s\n", xtrabackup_incremental_dir);
	}

		ret = fil_file_readdir_next_file(&err, xtrabackup_incremental_dir, dir,
								&dbinfo);
	while (ret == 0) {
		if (dbinfo.type == OS_FILE_TYPE_FILE
		    || dbinfo.type == OS_FILE_TYPE_UNKNOWN) {

		        goto next_datadir_item;
		}

		sprintf(dbpath, "%s/%s", xtrabackup_incremental_dir,
								dbinfo.name);
		srv_normalize_path_for_win(dbpath);

		dbdir = os_file_opendir(dbpath, FALSE);

		if (dbdir != NULL) {

			ret = fil_file_readdir_next_file(&err, dbpath, dbdir,
								&fileinfo);
			while (ret == 0) {

			        if (fileinfo.type == OS_FILE_TYPE_DIR) {

				        goto next_file_item_2;
				}

				if (strlen(fileinfo.name) > 6
				    && 0 == strcmp(fileinfo.name + 
						strlen(fileinfo.name) - 6,
						".delta")) {
					/* The name ends in .delta; try opening
					the file */
					xtrabackup_apply_delta(
						xtrabackup_incremental_dir, dbinfo.name,
						fileinfo.name, check_newer);
				}
next_file_item_2:
				ret = fil_file_readdir_next_file(&err,
								dbpath, dbdir,
								&fileinfo);
			}

			os_file_closedir(dbdir);
		}
next_datadir_item:
		ret = fil_file_readdir_next_file(&err,
						xtrabackup_incremental_dir,
								dir, &dbinfo);
	}

	os_file_closedir(dir);

}

static bool
xtrabackup_close_temp_log(bool clear_flag)
{
	os_file_t	src_file = -1;
	char	src_path[FN_REFLEN];
	char	dst_path[FN_REFLEN];
	ibool	success;

	byte*	log_buf;
	byte*	log_buf_ = NULL;


	if (!xtrabackup_logfile_is_renamed)
		return(FALSE);

	/* Restore log parameters */
	innobase_log_group_home_dir = innobase_log_group_home_dir_backup;
	innobase_log_file_size      = innobase_log_file_size_backup;
	innobase_log_files_in_group = innobase_log_files_in_group_backup;

	/* rename 'ib_logfile0' to 'xtrabackup_logfile' */
	if(!xtrabackup_incremental_dir) {
		sprintf(dst_path, "%s%s", xtrabackup_target_dir, "/ib_logfile0");
		sprintf(src_path, "%s%s", xtrabackup_target_dir, "/xtrabackup_logfile");
	} else {
		sprintf(dst_path, "%s%s", xtrabackup_incremental_dir, "/ib_logfile0");
		sprintf(src_path, "%s%s", xtrabackup_incremental_dir, "/xtrabackup_logfile");
	}

	srv_normalize_path_for_win(dst_path);
	srv_normalize_path_for_win(src_path);

	success = os_file_rename(
			0 /* dummy of innodb_file_data_key */,
			dst_path, src_path);
	if (!success) {
		goto error;
	}
	xtrabackup_logfile_is_renamed = FALSE;

	if (!clear_flag)
		return(FALSE);

	/* clear LOG_FILE_WAS_CREATED_BY_HOT_BACKUP field */
	src_file = os_file_create_simple_no_error_handling(
				0 /* dummy of innodb_file_data_key */,
				src_path, OS_FILE_OPEN,
				OS_FILE_READ_WRITE, &success);
	if (!success) {
		goto error;
	}

#ifdef USE_POSIX_FADVISE
	posix_fadvise(src_file, 0, 0, POSIX_FADV_DONTNEED);
#endif

	if (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT) {
		os_file_set_nocache(src_file, src_path, "OPEN");
	}

	log_buf_ = (unsigned char*) ut_malloc(LOG_FILE_HDR_SIZE * 2);
	log_buf = (unsigned char*) ut_align(log_buf_, LOG_FILE_HDR_SIZE);

	success = os_file_read(src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	memset(log_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP, ' ', 4);

	success = os_file_write(src_path, src_file, log_buf, 0, 0, LOG_FILE_HDR_SIZE);
	if (!success) {
		goto error;
	}

	os_file_close(src_file);
	src_file = -1;

	return(FALSE);
error:
	if (src_file != -1)
		os_file_close(src_file);
	if (log_buf_)
		ut_free(log_buf_);
	fprintf(stderr, "xtrabackup: Error: xtrabackup_close_temp_log() failed.\n");
	return(TRUE); /*ERROR*/
}

static void
xtrabackup_prepare_func(void)
{
	/* cd to target-dir */

	if (chdir(xtrabackup_real_target_dir) != 0)
	{
		fprintf(stderr, "xtrabackup: cannot my_setwd %s\n", xtrabackup_real_target_dir);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "xtrabackup: cd to %s\n", xtrabackup_real_target_dir);

	xtrabackup_target_dir= mysql_data_home_buff;
	mysql_data_home_buff[0]=FN_CURLIB;		// all paths are relative from here
	mysql_data_home_buff[1]=0;

	/* read metadata of target */
	{
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_target_dir, XTRABACKUP_METADATA_FILENAME);

		if (xtrabackup_read_metadata(filename))
			fprintf(stderr, "xtrabackup: error: xtrabackup_read_metadata()\n");

		if (!strcmp(metadata_type, "full-backuped")) {
			fprintf(stderr, "xtrabackup: This target seems to be not prepared yet.\n");
		} else if (!strcmp(metadata_type, "full-prepared")) {
			fprintf(stderr, "xtrabackup: This target seems to be already prepared.\n");
			goto skip_check;
		} else {
			fprintf(stderr, "xtrabackup: This target seems not to have correct metadata...\n");
		}

		if (xtrabackup_incremental) {
			fprintf(stderr,
			"xtrabackup: error: applying incremental backup needs target prepared.\n");
			exit(EXIT_FAILURE);
		}
skip_check:
		if (xtrabackup_incremental
		    && ut_dulint_cmp(metadata_to_lsn, incremental_lsn) != 0) {
			fprintf(stderr,
			"xtrabackup: error: This incremental backup seems not to be proper for the target.\n"
			"xtrabackup:  Check 'to_lsn' of the target and 'from_lsn' of the incremental.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* Create logfiles for recovery from 'xtrabackup_logfile', before start InnoDB */
	srv_max_n_threads = 1000;
	os_sync_mutex = NULL;
	ut_mem_init();
#ifdef XTRADB_BASED
	/* temporally dummy value to avoid crash */
	srv_page_size_shift = 14;
	srv_page_size = (1 << srv_page_size_shift);
#endif
	os_sync_init();
	sync_init();
	os_io_init_simple();
	if(xtrabackup_init_temp_log())
		goto error;

	if(xtrabackup_incremental)
		xtrabackup_apply_deltas(TRUE);

	sync_close();
	sync_initialized = FALSE;
	os_sync_free();
	os_sync_mutex = NULL;
	ut_free_all_mem();

	/* check the accessibility of target-dir */
	/* ############# TODO ##################### */

	if(innodb_init_param())
		goto error;

	srv_apply_log_only = (ibool) xtrabackup_apply_log_only;

	/* increase IO threads */
	if(srv_n_file_io_threads < 10) {
		srv_n_file_io_threads = 10;
	}

	fprintf(stderr, "xtrabackup: Starting InnoDB instance for recovery.\n"
		"xtrabackup: Using %"PRIu64" bytes for buffer pool (set by --use-memory parameter)\n",
		xtrabackup_use_memory);

	if(innodb_init())
		goto error;

	//printf("Hello InnoDB world!\n");

	/* TEST: innodb status*/
/*
	ulint	trx_list_start = ULINT_UNDEFINED;
	ulint	trx_list_end = ULINT_UNDEFINED;
	srv_printf_innodb_monitor(stdout, &trx_list_start, &trx_list_end);
*/
	/* TEST: list of datafiles and transaction log files and LSN*/
/*
	{
	fil_system_t*   f_system = fil_system;
	fil_space_t*	space;
	fil_node_t*	node;

        mutex_enter(&(f_system->mutex));

        space = UT_LIST_GET_FIRST(f_system->space_list);

        while (space != NULL) {
		printf("space: name=%s, id=%d, purpose=%d, size=%d\n",
			space->name, space->id, space->purpose, space->size);

                node = UT_LIST_GET_FIRST(space->chain);

                while (node != NULL) {
			printf("node: name=%s, open=%d, size=%d\n",
				node->name, node->open, node->size);

                        node = UT_LIST_GET_NEXT(chain, node);
                }
                space = UT_LIST_GET_NEXT(space_list, space);
        }

        mutex_exit(&(f_system->mutex));
	}
*/
	/* align space sizes along with fsp header */
	{
	fil_system_t*	f_system = fil_system;
	fil_space_t*	space;

	mutex_enter(&(f_system->mutex));
	space = UT_LIST_GET_FIRST(f_system->space_list);

	while (space != NULL) {
		byte*	header;
		ulint	size;
		ulint	actual_size;
		mtr_t	mtr;
		buf_block_t*	block;
		ulint	flags;

		if (space->purpose == FIL_TABLESPACE) {
			mutex_exit(&(f_system->mutex));

			mtr_start(&mtr);

			mtr_s_lock(fil_space_get_latch(space->id, &flags), &mtr);

			block = buf_page_get(space->id,
					     dict_table_flags_to_zip_size(flags),
					     0, RW_S_LATCH, &mtr);
			header = FIL_PAGE_DATA /*FSP_HEADER_OFFSET*/
				+ buf_block_get_frame(block);

			size = mtr_read_ulint(header + 8 /* FSP_SIZE */, MLOG_4BYTES, &mtr);

			mtr_commit(&mtr);

			//printf("%d, %d\n", space->id, size);

			fil_extend_space_to_desired_size(&actual_size, space->id, size);

			mutex_enter(&(f_system->mutex));
		}

		space = UT_LIST_GET_NEXT(space_list, space);
	}

	mutex_exit(&(f_system->mutex));
	}



	if (xtrabackup_export) {
		printf("xtrabackup: export option is specified.\n");
		if (innobase_file_per_table) {
			fil_system_t*	f_system = fil_system;
			fil_space_t*	space;
			fil_node_t*	node;
			os_file_t	info_file = -1;
			char		info_file_path[FN_REFLEN];
			ibool		success;
			char		table_name[FN_REFLEN];

			byte*		page;
			byte*		buf = NULL;

			buf = (byte*) ut_malloc(UNIV_PAGE_SIZE * 2);
			page = (byte*) ut_align(buf, UNIV_PAGE_SIZE);

			/* flush insert buffer at shutdwon */
			innobase_fast_shutdown = 0;

			mutex_enter(&(f_system->mutex));

			space = UT_LIST_GET_FIRST(f_system->space_list);
			while (space != NULL) {
				/* treat file_per_table only */
				if (space->purpose != FIL_TABLESPACE
#ifdef XTRADB_BASED
				    || trx_sys_sys_space(space->id)
#else
				    || space->id == 0
#endif
				   )
				{
					space = UT_LIST_GET_NEXT(space_list, space);
					continue;
				}

				node = UT_LIST_GET_FIRST(space->chain);
				while (node != NULL) {
					int len;
					char *next, *prev, *p;
					dict_table_t*	table;
					dict_index_t*	index;
					ulint		n_index;

					/* node exist == file exist, here */
					strncpy(info_file_path, node->name, FN_REFLEN);
					len = strlen(info_file_path);
					info_file_path[len - 3] = 'e';
					info_file_path[len - 2] = 'x';
					info_file_path[len - 1] = 'p';

					p = info_file_path;
					prev = NULL;
					while ((next = strstr(p, SRV_PATH_SEPARATOR_STR)) != NULL)
					{
						prev = p;
						p = next + 1;
					}
					info_file_path[len - 4] = 0;
					strncpy(table_name, prev, FN_REFLEN);

					info_file_path[len - 4] = '.';

					mutex_exit(&(f_system->mutex));
					mutex_enter(&(dict_sys->mutex));

					table = dict_table_get_low(table_name);
					if (!table) {
						fprintf(stderr,
"xtrabackup: error: cannot find dictionary record of table %s\n", table_name);
						goto next_node;
					}
					index = dict_table_get_first_index(table);
					n_index = UT_LIST_GET_LEN(table->indexes);
					if (n_index > 31) {
						fprintf(stderr,
"xtrabackup: error: sorry, cannot export over 31 indexes for now.\n");
						goto next_node;
					}

					/* init exp file */
					bzero(page, UNIV_PAGE_SIZE);
					mach_write_to_4(page    , 0x78706f72UL);
					mach_write_to_4(page + 4, 0x74696e66UL);/*"xportinf"*/
					mach_write_to_4(page + 8, n_index);
					strncpy((char*)page + 12, table_name, 500);

					printf(
"xtrabackup: export metadata of table '%s' to file `%s` (%lu indexes)\n",
						table_name, info_file_path, n_index);

					n_index = 1;
					while (index) {
						mach_write_to_8(page + n_index * 512, index->id);
						mach_write_to_4(page + n_index * 512 + 8,
								index->page);

						strncpy((char*)page + n_index * 512 + 12, index->name, 500);

						printf(
"xtrabackup:     name=%s, id.low=%lu, page=%lu\n",
							index->name,
							(ulint)(index->id & 0xFFFFFFFFUL),

							(ulint) index->page);

						index = dict_table_get_next_index(index);
						n_index++;
					}

					srv_normalize_path_for_win(info_file_path);
					info_file = os_file_create(
								0 /* dummy of innodb_file_data_key */,
								info_file_path, OS_FILE_OVERWRITE,
								OS_FILE_NORMAL, OS_DATA_FILE, &success);
					if (!success) {
						os_file_get_last_error(TRUE);
						goto next_node;
					}
					success = os_file_write(info_file_path, info_file, page,
								0, 0, UNIV_PAGE_SIZE);
					if (!success) {
						os_file_get_last_error(TRUE);
						goto next_node;
					}
					success = os_file_flush(info_file);
					if (!success) {
						os_file_get_last_error(TRUE);
						goto next_node;
					}
next_node:
					if (info_file != -1) {
						os_file_close(info_file);
						info_file = -1;
					}
					mutex_exit(&(dict_sys->mutex));
					mutex_enter(&(f_system->mutex));

					node = UT_LIST_GET_NEXT(chain, node);
				}

				space = UT_LIST_GET_NEXT(space_list, space);
			}
			mutex_exit(&(f_system->mutex));

			ut_free(buf);
		} else {
			printf("xtrabackup: export option is for file_per_table only, disabled.\n");
		}
	}

	/* print binlog position (again?) */
	printf("\n[notice (again)]\n"
		"  If you use binary log and don't use any hack of group commit,\n"
		"  the binary log position seems to be:\n");
// FIXME: 	trx_sys_print_mysql_binlog_offset();
	printf("\n");

	/* output to xtrabackup_binlog_pos_innodb */
        if (false) {
//FIXME	if (*trx_sys_mysql_bin_log_name != '\0') {
		FILE *fp;

		fp = fopen("xtrabackup_binlog_pos_innodb", "w");
		if (fp) {
			fprintf(fp, "%s\t%llu\n",
                                "none", 0ULL);
                                /* FIXME
				trx_sys_mysql_bin_log_name,
				trx_sys_mysql_bin_log_pos);*/
			fclose(fp);
		} else {
			printf("xtrabackup: failed to open 'xtrabackup_binlog_pos_innodb'\n");
		}
	}

	/* Check whether the log is applied enough or not. */
	if ((xtrabackup_incremental
	     && ut_dulint_cmp(srv_start_lsn, incremental_last_lsn) < 0)
	    ||(!xtrabackup_incremental
	       && ut_dulint_cmp(srv_start_lsn, metadata_last_lsn) < 0)) {
		printf( "xtrabackup: ########################################################\n"
			"xtrabackup: # !!WARNING!!                                          #\n"
			"xtrabackup: # The transaction log file should be wrong or corrupt. #\n"
			"xtrabackup: # The log was not applied to the intended LSN!         #\n"
			"xtrabackup: ########################################################\n");
		if (xtrabackup_incremental) {
			printf("xtrabackup: The intended lsn is %"PRIu64"\n",
				incremental_last_lsn);
		} else {
			printf("xtrabackup: The intended lsn is %"PRIu64"\n",
				metadata_last_lsn);
		}
	}

	if(innodb_end())
		goto error;

	sync_initialized = FALSE;
	os_sync_mutex = NULL;

	/* re-init necessary components */
	ut_mem_init();

	os_sync_init();
	sync_init();
	os_io_init_simple();

	if(xtrabackup_close_temp_log(TRUE))
		exit(EXIT_FAILURE);

	/* output to metadata file */
	{
		char	filename[FN_REFLEN];

		strcpy(metadata_type, "full-prepared");

		if(xtrabackup_incremental
		   && ut_dulint_cmp(metadata_to_lsn, incremental_to_lsn) < 0)
		{
			metadata_to_lsn = incremental_to_lsn;
			metadata_last_lsn = incremental_last_lsn;
		}

		sprintf(filename, "%s/%s", xtrabackup_target_dir, XTRABACKUP_METADATA_FILENAME);
		if (xtrabackup_write_metadata(filename))
			fprintf(stderr, "xtrabackup: error: xtrabackup_write_metadata(xtrabackup_target_dir)\n");

		if(xtrabackup_extra_lsndir) {
			sprintf(filename, "%s/%s", xtrabackup_extra_lsndir, XTRABACKUP_METADATA_FILENAME);
			if (xtrabackup_write_metadata(filename))
				fprintf(stderr, "xtrabackup: error: xtrabackup_write_metadata(xtrabackup_extra_lsndir)\n");
		}
	}

	if(!xtrabackup_create_ib_logfile)
		return;

	/* TODO: make more smart */

	printf("\n[notice]\nWe cannot call InnoDB second time during the process lifetime.\n");
	printf("Please re-execte to create ib_logfile*. Sorry.\n");
/*
	printf("Restart InnoDB to create ib_logfile*.\n");

	if(innodb_init_param())
		goto error;

	if(innodb_init())
		goto error;

	if(innodb_end())
		goto error;
*/

	return;

error:
	xtrabackup_close_temp_log(FALSE);

	exit(EXIT_FAILURE);
}

/* ================= main =================== */

int main(int argc, char **argv)
{
	po::options_description commandline_options(_("Options used only in command line"));
	commandline_options.add_options()
	  ("target-dir", po::value<std::string>(), _("destination directory"))
          ("backup", po::value<bool>(&xtrabackup_backup)->default_value(false)->zero_tokens(), _("take backup to target-dir"))
	("stats", po::value<bool>(&xtrabackup_stats)->default_value(false)->zero_tokens(), _("calc statistic of datadir (offline mysqld is recommended)"))
	("prepare", po::value<bool>(&xtrabackup_prepare)->default_value(false)->zero_tokens(), _("prepare a backup for starting mysql server on the backup."))
	("export", po::value<bool>(&xtrabackup_export)->default_value(false)->zero_tokens(), _("create files to import to another database when prepare."))
	("apply-log-only", po::value<bool>(&xtrabackup_apply_log_only)->default_value(false)->zero_tokens(), _("stop recovery process not to progress LSN after applying log when prepare."))
	("print-param", po::value<bool>(&xtrabackup_print_param)->default_value(false)->zero_tokens(), _("print parameter of mysqld needed for copyback."))
	("use-memory", po::value<uint64_t>(&xtrabackup_use_memory)->default_value(100*1024*1024), _("The value is used instead of buffer_pool_size"))
	("suspend-at-end", po::value<bool>(&xtrabackup_suspend_at_end)->default_value(false)->zero_tokens(), _("creates a file 'xtrabackup_suspended' and waits until the user deletes that file at the end of '--backup'"))
	("throttle", po::value<long>(&xtrabackup_throttle), _("limit count of IO operations (pairs of read&write) per second to IOS values (for '--backup')"))
	("log-stream", po::value<bool>(&xtrabackup_stream)->default_value(false)->zero_tokens(), _("outputs the contents of 'xtrabackup_logfile' to stdout only until the file 'xtrabackup_suspended' deleted (for '--backup')."))
	  ("extra-lsndir", po::value<std::string>(), _("(for --backup): save an extra copy of the xtrabackup_checkpoints file in this directory."))
	  ("incremental-lsn", po::value<std::string>(), _("(for --backup): copy only .ibd pages newer than specified LSN 'high:low'. ##ATTENTION##: checkpoint lsn must be used. anyone can detect your mistake. be carefully!"))
	  ("incremental-basedir", po::value<std::string>(), _("(for --backup): copy only .ibd pages newer than backup at specified directory."))
	  ("incremental-dir", po::value<std::string>(), _("(for --prepare): apply .delta files and logfile in the specified directory."))
	  ("tables", po::value<std::string>(), _("filtering by regexp for table names."))
	  ("tables-file", po::value<std::string>(), _("filtering by list of the exact database.table name in the file."))
	("create-ib-logfile", po::value<bool>(&xtrabackup_create_ib_logfile), _("** not work for now** creates ib_logfile* also after '--prepare'. ### If you want create ib_logfile*, only re-execute this command in same options. ###"))
          ("datadir,h", po::value<std::string>(), _("Path to the database root."))
	  ("tmpdir,t", po::value<std::string>(), _("Path for temporary files. Several paths may be specified, separated by a colon (:), in this case they are used in a round-robin fashion."))
	("parallel", po::value<uint32_t>(&parallel)->default_value(1), _("Number of threads to use for parallel datafiles transfer. Does not have any effect in the stream mode. The default value is 1."))
	("innodb-adaptive-hash-index", po::value<bool>(&innobase_adaptive_hash_index)->default_value(true), _("Enable InnoDB adaptive hash index (enabled by default).  Disable with --skip-innodb-adaptive-hash-index."))
	("innodb-additional-mem-pool-size", po::value<long>(&innobase_additional_mem_pool_size)->default_value(1*1024*1024), _("Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures."))
	("innodb-autoextend-increment", po::value<uint32_t>(&srv_auto_extend_increment)->default_value(8), _("Data file autoextend increment in megabytes"))
	("innodb-buffer-pool-size", po::value<uint64_t>(&innobase_buffer_pool_size)->default_value(8*1024*1024), _("The size of the memory buffer InnoDB uses to cache data and indexes of its tables."))
	("innodb-checksums", po::value<bool>(&innobase_use_checksums)->default_value(true), _("Enable InnoDB checksums validation (enabled by default). Disable with --skip-innodb-checksums."))
	  ("innodb-data-file-path", po::value<std::string>(), _("Path to individual files and their sizes."))
	  ("innodb-data-home-dir", po::value<std::string>(), _("The common part for InnoDB table spaces."))
	("innodb-doublewrite", po::value<bool>(&innobase_use_doublewrite)->default_value(true), _("Enable InnoDB doublewrite buffer (enabled by default). Disable with --skip-innodb-doublewrite."))
	("innodb-file-io-threads", po::value<long>(&innobase_file_io_threads)->default_value(4), _("Number of file I/O threads in InnoDB."))
	("innodb-file-per-table", po::value<bool>(&innobase_file_per_table), _("Stores each InnoDB table to an .ibd file in the database dir."))
	("innodb-flush-log-at-trx-commit", po::value<ulong>(&srv_flush_log_at_trx_commit)->default_value(1), _("Set to 0 (write and flush once per second), 1 (write and flush at each commit) or 2 (write at commit, flush once per second)."))
          ("innodb-flush-method", po::value<std::string>(), _("With which method to flush data."))
/* ####### Should we use this option? ####### */
         ("innodb-force-recovery", po::value<long>(&innobase_force_recovery)->default_value(0), _("Helps to save your data in case the disk image of the database becomes corrupt."))
	("innodb-lock-wait-timeout", po::value<long>(&innobase_lock_wait_timeout)->default_value(50), _("Timeout in seconds an InnoDB transaction may wait for a lock before being rolled back."))
	("innodb-log-buffer-size", po::value<long>(&innobase_log_buffer_size)->default_value(1024*1024), _("The size of the buffer which InnoDB uses to write log to the log files on disk."))
	("innodb-log-file-size", po::value<log_file_constraint>(&innobase_log_file_size)->default_value(20*1024*1024L), _("Size of each log file in a log group."))
	("innodb-log-files-in-group", po::value<long>(&innobase_log_files_in_group)->default_value(2), _("Number of log files in the log group. InnoDB writes to the files in a circular fashion. Value 3 is recommended here."))
	  ("innodb-log-group-home-dir", po::value<std::string>(), _("Path to InnoDB log files."))
	("innodb-max_dirty-pages-pct", po::value<ulong>(&srv_max_buf_pool_modified_pct)->default_value(90), _("Percentage of dirty pages allowed in bufferpool."))
	("innodb-open-files", po::value<long>(&innobase_open_files)->default_value(300), _("How many files at the maximum InnoDB keeps open at the same time."))
#ifdef XTRADB_BASED
	("innodb-page-size", po::value<uint32_t>(&innobase_page_size)->default_value(1 << 14), _("The universal page size of the database."))
	("innodb-log-block-size", po::value<uint32_t>(&innobase_log_block_size)->default_value(512), _("###EXPERIMENTAL###: The log block size of the transaction log file. Changing for created log file is not supported. Use on your own risk!"))
	("innodb-fast-checksum", po::value<bool>(&innobase_fast_checksum), _("Change the algorithm of checksum for the whole of datapage to 4-bytes word based."))
	("innodb-extra-undoslots", po::value<bool>(&innobase_extra_undoslots), _("Enable to use about 4000 undo slots instead of default 1024. Not recommended to use, Because it is not change back to disable, once it is used."))
	("innodb-doublewrite-file", po::value<char *>(&innobase_doublewrite_file), _("Path to special datafile for doublewrite buffer. (default is "": not used)"))
#endif
	;

	po::variables_map vm;
	// Disable allow_guessing, it is evil and broken
	int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;
	po::store(po::command_line_parser(argc, argv).options(commandline_options).style(style).run(), vm);
	po::notify(vm);

	if (vm.count("target-dir"))
	  xtrabackup_target_dir= vm["target-dir"].as<std::string>().c_str();

	if (vm.count("extra-lsndir"))
	  xtrabackup_extra_lsndir= vm["extra-lsndir"].as<std::string>().c_str();

	if (vm.count("incremental-lsn"))
	  xtrabackup_incremental= vm["incremental-lsn"].as<std::string>().c_str();

	if (vm.count("incremental-basedir"))
	  xtrabackup_incremental_basedir= vm["incremental-basedir"].as<std::string>().c_str();

	boost::scoped_ptr<char> xtrabackup_tables_autoptr(new char[(vm.count("tables")) ? vm["tables"].as<std::string>().length() + 1: 0]);
	if (vm.count("tables"))
	{
	  xtrabackup_tables= xtrabackup_tables_autoptr.get();
	  strcpy(xtrabackup_tables, vm["tables"].as<std::string>().c_str());
	}

	if (vm.count("tables-file"))
	  xtrabackup_tables_file= vm["tables-file"].as<std::string>().c_str();

	if (vm.count("tmpdir"))
	  opt_mysql_tmpdir= vm["tmpdir"].as<std::string>().c_str();

	if (vm.count("innodb-data-file-path"))
	  innobase_data_file_path= vm["innodb-data-file-path"].as<std::string>().c_str();

	boost::scoped_ptr<char> xtrabackup_incremental_dir_autoptr(new char[(vm.count("incremental-dir")) ? vm["incremental-dir"].as<std::string>().length() + 1: 0]);
	if (vm.count("incremental-dir"))
	{
	  xtrabackup_incremental_dir= xtrabackup_incremental_dir_autoptr.get();
	  strcpy(xtrabackup_incremental_dir, vm["incremental-dir"].as<std::string>().c_str());
	}

	boost::scoped_ptr<char> innobase_data_home_dir_autoptr(new char[(vm.count("innodb-data-home-dir")) ? vm["innodb-data-home-dir"].as<std::string>().length() + 1 : 0]);
	if (vm.count("innodb-data-home-dir"))
	{
	  innobase_data_home_dir= innobase_data_home_dir_autoptr.get();
	  strcpy(innobase_data_home_dir, vm["innodb-data-home-dir"].as<std::string>().c_str());
	}

	boost::scoped_ptr<char> innobase_flush_method_autoptr(new char[(vm.count("innodb-flush-method")) ? vm["innodb-flush-method"].as<std::string>().length() + 1 : 0]);
	if (vm.count("innodb-flush-method"))
	{
	  innobase_unix_file_flush_method= innobase_flush_method_autoptr.get();
	  strcpy(innobase_unix_file_flush_method, vm["innodb-flush-method"].as<std::string>().c_str());
	}

	boost::scoped_ptr<char> innobase_log_group_home_dir_autoptr(new char[(vm.count("innodb-log-group-home-dir")) ? vm["innodb-log-group-home-dir"].as<std::string>().length() + 1: 0]);

	if (vm.count("innodb-log-group-home-dir"))
	{
	  innobase_log_group_home_dir= innobase_log_group_home_dir_autoptr.get();
	  strcpy(innobase_log_group_home_dir, vm["innodb-log-group-home-dir"].as<std::string>().c_str());
	}

	xtrabackup_use_memory-= xtrabackup_use_memory % (1024*1024);
	if (xtrabackup_use_memory < (1024*1024)) {
		fprintf(stderr, "xtrabackup: use-memory out of range\n");
		exit(EXIT_FAILURE);
	}

	if (parallel < 1) {
		fprintf(stderr, "xtrabackup: parallel needs to be greater than 0\n");
		exit(EXIT_FAILURE);
	}

	innobase_additional_mem_pool_size-= innobase_additional_mem_pool_size % 1024;
	if (innobase_additional_mem_pool_size < (512*1024)) {
		fprintf(stderr, "xtrabackup: innodb-additional-mem-pool-size out of range\n");
		exit(EXIT_FAILURE);		
	}

	if ((srv_auto_extend_increment < 1) || (srv_auto_extend_increment > 8)) {
		fprintf(stderr, "xtrabackup: innodb-auto-extend-increment out of range\n");
		exit(EXIT_FAILURE);		
	}

	innobase_buffer_pool_size-= innobase_buffer_pool_size % (1024*1024);
	if (innobase_buffer_pool_size < (1024*1024)) {
		fprintf(stderr, "xtrabackup: innodb-buffer-pool-size out of range\n");
		exit(EXIT_FAILURE);
	}

	if ((innobase_file_io_threads < 4) || (innobase_file_io_threads > 64)) {
		fprintf(stderr, "xtrabackup: innodb-file-io-threads out of range\n");
		exit(EXIT_FAILURE);		
	}

	if (srv_flush_log_at_trx_commit > 2) {
		fprintf(stderr, "xtrabackup: innodb-flush-log-at-trx-commit out of range\n");
		exit(EXIT_FAILURE);		
	}

	if (innobase_force_recovery > 6) {
		fprintf(stderr, "xtrabackup: innodb-force-recovery out of range\n");
		exit(EXIT_FAILURE);		
	}

	if ((innobase_lock_wait_timeout < 1) || (innobase_lock_wait_timeout > (1024*1024*1024))) {
		fprintf(stderr, "xtrabackup: innodb-lock-wait-timeout out of range\n");
		exit(EXIT_FAILURE);		
	}

	innobase_log_buffer_size-= innobase_log_buffer_size % 1024;
	if (innobase_additional_mem_pool_size < (256*1024)) {
		fprintf(stderr, "xtrabackup: innodb-log-buffer-size out of range\n");
		exit(EXIT_FAILURE);		
	}

	if (innobase_additional_mem_pool_size < (1024*1024)) {
		fprintf(stderr, "xtrabackup: innodb-log-file-size out of range\n");
		exit(EXIT_FAILURE);		
	}

	if ((innobase_log_files_in_group < 2) || (innobase_log_files_in_group > (100))) {
		fprintf(stderr, "xtrabackup: innodb-log-files-in-group out of range\n");
		exit(EXIT_FAILURE);		
	}

	if (srv_max_buf_pool_modified_pct > 100) {
		fprintf(stderr, "xtrabackup: innodb-max-buf-pool-modified-pct out of range\n");
		exit(EXIT_FAILURE);		
	}

	if (innobase_open_files < 10) {
		fprintf(stderr, "xtrabackup: innodb-open-files out of range\n");
		exit(EXIT_FAILURE);		
	}

	if ((innobase_page_size < (1 << 12)) || (innobase_page_size > (1 << UNIV_PAGE_SIZE_SHIFT_MAX))) {
		fprintf(stderr, "xtrabackup: innodb-page-size out of range\n");
		exit(EXIT_FAILURE);		
	}

	if ((innobase_log_block_size < (512)) || (innobase_log_block_size > (1 << UNIV_PAGE_SIZE_SHIFT_MAX))) {
		fprintf(stderr, "xtrabackup: innodb-log-block-size out of range\n");
		exit(EXIT_FAILURE);		
	}

        if (vm.count("datadir"))
        {
          mysql_data_home_arg.assign(vm["datadir"].as<std::string>());
        }
        else
        {
          mysql_data_home_arg.assign(LOCALSTATEDIR);
        }

        mysql_data_home= (char*)malloc(mysql_data_home_arg.length());
        strcpy(mysql_data_home, mysql_data_home_arg.c_str());

	if ((!xtrabackup_prepare) && (strcmp(mysql_data_home, "./") == 0)) {
		if (!xtrabackup_print_param)
			usage();
		printf("\nxtrabackup: Error: Please set parameter 'datadir'\n");
		exit(EXIT_FAILURE);
	}

	if (xtrabackup_tables) {
		/* init regexp */
		char *p, *next;
		int i;
		char errbuf[100];

		tables_regex_num = 1;

		p = xtrabackup_tables;
		while ((p = strchr(p, ',')) != NULL) {
			p++;
			tables_regex_num++;
		}

		tables_regex = (regex_t*) ut_malloc(sizeof(regex_t) * tables_regex_num);

		p = xtrabackup_tables;
		for (i=0; i < tables_regex_num; i++) {
			next = strchr(p, ',');
			ut_a(next || i == tables_regex_num - 1);

			next++;
			if (i != tables_regex_num - 1)
				*(next - 1) = '\0';

			regerror(regcomp(&tables_regex[i],p,REG_EXTENDED),
					&tables_regex[i],errbuf,sizeof(errbuf));
			fprintf(stderr, "xtrabackup: tables regcomp(%s): %s\n",p,errbuf);

			if (i != tables_regex_num - 1)
				*(next - 1) = ',';
			p = next;
		}
	}

	if (xtrabackup_tables_file) {
		char name_buf[NAME_LEN*2+2];
		FILE *fp;

		if (xtrabackup_stream) {
			fprintf(stderr, "xtrabackup: Warning: --tables_file option doesn't affect with --stream.\n");
			xtrabackup_tables_file = NULL;
			goto skip_tables_file_register;
		}

		name_buf[NAME_LEN*2+1] = '\0';

		/* init tables_hash */
		tables_hash = hash_create(1000);

		/* read and store the filenames */
		fp = fopen(xtrabackup_tables_file,"r");
		if (!fp) {
			fprintf(stderr, "xtrabackup: cannot open %s\n", xtrabackup_tables_file);
			exit(EXIT_FAILURE);
		}
		for (;;) {
			xtrabackup_tables_t*	table;
			char*	p = name_buf;

			if ( fgets(name_buf, NAME_LEN*2+1, fp) == 0 ) {
				break;
			}

			while (*p != '\0') {
				if (*p == '.') {
					*p = '/';
				}
				p++;
			}
			p = strchr(name_buf, '\n');
			if (p)
			{
				*p = '\0';
			}

			table = (xtrabackup_tables_t*) malloc(sizeof(xtrabackup_tables_t) + strlen(name_buf) + 1);
			memset(table, '\0', sizeof(xtrabackup_tables_t) + strlen(name_buf) + 1);
			table->name = ((char*)table) + sizeof(xtrabackup_tables_t);
			strcpy(table->name, name_buf);

			HASH_INSERT(xtrabackup_tables_t, name_hash, tables_hash,
					ut_fold_string(table->name), table);

			printf("xtrabackup: table '%s' is registerd to the list.\n", table->name);
		}
	}
skip_tables_file_register:

#ifdef XTRADB_BASED
	/* temporary setting of enough size */
	srv_page_size_shift = UNIV_PAGE_SIZE_SHIFT_MAX;
	srv_page_size = UNIV_PAGE_SIZE_MAX;
	srv_log_block_size = 512;
#endif
	if (xtrabackup_backup && xtrabackup_incremental) {
		/* direct specification is only for --backup */
		/* and the lsn is prior to the other option */

		char* endchar;
		int error = 0;

		incremental_lsn = strtoll(xtrabackup_incremental, &endchar, 10);
		if (*endchar != '\0')
			error = 1;

		if (error) {
			fprintf(stderr, "xtrabackup: value '%s' may be wrong format for incremental option.\n",
				xtrabackup_incremental);
			exit(EXIT_FAILURE);
		}

		/* allocate buffer for incremental backup (4096 pages) */
		incremental_buffer_base = (byte*) malloc((UNIV_PAGE_SIZE_MAX / 4 + 1) *
						 UNIV_PAGE_SIZE_MAX);
		incremental_buffer = (byte*) ut_align(incremental_buffer_base,
					      UNIV_PAGE_SIZE_MAX);
	} else if (xtrabackup_backup && xtrabackup_incremental_basedir) {
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_incremental_basedir, XTRABACKUP_METADATA_FILENAME);

		if (xtrabackup_read_metadata(filename)) {
			fprintf(stderr,
				"xtrabackup: error: failed to read metadata from %s\n",
				filename);
			exit(EXIT_FAILURE);
		}

		incremental_lsn = metadata_to_lsn;
		xtrabackup_incremental = xtrabackup_incremental_basedir; //dummy

		/* allocate buffer for incremental backup (4096 pages) */
		incremental_buffer_base = (byte*) malloc((UNIV_PAGE_SIZE_MAX / 4 + 1) *
						 UNIV_PAGE_SIZE_MAX);
		incremental_buffer = (byte*) ut_align(incremental_buffer_base,
					      UNIV_PAGE_SIZE_MAX);
	} else if (xtrabackup_prepare && xtrabackup_incremental_dir) {
		char	filename[FN_REFLEN];

		sprintf(filename, "%s/%s", xtrabackup_incremental_dir, XTRABACKUP_METADATA_FILENAME);

		if (xtrabackup_read_metadata(filename)) {
			fprintf(stderr,
				"xtrabackup: error: failed to read metadata from %s\n",
				filename);
			exit(EXIT_FAILURE);
		}

		incremental_lsn = metadata_from_lsn;
		incremental_to_lsn = metadata_to_lsn;
		incremental_last_lsn = metadata_last_lsn;
		xtrabackup_incremental = xtrabackup_incremental_dir; //dummy

		/* allocate buffer for incremental backup (4096 pages) */
		incremental_buffer_base = (byte*) malloc((UNIV_PAGE_SIZE / 4 + 1) *
						 UNIV_PAGE_SIZE);
		incremental_buffer = (byte*) ut_align(incremental_buffer_base,
					      UNIV_PAGE_SIZE);
	} else {
          /* allocate buffer for applying incremental (for header page only) */
          incremental_buffer_base = (byte*) malloc((1 + 1) * UNIV_PAGE_SIZE_MAX);
          incremental_buffer = (byte*) ut_align(incremental_buffer_base,
                                                UNIV_PAGE_SIZE_MAX);

		xtrabackup_incremental = NULL;
	}

	/* --print-param */
	if (xtrabackup_print_param) {
		printf("# This MySQL options file was generated by XtraBackup.\n");
		printf("[mysqld]\n");
		printf("datadir = \"%s\"\n", mysql_data_home);
		printf("tmpdir = \"%s\"\n", opt_mysql_tmpdir);
		printf("innodb_data_home_dir = \"%s\"\n",
			innobase_data_home_dir ? innobase_data_home_dir : mysql_data_home);
		printf("innodb_data_file_path = \"%s\"\n",
			innobase_data_file_path ? innobase_data_file_path : "ibdata1:10M:autoextend");
		printf("innodb_log_group_home_dir = \"%s\"\n",
			innobase_log_group_home_dir ? innobase_log_group_home_dir : mysql_data_home);
		printf("innodb_log_files_in_group = %ld\n", innobase_log_files_in_group);
		printf("innodb_log_file_size = %"PRIu64"\n", (uint64_t)innobase_log_file_size);
		printf("innodb_flush_method = \"%s\"\n",
		       (innobase_unix_file_flush_method != NULL) ?
		       innobase_unix_file_flush_method : "");
		exit(EXIT_SUCCESS);
	}

	if (!xtrabackup_stream) {
		print_version();
		if (xtrabackup_incremental) {
			printf("incremental backup from %"PRIu64" is enabled.\n",
				incremental_lsn);
		}
	} else {
		if (xtrabackup_backup) {
			xtrabackup_suspend_at_end = TRUE;
			fprintf(stderr, "xtrabackup: suspend-at-end is enabled.\n");
		}
	}

	/* cannot execute both for now */
	{
		int num = 0;

		if (xtrabackup_backup) num++;
		if (xtrabackup_stats) num++;
		if (xtrabackup_prepare) num++;
		if (num != 1) { /* !XOR (for now) */
			usage();
			exit(EXIT_FAILURE);
		}
	}

	/* --backup */
	if (xtrabackup_backup)
		xtrabackup_backup_func();

	/* --stats */
	if (xtrabackup_stats)
		xtrabackup_stats_func();

	/* --prepare */
	if (xtrabackup_prepare)
		xtrabackup_prepare_func();

	free(incremental_buffer_base);

	if (xtrabackup_tables) {
		/* free regexp */
		int i;

		for (i = 0; i < tables_regex_num; i++) {
			regfree(&tables_regex[i]);
		}
		ut_free(tables_regex);
	}

	if (xtrabackup_tables_file) {
		ulint	i;

		/* free the hash elements */
		for (i = 0; i < hash_get_n_cells(tables_hash); i++) {
			xtrabackup_tables_t*	table;

			table = (xtrabackup_tables_t*)
                          HASH_GET_FIRST(tables_hash, i);

			while (table) {
				xtrabackup_tables_t*	prev_table = table;

				table =  (xtrabackup_tables_t*)
                                  HASH_GET_NEXT(name_hash, prev_table);

				HASH_DELETE(xtrabackup_tables_t, name_hash, tables_hash,
						ut_fold_string(prev_table->name), prev_table);
				free(prev_table);
			}
		}

		/* free tables_hash */
		hash_table_free(tables_hash);
	}

	exit(EXIT_SUCCESS);
}
