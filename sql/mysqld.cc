/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include <m_ctype.h>
#include <my_dir.h>
#include <my_bit.h>
#include "slave.h"
#include "rpl_mi.h"
#include "sql_repl.h"
#include "rpl_filter.h"
#include "repl_failsafe.h"
#include "stacktrace.h"
#include "mysqld_suffix.h"
#include "mysys_err.h"

#include "../storage/myisam/ha_myisam.h"

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifndef DEFAULT_SKIP_THREAD_PRIORITY
#define DEFAULT_SKIP_THREAD_PRIORITY 0
#endif

#include <thr_alarm.h>
#include <errmsg.h>

#define mysqld_charset &my_charset_latin1

#ifdef HAVE_purify
#define IF_PURIFY(A,B) (A)
#else
#define IF_PURIFY(A,B) (B)
#endif

#if SIZEOF_CHARP == 4
#define MAX_MEM_TABLE_SIZE ~(ulong) 0
#else
#define MAX_MEM_TABLE_SIZE ~(ulonglong) 0
#endif

/* We have HAVE_purify below as this speeds up the shutdown of MySQL */

#if defined(HAVE_DEC_3_2_THREADS) || defined(SIGNALS_DONT_BREAK_READ) || defined(HAVE_purify) && defined(__linux__)
#define HAVE_CLOSE_SERVER_SOCK 1
#endif

extern "C" {					// Because of SCO 3.2V4.2
#include <errno.h>
#include <sys/stat.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__				// Skip warnings in getopt.h
#endif
#include <my_getopt.h>
#ifdef HAVE_SYSENT_H
#include <sysent.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>				// For getpwent
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#include <my_net.h>

#include <sys/resource.h>

#ifdef HAVE_SELECT_H
#  include <select.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <sys/utsname.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#define SIGNAL_FMT "signal %d"
  

#if defined(__FreeBSD__) && defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#ifdef HAVE_FP_EXCEPT				// Fix type conflict
typedef fp_except fp_except_t;
#endif
#endif /* __FreeBSD__ && HAVE_IEEEFP_H */

#ifdef HAVE_FPU_CONTROL_H
#include <fpu_control.h>
#endif

#ifdef HAVE_SYS_FPU_H
/* for IRIX to use set_fpc_csr() */
#include <sys/fpu.h>
#endif

inline void setup_fpu()
{
#if defined(__FreeBSD__) && defined(HAVE_IEEEFP_H)
  /*
     We can't handle floating point exceptions with threads, so disable
     this on freebsd.
     Don't fall for overflow, underflow,divide-by-zero or loss of precision
  */
#if defined(__i386__)
  fpsetmask(~(FP_X_INV | FP_X_DNML | FP_X_OFL | FP_X_UFL | FP_X_DZ |
	      FP_X_IMP));
#else
  fpsetmask(~(FP_X_INV |             FP_X_OFL | FP_X_UFL | FP_X_DZ |
              FP_X_IMP));
#endif /* __i386__ */
#endif /* __FreeBSD__ && HAVE_IEEEFP_H */

  /*
    x86 (32-bit) requires FPU precision to be explicitly set to 64 bit for
    portable results of floating point operations
  */
#if defined(__i386__) && defined(HAVE_FPU_CONTROL_H) && defined(_FPU_DOUBLE)
  fpu_control_t cw;
  _FPU_GETCW(cw);
  cw= (cw & ~_FPU_EXTENDED) | _FPU_DOUBLE;
  _FPU_SETCW(cw);
#endif /* __i386__ && HAVE_FPU_CONTROL_H && _FPU_DOUBLE */
}

} /* cplusplus */

#define MYSQL_KILL_SIGNAL SIGTERM

#include <my_pthread.h>			// For thr_setconcurency()

#ifdef SOLARIS
extern "C" int gethostname(char *name, int namelen);
#endif

extern "C" sig_handler handle_segfault(int sig);

/* Constants */

const char *show_comp_option_name[]= {"YES", "NO", "DISABLED"};
/*
  WARNING: When adding new SQL modes don't forget to update the
           tables definitions that stores it's value.
           (ie: mysql.event, mysql.proc)
*/
static const char *optimizer_switch_names[]=
{
  "no_materialization", "no_semijoin",
  NullS
};

/* Corresponding defines are named OPTIMIZER_SWITCH_XXX */
static const unsigned int optimizer_switch_names_len[]=
{
  /*no_materialization*/          19,
  /*no_semijoin*/                 11
};

TYPELIB optimizer_switch_typelib= { array_elements(optimizer_switch_names)-1,"",
                                    optimizer_switch_names,
                                    (unsigned int *)optimizer_switch_names_len };

static const char *tc_heuristic_recover_names[]=
{
  "COMMIT", "ROLLBACK", NullS
};
static TYPELIB tc_heuristic_recover_typelib=
{
  array_elements(tc_heuristic_recover_names)-1,"",
  tc_heuristic_recover_names, NULL
};

const char *first_keyword= "first", *binary_keyword= "BINARY";
const char *my_localhost= "localhost";
#if SIZEOF_OFF_T > 4 && defined(BIG_TABLES)
#define GET_HA_ROWS GET_ULL
#else
#define GET_HA_ROWS GET_ULONG
#endif

/*
  Used with --help for detailed option
*/
static my_bool opt_help= 0, opt_verbose= 0;

arg_cmp_func Arg_comparator::comparator_matrix[5][2] =
{{&Arg_comparator::compare_string,     &Arg_comparator::compare_e_string},
 {&Arg_comparator::compare_real,       &Arg_comparator::compare_e_real},
 {&Arg_comparator::compare_int_signed, &Arg_comparator::compare_e_int},
 {&Arg_comparator::compare_row,        &Arg_comparator::compare_e_row},
 {&Arg_comparator::compare_decimal,    &Arg_comparator::compare_e_decimal}};

const char *log_output_names[] = { "NONE", "FILE", "TABLE", NullS};
static const unsigned int log_output_names_len[]= { 4, 4, 5, 0 };
TYPELIB log_output_typelib= {array_elements(log_output_names)-1,"",
                             log_output_names, 
                             (unsigned int *) log_output_names_len};

/* static variables */

/* the default log output is log tables */
static bool lower_case_table_names_used= 0;
static bool volatile select_thread_in_use, signal_thread_in_use;
static bool volatile ready_to_exit;
static my_bool opt_debugging= 0, opt_external_locking= 0, opt_console= 0;
static my_bool opt_short_log_format= 0;
static uint kill_cached_threads, wake_thread;
static ulong killed_threads, thread_created;
static ulong max_used_connections;
static volatile ulong cached_thread_count= 0;
static char *mysqld_user, *mysqld_chroot, *log_error_file_ptr;
static char *opt_init_slave, *language_ptr, *opt_init_connect;
static char *default_character_set_name;
static char *character_set_filesystem_name;
static char *lc_time_names_name;
static char *my_bind_addr_str;
static char *default_collation_name; 
static char *default_storage_engine_str;
static char compiled_default_collation_name[]= MYSQL_DEFAULT_COLLATION_NAME;
static I_List<THD> thread_cache;
static double long_query_time;

static pthread_cond_t COND_thread_cache, COND_flush_thread_cache;

/* Global variables */

bool opt_update_log, opt_bin_log;
my_bool opt_log, opt_slow_log;
ulong log_output_options;
my_bool opt_log_queries_not_using_indexes= 0;
bool opt_error_log= IF_WIN(1,0);
bool opt_disable_networking=0, opt_skip_show_db=0;
my_bool opt_character_set_client_handshake= 1;
bool server_id_supplied = 0;
bool opt_endinfo, using_udf_functions;
my_bool locked_in_memory;
bool opt_using_transactions, using_update_log;
bool volatile abort_loop;
bool volatile shutdown_in_progress;
my_bool opt_skip_slave_start = 0; ///< If set, slave is not autostarted
my_bool opt_reckless_slave = 0;
my_bool opt_enable_named_pipe= 0;
my_bool opt_local_infile, opt_slave_compressed_protocol;
my_bool opt_safe_user_create = 0;
my_bool opt_show_slave_auth_info, opt_sql_bin_update = 0;
my_bool opt_log_slave_updates= 0;

/*
  Legacy global handlerton. These will be removed (please do not add more).
*/
handlerton *heap_hton;
handlerton *myisam_hton;

my_bool opt_readonly, use_temp_pool, relay_log_purge;
my_bool opt_sync_frm, opt_allow_suspicious_udfs;
my_bool opt_secure_auth= 0;
char* opt_secure_file_priv= 0;
my_bool opt_log_slow_admin_statements= 0;
my_bool opt_log_slow_slave_statements= 0;
my_bool lower_case_file_system= 0;
my_bool opt_old_style_user_limits= 0, trust_function_creators= 0;
/*
  True if there is at least one per-hour limit for some user, so we should
  check them before each query (and possibly reset counters when hour is
  changed). False otherwise.
*/
volatile bool mqh_used = 0;
my_bool opt_noacl;

ulong opt_binlog_rows_event_max_size;
const char *binlog_format_names[]= {"MIXED", "STATEMENT", "ROW", NullS};
TYPELIB binlog_format_typelib=
  { array_elements(binlog_format_names) - 1, "",
    binlog_format_names, NULL };
ulong opt_binlog_format_id= (ulong) BINLOG_FORMAT_UNSPEC;
const char *opt_binlog_format= binlog_format_names[opt_binlog_format_id];
#ifdef HAVE_INITGROUPS
static bool calling_initgroups= FALSE; /**< Used in SIGSEGV handler. */
#endif
uint mysqld_port, test_flags, select_errors, dropping_tables, ha_open_options;
uint mysqld_port_timeout;
uint delay_key_write_options, protocol_version;
uint lower_case_table_names;
uint tc_heuristic_recover= 0;
uint volatile thread_count, thread_running;
ulonglong thd_startup_options;
ulong back_log, connect_timeout, server_id;
ulong table_cache_size, table_def_size;
ulong what_to_log;
ulong query_buff_size, slow_launch_time, slave_open_temp_tables;
ulong open_files_limit, max_binlog_size, max_relay_log_size;
ulong slave_net_timeout, slave_trans_retries;
my_bool slave_allow_batching;
ulong slave_exec_mode_options;
const char *slave_exec_mode_str= "STRICT";
ulong thread_cache_size=0, thread_pool_size= 0;
ulong binlog_cache_size=0, max_binlog_cache_size=0;
ulong refresh_version;  /* Increments on each reload */
query_id_t global_query_id;
ulong aborted_threads, aborted_connects;
ulong flush_time;
ulong specialflag=0;
ulong binlog_cache_use= 0, binlog_cache_disk_use= 0;
ulong max_connections, max_connect_errors;
uint  max_user_connections= 0;
ulong thread_id=1L,current_pid;
ulong slow_launch_threads = 0, sync_binlog_period;
ulong expire_logs_days = 0;
ulong rpl_recovery_rank=0;
const char *log_output_str= "FILE";

const double log_10[] = {
  1e000, 1e001, 1e002, 1e003, 1e004, 1e005, 1e006, 1e007, 1e008, 1e009,
  1e010, 1e011, 1e012, 1e013, 1e014, 1e015, 1e016, 1e017, 1e018, 1e019,
  1e020, 1e021, 1e022, 1e023, 1e024, 1e025, 1e026, 1e027, 1e028, 1e029,
  1e030, 1e031, 1e032, 1e033, 1e034, 1e035, 1e036, 1e037, 1e038, 1e039,
  1e040, 1e041, 1e042, 1e043, 1e044, 1e045, 1e046, 1e047, 1e048, 1e049,
  1e050, 1e051, 1e052, 1e053, 1e054, 1e055, 1e056, 1e057, 1e058, 1e059,
  1e060, 1e061, 1e062, 1e063, 1e064, 1e065, 1e066, 1e067, 1e068, 1e069,
  1e070, 1e071, 1e072, 1e073, 1e074, 1e075, 1e076, 1e077, 1e078, 1e079,
  1e080, 1e081, 1e082, 1e083, 1e084, 1e085, 1e086, 1e087, 1e088, 1e089,
  1e090, 1e091, 1e092, 1e093, 1e094, 1e095, 1e096, 1e097, 1e098, 1e099,
  1e100, 1e101, 1e102, 1e103, 1e104, 1e105, 1e106, 1e107, 1e108, 1e109,
  1e110, 1e111, 1e112, 1e113, 1e114, 1e115, 1e116, 1e117, 1e118, 1e119,
  1e120, 1e121, 1e122, 1e123, 1e124, 1e125, 1e126, 1e127, 1e128, 1e129,
  1e130, 1e131, 1e132, 1e133, 1e134, 1e135, 1e136, 1e137, 1e138, 1e139,
  1e140, 1e141, 1e142, 1e143, 1e144, 1e145, 1e146, 1e147, 1e148, 1e149,
  1e150, 1e151, 1e152, 1e153, 1e154, 1e155, 1e156, 1e157, 1e158, 1e159,
  1e160, 1e161, 1e162, 1e163, 1e164, 1e165, 1e166, 1e167, 1e168, 1e169,
  1e170, 1e171, 1e172, 1e173, 1e174, 1e175, 1e176, 1e177, 1e178, 1e179,
  1e180, 1e181, 1e182, 1e183, 1e184, 1e185, 1e186, 1e187, 1e188, 1e189,
  1e190, 1e191, 1e192, 1e193, 1e194, 1e195, 1e196, 1e197, 1e198, 1e199,
  1e200, 1e201, 1e202, 1e203, 1e204, 1e205, 1e206, 1e207, 1e208, 1e209,
  1e210, 1e211, 1e212, 1e213, 1e214, 1e215, 1e216, 1e217, 1e218, 1e219,
  1e220, 1e221, 1e222, 1e223, 1e224, 1e225, 1e226, 1e227, 1e228, 1e229,
  1e230, 1e231, 1e232, 1e233, 1e234, 1e235, 1e236, 1e237, 1e238, 1e239,
  1e240, 1e241, 1e242, 1e243, 1e244, 1e245, 1e246, 1e247, 1e248, 1e249,
  1e250, 1e251, 1e252, 1e253, 1e254, 1e255, 1e256, 1e257, 1e258, 1e259,
  1e260, 1e261, 1e262, 1e263, 1e264, 1e265, 1e266, 1e267, 1e268, 1e269,
  1e270, 1e271, 1e272, 1e273, 1e274, 1e275, 1e276, 1e277, 1e278, 1e279,
  1e280, 1e281, 1e282, 1e283, 1e284, 1e285, 1e286, 1e287, 1e288, 1e289,
  1e290, 1e291, 1e292, 1e293, 1e294, 1e295, 1e296, 1e297, 1e298, 1e299,
  1e300, 1e301, 1e302, 1e303, 1e304, 1e305, 1e306, 1e307, 1e308
};

time_t server_start_time, flush_status_time;

char mysql_home[FN_REFLEN], pidfile_name[FN_REFLEN], system_time_zone[30];
char *default_tz_name;
char log_error_file[FN_REFLEN], glob_hostname[FN_REFLEN];
char mysql_real_data_home[FN_REFLEN],
     language[FN_REFLEN], reg_ext[FN_EXTLEN], mysql_charsets_dir[FN_REFLEN],
     *opt_init_file, *opt_tc_log_file;
char mysql_unpacked_real_data_home[FN_REFLEN];
uint reg_ext_length;
const key_map key_map_empty(0);
key_map key_map_full(0);                        // Will be initialized later

const char *opt_date_time_formats[3];

uint mysql_data_home_len;
char mysql_data_home_buff[2], *mysql_data_home=mysql_real_data_home;
char server_version[SERVER_VERSION_LENGTH];
char *opt_mysql_tmpdir;
const char **errmesg;			/**< Error messages */
const char *myisam_recover_options_str="OFF";
const char *myisam_stats_method_str="nulls_unequal";

/** name of reference on left espression in rewritten IN subquery */
const char *in_left_expr_name= "<left expr>";
/** name of additional condition */
const char *in_additional_cond= "<IN COND>";
const char *in_having_cond= "<IN HAVING>";

my_decimal decimal_zero;
/* classes for comparation parsing/processing */
Eq_creator eq_creator;
Ne_creator ne_creator;
Gt_creator gt_creator;
Lt_creator lt_creator;
Ge_creator ge_creator;
Le_creator le_creator;

FILE *bootstrap_file;
int bootstrap_error;
FILE *stderror_file=0;

I_List<THD> threads;
I_List<NAMED_LIST> key_caches;
Rpl_filter* rpl_filter;
Rpl_filter* binlog_filter;

struct system_variables global_system_variables;
struct system_variables max_system_variables;
struct system_status_var global_status_var;

MY_TMPDIR mysql_tmpdir_list;
MY_BITMAP temp_pool;

CHARSET_INFO *system_charset_info, *files_charset_info ;
CHARSET_INFO *national_charset_info, *table_alias_charset;
CHARSET_INFO *character_set_filesystem;

MY_LOCALE *my_default_lc_time_names;

SHOW_COMP_OPTION have_symlink, have_dlopen;
SHOW_COMP_OPTION have_crypt, have_compress;

/* Thread specific variables */

pthread_key(MEM_ROOT**,THR_MALLOC);
pthread_key(THD*, THR_THD);
pthread_mutex_t LOCK_mysql_create_db, LOCK_open, LOCK_thread_count,
		LOCK_mapped_file, LOCK_status, LOCK_global_read_lock,
		LOCK_error_log, LOCK_uuid_generator,
		LOCK_crypt,
	        LOCK_global_system_variables,
		LOCK_user_conn, LOCK_slave_list, LOCK_active_mi,
                LOCK_connection_count;

rw_lock_t	LOCK_sys_init_connect, LOCK_sys_init_slave;
rw_lock_t	LOCK_system_variables_hash;
pthread_cond_t COND_refresh, COND_thread_count, COND_global_read_lock;
pthread_t signal_thread;
pthread_attr_t connection_attrib;
pthread_cond_t  COND_server_started;

/* replication parameters, if master_host is not NULL, we are a slave */
uint report_port= MYSQL_PORT;
ulong master_retry_count=0;
char *master_info_file;
char *relay_log_info_file, *report_user, *report_password, *report_host;
char *opt_relay_logname = 0, *opt_relaylog_index_name=0;
char *opt_logname, *opt_slow_logname;

/* Static variables */

static bool kill_in_progress, segfaulted;
#ifdef HAVE_STACK_TRACE_ON_SEGV
static my_bool opt_do_pstack;
#endif /* HAVE_STACK_TRACE_ON_SEGV */
static my_bool opt_bootstrap, opt_myisam_log;
static int cleanup_done;
static ulong opt_specialflag, opt_myisam_block_size;
static char *opt_update_logname, *opt_binlog_index_name;
static char *opt_tc_heuristic_recover;
static char *mysql_home_ptr, *pidfile_name_ptr;
static int defaults_argc;
static char **defaults_argv;
static char *opt_bin_logname;

static my_socket ip_sock;
struct rand_struct sql_rand; ///< used by sql_class.cc:THD::THD()

struct passwd *user_info;
static pthread_t select_thread;
static uint thr_kill_signal;

/* OS specific variables */

bool mysqld_embedded=0;

#ifndef DBUG_OFF
static const char* default_dbug_option;
#endif

scheduler_functions thread_scheduler;

/**
  Number of currently active user connections. The variable is protected by
  LOCK_connection_count.
*/
uint connection_count= 0;

/* Function declarations */

pthread_handler_t signal_hand(void *arg);
static void mysql_init_variables(void);
static void get_options(int *argc,char **argv);
extern "C" my_bool mysqld_get_one_option(int, const struct my_option *, char *);
static void set_server_version(void);
static int init_thread_environment();
static char *get_relative_path(const char *path);
static void fix_paths(void);
void handle_connections_sockets();
pthread_handler_t kill_server_thread(void *arg);
static void bootstrap(FILE *file);
static bool read_init_file(char *file_name);
pthread_handler_t handle_slave(void *arg);
static ulong find_bit_type(const char *x, TYPELIB *bit_lib);
static ulong find_bit_type_or_exit(const char *x, TYPELIB *bit_lib,
                                   const char *option);
static void clean_up(bool print_message);
static int test_if_case_insensitive(const char *dir_name);

static void usage(void);
static void start_signal_handler(void);
static void close_server_sock();
static void clean_up_mutexes(void);
static void wait_for_signal_thread_to_end(void);
static void create_pid_file();
static void mysqld_exit(int exit_code) __attribute__((noreturn));

/****************************************************************************
** Code to end mysqld
****************************************************************************/

static void close_connections(void)
{
#ifdef EXTRA_DEBUG
  int count=0;
#endif
  DBUG_ENTER("close_connections");

  /* Clear thread cache */
  kill_cached_threads++;
  flush_thread_cache();

  /* kill flush thread */
  (void) pthread_mutex_lock(&LOCK_manager);
  if (manager_thread_in_use)
  {
    DBUG_PRINT("quit", ("killing manager thread: 0x%lx",
                        (ulong)manager_thread));
   (void) pthread_cond_signal(&COND_manager);
  }
  (void) pthread_mutex_unlock(&LOCK_manager);

  /* kill connection thread */
  DBUG_PRINT("quit", ("waiting for select thread: 0x%lx",
                      (ulong) select_thread));
  (void) pthread_mutex_lock(&LOCK_thread_count);

  while (select_thread_in_use)
  {
    struct timespec abstime;
    int error;
    DBUG_PRINT("info",("Waiting for select thread"));

#ifndef DONT_USE_THR_ALARM
    if (pthread_kill(select_thread, thr_client_alarm))
      break;					// allready dead
#endif
    set_timespec(abstime, 2);
    for (uint tmp=0 ; tmp < 10 && select_thread_in_use; tmp++)
    {
      error=pthread_cond_timedwait(&COND_thread_count,&LOCK_thread_count,
				   &abstime);
      if (error != EINTR)
	break;
    }
#ifdef EXTRA_DEBUG
    if (error != 0 && !count++)
      sql_print_error("Got error %d from pthread_cond_timedwait",error);
#endif
    close_server_sock();
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);


  /* Abort listening to new connections */
  DBUG_PRINT("quit",("Closing sockets"));
  if (!opt_disable_networking )
  {
    if (ip_sock != INVALID_SOCKET)
    {
      (void) shutdown(ip_sock, SHUT_RDWR);
      (void) closesocket(ip_sock);
      ip_sock= INVALID_SOCKET;
    }
  }
  end_thr_alarm(0);			 // Abort old alarms.

  /*
    First signal all threads that it's time to die
    This will give the threads some time to gracefully abort their
    statements and inform their clients that the server is about to die.
  */

  THD *tmp;
  (void) pthread_mutex_lock(&LOCK_thread_count); // For unlink from list

  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    DBUG_PRINT("quit",("Informing thread %ld that it's time to die",
		       tmp->thread_id));
    /* We skip slave threads & scheduler on this first loop through. */
    if (tmp->slave_thread)
      continue;

    tmp->killed= THD::KILL_CONNECTION;
    thread_scheduler.post_kill_notification(tmp);
    if (tmp->mysys_var)
    {
      tmp->mysys_var->abort=1;
      pthread_mutex_lock(&tmp->mysys_var->mutex);
      if (tmp->mysys_var->current_cond)
      {
	pthread_mutex_lock(tmp->mysys_var->current_mutex);
	pthread_cond_broadcast(tmp->mysys_var->current_cond);
	pthread_mutex_unlock(tmp->mysys_var->current_mutex);
      }
      pthread_mutex_unlock(&tmp->mysys_var->mutex);
    }
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count); // For unlink from list

  end_slave();

  if (thread_count)
    sleep(2);					// Give threads time to die

  /*
    Force remaining threads to die by closing the connection to the client
    This will ensure that threads that are waiting for a command from the
    client on a blocking read call are aborted.
  */

  for (;;)
  {
    DBUG_PRINT("quit",("Locking LOCK_thread_count"));
    (void) pthread_mutex_lock(&LOCK_thread_count); // For unlink from list
    if (!(tmp=threads.get()))
    {
      DBUG_PRINT("quit",("Unlocking LOCK_thread_count"));
      (void) pthread_mutex_unlock(&LOCK_thread_count);
      break;
    }
    if (tmp->vio_ok())
    {
      if (global_system_variables.log_warnings)
        sql_print_warning(ER(ER_FORCING_CLOSE),my_progname,
                          tmp->thread_id,
                          (tmp->main_security_ctx.user ?
                           tmp->main_security_ctx.user : ""));
      close_connection(tmp,0,0);
    }
    DBUG_PRINT("quit",("Unlocking LOCK_thread_count"));
    (void) pthread_mutex_unlock(&LOCK_thread_count);
  }
  /* All threads has now been aborted */
  DBUG_PRINT("quit",("Waiting for threads to die (count=%u)",thread_count));
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  DBUG_PRINT("quit",("close_connections thread"));
  DBUG_VOID_RETURN;
}


static void close_server_sock()
{
#ifdef HAVE_CLOSE_SERVER_SOCK
  DBUG_ENTER("close_server_sock");
  my_socket tmp_sock;
  tmp_sock=ip_sock;
  if (tmp_sock != INVALID_SOCKET)
  {
    ip_sock=INVALID_SOCKET;
    DBUG_PRINT("info",("calling shutdown on TCP/IP socket"));
    VOID(shutdown(tmp_sock, SHUT_RDWR));
  }
  DBUG_VOID_RETURN;
#endif
}


void kill_mysql(void)
{
  DBUG_ENTER("kill_mysql");

#if defined(SIGNALS_DONT_BREAK_READ)
  abort_loop=1;					// Break connection loops
  close_server_sock();				// Force accept to wake up
#endif

#if defined(HAVE_PTHREAD_KILL)
  if (pthread_kill(signal_thread, MYSQL_KILL_SIGNAL))
  {
    DBUG_PRINT("error",("Got error %d from pthread_kill",errno)); /* purecov: inspected */
  }
#elif !defined(SIGNALS_DONT_BREAK_READ)
  kill(current_pid, MYSQL_KILL_SIGNAL);
#endif
  DBUG_PRINT("quit",("After pthread_kill"));
  shutdown_in_progress=1;			// Safety if kill didn't work
#ifdef SIGNALS_DONT_BREAK_READ
  if (!kill_in_progress)
  {
    pthread_t tmp;
    abort_loop=1;
    if (pthread_create(&tmp,&connection_attrib, kill_server_thread,
			   (void*) 0))
      sql_print_error("Can't create thread to kill server");
  }
#endif
  DBUG_VOID_RETURN;
}

/**
  Force server down. Kill all connections and threads and exit.

  @param  sig_ptr       Signal number that caused kill_server to be called.

  @note
    A signal number of 0 mean that the function was not called
    from a signal handler and there is thus no signal to block
    or stop, we just want to kill the server.
*/

static void *kill_server(void *sig_ptr)
#define RETURN_FROM_KILL_SERVER DBUG_RETURN(0)
{
  DBUG_ENTER("kill_server");
  int sig=(int) (long) sig_ptr;			// This is passed a int
  // if there is a signal during the kill in progress, ignore the other
  if (kill_in_progress)				// Safety
    RETURN_FROM_KILL_SERVER;
  kill_in_progress=TRUE;
  abort_loop=1;					// This should be set
  if (sig != 0) // 0 is not a valid signal number
    my_sigset(sig, SIG_IGN);                    /* purify inspected */
  if (sig == MYSQL_KILL_SIGNAL || sig == 0)
    sql_print_information(ER(ER_NORMAL_SHUTDOWN),my_progname);
  else
    sql_print_error(ER(ER_GOT_SIGNAL),my_progname,sig); /* purecov: inspected */
  
  close_connections();
  if (sig != MYSQL_KILL_SIGNAL &&
      sig != 0)
    unireg_abort(1);				/* purecov: inspected */
  else
    unireg_end();

  /* purecov: begin deadcode */

  my_thread_end();
  pthread_exit(0);
  /* purecov: end */

  RETURN_FROM_KILL_SERVER;
}


#if defined(USE_ONE_SIGNAL_HAND)
pthread_handler_t kill_server_thread(void *arg __attribute__((unused)))
{
  my_thread_init();				// Initialize new thread
  kill_server(0);
  /* purecov: begin deadcode */
  my_thread_end();
  pthread_exit(0);
  return 0;
  /* purecov: end */
}
#endif


