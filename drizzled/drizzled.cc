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


#include <drizzled/configmake.h>
#include <drizzled/server_includes.h>
#include <drizzled/atomics.h>

#include <netdb.h>
#include <sys/poll.h>
#include <netinet/tcp.h>
#include <signal.h>

#include <mysys/my_bit.h>
#include <mysys/hash.h>
#include <drizzled/stacktrace.h>
#include <mysys/mysys_err.h>
#include <drizzled/error.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/tztime.h>
#include <drizzled/sql_base.h>
#include <drizzled/show.h>
#include <drizzled/sql_parse.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/session.h>
#include <drizzled/db.h>
#include <drizzled/item/create.h>
#include <drizzled/errmsg.h>
#include <drizzled/unireg.h>
#include <drizzled/scheduling.h>
#include "drizzled/temporal_format.h" /* For init_temporal_formats() */

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <storage/myisam/ha_myisam.h>

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include <locale.h>

#define mysqld_charset &my_charset_utf8_general_ci

#ifdef HAVE_purify
#define IF_PURIFY(A,B) (A)
#else
#define IF_PURIFY(A,B) (B)
#endif

#define MAX_MEM_TABLE_SIZE SIZE_MAX

extern "C" {					// Because of SCO 3.2V4.2
#include <errno.h>
#include <sys/stat.h>
#include <mysys/my_getopt.h>
#ifdef HAVE_SYSENT_H
#include <sysent.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>				// For getpwent
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

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

#include <mysys/my_pthread.h>			// For thr_setconcurency()

#include <drizzled/gettext.h>

#ifdef SOLARIS
extern "C" int gethostname(char *name, int namelen);
#endif

extern "C" void handle_segfault(int sig);

using namespace std;

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
  NULL
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
  "COMMIT", "ROLLBACK", NULL
};
static TYPELIB tc_heuristic_recover_typelib=
{
  array_elements(tc_heuristic_recover_names)-1,"",
  tc_heuristic_recover_names, NULL
};

const char *first_keyword= "first", *binary_keyword= "BINARY";
const char *my_localhost= "localhost";
const char * const DRIZZLE_CONFIG_NAME= "drizzled";
#define GET_HA_ROWS GET_ULL

/*
  Used with --help for detailed option
*/
static bool opt_help= false;

arg_cmp_func Arg_comparator::comparator_matrix[5][2] =
{{&Arg_comparator::compare_string,     &Arg_comparator::compare_e_string},
 {&Arg_comparator::compare_real,       &Arg_comparator::compare_e_real},
 {&Arg_comparator::compare_int_signed, &Arg_comparator::compare_e_int},
 {&Arg_comparator::compare_row,        &Arg_comparator::compare_e_row},
 {&Arg_comparator::compare_decimal,    &Arg_comparator::compare_e_decimal}};

/* static variables */

/* the default log output is log tables */
static bool volatile select_thread_in_use;
static bool volatile ready_to_exit;
static bool opt_debugging= 0;
static uint32_t wake_thread;
static uint32_t killed_threads;
static char *drizzled_user, *drizzled_chroot;
static char *language_ptr, *opt_init_connect;
static char *default_character_set_name;
static char *character_set_filesystem_name;
static char *lc_time_names_name;
static char *my_bind_addr_str;
static char *default_collation_name;
static char *default_storage_engine_str;
static char compiled_default_collation_name[]= DRIZZLE_DEFAULT_COLLATION_NAME;
static struct pollfd fds[UINT8_MAX];
static uint8_t pollfd_count= 0;

/* Global variables */

bool server_id_supplied = 0;
bool opt_endinfo, using_udf_functions;
bool locked_in_memory;
bool volatile abort_loop;
bool volatile shutdown_in_progress;
uint32_t max_used_connections;
const char *opt_scheduler= "multi_thread";

size_t my_thread_stack_size= 65536;

/*
  Legacy global StorageEngine. These will be removed (please do not add more).
*/
StorageEngine *heap_engine;
StorageEngine *myisam_engine;

bool use_temp_pool;
char* opt_secure_file_priv= 0;
/*
  True if there is at least one per-hour limit for some user, so we should
  check them before each query (and possibly reset counters when hour is
  changed). False otherwise.
*/
bool opt_noacl;

#ifdef HAVE_INITGROUPS
static bool calling_initgroups= false; /**< Used in SIGSEGV handler. */
#endif
uint32_t drizzled_port, test_flags, select_errors, dropping_tables, ha_open_options;
uint32_t drizzled_port_timeout;
uint32_t delay_key_write_options, protocol_version= PROTOCOL_VERSION;
uint32_t lower_case_table_names= 1;
uint32_t tc_heuristic_recover= 0;
uint64_t session_startup_options;
uint32_t back_log;
uint32_t connect_timeout;
uint32_t server_id;
uint64_t table_cache_size;
uint64_t table_def_size;
uint32_t refresh_version;  /* Increments on each reload */
uint64_t aborted_threads;
uint64_t aborted_connects;
uint64_t max_connect_errors;
uint32_t thread_id=1L;
pid_t current_pid;

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

time_t server_start_time;
time_t flush_status_time;

/* FRM Junk */
const char *reg_ext= ".frm";
uint32_t reg_ext_length= 4;

char drizzle_home[FN_REFLEN], pidfile_name[FN_REFLEN], system_time_zone[30];
char *default_tz_name;
char glob_hostname[FN_REFLEN];
char drizzle_real_data_home[FN_REFLEN],
     language[FN_REFLEN], 
     *opt_init_file, 
     *opt_tc_log_file;
char drizzle_unpacked_real_data_home[FN_REFLEN];
const key_map key_map_empty(0);
key_map key_map_full(0);                        // Will be initialized later

uint32_t drizzle_data_home_len;
char drizzle_data_home_buff[2], *drizzle_data_home=drizzle_real_data_home;
char server_version[SERVER_VERSION_LENGTH];
char *drizzle_tmpdir= NULL;
char *opt_drizzle_tmpdir= NULL;
const char *myisam_recover_options_str="OFF";
const char *myisam_stats_method_str="nulls_unequal";

/** name of reference on left espression in rewritten IN subquery */
const char *in_left_expr_name= "<left expr>";
/** name of additional condition */
const char *in_additional_cond= "<IN COND>";
const char *in_having_cond= "<IN HAVING>";

my_decimal decimal_zero;
/* classes for comparation parsing/processing */

FILE *stderror_file=0;

I_List<Session> session_list;
I_List<NAMED_LIST> key_caches;

struct system_variables global_system_variables;
struct system_variables max_system_variables;
struct system_status_var global_status_var;

MY_BITMAP temp_pool;

const CHARSET_INFO *system_charset_info, *files_charset_info ;
const CHARSET_INFO *table_alias_charset;
const CHARSET_INFO *character_set_filesystem;

MY_LOCALE *my_default_lc_time_names;

SHOW_COMP_OPTION have_symlink;

/* Thread specific variables */

pthread_key_t THR_Mem_root;
pthread_key_t THR_Session;
pthread_mutex_t LOCK_create_db, 
                LOCK_open, 
                LOCK_thread_count,
                LOCK_status,
                LOCK_global_read_lock,
                LOCK_global_system_variables;

pthread_rwlock_t	LOCK_sys_init_connect;
pthread_rwlock_t	LOCK_system_variables_hash;
pthread_cond_t COND_refresh, COND_thread_count, COND_global_read_lock;
pthread_t signal_thread;
pthread_cond_t  COND_server_started;

/* replication parameters, if master_host is not NULL, we are a slave */
uint32_t report_port= DRIZZLE_PORT;
uint32_t master_retry_count= 0;
char *master_info_file;
char *report_host;
char *opt_logname;

/* Static variables */

static bool segfaulted;
#ifdef HAVE_STACK_TRACE_ON_SEGV
static bool opt_do_pstack;
#endif /* HAVE_STACK_TRACE_ON_SEGV */
int cleanup_done;
static char *drizzle_home_ptr, *pidfile_name_ptr;
static int defaults_argc;
static char **defaults_argv;

struct passwd *user_info;
static pthread_t select_thread;
static uint32_t thr_kill_signal;

/**
  Number of currently active user connections. The variable is protected by
  LOCK_thread_count.
*/
drizzled::atomic<uint32_t> connection_count;

/* Function declarations */

extern "C" pthread_handler_t signal_hand(void *arg);
static void drizzle_init_variables(void);
static void get_options(int *argc,char **argv);
extern "C" bool drizzled_get_one_option(int, const struct my_option *, char *);
static void set_server_version(void);
static int init_thread_environment();
static const char *get_relative_path(const char *path);
static void fix_paths(void);
void handle_connections_sockets();
extern "C" pthread_handler_t handle_slave(void *arg);
static uint32_t find_bit_type(const char *x, TYPELIB *bit_lib);
static uint32_t find_bit_type_or_exit(const char *x, TYPELIB *bit_lib,
                                   const char *option);
static void clean_up(bool print_message);

static void usage(void);
static void clean_up_mutexes(void);

/****************************************************************************
** Code to end drizzled
****************************************************************************/

void close_connections(void)
{
  int x;

  /* Abort listening to new connections */
  for (x= 0; x < pollfd_count; x++)
  {
    if (fds[x].fd != -1)
    {
      (void) shutdown(fds[x].fd, SHUT_RDWR);
      (void) close(fds[x].fd);
      fds[x].fd= -1;
    }
  }


  /* kill connection thread */
  (void) pthread_mutex_lock(&LOCK_thread_count);

  while (select_thread_in_use)
  {
    struct timespec abstime;
    int error;

    set_timespec(abstime, 2);
    for (uint32_t tmp=0 ; tmp < 10 && select_thread_in_use; tmp++)
    {
      error=pthread_cond_timedwait(&COND_thread_count,&LOCK_thread_count, &abstime);
      if (error != EINTR)
        break;
    }
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);


  /*
    First signal all threads that it's time to die
    This will give the threads some time to gracefully abort their
    statements and inform their clients that the server is about to die.
  */

  Session *tmp;
  Scheduler &thread_scheduler= get_thread_scheduler();

  (void) pthread_mutex_lock(&LOCK_thread_count); // For unlink from list

  I_List_iterator<Session> it(session_list);
  while ((tmp=it++))
  {
    tmp->killed= Session::KILL_CONNECTION;
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

  if (connection_count)
    sleep(2);                                   // Give threads time to die

  /*
    Force remaining threads to die by closing the connection to the client
    This will ensure that threads that are waiting for a command from the
    client on a blocking read call are aborted.
  */
  for (;;)
  {
    (void) pthread_mutex_lock(&LOCK_thread_count); // For unlink from list
    if (!(tmp= session_list.get()))
    {
      (void) pthread_mutex_unlock(&LOCK_thread_count);
      break;
    }
    (void) pthread_mutex_unlock(&LOCK_thread_count);
    unlink_session(tmp);
  }
  assert(session_list.is_empty());
}


extern "C" void print_signal_warning(int sig)
{
  if (global_system_variables.log_warnings)
    errmsg_printf(ERRMSG_LVL_WARN, _("Got signal %d from thread %"PRIu64), sig,my_thread_id());
#ifndef HAVE_BSD_SIGNALS
  my_sigset(sig,print_signal_warning);		/* int. thread system calls */
#endif
  if (sig == SIGALRM)
    alarm(2);					/* reschedule alarm */
}

/**
  cleanup all memory and end program nicely.

    If SIGNALS_DONT_BREAK_READ is defined, this function is called
    by the main thread. To get Drizzle to shut down nicely in this case
    (Mac OS X) we have to call exit() instead if pthread_exit().

  @note
    This function never returns.
*/
extern "C" void unireg_end(void)
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

  if (exit_code)
    errmsg_printf(ERRMSG_LVL_ERROR, _("Aborting\n"));
  else if (opt_help)
    usage();
  clean_up(!opt_help && (exit_code)); /* purecov: inspected */
  clean_up_mutexes();
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  exit(exit_code); /* purecov: inspected */
}


static void clean_up(bool print_message)
{
  if (cleanup_done++)
    return; /* purecov: inspected */

  table_cache_free();
  table_def_free();
  set_var_free();
  free_charsets();
  plugin_shutdown();
  ha_end();
  xid_cache_free();
  delete_elements(&key_caches, (void (*)(const char*, unsigned char*)) free_key_cache);
  multi_keycache_free();
  free_status_vars();
  my_free_open_file_info();
  if (defaults_argv)
    free_defaults(defaults_argv);
  free(sys_init_connect.value);
  free(drizzle_tmpdir);
  if (opt_secure_file_priv)
    free(opt_secure_file_priv);
  bitmap_free(&temp_pool);

  (void) unlink(pidfile_name);	// This may not always exist

  if (print_message && server_start_time)
    errmsg_printf(ERRMSG_LVL_INFO, _(ER(ER_SHUTDOWN_COMPLETE)),my_progname);
  /* Returns NULL on globerrs, we don't want to try to free that */
  //void *freeme=
  (void *)my_error_unregister(ER_ERROR_FIRST, ER_ERROR_LAST);
  // TODO!!!! EPIC FAIL!!!! This sefaults if uncommented.
/*  if (freeme != NULL)
    free(freeme);  */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  ready_to_exit=1;
  /* do the broadcast inside the lock to ensure that my_end() is not called */
  (void) pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  /*
    The following lines may never be executed as the main thread may have
    killed us
  */
} /* clean_up */


static void clean_up_mutexes()
{
  (void) pthread_mutex_destroy(&LOCK_create_db);
  (void) pthread_mutex_destroy(&LOCK_lock_db);
  (void) pthread_mutex_destroy(&LOCK_open);
  (void) pthread_mutex_destroy(&LOCK_thread_count);
  (void) pthread_mutex_destroy(&LOCK_status);
  (void) pthread_rwlock_destroy(&LOCK_sys_init_connect);
  (void) pthread_mutex_destroy(&LOCK_global_system_variables);
  (void) pthread_rwlock_destroy(&LOCK_system_variables_hash);
  (void) pthread_mutex_destroy(&LOCK_global_read_lock);
  (void) pthread_cond_destroy(&COND_thread_count);
  (void) pthread_cond_destroy(&COND_refresh);
  (void) pthread_cond_destroy(&COND_global_read_lock);
}


/****************************************************************************
** Init IP and UNIX socket
****************************************************************************/

static void set_ports()
{
  char	*env;
  if (!drizzled_port)
  {					// Get port if not from commandline
    drizzled_port= DRIZZLE_PORT;

    /*
      if builder specifically requested a default port, use that
      (even if it coincides with our factory default).
      only if they didn't do we check /etc/services (and, failing
      on that, fall back to the factory default of 4427).
      either default can be overridden by the environment variable
      DRIZZLE_TCP_PORT, which in turn can be overridden with command
      line options.
    */

    struct  servent *serv_ptr;
    if ((serv_ptr= getservbyname("drizzle", "tcp")))
      drizzled_port= ntohs((u_short) serv_ptr->s_port); /* purecov: inspected */

    if ((env = getenv("DRIZZLE_TCP_PORT")))
      drizzled_port= (uint32_t) atoi(env);		/* purecov: inspected */

    assert(drizzled_port);
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
            errmsg_printf(ERRMSG_LVL_WARN, _("One can only use the --user switch "
                            "if running as root\n"));
      /* purecov: end */
    }
    return NULL;
  }
  if (!user)
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Fatal error: Please read \"Security\" section of "
                      "the manual to find out how to run drizzled as root!\n"));
    unireg_abort(1);
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
  errmsg_printf(ERRMSG_LVL_ERROR, _("Fatal error: Can't change to run as user '%s' ;  "
                    "Please check that the user exists!\n"),user);
  unireg_abort(1);

#ifdef PR_SET_DUMPABLE
  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    /* inform kernel that process is dumpable */
    (void) prctl(PR_SET_DUMPABLE, 1);
  }
#endif

/* Sun Studio 5.10 doesn't like this line.  5.9 requires it */
#if defined(__SUNPRO_CC) && (__SUNPRO_CC <= 0x590)
  return NULL;
#endif

}

static void set_user(const char *user, struct passwd *user_info_arg)
{
  /* purecov: begin tested */
  assert(user_info_arg != 0);
#ifdef HAVE_INITGROUPS
  /*
    We can get a SIGSEGV when calling initgroups() on some systems when NSS
    is configured to use LDAP and the server is statically linked.  We set
    calling_initgroups as a flag to the SIGSEGV handler that is then used to
    output a specific message to help the user resolve this problem.
  */
  calling_initgroups= true;
  initgroups((char*) user, user_info_arg->pw_gid);
  calling_initgroups= false;
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
  assert(user_info_arg != 0);
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
  if ((chroot(path) == -1) || !chdir("/"))
  {
    sql_perror("chroot");
    unireg_abort(1);
  }
}


static void network_init(void)
{
  int   ret;
  uint32_t  waited;
  uint32_t  this_wait;
  uint32_t  retry;
  char port_buf[NI_MAXSERV];
  struct addrinfo *ai;
  struct addrinfo *next;
  struct addrinfo hints;
  int error;

  set_ports();

  memset(fds, 0, sizeof(struct pollfd) * UINT8_MAX);
  memset(&hints, 0, sizeof (hints));
  hints.ai_flags= AI_PASSIVE;
  hints.ai_socktype= SOCK_STREAM;

  snprintf(port_buf, NI_MAXSERV, "%d", drizzled_port);
  error= getaddrinfo(my_bind_addr_str, port_buf, &hints, &ai);
  if (error != 0)
  {
    sql_perror(ER(ER_IPSOCK_ERROR));		/* purecov: tested */
    unireg_abort(1);				/* purecov: tested */
  }

  for (next= ai, pollfd_count= 0; next; next= next->ai_next, pollfd_count++)
  {
    int ip_sock;

    ip_sock= socket(next->ai_family, next->ai_socktype, next->ai_protocol);

    if (ip_sock == -1)
    {
      sql_perror(ER(ER_IPSOCK_ERROR));		/* purecov: tested */
      unireg_abort(1);				/* purecov: tested */
    }

    fds[pollfd_count].fd= ip_sock;
    fds[pollfd_count].events= POLLIN | POLLERR;

    /* Add options for our listening socket */
    {
      struct linger ling = {0, 0};
      int flags =1;

#ifdef IPV6_V6ONLY
      if (next->ai_family == AF_INET6)
      {
        error= setsockopt(ip_sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &flags, sizeof(flags));
        if (error != 0)
        {
          perror("setsockopt");
          assert(error == 0);
        }
      }
#endif
      error= setsockopt(ip_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&flags, sizeof(flags));
      if (error != 0)
      {
        perror("setsockopt");
        assert(error == 0);
      }
      error= setsockopt(ip_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
      if (error != 0)
      {
        perror("setsockopt");
        assert(error == 0);
      }
      error= setsockopt(ip_sock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
      if (error != 0)
      {
        perror("setsockopt");
        assert(error == 0);
      }
      error= setsockopt(ip_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));
      if (error != 0)
      {
        perror("setsockopt");
        assert(error == 0);
      }
    }


    /*
      Sometimes the port is not released fast enough when stopping and
      restarting the server. This happens quite often with the test suite
      on busy Linux systems. Retry to bind the address at these intervals:
      Sleep intervals: 1, 2, 4,  6,  9, 13, 17, 22, ...
      Retry at second: 1, 3, 7, 13, 22, 35, 52, 74, ...
      Limit the sequence by drizzled_port_timeout (set --port-open-timeout=#).
    */
    for (waited= 0, retry= 1; ; retry++, waited+= this_wait)
    {
      if (((ret= ::bind(ip_sock, next->ai_addr, next->ai_addrlen)) >= 0 ) ||
          (errno != EADDRINUSE) ||
          (waited >= drizzled_port_timeout))
        break;
          errmsg_printf(ERRMSG_LVL_INFO, _("Retrying bind on TCP/IP port %u"), drizzled_port);
      this_wait= retry * retry / 3 + 1;
      sleep(this_wait);
    }
    if (ret < 0)
    {
      sql_perror(_("Can't start server: Bind on TCP/IP port"));
          errmsg_printf(ERRMSG_LVL_ERROR, _("Do you already have another drizzled server running "
                        "on port: %d ?"),drizzled_port);
      unireg_abort(1);
    }
    if (listen(ip_sock,(int) back_log) < 0)
    {
      sql_perror(_("Can't start server: listen() on TCP/IP port"));
          errmsg_printf(ERRMSG_LVL_ERROR, _("listen() on TCP/IP failed with error %d"),
                      errno);
      unireg_abort(1);
    }
  }

  freeaddrinfo(ai);
  return;
}



/** Called when a thread is aborted. */
/* ARGSUSED */
extern "C" void end_thread_signal(int )
{
  Session *session=current_session;
  if (session)
  {
    statistic_increment(killed_threads, &LOCK_status);
    Scheduler &thread_scheduler= get_thread_scheduler();
    (void)thread_scheduler.end_thread(session, 0);
  }
  return;				/* purecov: deadcode */
}


/*
  Unlink session from global list of available connections and free session

  SYNOPSIS
    unlink_session()
    session		 Thread handler

  NOTES
    LOCK_thread_count is locked and left locked
*/

void unlink_session(Session *session)
{
  connection_count--;

  session->cleanup();

  (void) pthread_mutex_lock(&LOCK_thread_count);
  pthread_mutex_lock(&session->LOCK_delete);
  delete session;
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  return;
}


#ifdef THREAD_SPECIFIC_SIGPIPE
/**
  Aborts a thread nicely. Comes here on SIGPIPE.

  @todo
    One should have to fix that thr_alarm know about this thread too.
*/
extern "C" void abort_thread(int )
{
  Session *session=current_session;
  if (session)
    session->killed= Session::KILL_CONNECTION;
  return;;
}
#endif

#if defined(BACKTRACE_DEMANGLE)
#include <cxxabi.h>
extern "C" char *my_demangle(const char *mangled_name, int *status)
{
  return abi::__cxa_demangle(mangled_name, NULL, NULL, status);
}
#endif


extern "C" void handle_segfault(int sig)
{
  time_t curr_time;
  struct tm tm;

  /*
    Strictly speaking, one needs a mutex here
    but since we have got SIGSEGV already, things are a mess
    so not having the mutex is not as bad as possibly using a buggy
    mutex - so we keep things simple
  */
  if (segfaulted)
  {
    fprintf(stderr, _("Fatal " SIGNAL_FMT " while backtracing\n"), sig);
    exit(1);
  }

  segfaulted = 1;

  curr_time= time(NULL);
  if(curr_time == (time_t)-1)
  {
    fprintf(stderr, "Fetal: time() call failed\n");
    exit(1);
  }

  localtime_r(&curr_time, &tm);
  Scheduler &thread_scheduler= get_thread_scheduler();
  
  fprintf(stderr,"%02d%02d%02d %2d:%02d:%02d - drizzled got "
          SIGNAL_FMT " ;\n"
          "This could be because you hit a bug. It is also possible that "
          "this binary\n or one of the libraries it was linked against is "
          "corrupt, improperly built,\n or misconfigured. This error can "
          "also be caused by malfunctioning hardware.\n",
          tm.tm_year % 100, tm.tm_mon+1, tm.tm_mday,
          tm.tm_hour, tm.tm_min, tm.tm_sec,
          sig);
  fprintf(stderr, _("We will try our best to scrape up some info that "
                    "will hopefully help diagnose\n"
                    "the problem, but since we have already crashed, "
                    "something is definitely wrong\nand this may fail.\n\n"));
  fprintf(stderr, "key_buffer_size=%u\n",
          (uint32_t) dflt_key_cache->key_cache_mem_size);
  fprintf(stderr, "read_buffer_size=%ld\n", (long) global_system_variables.read_buff_size);
  fprintf(stderr, "max_used_connections=%u\n", max_used_connections);
  fprintf(stderr, "max_threads=%u\n", thread_scheduler.get_max_threads());
  fprintf(stderr, "thread_count=%u\n", thread_scheduler.count());
  fprintf(stderr, "connection_count=%u\n", uint32_t(connection_count));
  fprintf(stderr, _("It is possible that drizzled could use up to \n"
                    "key_buffer_size + (read_buffer_size + "
                    "sort_buffer_size)*max_threads = %"PRIu64" K\n"
                    "bytes of memory\n"
                    "Hope that's ok; if not, decrease some variables in the "
                    "equation.\n\n"),
          (uint64_t)(((uint32_t) dflt_key_cache->key_cache_mem_size +
                     (global_system_variables.read_buff_size +
                      global_system_variables.sortbuff_size) *
                     thread_scheduler.get_max_threads()) / 1024));

#ifdef HAVE_STACKTRACE
  Session *session= current_session;

  if (!(test_flags & TEST_NO_STACKTRACE))
  {
    fprintf(stderr,"session: 0x%lx\n",(long) session);
    fprintf(stderr,_("Attempting backtrace. You can use the following "
                     "information to find out\n"
                     "where drizzled died. If you see no messages after this, "
                     "something went\n"
                     "terribly wrong...\n"));
    print_stacktrace(session ? (unsigned char*) session->thread_stack : (unsigned char*) 0,
                     my_thread_stack_size);
  }
  if (session)
  {
    const char *kreason= "UNKNOWN";
    switch (session->killed) {
    case Session::NOT_KILLED:
      kreason= "NOT_KILLED";
      break;
    case Session::KILL_BAD_DATA:
      kreason= "KILL_BAD_DATA";
      break;
    case Session::KILL_CONNECTION:
      kreason= "KILL_CONNECTION";
      break;
    case Session::KILL_QUERY:
      kreason= "KILL_QUERY";
      break;
    case Session::KILLED_NO_VALUE:
      kreason= "KILLED_NO_VALUE";
      break;
    }
    fprintf(stderr, _("Trying to get some variables.\n"
                      "Some pointers may be invalid and cause the "
                      "dump to abort...\n"));
    safe_print_str("session->query", session->query, 1024);
    fprintf(stderr, "session->thread_id=%"PRIu32"\n", (uint32_t) session->thread_id);
    fprintf(stderr, "session->killed=%s\n", kreason);
  }
  fflush(stderr);
#endif /* HAVE_STACKTRACE */

#ifdef HAVE_INITGROUPS
  if (calling_initgroups)
    fprintf(stderr, _("\nThis crash occurred while the server was calling "
                      "initgroups(). This is\n"
                      "often due to the use of a drizzled that is statically "
                      "linked against glibc\n"
                      "and configured to use LDAP in /etc/nsswitch.conf. "
                      "You will need to either\n"
                      "upgrade to a version of glibc that does not have this "
                      "problem (2.3.4 or\n"
                      "later when used with nscd), disable LDAP in your "
                      "nsswitch.conf, or use a\n"
                      "drizzled that is not statically linked.\n"));
#endif

  if (thd_lib_detected == THD_LIB_LT && !getenv("LD_ASSUME_KERNEL"))
    fprintf(stderr,
            _("\nYou are running a statically-linked LinuxThreads binary "
              "on an NPTL system.\n"
              "This can result in crashes on some distributions due "
              "to LT/NPTL conflicts.\n"
              "You should either build a dynamically-linked binary, or force "
              "LinuxThreads\n"
              "to be used with the LD_ASSUME_KERNEL environment variable. "
              "Please consult\n"
              "the documentation for your distribution on how to do that.\n"));

  if (locked_in_memory)
  {
    fprintf(stderr,
            _("\nThe '--memlock' argument, which was enabled, uses system "
              "calls that are\n"
              "unreliable and unstable on some operating systems and "
              "operating-system\n"
              "versions (notably, some versions of Linux).  "
              "This crash could be due to use\n"
              "of those buggy OS calls.  You should consider whether you "
              "really need the\n"
              "'--memlock' parameter and/or consult the OS "
              "distributor about 'mlockall'\n bugs.\n"));
  }

#ifdef HAVE_WRITE_CORE
  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    fprintf(stderr, _("Writing a core file\n"));
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
    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl) && global_system_variables.log_warnings)
        errmsg_printf(ERRMSG_LVL_WARN, _("setrlimit could not change the size of core files "
                          "to 'infinity';  We may not be able to generate a "
                          "core file on signals"));
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
  return;;
}