extern "C" sig_handler print_signal_warning(int sig)
{
  if (global_system_variables.log_warnings)
    sql_print_warning("Got signal %d from thread %ld", sig,my_thread_id());
#ifdef DONT_REMEMBER_SIGNAL
  my_sigset(sig,print_signal_warning);		/* int. thread system calls */
#endif
  if (sig == SIGALRM)
    alarm(2);					/* reschedule alarm */
}

/**
  cleanup all memory and end program nicely.

    If SIGNALS_DONT_BREAK_READ is defined, this function is called
    by the main thread. To get MySQL to shut down nicely in this case
    (Mac OS X) we have to call exit() instead if pthread_exit().

  @note
    This function never returns.
*/
void unireg_end(void)
{
  clean_up(1);
  my_thread_end();
#if defined(SIGNALS_DONT_BREAK_READ)
  exit(0);
#else
  pthread_exit(0);				// Exit is in main thread
#endif
}


extern "C" void unireg_abort(int exit_code)
{
  DBUG_ENTER("unireg_abort");

  if (exit_code)
    sql_print_error("Aborting\n");
  else if (opt_help)
    usage();
  clean_up(!opt_help && (exit_code || !opt_bootstrap)); /* purecov: inspected */
  DBUG_PRINT("quit",("done with cleanup in unireg_abort"));
  mysqld_exit(exit_code);
}


static void mysqld_exit(int exit_code)
{
  wait_for_signal_thread_to_end();
  clean_up_mutexes();
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  exit(exit_code); /* purecov: inspected */
}


void clean_up(bool print_message)
{
  DBUG_PRINT("exit",("clean_up"));
  if (cleanup_done++)
    return; /* purecov: inspected */

  /*
    make sure that handlers finish up
    what they have that is dependent on the binlog
  */
  ha_binlog_end(current_thd);

  logger.cleanup_base();

  mysql_bin_log.cleanup();

  if (use_slave_mask)
    bitmap_free(&slave_error_mask);
  my_database_names_free();
  table_cache_free();
  table_def_free();
  lex_free();				/* Free some memory */
  item_create_cleanup();
  set_var_free();
  free_charsets();
  udf_free();
  plugin_shutdown();
  ha_end();
  if (tc_log)
    tc_log->close();
  xid_cache_free();
  delete_elements(&key_caches, (void (*)(const char*, uchar*)) free_key_cache);
  multi_keycache_free();
  free_status_vars();
  end_thr_alarm(1);			/* Free allocated memory */
  my_free_open_file_info();
  my_free((char*) global_system_variables.date_format,
	  MYF(MY_ALLOW_ZERO_PTR));
  my_free((char*) global_system_variables.time_format,
	  MYF(MY_ALLOW_ZERO_PTR));
  my_free((char*) global_system_variables.datetime_format,
	  MYF(MY_ALLOW_ZERO_PTR));
  if (defaults_argv)
    free_defaults(defaults_argv);
  my_free(sys_init_connect.value, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sys_init_slave.value, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sys_var_general_log_path.value, MYF(MY_ALLOW_ZERO_PTR));
  my_free(sys_var_slow_log_path.value, MYF(MY_ALLOW_ZERO_PTR));
  free_tmpdir(&mysql_tmpdir_list);
  my_free(slave_load_tmpdir,MYF(MY_ALLOW_ZERO_PTR));
  x_free(opt_bin_logname);
  x_free(opt_relay_logname);
  x_free(opt_secure_file_priv);
  bitmap_free(&temp_pool);
  end_slave_list();
  delete binlog_filter;
  delete rpl_filter;
  vio_end();

  if (!opt_bootstrap)
    (void) my_delete(pidfile_name,MYF(0));	// This may not always exist
  if (print_message && errmesg && server_start_time)
    sql_print_information(ER(ER_SHUTDOWN_COMPLETE),my_progname);
  thread_scheduler.end();
  finish_client_errs();
  my_free((uchar*) my_error_unregister(ER_ERROR_FIRST, ER_ERROR_LAST),
          MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
  DBUG_PRINT("quit", ("Error messages freed"));
  /* Tell main we are ready */
  logger.cleanup_end();
  (void) pthread_mutex_lock(&LOCK_thread_count);
  DBUG_PRINT("quit", ("got thread count lock"));
  ready_to_exit=1;
  /* do the broadcast inside the lock to ensure that my_end() is not called */
  (void) pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  /*
    The following lines may never be executed as the main thread may have
    killed us
  */
  DBUG_PRINT("quit", ("done with cleanup"));
} /* clean_up */


/**
  This is mainly needed when running with purify, but it's still nice to
  know that all child threads have died when mysqld exits.
*/
static void wait_for_signal_thread_to_end()
{
  uint i;
  /*
    Wait up to 10 seconds for signal thread to die. We use this mainly to
    avoid getting warnings that my_thread_end has not been called
  */
  for (i= 0 ; i < 100 && signal_thread_in_use; i++)
  {
    if (pthread_kill(signal_thread, MYSQL_KILL_SIGNAL) != ESRCH)
      break;
    my_sleep(100);				// Give it time to die
  }
}


static void clean_up_mutexes()
{
  (void) pthread_mutex_destroy(&LOCK_mysql_create_db);
  (void) pthread_mutex_destroy(&LOCK_lock_db);
  (void) pthread_mutex_destroy(&LOCK_open);
  (void) pthread_mutex_destroy(&LOCK_thread_count);
  (void) pthread_mutex_destroy(&LOCK_mapped_file);
  (void) pthread_mutex_destroy(&LOCK_status);
  (void) pthread_mutex_destroy(&LOCK_error_log);
  (void) pthread_mutex_destroy(&LOCK_manager);
  (void) pthread_mutex_destroy(&LOCK_crypt);
  (void) pthread_mutex_destroy(&LOCK_user_conn);
  (void) pthread_mutex_destroy(&LOCK_connection_count);
  (void) pthread_mutex_destroy(&LOCK_rpl_status);
  (void) pthread_cond_destroy(&COND_rpl_status);
  (void) pthread_mutex_destroy(&LOCK_active_mi);
  (void) rwlock_destroy(&LOCK_sys_init_connect);
  (void) rwlock_destroy(&LOCK_sys_init_slave);
  (void) pthread_mutex_destroy(&LOCK_global_system_variables);
  (void) rwlock_destroy(&LOCK_system_variables_hash);
  (void) pthread_mutex_destroy(&LOCK_global_read_lock);
  (void) pthread_mutex_destroy(&LOCK_uuid_generator);
  (void) pthread_cond_destroy(&COND_thread_count);
  (void) pthread_cond_destroy(&COND_refresh);
  (void) pthread_cond_destroy(&COND_global_read_lock);
  (void) pthread_cond_destroy(&COND_thread_cache);
  (void) pthread_cond_destroy(&COND_flush_thread_cache);
  (void) pthread_cond_destroy(&COND_manager);
}


/****************************************************************************
** Init IP and UNIX socket
****************************************************************************/

static void set_ports()
{
  char	*env;
  if (!mysqld_port && !opt_disable_networking)
  {					// Get port if not from commandline
    mysqld_port= MYSQL_PORT;

    /*
      if builder specifically requested a default port, use that
      (even if it coincides with our factory default).
      only if they didn't do we check /etc/services (and, failing
      on that, fall back to the factory default of 3306).
      either default can be overridden by the environment variable
      MYSQL_TCP_PORT, which in turn can be overridden with command
      line options.
    */

#if MYSQL_PORT_DEFAULT == 0
    struct  servent *serv_ptr;
    if ((serv_ptr= getservbyname("mysql", "tcp")))
      mysqld_port= ntohs((u_short) serv_ptr->s_port); /* purecov: inspected */
#endif
    if ((env = getenv("MYSQL_TCP_PORT")))
      mysqld_port= (uint) atoi(env);		/* purecov: inspected */
  }
}

/* Change to run as another user if started with --user */

static struct passwd *check_user(const char *user)
{
  struct passwd *tmp_user_info;
  uid_t user_id= geteuid();

  // Don't bother if we aren't superuser
  if (user_id)
  {
    if (user)
    {
      /* Don't give a warning, if real user is same as given with --user */
      /* purecov: begin tested */
      tmp_user_info= getpwnam(user);
      if ((!tmp_user_info || user_id != tmp_user_info->pw_uid) &&
	  global_system_variables.log_warnings)
        sql_print_warning(
                    "One can only use the --user switch if running as root\n");
      /* purecov: end */
    }
    return NULL;
  }
  if (!user)
  {
    if (!opt_bootstrap)
    {
      sql_print_error("Fatal error: Please read \"Security\" section of the manual to find out how to run mysqld as root!\n");
      unireg_abort(1);
    }
    return NULL;
  }
  /* purecov: begin tested */
  if (!strcmp(user,"root"))
    return NULL;                        // Avoid problem with dynamic libraries

  if (!(tmp_user_info= getpwnam(user)))
  {
    // Allow a numeric uid to be used
    const char *pos;
    for (pos= user; my_isdigit(mysqld_charset,*pos); pos++) ;
    if (*pos)                                   // Not numeric id
      goto err;
    if (!(tmp_user_info= getpwuid(atoi(user))))
      goto err;
  }
  return tmp_user_info;
  /* purecov: end */

err:
  sql_print_error("Fatal error: Can't change to run as user '%s' ;  Please check that the user exists!\n",user);
  unireg_abort(1);

#ifdef PR_SET_DUMPABLE
  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    /* inform kernel that process is dumpable */
    (void) prctl(PR_SET_DUMPABLE, 1);
  }
#endif

  return NULL;
}

static void set_user(const char *user, struct passwd *user_info_arg)
{
  /* purecov: begin tested */
  DBUG_ASSERT(user_info_arg != 0);
#ifdef HAVE_INITGROUPS
  /*
    We can get a SIGSEGV when calling initgroups() on some systems when NSS
    is configured to use LDAP and the server is statically linked.  We set
    calling_initgroups as a flag to the SIGSEGV handler that is then used to
    output a specific message to help the user resolve this problem.
  */
  calling_initgroups= TRUE;
  initgroups((char*) user, user_info_arg->pw_gid);
  calling_initgroups= FALSE;
#endif
  if (setgid(user_info_arg->pw_gid) == -1)
  {
    sql_perror("setgid");
    unireg_abort(1);
  }
  if (setuid(user_info_arg->pw_uid) == -1)
  {
    sql_perror("setuid");
    unireg_abort(1);
  }
  /* purecov: end */
}


static void set_effective_user(struct passwd *user_info_arg)
{
  DBUG_ASSERT(user_info_arg != 0);
  if (setregid((gid_t)-1, user_info_arg->pw_gid) == -1)
  {
    sql_perror("setregid");
    unireg_abort(1);
  }
  if (setreuid((uid_t)-1, user_info_arg->pw_uid) == -1)
  {
    sql_perror("setreuid");
    unireg_abort(1);
  }
}


/** Change root user if started with @c --chroot . */
static void set_root(const char *path)
{
  if (chroot(path) == -1)
  {
    sql_perror("chroot");
    unireg_abort(1);
  }
  my_setwd("/", MYF(0));
}


static void network_init(void)
{
  int	arg;
  int   ret;
  uint  waited;
  uint  this_wait;
  uint  retry;
  char port_buf[NI_MAXSERV];
  DBUG_ENTER("network_init");

  if (thread_scheduler.init())
    unireg_abort(1);			/* purecov: inspected */

  set_ports();

  if (mysqld_port != 0 && !opt_disable_networking && !opt_bootstrap)
  {
    struct addrinfo *ai;
    struct addrinfo hints;
    int error;
    DBUG_PRINT("general",("IP Socket is %d",mysqld_port));

    bzero(&hints, sizeof (hints));
    hints.ai_flags= AI_PASSIVE;
    hints.ai_socktype= SOCK_STREAM;
    hints.ai_family= AF_UNSPEC;

    my_snprintf(port_buf, NI_MAXSERV, "%d", mysqld_port);
    error= getaddrinfo(my_bind_addr_str, port_buf, &hints, &ai);
    if (error != 0)
    {
      DBUG_PRINT("error",("Got error: %d from getaddrinfo()", error));
      sql_perror(ER(ER_IPSOCK_ERROR));		/* purecov: tested */
      unireg_abort(1);				/* purecov: tested */
    }


    ip_sock= socket(ai->ai_family, ai->ai_socktype,
                    ai->ai_protocol);

    if (ip_sock == INVALID_SOCKET)
    {
      DBUG_PRINT("error",("Got error: %d from socket()",socket_errno));
      sql_perror(ER(ER_IPSOCK_ERROR));		/* purecov: tested */
      unireg_abort(1);				/* purecov: tested */
    }

    /*
      We should not use SO_REUSEADDR on windows as this would enable a
      user to open two mysqld servers with the same TCP/IP port.
    */
    arg= 1;
    (void) setsockopt(ip_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&arg,sizeof(arg));

#ifdef IPV6_V6ONLY
     /*
       For interoperability with older clients, IPv6 socket should
       listen on both IPv6 and IPv4 wildcard addresses.
       Turn off IPV6_V6ONLY option.
     */
    if (ai->ai_family == AF_INET6)
    {
      arg= 0;      
      (void) setsockopt(ip_sock, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&arg,
                sizeof(arg));
    }
#endif
    /*
      Sometimes the port is not released fast enough when stopping and
      restarting the server. This happens quite often with the test suite
      on busy Linux systems. Retry to bind the address at these intervals:
      Sleep intervals: 1, 2, 4,  6,  9, 13, 17, 22, ...
      Retry at second: 1, 3, 7, 13, 22, 35, 52, 74, ...
      Limit the sequence by mysqld_port_timeout (set --port-open-timeout=#).
    */
    for (waited= 0, retry= 1; ; retry++, waited+= this_wait)
    {
      if (((ret= bind(ip_sock, ai->ai_addr, ai->ai_addrlen)) >= 0 ) ||
          (socket_errno != SOCKET_EADDRINUSE) ||
          (waited >= mysqld_port_timeout))
        break;
      sql_print_information("Retrying bind on TCP/IP port %u", mysqld_port);
      this_wait= retry * retry / 3 + 1;
      sleep(this_wait);
    }
    freeaddrinfo(ai);
    if (ret < 0)
    {
      DBUG_PRINT("error",("Got error: %d from bind",socket_errno));
      sql_perror("Can't start server: Bind on TCP/IP port");
      sql_print_error("Do you already have another mysqld server running on port: %d ?",mysqld_port);
      unireg_abort(1);
    }
    if (listen(ip_sock,(int) back_log) < 0)
    {
      sql_perror("Can't start server: listen() on TCP/IP port");
      sql_print_error("listen() on TCP/IP failed with error %d",
		      socket_errno);
      unireg_abort(1);
    }
  }

  DBUG_PRINT("info",("server started"));
  DBUG_VOID_RETURN;
}

/**
  Close a connection.

  @param thd		Thread handle
  @param errcode	Error code to print to console
  @param lock	        1 if we have have to lock LOCK_thread_count

  @note
    For the connection that is doing shutdown, this is called twice
*/
void close_connection(THD *thd, uint errcode, bool lock)
{
  st_vio *vio;
  DBUG_ENTER("close_connection");
  DBUG_PRINT("enter",("fd: %s  error: '%s'",
		      thd->net.vio ? vio_description(thd->net.vio) :
		      "(not connected)",
		      errcode ? ER(errcode) : ""));
  if (lock)
    (void) pthread_mutex_lock(&LOCK_thread_count);
  thd->killed= THD::KILL_CONNECTION;
  if ((vio= thd->net.vio) != 0)
  {
    if (errcode)
      net_send_error(thd, errcode, ER(errcode)); /* purecov: inspected */
    vio_close(vio);			/* vio is freed in delete thd */
  }
  if (lock)
    (void) pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
}


/** Called when a thread is aborted. */
/* ARGSUSED */
extern "C" sig_handler end_thread_signal(int sig __attribute__((unused)))
{
  THD *thd=current_thd;
  DBUG_ENTER("end_thread_signal");
  if (thd && ! thd->bootstrap)
  {
    statistic_increment(killed_threads, &LOCK_status);
    thread_scheduler.end_thread(thd,0);		/* purecov: inspected */
  }
  DBUG_VOID_RETURN;				/* purecov: deadcode */
}


/*
  Unlink thd from global list of available connections and free thd

  SYNOPSIS
    unlink_thd()
    thd		 Thread handler

  NOTES
    LOCK_thread_count is locked and left locked
*/

void unlink_thd(THD *thd)
{
  DBUG_ENTER("unlink_thd");
  DBUG_PRINT("enter", ("thd: 0x%lx", (long) thd));
  thd->cleanup();

  pthread_mutex_lock(&LOCK_connection_count);
  --connection_count;
  pthread_mutex_unlock(&LOCK_connection_count);

  (void) pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  delete thd;
  DBUG_VOID_RETURN;
}


/*
  Store thread in cache for reuse by new connections

  SYNOPSIS
    cache_thread()

  NOTES
    LOCK_thread_count has to be locked

  RETURN
    0  Thread was not put in cache
    1  Thread is to be reused by new connection.
       (ie, caller should return, not abort with pthread_exit())
*/


static bool cache_thread()
{
  safe_mutex_assert_owner(&LOCK_thread_count);
  if (cached_thread_count < thread_cache_size &&
      ! abort_loop && !kill_cached_threads)
  {
    /* Don't kill the thread, just put it in cache for reuse */
    DBUG_PRINT("info", ("Adding thread to cache"));
    cached_thread_count++;
    while (!abort_loop && ! wake_thread && ! kill_cached_threads)
      (void) pthread_cond_wait(&COND_thread_cache, &LOCK_thread_count);
    cached_thread_count--;
    if (kill_cached_threads)
      pthread_cond_signal(&COND_flush_thread_cache);
    if (wake_thread)
    {
      THD *thd;
      wake_thread--;
      thd= thread_cache.get();
      thd->thread_stack= (char*) &thd;          // For store_globals
      (void) thd->store_globals();
      /*
        THD::mysys_var::abort is associated with physical thread rather
        than with THD object. So we need to reset this flag before using
        this thread for handling of new THD object/connection.
      */
      thd->mysys_var->abort= 0;
      thd->thr_create_utime= my_micro_time();
      threads.append(thd);
      return(1);
    }
  }
  return(0);
}


/*
  End thread for the current connection

  SYNOPSIS
    one_thread_per_connection_end()
    thd		  Thread handler
    put_in_cache  Store thread in cache, if there is room in it
                  Normally this is true in all cases except when we got
                  out of resources initializing the current thread

  NOTES
    If thread is cached, we will wait until thread is scheduled to be
    reused and then we will return.
    If thread is not cached, we end the thread.

  RETURN
    0    Signal to handle_one_connection to reuse connection
*/

bool one_thread_per_connection_end(THD *thd, bool put_in_cache)
{
  DBUG_ENTER("one_thread_per_connection_end");
  unlink_thd(thd);
  if (put_in_cache)
    put_in_cache= cache_thread();
  pthread_mutex_unlock(&LOCK_thread_count);
  if (put_in_cache)
    DBUG_RETURN(0);                             // Thread is reused

  /* It's safe to broadcast outside a lock (COND... is not deleted here) */
  DBUG_PRINT("signal", ("Broadcasting COND_thread_count"));
  my_thread_end();
  (void) pthread_cond_broadcast(&COND_thread_count);

  pthread_exit(0);
  DBUG_RETURN(0);                               // Impossible
}


void flush_thread_cache()
{
  (void) pthread_mutex_lock(&LOCK_thread_count);
  kill_cached_threads++;
  while (cached_thread_count)
  {
    pthread_cond_broadcast(&COND_thread_cache);
    pthread_cond_wait(&COND_flush_thread_cache,&LOCK_thread_count);
  }
  kill_cached_threads--;
  (void) pthread_mutex_unlock(&LOCK_thread_count);
}


#ifdef THREAD_SPECIFIC_SIGPIPE
/**
  Aborts a thread nicely. Comes here on SIGPIPE.

  @todo
    One should have to fix that thr_alarm know about this thread too.
*/
extern "C" sig_handler abort_thread(int sig __attribute__((unused)))
{
  THD *thd=current_thd;
  DBUG_ENTER("abort_thread");
  if (thd)
    thd->killed= THD::KILL_CONNECTION;
  DBUG_VOID_RETURN;
}
#endif

#if BACKTRACE_DEMANGLE
#include <cxxabi.h>
extern "C" char *my_demangle(const char *mangled_name, int *status)
{
  return abi::__cxa_demangle(mangled_name, NULL, NULL, status);
}
#endif


extern "C" sig_handler handle_segfault(int sig)
{
  time_t curr_time;
  struct tm tm;
  THD *thd=current_thd;

  /*
    Strictly speaking, one needs a mutex here
    but since we have got SIGSEGV already, things are a mess
    so not having the mutex is not as bad as possibly using a buggy
    mutex - so we keep things simple
  */
  if (segfaulted)
  {
    fprintf(stderr, "Fatal " SIGNAL_FMT " while backtracing\n", sig);
    exit(1);
  }

  segfaulted = 1;

  curr_time= my_time(0);
  localtime_r(&curr_time, &tm);

  fprintf(stderr,"\
%02d%02d%02d %2d:%02d:%02d - mysqld got " SIGNAL_FMT " ;\n\
This could be because you hit a bug. It is also possible that this binary\n\
or one of the libraries it was linked against is corrupt, improperly built,\n\
or misconfigured. This error can also be caused by malfunctioning hardware.\n",
          tm.tm_year % 100, tm.tm_mon+1, tm.tm_mday,
          tm.tm_hour, tm.tm_min, tm.tm_sec,
	  sig);
  fprintf(stderr, "\
We will try our best to scrape up some info that will hopefully help diagnose\n\
the problem, but since we have already crashed, something is definitely wrong\n\
and this may fail.\n\n");
  fprintf(stderr, "key_buffer_size=%lu\n",
          (ulong) dflt_key_cache->key_cache_mem_size);
  fprintf(stderr, "read_buffer_size=%ld\n", (long) global_system_variables.read_buff_size);
  fprintf(stderr, "max_used_connections=%lu\n", max_used_connections);
  fprintf(stderr, "max_threads=%u\n", thread_scheduler.max_threads);
  fprintf(stderr, "thread_count=%u\n", thread_count);
  fprintf(stderr, "connection_count=%u\n", connection_count);
  fprintf(stderr, "It is possible that mysqld could use up to \n\
key_buffer_size + (read_buffer_size + sort_buffer_size)*max_threads = %lu K\n\
bytes of memory\n", ((ulong) dflt_key_cache->key_cache_mem_size +
		     (global_system_variables.read_buff_size +
		      global_system_variables.sortbuff_size) *
		     thread_scheduler.max_threads +
                     max_connections * sizeof(THD)) / 1024);
  fprintf(stderr, "Hope that's ok; if not, decrease some variables in the equation.\n\n");

#ifdef HAVE_STACKTRACE
  if (!(test_flags & TEST_NO_STACKTRACE))
  {
    fprintf(stderr,"thd: 0x%lx\n",(long) thd);
    fprintf(stderr,"\
Attempting backtrace. You can use the following information to find out\n\
where mysqld died. If you see no messages after this, something went\n\
terribly wrong...\n");  
    print_stacktrace(thd ? (uchar*) thd->thread_stack : (uchar*) 0,
                     my_thread_stack_size);
  }
  if (thd)
  {
    const char *kreason= "UNKNOWN";
    switch (thd->killed) {
    case THD::NOT_KILLED:
      kreason= "NOT_KILLED";
      break;
    case THD::KILL_BAD_DATA:
      kreason= "KILL_BAD_DATA";
      break;
    case THD::KILL_CONNECTION:
      kreason= "KILL_CONNECTION";
      break;
    case THD::KILL_QUERY:
      kreason= "KILL_QUERY";
      break;
    case THD::KILLED_NO_VALUE:
      kreason= "KILLED_NO_VALUE";
      break;
    }
    fprintf(stderr, "Trying to get some variables.\n\
Some pointers may be invalid and cause the dump to abort...\n");
    safe_print_str("thd->query", thd->query, 1024);
    fprintf(stderr, "thd->thread_id=%lu\n", (ulong) thd->thread_id);
    fprintf(stderr, "thd->killed=%s\n", kreason);
  }
  fprintf(stderr, "\
The manual page at http://dev.mysql.com/doc/mysql/en/crashing.html contains\n\
information that should help you find out what is causing the crash.\n");
  fflush(stderr);
#endif /* HAVE_STACKTRACE */

#ifdef HAVE_INITGROUPS
  if (calling_initgroups)
    fprintf(stderr, "\n\
This crash occured while the server was calling initgroups(). This is\n\
often due to the use of a mysqld that is statically linked against glibc\n\
and configured to use LDAP in /etc/nsswitch.conf. You will need to either\n\
upgrade to a version of glibc that does not have this problem (2.3.4 or\n\
later when used with nscd), disable LDAP in your nsswitch.conf, or use a\n\
mysqld that is not statically linked.\n");
#endif

  if (thd_lib_detected == THD_LIB_LT && !getenv("LD_ASSUME_KERNEL"))
    fprintf(stderr,"\n\
You are running a statically-linked LinuxThreads binary on an NPTL system.\n\
This can result in crashes on some distributions due to LT/NPTL conflicts.\n\
You should either build a dynamically-linked binary, or force LinuxThreads\n\
to be used with the LD_ASSUME_KERNEL environment variable. Please consult\n\
the documentation for your distribution on how to do that.\n");
  
  if (locked_in_memory)
  {
    fprintf(stderr, "\n\
The \"--memlock\" argument, which was enabled, uses system calls that are\n\
unreliable and unstable on some operating systems and operating-system\n\
versions (notably, some versions of Linux).  This crash could be due to use\n\
of those buggy OS calls.  You should consider whether you really need the\n\
\"--memlock\" parameter and/or consult the OS distributer about \"mlockall\"\n\
bugs.\n");
  }

#ifdef HAVE_WRITE_CORE
  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    fprintf(stderr, "Writing a core file\n");
    fflush(stderr);
    write_core(sig);
  }
#endif

  exit(1);
}

#ifndef SA_RESETHAND
#define SA_RESETHAND 0
#endif
#ifndef SA_NODEFER
#define SA_NODEFER 0
#endif

static void init_signals(void)
{
  sigset_t set;
  struct sigaction sa;
  DBUG_ENTER("init_signals");

  my_sigset(THR_SERVER_ALARM,print_signal_warning); // Should never be called!

  if (!(test_flags & TEST_NO_STACKTRACE) || (test_flags & TEST_CORE_ON_SIGNAL))
  {
    sa.sa_flags = SA_RESETHAND | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigprocmask(SIG_SETMASK,&sa.sa_mask,NULL);

    init_stacktrace();
    sa.sa_handler=handle_segfault;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
#ifdef SIGBUS
    sigaction(SIGBUS, &sa, NULL);
#endif
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
  }

#ifdef HAVE_GETRLIMIT
  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    /* Change limits so that we will get a core file */
    STRUCT_RLIMIT rl;
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl) && global_system_variables.log_warnings)
      sql_print_warning("setrlimit could not change the size of core files to 'infinity';  We may not be able to generate a core file on signals");
  }
#endif
  (void) sigemptyset(&set);
  my_sigset(SIGPIPE,SIG_IGN);
  sigaddset(&set,SIGPIPE);
#ifndef IGNORE_SIGHUP_SIGQUIT
  sigaddset(&set,SIGQUIT);
  sigaddset(&set,SIGHUP);
#endif
  sigaddset(&set,SIGTERM);

  /* Fix signals if blocked by parents (can happen on Mac OS X) */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = print_signal_warning;
  sigaction(SIGTERM, &sa, (struct sigaction*) 0);
  sa.sa_flags = 0;
  sa.sa_handler = print_signal_warning;
  sigaction(SIGHUP, &sa, (struct sigaction*) 0);
#ifdef SIGTSTP
  sigaddset(&set,SIGTSTP);