static void check_data_home(const char *)
{}


/**
  All global error messages are sent here where the first one is stored
  for the client.
*/
/* ARGSUSED */
extern "C" void my_message_sql(uint32_t error, const char *str, myf MyFlags);

void my_message_sql(uint32_t error, const char *str, myf MyFlags)
{
  Session *session;
  /*
    Put here following assertion when situation with EE_* error codes
    will be fixed
  */
  if ((session= current_session))
  {
    if (MyFlags & ME_FATALERROR)
      session->is_fatal_error= 1;

    /*
      TODO: There are two exceptions mechanism (Session and sp_rcontext),
      this could be improved by having a common stack of handlers.
    */
    if (session->handle_error(error, str,
                          DRIZZLE_ERROR::WARN_LEVEL_ERROR))
      return;;

    /*
      session->lex->current_select == 0 if lex structure is not inited
      (not query command (COM_QUERY))
    */
    if (! (session->lex->current_select &&
        session->lex->current_select->no_error && !session->is_fatal_error))
    {
      if (! session->main_da.is_error())            // Return only first message
      {
        if (error == 0)
          error= ER_UNKNOWN_ERROR;
        if (str == NULL)
          str= ER(error);
        session->main_da.set_error_status(session, error, str);
      }
    }

    if (!session->no_warnings_for_error && !session->is_fatal_error)
    {
      /*
        Suppress infinite recursion if there a memory allocation error
        inside push_warning.
      */
      session->no_warnings_for_error= true;
      push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR, error, str);
      session->no_warnings_for_error= false;
    }
  }
  if (!session || MyFlags & ME_NOREFRESH)
    errmsg_printf(ERRMSG_LVL_ERROR, "%s: %s",my_progname,str); /* purecov: inspected */
  return;;
}


static const char *load_default_groups[]= {
DRIZZLE_CONFIG_NAME,"server", DRIZZLE_BASE_VERSION, 0, 0};

SHOW_VAR com_status_vars[]= {
  {"admin_commands",       (char*) offsetof(STATUS_VAR, com_other), SHOW_LONG_STATUS},
  {"assign_to_keycache",   (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_ASSIGN_TO_KEYCACHE]), SHOW_LONG_STATUS},
  {"alter_db",             (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_ALTER_DB]), SHOW_LONG_STATUS},
  {"alter_table",          (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_ALTER_TABLE]), SHOW_LONG_STATUS},
  {"analyze",              (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_ANALYZE]), SHOW_LONG_STATUS},
  {"begin",                (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_BEGIN]), SHOW_LONG_STATUS},
  {"change_db",            (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_CHANGE_DB]), SHOW_LONG_STATUS},
  {"check",                (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_CHECK]), SHOW_LONG_STATUS},
  {"checksum",             (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_CHECKSUM]), SHOW_LONG_STATUS},
  {"commit",               (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_COMMIT]), SHOW_LONG_STATUS},
  {"create_db",            (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_CREATE_DB]), SHOW_LONG_STATUS},
  {"create_index",         (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_CREATE_INDEX]), SHOW_LONG_STATUS},
  {"create_table",         (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_CREATE_TABLE]), SHOW_LONG_STATUS},
  {"delete",               (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_DELETE]), SHOW_LONG_STATUS},
  {"delete_multi",         (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_DELETE_MULTI]), SHOW_LONG_STATUS},
  {"drop_db",              (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_DROP_DB]), SHOW_LONG_STATUS},
  {"drop_index",           (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_DROP_INDEX]), SHOW_LONG_STATUS},
  {"drop_table",           (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_DROP_TABLE]), SHOW_LONG_STATUS},
  {"empty_query",          (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_EMPTY_QUERY]), SHOW_LONG_STATUS},
  {"flush",                (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_FLUSH]), SHOW_LONG_STATUS},
  {"insert",               (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_INSERT]), SHOW_LONG_STATUS},
  {"insert_select",        (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_INSERT_SELECT]), SHOW_LONG_STATUS},
  {"kill",                 (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_KILL]), SHOW_LONG_STATUS},
  {"load",                 (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_LOAD]), SHOW_LONG_STATUS},
  {"lock_tables",          (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_LOCK_TABLES]), SHOW_LONG_STATUS},
  {"optimize",             (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_OPTIMIZE]), SHOW_LONG_STATUS},
  {"release_savepoint",    (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_RELEASE_SAVEPOINT]), SHOW_LONG_STATUS},
  {"rename_table",         (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_RENAME_TABLE]), SHOW_LONG_STATUS},
  {"repair",               (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_REPAIR]), SHOW_LONG_STATUS},
  {"replace",              (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_REPLACE]), SHOW_LONG_STATUS},
  {"replace_select",       (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_REPLACE_SELECT]), SHOW_LONG_STATUS},
  {"rollback",             (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_ROLLBACK]), SHOW_LONG_STATUS},
  {"rollback_to_savepoint",(char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_ROLLBACK_TO_SAVEPOINT]), SHOW_LONG_STATUS},
  {"savepoint",            (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SAVEPOINT]), SHOW_LONG_STATUS},
  {"select",               (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SELECT]), SHOW_LONG_STATUS},
  {"set_option",           (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SET_OPTION]), SHOW_LONG_STATUS},
  {"show_create_db",       (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_CREATE_DB]), SHOW_LONG_STATUS},
  {"show_create_table",    (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_CREATE]), SHOW_LONG_STATUS},
  {"show_databases",       (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_DATABASES]), SHOW_LONG_STATUS},
  {"show_engine_status",   (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_ENGINE_STATUS]), SHOW_LONG_STATUS},
  {"show_errors",          (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_ERRORS]), SHOW_LONG_STATUS},
  {"show_fields",          (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_FIELDS]), SHOW_LONG_STATUS},
  {"show_keys",            (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_KEYS]), SHOW_LONG_STATUS},
  {"show_open_tables",     (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_OPEN_TABLES]), SHOW_LONG_STATUS},
  {"show_plugins",         (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_PLUGINS]), SHOW_LONG_STATUS},
  {"show_processlist",     (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_PROCESSLIST]), SHOW_LONG_STATUS},
  {"show_status",          (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_STATUS]), SHOW_LONG_STATUS},
  {"show_table_status",    (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_TABLE_STATUS]), SHOW_LONG_STATUS},
  {"show_tables",          (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_TABLES]), SHOW_LONG_STATUS},
  {"show_variables",       (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_VARIABLES]), SHOW_LONG_STATUS},
  {"show_warnings",        (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_SHOW_WARNS]), SHOW_LONG_STATUS},
  {"truncate",             (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_TRUNCATE]), SHOW_LONG_STATUS},
  {"unlock_tables",        (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_UNLOCK_TABLES]), SHOW_LONG_STATUS},
  {"update",               (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_UPDATE]), SHOW_LONG_STATUS},
  {"update_multi",         (char*) offsetof(STATUS_VAR, com_stat[(uint32_t) SQLCOM_UPDATE_MULTI]), SHOW_LONG_STATUS},
  {NULL, NULL, SHOW_LONGLONG}
};

static int init_common_variables(const char *conf_file_name, int argc,
                                 char **argv, const char **groups)
{
  time_t curr_time;
  umask(((~my_umask) & 0666));
  my_decimal_set_zero(&decimal_zero); // set decimal_zero constant;
  tzset();			// Set tzname

  curr_time= time(NULL);
  if (curr_time == (time_t)-1)
    return 1;

  max_system_variables.pseudo_thread_id= UINT32_MAX;
  server_start_time= flush_status_time= curr_time;

  if (init_thread_environment())
    return 1;
  drizzle_init_variables();

  {
    struct tm tm_tmp;
    localtime_r(&server_start_time,&tm_tmp);
    strncpy(system_time_zone, tzname[tm_tmp.tm_isdst != 0 ? 1 : 0],
            sizeof(system_time_zone)-1);

 }
  /*
    We set SYSTEM time zone as reasonable default and
    also for failure of my_tz_init() and bootstrap mode.
    If user explicitly set time zone with --default-time-zone
    option we will change this value in my_tz_init().
  */
  global_system_variables.time_zone= my_tz_SYSTEM;

  if (gethostname(glob_hostname,sizeof(glob_hostname)) < 0)
  {
    strncpy(glob_hostname, STRING_WITH_LEN("localhost"));
      errmsg_printf(ERRMSG_LVL_WARN, _("gethostname failed, using '%s' as hostname"),
                      glob_hostname);
    strncpy(pidfile_name, STRING_WITH_LEN("drizzle"));
  }
  else
    strncpy(pidfile_name, glob_hostname, sizeof(pidfile_name)-5);
  strcpy(fn_ext(pidfile_name),".pid");		// Add proper extension

  /*
    Add server status variables to the dynamic list of
    status variables that is shown by SHOW STATUS.
    Later, in plugin_init, and mysql_install_plugin
    new entries could be added to that list.
  */
  if (add_status_vars(status_vars))
    return 1; // an error was already reported

  load_defaults(conf_file_name, groups, &argc, &argv);
  defaults_argv=argv;
  defaults_argc=argc;
  get_options(&defaults_argc, defaults_argv);
  set_server_version();


  /* connections and databases needs lots of files */
  (void) my_set_max_open_files(0xFFFFFFFF);

  current_pid=(ulong) getpid();		/* Save for later ref */
  init_time();				/* Init time-functions (read zone) */

  if (init_errmessage())	/* Read error messages from file */
    return 1;
  if (item_create_init())
    return 1;
  if (set_var_init())
    return 1;
  /* Creates static regex matching for temporal values */
  if (! init_temporal_formats())
    return 1;
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
          get_charset_by_csname(default_character_set_name, MY_CS_PRIMARY)))
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
    const CHARSET_INFO * const default_collation= get_charset_by_name(default_collation_name);
    if (!default_collation)
    {
          errmsg_printf(ERRMSG_LVL_ERROR, _(ER(ER_UNKNOWN_COLLATION)), default_collation_name);
      return 1;
    }
    if (!my_charset_same(default_charset_info, default_collation))
    {
          errmsg_printf(ERRMSG_LVL_ERROR, _(ER(ER_COLLATION_CHARSET_MISMATCH)),
                      default_collation_name,
                      default_charset_info->csname);
      return 1;
    }
    default_charset_info= default_collation;
  }
  /* Set collactions that depends on the default collation */
  global_system_variables.collation_server=	 default_charset_info;
  global_system_variables.collation_database=	 default_charset_info;

  global_system_variables.optimizer_use_mrr= 1;
  global_system_variables.optimizer_switch= 0;

  if (!(character_set_filesystem=
        get_charset_by_csname(character_set_filesystem_name, MY_CS_PRIMARY)))
    return 1;
  global_system_variables.character_set_filesystem= character_set_filesystem;

  if (!(my_default_lc_time_names=
        my_locale_by_name(lc_time_names_name)))
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Unknown locale: '%s'"), lc_time_names_name);
    return 1;
  }
  global_system_variables.lc_time_names= my_default_lc_time_names;

  sys_init_connect.value_length= 0;
  if ((sys_init_connect.value= opt_init_connect))
    sys_init_connect.value_length= strlen(opt_init_connect);
  else
    sys_init_connect.value=strdup("");
  if (sys_init_connect.value == NULL)
    return 1;

  if (use_temp_pool && bitmap_init(&temp_pool,0,1024,1))
    return 1;

  /* Reset table_alias_charset, now that lower_case_table_names is set. */
  lower_case_table_names= 1; /* This we need to look at */
  table_alias_charset= files_charset_info;

  return 0;
}


static int init_thread_environment()
{
  (void) pthread_mutex_init(&LOCK_create_db, NULL);
  (void) pthread_mutex_init(&LOCK_lock_db, NULL);
  (void) pthread_mutex_init(&LOCK_open, NULL);
  (void) pthread_mutex_init(&LOCK_thread_count,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_status,MY_MUTEX_INIT_FAST);
  (void) pthread_mutex_init(&LOCK_global_system_variables, MY_MUTEX_INIT_FAST);
  (void) pthread_rwlock_init(&LOCK_system_variables_hash, NULL);
  (void) pthread_mutex_init(&LOCK_global_read_lock, MY_MUTEX_INIT_FAST);
  (void) pthread_rwlock_init(&LOCK_sys_init_connect, NULL);
  (void) pthread_cond_init(&COND_thread_count,NULL);
  (void) pthread_cond_init(&COND_refresh,NULL);
  (void) pthread_cond_init(&COND_global_read_lock,NULL);

  if (pthread_key_create(&THR_Session,NULL) ||
      pthread_key_create(&THR_Mem_root,NULL))
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Can't create thread-keys"));
    return 1;
  }
  return 0;
}


static int init_server_components()
{
  /*
    We need to call each of these following functions to ensure that
    all things are initialized so that unireg_abort() doesn't fail
  */
  if (table_cache_init() | table_def_init())
    unireg_abort(1);

  setup_fpu();
  init_thr_lock();

  /* Setup logs */

  if (xid_cache_init())
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Out of memory"));
    unireg_abort(1);
  }

  /* call ha_init_key_cache() on all key caches to init them */
  process_key_caches(&ha_init_key_cache);

  /* Allow storage engine to give real error messages */
  if (ha_init_errors())
    return(1);

  if (plugin_init(&defaults_argc, defaults_argv,
                  (opt_noacl ? PLUGIN_INIT_SKIP_PLUGIN_TABLE : 0) |
                  (opt_help ? PLUGIN_INIT_SKIP_INITIALIZATION : 0)))
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to initialize plugins."));
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
                                  drizzled_get_one_option)))
      unireg_abort(ho_error);

    if (defaults_argc)
    {
      fprintf(stderr,
              _("%s: Too many arguments (first extra is '%s').\n"
                "Use --verbose --help to get a list of available options\n"),
              my_progname, *tmp_argv);
      unireg_abort(1);
    }
  }

  /* We have to initialize the storage engines before CSV logging */
  if (ha_init())
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Can't init databases"));
    unireg_abort(1);
  }

  /*
    Check that the default storage engine is actually available.
  */
  if (default_storage_engine_str)
  {
    LEX_STRING name= { default_storage_engine_str,
                       strlen(default_storage_engine_str) };
    st_plugin_int *plugin;
    StorageEngine *engine;

    if ((plugin= ha_resolve_by_name(0, &name)))
    {
      engine= static_cast<StorageEngine *>(plugin->data);
    }
    else
    {
          errmsg_printf(ERRMSG_LVL_ERROR, _("Unknown/unsupported table type: %s"),
                      default_storage_engine_str);
      unireg_abort(1);
    }
    if (!engine->is_enabled())
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Default storage engine (%s) is not available"),
                    default_storage_engine_str);
      unireg_abort(1);
      //assert(global_system_variables.table_plugin);
    }
    else
    {
      /*
        Need to unlock as global_system_variables.table_plugin
        was acquired during plugin_init()
      */
      global_system_variables.table_plugin= plugin;
    }
  }

  if (ha_recover(0))
  {
    unireg_abort(1);
  }

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
            errmsg_printf(ERRMSG_LVL_WARN, _("Failed to lock memory. Errno: %d\n"),errno);
      locked_in_memory= 0;
    }
    if (user_info)
      set_user(drizzled_user, user_info);
  }
  else
#endif
    locked_in_memory=0;

  init_update_queries();
  return(0);
}


int main(int argc, char **argv)
{
#if defined(ENABLE_NLS)
# if defined(HAVE_LOCALE_H)
  setlocale(LC_ALL, "");
# endif
  bindtextdomain("drizzle", LOCALEDIR);
  textdomain("drizzle");
#endif

  MY_INIT(argv[0]);		// init my_sys library & pthreads
  /* nothing should come before this line ^^^ */

  /* Set signal used to kill Drizzle */
#if defined(SIGUSR2)
  thr_kill_signal= thd_lib_detected == THD_LIB_LT ? SIGINT : SIGUSR2;
#else
  thr_kill_signal= SIGINT;
#endif

#ifdef _CUSTOMSTARTUPCONFIG_
  if (_cust_check_startup())
  {
    / * _cust_check_startup will report startup failure error * /
    exit(1);
  }
#endif

  if (init_common_variables(DRIZZLE_CONFIG_NAME,
			    argc, argv, load_default_groups))
    unireg_abort(1);				// Will do exit

  init_signals();

#ifdef TODO_MOVE_OUT_TO_SCHEDULER_API
  pthread_attr_setstacksize(&connection_attrib, my_thread_stack_size);

#ifdef HAVE_PTHREAD_ATTR_GETSTACKSIZE
  {
    /* Retrieve used stack size;  Needed for checking stack overflows */
    size_t stack_size= 0;
    pthread_attr_getstacksize(&connection_attrib, &stack_size);
    /* We must check if stack_size = 0 as Solaris 2.9 can return 0 here */
    if (stack_size && stack_size < my_thread_stack_size)
    {
      if (global_system_variables.log_warnings)
      {
            errmsg_printf(ERRMSG_LVL_WARN, _("Asked for %"PRIu64" thread stack, "
                            "but got %"PRIu64),
                          (uint64_t)my_thread_stack_size,
                          (uint64_t)stack_size);
      }
      my_thread_stack_size= stack_size;
    }
  }
#endif
#endif

  select_thread=pthread_self();
  select_thread_in_use=1;

  /*
    We have enough space for fiddling with the argv, continue
  */
  check_data_home(drizzle_real_data_home);
  if (chdir(drizzle_real_data_home) && !opt_help)
    unireg_abort(1);				/* purecov: inspected */
  drizzle_data_home= drizzle_data_home_buff;
  drizzle_data_home[0]=FN_CURLIB;		// all paths are relative from here
  drizzle_data_home[1]=0;
  drizzle_data_home_len= 2;

  if ((user_info= check_user(drizzled_user)))
  {
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
    if (locked_in_memory) // getuid() == 0 here
      set_effective_user(user_info);
    else
#endif
      set_user(drizzled_user, user_info);
  }

  if (server_id == 0)
  {
    server_id= 1;
  }

  if (init_server_components())
    unireg_abort(1);

  network_init();

  /*
    init signals & alarm
    After this we can't quit by a simple unireg_abort
  */
  error_handler_hook= my_message_sql;

  if (drizzle_rm_tmp_tables() || my_tz_init((Session *)0, default_tz_name))
  {
    abort_loop= true;
    select_thread_in_use=0;
    (void) pthread_kill(signal_thread, SIGTERM);

    (void) unlink(pidfile_name);	// Not needed anymore

    exit(1);
  }

  init_status_vars();

  errmsg_printf(ERRMSG_LVL_INFO, _(ER(ER_STARTUP)),my_progname,server_version,
                        "", drizzled_port, COMPILATION_COMMENT);


  handle_connections_sockets();
  /* (void) pthread_attr_destroy(&connection_attrib); */


  (void) pthread_mutex_lock(&LOCK_thread_count);
  select_thread_in_use=0;			// For close_connections
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  (void) pthread_cond_broadcast(&COND_thread_count);

  /* Wait until cleanup is done */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (!ready_to_exit)
    pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  clean_up(1);
  clean_up_mutexes();
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  return 0;
}


/**
  Create new thread to handle incoming connection.

    This function will create new thread to handle the incoming
    connection.  If there are idle cached threads one will be used.
    'session' will be pushed into 'threads'.

    In single-threaded mode (\#define ONE_THREAD) connection will be
    handled inside this function.

  @param[in,out] session    Thread handle of future thread.
*/

static void create_new_thread(Session *session)
{
  Scheduler &thread_scheduler= get_thread_scheduler();

  ++connection_count;

  if (connection_count > max_used_connections)
    max_used_connections= connection_count;

  /*
    The initialization of thread_id is done in create_embedded_session() for
    the embedded library.
    TODO: refactor this to avoid code duplication there
  */
  session->thread_id= session->variables.pseudo_thread_id= thread_id++;

  /* 
    If we error on creation we drop the connection and delete the session.
  */
  pthread_mutex_lock(&LOCK_thread_count);
  session_list.append(session);
  pthread_mutex_unlock(&LOCK_thread_count);
  if (thread_scheduler.add_connection(session))
  {
    char error_message_buff[DRIZZLE_ERRMSG_SIZE];

    session->killed= Session::KILL_CONNECTION;                        // Safety

    statistic_increment(aborted_connects, &LOCK_status);

    /* Can't use my_error() since store_globals has not been called. */
    snprintf(error_message_buff, sizeof(error_message_buff), ER(ER_CANT_CREATE_THREAD), 1); /* TODO replace will better error message */
    session->protocol->send_error(ER_CANT_CREATE_THREAD, error_message_buff);
    unlink_session(session);
  }
}


	/* Handle new connections and spawn new process to handle them */

void handle_connections_sockets()
{
  int x;
  int sock,new_sock;
  uint32_t error_count=0;
  Session *session;
  struct sockaddr_storage cAddr;

  while (!abort_loop)
  {
    int number_of;

    if ((number_of= poll(fds, pollfd_count, -1)) == -1)
    {
      if (errno != EINTR)
      {
        if (!select_errors++ && !abort_loop)	/* purecov: inspected */
                errmsg_printf(ERRMSG_LVL_ERROR, _("drizzled: Got error %d from select"),
                          errno); /* purecov: inspected */
      }
      continue;
    }
    if (number_of == 0)
      continue;

#ifdef FIXME_IF_WE_WERE_KEEPING_THIS
    assert(number_of > 1); /* Not handling this at the moment */
#endif

    if (abort_loop)
    {
      break;
    }

    for (x= 0, sock= -1; x < pollfd_count; x++)
    {
      if (fds[x].revents == POLLIN)
      {
        sock= fds[x].fd;
        break;
      }
    }
    assert(sock != -1);

    for (uint32_t retry=0; retry < MAX_ACCEPT_RETRY; retry++)
    {
      socklen_t length= sizeof(struct sockaddr_storage);
      new_sock= accept(sock, (struct sockaddr *)(&cAddr),
                       &length);
      if (new_sock != -1 || (errno != EINTR && errno != EAGAIN))
        break;
    }


    if (new_sock == -1)
    {
      if ((error_count++ & 255) == 0)		// This can happen often
        sql_perror("Error in accept");
      if (errno == ENFILE || errno == EMFILE)
        sleep(1);				// Give other threads some time
      continue;
    }

    {
      socklen_t dummyLen;
      struct sockaddr_storage dummy;
      dummyLen = sizeof(dummy);
      if (  getsockname(new_sock,(struct sockaddr *)&dummy,
                        (socklen_t *)&dummyLen) < 0  )
      {
        sql_perror("Error on new connection socket");
        (void) shutdown(new_sock, SHUT_RDWR);
        (void) close(new_sock);
        continue;
      }
      dummyLen = sizeof(dummy);
      if ( getpeername(new_sock, (struct sockaddr *)&dummy,
                       (socklen_t *)&dummyLen) < 0)
      {
        sql_perror("Error on new connection socket");
        (void) shutdown(new_sock, SHUT_RDWR);
        (void) close(new_sock);
         continue;
      }
    }

    /*
    ** Don't allow too many connections
    */

    if (!(session= new Session))
    {
      (void) shutdown(new_sock, SHUT_RDWR);
      close(new_sock);
      continue;
    }

    if (session->protocol->init_file_descriptor(new_sock))
    {
      delete session;
      continue;
    }

    create_new_thread(session);
  }
}


/****************************************************************************
  Handle start options
******************************************************************************/