#endif
  if (thd_lib_detected != THD_LIB_LT)
    sigaddset(&set,THR_SERVER_ALARM);
  if (test_flags & TEST_SIGINT)
  {
    my_sigset(thr_kill_signal, end_thread_signal);
    // May be SIGINT
    sigdelset(&set, thr_kill_signal);
  }
  else
    sigaddset(&set,SIGINT);
  sigprocmask(SIG_SETMASK,&set,NULL);
  pthread_sigmask(SIG_SETMASK,&set,NULL);
  DBUG_VOID_RETURN;
}


static void start_signal_handler(void)
{
  int error;
  pthread_attr_t thr_attr;
  DBUG_ENTER("start_signal_handler");

  (void) pthread_attr_init(&thr_attr);
  pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
  (void) pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);
  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
  {
    struct sched_param tmp_sched_param;

    memset(&tmp_sched_param, 0, sizeof(tmp_sched_param));
    tmp_sched_param.sched_priority= INTERRUPT_PRIOR;
    (void)pthread_attr_setschedparam(&thr_attr, &tmp_sched_param);
  }
#if defined(__ia64__) || defined(__ia64)
  /*
    Peculiar things with ia64 platforms - it seems we only have half the
    stack size in reality, so we have to double it here
  */
  pthread_attr_setstacksize(&thr_attr,my_thread_stack_size*2);
#else
  pthread_attr_setstacksize(&thr_attr,my_thread_stack_size);

  (void) pthread_mutex_lock(&LOCK_thread_count);
  if ((error=pthread_create(&signal_thread,&thr_attr,signal_hand,0)))
  {
    sql_print_error("Can't create interrupt-thread (error %d, errno: %d)",
		    error,errno);
    exit(1);
  }
  (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  pthread_mutex_unlock(&LOCK_thread_count);

  (void) pthread_attr_destroy(&thr_attr);
  DBUG_VOID_RETURN;
}


/** This threads handles all signals and alarms. */
/* ARGSUSED */
pthread_handler_t signal_hand(void *arg __attribute__((unused)))
{
  sigset_t set;
  int sig;
  my_thread_init();				// Init new thread
  DBUG_ENTER("signal_hand");
  signal_thread_in_use= 1;

  /*
    Setup alarm handler
    This should actually be '+ max_number_of_slaves' instead of +10,
    but the +10 should be quite safe.
  */
  init_thr_alarm(thread_scheduler.max_threads + 10);
  if (thd_lib_detected != THD_LIB_LT && (test_flags & TEST_SIGINT))
  {
    (void) sigemptyset(&set);			// Setup up SIGINT for debug
    (void) sigaddset(&set,SIGINT);		// For debugging
    (void) pthread_sigmask(SIG_UNBLOCK,&set,NULL);
  }
  (void) sigemptyset(&set);			// Setup up SIGINT for debug
#ifdef USE_ONE_SIGNAL_HAND
  (void) sigaddset(&set,THR_SERVER_ALARM);	// For alarms
#endif
#ifndef IGNORE_SIGHUP_SIGQUIT
  (void) sigaddset(&set,SIGQUIT);
  (void) sigaddset(&set,SIGHUP);
#endif
  (void) sigaddset(&set,SIGTERM);
  (void) sigaddset(&set,SIGTSTP);

  /* Save pid to this process (or thread on Linux) */
  if (!opt_bootstrap)
    create_pid_file();

#ifdef HAVE_STACK_TRACE_ON_SEGV
  if (opt_do_pstack)
  {
    sprintf(pstack_file_name,"mysqld-%lu-%%d-%%d.backtrace", (ulong)getpid());
    pstack_install_segv_action(pstack_file_name);
  }
#endif /* HAVE_STACK_TRACE_ON_SEGV */

  /*
    signal to start_signal_handler that we are ready
    This works by waiting for start_signal_handler to free mutex,
    after which we signal it that we are ready.
    At this pointer there is no other threads running, so there
    should not be any other pthread_cond_signal() calls.
  */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  (void) pthread_cond_broadcast(&COND_thread_count);

  (void) pthread_sigmask(SIG_BLOCK,&set,NULL);
  for (;;)
  {
    int error;					// Used when debugging
    if (shutdown_in_progress && !abort_loop)
    {
      sig= SIGTERM;
      error=0;
    }
    else
      while ((error=my_sigwait(&set,&sig)) == EINTR) ;
    if (cleanup_done)
    {
      DBUG_PRINT("quit",("signal_handler: calling my_thread_end()"));
      my_thread_end();
      signal_thread_in_use= 0;
      pthread_exit(0);				// Safety
    }
    switch (sig) {
    case SIGTERM:
    case SIGQUIT:
    case SIGKILL:
#ifdef EXTRA_DEBUG
      sql_print_information("Got signal %d to shutdown mysqld",sig);
#endif
      /* switch to the old log message processing */
      logger.set_handlers(LOG_FILE, opt_slow_log ? LOG_FILE:LOG_NONE,
                          opt_log ? LOG_FILE:LOG_NONE);
      DBUG_PRINT("info",("Got signal: %d  abort_loop: %d",sig,abort_loop));
      if (!abort_loop)
      {
	abort_loop=1;				// mark abort for threads
#ifdef USE_ONE_SIGNAL_HAND
	pthread_t tmp;
	if (!(opt_specialflag & SPECIAL_NO_PRIOR))
        {
          struct sched_param tmp_sched_param;

          memset(&tmp_sched_param, 0, sizeof(tmp_sched_param));
          tmp_sched_param.sched_priority= INTERRUPT_PRIOR;
          (void)pthread_attr_setschedparam(&connection_attrib, &tmp_sched_param);
        }
	if (pthread_create(&tmp,&connection_attrib, kill_server_thread,
			   (void*) &sig))
	  sql_print_error("Can't create thread to kill server");
#else
	kill_server((void*) sig);	// MIT THREAD has a alarm thread
#endif
      }
      break;
    case SIGHUP:
      if (!abort_loop)
      {
        bool not_used;
	mysql_print_status();		// Print some debug info
        reload_cache((THD*) 0,
                     (REFRESH_LOG | REFRESH_TABLES | REFRESH_FAST |
                      REFRESH_GRANT |
                      REFRESH_THREADS | REFRESH_HOSTS),
                     (TABLE_LIST*) 0, &not_used); // Flush logs
      }
      /* reenable logs after the options were reloaded */
      if (log_output_options & LOG_NONE)
      {
        logger.set_handlers(LOG_FILE,
                            opt_slow_log ? LOG_TABLE : LOG_NONE,
                            opt_log ? LOG_TABLE : LOG_NONE);
      }
      else
      {
        logger.set_handlers(LOG_FILE,
                            opt_slow_log ? log_output_options : LOG_NONE,
                            opt_log ? log_output_options : LOG_NONE);
      }
      break;
#ifdef USE_ONE_SIGNAL_HAND
    case THR_SERVER_ALARM:
      process_alarm(sig);			// Trigger alarms.
      break;
#endif
    default:
#ifdef EXTRA_DEBUG
      sql_print_warning("Got signal: %d  error: %d",sig,error); /* purecov: tested */
#endif
      break;					/* purecov: tested */
    }
  }
  return(0);					/* purecov: deadcode */
}

static void check_data_home(const char *path)
{}

#endif	/* __WIN__*/


/**
  All global error messages are sent here where the first one is stored
  for the client.
*/
/* ARGSUSED */
extern "C" void my_message_sql(uint error, const char *str, myf MyFlags);

void my_message_sql(uint error, const char *str, myf MyFlags)
{
  THD *thd;
  DBUG_ENTER("my_message_sql");
  DBUG_PRINT("error", ("error: %u  message: '%s'", error, str));
  /*
    Put here following assertion when situation with EE_* error codes
    will be fixed
    DBUG_ASSERT(error != 0);
  */
  if ((thd= current_thd))
  {
    if (MyFlags & ME_FATALERROR)
      thd->is_fatal_error= 1;

#ifdef BUG_36098_FIXED
    mysql_audit_general(thd,MYSQL_AUDIT_GENERAL_ERROR,error,my_time(0),
                        0,0,str,str ? strlen(str) : 0,
                        thd->query,thd->query_length,
                        thd->variables.character_set_client,
                        thd->row_count);
#endif


    /*
      TODO: There are two exceptions mechanism (THD and sp_rcontext),
      this could be improved by having a common stack of handlers.
    */
    if (thd->handle_error(error, str,
                          MYSQL_ERROR::WARN_LEVEL_ERROR))
      DBUG_VOID_RETURN;

    thd->is_slave_error=  1; // needed to catch query errors during replication

    /*
      thd->lex->current_select == 0 if lex structure is not inited
      (not query command (COM_QUERY))
    */
    if (thd->lex->current_select &&
	thd->lex->current_select->no_error && !thd->is_fatal_error)
    {
      DBUG_PRINT("error",
                 ("Error converted to warning: current_select: no_error %d  "
                  "fatal_error: %d",
                  (thd->lex->current_select ?
                   thd->lex->current_select->no_error : 0),
                  (int) thd->is_fatal_error));
    }
    else
    {
      if (! thd->main_da.is_error())            // Return only first message
      {
        if (error == 0)
          error= ER_UNKNOWN_ERROR;
        if (str == NULL)
          str= ER(error);
        thd->main_da.set_error_status(thd, error, str);
      }
    }

    if (!thd->no_warnings_for_error && !thd->is_fatal_error)
    {
      /*
        Suppress infinite recursion if there a memory allocation error
        inside push_warning.
      */
      thd->no_warnings_for_error= TRUE;
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR, error, str);
      thd->no_warnings_for_error= FALSE;
    }
  }
  if (!thd || MyFlags & ME_NOREFRESH)
    sql_print_error("%s: %s",my_progname,str); /* purecov: inspected */
  DBUG_VOID_RETURN;
}


extern "C" void *my_str_malloc_mysqld(size_t size);
extern "C" void my_str_free_mysqld(void *ptr);

void *my_str_malloc_mysqld(size_t size)
{
  return my_malloc(size, MYF(MY_FAE));
}


void my_str_free_mysqld(void *ptr)
{
  my_free((uchar*)ptr, MYF(MY_FAE));
}


static const char *load_default_groups[]= {
"mysqld","server", MYSQL_BASE_VERSION, 0, 0};


/**
  Initialize one of the global date/time format variables.

  @param format_type		What kind of format should be supported
  @param var_ptr		Pointer to variable that should be updated

  @note
    The default value is taken from either opt_date_time_formats[] or
    the ISO format (ANSI SQL)

  @retval
    0 ok
  @retval
    1 error
*/

static bool init_global_datetime_format(timestamp_type format_type,
                                        DATE_TIME_FORMAT **var_ptr)
{
  /* Get command line option */
  const char *str= opt_date_time_formats[format_type];

  if (!str)					// No specified format
  {
    str= get_date_time_format_str(&known_date_time_formats[ISO_FORMAT],
				  format_type);
    /*
      Set the "command line" option to point to the generated string so
      that we can set global formats back to default
    */
    opt_date_time_formats[format_type]= str;
  }
  if (!(*var_ptr= date_time_format_make(format_type, str, strlen(str))))
  {
    fprintf(stderr, "Wrong date/time format specifier: %s\n", str);
    return 1;
  }
  return 0;
}

SHOW_VAR com_status_vars[]= {
  {"admin_commands",       (char*) offsetof(STATUS_VAR, com_other), SHOW_LONG_STATUS},
  {"assign_to_keycache",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ASSIGN_TO_KEYCACHE]), SHOW_LONG_STATUS},
  {"alter_db",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_DB]), SHOW_LONG_STATUS},
  {"alter_db_upgrade",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_DB_UPGRADE]), SHOW_LONG_STATUS},
  {"alter_table",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_TABLE]), SHOW_LONG_STATUS},
  {"alter_tablespace",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ALTER_TABLESPACE]), SHOW_LONG_STATUS},
  {"analyze",              (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ANALYZE]), SHOW_LONG_STATUS},
  {"begin",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_BEGIN]), SHOW_LONG_STATUS},
  {"binlog",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_BINLOG_BASE64_EVENT]), SHOW_LONG_STATUS},
  {"change_db",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CHANGE_DB]), SHOW_LONG_STATUS},
  {"change_master",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CHANGE_MASTER]), SHOW_LONG_STATUS},
  {"check",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CHECK]), SHOW_LONG_STATUS},
  {"checksum",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CHECKSUM]), SHOW_LONG_STATUS},
  {"commit",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_COMMIT]), SHOW_LONG_STATUS},
  {"create_db",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_DB]), SHOW_LONG_STATUS},
  {"create_index",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_INDEX]), SHOW_LONG_STATUS},
  {"create_table",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_CREATE_TABLE]), SHOW_LONG_STATUS},
  {"delete",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DELETE]), SHOW_LONG_STATUS},
  {"delete_multi",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DELETE_MULTI]), SHOW_LONG_STATUS},
  {"drop_db",              (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_DB]), SHOW_LONG_STATUS},
  {"drop_index",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_INDEX]), SHOW_LONG_STATUS},
  {"drop_table",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_DROP_TABLE]), SHOW_LONG_STATUS},
  {"empty_query",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_EMPTY_QUERY]), SHOW_LONG_STATUS},
  {"flush",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_FLUSH]), SHOW_LONG_STATUS},
  {"insert",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_INSERT]), SHOW_LONG_STATUS},
  {"insert_select",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_INSERT_SELECT]), SHOW_LONG_STATUS},
  {"kill",                 (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_KILL]), SHOW_LONG_STATUS},
  {"load",                 (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_LOAD]), SHOW_LONG_STATUS},
  {"lock_tables",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_LOCK_TABLES]), SHOW_LONG_STATUS},
  {"optimize",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_OPTIMIZE]), SHOW_LONG_STATUS},
  {"purge",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_PURGE]), SHOW_LONG_STATUS},
  {"purge_before_date",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_PURGE_BEFORE]), SHOW_LONG_STATUS},
  {"release_savepoint",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_RELEASE_SAVEPOINT]), SHOW_LONG_STATUS},
  {"rename_table",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_RENAME_TABLE]), SHOW_LONG_STATUS},
  {"repair",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REPAIR]), SHOW_LONG_STATUS},
  {"replace",              (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REPLACE]), SHOW_LONG_STATUS},
  {"replace_select",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_REPLACE_SELECT]), SHOW_LONG_STATUS},
  {"reset",                (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_RESET]), SHOW_LONG_STATUS},
  {"rollback",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ROLLBACK]), SHOW_LONG_STATUS},
  {"rollback_to_savepoint",(char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_ROLLBACK_TO_SAVEPOINT]), SHOW_LONG_STATUS},
  {"savepoint",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SAVEPOINT]), SHOW_LONG_STATUS},
  {"select",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SELECT]), SHOW_LONG_STATUS},
  {"set_option",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SET_OPTION]), SHOW_LONG_STATUS},
  {"show_binlog_events",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_BINLOG_EVENTS]), SHOW_LONG_STATUS},
  {"show_binlogs",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_BINLOGS]), SHOW_LONG_STATUS},
  {"show_charsets",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CHARSETS]), SHOW_LONG_STATUS},
  {"show_collations",      (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_COLLATIONS]), SHOW_LONG_STATUS},
  {"show_create_db",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CREATE_DB]), SHOW_LONG_STATUS},
  {"show_create_table",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_CREATE]), SHOW_LONG_STATUS},
  {"show_databases",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_DATABASES]), SHOW_LONG_STATUS},
  {"show_engine_status",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_ENGINE_STATUS]), SHOW_LONG_STATUS},
  {"show_errors",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_ERRORS]), SHOW_LONG_STATUS},
  {"show_fields",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_FIELDS]), SHOW_LONG_STATUS},
  {"show_keys",            (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_KEYS]), SHOW_LONG_STATUS},
  {"show_master_status",   (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_MASTER_STAT]), SHOW_LONG_STATUS},
  {"show_open_tables",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_OPEN_TABLES]), SHOW_LONG_STATUS},
  {"show_plugins",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_PLUGINS]), SHOW_LONG_STATUS},
  {"show_processlist",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_PROCESSLIST]), SHOW_LONG_STATUS},
  {"show_slave_hosts",     (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_SLAVE_HOSTS]), SHOW_LONG_STATUS},
  {"show_slave_status",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_SLAVE_STAT]), SHOW_LONG_STATUS},
  {"show_status",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_STATUS]), SHOW_LONG_STATUS},
  {"show_table_status",    (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_TABLE_STATUS]), SHOW_LONG_STATUS},
  {"show_tables",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_TABLES]), SHOW_LONG_STATUS},
  {"show_variables",       (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_VARIABLES]), SHOW_LONG_STATUS},
  {"show_warnings",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SHOW_WARNS]), SHOW_LONG_STATUS},
  {"slave_start",          (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SLAVE_START]), SHOW_LONG_STATUS},
  {"slave_stop",           (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_SLAVE_STOP]), SHOW_LONG_STATUS},
  {"truncate",             (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_TRUNCATE]), SHOW_LONG_STATUS},
  {"unlock_tables",        (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_UNLOCK_TABLES]), SHOW_LONG_STATUS},
  {"update",               (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_UPDATE]), SHOW_LONG_STATUS},
  {"update_multi",         (char*) offsetof(STATUS_VAR, com_stat[(uint) SQLCOM_UPDATE_MULTI]), SHOW_LONG_STATUS},
  {NullS, NullS, SHOW_LONG}
};

static int init_common_variables(const char *conf_file_name, int argc,
				 char **argv, const char **groups)
{
  char buff[FN_REFLEN], *s;
  umask(((~my_umask) & 0666));
  my_decimal_set_zero(&decimal_zero); // set decimal_zero constant;
  tzset();			// Set tzname

  max_system_variables.pseudo_thread_id= (ulong)~0;
  server_start_time= flush_status_time= my_time(0);
  rpl_filter= new Rpl_filter;
  binlog_filter= new Rpl_filter;
  if (!rpl_filter || !binlog_filter) 
  {
    sql_perror("Could not allocate replication and binlog filters");
    exit(1);
  }

  if (init_thread_environment())
    return 1;
  mysql_init_variables();

#ifdef HAVE_TZNAME
  {
    struct tm tm_tmp;
    localtime_r(&server_start_time,&tm_tmp);
    strmake(system_time_zone, tzname[tm_tmp.tm_isdst != 0 ? 1 : 0],
            sizeof(system_time_zone)-1);

 }
#endif
  /*
    We set SYSTEM time zone as reasonable default and
    also for failure of my_tz_init() and bootstrap mode.
    If user explicitly set time zone with --default-time-zone
    option we will change this value in my_tz_init().
  */
  global_system_variables.time_zone= my_tz_SYSTEM;

  /*
    Init mutexes for the global MYSQL_BIN_LOG objects.
    As safe_mutex depends on what MY_INIT() does, we can't init the mutexes of
    global MYSQL_BIN_LOGs in their constructors, because then they would be
    inited before MY_INIT(). So we do it here.
  */
  mysql_bin_log.init_pthread_objects();

  if (gethostname(glob_hostname,sizeof(glob_hostname)) < 0)
  {
    strmake(glob_hostname, STRING_WITH_LEN("localhost"));
    sql_print_warning("gethostname failed, using '%s' as hostname",
                      glob_hostname);
    strmake(pidfile_name, STRING_WITH_LEN("mysql"));
  }
  else
  strmake(pidfile_name, glob_hostname, sizeof(pidfile_name)-5);
  strmov(fn_ext(pidfile_name),".pid");		// Add proper extension

  /*
    Add server status variables to the dynamic list of
    status variables that is shown by SHOW STATUS.
    Later, in plugin_init, and mysql_install_plugin
    new entries could be added to that list.
  */
  if (add_status_vars(status_vars))
    return 1; // an error was already reported

#ifndef DBUG_OFF
  /*
    We have few debug-only commands in com_status_vars, only visible in debug
    builds. for simplicity we enable the assert only in debug builds

    There are 7 Com_ variables which don't have corresponding SQLCOM_ values:
    (TODO strictly speaking they shouldn't be here, should not have Com_ prefix
    that is. Perhaps Stmt_ ? Comstmt_ ? Prepstmt_ ?)

      Com_admin_commands       => com_other
      Com_stmt_close           => com_stmt_close
      Com_stmt_execute         => com_stmt_execute
      Com_stmt_fetch           => com_stmt_fetch
      Com_stmt_prepare         => com_stmt_prepare
      Com_stmt_reset           => com_stmt_reset
      Com_stmt_send_long_data  => com_stmt_send_long_data

    With this correction the number of Com_ variables (number of elements in
    the array, excluding the last element - terminator) must match the number
    of SQLCOM_ constants.
  */
  compile_time_assert(sizeof(com_status_vars)/sizeof(com_status_vars[0]) - 1 ==
                     SQLCOM_END + 7);
#endif

  load_defaults(conf_file_name, groups, &argc, &argv);
  defaults_argv=argv;
  defaults_argc=argc;
  get_options(&defaults_argc, defaults_argv);
  set_server_version();

  DBUG_PRINT("info",("%s  Ver %s for %s on %s\n",my_progname,
		     server_version, SYSTEM_TYPE,MACHINE_TYPE));

  /* connections and databases needs lots of files */
  {
    uint files, wanted_files, max_open_files;

    /* MyISAM requires two file handles per table. */
    wanted_files= 10+max_connections+table_cache_size*2;
    /*
      We are trying to allocate no less than max_connections*5 file
      handles (i.e. we are trying to set the limit so that they will
      be available).  In addition, we allocate no less than how much
      was already allocated.  However below we report a warning and
      recompute values only if we got less file handles than were
      explicitly requested.  No warning and re-computation occur if we
      can't get max_connections*5 but still got no less than was
      requested (value of wanted_files).
    */
    max_open_files= max(max(wanted_files, max_connections*5),
                        open_files_limit);
    files= my_set_max_open_files(max_open_files);

    if (files < wanted_files)
    {
      if (!open_files_limit)
      {
        /*
          If we have requested too much file handles than we bring
          max_connections in supported bounds.
        */
        max_connections= (ulong) min(files-10-TABLE_OPEN_CACHE_MIN*2,
                                     max_connections);
        /*
          Decrease table_cache_size according to max_connections, but
          not below TABLE_OPEN_CACHE_MIN.  Outer min() ensures that we
          never increase table_cache_size automatically (that could
          happen if max_connections is decreased above).
        */
        table_cache_size= (ulong) min(max((files-10-max_connections)/2,
                                          TABLE_OPEN_CACHE_MIN),
                                      table_cache_size);
	DBUG_PRINT("warning",
		   ("Changed limits: max_open_files: %u  max_connections: %ld  table_cache: %ld",
		    files, max_connections, table_cache_size));
	if (global_system_variables.log_warnings)
	  sql_print_warning("Changed limits: max_open_files: %u  max_connections: %ld  table_cache: %ld",
			files, max_connections, table_cache_size);
      }
      else if (global_system_variables.log_warnings)
	sql_print_warning("Could not increase number of max_open_files to more than %u (request: %u)", files, wanted_files);
    }
    open_files_limit= files;
  }
  unireg_init(opt_specialflag); /* Set up extern variabels */
  if (init_errmessage())	/* Read error messages from file */
    return 1;
  init_client_errs();
  lex_init();
  if (item_create_init())
    return 1;
  item_init();
  if (set_var_init())
    return 1;
  if (init_replication_sys_vars())
    return 1;
  mysys_uses_curses=0;
  /*
    Process a comma-separated character set list and choose
    the first available character set. This is mostly for
    test purposes, to be able to start "mysqld" even if
    the requested character set is not available (see bug#18743).
  */
  for (;;)
  {
    char *next_character_set_name= strchr(default_character_set_name, ',');
    if (next_character_set_name)
      *next_character_set_name++= '\0';
    if (!(default_charset_info=
          get_charset_by_csname(default_character_set_name,
                                MY_CS_PRIMARY, MYF(MY_WME))))
    {
      if (next_character_set_name)
      {
        default_character_set_name= next_character_set_name;
        default_collation_name= 0;          // Ignore collation
      }
      else
        return 1;                           // Eof of the list
    }
    else
      break;
  }

  if (default_collation_name)
  {
    CHARSET_INFO *default_collation;
    default_collation= get_charset_by_name(default_collation_name, MYF(0));
    if (!default_collation)
    {
      sql_print_error(ER(ER_UNKNOWN_COLLATION), default_collation_name);
      return 1;
    }
    if (!my_charset_same(default_charset_info, default_collation))
    {
      sql_print_error(ER(ER_COLLATION_CHARSET_MISMATCH),
		      default_collation_name,
		      default_charset_info->csname);
      return 1;
    }
    default_charset_info= default_collation;
  }
  /* Set collactions that depends on the default collation */
  global_system_variables.collation_server=	 default_charset_info;
  global_system_variables.collation_database=	 default_charset_info;
  global_system_variables.collation_connection=  default_charset_info;
  global_system_variables.character_set_results= default_charset_info;
  global_system_variables.character_set_client= default_charset_info;

  global_system_variables.optimizer_use_mrr= 1;
  global_system_variables.optimizer_switch= 0;

  if (!(character_set_filesystem= 
        get_charset_by_csname(character_set_filesystem_name,
                              MY_CS_PRIMARY, MYF(MY_WME))))
    return 1;
  global_system_variables.character_set_filesystem= character_set_filesystem;

  if (!(my_default_lc_time_names=
        my_locale_by_name(lc_time_names_name)))
  {
    sql_print_error("Unknown locale: '%s'", lc_time_names_name);
    return 1;
  }
  global_system_variables.lc_time_names= my_default_lc_time_names;
  
  sys_init_connect.value_length= 0;
  if ((sys_init_connect.value= opt_init_connect))
    sys_init_connect.value_length= strlen(opt_init_connect);
  else
    sys_init_connect.value=my_strdup("",MYF(0));

  sys_init_slave.value_length= 0;
  if ((sys_init_slave.value= opt_init_slave))
    sys_init_slave.value_length= strlen(opt_init_slave);
  else
    sys_init_slave.value=my_strdup("",MYF(0));

  /* check log options and issue warnings if needed */
  if (opt_log && opt_logname && !(log_output_options & LOG_FILE) &&
      !(log_output_options & LOG_NONE))
    sql_print_warning("Although a path was specified for the "
                      "--log option, log tables are used. "
                      "To enable logging to files use the --log-output option.");

  if (opt_slow_log && opt_slow_logname && !(log_output_options & LOG_FILE)
      && !(log_output_options & LOG_NONE))
    sql_print_warning("Although a path was specified for the "
                      "--log-slow-queries option, log tables are used. "
                      "To enable logging to files use the --log-output=file option.");

  s= opt_logname ? opt_logname : make_default_log_name(buff, ".log");
  sys_var_general_log_path.value= my_strdup(s, MYF(0));
  sys_var_general_log_path.value_length= strlen(s);

  s= opt_slow_logname ? opt_slow_logname : make_default_log_name(buff, "-slow.log");
  sys_var_slow_log_path.value= my_strdup(s, MYF(0));
  sys_var_slow_log_path.value_length= strlen(s);

  if (use_temp_pool && bitmap_init(&temp_pool,0,1024,1))
    return 1;
  if (my_database_names_init())
    return 1;

  /*
    Ensure that lower_case_table_names is set on system where we have case
    insensitive names.  If this is not done the users MyISAM tables will
    get corrupted if accesses with names of different case.
  */
  DBUG_PRINT("info", ("lower_case_table_names: %d", lower_case_table_names));
  lower_case_file_system= test_if_case_insensitive(mysql_real_data_home);
  if (!lower_case_table_names && lower_case_file_system == 1)
  {
    if (lower_case_table_names_used)
    {
      if (global_system_variables.log_warnings)
	sql_print_warning("\
You have forced lower_case_table_names to 0 through a command-line \
option, even though your file system '%s' is case insensitive.  This means \
that you can corrupt a MyISAM table by accessing it with different cases. \
You should consider changing lower_case_table_names to 1 or 2",
			mysql_real_data_home);
    }
    else
    {
      if (global_system_variables.log_warnings)
	sql_print_warning("Setting lower_case_table_names=2 because file system for %s is case insensitive", mysql_real_data_home);
      lower_case_table_names= 2;
    }
  }
  else if (lower_case_table_names == 2 &&
           !(lower_case_file_system=
             (test_if_case_insensitive(mysql_real_data_home) == 1)))
  {
    if (global_system_variables.log_warnings)
      sql_print_warning("lower_case_table_names was set to 2, even though your "
                        "the file system '%s' is case sensitive.  Now setting "
                        "lower_case_table_names to 0 to avoid future problems.",
			mysql_real_data_home);
    lower_case_table_names= 0;
  }
  else
  {
    lower_case_file_system=
      (test_if_case_insensitive(mysql_real_data_home) == 1);
  }

  /* Reset table_alias_charset, now that lower_case_table_names is set. */
  table_alias_charset= (lower_case_table_names ?
			files_charset_info :
			&my_charset_bin);

  return 0;
}