enum options_drizzled
{
  OPT_ISAM_LOG=256,
  OPT_SOCKET,
  OPT_BIND_ADDRESS,            OPT_PID_FILE,
  OPT_STORAGE_ENGINE,          
  OPT_INIT_FILE,
  OPT_DELAY_KEY_WRITE_ALL,
  OPT_DELAY_KEY_WRITE,
  OPT_WANT_CORE,
  OPT_MEMLOCK,
  OPT_MYISAM_RECOVER,
  OPT_SERVER_ID,
  OPT_TC_HEURISTIC_RECOVER,
  OPT_ENGINE_CONDITION_PUSHDOWN,
  OPT_TEMP_POOL, OPT_TX_ISOLATION, OPT_COMPLETION_TYPE,
  OPT_SKIP_STACK_TRACE, OPT_SKIP_SYMLINKS,
  OPT_DO_PSTACK,
  OPT_LOCAL_INFILE,
  OPT_BACK_LOG,
  OPT_CONNECT_TIMEOUT,
  OPT_JOIN_BUFF_SIZE,
  OPT_KEY_BUFFER_SIZE, OPT_KEY_CACHE_BLOCK_SIZE,
  OPT_KEY_CACHE_DIVISION_LIMIT, OPT_KEY_CACHE_AGE_THRESHOLD,
  OPT_MAX_ALLOWED_PACKET,
  OPT_MAX_CONNECT_ERRORS,
  OPT_MAX_HEP_TABLE_SIZE,
  OPT_MAX_JOIN_SIZE,
  OPT_MAX_SORT_LENGTH,
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
  OPT_PRELOAD_BUFFER_SIZE,
  OPT_RECORD_BUFFER,
  OPT_RECORD_RND_BUFFER, OPT_DIV_PRECINCREMENT,
  OPT_DEBUGGING,
  OPT_SORT_BUFFER, OPT_TABLE_OPEN_CACHE, OPT_TABLE_DEF_CACHE,
  OPT_TMP_TABLE_SIZE, OPT_THREAD_STACK,
  OPT_WAIT_TIMEOUT,
  OPT_RANGE_ALLOC_BLOCK_SIZE,
  OPT_QUERY_ALLOC_BLOCK_SIZE, OPT_QUERY_PREALLOC_SIZE,
  OPT_TRANS_ALLOC_BLOCK_SIZE, OPT_TRANS_PREALLOC_SIZE,
  OPT_OLD_ALTER_TABLE,
  OPT_GROUP_CONCAT_MAX_LEN,
  OPT_DEFAULT_COLLATION,
  OPT_CHARACTER_SET_FILESYSTEM,
  OPT_LC_TIME_NAMES,
  OPT_INIT_CONNECT,
  OPT_DEFAULT_TIME_ZONE,
  OPT_OPTIMIZER_SEARCH_DEPTH,
  OPT_SCHEDULER,
  OPT_OPTIMIZER_PRUNE_LEVEL,
  OPT_AUTO_INCREMENT, OPT_AUTO_INCREMENT_OFFSET,
  OPT_ENABLE_LARGE_PAGES,
  OPT_TIMED_MUTEXES,
  OPT_TABLE_LOCK_WAIT_TIMEOUT,
  OPT_PLUGIN_LOAD,
  OPT_PLUGIN_DIR,
  OPT_PORT_OPEN_TIMEOUT,
  OPT_KEEP_FILES_ON_CREATE,
  OPT_SECURE_FILE_PRIV,
  OPT_MIN_EXAMINED_ROW_LIMIT
};


#define LONG_TIMEOUT ((uint32_t) 3600L*24L*365L)

struct my_option my_long_options[] =
{
  {"help", '?', N_("Display this help and exit."),
   (char**) &opt_help, (char**) &opt_help, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"auto-increment-increment", OPT_AUTO_INCREMENT,
   N_("Auto-increment columns are incremented by this"),
   (char**) &global_system_variables.auto_increment_increment,
   (char**) &max_system_variables.auto_increment_increment, 0, GET_ULL,
   OPT_ARG, 1, 1, UINT64_MAX, 0, 1, 0 },
  {"auto-increment-offset", OPT_AUTO_INCREMENT_OFFSET,
   N_("Offset added to Auto-increment columns. Used when "
      "auto-increment-increment != 1"),
   (char**) &global_system_variables.auto_increment_offset,
   (char**) &max_system_variables.auto_increment_offset, 0, GET_ULL, OPT_ARG,
   1, 1, UINT64_MAX, 0, 1, 0 },
  {"basedir", 'b',
   N_("Path to installation directory. All paths are usually resolved "
      "relative to this."),
   (char**) &drizzle_home_ptr, (char**) &drizzle_home_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"bind-address", OPT_BIND_ADDRESS, N_("IP address to bind to."),
   (char**) &my_bind_addr_str, (char**) &my_bind_addr_str, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"character-set-filesystem", OPT_CHARACTER_SET_FILESYSTEM,
   N_("Set the filesystem character set."),
   (char**) &character_set_filesystem_name,
   (char**) &character_set_filesystem_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"character-set-server", 'C',
   N_("Set the default character set."),
   (char**) &default_character_set_name, (char**) &default_character_set_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"chroot", 'r',
   N_("Chroot drizzled daemon during startup."),
   (char**) &drizzled_chroot, (char**) &drizzled_chroot, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"collation-server", OPT_DEFAULT_COLLATION,
   N_("Set the default collation."),
   (char**) &default_collation_name, (char**) &default_collation_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"completion-type", OPT_COMPLETION_TYPE,
   N_("Default completion type."),
   (char**) &global_system_variables.completion_type,
   (char**) &max_system_variables.completion_type, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, 2, 0, 1, 0},
  {"core-file", OPT_WANT_CORE,
   N_("Write core on errors."),
   0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'h',
   N_("Path to the database root."),
   (char**) &drizzle_data_home,
   (char**) &drizzle_data_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-storage-engine", OPT_STORAGE_ENGINE,
   N_("Set the default storage engine (table type) for tables."),
   (char**)&default_storage_engine_str, (char**)&default_storage_engine_str,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-time-zone", OPT_DEFAULT_TIME_ZONE,
   N_("Set the default time zone."),
   (char**) &default_tz_name, (char**) &default_tz_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"delay-key-write", OPT_DELAY_KEY_WRITE,
   N_("Type of DELAY_KEY_WRITE."),
   0,0,0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_STACK_TRACE_ON_SEGV
  {"enable-pstack", OPT_DO_PSTACK,
   N_("Print a symbolic stack trace on failure."),
   (char**) &opt_do_pstack, (char**) &opt_do_pstack, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
#endif /* HAVE_STACK_TRACE_ON_SEGV */
  {"engine-condition-pushdown",
   OPT_ENGINE_CONDITION_PUSHDOWN,
   N_("Push supported query conditions to the storage engine."),
   (char**) &global_system_variables.engine_condition_pushdown,
   (char**) &global_system_variables.engine_condition_pushdown,
   0, GET_BOOL, NO_ARG, false, 0, 0, 0, 0, 0},
  /* See how it's handled in get_one_option() */
  {"exit-info", 'T',
   N_("Used for debugging;  Use at your own risk!"),
   0, 0, 0, GET_LONG, OPT_ARG, 0, 0, 0, 0, 0, 0},
  /* We must always support the next option to make scripts like mysqltest
     easier to do */
  {"gdb", OPT_DEBUGGING,
   N_("Set up signals usable for debugging"),
   (char**) &opt_debugging, (char**) &opt_debugging,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"init-connect", OPT_INIT_CONNECT,
   N_("Command(s) that are executed for each new connection"),
   (char**) &opt_init_connect, (char**) &opt_init_connect, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"init-file", OPT_INIT_FILE,
   N_("Read SQL commands from this file at startup."),
   (char**) &opt_init_file, (char**) &opt_init_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"language", 'L',
   N_("(IGNORED)"),
   (char**) &language_ptr, (char**) &language_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"lc-time-names", OPT_LC_TIME_NAMES,
   N_("Set the language used for the month names and the days of the week."),
   (char**) &lc_time_names_name,
   (char**) &lc_time_names_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"log", 'l',
   N_("Log connections and queries to file."),
   (char**) &opt_logname,
   (char**) &opt_logname, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-isam", OPT_ISAM_LOG,
   N_("Log all MyISAM changes to file."),
   (char**) &myisam_log_filename, (char**) &myisam_log_filename, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"log-warnings", 'W',
   N_("Log some not critical warnings to the log file."),
   (char**) &global_system_variables.log_warnings,
   (char**) &max_system_variables.log_warnings, 0, GET_BOOL, OPT_ARG, 1, 0, 0,
   0, 0, 0},
  {"memlock", OPT_MEMLOCK,
   N_("Lock drizzled in memory."),
   (char**) &locked_in_memory,
   (char**) &locked_in_memory, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"myisam-recover", OPT_MYISAM_RECOVER,
   N_("Syntax: myisam-recover[=option[,option...]], where option can be "
      "DEFAULT, BACKUP, FORCE or QUICK."),
   (char**) &myisam_recover_options_str, (char**) &myisam_recover_options_str, 0,
   GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"old-alter-table", OPT_OLD_ALTER_TABLE,
   N_("Use old, non-optimized alter table."),
   (char**) &global_system_variables.old_alter_table,
   (char**) &max_system_variables.old_alter_table, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"pid-file", OPT_PID_FILE,
   N_("Pid file used by safe_mysqld."),
   (char**) &pidfile_name_ptr, (char**) &pidfile_name_ptr, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P',
   N_("Port number to use for connection or 0 for default to, in "
      "order of preference, drizzle.cnf, $DRIZZLE_TCP_PORT, "
      "built-in default (" STRINGIFY_ARG(DRIZZLE_PORT) ")."),
   (char**) &drizzled_port,
   (char**) &drizzled_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port-open-timeout", OPT_PORT_OPEN_TIMEOUT,
   N_("Maximum time in seconds to wait for the port to become free. "
      "(Default: no wait)"),
   (char**) &drizzled_port_timeout,
   (char**) &drizzled_port_timeout, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"secure-file-priv", OPT_SECURE_FILE_PRIV,
   N_("Limit LOAD DATA, SELECT ... OUTFILE, and LOAD_FILE() to files "
      "within specified directory"),
   (char**) &opt_secure_file_priv, (char**) &opt_secure_file_priv, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-id",	OPT_SERVER_ID,
   N_("Uniquely identifies the server instance in the community of "
      "replication partners."),
   (char**) &server_id, (char**) &server_id, 0, GET_UINT32, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"skip-stack-trace", OPT_SKIP_STACK_TRACE,
   N_("Don't print a stack trace on failure."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"symbolic-links", 's',
   N_("Enable symbolic link support."),
   (char**) &my_use_symdir, (char**) &my_use_symdir, 0, GET_BOOL, NO_ARG,
   /*
     The system call realpath() produces warnings under valgrind and
     purify. These are not suppressed: instead we disable symlinks
     option if compiled with valgrind support.
   */
   IF_PURIFY(0,1), 0, 0, 0, 0, 0},
  {"temp-pool", OPT_TEMP_POOL,
   N_("Using this option will cause most temporary files created to use a "
      "small set of names, rather than a unique name for each new file."),
   (char**) &use_temp_pool, (char**) &use_temp_pool, 0, GET_BOOL, NO_ARG, 1,
   0, 0, 0, 0, 0},
  {"timed_mutexes", OPT_TIMED_MUTEXES,
   N_("Specify whether to time mutexes (only InnoDB mutexes are currently "
      "supported)"),
   (char**) &timed_mutexes, (char**) &timed_mutexes, 0, GET_BOOL, NO_ARG, 0,
    0, 0, 0, 0, 0},
  {"tmpdir", 't',
   N_("Path for temporary files."),
   (char**) &opt_drizzle_tmpdir,
   (char**) &opt_drizzle_tmpdir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"transaction-isolation", OPT_TX_ISOLATION,
   N_("Default transaction isolation level."),
   0, 0, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"user", 'u',
   N_("Run drizzled daemon as user."),
   0, 0, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"version", 'V',
   N_("Output version information and exit."),
   0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"back_log", OPT_BACK_LOG,
   N_("The number of outstanding connection requests Drizzle can have. This "
      "comes into play when the main Drizzle thread gets very many connection "
      "requests in a very short time."),
    (char**) &back_log, (char**) &back_log, 0, GET_UINT,
    REQUIRED_ARG, 50, 1, 65535, 0, 1, 0 },
  { "bulk_insert_buffer_size", OPT_BULK_INSERT_BUFFER_SIZE,
    N_("Size of tree cache used in bulk insert optimization. Note that this is "
       "a limit per thread!"),
    (char**) &global_system_variables.bulk_insert_buff_size,
    (char**) &max_system_variables.bulk_insert_buff_size,
    0, GET_ULL, REQUIRED_ARG, 8192*1024, 0, ULONG_MAX, 0, 1, 0},
  { "connect_timeout", OPT_CONNECT_TIMEOUT,
    N_("The number of seconds the drizzled server is waiting for a connect "
       "packet before responding with 'Bad handshake'."),
    (char**) &connect_timeout, (char**) &connect_timeout,
    0, GET_UINT32, REQUIRED_ARG, CONNECT_TIMEOUT, 2, LONG_TIMEOUT, 0, 1, 0 },
  { "div_precision_increment", OPT_DIV_PRECINCREMENT,
   N_("Precision of the result of '/' operator will be increased on that "
      "value."),
   (char**) &global_system_variables.div_precincrement,
   (char**) &max_system_variables.div_precincrement, 0, GET_UINT,
   REQUIRED_ARG, 4, 0, DECIMAL_MAX_SCALE, 0, 0, 0},
  { "group_concat_max_len", OPT_GROUP_CONCAT_MAX_LEN,
    N_("The maximum length of the result of function  group_concat."),
    (char**) &global_system_variables.group_concat_max_len,
    (char**) &max_system_variables.group_concat_max_len, 0, GET_UINT64,
    REQUIRED_ARG, 1024, 4, ULONG_MAX, 0, 1, 0},
  { "join_buffer_size", OPT_JOIN_BUFF_SIZE,
    N_("The size of the buffer that is used for full joins."),
   (char**) &global_system_variables.join_buff_size,
   (char**) &max_system_variables.join_buff_size, 0, GET_UINT64,
   REQUIRED_ARG, 128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, ULONG_MAX,
   MALLOC_OVERHEAD, IO_SIZE, 0},
  {"keep_files_on_create", OPT_KEEP_FILES_ON_CREATE,
   N_("Don't overwrite stale .MYD and .MYI even if no directory is specified."),
   (char**) &global_system_variables.keep_files_on_create,
   (char**) &max_system_variables.keep_files_on_create,
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"key_buffer_size", OPT_KEY_BUFFER_SIZE,
   N_("The size of the buffer used for index blocks for MyISAM tables. "
      "Increase this to get better index handling (for all reads and multiple "
      "writes) to as much as you can afford;"),
   (char**) &dflt_key_cache_var.param_buff_size,
   (char**) 0,
   0, (GET_ULL | GET_ASK_ADDR),
   REQUIRED_ARG, KEY_CACHE_SIZE, MALLOC_OVERHEAD, SIZE_T_MAX, MALLOC_OVERHEAD,
   IO_SIZE, 0},
  {"key_cache_age_threshold", OPT_KEY_CACHE_AGE_THRESHOLD,
   N_("This characterizes the number of hits a hot block has to be untouched "
      "until it is considered aged enough to be downgraded to a warm block. "
      "This specifies the percentage ratio of that number of hits to the "
      "total number of blocks in key cache"),
   (char**) &dflt_key_cache_var.param_age_threshold,
   (char**) 0,
   0, (GET_UINT32 | GET_ASK_ADDR), REQUIRED_ARG,
   300, 100, ULONG_MAX, 0, 100, 0},
  {"key_cache_block_size", OPT_KEY_CACHE_BLOCK_SIZE,
   N_("The default size of key cache blocks"),
   (char**) &dflt_key_cache_var.param_block_size,
   (char**) 0,
   0, (GET_UINT32 | GET_ASK_ADDR), REQUIRED_ARG,
   KEY_CACHE_BLOCK_SIZE, 512, 1024 * 16, 0, 512, 0},
  {"key_cache_division_limit", OPT_KEY_CACHE_DIVISION_LIMIT,
   N_("The minimum percentage of warm blocks in key cache"),
   (char**) &dflt_key_cache_var.param_division_limit,
   (char**) 0,
   0, (GET_UINT32 | GET_ASK_ADDR) , REQUIRED_ARG, 100,
   1, 100, 0, 1, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET,
   N_("Max packetlength to send/receive from to server."),
   (char**) &global_system_variables.max_allowed_packet,
   (char**) &max_system_variables.max_allowed_packet, 0, GET_UINT32,
   REQUIRED_ARG, 1024*1024L, 1024, 1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"max_connect_errors", OPT_MAX_CONNECT_ERRORS,
   N_("If there is more than this number of interrupted connections from a "
      "host this host will be blocked from further connections."),
   (char**) &max_connect_errors, (char**) &max_connect_errors, 0, GET_UINT64,
   REQUIRED_ARG, MAX_CONNECT_ERRORS, 1, ULONG_MAX, 0, 1, 0},
  {"max_error_count", OPT_MAX_ERROR_COUNT,
   N_("Max number of errors/warnings to store for a statement."),
   (char**) &global_system_variables.max_error_count,
   (char**) &max_system_variables.max_error_count,
   0, GET_UINT64, REQUIRED_ARG, DEFAULT_ERROR_COUNT, 0, 65535, 0, 1, 0},
  {"max_heap_table_size", OPT_MAX_HEP_TABLE_SIZE,
   N_("Don't allow creation of heap tables bigger than this."),
   (char**) &global_system_variables.max_heap_table_size,
   (char**) &max_system_variables.max_heap_table_size, 0, GET_ULL,
   REQUIRED_ARG, 16*1024*1024L, 16384, MAX_MEM_TABLE_SIZE,
   MALLOC_OVERHEAD, 1024, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   N_("Joins that are probably going to read more than max_join_size records "
      "return an error."),
   (char**) &global_system_variables.max_join_size,
   (char**) &max_system_variables.max_join_size, 0, GET_HA_ROWS, REQUIRED_ARG,
   INT32_MAX, 1, INT32_MAX, 0, 1, 0},
  {"max_length_for_sort_data", OPT_MAX_LENGTH_FOR_SORT_DATA,
   N_("Max number of bytes in sorted records."),
   (char**) &global_system_variables.max_length_for_sort_data,
   (char**) &max_system_variables.max_length_for_sort_data, 0, GET_ULL,
   REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  { "max_seeks_for_key", OPT_MAX_SEEKS_FOR_KEY,
    N_("Limit assumed max number of seeks when looking up rows based on a key"),
    (char**) &global_system_variables.max_seeks_for_key,
    (char**) &max_system_variables.max_seeks_for_key, 0, GET_UINT64,
    REQUIRED_ARG, ULONG_MAX, 1, ULONG_MAX, 0, 1, 0 },
  {"max_sort_length", OPT_MAX_SORT_LENGTH,
   N_("The number of bytes to use when sorting BLOB or TEXT values "
      "(only the first max_sort_length bytes of each value are used; the "
      "rest are ignored)."),
   (char**) &global_system_variables.max_sort_length,
   (char**) &max_system_variables.max_sort_length, 0, GET_SIZE,
   REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  {"max_tmp_tables", OPT_MAX_TMP_TABLES,
   N_("Maximum number of temporary tables a client can keep open at a time."),
   (char**) &global_system_variables.max_tmp_tables,
   (char**) &max_system_variables.max_tmp_tables, 0, GET_UINT64,
   REQUIRED_ARG, 32, 1, ULONG_MAX, 0, 1, 0},
  {"max_write_lock_count", OPT_MAX_WRITE_LOCK_COUNT,
   N_("After this many write locks, allow some read locks to run in between."),
   (char**) &max_write_lock_count, (char**) &max_write_lock_count, 0, GET_ULL,
   REQUIRED_ARG, ULONG_MAX, 1, ULONG_MAX, 0, 1, 0},
  {"min_examined_row_limit", OPT_MIN_EXAMINED_ROW_LIMIT,
   N_("Don't log queries which examine less than min_examined_row_limit "
      "rows to file."),
   (char**) &global_system_variables.min_examined_row_limit,
   (char**) &max_system_variables.min_examined_row_limit, 0, GET_ULL,
   REQUIRED_ARG, 0, 0, ULONG_MAX, 0, 1L, 0},
  {"myisam_stats_method", OPT_MYISAM_STATS_METHOD,
   N_("Specifies how MyISAM index statistics collection code should threat "
      "NULLs. Possible values of name are 'nulls_unequal' "
      "(default behavior), "
      "'nulls_equal' (emulate MySQL 4.0 behavior), and 'nulls_ignored'."),
   (char**) &myisam_stats_method_str, (char**) &myisam_stats_method_str, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"net_buffer_length", OPT_NET_BUFFER_LENGTH,
   N_("Buffer length for TCP/IP and socket communication."),
   (char**) &global_system_variables.net_buffer_length,
   (char**) &max_system_variables.net_buffer_length, 0, GET_UINT32,
   REQUIRED_ARG, 16384, 1024, 1024*1024L, 0, 1024, 0},
  {"net_read_timeout", OPT_NET_READ_TIMEOUT,
   N_("Number of seconds to wait for more data from a connection before "
      "aborting the read."),
   (char**) &global_system_variables.net_read_timeout,
   (char**) &max_system_variables.net_read_timeout, 0, GET_UINT32,
   REQUIRED_ARG, NET_READ_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"net_retry_count", OPT_NET_RETRY_COUNT,
   N_("If a read on a communication port is interrupted, retry this many "
      "times before giving up."),
   (char**) &global_system_variables.net_retry_count,
   (char**) &max_system_variables.net_retry_count,0,
   GET_UINT32, REQUIRED_ARG, MYSQLD_NET_RETRY_COUNT, 1, ULONG_MAX, 0, 1, 0},
  {"net_write_timeout", OPT_NET_WRITE_TIMEOUT,
   N_("Number of seconds to wait for a block to be written to a connection "
      "before aborting the write."),
   (char**) &global_system_variables.net_write_timeout,
   (char**) &max_system_variables.net_write_timeout, 0, GET_UINT32,
   REQUIRED_ARG, NET_WRITE_TIMEOUT, 1, LONG_TIMEOUT, 0, 1, 0},
  {"optimizer_prune_level", OPT_OPTIMIZER_PRUNE_LEVEL,
    N_("Controls the heuristic(s) applied during query optimization to prune "
       "less-promising partial plans from the optimizer search space. Meaning: "
       "false - do not apply any heuristic, thus perform exhaustive search; "
       "true - prune plans based on number of retrieved rows."),
    (char**) &global_system_variables.optimizer_prune_level,
    (char**) &max_system_variables.optimizer_prune_level,
    0, GET_BOOL, OPT_ARG, 1, 0, 1, 0, 1, 0},
  {"optimizer_search_depth", OPT_OPTIMIZER_SEARCH_DEPTH,
   N_("Maximum depth of search performed by the query optimizer. Values "
      "larger than the number of relations in a query result in better query "
      "plans, but take longer to compile a query. Smaller values than the "
      "number of tables in a relation result in faster optimization, but may "
      "produce very bad query plans. If set to 0, the system will "
      "automatically pick a reasonable value; if set to MAX_TABLES+2, the "
      "optimizer will switch to the original find_best (used for "
      "testing/comparison)."),
   (char**) &global_system_variables.optimizer_search_depth,
   (char**) &max_system_variables.optimizer_search_depth,
   0, GET_UINT, OPT_ARG, MAX_TABLES+1, 0, MAX_TABLES+2, 0, 1, 0},
  {"plugin_dir", OPT_PLUGIN_DIR,
   N_("Directory for plugins."),
   (char**) &opt_plugin_dir_ptr, (char**) &opt_plugin_dir_ptr, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin_load", OPT_PLUGIN_LOAD,
   N_("Optional colon (or semicolon) separated list of plugins to load,"
      "where each plugin is identified by the name of the shared library. "
      "[for example: --plugin_load=libmd5udf.so:libauth_pam.so]"),
   (char**) &opt_plugin_load, (char**) &opt_plugin_load, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"preload_buffer_size", OPT_PRELOAD_BUFFER_SIZE,
   N_("The size of the buffer that is allocated when preloading indexes"),
   (char**) &global_system_variables.preload_buff_size,
   (char**) &max_system_variables.preload_buff_size, 0, GET_ULL,
   REQUIRED_ARG, 32*1024L, 1024, 1024*1024*1024L, 0, 1, 0},
  {"query_alloc_block_size", OPT_QUERY_ALLOC_BLOCK_SIZE,
   N_("Allocation block size for query parsing and execution"),
   (char**) &global_system_variables.query_alloc_block_size,
   (char**) &max_system_variables.query_alloc_block_size, 0, GET_UINT,
   REQUIRED_ARG, QUERY_ALLOC_BLOCK_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
  {"query_prealloc_size", OPT_QUERY_PREALLOC_SIZE,
   N_("Persistent buffer for query parsing and execution"),
   (char**) &global_system_variables.query_prealloc_size,
   (char**) &max_system_variables.query_prealloc_size, 0, GET_UINT,
   REQUIRED_ARG, QUERY_ALLOC_PREALLOC_SIZE, QUERY_ALLOC_PREALLOC_SIZE,
   ULONG_MAX, 0, 1024, 0},
  {"range_alloc_block_size", OPT_RANGE_ALLOC_BLOCK_SIZE,
   N_("Allocation block size for storing ranges during optimization"),
   (char**) &global_system_variables.range_alloc_block_size,
   (char**) &max_system_variables.range_alloc_block_size, 0, GET_SIZE,
   REQUIRED_ARG, RANGE_ALLOC_BLOCK_SIZE, RANGE_ALLOC_BLOCK_SIZE, SIZE_MAX,
   0, 1024, 0},
  {"read_buffer_size", OPT_RECORD_BUFFER,
    N_("Each thread that does a sequential scan allocates a buffer of this "
       "size for each table it scans. If you do many sequential scans, you may "
       "want to increase this value."),
    (char**) &global_system_variables.read_buff_size,
    (char**) &max_system_variables.read_buff_size,0, GET_UINT, REQUIRED_ARG,
    128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, INT32_MAX, MALLOC_OVERHEAD, IO_SIZE,
    0},
  {"read_rnd_buffer_size", OPT_RECORD_RND_BUFFER,
   N_("When reading rows in sorted order after a sort, the rows are read "
      "through this buffer to avoid a disk seeks. If not set, then it's set "
      "to the value of record_buffer."),
   (char**) &global_system_variables.read_rnd_buff_size,
   (char**) &max_system_variables.read_rnd_buff_size, 0,
   GET_UINT, REQUIRED_ARG, 256*1024L, 64 /*IO_SIZE*2+MALLOC_OVERHEAD*/ ,
   UINT32_MAX, MALLOC_OVERHEAD, 1 /* Small lower limit to be able to test MRR */, 0},
  {"scheduler", OPT_SCHEDULER,
   N_("Select scheduler to be used (by default pool-of-threads)."),
   (char**) &opt_scheduler, (char**) &opt_scheduler, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"sort_buffer_size", OPT_SORT_BUFFER,
   N_("Each thread that needs to do a sort allocates a buffer of this size."),
   (char**) &global_system_variables.sortbuff_size,
   (char**) &max_system_variables.sortbuff_size, 0, GET_SIZE, REQUIRED_ARG,
   MAX_SORT_MEMORY, MIN_SORT_MEMORY+MALLOC_OVERHEAD*2, SIZE_MAX,
   MALLOC_OVERHEAD, 1, 0},
  {"table_definition_cache", OPT_TABLE_DEF_CACHE,
   N_("The number of cached table definitions."),
   (char**) &table_def_size, (char**) &table_def_size,
   0, GET_ULL, REQUIRED_ARG, 128, 1, 512*1024L, 0, 1, 0},
  {"table_open_cache", OPT_TABLE_OPEN_CACHE,
   N_("The number of cached open tables."),
   (char**) &table_cache_size, (char**) &table_cache_size, 0, GET_UINT64,
   REQUIRED_ARG, TABLE_OPEN_CACHE_DEFAULT, 1, 512*1024L, 0, 1, 0},
  {"table_lock_wait_timeout", OPT_TABLE_LOCK_WAIT_TIMEOUT,
   N_("Timeout in seconds to wait for a table level lock before returning an "
      "error. Used only if the connection has active cursors."),
   (char**) &table_lock_wait_timeout, (char**) &table_lock_wait_timeout,
   0, GET_ULL, REQUIRED_ARG, 50, 1, 1024 * 1024 * 1024, 0, 1, 0},
  {"thread_stack", OPT_THREAD_STACK,
   N_("The stack size for each thread."),
   (char**) &my_thread_stack_size,
   (char**) &my_thread_stack_size, 0, GET_SIZE,
   REQUIRED_ARG,DEFAULT_THREAD_STACK,
   UINT32_C(1024*128), SIZE_MAX, 0, 1024, 0},
  {"tmp_table_size", OPT_TMP_TABLE_SIZE,
   N_("If an internal in-memory temporary table exceeds this size, Drizzle will"
      " automatically convert it to an on-disk MyISAM table."),
   (char**) &global_system_variables.tmp_table_size,
   (char**) &max_system_variables.tmp_table_size, 0, GET_ULL,
   REQUIRED_ARG, 16*1024*1024L, 1024, MAX_MEM_TABLE_SIZE, 0, 1, 0},
  {"transaction_alloc_block_size", OPT_TRANS_ALLOC_BLOCK_SIZE,
   N_("Allocation block size for transactions to be stored in binary log"),
   (char**) &global_system_variables.trans_alloc_block_size,
   (char**) &max_system_variables.trans_alloc_block_size, 0, GET_UINT,
   REQUIRED_ARG, QUERY_ALLOC_BLOCK_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
  {"transaction_prealloc_size", OPT_TRANS_PREALLOC_SIZE,
   N_("Persistent buffer for transactions to be stored in binary log"),
   (char**) &global_system_variables.trans_prealloc_size,
   (char**) &max_system_variables.trans_prealloc_size, 0, GET_UINT,
   REQUIRED_ARG, TRANS_ALLOC_PREALLOC_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
  {"wait_timeout", OPT_WAIT_TIMEOUT,
   N_("The number of seconds the server waits for activity on a connection "
      "before closing it."),
   (char**) &global_system_variables.net_wait_timeout,
   (char**) &max_system_variables.net_wait_timeout, 0, GET_UINT,
   REQUIRED_ARG, NET_WAIT_TIMEOUT, 1, LONG_TIMEOUT,
   0, 1, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static int show_net_compression(Session *session,
                                SHOW_VAR *var,
                                char *)
{
  var->type= SHOW_MY_BOOL;
  var->value= (char *)&session->compression;
  return 0;
}

static st_show_var_func_container
show_net_compression_cont= { &show_net_compression };

static int show_starttime(Session *session, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (session->query_start() - server_start_time);
  return 0;
}

static st_show_var_func_container
show_starttime_cont= { &show_starttime };

static int show_flushstatustime(Session *session, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (session->query_start() - flush_status_time);
  return 0;
}

static st_show_var_func_container
show_flushstatustime_cont= { &show_flushstatustime };

static int show_open_tables(Session *, SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long)cached_open_tables();
  return 0;
}

static int show_table_definitions(Session *,
                                  SHOW_VAR *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long)cached_table_definitions();
  return 0;
}

static st_show_var_func_container
show_open_tables_cont= { &show_open_tables };
static st_show_var_func_container
show_table_definitions_cont= { &show_table_definitions };

/*
  Variables shown by SHOW STATUS in alphabetical order
*/

SHOW_VAR status_vars[]= {
  {"Aborted_clients",          (char*) &aborted_threads,        SHOW_LONGLONG},
  {"Aborted_connects",         (char*) &aborted_connects,       SHOW_LONGLONG},
  {"Bytes_received",           (char*) offsetof(STATUS_VAR, bytes_received), SHOW_LONGLONG_STATUS},
  {"Bytes_sent",               (char*) offsetof(STATUS_VAR, bytes_sent), SHOW_LONGLONG_STATUS},
  {"Com",                      (char*) com_status_vars, SHOW_ARRAY},
  {"Compression",              (char*) &show_net_compression_cont, SHOW_FUNC},
  {"Connections",              (char*) &thread_id,          SHOW_INT_NOFLUSH},
  {"Created_tmp_disk_tables",  (char*) offsetof(STATUS_VAR, created_tmp_disk_tables), SHOW_LONG_STATUS},
  {"Created_tmp_files",	       (char*) &my_tmp_file_created,SHOW_INT},
  {"Created_tmp_tables",       (char*) offsetof(STATUS_VAR, created_tmp_tables), SHOW_LONG_STATUS},
  {"Flush_commands",           (char*) &refresh_version,    SHOW_INT_NOFLUSH},
  {"Handler_commit",           (char*) offsetof(STATUS_VAR, ha_commit_count), SHOW_LONG_STATUS},
  {"Handler_delete",           (char*) offsetof(STATUS_VAR, ha_delete_count), SHOW_LONG_STATUS},
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
  {"Max_used_connections",     (char*) &max_used_connections,  SHOW_INT},
  {"Open_files",               (char*) &my_file_opened,    SHOW_INT_NOFLUSH},
  {"Open_streams",             (char*) &my_stream_opened,  SHOW_INT_NOFLUSH},
  {"Open_table_definitions",   (char*) &show_table_definitions_cont, SHOW_FUNC},
  {"Open_tables",              (char*) &show_open_tables_cont,       SHOW_FUNC},
  {"Opened_files",             (char*) &my_file_total_opened, SHOW_INT_NOFLUSH},
  {"Opened_tables",            (char*) offsetof(STATUS_VAR, opened_tables), SHOW_LONG_STATUS},
  {"Opened_table_definitions", (char*) offsetof(STATUS_VAR, opened_shares), SHOW_LONG_STATUS},
  {"Questions",                (char*) offsetof(STATUS_VAR, questions), SHOW_LONG_STATUS},
  {"Select_full_join",         (char*) offsetof(STATUS_VAR, select_full_join_count), SHOW_LONG_STATUS},
  {"Select_full_range_join",   (char*) offsetof(STATUS_VAR, select_full_range_join_count), SHOW_LONG_STATUS},
  {"Select_range",             (char*) offsetof(STATUS_VAR, select_range_count), SHOW_LONG_STATUS},
  {"Select_range_check",       (char*) offsetof(STATUS_VAR, select_range_check_count), SHOW_LONG_STATUS},
  {"Select_scan",	       (char*) offsetof(STATUS_VAR, select_scan_count), SHOW_LONG_STATUS},
  {"Slow_queries",             (char*) offsetof(STATUS_VAR, long_query_count), SHOW_LONG_STATUS},
  {"Sort_merge_passes",	       (char*) offsetof(STATUS_VAR, filesort_merge_passes), SHOW_LONG_STATUS},
  {"Sort_range",	       (char*) offsetof(STATUS_VAR, filesort_range_count), SHOW_LONG_STATUS},
  {"Sort_rows",		       (char*) offsetof(STATUS_VAR, filesort_rows), SHOW_LONG_STATUS},
  {"Sort_scan",		       (char*) offsetof(STATUS_VAR, filesort_scan_count), SHOW_LONG_STATUS},
  {"Table_locks_immediate",    (char*) &locks_immediate,        SHOW_INT},
  {"Table_locks_waited",       (char*) &locks_waited,           SHOW_INT},
  {"Threads_connected",        (char*) &connection_count,       SHOW_INT},
  {"Uptime",                   (char*) &show_starttime_cont,         SHOW_FUNC},
  {"Uptime_since_flush_status",(char*) &show_flushstatustime_cont,   SHOW_FUNC},
  {NULL, NULL, SHOW_LONGLONG}
};

static void print_version(void)
{
  set_server_version();
  /*
    Note: the instance manager keys off the string 'Ver' so it can find the
    version from the output of 'drizzled --version', so don't change it!
  */
  printf("%s  Ver %s for %s on %s (%s)\n",my_progname,
	 server_version,SYSTEM_TYPE,MACHINE_TYPE, COMPILATION_COMMENT);
}

static void usage(void)
{
  if (!(default_charset_info= get_charset_by_csname(default_character_set_name, MY_CS_PRIMARY)))
    exit(1);
  if (!default_collation_name)
    default_collation_name= (char*) default_charset_info->name;
  print_version();
  puts(_("Copyright (C) 2008 Sun Microsystems\n"
         "This software comes with ABSOLUTELY NO WARRANTY. "
         "This is free software,\n"
         "and you are welcome to modify and redistribute it under the GPL "
         "license\n\n"
         "Starts the Drizzle database server\n"));

  printf(_("Usage: %s [OPTIONS]\n"), my_progname);
  {
#ifdef FOO
  print_defaults(DRIZZLE_CONFIG_NAME,load_default_groups);
  puts("");
  set_ports();
#endif

  /* Print out all the options including plugin supplied options */
  my_print_help_inc_plugins(my_long_options, sizeof(my_long_options)/sizeof(my_option));

  puts(_("\nTo see what values a running Drizzle server is using, type\n"
         "'drizzleadmin variables' instead of 'drizzled --help'."));
  }
}


/**
  Initialize all Drizzle global variables to default values.

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

static void drizzle_init_variables(void)
{
  /* Things reset to zero */
  drizzle_home[0]= pidfile_name[0]= 0;
  opt_logname= 0;
  opt_tc_log_file= (char *)"tc.log";      // no hostname in tc_log file name !
  opt_secure_file_priv= 0;
  segfaulted= 0;
  cleanup_done= 0;
  defaults_argc= 0;
  defaults_argv= 0;
  server_id_supplied= 0;
  test_flags= select_errors= dropping_tables= ha_open_options=0;
  wake_thread=0;
  opt_endinfo= using_udf_functions= 0;
  abort_loop= select_thread_in_use= false;
  ready_to_exit= shutdown_in_progress= 0;
  aborted_threads= aborted_connects= 0;
  max_used_connections= 0;
  drizzled_user= drizzled_chroot= opt_init_file= 0;
  my_bind_addr_str= NULL;
  memset(&global_status_var, 0, sizeof(global_status_var));
  key_map_full.set_all();

  /* Character sets */
  system_charset_info= &my_charset_utf8_general_ci;
  files_charset_info= &my_charset_utf8_general_ci;
  table_alias_charset= &my_charset_bin;
  character_set_filesystem= &my_charset_bin;

  /* Things with default values that are not zero */
  delay_key_write_options= (uint32_t) DELAY_KEY_WRITE_ON;
  drizzle_home_ptr= drizzle_home;
  pidfile_name_ptr= pidfile_name;
  language_ptr= language;
  drizzle_data_home= drizzle_real_data_home;
  session_startup_options= (OPTION_AUTO_IS_NULL | OPTION_SQL_NOTES);
  refresh_version= 1L;	/* Increments on each reload */
  thread_id= 1;
  strcpy(server_version, VERSION);
  myisam_recover_options_str= "OFF";
  myisam_stats_method_str= "nulls_unequal";
  session_list.empty();
  key_caches.empty();
  if (!(dflt_key_cache= get_or_create_key_cache(default_key_cache_base.str,
                                                default_key_cache_base.length)))
    exit(1);
  /* set key_cache_hash.default_value = dflt_key_cache */
  multi_keycache_init();

  /* Set directory paths */
  strncpy(language, LANGUAGE, sizeof(language)-1);
  strncpy(drizzle_real_data_home, get_relative_path(LOCALSTATEDIR),
          sizeof(drizzle_real_data_home)-1);
  drizzle_data_home_buff[0]=FN_CURLIB;	// all paths are relative from here
  drizzle_data_home_buff[1]=0;
  drizzle_data_home_len= 2;

  /* Variables in libraries */
  default_character_set_name= (char*) DRIZZLE_DEFAULT_CHARSET_NAME;
  default_collation_name= compiled_default_collation_name;
  character_set_filesystem_name= (char*) "binary";
  lc_time_names_name= (char*) "en_US";
  /* Set default values for some option variables */
  default_storage_engine_str= (char*) "innodb";
  global_system_variables.table_plugin= NULL;
  global_system_variables.tx_isolation= ISO_REPEATABLE_READ;
  global_system_variables.select_limit= (uint64_t) HA_POS_ERROR;
  max_system_variables.select_limit=    (uint64_t) HA_POS_ERROR;
  global_system_variables.max_join_size= (uint64_t) HA_POS_ERROR;
  max_system_variables.max_join_size=   (uint64_t) HA_POS_ERROR;
  global_system_variables.old_alter_table= 0;
  /*
    Default behavior for 4.1 and 5.0 is to treat NULL values as unequal
    when collecting index statistics for MyISAM tables.
  */
  global_system_variables.myisam_stats_method= MI_STATS_METHOD_NULLS_NOT_EQUAL;

  /* Variables that depends on compile options */
#ifdef HAVE_BROKEN_REALPATH
  have_symlink=SHOW_OPTION_NO;
#else
  have_symlink=SHOW_OPTION_YES;
#endif

  const char *tmpenv;
  if (!(tmpenv = getenv("MY_BASEDIR_VERSION")))
    tmpenv = PREFIX;
  (void) strncpy(drizzle_home, tmpenv, sizeof(drizzle_home)-1);
  
  connection_count= 0;
}


bool
drizzled_get_one_option(int optid, const struct my_option *opt,
                        char *argument)
{
  switch(optid) {
  case '#':
    opt_endinfo=1;				/* unireg: memory allocation */
    break;
  case 'a':
    global_system_variables.tx_isolation= ISO_SERIALIZABLE;
    break;
  case 'b':
    strncpy(drizzle_home,argument,sizeof(drizzle_home)-1);
    break;
  case 'C':
    if (default_collation_name == compiled_default_collation_name)
      default_collation_name= 0;
    break;
  case 'h':
    strncpy(drizzle_real_data_home,argument, sizeof(drizzle_real_data_home)-1);
    /* Correct pointer set by my_getopt (for embedded library) */
    drizzle_data_home= drizzle_real_data_home;
    drizzle_data_home_len= strlen(drizzle_data_home);
    break;
  case 'u':
    if (!drizzled_user || !strcmp(drizzled_user, argument))
      drizzled_user= argument;
    else
      errmsg_printf(ERRMSG_LVL_WARN, _("Ignoring user change to '%s' because the user was "
                          "set to '%s' earlier on the command line\n"),
                        argument, drizzled_user);
    break;
  case 'L':
    strncpy(language, argument, sizeof(language)-1);
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
    test_flags= argument ? (uint32_t) atoi(argument) : 0;
    opt_endinfo=1;
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

      memset(&hints, 0, sizeof(struct addrinfo));
      hints.ai_socktype= SOCK_STREAM;
      hints.ai_protocol= IPPROTO_TCP;

      if (getaddrinfo(argument, NULL, &hints, &res_lst) != 0)
      {
          errmsg_printf(ERRMSG_LVL_ERROR, _("Can't start server: cannot resolve hostname!"));
        exit(1);
      }

      if (res_lst->ai_next)
      {
          errmsg_printf(ERRMSG_LVL_ERROR, _("Can't start server: bind-address refers to "
                          "multiple interfaces!"));
        exit(1);
      }
      freeaddrinfo(res_lst);
    }
    break;
  case (int) OPT_PID_FILE:
    strncpy(pidfile_name, argument, sizeof(pidfile_name)-1);
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
      delay_key_write_options= (uint32_t) DELAY_KEY_WRITE_NONE;
    else if (! argument)
      delay_key_write_options= (uint32_t) DELAY_KEY_WRITE_ON;
    else
    {
      int type;
      type= find_type_or_exit(argument, &delay_key_write_typelib, opt->name);
      delay_key_write_options= (uint32_t) type-1;
    }
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
      uint32_t method_conv;
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
  }
  return 0;
}


/** Handle arguments for multiple key caches. */

extern "C" char **drizzle_getopt_value(const char *keyname, uint32_t key_length,
                                       const struct my_option *option);

char**
drizzle_getopt_value(const char *keyname, uint32_t key_length,
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
      return (char**) &key_cache->param_buff_size;
    case OPT_KEY_CACHE_BLOCK_SIZE:
      return (char**) &key_cache->param_block_size;
    case OPT_KEY_CACHE_DIVISION_LIMIT:
      return (char**) &key_cache->param_division_limit;
    case OPT_KEY_CACHE_AGE_THRESHOLD:
      return (char**) &key_cache->param_age_threshold;
    }
  }
  }
  return (char **)option->value;
}


extern "C" void option_error_reporter(enum loglevel level, const char *format, ...);

void option_error_reporter(enum loglevel level, const char *format, ...)
{
  va_list args;
  va_start(args, format);

  /* Don't print warnings for --loose options during bootstrap */
  if (level == ERROR_LEVEL || global_system_variables.log_warnings)
  {
    errmsg_vprintf (current_session, ERROR_LEVEL, format, args);
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

  my_getopt_register_get_addr(drizzle_getopt_value);
  my_getopt_error_reporter= option_error_reporter;

  /* Skip unknown options so that they may be processed later by plugins */
  my_getopt_skip_unknown= true;

  if ((ho_error= handle_options(argc, &argv, my_long_options,
                                drizzled_get_one_option)))
    exit(ho_error);
  (*argc)++; /* add back one for the progname handle_options removes */
             /* no need to do this for argv as we are discarding it. */

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
  fix_delay_key_write((Session*) 0, OPT_GLOBAL);

  if (drizzled_chroot)
    set_root(drizzled_chroot);
  fix_paths();

  /*
    Set some global variables from the global_system_variables
    In most cases the global variables will not be used
  */
  my_default_record_cache_size=global_system_variables.read_buff_size;
  myisam_max_temp_length= INT32_MAX;
}

/*
  Create version name for running drizzled version
  We automaticly add suffixes -debug, -embedded and -log to the version
  name to make the version more descriptive.
  (DRIZZLE_SERVER_SUFFIX is set by the compilation environment)
*/

#ifdef DRIZZLE_SERVER_SUFFIX
#define DRIZZLE_SERVER_SUFFIX_STR STRINGIFY_ARG(DRIZZLE_SERVER_SUFFIX)
#else
#define DRIZZLE_SERVER_SUFFIX_STR ""
#endif

static void set_server_version(void)
{
  char *end= server_version;
  end+= sprintf(server_version, "%s%s", VERSION, 
                DRIZZLE_SERVER_SUFFIX_STR);
}


static const char *get_relative_path(const char *path)
{
  if (test_if_hard_path(path) &&
      is_prefix(path,PREFIX) &&
      strcmp(PREFIX,FN_ROOTDIR))
  {
    if (strlen(PREFIX) < strlen(path))
      path+=(size_t) strlen(PREFIX);
    while (*path == FN_LIBCHAR)
      path++;
  }
  return path;
}


static void fix_paths(void)
{
  char buff[FN_REFLEN],*pos;
  convert_dirname(drizzle_home,drizzle_home,NULL);
  /* Resolve symlinks to allow 'drizzle_home' to be a relative symlink */
  my_realpath(drizzle_home,drizzle_home,MYF(0));
  /* Ensure that drizzle_home ends in FN_LIBCHAR */
  pos= strchr(drizzle_home, '\0');
  if (pos[-1] != FN_LIBCHAR)
  {
    pos[0]= FN_LIBCHAR;
    pos[1]= 0;
  }
  convert_dirname(drizzle_real_data_home,drizzle_real_data_home,NULL);
  (void) fn_format(buff, drizzle_real_data_home, "", "",
                   (MY_RETURN_REAL_PATH|MY_RESOLVE_SYMLINKS));
  (void) unpack_dirname(drizzle_unpacked_real_data_home, buff);
  convert_dirname(language,language,NULL);
  (void) my_load_path(drizzle_home, drizzle_home,""); // Resolve current dir
  (void) my_load_path(drizzle_real_data_home, drizzle_real_data_home,drizzle_home);
  (void) my_load_path(pidfile_name, pidfile_name,drizzle_real_data_home);
  (void) my_load_path(opt_plugin_dir, opt_plugin_dir_ptr ? opt_plugin_dir_ptr :
                                      get_relative_path(PKGPLUGINDIR),
                                      drizzle_home);
  opt_plugin_dir_ptr= opt_plugin_dir;

  const char *sharedir= get_relative_path(PKGDATADIR);
  if (test_if_hard_path(sharedir))
    strncpy(buff,sharedir,sizeof(buff)-1);		/* purecov: tested */
  else
  {
    strcpy(buff, drizzle_home);
    strncat(buff, sharedir, sizeof(buff)-strlen(drizzle_home)-1);
  }
  convert_dirname(buff,buff,NULL);
  (void) my_load_path(language,language,buff);

  {
    char *tmp_string;
    struct stat buf;

    tmp_string= getenv("TMPDIR");

    if (opt_drizzle_tmpdir)
      drizzle_tmpdir= strdup(opt_drizzle_tmpdir);
    else if (tmp_string == NULL)
      drizzle_tmpdir= strdup(P_tmpdir);
    else
      drizzle_tmpdir= strdup(tmp_string);

    assert(drizzle_tmpdir);

    if (stat(drizzle_tmpdir, &buf) || (S_ISDIR(buf.st_mode) == false))
    {
      exit(1);
    }
  }

  /*
    Convert the secure-file-priv option to system format, allowing
    a quick strcmp to check if read or write is in an allowed dir
   */
  if (opt_secure_file_priv)
  {
    convert_dirname(buff, opt_secure_file_priv, NULL);
    free(opt_secure_file_priv);
    opt_secure_file_priv= strdup(buff);
    if (opt_secure_file_priv == NULL)
      exit(1);
  }
}


static uint32_t find_bit_type_or_exit(const char *x, TYPELIB *bit_lib,
                                      const char *option)
{
  uint32_t res;

  const char **ptr;

  if ((res= find_bit_type(x, bit_lib)) == ~(uint32_t) 0)
  {
    ptr= bit_lib->type_names;
    if (!*x)
      fprintf(stderr, _("No option given to %s\n"), option);
    else
      fprintf(stderr, _("Wrong option to %s. Option(s) given: %s\n"),
              option, x);
    fprintf(stderr, _("Alternatives are: '%s'"), *ptr);
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
    ~(uint32_t) 0 on error.
*/

static uint32_t find_bit_type(const char *x, TYPELIB *bit_lib)
{
  bool found_end;
  int  found_count;
  const char *end,*i,*j;
  const char **array, *pos;
  uint32_t found,found_int,bit;

  found=0;
  found_end= 0;
  pos=(char *) x;
  while (*pos == ' ') pos++;
  found_end= *pos == 0;
  while (!found_end)
  {
    if ((end=strrchr(pos,',')) != NULL)		/* Let end point at fieldend */
    {
      while (end > pos && end[-1] == ' ')
	end--;					/* Skip end-space */
      found_end=1;
    }
    else
    {
        end=pos+strlen(pos);
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
      return(~(uint32_t) 0);				// No unique value
    found|=found_int;
    pos=end+1;
  }

  return(found);
} /* find_bit_type */

/*****************************************************************************
  Instantiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
/* Used templates */
template class I_List<Session>;
template class I_List_iterator<Session>;
template class I_List<i_string>;
template class I_List<i_string_pair>;
template class I_List<NAMED_LIST>;
template class I_List<Statement>;
template class I_List_iterator<Statement>;
#endif