static int init_thread_environment()
{
  (void) pthread_mutex_init(&LOCK_mysql_create_db,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_lock_db,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_open, NULL);
  (void) pthread_mutex_init(&LOCK_thread_count,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_mapped_file,MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_status,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_error_log,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_manager,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_crypt,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_user_conn, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_active_mi, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_global_system_variables, MY_MUTEX_INIT_FAST);
  (void) my_rwlock_init(&LOCK_system_variables_hash, NULL);
  (void) pthread_mutex_init(&LOCK_global_read_lock, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_uuid_generator, MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_connection_count, MY_MUTEX_INIT_FAST);
  (void) my_rwlock_init(&LOCK_sys_init_connect, NULL);
  (void) my_rwlock_init(&LOCK_sys_init_slave, NULL);
  (void) pthread_cond_init(&COND_thread_count,NULL);
  (void) pthread_cond_init(&COND_refresh,NULL);
  (void) pthread_cond_init(&COND_global_read_lock,NULL);
  (void) pthread_cond_init(&COND_thread_cache,NULL);
  (void) pthread_cond_init(&COND_flush_thread_cache,NULL);
  (void) pthread_cond_init(&COND_manager,NULL);
  (void) pthread_mutex_init(&LOCK_rpl_status, MY_MUTEX_INIT_FAST);
  (void) pthread_cond_init(&COND_rpl_status, NULL);

  /* Parameter for threads created for connections */
  (void) pthread_attr_init(&connection_attrib);
  (void) pthread_attr_setdetachstate(&connection_attrib,
				     PTHREAD_CREATE_DETACHED);
  pthread_attr_setscope(&connection_attrib, PTHREAD_SCOPE_SYSTEM);
  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
  {
    struct sched_param tmp_sched_param;

    memset(&tmp_sched_param, 0, sizeof(tmp_sched_param));
    tmp_sched_param.sched_priority= WAIT_PRIOR;
    (void)pthread_attr_setschedparam(&connection_attrib, &tmp_sched_param);
  }

  if (pthread_key_create(&THR_THD,NULL) ||
      pthread_key_create(&THR_MALLOC,NULL))
  {
    sql_print_error("Can't create thread-keys");
    return 1;
  }
  return 0;
}


static int init_server_components()
{
  DBUG_ENTER("init_server_components");
  /*
    We need to call each of these following functions to ensure that
    all things are initialized so that unireg_abort() doesn't fail
  */
  if (table_cache_init() | table_def_init())
    unireg_abort(1);

  randominit(&sql_rand,(ulong) server_start_time,(ulong) server_start_time/2);
  setup_fpu();
  init_thr_lock();
  init_slave_list();

  /* Setup logs */

  /*
    Enable old-fashioned error log, except when the user has requested
    help information. Since the implementation of plugin server
    variables the help output is now written much later.
  */
  if (opt_error_log && !opt_help)
  {
    if (!log_error_file_ptr[0])
      fn_format(log_error_file, pidfile_name, mysql_data_home, ".err",
                MY_REPLACE_EXT); /* replace '.<domain>' by '.err', bug#4997 */
    else
      fn_format(log_error_file, log_error_file_ptr, mysql_data_home, ".err",
                MY_UNPACK_FILENAME | MY_SAFE_PATH);
    if (!log_error_file[0])
      opt_error_log= 1;				// Too long file name
    else
    {
      if (freopen(log_error_file, "a+", stdout))
        freopen(log_error_file, "a+", stderr);
    }
  }

  if (xid_cache_init())
  {
    sql_print_error("Out of memory");
    unireg_abort(1);
  }

  /* need to configure logging before initializing storage engines */
  if (opt_update_log)
  {
    /*
      Update log is removed since 5.0. But we still accept the option.
      The idea is if the user already uses the binlog and the update log,
      we completely ignore any option/variable related to the update log, like
      if the update log did not exist. But if the user uses only the update
      log, then we translate everything into binlog for him (with warnings).
      Implementation of the above :
      - If mysqld is started with --log-update and --log-bin,
      ignore --log-update (print a warning), push a warning when SQL_LOG_UPDATE
      is used, and turn off --sql-bin-update-same.
      This will completely ignore SQL_LOG_UPDATE
      - If mysqld is started with --log-update only,
      change it to --log-bin (with the filename passed to log-update,
      plus '-bin') (print a warning), push a warning when SQL_LOG_UPDATE is
      used, and turn on --sql-bin-update-same.
      This will translate SQL_LOG_UPDATE to SQL_LOG_BIN.

      Note that we tell the user that --sql-bin-update-same is deprecated and
      does nothing, and we don't take into account if he used this option or
      not; but internally we give this variable a value to have the behaviour
      we want (i.e. have SQL_LOG_UPDATE influence SQL_LOG_BIN or not).
      As sql-bin-update-same, log-update and log-bin cannot be changed by the
      user after starting the server (they are not variables), the user will
      not later interfere with the settings we do here.
    */
    if (opt_bin_log)
    {
      opt_sql_bin_update= 0;
      sql_print_error("The update log is no longer supported by MySQL in \
version 5.0 and above. It is replaced by the binary log.");
    }
    else
    {
      opt_sql_bin_update= 1;
      opt_bin_log= 1;
      if (opt_update_logname)
      {
        /* as opt_bin_log==0, no need to free opt_bin_logname */
        if (!(opt_bin_logname= my_strdup(opt_update_logname, MYF(MY_WME))))
          exit(EXIT_OUT_OF_MEMORY);
        sql_print_error("The update log is no longer supported by MySQL in \
version 5.0 and above. It is replaced by the binary log. Now starting MySQL \
with --log-bin='%s' instead.",opt_bin_logname);
      }
      else
        sql_print_error("The update log is no longer supported by MySQL in \
version 5.0 and above. It is replaced by the binary log. Now starting MySQL \
with --log-bin instead.");
    }
  }
  if (opt_log_slave_updates && !opt_bin_log)
  {
    sql_print_error("You need to use --log-bin to make "
                    "--log-slave-updates work.");
    unireg_abort(1);
  }
  if (!opt_bin_log)
    if (opt_binlog_format_id != BINLOG_FORMAT_UNSPEC)
  {
    sql_print_error("You need to use --log-bin to make "
                    "--binlog-format work.");
    unireg_abort(1);
  }
    else
  {
      global_system_variables.binlog_format= BINLOG_FORMAT_MIXED;
    }
  else
    if (opt_binlog_format_id == BINLOG_FORMAT_UNSPEC)
      global_system_variables.binlog_format= BINLOG_FORMAT_MIXED;
    else
    { 
      DBUG_ASSERT(global_system_variables.binlog_format != BINLOG_FORMAT_UNSPEC);
  }

  /* Check that we have not let the format to unspecified at this point */
  DBUG_ASSERT((uint)global_system_variables.binlog_format <=
              array_elements(binlog_format_names)-1);

  if (opt_log_slave_updates && replicate_same_server_id)
  {
    sql_print_error("\
using --replicate-same-server-id in conjunction with \
--log-slave-updates is impossible, it would lead to infinite loops in this \
server.");
    unireg_abort(1);
  }

  if (opt_bin_log)
  {
    char buf[FN_REFLEN];
    const char *ln;
    ln= mysql_bin_log.generate_name(opt_bin_logname, "-bin", 1, buf);
    if (!opt_bin_logname && !opt_binlog_index_name)
    {
      /*
        User didn't give us info to name the binlog index file.
        Picking `hostname`-bin.index like did in 4.x, causes replication to
        fail if the hostname is changed later. So, we would like to instead
        require a name. But as we don't want to break many existing setups, we
        only give warning, not error.
      */
      sql_print_warning("No argument was provided to --log-bin, and "
                        "--log-bin-index was not used; so replication "
                        "may break when this MySQL server acts as a "
                        "master and has his hostname changed!! Please "
                        "use '--log-bin=%s' to avoid this problem.", ln);
    }
    if (ln == buf)
    {
      my_free(opt_bin_logname, MYF(MY_ALLOW_ZERO_PTR));
      opt_bin_logname=my_strdup(buf, MYF(0));
    }
    if (mysql_bin_log.open_index_file(opt_binlog_index_name, ln))
    {
      unireg_abort(1);
    }

    /*
      Used to specify which type of lock we need to use for queries of type
      INSERT ... SELECT. This will change when we have row level logging.
    */
    using_update_log=1;
  }

  /* call ha_init_key_cache() on all key caches to init them */
  process_key_caches(&ha_init_key_cache);

  /* Allow storage engine to give real error messages */
  if (ha_init_errors())
    DBUG_RETURN(1);

  if (plugin_init(&defaults_argc, defaults_argv,
                  (opt_noacl ? PLUGIN_INIT_SKIP_PLUGIN_TABLE : 0) |
                  (opt_help ? PLUGIN_INIT_SKIP_INITIALIZATION : 0)))
  {
    sql_print_error("Failed to initialize plugins.");
    unireg_abort(1);
  }

  if (opt_help)
    unireg_abort(0);

  /* we do want to exit if there are any other unknown options */
  if (defaults_argc > 1)
  {
    int ho_error;
    char **tmp_argv= defaults_argv;
    struct my_option no_opts[]=
    {
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
    };
    /*
      We need to eat any 'loose' arguments first before we conclude
      that there are unprocessed options.
      But we need to preserve defaults_argv pointer intact for
      free_defaults() to work. Thus we use a copy here.
    */
    my_getopt_skip_unknown= 0;

    if ((ho_error= handle_options(&defaults_argc, &tmp_argv, no_opts,
                                  mysqld_get_one_option)))
      unireg_abort(ho_error);

    if (defaults_argc)
    {
      fprintf(stderr, "%s: Too many arguments (first extra is '%s').\n"
              "Use --verbose --help to get a list of available options\n",
              my_progname, *tmp_argv);
      unireg_abort(1);
    }
  }

  /* if the errmsg.sys is not loaded, terminate to maintain behaviour */
  if (!errmesg[0][0])
    unireg_abort(1);

  /* We have to initialize the storage engines before CSV logging */
  if (ha_init())
  {
    sql_print_error("Can't init databases");
    unireg_abort(1);
  }

  logger.set_handlers(LOG_FILE, opt_slow_log ? LOG_FILE:LOG_NONE,
                      opt_log ? LOG_FILE:LOG_NONE);

  /*
    Check that the default storage engine is actually available.
  */
  if (default_storage_engine_str)
  {
    LEX_STRING name= { default_storage_engine_str,
                       strlen(default_storage_engine_str) };
    plugin_ref plugin;
    handlerton *hton;
    
    if ((plugin= ha_resolve_by_name(0, &name)))
      hton= plugin_data(plugin, handlerton*);
    else
    {
      sql_print_error("Unknown/unsupported table type: %s",
                      default_storage_engine_str);
      unireg_abort(1);
    }
    if (!ha_storage_engine_is_enabled(hton))
    {
      if (!opt_bootstrap)
      {
        sql_print_error("Default storage engine (%s) is not available",
                        default_storage_engine_str);
        unireg_abort(1);
      }
      DBUG_ASSERT(global_system_variables.table_plugin);
    }
    else
    {
      /*
        Need to unlock as global_system_variables.table_plugin 
        was acquired during plugin_init()
      */
      plugin_unlock(0, global_system_variables.table_plugin);
      global_system_variables.table_plugin= plugin;
    }
  }

  tc_log= (total_ha_2pc > 1 ? (opt_bin_log  ?
                               (TC_LOG *) &mysql_bin_log :
                               (TC_LOG *) &tc_log_mmap) :
           (TC_LOG *) &tc_log_dummy);

  if (tc_log->open(opt_bin_log ? opt_bin_logname : opt_tc_log_file))
  {
    sql_print_error("Can't init tc log");
    unireg_abort(1);
  }

  if (ha_recover(0))
  {
    unireg_abort(1);
  }

  if (opt_bin_log && mysql_bin_log.open(opt_bin_logname, LOG_BIN, 0,
                                        WRITE_CACHE, 0, max_binlog_size, 0))
    unireg_abort(1);

  if (opt_bin_log && expire_logs_days)
  {
    time_t purge_time= server_start_time - expire_logs_days*24*60*60;
    if (purge_time >= 0)
      mysql_bin_log.purge_logs_before_date(purge_time);
  }

  if (opt_myisam_log)
    (void) mi_log(1);

#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  if (locked_in_memory && !getuid())
  {
    if (setreuid((uid_t)-1, 0) == -1)
    {                        // this should never happen
      sql_perror("setreuid");
      unireg_abort(1);
    }
    if (mlockall(MCL_CURRENT))
    {
      if (global_system_variables.log_warnings)
	sql_print_warning("Failed to lock memory. Errno: %d\n",errno);
      locked_in_memory= 0;
    }
    if (user_info)
      set_user(mysqld_user, user_info);
  }
  else
#endif
    locked_in_memory=0;

  ft_init_stopwords();

  init_update_queries();
  DBUG_RETURN(0);
}


static void create_maintenance_thread()
{
  if (flush_time && flush_time != ~(ulong) 0L)
  {
    pthread_t hThread;
    if (pthread_create(&hThread,&connection_attrib,handle_manager,0))
      sql_print_warning("Can't create thread to manage maintenance");
  }
}


int main(int argc, char **argv)
{
  MY_INIT(argv[0]);		// init my_sys library & pthreads
  /* nothing should come before this line ^^^ */

  /* Set signal used to kill MySQL */
#if defined(SIGUSR2)
  thr_kill_signal= thd_lib_detected == THD_LIB_LT ? SIGINT : SIGUSR2;
#else
  thr_kill_signal= SIGINT;
#endif

  /*
    Perform basic logger initialization logger. Should be called after
    MY_INIT, as it initializes mutexes. Log tables are inited later.
  */
  logger.init_base();

#ifdef _CUSTOMSTARTUPCONFIG_
  if (_cust_check_startup())
  {
    / * _cust_check_startup will report startup failure error * /
    exit(1);
  }
#endif

  if (init_common_variables(MYSQL_CONFIG_NAME,
			    argc, argv, load_default_groups))
    unireg_abort(1);				// Will do exit

  init_signals();
  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
  {
    struct sched_param tmp_sched_param;

    memset(&tmp_sched_param, 0, sizeof(tmp_sched_param));
    tmp_sched_param.sched_priority= my_thread_stack_size*2;
    (void)pthread_setschedparam(pthread_self(), SCHED_OTHER, &tmp_sched_param);
  }
  pthread_attr_setstacksize(&connection_attrib,my_thread_stack_size);
#ifdef HAVE_PTHREAD_ATTR_GETSTACKSIZE
  {
    /* Retrieve used stack size;  Needed for checking stack overflows */
    size_t stack_size= 0;
    pthread_attr_getstacksize(&connection_attrib, &stack_size);
    /* We must check if stack_size = 0 as Solaris 2.9 can return 0 here */
    if (stack_size && stack_size < my_thread_stack_size)
    {
      if (global_system_variables.log_warnings)
	sql_print_warning("Asked for %lu thread stack, but got %ld",
			  my_thread_stack_size, (long) stack_size);
      my_thread_stack_size= stack_size;
    }
  }
#endif

  select_thread=pthread_self();
  select_thread_in_use=1;

  /*
    We have enough space for fiddling with the argv, continue
  */
  check_data_home(mysql_real_data_home);
  if (my_setwd(mysql_real_data_home,MYF(MY_WME)) && !opt_help)
    unireg_abort(1);				/* purecov: inspected */
  mysql_data_home= mysql_data_home_buff;
  mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
  mysql_data_home[1]=0;
  mysql_data_home_len= 2;

  if ((user_info= check_user(mysqld_user)))
  {
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
    if (locked_in_memory) // getuid() == 0 here
      set_effective_user(user_info);
    else
#endif
      set_user(mysqld_user, user_info);
  }

  if (opt_bin_log && !server_id)
  {
    server_id= 1;
#ifdef EXTRA_DEBUG
    sql_print_warning("You have enabled the binary log, but you haven't set "
                      "server-id to a non-zero value: we force server id to 1; "
                      "updates will be logged to the binary log, but "
                      "connections from slaves will not be accepted.");
#endif
  }

  if (init_server_components())
    unireg_abort(1);

  network_init();

  /*
   Initialize my_str_malloc() and my_str_free()
  */
  my_str_malloc= &my_str_malloc_mysqld;
  my_str_free= &my_str_free_mysqld;

  /*
    init signals & alarm
    After this we can't quit by a simple unireg_abort
  */
  error_handler_hook= my_message_sql;
  start_signal_handler();				// Creates pidfile

  if (mysql_rm_tmp_tables() || my_tz_init((THD *)0, default_tz_name, opt_bootstrap))
  {
    abort_loop=1;
    select_thread_in_use=0;
    (void) pthread_kill(signal_thread, MYSQL_KILL_SIGNAL);

    if (!opt_bootstrap)
      (void) my_delete(pidfile_name,MYF(MY_WME));	// Not needed anymore

    exit(1);
  }

  udf_init();

  init_status_vars();
  if (opt_bootstrap) /* If running with bootstrap, do not start replication. */
    opt_skip_slave_start= 1;
  /*
    init_slave() must be called after the thread keys are created.
    Some parts of the code (e.g. SHOW STATUS LIKE 'slave_running' and other
    places) assume that active_mi != 0, so let's fail if it's 0 (out of
    memory); a message has already been printed.
  */
  if (init_slave() && !active_mi)
  {
    unireg_abort(1);
  }

  if (opt_bootstrap)
  {
    select_thread_in_use= 0;                    // Allow 'kill' to work
    bootstrap(stdin);
    unireg_abort(bootstrap_error ? 1 : 0);
  }
  if (opt_init_file)
  {
    if (read_init_file(opt_init_file))
      unireg_abort(1);
  }

  create_maintenance_thread();

  sql_print_information(ER(ER_STARTUP),my_progname,server_version,
                        "", mysqld_port, MYSQL_COMPILATION_COMMENT);


  handle_connections_sockets();

  /* (void) pthread_attr_destroy(&connection_attrib); */
  
  DBUG_PRINT("quit",("Exiting main thread"));

#ifdef EXTRA_DEBUG2
  sql_print_error("Before Lock_thread_count");
#endif
  (void) pthread_mutex_lock(&LOCK_thread_count);
  DBUG_PRINT("quit", ("Got thread_count mutex"));
  select_thread_in_use=0;			// For close_connections
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  (void) pthread_cond_broadcast(&COND_thread_count);
#ifdef EXTRA_DEBUG2
  sql_print_error("After lock_thread_count");
#endif

  /* Wait until cleanup is done */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (!ready_to_exit)
    pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  clean_up(1);
  mysqld_exit(0);
}


/**
  Execute all commands from a file. Used by the mysql_install_db script to
  create MySQL privilege tables without having to start a full MySQL server.
*/

static void bootstrap(FILE *file)
{
  DBUG_ENTER("bootstrap");

  THD *thd= new THD;
  thd->bootstrap=1;
  my_net_init(&thd->net,(st_vio*) 0);
  thd->max_client_packet_length= thd->net.max_packet;
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;
  thread_count++;

  bootstrap_file=file;
  if (pthread_create(&thd->real_id,&connection_attrib,handle_bootstrap,
		     (void*) thd))
  {
    sql_print_warning("Can't create thread to handle bootstrap");
    bootstrap_error=-1;
    DBUG_VOID_RETURN;
  }
  /* Wait for thread to die */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    (void) pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  DBUG_VOID_RETURN;
}


static bool read_init_file(char *file_name)
{
  FILE *file;
  DBUG_ENTER("read_init_file");
  DBUG_PRINT("enter",("name: %s",file_name));
  if (!(file=my_fopen(file_name,O_RDONLY,MYF(MY_WME))))
    return(1);
  bootstrap(file);
  (void) my_fclose(file,MYF(MY_WME));
  return 0;
}


/**
  Create new thread to handle incoming connection.

    This function will create new thread to handle the incoming
    connection.  If there are idle cached threads one will be used.
    'thd' will be pushed into 'threads'.

    In single-threaded mode (\#define ONE_THREAD) connection will be
    handled inside this function.

  @param[in,out] thd    Thread handle of future thread.
*/

static void create_new_thread(THD *thd)
{
  DBUG_ENTER("create_new_thread");

  /*
    Don't allow too many connections. We roughly check here that we allow
    only (max_connections + 1) connections.
  */

  pthread_mutex_lock(&LOCK_connection_count);

  if (connection_count >= max_connections + 1 || abort_loop)
  {
    pthread_mutex_unlock(&LOCK_connection_count);

    DBUG_PRINT("error",("Too many connections"));
    close_connection(thd, ER_CON_COUNT_ERROR, 1);
    delete thd;
    DBUG_VOID_RETURN;
  }

  ++connection_count;

  if (connection_count > max_used_connections)
    max_used_connections= connection_count;

  pthread_mutex_unlock(&LOCK_connection_count);

  /* Start a new thread to handle connection. */

  pthread_mutex_lock(&LOCK_thread_count);

  /*
    The initialization of thread_id is done in create_embedded_thd() for
    the embedded library.
    TODO: refactor this to avoid code duplication there
  */
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;

  thread_count++;

  thread_scheduler.add_connection(thd);

  DBUG_VOID_RETURN;
}


#ifdef SIGNALS_DONT_BREAK_READ
inline void kill_broken_server()
{
  /* hack to get around signals ignored in syscalls for problem OS's */
  if ((!opt_disable_networking && ip_sock == INVALID_SOCKET))
  {
    select_thread_in_use = 0;
    /* The following call will never return */
    kill_server((void*) MYSQL_KILL_SIGNAL);
  }
}
#define MAYBE_BROKEN_SYSCALL kill_broken_server();
#else
#define MAYBE_BROKEN_SYSCALL
#endif

	/* Handle new connections and spawn new process to handle them */

void handle_connections_sockets()
{
  my_socket sock,new_sock;
  uint error_count=0;
  uint max_used_connection= (uint)ip_sock+1;
  fd_set readFDs,clientFDs;
  THD *thd;
  struct sockaddr_storage cAddr;
  int ip_flags=0, flags;
  st_vio *vio_tmp;

  FD_ZERO(&clientFDs);
  if (ip_sock != INVALID_SOCKET)
  {
    FD_SET(ip_sock,&clientFDs);
#ifdef HAVE_FCNTL
    ip_flags = fcntl(ip_sock, F_GETFL, 0);
#endif
  }
  DBUG_PRINT("general",("Waiting for connections."));
  MAYBE_BROKEN_SYSCALL;
  while (!abort_loop)
  {
    readFDs=clientFDs;
    if (select((int) max_used_connection,&readFDs,0,0,0) < 0)
    {
      if (socket_errno != SOCKET_EINTR)
      {
	if (!select_errors++ && !abort_loop)	/* purecov: inspected */
	  sql_print_error("mysqld: Got error %d from select",socket_errno); /* purecov: inspected */
      }
      MAYBE_BROKEN_SYSCALL
      continue;
    }
    if (abort_loop)
    {
      MAYBE_BROKEN_SYSCALL;
      break;
    }

    /* Is this a new connection request ? */
    {
      sock = ip_sock;
      flags= ip_flags;
    }

#if !defined(NO_FCNTL_NONBLOCK)
    if (!(test_flags & TEST_BLOCKING))
    {
#if defined(O_NONBLOCK)
      fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#elif defined(O_NDELAY)
      fcntl(sock, F_SETFL, flags | O_NDELAY);
#endif
    }
#endif /* NO_FCNTL_NONBLOCK */
    for (uint retry=0; retry < MAX_ACCEPT_RETRY; retry++)
    {
      size_socket length= sizeof(struct sockaddr_storage);
      new_sock= accept(sock, (struct sockaddr *)(&cAddr),
                       &length);
      if (new_sock != INVALID_SOCKET ||
	  (socket_errno != SOCKET_EINTR && socket_errno != SOCKET_EAGAIN))
	break;
      MAYBE_BROKEN_SYSCALL;
#if !defined(NO_FCNTL_NONBLOCK)
      if (!(test_flags & TEST_BLOCKING))
      {
	if (retry == MAX_ACCEPT_RETRY - 1)
	  fcntl(sock, F_SETFL, flags);		// Try without O_NONBLOCK
      }
#endif
    }
#if !defined(NO_FCNTL_NONBLOCK)
    if (!(test_flags & TEST_BLOCKING))
      fcntl(sock, F_SETFL, flags);
#endif
    if (new_sock == INVALID_SOCKET)
    {
      if ((error_count++ & 255) == 0)		// This can happen often
	sql_perror("Error in accept");
      MAYBE_BROKEN_SYSCALL;
      if (socket_errno == SOCKET_ENFILE || socket_errno == SOCKET_EMFILE)
	sleep(1);				// Give other threads some time
      continue;
    }

    {
      size_socket dummyLen;
      struct sockaddr_storage dummy;
      dummyLen = sizeof(dummy);
      if (  getsockname(new_sock,(struct sockaddr *)&dummy, 
                  (socklen_t *)&dummyLen) < 0  )
      {
	sql_perror("Error on new connection socket");
	(void) shutdown(new_sock, SHUT_RDWR);
	(void) closesocket(new_sock);
	continue;
      }
    }

    /*
    ** Don't allow too many connections
    */

    if (!(thd= new THD))
    {
      (void) shutdown(new_sock, SHUT_RDWR);
      VOID(closesocket(new_sock));
      continue;
    }
    if (!(vio_tmp=vio_new(new_sock, VIO_TYPE_TCPIP, sock == 0)) ||
	my_net_init(&thd->net,vio_tmp))
    {
      /*
        Only delete the temporary vio if we didn't already attach it to the
        NET object. The destructor in THD will delete any initialized net
        structure.
      */
      if (vio_tmp && thd->net.vio != vio_tmp)
        vio_delete(vio_tmp);
      else
      {
	(void) shutdown(new_sock, SHUT_RDWR);
	(void) closesocket(new_sock);
      }
      delete thd;
      continue;
    }

    create_new_thread(thd);
  }
}


/****************************************************************************
  Handle start options
******************************************************************************/

enum options_mysqld
{
  OPT_ISAM_LOG=256,            OPT_SKIP_NEW, 
  OPT_SKIP_GRANT,              OPT_SKIP_LOCK, 
  OPT_ENABLE_LOCK,             OPT_USE_LOCKING,
  OPT_SOCKET,                  OPT_UPDATE_LOG,
  OPT_BIN_LOG,                 
  OPT_BIN_LOG_INDEX,
  OPT_BIND_ADDRESS,            OPT_PID_FILE,
  OPT_SKIP_PRIOR,              OPT_BIG_TABLES,
  OPT_STANDALONE,
  OPT_CONSOLE,                 OPT_LOW_PRIORITY_UPDATES,
  OPT_SHORT_LOG_FORMAT,
  OPT_FLUSH,                   OPT_SAFE,
  OPT_BOOTSTRAP,               OPT_SKIP_SHOW_DB,
  OPT_STORAGE_ENGINE,          OPT_INIT_FILE,
  OPT_DELAY_KEY_WRITE_ALL,     OPT_SLOW_QUERY_LOG,
  OPT_DELAY_KEY_WRITE,	       OPT_CHARSETS_DIR,
  OPT_MASTER_INFO_FILE,
  OPT_MASTER_RETRY_COUNT,      OPT_LOG_TC, OPT_LOG_TC_SIZE,
  OPT_SQL_BIN_UPDATE_SAME,     OPT_REPLICATE_DO_DB,
  OPT_REPLICATE_IGNORE_DB,     OPT_LOG_SLAVE_UPDATES,
  OPT_BINLOG_DO_DB,            OPT_BINLOG_IGNORE_DB,
  OPT_BINLOG_FORMAT,
#ifndef DBUG_OFF
  OPT_BINLOG_SHOW_XID,
#endif
  OPT_BINLOG_ROWS_EVENT_MAX_SIZE, 
  OPT_WANT_CORE,
  OPT_MEMLOCK,                 OPT_MYISAM_RECOVER,
  OPT_REPLICATE_REWRITE_DB,    OPT_SERVER_ID,
  OPT_SKIP_SLAVE_START,
  OPT_REPLICATE_DO_TABLE,
  OPT_REPLICATE_IGNORE_TABLE,  OPT_REPLICATE_WILD_DO_TABLE,
  OPT_REPLICATE_WILD_IGNORE_TABLE, OPT_REPLICATE_SAME_SERVER_ID,
  OPT_DISCONNECT_SLAVE_EVENT_COUNT, OPT_TC_HEURISTIC_RECOVER,
  OPT_ABORT_SLAVE_EVENT_COUNT,
  OPT_LOG_BIN_TRUST_FUNCTION_CREATORS,
  OPT_ENGINE_CONDITION_PUSHDOWN, 
  OPT_TEMP_POOL, OPT_TX_ISOLATION, OPT_COMPLETION_TYPE,
  OPT_SKIP_STACK_TRACE, OPT_SKIP_SYMLINKS,
  OPT_MAX_BINLOG_DUMP_EVENTS, OPT_SPORADIC_BINLOG_DUMP_FAIL,
  OPT_SAFE_USER_CREATE,
  OPT_DO_PSTACK, OPT_REPORT_HOST,
  OPT_REPORT_USER, OPT_REPORT_PASSWORD, OPT_REPORT_PORT,
  OPT_SHOW_SLAVE_AUTH_INFO,
  OPT_SLAVE_LOAD_TMPDIR, OPT_NO_MIX_TYPE,
  OPT_RPL_RECOVERY_RANK,
  OPT_RELAY_LOG, OPT_RELAY_LOG_INDEX, OPT_RELAY_LOG_INFO_FILE,
  OPT_SLAVE_SKIP_ERRORS, OPT_SLAVE_ALLOW_BATCHING, OPT_DES_KEY_FILE, OPT_LOCAL_INFILE,
  OPT_SSL_SSL, OPT_SSL_KEY, OPT_SSL_CERT, OPT_SSL_CA,
  OPT_SSL_CAPATH, OPT_SSL_CIPHER,
  OPT_BACK_LOG, OPT_BINLOG_CACHE_SIZE,
  OPT_CONNECT_TIMEOUT,
  OPT_FLUSH_TIME, OPT_FT_MIN_WORD_LEN, OPT_FT_BOOLEAN_SYNTAX,
  OPT_INTERACTIVE_TIMEOUT, OPT_JOIN_BUFF_SIZE,
  OPT_KEY_BUFFER_SIZE, OPT_KEY_CACHE_BLOCK_SIZE,
  OPT_KEY_CACHE_DIVISION_LIMIT, OPT_KEY_CACHE_AGE_THRESHOLD,
  OPT_LONG_QUERY_TIME,
  OPT_LOWER_CASE_TABLE_NAMES, OPT_MAX_ALLOWED_PACKET,
  OPT_MAX_BINLOG_CACHE_SIZE, OPT_MAX_BINLOG_SIZE,
  OPT_MAX_CONNECTIONS, OPT_MAX_CONNECT_ERRORS,
  OPT_MAX_HEP_TABLE_SIZE,
  OPT_MAX_JOIN_SIZE, OPT_MAX_PREPARED_STMT_COUNT,
  OPT_MAX_RELAY_LOG_SIZE, OPT_MAX_SORT_LENGTH,
  OPT_MAX_SEEKS_FOR_KEY, OPT_MAX_TMP_TABLES, OPT_MAX_USER_CONNECTIONS,
  OPT_MAX_LENGTH_FOR_SORT_DATA,
  OPT_MAX_WRITE_LOCK_COUNT, OPT_BULK_INSERT_BUFFER_SIZE,
  OPT_MAX_ERROR_COUNT, OPT_MULTI_RANGE_COUNT, OPT_MYISAM_DATA_POINTER_SIZE,
  OPT_MYISAM_BLOCK_SIZE, OPT_MYISAM_MAX_EXTRA_SORT_FILE_SIZE,
  OPT_MYISAM_MAX_SORT_FILE_SIZE, OPT_MYISAM_SORT_BUFFER_SIZE,
  OPT_MYISAM_USE_MMAP, OPT_MYISAM_REPAIR_THREADS,
  OPT_MYISAM_STATS_METHOD,
  OPT_NET_BUFFER_LENGTH, OPT_NET_RETRY_COUNT,
  OPT_NET_READ_TIMEOUT, OPT_NET_WRITE_TIMEOUT,
  OPT_OPEN_FILES_LIMIT,
  OPT_PRELOAD_BUFFER_SIZE,
  OPT_QUERY_CACHE_LIMIT, OPT_QUERY_CACHE_MIN_RES_UNIT, OPT_QUERY_CACHE_SIZE,
  OPT_QUERY_CACHE_TYPE, OPT_QUERY_CACHE_WLOCK_INVALIDATE, OPT_RECORD_BUFFER,
  OPT_RECORD_RND_BUFFER, OPT_DIV_PRECINCREMENT, OPT_RELAY_LOG_SPACE_LIMIT,
  OPT_RELAY_LOG_PURGE,
  OPT_SLAVE_NET_TIMEOUT, OPT_SLAVE_COMPRESSED_PROTOCOL, OPT_SLOW_LAUNCH_TIME,
  OPT_SLAVE_TRANS_RETRIES, OPT_READONLY, OPT_DEBUGGING,
  OPT_SORT_BUFFER, OPT_TABLE_OPEN_CACHE, OPT_TABLE_DEF_CACHE,
  OPT_THREAD_CONCURRENCY, OPT_THREAD_CACHE_SIZE,
  OPT_TMP_TABLE_SIZE, OPT_THREAD_STACK,
  OPT_WAIT_TIMEOUT,
  OPT_ERROR_LOG_FILE,
  OPT_DEFAULT_WEEK_FORMAT,
  OPT_RANGE_ALLOC_BLOCK_SIZE, OPT_ALLOW_SUSPICIOUS_UDFS,
  OPT_QUERY_ALLOC_BLOCK_SIZE, OPT_QUERY_PREALLOC_SIZE,
  OPT_TRANS_ALLOC_BLOCK_SIZE, OPT_TRANS_PREALLOC_SIZE,
  OPT_SYNC_FRM, OPT_SYNC_BINLOG,
  OPT_SYNC_REPLICATION,
  OPT_SYNC_REPLICATION_SLAVE_ID,
  OPT_SYNC_REPLICATION_TIMEOUT,
  OPT_ENABLE_SHARED_MEMORY,
  OPT_SHARED_MEMORY_BASE_NAME,
  OPT_OLD_PASSWORDS,
  OPT_OLD_ALTER_TABLE,
  OPT_EXPIRE_LOGS_DAYS,
  OPT_GROUP_CONCAT_MAX_LEN,
  OPT_DEFAULT_COLLATION,
  OPT_CHARACTER_SET_CLIENT_HANDSHAKE,
  OPT_CHARACTER_SET_FILESYSTEM,
  OPT_LC_TIME_NAMES,
  OPT_INIT_CONNECT,
  OPT_INIT_SLAVE,
  OPT_SECURE_AUTH,
  OPT_DATE_FORMAT,
  OPT_TIME_FORMAT,
  OPT_DATETIME_FORMAT,
  OPT_LOG_QUERIES_NOT_USING_INDEXES,
  OPT_DEFAULT_TIME_ZONE,
  OPT_SYSDATE_IS_NOW,
  OPT_OPTIMIZER_SEARCH_DEPTH,
  OPT_OPTIMIZER_PRUNE_LEVEL,
  OPT_UPDATABLE_VIEWS_WITH_LIMIT,
  OPT_AUTO_INCREMENT, OPT_AUTO_INCREMENT_OFFSET,
  OPT_ENABLE_LARGE_PAGES,
  OPT_TIMED_MUTEXES,
  OPT_OLD_STYLE_USER_LIMITS,
  OPT_LOG_SLOW_ADMIN_STATEMENTS,
  OPT_TABLE_LOCK_WAIT_TIMEOUT,
  OPT_PLUGIN_LOAD,
  OPT_PLUGIN_DIR,
  OPT_LOG_OUTPUT,
  OPT_PORT_OPEN_TIMEOUT,
  OPT_PROFILING,
  OPT_KEEP_FILES_ON_CREATE,
  OPT_GENERAL_LOG,
  OPT_SLOW_LOG,
  OPT_THREAD_HANDLING,
  OPT_INNODB_ROLLBACK_ON_TIMEOUT,
  OPT_SECURE_FILE_PRIV,
  OPT_MIN_EXAMINED_ROW_LIMIT,
  OPT_LOG_SLOW_SLAVE_STATEMENTS,
  OPT_OLD_MODE,
  OPT_POOL_OF_THREADS,
  OPT_SLAVE_EXEC_MODE
};


#define LONG_TIMEOUT ((ulong) 3600L*24L*365L)

struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 
   (uchar**) &opt_help, (uchar**) &opt_help, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"abort-slave-event-count", OPT_ABORT_SLAVE_EVENT_COUNT,
   "Option used by mysql-test for debugging and testing of replication.",
   (uchar**) &abort_slave_event_count,  (uchar**) &abort_slave_event_count,
   0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"allow-suspicious-udfs", OPT_ALLOW_SUSPICIOUS_UDFS,
   "Allows use of UDFs consisting of only one symbol xxx() "
   "without corresponding xxx_init() or xxx_deinit(). That also means "
   "that one can load any function from any library, for example exit() "
   "from libc.so",
   (uchar**) &opt_allow_suspicious_udfs, (uchar**) &opt_allow_suspicious_udfs,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"ansi", 'a', "Use ANSI SQL syntax instead of MySQL syntax. This mode will also set transaction isolation level 'serializable'.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-increment-increment", OPT_AUTO_INCREMENT,
   "Auto-increment columns are incremented by this",
   (uchar**) &global_system_variables.auto_increment_increment,
   (uchar**) &max_system_variables.auto_increment_increment, 0, GET_ULONG,
   OPT_ARG, 1, 1, 65535, 0, 1, 0 },
  {"auto-increment-offset", OPT_AUTO_INCREMENT_OFFSET,
   "Offset added to Auto-increment columns. Used when auto-increment-increment != 1",
   (uchar**) &global_system_variables.auto_increment_offset,
   (uchar**) &max_system_variables.auto_increment_offset, 0, GET_ULONG, OPT_ARG,
   1, 1, 65535, 0, 1, 0 },
  {"basedir", 'b',
   "Path to installation directory. All paths are usually resolved relative to this.",
   (uchar**) &mysql_home_ptr, (uchar**) &mysql_home_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"big-tables", OPT_BIG_TABLES,
   "Allow big result sets by saving all temporary sets on file (Solves most 'table full' errors).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"bind-address", OPT_BIND_ADDRESS, "IP address to bind to.",
   (uchar**) &my_bind_addr_str, (uchar**) &my_bind_addr_str, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog_format", OPT_BINLOG_FORMAT,
   "Does not have any effect without '--log-bin'. "
   "Tell the master the form of binary logging to use: either 'row' for "
   "row-based binary logging, or 'statement' for statement-based binary "
   "logging, or 'mixed'. 'mixed' is statement-based binary logging except "
   "for those statements where only row-based is correct: those which "
   "involve user-defined functions (i.e. UDFs) or the UUID() function; for "
   "those, row-based binary logging is automatically used. "
   ,(uchar**) &opt_binlog_format, (uchar**) &opt_binlog_format,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-do-db", OPT_BINLOG_DO_DB,
   "Tells the master it should log updates for the specified database, and exclude all others not explicitly mentioned.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-ignore-db", OPT_BINLOG_IGNORE_DB,
   "Tells the master that updates to the given database should not be logged tothe binary log.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-row-event-max-size", OPT_BINLOG_ROWS_EVENT_MAX_SIZE,
   "The maximum size of a row-based binary log event in bytes. Rows will be "
   "grouped into events smaller than this size if possible. "
   "The value has to be a multiple of 256.",
   (uchar**) &opt_binlog_rows_event_max_size, 
   (uchar**) &opt_binlog_rows_event_max_size, 0, 
   GET_ULONG, REQUIRED_ARG, 
   /* def_value */ 1024, /* min_value */  256, /* max_value */ ULONG_MAX, 
   /* sub_size */     0, /* block_size */ 256, 
   /* app_type */ 0
  },
#ifndef DISABLE_GRANT_OPTIONS
  {"bootstrap", OPT_BOOTSTRAP, "Used by mysql installation scripts.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"character-set-client-handshake", OPT_CHARACTER_SET_CLIENT_HANDSHAKE,
   "Don't ignore client side character set value sent during handshake.",
   (uchar**) &opt_character_set_client_handshake,
   (uchar**) &opt_character_set_client_handshake,
    0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"character-set-filesystem", OPT_CHARACTER_SET_FILESYSTEM,
   "Set the filesystem character set.",
   (uchar**) &character_set_filesystem_name,
   (uchar**) &character_set_filesystem_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"character-set-server", 'C', "Set the default character set.",
   (uchar**) &default_character_set_name, (uchar**) &default_character_set_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.", (uchar**) &charsets_dir,
   (uchar**) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"chroot", 'r', "Chroot mysqld daemon during startup.",
   (uchar**) &mysqld_chroot, (uchar**) &mysqld_chroot, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"collation-server", OPT_DEFAULT_COLLATION, "Set the default collation.",
   (uchar**) &default_collation_name, (uchar**) &default_collation_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"completion-type", OPT_COMPLETION_TYPE, "Default completion type.",
   (uchar**) &global_system_variables.completion_type,
   (uchar**) &max_system_variables.completion_type, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 2, 0, 1, 0},
  {"console", OPT_CONSOLE, "Write error output on screen; Don't remove the console window on windows.",
   (uchar**) &opt_console, (uchar**) &opt_console, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"core-file", OPT_WANT_CORE, "Write core on errors.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'h', "Path to the database root.", (uchar**) &mysql_data_home,
   (uchar**) &mysql_data_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Debug log.", (uchar**) &default_dbug_option,
   (uchar**) &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"default-character-set", 'C', "Set the default character set (deprecated option, use --character-set-server instead).",
   (uchar**) &default_character_set_name, (uchar**) &default_character_set_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"default-collation", OPT_DEFAULT_COLLATION, "Set the default collation (deprecated option, use --collation-server instead).",
   (uchar**) &default_collation_name, (uchar**) &default_collation_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"default-storage-engine", OPT_STORAGE_ENGINE,
   "Set the default storage engine (table type) for tables.",
   (uchar**)&default_storage_engine_str, (uchar**)&default_storage_engine_str,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-table-type", OPT_STORAGE_ENGINE,
   "(deprecated) Use --default-storage-engine.",
   (uchar**)&default_storage_engine_str, (uchar**)&default_storage_engine_str,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-time-zone", OPT_DEFAULT_TIME_ZONE, "Set the default time zone.",
   (uchar**) &default_tz_name, (uchar**) &default_tz_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"delay-key-write", OPT_DELAY_KEY_WRITE, "Type of DELAY_KEY_WRITE.",
   0,0,0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"delay-key-write-for-all-tables", OPT_DELAY_KEY_WRITE_ALL,
   "Don't flush key buffers between writes for any MyISAM table (Deprecated option, use --delay-key-write=all instead).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"disconnect-slave-event-count", OPT_DISCONNECT_SLAVE_EVENT_COUNT,
   "Option used by mysql-test for debugging and testing of replication.",
   (uchar**) &disconnect_slave_event_count,
   (uchar**) &disconnect_slave_event_count, 0, GET_INT, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
#ifdef HAVE_STACK_TRACE_ON_SEGV
  {"enable-pstack", OPT_DO_PSTACK, "Print a symbolic stack trace on failure.",
   (uchar**) &opt_do_pstack, (uchar**) &opt_do_pstack, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
#endif /* HAVE_STACK_TRACE_ON_SEGV */
  {"engine-condition-pushdown",
   OPT_ENGINE_CONDITION_PUSHDOWN,
   "Push supported query conditions to the storage engine.",
   (uchar**) &global_system_variables.engine_condition_pushdown,
   (uchar**) &global_system_variables.engine_condition_pushdown,
   0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  /* See how it's handled in get_one_option() */
  {"exit-info", 'T', "Used for debugging;  Use at your own risk!", 0, 0, 0,
   GET_LONG, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"flush", OPT_FLUSH, "Flush tables to disk between SQL commands.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  /* We must always support the next option to make scripts like mysqltest
     easier to do */
  {"gdb", OPT_DEBUGGING,
   "Set up signals usable for debugging",
   (uchar**) &opt_debugging, (uchar**) &opt_debugging,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"general-log", OPT_GENERAL_LOG,
   "Enable|disable general log", (uchar**) &opt_log,
   (uchar**) &opt_log, 0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"init-connect", OPT_INIT_CONNECT, "Command(s) that are executed for each new connection",
   (uchar**) &opt_init_connect, (uchar**) &opt_init_connect, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"init-file", OPT_INIT_FILE, "Read SQL commands from this file at startup.",
   (uchar**) &opt_init_file, (uchar**) &opt_init_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"init-slave", OPT_INIT_SLAVE, "Command(s) that are executed when a slave connects to this master",
   (uchar**) &opt_init_slave, (uchar**) &opt_init_slave, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"language", 'L',
   "Client error messages in given language. May be given as a full path.",
   (uchar**) &language_ptr, (uchar**) &language_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"lc-time-names", OPT_LC_TIME_NAMES,
   "Set the language used for the month names and the days of the week.",
   (uchar**) &lc_time_names_name,
   (uchar**) &lc_time_names_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"local-infile", OPT_LOCAL_INFILE,
   "Enable/disable LOAD DATA LOCAL INFILE (takes values 1|0).",
   (uchar**) &opt_local_infile,
   (uchar**) &opt_local_infile, 0, GET_BOOL, OPT_ARG,
   1, 0, 0, 0, 0, 0},
  {"log", 'l', "Log connections and queries to file.", (uchar**) &opt_logname,
   (uchar**) &opt_logname, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-bin", OPT_BIN_LOG,
   "Log update queries in binary format. Optional (but strongly recommended "
   "to avoid replication problems if server's hostname changes) argument "
   "should be the chosen location for the binary log files.",
   (uchar**) &opt_bin_logname, (uchar**) &opt_bin_logname, 0, GET_STR_ALLOC,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-bin-index", OPT_BIN_LOG_INDEX,
   "File that holds the names for last binary log files.",
   (uchar**) &opt_binlog_index_name, (uchar**) &opt_binlog_index_name, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  /*
    This option starts with "log-bin" to emphasize that it is specific of
    binary logging.
  */
  {"log-bin-trust-function-creators", OPT_LOG_BIN_TRUST_FUNCTION_CREATORS,
   "If equal to 0 (the default), then when --log-bin is used, creation of "
   "a stored function (or trigger) is allowed only to users having the SUPER privilege "
   "and only if this stored function (trigger) may not break binary logging."
   "Note that if ALL connections to this server ALWAYS use row-based binary "
   "logging, the security issues do not exist and the binary logging cannot "
   "break, so you can safely set this to 1."
   ,(uchar**) &trust_function_creators, (uchar**) &trust_function_creators, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-error", OPT_ERROR_LOG_FILE, "Error log file.",
   (uchar**) &log_error_file_ptr, (uchar**) &log_error_file_ptr, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-isam", OPT_ISAM_LOG, "Log all MyISAM changes to file.",
   (uchar**) &myisam_log_filename, (uchar**) &myisam_log_filename, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-long-format", '0',
   "Log some extra information to update log. Please note that this option is deprecated; see --log-short-format option.", 
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef WITH_CSV_STORAGE_ENGINE
  {"log-output", OPT_LOG_OUTPUT,
   "Syntax: log-output[=value[,value...]], where \"value\" could be TABLE, "
   "FILE or NONE.",
   (uchar**) &log_output_str, (uchar**) &log_output_str, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"log-queries-not-using-indexes", OPT_LOG_QUERIES_NOT_USING_INDEXES,
   "Log queries that are executed without benefit of any index to the slow log if it is open.",
   (uchar**) &opt_log_queries_not_using_indexes, (uchar**) &opt_log_queries_not_using_indexes,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-short-format", OPT_SHORT_LOG_FORMAT,
   "Don't log extra information to update and slow-query logs.",
   (uchar**) &opt_short_log_format, (uchar**) &opt_short_log_format,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slave-updates", OPT_LOG_SLAVE_UPDATES,
   "Tells the slave to log the updates from the slave thread to the binary log. You will need to turn it on if you plan to daisy-chain the slaves.",
   (uchar**) &opt_log_slave_updates, (uchar**) &opt_log_slave_updates, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slow-admin-statements", OPT_LOG_SLOW_ADMIN_STATEMENTS,
   "Log slow OPTIMIZE, ANALYZE, ALTER and other administrative statements to the slow log if it is open.",
   (uchar**) &opt_log_slow_admin_statements,
   (uchar**) &opt_log_slow_admin_statements,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
 {"log-slow-slave-statements", OPT_LOG_SLOW_SLAVE_STATEMENTS,
  "Log slow statements executed by slave thread to the slow log if it is open.",
  (uchar**) &opt_log_slow_slave_statements,
  (uchar**) &opt_log_slow_slave_statements,
  0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-slow-queries", OPT_SLOW_QUERY_LOG,
    "Log slow queries to a table or log file. Defaults logging to table mysql.slow_log or hostname-slow.log if --log-output=file is used. Must be enabled to activate other slow log options.",
   (uchar**) &opt_slow_logname, (uchar**) &opt_slow_logname, 0, GET_STR, OPT_ARG,
   0, 0, 0, 0, 0, 0},
  {"log-tc", OPT_LOG_TC,
   "Path to transaction coordinator log (used for transactions that affect "
   "more than one storage engine, when binary log is disabled)",
   (uchar**) &opt_tc_log_file, (uchar**) &opt_tc_log_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_MMAP
  {"log-tc-size", OPT_LOG_TC_SIZE, "Size of transaction coordinator log.",
   (uchar**) &opt_tc_log_size, (uchar**) &opt_tc_log_size, 0, GET_ULONG,
   REQUIRED_ARG, TC_LOG_MIN_SIZE, TC_LOG_MIN_SIZE, ULONG_MAX, 0,
   TC_LOG_PAGE_SIZE, 0},
#endif
  {"log-update", OPT_UPDATE_LOG,
   "The update log is deprecated since version 5.0, is replaced by the binary \
log and this option justs turns on --log-bin instead.",
   (uchar**) &opt_update_logname, (uchar**) &opt_update_logname, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-warnings", 'W', "Log some not critical warnings to the log file.",
   (uchar**) &global_system_variables.log_warnings,
   (uchar**) &max_system_variables.log_warnings, 0, GET_ULONG, OPT_ARG, 1, 0, 0,
   0, 0, 0},
  {"low-priority-updates", OPT_LOW_PRIORITY_UPDATES,
   "INSERT/DELETE/UPDATE has lower priority than selects.",
   (uchar**) &global_system_variables.low_priority_updates,
   (uchar**) &max_system_variables.low_priority_updates,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"master-info-file", OPT_MASTER_INFO_FILE,
   "The location and name of the file that remembers the master and where the I/O replication \
thread is in the master's binlogs.",
   (uchar**) &master_info_file, (uchar**) &master_info_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"master-retry-count", OPT_MASTER_RETRY_COUNT,
   "The number of tries the slave will make to connect to the master before giving up.",
   (uchar**) &master_retry_count, (uchar**) &master_retry_count, 0, GET_ULONG,
   REQUIRED_ARG, 3600*24, 0, 0, 0, 0, 0},
  {"max-binlog-dump-events", OPT_MAX_BINLOG_DUMP_EVENTS,
   "Option used by mysql-test for debugging and testing of replication.",
   (uchar**) &max_binlog_dump_events, (uchar**) &max_binlog_dump_events, 0,
   GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"memlock", OPT_MEMLOCK, "Lock mysqld in memory.", (uchar**) &locked_in_memory,
   (uchar**) &locked_in_memory, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"myisam-recover", OPT_MYISAM_RECOVER,
   "Syntax: myisam-recover[=option[,option...]], where option can be DEFAULT, BACKUP, FORCE or QUICK.",
   (uchar**) &myisam_recover_options_str, (uchar**) &myisam_recover_options_str, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"new", 'n', "Use very new possible 'unsafe' functions.",
   (uchar**) &global_system_variables.new_mode,
   (uchar**) &max_system_variables.new_mode,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"old-alter-table", OPT_OLD_ALTER_TABLE,
   "Use old, non-optimized alter table.",
   (uchar**) &global_system_variables.old_alter_table,
   (uchar**) &max_system_variables.old_alter_table, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"old-passwords", OPT_OLD_PASSWORDS, "Use old password encryption method (needed for 4.0 and older clients).",
   (uchar**) &global_system_variables.old_passwords,
   (uchar**) &max_system_variables.old_passwords, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"old-style-user-limits", OPT_OLD_STYLE_USER_LIMITS,
   "Enable old-style user limits (before 5.0.3 user resources were counted per each user+host vs. per account)",
   (uchar**) &opt_old_style_user_limits, (uchar**) &opt_old_style_user_limits,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"pid-file", OPT_PID_FILE, "Pid file used by safe_mysqld.",
   (uchar**) &pidfile_name_ptr, (uchar**) &pidfile_name_ptr, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   (uchar**) &mysqld_port,
   (uchar**) &mysqld_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port-open-timeout", OPT_PORT_OPEN_TIMEOUT,
   "Maximum time in seconds to wait for the port to become free. "
   "(Default: no wait)", (uchar**) &mysqld_port_timeout,
   (uchar**) &mysqld_port_timeout, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log", OPT_RELAY_LOG,
   "The location and name to use for relay logs.",
   (uchar**) &opt_relay_logname, (uchar**) &opt_relay_logname, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log-index", OPT_RELAY_LOG_INDEX,
   "The location and name to use for the file that keeps a list of the last \
relay logs.",
   (uchar**) &opt_relaylog_index_name, (uchar**) &opt_relaylog_index_name, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"relay-log-info-file", OPT_RELAY_LOG_INFO_FILE,
   "The location and name of the file that remembers where the SQL replication \
thread is in the relay logs.",
   (uchar**) &relay_log_info_file, (uchar**) &relay_log_info_file, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-do-db", OPT_REPLICATE_DO_DB,
   "Tells the slave thread to restrict replication to the specified database. To specify more than one database, use the directive multiple times, once for each database. Note that this will only work if you do not use cross-database queries such as UPDATE some_db.some_table SET foo='bar' while having selected a different or no database. If you need cross database updates to work, make sure you have 3.23.28 or later, and use replicate-wild-do-table=db_name.%.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-do-table", OPT_REPLICATE_DO_TABLE,
   "Tells the slave thread to restrict replication to the specified table. To specify more than one table, use the directive multiple times, once for each table. This will work for cross-database updates, in contrast to replicate-do-db.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-ignore-db", OPT_REPLICATE_IGNORE_DB,
   "Tells the slave thread to not replicate to the specified database. To specify more than one database to ignore, use the directive multiple times, once for each database. This option will not work if you use cross database updates. If you need cross database updates to work, make sure you have 3.23.28 or later, and use replicate-wild-ignore-table=db_name.%. ",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-ignore-table", OPT_REPLICATE_IGNORE_TABLE,
   "Tells the slave thread to not replicate to the specified table. To specify more than one table to ignore, use the directive multiple times, once for each table. This will work for cross-datbase updates, in contrast to replicate-ignore-db.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-rewrite-db", OPT_REPLICATE_REWRITE_DB,
   "Updates to a database with a different name than the original. Example: replicate-rewrite-db=master_db_name->slave_db_name.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-same-server-id", OPT_REPLICATE_SAME_SERVER_ID,
   "In replication, if set to 1, do not skip events having our server id. \
Default value is 0 (to break infinite loops in circular replication). \
Can't be set to 1 if --log-slave-updates is used.",
   (uchar**) &replicate_same_server_id,
   (uchar**) &replicate_same_server_id,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-wild-do-table", OPT_REPLICATE_WILD_DO_TABLE,
   "Tells the slave thread to restrict replication to the tables that match the specified wildcard pattern. To specify more than one table, use the directive multiple times, once for each table. This will work for cross-database updates. Example: replicate-wild-do-table=foo%.bar% will replicate only updates to tables in all databases that start with foo and whose table names start with bar.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"replicate-wild-ignore-table", OPT_REPLICATE_WILD_IGNORE_TABLE,
   "Tells the slave thread to not replicate to the tables that match the given wildcard pattern. To specify more than one table to ignore, use the directive multiple times, once for each table. This will work for cross-database updates. Example: replicate-wild-ignore-table=foo%.bar% will not do updates to tables in databases that start with foo and whose table names start with bar.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  // In replication, we may need to tell the other servers how to connect
  {"report-host", OPT_REPORT_HOST,
   "Hostname or IP of the slave to be reported to to the master during slave registration. Will appear in the output of SHOW SLAVE HOSTS. Leave unset if you do not want the slave to register itself with the master. Note that it is not sufficient for the master to simply read the IP of the slave off the socket once the slave connects. Due to NAT and other routing issues, that IP may not be valid for connecting to the slave from the master or other hosts.",
   (uchar**) &report_host, (uchar**) &report_host, 0, GET_STR, REQUIRED_ARG, 0, 0,
   0, 0, 0, 0},
  {"report-password", OPT_REPORT_PASSWORD, "Undocumented.",
   (uchar**) &report_password, (uchar**) &report_password, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"report-port", OPT_REPORT_PORT,
   "Port for connecting to slave reported to the master during slave registration. Set it only if the slave is listening on a non-default port or if you have a special tunnel from the master or other clients to the slave. If not sure, leave this option unset.",
   (uchar**) &report_port, (uchar**) &report_port, 0, GET_UINT, REQUIRED_ARG,
   MYSQL_PORT, 0, 0, 0, 0, 0},
  {"safe-mode", OPT_SAFE, "Skip some optimize stages (for testing).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"secure-file-priv", OPT_SECURE_FILE_PRIV,
   "Limit LOAD DATA, SELECT ... OUTFILE, and LOAD_FILE() to files within specified directory",
   (uchar**) &opt_secure_file_priv, (uchar**) &opt_secure_file_priv, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-id",	OPT_SERVER_ID,
   "Uniquely identifies the server instance in the community of replication partners.",
   (uchar**) &server_id, (uchar**) &server_id, 0, GET_ULONG, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"set-variable", 'O',
   "Change the value of a variable. Please note that this option is deprecated;you can set variables directly with --variable-name=value.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-new", OPT_SKIP_NEW, "Don't use new, possible wrong routines.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-slave-start", OPT_SKIP_SLAVE_START,
   "If set, slave is not autostarted.", (uchar**) &opt_skip_slave_start,
   (uchar**) &opt_skip_slave_start, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-stack-trace", OPT_SKIP_STACK_TRACE,
   "Don't print a stack trace on failure.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"skip-symlink", OPT_SKIP_SYMLINKS, "Don't allow symlinking of tables. Deprecated option.  Use --skip-symbolic-links instead.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-thread-priority", OPT_SKIP_PRIOR,
   "Don't give threads different priorities.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   DEFAULT_SKIP_THREAD_PRIORITY, 0, 0, 0, 0, 0},
  {"slave-load-tmpdir", OPT_SLAVE_LOAD_TMPDIR,
   "The location where the slave should put its temporary files when \
replicating a LOAD DATA INFILE command.",
   (uchar**) &slave_load_tmpdir, (uchar**) &slave_load_tmpdir, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"slave-skip-errors", OPT_SLAVE_SKIP_ERRORS,
   "Tells the slave thread to continue replication when a query event returns an error from the provided list.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"slave-exec-mode", OPT_SLAVE_EXEC_MODE,
   "Modes for how replication events should be executed.  Legal values are STRICT (default) and IDEMPOTENT. In IDEMPOTENT mode, replication will not stop for operations that are idempotent. In STRICT mode, replication will stop on any unexpected difference between the master and the slave.",
   (uchar**) &slave_exec_mode_str, (uchar**) &slave_exec_mode_str, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"slow-query-log", OPT_SLOW_LOG,
   "Enable|disable slow query log", (uchar**) &opt_slow_log,
   (uchar**) &opt_slow_log, 0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"sporadic-binlog-dump-fail", OPT_SPORADIC_BINLOG_DUMP_FAIL,
   "Option used by mysql-test for debugging and testing of replication.",
   (uchar**) &opt_sporadic_binlog_dump_fail,
   (uchar**) &opt_sporadic_binlog_dump_fail, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"sql-bin-update-same", OPT_SQL_BIN_UPDATE_SAME,
   "The update log is deprecated since version 5.0, is replaced by the binary \
log and this option does nothing anymore.",
   0, 0, 0, GET_DISABLED, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"symbolic-links", 's', "Enable symbolic link support.",
   (uchar**) &my_use_symdir, (uchar**) &my_use_symdir, 0, GET_BOOL, NO_ARG,
   /*
     The system call realpath() produces warnings under valgrind and
     purify. These are not suppressed: instead we disable symlinks
     option if compiled with valgrind support.
   */
   IF_PURIFY(0,1), 0, 0, 0, 0, 0},
  {"sysdate-is-now", OPT_SYSDATE_IS_NOW,
   "Non-default option to alias SYSDATE() to NOW() to make it safe-replicable. Since 5.0, SYSDATE() returns a `dynamic' value different for different invocations, even within the same statement.",
   (uchar**) &global_system_variables.sysdate_is_now,
   0, 0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
  {"tc-heuristic-recover", OPT_TC_HEURISTIC_RECOVER,
   "Decision to use in heuristic recover process. Possible values are COMMIT or ROLLBACK.",
   (uchar**) &opt_tc_heuristic_recover, (uchar**) &opt_tc_heuristic_recover,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"temp-pool", OPT_TEMP_POOL,
   "Using this option will cause most temporary files created to use a small set of names, rather than a unique name for each new file.",
   (uchar**) &use_temp_pool, (uchar**) &use_temp_pool, 0, GET_BOOL, NO_ARG, 1,
   0, 0, 0, 0, 0},
  {"timed_mutexes", OPT_TIMED_MUTEXES,
   "Specify whether to time mutexes (only InnoDB mutexes are currently supported)",
   (uchar**) &timed_mutexes, (uchar**) &timed_mutexes, 0, GET_BOOL, NO_ARG, 0, 
    0, 0, 0, 0, 0},
  {"tmpdir", 't',
   "Path for temporary files. Several paths may be specified, separated by a "
   "colon (:)"
   ", in this case they are used in a round-robin fashion.",
   (uchar**) &opt_mysql_tmpdir,
   (uchar**) &opt_mysql_tmpdir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"transaction-isolation", OPT_TX_ISOLATION,
   "Default transaction isolation level.", 0, 0, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"use-symbolic-links", 's', "Enable symbolic link support. Deprecated option; use --symbolic-links instead.",
   (uchar**) &my_use_symdir, (uchar**) &my_use_symdir, 0, GET_BOOL, NO_ARG,
   IF_PURIFY(0,1), 0, 0, 0, 0, 0},
  {"user", 'u', "Run mysqld daemon as user.", 0, 0, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Used with --help option for detailed help",
   (uchar**) &opt_verbose, (uchar**) &opt_verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"warnings", 'W', "Deprecated; use --log-warnings instead.",
   (uchar**) &global_system_variables.log_warnings,
   (uchar**) &max_system_variables.log_warnings, 0, GET_ULONG, OPT_ARG,
   1, 0, ULONG_MAX, 0, 0, 0},
  { "back_log", OPT_BACK_LOG,
    "The number of outstanding connection requests MySQL can have. This comes into play when the main MySQL thread gets very many connection requests in a very short time.",
    (uchar**) &back_log, (uchar**) &back_log, 0, GET_ULONG,
    REQUIRED_ARG, 50, 1, 65535, 0, 1, 0 },
  {"binlog_cache_size", OPT_BINLOG_CACHE_SIZE,
   "The size of the cache to hold the SQL statements for the binary log during a transaction. If you often use big, multi-statement transactions you can increase this to get more performance.",
   (uchar**) &binlog_cache_size, (uchar**) &binlog_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 32*1024L, IO_SIZE, ULONG_MAX, 0, IO_SIZE, 0},
  {"bulk_insert_buffer_size", OPT_BULK_INSERT_BUFFER_SIZE,
   "Size of tree cache used in bulk insert optimisation. Note that this is a limit per thread!",
   (uchar**) &global_system_variables.bulk_insert_buff_size,
   (uchar**) &max_system_variables.bulk_insert_buff_size,
   0, GET_ULONG, REQUIRED_ARG, 8192*1024, 0, ULONG_MAX, 0, 1, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT,
   "The number of seconds the mysqld server is waiting for a connect packet before responding with 'Bad handshake'.",
    (uchar**) &connect_timeout, (uchar**) &connect_timeout,
   0, GET_ULONG, REQUIRED_ARG, CONNECT_TIMEOUT, 2, LONG_TIMEOUT, 0, 1, 0 },
  { "date_format", OPT_DATE_FORMAT,
    "The DATE format (For future).",
    (uchar**) &opt_date_time_formats[MYSQL_TIMESTAMP_DATE],
    (uchar**) &opt_date_time_formats[MYSQL_TIMESTAMP_DATE],
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "datetime_format", OPT_DATETIME_FORMAT,
    "The DATETIME/TIMESTAMP format (for future).",
    (uchar**) &opt_date_time_formats[MYSQL_TIMESTAMP_DATETIME],
    (uchar**) &opt_date_time_formats[MYSQL_TIMESTAMP_DATETIME],
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "default_week_format", OPT_DEFAULT_WEEK_FORMAT,
    "The default week format used by WEEK() functions.",
    (uchar**) &global_system_variables.default_week_format,
    (uchar**) &max_system_variables.default_week_format,
    0, GET_ULONG, REQUIRED_ARG, 0, 0, 7L, 0, 1, 0},
  {"div_precision_increment", OPT_DIV_PRECINCREMENT,
   "Precision of the result of '/' operator will be increased on that value.",
   (uchar**) &global_system_variables.div_precincrement,
   (uchar**) &max_system_variables.div_precincrement, 0, GET_ULONG,
   REQUIRED_ARG, 4, 0, DECIMAL_MAX_SCALE, 0, 0, 0},
  {"expire_logs_days", OPT_EXPIRE_LOGS_DAYS,
   "If non-zero, binary logs will be purged after expire_logs_days "
   "days; possible purges happen at startup and at binary log rotation.",
   (uchar**) &expire_logs_days,
   (uchar**) &expire_logs_days, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 99, 0, 1, 0},
  { "flush_time", OPT_FLUSH_TIME,
    "A dedicated thread is created to flush all tables at the given interval.",
    (uchar**) &flush_time, (uchar**) &flush_time, 0, GET_ULONG, REQUIRED_ARG,
    FLUSH_TIME, 0, LONG_TIMEOUT, 0, 1, 0},
  { "group_concat_max_len", OPT_GROUP_CONCAT_MAX_LEN,
    "The maximum length of the result of function  group_concat.",
    (uchar**) &global_system_variables.group_concat_max_len,
    (uchar**) &max_system_variables.group_concat_max_len, 0, GET_ULONG,
    REQUIRED_ARG, 1024, 4, ULONG_MAX, 0, 1, 0},
  {"interactive_timeout", OPT_INTERACTIVE_TIMEOUT,
   "The number of seconds the server waits for activity on an interactive connection before closing it.",
   (uchar**) &global_system_variables.net_interactive_timeout,
   (uchar**) &max_system_variables.net_interactive_timeout, 0,
   GET_ULONG, REQUIRED_ARG, NET_WAIT_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"join_buffer_size", OPT_JOIN_BUFF_SIZE,
   "The size of the buffer that is used for full joins.",
   (uchar**) &global_system_variables.join_buff_size,
   (uchar**) &max_system_variables.join_buff_size, 0, GET_ULONG,
   REQUIRED_ARG, 128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, ULONG_MAX,
   MALLOC_OVERHEAD, IO_SIZE, 0},
  {"keep_files_on_create", OPT_KEEP_FILES_ON_CREATE,
   "Don't overwrite stale .MYD and .MYI even if no directory is specified.",
   (uchar**) &global_system_variables.keep_files_on_create,
   (uchar**) &max_system_variables.keep_files_on_create,
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"key_buffer_size", OPT_KEY_BUFFER_SIZE,
   "The size of the buffer used for index blocks for MyISAM tables. Increase this to get better index handling (for all reads and multiple writes) to as much as you can afford; 64M on a 256M machine that mainly runs MySQL is quite common.",
   (uchar**) &dflt_key_cache_var.param_buff_size,
   (uchar**) 0,
   0, (GET_ULL | GET_ASK_ADDR),
   REQUIRED_ARG, KEY_CACHE_SIZE, MALLOC_OVERHEAD, SIZE_T_MAX, MALLOC_OVERHEAD,
   IO_SIZE, 0},
  {"key_cache_age_threshold", OPT_KEY_CACHE_AGE_THRESHOLD,
   "This characterizes the number of hits a hot block has to be untouched until it is considered aged enough to be downgraded to a warm block. This specifies the percentage ratio of that number of hits to the total number of blocks in key cache",
   (uchar**) &dflt_key_cache_var.param_age_threshold,
   (uchar**) 0,
   0, (GET_ULONG | GET_ASK_ADDR), REQUIRED_ARG, 
   300, 100, ULONG_MAX, 0, 100, 0},
  {"key_cache_block_size", OPT_KEY_CACHE_BLOCK_SIZE,
   "The default size of key cache blocks",
   (uchar**) &dflt_key_cache_var.param_block_size,
   (uchar**) 0,
   0, (GET_ULONG | GET_ASK_ADDR), REQUIRED_ARG,
   KEY_CACHE_BLOCK_SIZE, 512, 1024 * 16, 0, 512, 0},
  {"key_cache_division_limit", OPT_KEY_CACHE_DIVISION_LIMIT,
   "The minimum percentage of warm blocks in key cache",
   (uchar**) &dflt_key_cache_var.param_division_limit,
   (uchar**) 0,
   0, (GET_ULONG | GET_ASK_ADDR) , REQUIRED_ARG, 100,
   1, 100, 0, 1, 0},
  {"long_query_time", OPT_LONG_QUERY_TIME,
   "Log all queries that have taken more than long_query_time seconds to execute to file. "
   "The argument will be treated as a decimal value with microsecond precission.",
   (uchar**) &long_query_time, (uchar**) &long_query_time, 0, GET_DOUBLE,
   REQUIRED_ARG, 10, 0, LONG_TIMEOUT, 0, 0, 0},
  {"lower_case_table_names", OPT_LOWER_CASE_TABLE_NAMES,
   "If set to 1 table names are stored in lowercase on disk and table names will be case-insensitive.  Should be set to 2 if you are using a case insensitive file system",
   (uchar**) &lower_case_table_names,
   (uchar**) &lower_case_table_names, 0, GET_UINT, OPT_ARG,
#ifdef FN_NO_CASE_SENCE
    1
#else
    0
#endif
   , 0, 2, 0, 1, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET,
   "Max packetlength to send/receive from to server.",
   (uchar**) &global_system_variables.max_allowed_packet,
   (uchar**) &max_system_variables.max_allowed_packet, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L, 1024, 1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"max_binlog_cache_size", OPT_MAX_BINLOG_CACHE_SIZE,
   "Can be used to restrict the total size used to cache a multi-transaction query.",
   (uchar**) &max_binlog_cache_size, (uchar**) &max_binlog_cache_size, 0,
   GET_ULONG, REQUIRED_ARG, ULONG_MAX, IO_SIZE, ULONG_MAX, 0, IO_SIZE, 0},
  {"max_binlog_size", OPT_MAX_BINLOG_SIZE,
   "Binary log will be rotated automatically when the size exceeds this \
value. Will also apply to relay logs if max_relay_log_size is 0. \
The minimum value for this variable is 4096.",
   (uchar**) &max_binlog_size, (uchar**) &max_binlog_size, 0, GET_ULONG,
   REQUIRED_ARG, 1024*1024L*1024L, IO_SIZE, 1024*1024L*1024L, 0, IO_SIZE, 0},
  {"max_connect_errors", OPT_MAX_CONNECT_ERRORS,
   "If there is more than this number of interrupted connections from a host this host will be blocked from further connections.",
   (uchar**) &max_connect_errors, (uchar**) &max_connect_errors, 0, GET_ULONG,
    REQUIRED_ARG, MAX_CONNECT_ERRORS, 1, ULONG_MAX, 0, 1, 0},
  // Default max_connections of 151 is larger than Apache's default max
  // children, to avoid "too many connections" error in a common setup
  {"max_connections", OPT_MAX_CONNECTIONS,
   "The number of simultaneous clients allowed.", (uchar**) &max_connections,
   (uchar**) &max_connections, 0, GET_ULONG, REQUIRED_ARG, 151, 1, 100000, 0, 1,
   0},
  {"max_error_count", OPT_MAX_ERROR_COUNT,
   "Max number of errors/warnings to store for a statement.",
   (uchar**) &global_system_variables.max_error_count,
   (uchar**) &max_system_variables.max_error_count,
   0, GET_ULONG, REQUIRED_ARG, DEFAULT_ERROR_COUNT, 0, 65535, 0, 1, 0},
  {"max_heap_table_size", OPT_MAX_HEP_TABLE_SIZE,
   "Don't allow creation of heap tables bigger than this.",
   (uchar**) &global_system_variables.max_heap_table_size,
   (uchar**) &max_system_variables.max_heap_table_size, 0, GET_ULL,
   REQUIRED_ARG, 16*1024*1024L, 16384, MAX_MEM_TABLE_SIZE,
   MALLOC_OVERHEAD, 1024, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   "Joins that are probably going to read more than max_join_size records return an error.",
   (uchar**) &global_system_variables.max_join_size,
   (uchar**) &max_system_variables.max_join_size, 0, GET_HA_ROWS, REQUIRED_ARG,
   ~0L, 1, ~0L, 0, 1, 0},
   {"max_length_for_sort_data", OPT_MAX_LENGTH_FOR_SORT_DATA,
    "Max number of bytes in sorted records.",
    (uchar**) &global_system_variables.max_length_for_sort_data,
    (uchar**) &max_system_variables.max_length_for_sort_data, 0, GET_ULONG,
    REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  {"max_relay_log_size", OPT_MAX_RELAY_LOG_SIZE,
   "If non-zero: relay log will be rotated automatically when the size exceeds this value; if zero (the default): when the size exceeds max_binlog_size. 0 excepted, the minimum value for this variable is 4096.",
   (uchar**) &max_relay_log_size, (uchar**) &max_relay_log_size, 0, GET_ULONG,
   REQUIRED_ARG, 0L, 0L, 1024*1024L*1024L, 0, IO_SIZE, 0},
  { "max_seeks_for_key", OPT_MAX_SEEKS_FOR_KEY,
    "Limit assumed max number of seeks when looking up rows based on a key",
    (uchar**) &global_system_variables.max_seeks_for_key,
    (uchar**) &max_system_variables.max_seeks_for_key, 0, GET_ULONG,
    REQUIRED_ARG, ULONG_MAX, 1, ULONG_MAX, 0, 1, 0 },
  {"max_sort_length", OPT_MAX_SORT_LENGTH,
   "The number of bytes to use when sorting BLOB or TEXT values (only the first max_sort_length bytes of each value are used; the rest are ignored).",
   (uchar**) &global_system_variables.max_sort_length,
   (uchar**) &max_system_variables.max_sort_length, 0, GET_ULONG,
   REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  {"max_tmp_tables", OPT_MAX_TMP_TABLES,
   "Maximum number of temporary tables a client can keep open at a time.",
   (uchar**) &global_system_variables.max_tmp_tables,
   (uchar**) &max_system_variables.max_tmp_tables, 0, GET_ULONG,
   REQUIRED_ARG, 32, 1, ULONG_MAX, 0, 1, 0},
  {"max_write_lock_count", OPT_MAX_WRITE_LOCK_COUNT,
   "After this many write locks, allow some read locks to run in between.",
   (uchar**) &max_write_lock_count, (uchar**) &max_write_lock_count, 0, GET_ULONG,
   REQUIRED_ARG, ULONG_MAX, 1, ULONG_MAX, 0, 1, 0},
  {"min_examined_row_limit", OPT_MIN_EXAMINED_ROW_LIMIT,
   "Don't log queries which examine less than min_examined_row_limit rows to file.",
   (uchar**) &global_system_variables.min_examined_row_limit,
   (uchar**) &max_system_variables.min_examined_row_limit, 0, GET_ULONG,
  REQUIRED_ARG, 0, 0, ULONG_MAX, 0, 1L, 0},
  {"myisam_block_size", OPT_MYISAM_BLOCK_SIZE,
   "Block size to be used for MyISAM index pages.",
   (uchar**) &opt_myisam_block_size,
   (uchar**) &opt_myisam_block_size, 0, GET_ULONG, REQUIRED_ARG,
   MI_KEY_BLOCK_LENGTH, MI_MIN_KEY_BLOCK_LENGTH, MI_MAX_KEY_BLOCK_LENGTH,
   0, MI_MIN_KEY_BLOCK_LENGTH, 0},
  {"myisam_data_pointer_size", OPT_MYISAM_DATA_POINTER_SIZE,
   "Default pointer size to be used for MyISAM tables.",
   (uchar**) &myisam_data_pointer_size,
   (uchar**) &myisam_data_pointer_size, 0, GET_ULONG, REQUIRED_ARG,
   6, 2, 7, 0, 1, 0},
  {"myisam_max_extra_sort_file_size", OPT_MYISAM_MAX_EXTRA_SORT_FILE_SIZE,
   "Deprecated option",
   (uchar**) &global_system_variables.myisam_max_extra_sort_file_size,
   (uchar**) &max_system_variables.myisam_max_extra_sort_file_size,
   0, GET_ULL, REQUIRED_ARG, (ulonglong) MI_MAX_TEMP_LENGTH,
   0, (ulonglong) MAX_FILE_SIZE, 0, 1, 0},
  {"myisam_max_sort_file_size", OPT_MYISAM_MAX_SORT_FILE_SIZE,
   "Don't use the fast sort index method to created index if the temporary file would get bigger than this.",
   (uchar**) &global_system_variables.myisam_max_sort_file_size,
   (uchar**) &max_system_variables.myisam_max_sort_file_size, 0,
   GET_ULL, REQUIRED_ARG, (longlong) LONG_MAX, 0, (ulonglong) MAX_FILE_SIZE,
   0, 1024*1024, 0},
  {"myisam_repair_threads", OPT_MYISAM_REPAIR_THREADS,
   "Number of threads to use when repairing MyISAM tables. The value of 1 disables parallel repair.",
   (uchar**) &global_system_variables.myisam_repair_threads,
   (uchar**) &max_system_variables.myisam_repair_threads, 0,
   GET_ULONG, REQUIRED_ARG, 1, 1, ULONG_MAX, 0, 1, 0},
  {"myisam_sort_buffer_size", OPT_MYISAM_SORT_BUFFER_SIZE,
   "The buffer that is allocated when sorting the index when doing a REPAIR or when creating indexes with CREATE INDEX or ALTER TABLE.",
   (uchar**) &global_system_variables.myisam_sort_buff_size,
   (uchar**) &max_system_variables.myisam_sort_buff_size, 0,
   GET_ULONG, REQUIRED_ARG, 8192*1024, 4, ~0L, 0, 1, 0},
  {"myisam_stats_method", OPT_MYISAM_STATS_METHOD,
   "Specifies how MyISAM index statistics collection code should threat NULLs. "
   "Possible values of name are \"nulls_unequal\" (default behavior for 4.1/5.0), "
   "\"nulls_equal\" (emulate 4.0 behavior), and \"nulls_ignored\".",
   (uchar**) &myisam_stats_method_str, (uchar**) &myisam_stats_method_str, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"net_buffer_length", OPT_NET_BUFFER_LENGTH,
   "Buffer length for TCP/IP and socket communication.",
   (uchar**) &global_system_variables.net_buffer_length,
   (uchar**) &max_system_variables.net_buffer_length, 0, GET_ULONG,
   REQUIRED_ARG, 16384, 1024, 1024*1024L, 0, 1024, 0},
  {"net_read_timeout", OPT_NET_READ_TIMEOUT,
   "Number of seconds to wait for more data from a connection before aborting the read.",
   (uchar**) &global_system_variables.net_read_timeout,
   (uchar**) &max_system_variables.net_read_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_READ_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"net_retry_count", OPT_NET_RETRY_COUNT,
   "If a read on a communication port is interrupted, retry this many times before giving up.",
   (uchar**) &global_system_variables.net_retry_count,
   (uchar**) &max_system_variables.net_retry_count,0,
   GET_ULONG, REQUIRED_ARG, MYSQLD_NET_RETRY_COUNT, 1, ULONG_MAX, 0, 1, 0},
  {"net_write_timeout", OPT_NET_WRITE_TIMEOUT,
   "Number of seconds to wait for a block to be written to a connection  before aborting the write.",
   (uchar**) &global_system_variables.net_write_timeout,
   (uchar**) &max_system_variables.net_write_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_WRITE_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  { "old", OPT_OLD_MODE, "Use compatible behavior.", 
    (uchar**) &global_system_variables.old_mode,
    (uchar**) &max_system_variables.old_mode, 0, GET_BOOL, NO_ARG, 
    0, 0, 0, 0, 0, 0},
  {"open_files_limit", OPT_OPEN_FILES_LIMIT,
   "If this is not 0, then mysqld will use this value to reserve file descriptors to use with setrlimit(). If this value is 0 then mysqld will reserve max_connections*5 or max_connections + table_cache*2 (whichever is larger) number of files.",
   (uchar**) &open_files_limit, (uchar**) &open_files_limit, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, OS_FILE_LIMIT, 0, 1, 0},
  {"optimizer_prune_level", OPT_OPTIMIZER_PRUNE_LEVEL,
   "Controls the heuristic(s) applied during query optimization to prune less-promising partial plans from the optimizer search space. Meaning: 0 - do not apply any heuristic, thus perform exhaustive search; 1 - prune plans based on number of retrieved rows.",
   (uchar**) &global_system_variables.optimizer_prune_level,
   (uchar**) &max_system_variables.optimizer_prune_level,
   0, GET_ULONG, OPT_ARG, 1, 0, 1, 0, 1, 0},
  {"optimizer_search_depth", OPT_OPTIMIZER_SEARCH_DEPTH,
   "Maximum depth of search performed by the query optimizer. Values larger than the number of relations in a query result in better query plans, but take longer to compile a query. Smaller values than the number of tables in a relation result in faster optimization, but may produce very bad query plans. If set to 0, the system will automatically pick a reasonable value; if set to MAX_TABLES+2, the optimizer will switch to the original find_best (used for testing/comparison).",
   (uchar**) &global_system_variables.optimizer_search_depth,
   (uchar**) &max_system_variables.optimizer_search_depth,
   0, GET_ULONG, OPT_ARG, MAX_TABLES+1, 0, MAX_TABLES+2, 0, 1, 0},
  {"plugin_dir", OPT_PLUGIN_DIR,
   "Directory for plugins.",
   (uchar**) &opt_plugin_dir_ptr, (uchar**) &opt_plugin_dir_ptr, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin_load", OPT_PLUGIN_LOAD,
   "Optional colon separated list of plugins to load, where each plugin is "
   "identified by name and path to library seperated by an equals.",
   (uchar**) &opt_plugin_load, (uchar**) &opt_plugin_load, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"preload_buffer_size", OPT_PRELOAD_BUFFER_SIZE,
   "The size of the buffer that is allocated when preloading indexes",
   (uchar**) &global_system_variables.preload_buff_size,
   (uchar**) &max_system_variables.preload_buff_size, 0, GET_ULONG,
   REQUIRED_ARG, 32*1024L, 1024, 1024*1024*1024L, 0, 1, 0},
  {"query_alloc_block_size", OPT_QUERY_ALLOC_BLOCK_SIZE,
   "Allocation block size for query parsing and execution",
   (uchar**) &global_system_variables.query_alloc_block_size,
   (uchar**) &max_system_variables.query_alloc_block_size, 0, GET_ULONG,
   REQUIRED_ARG, QUERY_ALLOC_BLOCK_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
  {"query_prealloc_size", OPT_QUERY_PREALLOC_SIZE,
   "Persistent buffer for query parsing and execution",
   (uchar**) &global_system_variables.query_prealloc_size,
   (uchar**) &max_system_variables.query_prealloc_size, 0, GET_ULONG,
   REQUIRED_ARG, QUERY_ALLOC_PREALLOC_SIZE, QUERY_ALLOC_PREALLOC_SIZE,
   ULONG_MAX, 0, 1024, 0},
  {"range_alloc_block_size", OPT_RANGE_ALLOC_BLOCK_SIZE,
   "Allocation block size for storing ranges during optimization",
   (uchar**) &global_system_variables.range_alloc_block_size,
   (uchar**) &max_system_variables.range_alloc_block_size, 0, GET_ULONG,
   REQUIRED_ARG, RANGE_ALLOC_BLOCK_SIZE, RANGE_ALLOC_BLOCK_SIZE, ULONG_MAX,
   0, 1024, 0},
  {"read_buffer_size", OPT_RECORD_BUFFER,
   "Each thread that does a sequential scan allocates a buffer of this size for each table it scans. If you do many sequential scans, you may want to increase this value.",
   (uchar**) &global_system_variables.read_buff_size,
   (uchar**) &max_system_variables.read_buff_size,0, GET_ULONG, REQUIRED_ARG,
   128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, INT_MAX32, MALLOC_OVERHEAD, IO_SIZE,
   0},
  {"read_only", OPT_READONLY,
   "Make all non-temporary tables read-only, with the exception for replication (slave) threads and users with the SUPER privilege",
   (uchar**) &opt_readonly,
   (uchar**) &opt_readonly,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
  {"read_rnd_buffer_size", OPT_RECORD_RND_BUFFER,
   "When reading rows in sorted order after a sort, the rows are read through this buffer to avoid a disk seeks. If not set, then it's set to the value of record_buffer.",
   (uchar**) &global_system_variables.read_rnd_buff_size,
   (uchar**) &max_system_variables.read_rnd_buff_size, 0,
   GET_ULONG, REQUIRED_ARG, 256*1024L, 64 /*IO_SIZE*2+MALLOC_OVERHEAD*/ ,
   INT_MAX32, MALLOC_OVERHEAD, 1 /* Small lower limit to be able to test MRR */, 0},
  {"record_buffer", OPT_RECORD_BUFFER,
   "Alias for read_buffer_size",
   (uchar**) &global_system_variables.read_buff_size,
   (uchar**) &max_system_variables.read_buff_size,0, GET_ULONG, REQUIRED_ARG,
   128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, INT_MAX32, MALLOC_OVERHEAD, IO_SIZE, 0},
  {"relay_log_purge", OPT_RELAY_LOG_PURGE,
   "0 = do not purge relay logs. 1 = purge them as soon as they are no more needed.",
   (uchar**) &relay_log_purge,
   (uchar**) &relay_log_purge, 0, GET_BOOL, NO_ARG,
   1, 0, 1, 0, 1, 0},
  {"relay_log_space_limit", OPT_RELAY_LOG_SPACE_LIMIT,
   "Maximum space to use for all relay logs.",
   (uchar**) &relay_log_space_limit,
   (uchar**) &relay_log_space_limit, 0, GET_ULL, REQUIRED_ARG, 0L, 0L,
   (longlong) ULONG_MAX, 0, 1, 0},
  {"slave_compressed_protocol", OPT_SLAVE_COMPRESSED_PROTOCOL,
   "Use compression on master/slave protocol.",
   (uchar**) &opt_slave_compressed_protocol,
   (uchar**) &opt_slave_compressed_protocol,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
  {"slave_net_timeout", OPT_SLAVE_NET_TIMEOUT,
   "Number of seconds to wait for more data from a master/slave connection before aborting the read.",
   (uchar**) &slave_net_timeout, (uchar**) &slave_net_timeout, 0,
   GET_ULONG, REQUIRED_ARG, SLAVE_NET_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"slave_transaction_retries", OPT_SLAVE_TRANS_RETRIES,
   "Number of times the slave SQL thread will retry a transaction in case "
   "it failed with a deadlock or elapsed lock wait timeout, "
   "before giving up and stopping.",
   (uchar**) &slave_trans_retries, (uchar**) &slave_trans_retries, 0,
   GET_ULONG, REQUIRED_ARG, 10L, 0L, (longlong) ULONG_MAX, 0, 1, 0},
  {"slave-allow-batching", OPT_SLAVE_ALLOW_BATCHING,
   "Allow slave to batch requests.",
   (uchar**) &slave_allow_batching, (uchar**) &slave_allow_batching,
   0, GET_BOOL, NO_ARG, 0, 0, 1, 0, 1, 0},
  {"slow_launch_time", OPT_SLOW_LAUNCH_TIME,
   "If creating the thread takes longer than this value (in seconds), the Slow_launch_threads counter will be incremented.",
   (uchar**) &slow_launch_time, (uchar**) &slow_launch_time, 0, GET_ULONG,
   REQUIRED_ARG, 2L, 0L, LONG_TIMEOUT, 0, 1, 0},
  {"sort_buffer_size", OPT_SORT_BUFFER,
   "Each thread that needs to do a sort allocates a buffer of this size.",
   (uchar**) &global_system_variables.sortbuff_size,
   (uchar**) &max_system_variables.sortbuff_size, 0, GET_ULONG, REQUIRED_ARG,
   MAX_SORT_MEMORY, MIN_SORT_MEMORY+MALLOC_OVERHEAD*2, ULONG_MAX,
   MALLOC_OVERHEAD, 1, 0},
  {"sync-binlog", OPT_SYNC_BINLOG,
   "Synchronously flush binary log to disk after every #th event. "
   "Use 0 (default) to disable synchronous flushing.",
   (uchar**) &sync_binlog_period, (uchar**) &sync_binlog_period, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, ULONG_MAX, 0, 1, 0},
  {"sync-frm", OPT_SYNC_FRM, "Sync .frm to disk on create. Enabled by default.",
   (uchar**) &opt_sync_frm, (uchar**) &opt_sync_frm, 0, GET_BOOL, NO_ARG, 1, 0,
   0, 0, 0, 0},
  {"table_cache", OPT_TABLE_OPEN_CACHE,
   "Deprecated; use --table_open_cache instead.",
   (uchar**) &table_cache_size, (uchar**) &table_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, TABLE_OPEN_CACHE_DEFAULT, 1, 512*1024L, 0, 1, 0},
  {"table_definition_cache", OPT_TABLE_DEF_CACHE,
   "The number of cached table definitions.",
   (uchar**) &table_def_size, (uchar**) &table_def_size,
   0, GET_ULONG, REQUIRED_ARG, 128, 1, 512*1024L, 0, 1, 0},
  {"table_open_cache", OPT_TABLE_OPEN_CACHE,
   "The number of cached open tables.",
   (uchar**) &table_cache_size, (uchar**) &table_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, TABLE_OPEN_CACHE_DEFAULT, 1, 512*1024L, 0, 1, 0},
  {"table_lock_wait_timeout", OPT_TABLE_LOCK_WAIT_TIMEOUT,
   "Timeout in seconds to wait for a table level lock before returning an "
   "error. Used only if the connection has active cursors.",
   (uchar**) &table_lock_wait_timeout, (uchar**) &table_lock_wait_timeout,
   0, GET_ULONG, REQUIRED_ARG, 50, 1, 1024 * 1024 * 1024, 0, 1, 0},
  {"thread_cache_size", OPT_THREAD_CACHE_SIZE,
   "How many threads we should keep in a cache for reuse.",
   (uchar**) &thread_cache_size, (uchar**) &thread_cache_size, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 16384, 0, 1, 0},
  {"thread_pool_size", OPT_THREAD_CACHE_SIZE,
   "How many threads we should create to handle query requests in case of 'thread_handling=pool-of-threads'",
   (uchar**) &thread_pool_size, (uchar**) &thread_pool_size, 0, GET_ULONG,
   REQUIRED_ARG, 20, 1, 16384, 0, 1, 0},
  {"thread_stack", OPT_THREAD_STACK,
   "The stack size for each thread.", (uchar**) &my_thread_stack_size,
   (uchar**) &my_thread_stack_size, 0, GET_ULONG, REQUIRED_ARG,DEFAULT_THREAD_STACK,
   1024L*128L, ULONG_MAX, 0, 1024, 0},
  { "time_format", OPT_TIME_FORMAT,
    "The TIME format (for future).",
    (uchar**) &opt_date_time_formats[MYSQL_TIMESTAMP_TIME],
    (uchar**) &opt_date_time_formats[MYSQL_TIMESTAMP_TIME],
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmp_table_size", OPT_TMP_TABLE_SIZE,
   "If an internal in-memory temporary table exceeds this size, MySQL will"
   " automatically convert it to an on-disk MyISAM table.",
   (uchar**) &global_system_variables.tmp_table_size,
   (uchar**) &max_system_variables.tmp_table_size, 0, GET_ULL,
   REQUIRED_ARG, 16*1024*1024L, 1024, MAX_MEM_TABLE_SIZE, 0, 1, 0},
  {"transaction_alloc_block_size", OPT_TRANS_ALLOC_BLOCK_SIZE,
   "Allocation block size for transactions to be stored in binary log",
   (uchar**) &global_system_variables.trans_alloc_block_size,
   (uchar**) &max_system_variables.trans_alloc_block_size, 0, GET_ULONG,
   REQUIRED_ARG, QUERY_ALLOC_BLOCK_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
  {"transaction_prealloc_size", OPT_TRANS_PREALLOC_SIZE,
   "Persistent buffer for transactions to be stored in binary log",
   (uchar**) &global_system_variables.trans_prealloc_size,
   (uchar**) &max_system_variables.trans_prealloc_size, 0, GET_ULONG,
   REQUIRED_ARG, TRANS_ALLOC_PREALLOC_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
  {"wait_timeout", OPT_WAIT_TIMEOUT,
   "The number of seconds the server waits for activity on a connection before closing it.",
   (uchar**) &global_system_variables.net_wait_timeout,
   (uchar**) &max_system_variables.net_wait_timeout, 0, GET_ULONG,
   REQUIRED_ARG, NET_WAIT_TIMEOUT, 1, IF_WIN(INT_MAX32/1000, LONG_TIMEOUT),
   0, 1, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static int show_net_compression(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_MY_BOOL;
  var->value= (char *)&thd->net.compress;
  return 0;
}

static int show_starttime(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (thd->query_start() - server_start_time);
  return 0;
}

static int show_flushstatustime(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (thd->query_start() - flush_status_time);
  return 0;
}

static int show_slave_running(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_MY_BOOL;
  pthread_mutex_lock(&LOCK_active_mi);
  var->value= buff;
  *((my_bool *)buff)= (my_bool) (active_mi && active_mi->slave_running &&
                                 active_mi->rli.slave_running);
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}

static int show_slave_retried_trans(THD *thd, SHOW_VAR *var, char *buff)
{
  /*
    TODO: with multimaster, have one such counter per line in
    SHOW SLAVE STATUS, and have the sum over all lines here.
  */
  pthread_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    var->type= SHOW_LONG;
    var->value= buff;
    pthread_mutex_lock(&active_mi->rli.data_lock);
    *((long *)buff)= (long)active_mi->rli.retried_trans;
    pthread_mutex_unlock(&active_mi->rli.data_lock);
  }
  else
    var->type= SHOW_UNDEF;
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}

static int show_slave_received_heartbeats(THD *thd, SHOW_VAR *var, char *buff)
{
  pthread_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    var->type= SHOW_LONGLONG;
    var->value= buff;
    pthread_mutex_lock(&active_mi->rli.data_lock);
    *((longlong *)buff)= active_mi->received_heartbeats;
    pthread_mutex_unlock(&active_mi->rli.data_lock);
  }
  else
    var->type= SHOW_UNDEF;
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}

static int show_heartbeat_period(THD *thd, SHOW_VAR *var, char *buff)
{
  pthread_mutex_lock(&LOCK_active_mi);
  if (active_mi)
  {
    var->type= SHOW_CHAR;
    var->value= buff;
    my_sprintf(buff, (buff, "%.3f",active_mi->heartbeat_period));
  }
  else
    var->type= SHOW_UNDEF;
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}


static int show_open_tables(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long)cached_open_tables();
  return 0;
}

static int show_table_definitions(THD *thd, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long)cached_table_definitions();
  return 0;
}

/*
  Variables shown by SHOW STATUS in alphabetical order
*/

SHOW_VAR status_vars[]= {
  {"Aborted_clients",          (char*) &aborted_threads,        SHOW_LONG},
  {"Aborted_connects",         (char*) &aborted_connects,       SHOW_LONG},
  {"Binlog_cache_disk_use",    (char*) &binlog_cache_disk_use,  SHOW_LONG},
  {"Binlog_cache_use",         (char*) &binlog_cache_use,       SHOW_LONG},
  {"Bytes_received",           (char*) offsetof(STATUS_VAR, bytes_received), SHOW_LONGLONG_STATUS},
  {"Bytes_sent",               (char*) offsetof(STATUS_VAR, bytes_sent), SHOW_LONGLONG_STATUS},
  {"Com",                      (char*) com_status_vars, SHOW_ARRAY},
  {"Compression",              (char*) &show_net_compression, SHOW_FUNC},
  {"Connections",              (char*) &thread_id,              SHOW_LONG_NOFLUSH},
  {"Created_tmp_disk_tables",  (char*) offsetof(STATUS_VAR, created_tmp_disk_tables), SHOW_LONG_STATUS},
  {"Created_tmp_files",	       (char*) &my_tmp_file_created,	SHOW_LONG},
  {"Created_tmp_tables",       (char*) offsetof(STATUS_VAR, created_tmp_tables), SHOW_LONG_STATUS},
  {"Flush_commands",           (char*) &refresh_version,        SHOW_LONG_NOFLUSH},
  {"Handler_commit",           (char*) offsetof(STATUS_VAR, ha_commit_count), SHOW_LONG_STATUS},
  {"Handler_delete",           (char*) offsetof(STATUS_VAR, ha_delete_count), SHOW_LONG_STATUS},
  {"Handler_discover",         (char*) offsetof(STATUS_VAR, ha_discover_count), SHOW_LONG_STATUS},
  {"Handler_prepare",          (char*) offsetof(STATUS_VAR, ha_prepare_count),  SHOW_LONG_STATUS},
  {"Handler_read_first",       (char*) offsetof(STATUS_VAR, ha_read_first_count), SHOW_LONG_STATUS},
  {"Handler_read_key",         (char*) offsetof(STATUS_VAR, ha_read_key_count), SHOW_LONG_STATUS},
  {"Handler_read_next",        (char*) offsetof(STATUS_VAR, ha_read_next_count), SHOW_LONG_STATUS},
  {"Handler_read_prev",        (char*) offsetof(STATUS_VAR, ha_read_prev_count), SHOW_LONG_STATUS},
  {"Handler_read_rnd",         (char*) offsetof(STATUS_VAR, ha_read_rnd_count), SHOW_LONG_STATUS},
  {"Handler_read_rnd_next",    (char*) offsetof(STATUS_VAR, ha_read_rnd_next_count), SHOW_LONG_STATUS},
  {"Handler_rollback",         (char*) offsetof(STATUS_VAR, ha_rollback_count), SHOW_LONG_STATUS},
  {"Handler_savepoint",        (char*) offsetof(STATUS_VAR, ha_savepoint_count), SHOW_LONG_STATUS},
  {"Handler_savepoint_rollback",(char*) offsetof(STATUS_VAR, ha_savepoint_rollback_count), SHOW_LONG_STATUS},
  {"Handler_update",           (char*) offsetof(STATUS_VAR, ha_update_count), SHOW_LONG_STATUS},
  {"Handler_write",            (char*) offsetof(STATUS_VAR, ha_write_count), SHOW_LONG_STATUS},
  {"Key_blocks_not_flushed",   (char*) offsetof(KEY_CACHE, global_blocks_changed), SHOW_KEY_CACHE_LONG},
  {"Key_blocks_unused",        (char*) offsetof(KEY_CACHE, blocks_unused), SHOW_KEY_CACHE_LONG},
  {"Key_blocks_used",          (char*) offsetof(KEY_CACHE, blocks_used), SHOW_KEY_CACHE_LONG},
  {"Key_read_requests",        (char*) offsetof(KEY_CACHE, global_cache_r_requests), SHOW_KEY_CACHE_LONGLONG},
  {"Key_reads",                (char*) offsetof(KEY_CACHE, global_cache_read), SHOW_KEY_CACHE_LONGLONG},
  {"Key_write_requests",       (char*) offsetof(KEY_CACHE, global_cache_w_requests), SHOW_KEY_CACHE_LONGLONG},
  {"Key_writes",               (char*) offsetof(KEY_CACHE, global_cache_write), SHOW_KEY_CACHE_LONGLONG},
  {"Last_query_cost",          (char*) offsetof(STATUS_VAR, last_query_cost), SHOW_DOUBLE_STATUS},
  {"Max_used_connections",     (char*) &max_used_connections,  SHOW_LONG},
  {"Open_files",               (char*) &my_file_opened,         SHOW_LONG_NOFLUSH},
  {"Open_streams",             (char*) &my_stream_opened,       SHOW_LONG_NOFLUSH},
  {"Open_table_definitions",   (char*) &show_table_definitions, SHOW_FUNC},
  {"Open_tables",              (char*) &show_open_tables,       SHOW_FUNC},
  {"Opened_files",             (char*) &my_file_total_opened, SHOW_LONG_NOFLUSH},
  {"Opened_tables",            (char*) offsetof(STATUS_VAR, opened_tables), SHOW_LONG_STATUS},
  {"Opened_table_definitions", (char*) offsetof(STATUS_VAR, opened_shares), SHOW_LONG_STATUS},
  {"Questions",                (char*) offsetof(STATUS_VAR, questions), SHOW_LONG_STATUS},
  {"Select_full_join",         (char*) offsetof(STATUS_VAR, select_full_join_count), SHOW_LONG_STATUS},
  {"Select_full_range_join",   (char*) offsetof(STATUS_VAR, select_full_range_join_count), SHOW_LONG_STATUS},
  {"Select_range",             (char*) offsetof(STATUS_VAR, select_range_count), SHOW_LONG_STATUS},
  {"Select_range_check",       (char*) offsetof(STATUS_VAR, select_range_check_count), SHOW_LONG_STATUS},
  {"Select_scan",	       (char*) offsetof(STATUS_VAR, select_scan_count), SHOW_LONG_STATUS},
  {"Slave_open_temp_tables",   (char*) &slave_open_temp_tables, SHOW_LONG},
  {"Slave_retried_transactions",(char*) &show_slave_retried_trans, SHOW_FUNC},
  {"Slave_heartbeat_period",   (char*) &show_heartbeat_period, SHOW_FUNC},
  {"Slave_received_heartbeats",(char*) &show_slave_received_heartbeats, SHOW_FUNC},
  {"Slave_running",            (char*) &show_slave_running,     SHOW_FUNC},
  {"Slow_launch_threads",      (char*) &slow_launch_threads,    SHOW_LONG},
  {"Slow_queries",             (char*) offsetof(STATUS_VAR, long_query_count), SHOW_LONG_STATUS},
  {"Sort_merge_passes",	       (char*) offsetof(STATUS_VAR, filesort_merge_passes), SHOW_LONG_STATUS},
  {"Sort_range",	       (char*) offsetof(STATUS_VAR, filesort_range_count), SHOW_LONG_STATUS},
  {"Sort_rows",		       (char*) offsetof(STATUS_VAR, filesort_rows), SHOW_LONG_STATUS},
  {"Sort_scan",		       (char*) offsetof(STATUS_VAR, filesort_scan_count), SHOW_LONG_STATUS},
  {"Table_locks_immediate",    (char*) &locks_immediate,        SHOW_LONG},
  {"Table_locks_waited",       (char*) &locks_waited,           SHOW_LONG},
#ifdef HAVE_MMAP
  {"Tc_log_max_pages_used",    (char*) &tc_log_max_pages_used,  SHOW_LONG},
  {"Tc_log_page_size",         (char*) &tc_log_page_size,       SHOW_LONG},
  {"Tc_log_page_waits",        (char*) &tc_log_page_waits,      SHOW_LONG},
#endif
  {"Threads_cached",           (char*) &cached_thread_count,    SHOW_LONG_NOFLUSH},
  {"Threads_connected",        (char*) &connection_count,       SHOW_INT},
  {"Threads_created",	       (char*) &thread_created,		SHOW_LONG_NOFLUSH},
  {"Threads_running",          (char*) &thread_running,         SHOW_INT},
  {"Uptime",                   (char*) &show_starttime,         SHOW_FUNC},
  {"Uptime_since_flush_status",(char*) &show_flushstatustime,   SHOW_FUNC},
  {NullS, NullS, SHOW_LONG}
};

static void print_version(void)
{
  set_server_version();
  /*
    Note: the instance manager keys off the string 'Ver' so it can find the
    version from the output of 'mysqld --version', so don't change it!
  */
  printf("%s  Ver %s for %s on %s (%s)\n",my_progname,
	 server_version,SYSTEM_TYPE,MACHINE_TYPE, MYSQL_COMPILATION_COMMENT);
}

static void usage(void)
{
  if (!(default_charset_info= get_charset_by_csname(default_character_set_name,
					           MY_CS_PRIMARY,
						   MYF(MY_WME))))
    exit(1);
  if (!default_collation_name)
    default_collation_name= (char*) default_charset_info->name;
  print_version();
  puts("\
Copyright (C) 2000 MySQL AB, by Monty and others\n\
This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n\
and you are welcome to modify and redistribute it under the GPL license\n\n\
Starts the MySQL database server\n");

  printf("Usage: %s [OPTIONS]\n", my_progname);
  if (!opt_verbose)
    puts("\nFor more help options (several pages), use mysqld --verbose --help");
  else
  {
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  set_ports();

  /* Print out all the options including plugin supplied options */
  my_print_help_inc_plugins(my_long_options, sizeof(my_long_options)/sizeof(my_option));

  puts("\n\
To see what values a running MySQL server is using, type\n\
'mysqladmin variables' instead of 'mysqld --verbose --help'.");
  }
}


/**
  Initialize all MySQL global variables to default values.

  We don't need to set numeric variables refered to in my_long_options
  as these are initialized by my_getopt.

  @note
    The reason to set a lot of global variables to zero is to allow one to
    restart the embedded server with a clean environment
    It's also needed on some exotic platforms where global variables are
    not set to 0 when a program starts.

    We don't need to set numeric variables refered to in my_long_options
    as these are initialized by my_getopt.
*/

static void mysql_init_variables(void)
{
  /* Things reset to zero */
  opt_skip_slave_start= opt_reckless_slave = 0;
  mysql_home[0]= pidfile_name[0]= log_error_file[0]= 0;
  opt_log= opt_slow_log= 0;
  opt_update_log= 0;
  log_output_options= find_bit_type(log_output_str, &log_output_typelib);
  opt_bin_log= 0;
  opt_disable_networking= opt_skip_show_db=0;
  opt_logname= opt_update_logname= opt_binlog_index_name= opt_slow_logname= 0;
  opt_tc_log_file= (char *)"tc.log";      // no hostname in tc_log file name !
  opt_secure_auth= 0;
  opt_secure_file_priv= 0;
  opt_bootstrap= opt_myisam_log= 0;
  mqh_used= 0;
  segfaulted= kill_in_progress= 0;
  cleanup_done= 0;
  defaults_argc= 0;
  defaults_argv= 0;
  server_id_supplied= 0;
  test_flags= select_errors= dropping_tables= ha_open_options=0;
  thread_count= thread_running= kill_cached_threads= wake_thread=0;
  slave_open_temp_tables= 0;
  cached_thread_count= 0;
  opt_endinfo= using_udf_functions= 0;
  opt_using_transactions= using_update_log= 0;
  abort_loop= select_thread_in_use= signal_thread_in_use= 0;
  ready_to_exit= shutdown_in_progress= 0;
  aborted_threads= aborted_connects= 0;
  specialflag= 0;
  binlog_cache_use=  binlog_cache_disk_use= 0;
  max_used_connections= slow_launch_threads = 0;
  mysqld_user= mysqld_chroot= opt_init_file= opt_bin_logname = 0;
  errmesg= 0;
  opt_mysql_tmpdir= my_bind_addr_str= NullS;
  bzero((uchar*) &mysql_tmpdir_list, sizeof(mysql_tmpdir_list));
  bzero((char *) &global_status_var, sizeof(global_status_var));
  key_map_full.set_all();

  /* Character sets */
  system_charset_info= &my_charset_utf8_general_ci;
  files_charset_info= &my_charset_utf8_general_ci;
  national_charset_info= &my_charset_utf8_general_ci;
  table_alias_charset= &my_charset_bin;
  character_set_filesystem= &my_charset_bin;

  opt_date_time_formats[0]= opt_date_time_formats[1]= opt_date_time_formats[2]= 0;

  /* Things with default values that are not zero */
  delay_key_write_options= (uint) DELAY_KEY_WRITE_ON;
  slave_exec_mode_options= 0;
  slave_exec_mode_options= (uint)
    find_bit_type_or_exit(slave_exec_mode_str, &slave_exec_mode_typelib, NULL);
  opt_specialflag= SPECIAL_ENGLISH;
  ip_sock= INVALID_SOCKET;
  mysql_home_ptr= mysql_home;
  pidfile_name_ptr= pidfile_name;
  log_error_file_ptr= log_error_file;
  language_ptr= language;
  mysql_data_home= mysql_real_data_home;
  thd_startup_options= (OPTION_AUTO_IS_NULL | OPTION_BIN_LOG |
                        OPTION_QUOTE_SHOW_CREATE | OPTION_SQL_NOTES);
  protocol_version= PROTOCOL_VERSION;
  what_to_log= ~ (1L << (uint) COM_TIME);
  refresh_version= 1L;	/* Increments on each reload */
  global_query_id= thread_id= 1L;
  strmov(server_version, MYSQL_SERVER_VERSION);
  myisam_recover_options_str= "OFF";
  myisam_stats_method_str= "nulls_unequal";
  threads.empty();
  thread_cache.empty();
  key_caches.empty();
  if (!(dflt_key_cache= get_or_create_key_cache(default_key_cache_base.str,
                                                default_key_cache_base.length)))
    exit(1);
  /* set key_cache_hash.default_value = dflt_key_cache */
  multi_keycache_init();

  /* Set directory paths */
  strmake(language, LANGUAGE, sizeof(language)-1);
  strmake(mysql_real_data_home, get_relative_path(DATADIR),
	  sizeof(mysql_real_data_home)-1);
  mysql_data_home_buff[0]=FN_CURLIB;	// all paths are relative from here
  mysql_data_home_buff[1]=0;
  mysql_data_home_len= 2;

  /* Replication parameters */
  master_info_file= (char*) "master.info",
    relay_log_info_file= (char*) "relay-log.info";
  report_user= report_password = report_host= 0;	/* TO BE DELETED */
  opt_relay_logname= opt_relaylog_index_name= 0;

  /* Variables in libraries */
  charsets_dir= 0;
  default_character_set_name= (char*) MYSQL_DEFAULT_CHARSET_NAME;
  default_collation_name= compiled_default_collation_name;
  sys_charset_system.set((char*) system_charset_info->csname);
  character_set_filesystem_name= (char*) "binary";
  lc_time_names_name= (char*) "en_US";
  /* Set default values for some option variables */
  default_storage_engine_str= (char*) "MyISAM";
  global_system_variables.table_plugin= NULL;
  global_system_variables.tx_isolation= ISO_REPEATABLE_READ;
  global_system_variables.select_limit= (ulonglong) HA_POS_ERROR;
  max_system_variables.select_limit=    (ulonglong) HA_POS_ERROR;
  global_system_variables.max_join_size= (ulonglong) HA_POS_ERROR;
  max_system_variables.max_join_size=   (ulonglong) HA_POS_ERROR;
  global_system_variables.old_passwords= 0;
  global_system_variables.old_alter_table= 0;
  global_system_variables.binlog_format= BINLOG_FORMAT_UNSPEC;
  /*
    Default behavior for 4.1 and 5.0 is to treat NULL values as unequal
    when collecting index statistics for MyISAM tables.
  */
  global_system_variables.myisam_stats_method= MI_STATS_METHOD_NULLS_NOT_EQUAL;

  /* Variables that depends on compile options */
#ifndef DBUG_OFF
  default_dbug_option=IF_WIN("d:t:i:O,\\mysqld.trace",
			     "d:t:i:o,/tmp/mysqld.trace");
#endif
  opt_error_log= IF_WIN(1,0);
#ifdef HAVE_BROKEN_REALPATH
  have_symlink=SHOW_OPTION_NO;
#else
  have_symlink=SHOW_OPTION_YES;
#endif
#ifdef HAVE_DLOPEN
  have_dlopen=SHOW_OPTION_YES;
#else
  have_dlopen=SHOW_OPTION_NO;
#endif
#ifdef HAVE_CRYPT
  have_crypt=SHOW_OPTION_YES;
#else
  have_crypt=SHOW_OPTION_NO;
#endif
#ifdef HAVE_COMPRESS
  have_compress= SHOW_OPTION_YES;
#else
  have_compress= SHOW_OPTION_NO;
#endif

  const char *tmpenv;
  if (!(tmpenv = getenv("MY_BASEDIR_VERSION")))
    tmpenv = DEFAULT_MYSQL_HOME;
  (void) strmake(mysql_home, tmpenv, sizeof(mysql_home)-1);
}


my_bool
mysqld_get_one_option(int optid,
                      const struct my_option *opt __attribute__((unused)),
                      char *argument)
{
  switch(optid) {
  case '#':
#ifndef DBUG_OFF
    DBUG_SET_INITIAL(argument ? argument : default_dbug_option);
#endif
    opt_endinfo=1;				/* unireg: memory allocation */
    break;
  case 'a':
    global_system_variables.tx_isolation= ISO_SERIALIZABLE;
    break;
  case 'b':
    strmake(mysql_home,argument,sizeof(mysql_home)-1);
    break;
  case 'C':
    if (default_collation_name == compiled_default_collation_name)
      default_collation_name= 0;
    break;
  case 'l':
    opt_log=1;
    break;
  case 'h':
    strmake(mysql_real_data_home,argument, sizeof(mysql_real_data_home)-1);
    /* Correct pointer set by my_getopt (for embedded library) */
    mysql_data_home= mysql_real_data_home;
    mysql_data_home_len= strlen(mysql_data_home);
    break;
  case 'u':
    if (!mysqld_user || !strcmp(mysqld_user, argument))
      mysqld_user= argument;
    else
      sql_print_warning("Ignoring user change to '%s' because the user was set to '%s' earlier on the command line\n", argument, mysqld_user);
    break;
  case 'L':
    strmake(language, argument, sizeof(language)-1);
    break;
  case OPT_SLAVE_SKIP_ERRORS:
    init_slave_skip_errors(argument);
    break;
  case OPT_SLAVE_EXEC_MODE:
    slave_exec_mode_options= (uint)
      find_bit_type_or_exit(argument, &slave_exec_mode_typelib, "");
    break;
  case 'V':
    print_version();
    exit(0);
  case 'W':
    if (!argument)
      global_system_variables.log_warnings++;
    else if (argument == disabled_my_option)
      global_system_variables.log_warnings= 0L;
    else
      global_system_variables.log_warnings= atoi(argument);
    break;
  case 'T':
    test_flags= argument ? (uint) atoi(argument) : 0;
    opt_endinfo=1;
    break;
  case (int) OPT_BIG_TABLES:
    thd_startup_options|=OPTION_BIG_TABLES;
    break;
  case (int) OPT_ISAM_LOG:
    opt_myisam_log=1;
    break;
  case (int) OPT_UPDATE_LOG:
    opt_update_log=1;
    break;
  case (int) OPT_BIN_LOG:
    opt_bin_log= test(argument != disabled_my_option);
    break;
  case (int) OPT_ERROR_LOG_FILE:
    opt_error_log= 1;
    break;
  case (int)OPT_REPLICATE_IGNORE_DB:
  {
    rpl_filter->add_ignore_db(argument);
    break;
  }
  case (int)OPT_REPLICATE_DO_DB:
  {
    rpl_filter->add_do_db(argument);
    break;
  }
  case (int)OPT_REPLICATE_REWRITE_DB:
  {
    char* key = argument,*p, *val;

    if (!(p= strstr(argument, "->")))
    {
      fprintf(stderr,
	      "Bad syntax in replicate-rewrite-db - missing '->'!\n");
      exit(1);
    }
    val= p--;
    while (my_isspace(mysqld_charset, *p) && p > argument)
      *p-- = 0;
    if (p == argument)
    {
      fprintf(stderr,
	      "Bad syntax in replicate-rewrite-db - empty FROM db!\n");
      exit(1);
    }
    *val= 0;
    val+= 2;
    while (*val && my_isspace(mysqld_charset, *val))
      *val++;
    if (!*val)
    {
      fprintf(stderr,
	      "Bad syntax in replicate-rewrite-db - empty TO db!\n");
      exit(1);
    }

    rpl_filter->add_db_rewrite(key, val);
    break;
  }

  case (int)OPT_BINLOG_IGNORE_DB:
  {
    binlog_filter->add_ignore_db(argument);
    break;
  }
  case OPT_BINLOG_FORMAT:
  {
    int id;
    id= find_type_or_exit(argument, &binlog_format_typelib, opt->name);
    global_system_variables.binlog_format= opt_binlog_format_id= id - 1;
    break;
  }
  case (int)OPT_BINLOG_DO_DB:
  {
    binlog_filter->add_do_db(argument);
    break;
  }
  case (int)OPT_REPLICATE_DO_TABLE:
  {
    if (rpl_filter->add_do_table(argument))
    {
      fprintf(stderr, "Could not add do table rule '%s'!\n", argument);
      exit(1);
    }
    break;
  }
  case (int)OPT_REPLICATE_WILD_DO_TABLE:
  {
    if (rpl_filter->add_wild_do_table(argument))
    {
      fprintf(stderr, "Could not add do table rule '%s'!\n", argument);
      exit(1);
    }
    break;
  }
  case (int)OPT_REPLICATE_WILD_IGNORE_TABLE:
  {
    if (rpl_filter->add_wild_ignore_table(argument))
    {
      fprintf(stderr, "Could not add ignore table rule '%s'!\n", argument);
      exit(1);
    }
    break;
  }
  case (int)OPT_REPLICATE_IGNORE_TABLE:
  {
    if (rpl_filter->add_ignore_table(argument))
    {
      fprintf(stderr, "Could not add ignore table rule '%s'!\n", argument);
      exit(1);
    }
    break;
  }
  case (int) OPT_SLOW_QUERY_LOG:
    opt_slow_log= 1;
    break;
#ifdef WITH_CSV_STORAGE_ENGINE
  case  OPT_LOG_OUTPUT:
  {
    if (!argument || !argument[0])
    {
      log_output_options= LOG_FILE;
      log_output_str= log_output_typelib.type_names[1];
    }
    else
    {
      log_output_str= argument;
      log_output_options=
        find_bit_type_or_exit(argument, &log_output_typelib, opt->name);
  }
    break;
  }
#endif
  case (int) OPT_SKIP_NEW:
    opt_specialflag|= SPECIAL_NO_NEW_FUNC;
    delay_key_write_options= (uint) DELAY_KEY_WRITE_NONE;
    myisam_concurrent_insert=0;
    myisam_recover_options= HA_RECOVER_NONE;
    my_use_symdir=0;
    ha_open_options&= ~(HA_OPEN_ABORT_IF_CRASHED | HA_OPEN_DELAY_KEY_WRITE);
    break;
  case (int) OPT_SAFE:
    opt_specialflag|= SPECIAL_SAFE_MODE;
    delay_key_write_options= (uint) DELAY_KEY_WRITE_NONE;
    myisam_recover_options= HA_RECOVER_DEFAULT;
    ha_open_options&= ~(HA_OPEN_DELAY_KEY_WRITE);
    break;
  case (int) OPT_SKIP_PRIOR:
    opt_specialflag|= SPECIAL_NO_PRIOR;
    break;
  case (int) OPT_SKIP_LOCK:
    opt_external_locking=0;
    break;
  case (int) OPT_SKIP_SHOW_DB:
    opt_skip_show_db=1;
    opt_specialflag|=SPECIAL_SKIP_SHOW_DB;
    break;
  case (int) OPT_WANT_CORE:
    test_flags |= TEST_CORE_ON_SIGNAL;
    break;
  case (int) OPT_SKIP_STACK_TRACE:
    test_flags|=TEST_NO_STACKTRACE;
    break;
  case (int) OPT_SKIP_SYMLINKS:
    my_use_symdir=0;
    break;
  case (int) OPT_BIND_ADDRESS:
    {
      struct addrinfo *res_lst, hints;    

      bzero(&hints, sizeof(struct addrinfo));
      hints.ai_socktype= SOCK_STREAM;
      hints.ai_protocol= IPPROTO_TCP;

      if (getaddrinfo(argument, NULL, &hints, &res_lst) != 0) 
      {
        sql_print_error("Can't start server: cannot resolve hostname!");
        exit(1);
      }

      if (res_lst->ai_next)
      {
        sql_print_error("Can't start server: bind-address refers to multiple interfaces!");
        exit(1);
      }
      freeaddrinfo(res_lst);
    }
    break;
  case (int) OPT_PID_FILE:
    strmake(pidfile_name, argument, sizeof(pidfile_name)-1);
    break;
  case OPT_CONSOLE:
    if (opt_console)
      opt_error_log= 0;			// Force logs to stdout
    break;
  case (int) OPT_FLUSH:
    myisam_flush=1;
    flush_time=0;			// No auto flush
    break;
  case OPT_LOW_PRIORITY_UPDATES:
    thr_upgraded_concurrent_insert_lock= TL_WRITE_LOW_PRIORITY;
    global_system_variables.low_priority_updates=1;
    break;
  case OPT_BOOTSTRAP:
    opt_noacl=opt_bootstrap=1;
    break;
  case OPT_SERVER_ID:
    server_id_supplied = 1;
    break;
  case OPT_DELAY_KEY_WRITE_ALL:
    if (argument != disabled_my_option)
      argument= (char*) "ALL";
    /* Fall through */
  case OPT_DELAY_KEY_WRITE:
    if (argument == disabled_my_option)
      delay_key_write_options= (uint) DELAY_KEY_WRITE_NONE;
    else if (! argument)
      delay_key_write_options= (uint) DELAY_KEY_WRITE_ON;
    else
    {
      int type;
      type= find_type_or_exit(argument, &delay_key_write_typelib, opt->name);
      delay_key_write_options= (uint) type-1;
    }
    break;
  case OPT_CHARSETS_DIR:
    strmake(mysql_charsets_dir, argument, sizeof(mysql_charsets_dir)-1);
    charsets_dir = mysql_charsets_dir;
    break;
  case OPT_TX_ISOLATION:
  {
    int type;
    type= find_type_or_exit(argument, &tx_isolation_typelib, opt->name);
    global_system_variables.tx_isolation= (type-1);
    break;
  }
  case OPT_MYISAM_RECOVER:
  {
    if (!argument)
    {
      myisam_recover_options=    HA_RECOVER_DEFAULT;
      myisam_recover_options_str= myisam_recover_typelib.type_names[0];
    }
    else if (!argument[0])
    {
      myisam_recover_options= HA_RECOVER_NONE;
      myisam_recover_options_str= "OFF";
    }
    else
    {
      myisam_recover_options_str=argument;
      myisam_recover_options=
        find_bit_type_or_exit(argument, &myisam_recover_typelib, opt->name);
    }
    ha_open_options|=HA_OPEN_ABORT_IF_CRASHED;
    break;
  }
  case OPT_TC_HEURISTIC_RECOVER:
    tc_heuristic_recover= find_type_or_exit(argument,
                                            &tc_heuristic_recover_typelib,
                                            opt->name);
    break;
  case OPT_MYISAM_STATS_METHOD:
  {
    ulong method_conv;
    int method;

    myisam_stats_method_str= argument;
    method= find_type_or_exit(argument, &myisam_stats_method_typelib,
                              opt->name);
    switch (method-1) {
    case 2:
      method_conv= MI_STATS_METHOD_IGNORE_NULLS;
      break;
    case 1:
      method_conv= MI_STATS_METHOD_NULLS_EQUAL;
      break;
    case 0:
    default:
      method_conv= MI_STATS_METHOD_NULLS_NOT_EQUAL;
      break;
    }
    global_system_variables.myisam_stats_method= method_conv;
    break;
  }
  case OPT_LOWER_CASE_TABLE_NAMES:
    lower_case_table_names= argument ? atoi(argument) : 1;
    lower_case_table_names_used= 1;
    break;
  }
  return 0;
}


/** Handle arguments for multiple key caches. */

extern "C" uchar **mysql_getopt_value(const char *keyname, uint key_length,
                                      const struct my_option *option);

uchar* *
mysql_getopt_value(const char *keyname, uint key_length,
		   const struct my_option *option)
{
  switch (option->id) {
  case OPT_KEY_BUFFER_SIZE:
  case OPT_KEY_CACHE_BLOCK_SIZE:
  case OPT_KEY_CACHE_DIVISION_LIMIT:
  case OPT_KEY_CACHE_AGE_THRESHOLD:
  {
    KEY_CACHE *key_cache;
    if (!(key_cache= get_or_create_key_cache(keyname, key_length)))
      exit(1);
    switch (option->id) {
    case OPT_KEY_BUFFER_SIZE:
      return (uchar**) &key_cache->param_buff_size;
    case OPT_KEY_CACHE_BLOCK_SIZE:
      return (uchar**) &key_cache->param_block_size;
    case OPT_KEY_CACHE_DIVISION_LIMIT:
      return (uchar**) &key_cache->param_division_limit;
    case OPT_KEY_CACHE_AGE_THRESHOLD:
      return (uchar**) &key_cache->param_age_threshold;
    }
  }
  }
  return option->value;
}


extern "C" void option_error_reporter(enum loglevel level, const char *format, ...);

void option_error_reporter(enum loglevel level, const char *format, ...)
{
  va_list args;
  va_start(args, format);

  /* Don't print warnings for --loose options during bootstrap */
  if (level == ERROR_LEVEL || !opt_bootstrap ||
      global_system_variables.log_warnings)
  {
    vprint_msg_to_log(level, format, args);
  }
  va_end(args);
}


/**
  @todo
  - FIXME add EXIT_TOO_MANY_ARGUMENTS to "mysys_err.h" and return that code?
*/
static void get_options(int *argc,char **argv)
{
  int ho_error;

  my_getopt_register_get_addr(mysql_getopt_value);
  my_getopt_error_reporter= option_error_reporter;

  /* Skip unknown options so that they may be processed later by plugins */
  my_getopt_skip_unknown= TRUE;

  if ((ho_error= handle_options(argc, &argv, my_long_options,
                                mysqld_get_one_option)))
    exit(ho_error);
  (*argc)++; /* add back one for the progname handle_options removes */
             /* no need to do this for argv as we are discarding it. */

  if ((opt_log_slow_admin_statements || opt_log_queries_not_using_indexes ||
       opt_log_slow_slave_statements) &&
      !opt_slow_log)
    sql_print_warning("options --log-slow-admin-statements, --log-queries-not-using-indexes and --log-slow-slave-statements have no effect if --log-slow-queries is not set");

#if defined(HAVE_BROKEN_REALPATH)
  my_use_symdir=0;
  my_disable_symlinks=1;
  have_symlink=SHOW_OPTION_NO;
#else
  if (!my_use_symdir)
  {
    my_disable_symlinks=1;
    have_symlink=SHOW_OPTION_DISABLED;
  }
#endif
  if (opt_debugging)
  {
    /* Allow break with SIGINT, no core or stack trace */
    test_flags|= TEST_SIGINT | TEST_NO_STACKTRACE;
    test_flags&= ~TEST_CORE_ON_SIGNAL;
  }
  /* Set global MyISAM variables from delay_key_write_options */
  fix_delay_key_write((THD*) 0, OPT_GLOBAL);
  /* Set global slave_exec_mode from its option */
  fix_slave_exec_mode(OPT_GLOBAL);

  if (mysqld_chroot)
    set_root(mysqld_chroot);
  fix_paths();

  /*
    Set some global variables from the global_system_variables
    In most cases the global variables will not be used
  */
  my_disable_locking= myisam_single_user= test(opt_external_locking == 0);
  my_default_record_cache_size=global_system_variables.read_buff_size;
  myisam_max_temp_length=
    (my_off_t) global_system_variables.myisam_max_sort_file_size;

  /* Set global variables based on startup options */
  myisam_block_size=(uint) 1 << my_bit_log2(opt_myisam_block_size);

  /* long_query_time is in microseconds */
  global_system_variables.long_query_time= max_system_variables.long_query_time=
    (longlong) (long_query_time * 1000000.0);

  if (opt_short_log_format)
    opt_specialflag|= SPECIAL_SHORT_LOG_FORMAT;

  if (init_global_datetime_format(MYSQL_TIMESTAMP_DATE,
				  &global_system_variables.date_format) ||
      init_global_datetime_format(MYSQL_TIMESTAMP_TIME,
				  &global_system_variables.time_format) ||
      init_global_datetime_format(MYSQL_TIMESTAMP_DATETIME,
				  &global_system_variables.datetime_format))
    exit(1);

  pool_of_threads_scheduler(&thread_scheduler);  /* purecov: tested */
}


/*
  Create version name for running mysqld version
  We automaticly add suffixes -debug, -embedded and -log to the version
  name to make the version more descriptive.
  (MYSQL_SERVER_SUFFIX is set by the compilation environment)
*/

static void set_server_version(void)
{
  char *end= strxmov(server_version, MYSQL_SERVER_VERSION,
                     MYSQL_SERVER_SUFFIX_STR, NullS);
#ifndef DBUG_OFF
  if (!strstr(MYSQL_SERVER_SUFFIX_STR, "-debug"))
    end= strmov(end, "-debug");
#endif
  if (opt_log || opt_update_log || opt_slow_log || opt_bin_log)
    strmov(end, "-log");                        // This may slow down system
}


static char *get_relative_path(const char *path)
{
  if (test_if_hard_path(path) &&
      is_prefix(path,DEFAULT_MYSQL_HOME) &&
      strcmp(DEFAULT_MYSQL_HOME,FN_ROOTDIR))
  {
    path+=(uint) strlen(DEFAULT_MYSQL_HOME);
    while (*path == FN_LIBCHAR)
      path++;
  }
  return (char*) path;
}


/**
  Fix filename and replace extension where 'dir' is relative to
  mysql_real_data_home.
  @return
    1 if len(path) > FN_REFLEN
*/

bool
fn_format_relative_to_data_home(char * to, const char *name,
				const char *dir, const char *extension)
{
  char tmp_path[FN_REFLEN];
  if (!test_if_hard_path(dir))
  {
    strxnmov(tmp_path,sizeof(tmp_path)-1, mysql_real_data_home,
	     dir, NullS);
    dir=tmp_path;
  }
  return !fn_format(to, name, dir, extension,
		    MY_APPEND_EXT | MY_UNPACK_FILENAME | MY_SAFE_PATH);
}


static void fix_paths(void)
{
  char buff[FN_REFLEN],*pos;
  convert_dirname(mysql_home,mysql_home,NullS);
  /* Resolve symlinks to allow 'mysql_home' to be a relative symlink */
  my_realpath(mysql_home,mysql_home,MYF(0));
  /* Ensure that mysql_home ends in FN_LIBCHAR */
  pos=strend(mysql_home);
  if (pos[-1] != FN_LIBCHAR)
  {
    pos[0]= FN_LIBCHAR;
    pos[1]= 0;
  }
  convert_dirname(mysql_real_data_home,mysql_real_data_home,NullS);
  (void) fn_format(buff, mysql_real_data_home, "", "",
                   (MY_RETURN_REAL_PATH|MY_RESOLVE_SYMLINKS));
  (void) unpack_dirname(mysql_unpacked_real_data_home, buff);
  convert_dirname(language,language,NullS);
  (void) my_load_path(mysql_home,mysql_home,""); // Resolve current dir
  (void) my_load_path(mysql_real_data_home,mysql_real_data_home,mysql_home);
  (void) my_load_path(pidfile_name,pidfile_name,mysql_real_data_home);
  (void) my_load_path(opt_plugin_dir, opt_plugin_dir_ptr ? opt_plugin_dir_ptr :
                                      get_relative_path(PLUGINDIR), mysql_home);
  opt_plugin_dir_ptr= opt_plugin_dir;

  char *sharedir=get_relative_path(SHAREDIR);
  if (test_if_hard_path(sharedir))
    strmake(buff,sharedir,sizeof(buff)-1);		/* purecov: tested */
  else
    strxnmov(buff,sizeof(buff)-1,mysql_home,sharedir,NullS);
  convert_dirname(buff,buff,NullS);
  (void) my_load_path(language,language,buff);

  /* If --character-sets-dir isn't given, use shared library dir */
  if (charsets_dir != mysql_charsets_dir)
  {
    strxnmov(mysql_charsets_dir, sizeof(mysql_charsets_dir)-1, buff,
	     CHARSET_DIR, NullS);
  }
  (void) my_load_path(mysql_charsets_dir, mysql_charsets_dir, buff);
  convert_dirname(mysql_charsets_dir, mysql_charsets_dir, NullS);
  charsets_dir=mysql_charsets_dir;

  if (init_tmpdir(&mysql_tmpdir_list, opt_mysql_tmpdir))
    exit(1);
  if (!slave_load_tmpdir)
  {
    if (!(slave_load_tmpdir = (char*) my_strdup(mysql_tmpdir, MYF(MY_FAE))))
      exit(1);
  }
  /*
    Convert the secure-file-priv option to system format, allowing
    a quick strcmp to check if read or write is in an allowed dir
   */
  if (opt_secure_file_priv)
  {
    convert_dirname(buff, opt_secure_file_priv, NullS);
    my_free(opt_secure_file_priv, MYF(0));
    opt_secure_file_priv= my_strdup(buff, MYF(MY_FAE));
  }
}


static ulong find_bit_type_or_exit(const char *x, TYPELIB *bit_lib,
                                   const char *option)
{
  ulong res;

  const char **ptr;
  
  if ((res= find_bit_type(x, bit_lib)) == ~(ulong) 0)
  {
    ptr= bit_lib->type_names;
    if (!*x)
      fprintf(stderr, "No option given to %s\n", option);
    else
      fprintf(stderr, "Wrong option to %s. Option(s) given: %s\n", option, x);
    fprintf(stderr, "Alternatives are: '%s'", *ptr);
    while (*++ptr)
      fprintf(stderr, ",'%s'", *ptr);
    fprintf(stderr, "\n");
    exit(1);
  }
  return res;
}


/**
  @return
    a bitfield from a string of substrings separated by ','
    or
    ~(ulong) 0 on error.
*/

static ulong find_bit_type(const char *x, TYPELIB *bit_lib)
{
  bool found_end;
  int  found_count;
  const char *end,*i,*j;
  const char **array, *pos;
  ulong found,found_int,bit;
  DBUG_ENTER("find_bit_type");
  DBUG_PRINT("enter",("x: '%s'",x));

  found=0;
  found_end= 0;
  pos=(char *) x;
  while (*pos == ' ') pos++;
  found_end= *pos == 0;
  while (!found_end)
  {
    if (!*(end=strcend(pos,',')))		/* Let end point at fieldend */
    {
      while (end > pos && end[-1] == ' ')
	end--;					/* Skip end-space */
      found_end=1;
    }
    found_int=0; found_count=0;
    for (array=bit_lib->type_names, bit=1 ; (i= *array++) ; bit<<=1)
    {
      j=pos;
      while (j != end)
      {
	if (my_toupper(mysqld_charset,*i++) !=
            my_toupper(mysqld_charset,*j++))
	  goto skip;
      }
      found_int=bit;
      if (! *i)
      {
	found_count=1;
	break;
      }
      else if (j != pos)			// Half field found
      {
	found_count++;				// Could be one of two values
      }
skip: ;
    }
    if (found_count != 1)
      DBUG_RETURN(~(ulong) 0);				// No unique value
    found|=found_int;
    pos=end+1;
  }

  DBUG_PRINT("exit",("bit-field: %ld",(ulong) found));
  DBUG_RETURN(found);
} /* find_bit_type */


/**
  Check if file system used for databases is case insensitive.

  @param dir_name			Directory to test

  @retval
    -1  Don't know (Test failed)
  @retval
    0   File system is case sensitive
  @retval
    1   File system is case insensitive
*/

static int test_if_case_insensitive(const char *dir_name)
{
  int result= 0;
  File file;
  char buff[FN_REFLEN], buff2[FN_REFLEN];
  struct stat stat_info;
  DBUG_ENTER("test_if_case_insensitive");

  fn_format(buff, glob_hostname, dir_name, ".lower-test",
	    MY_UNPACK_FILENAME | MY_REPLACE_EXT | MY_REPLACE_DIR);
  fn_format(buff2, glob_hostname, dir_name, ".LOWER-TEST",
	    MY_UNPACK_FILENAME | MY_REPLACE_EXT | MY_REPLACE_DIR);
  (void) my_delete(buff2, MYF(0));
  if ((file= my_create(buff, 0666, O_RDWR, MYF(0))) < 0)
  {
    sql_print_warning("Can't create test file %s", buff);
    DBUG_RETURN(-1);
  }
  my_close(file, MYF(0));
  if (!stat(buff2, &stat_info))
    result= 1;					// Can access file
  (void) my_delete(buff, MYF(MY_WME));
  DBUG_PRINT("exit", ("result: %d", result));
  DBUG_RETURN(result);
}


/**
  Create file to store pid number.
*/
static void create_pid_file()
{
  File file;
  if ((file = my_create(pidfile_name,0664,
			O_WRONLY | O_TRUNC, MYF(MY_WME))) >= 0)
  {
    char buff[21], *end;
    end= int10_to_str((long) getpid(), buff, 10);
    *end++= '\n';
    if (!my_write(file, (uchar*) buff, (uint) (end-buff), MYF(MY_WME | MY_NABP)))
    {
      (void) my_close(file, MYF(0));
      return;
    }
    (void) my_close(file, MYF(0));
  }
  sql_perror("Can't start server: can't create PID file");
  exit(1);
}

/** Clear most status variables. */
void refresh_status(THD *thd)
{
  pthread_mutex_lock(&LOCK_status);

  /* Add thread's status variabes to global status */
  add_to_status(&global_status_var, &thd->status_var);

  /* Reset thread's status variables */
  bzero((uchar*) &thd->status_var, sizeof(thd->status_var));

  /* Reset some global variables */
  reset_status_vars();

  /* Reset the counters of all key caches (default and named). */
  process_key_caches(reset_key_cache_counters);
  flush_status_time= time((time_t*) 0);
  pthread_mutex_unlock(&LOCK_status);

  /*
    Set max_used_connections to the number of currently open
    connections.  Lock LOCK_thread_count out of LOCK_status to avoid
    deadlocks.  Status reset becomes not atomic, but status data is
    not exact anyway.
  */
  pthread_mutex_lock(&LOCK_thread_count);
  max_used_connections= thread_count;
  pthread_mutex_unlock(&LOCK_thread_count);
}


/*****************************************************************************
  Instantiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
/* Used templates */
template class I_List<THD>;
template class I_List_iterator<THD>;
template class I_List<i_string>;
template class I_List<i_string_pair>;
template class I_List<NAMED_LIST>;
template class I_List<Statement>;
template class I_List_iterator<Statement>;
#endif
