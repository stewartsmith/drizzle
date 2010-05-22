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

#include "config.h"
#include <drizzled/configmake.h>
#include <drizzled/atomics.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <signal.h>
#include <limits.h>

#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/my_bit.h"
#include <drizzled/my_hash.h>
#include <drizzled/error.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/tztime.h>
#include <drizzled/sql_base.h>
#include <drizzled/show.h>
#include <drizzled/sql_parse.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/session.h>
#include <drizzled/item/create.h>
#include <drizzled/unireg.h>
#include "drizzled/temporal_format.h" /* For init_temporal_formats() */
#include "drizzled/plugin/listen.h"
#include "drizzled/plugin/error_message.h"
#include "drizzled/plugin/client.h"
#include "drizzled/plugin/scheduler.h"
#include "drizzled/plugin/xa_resource_manager.h"
#include "drizzled/plugin/monitored_in_transaction.h"
#include "drizzled/replication_services.h" /* For ReplicationServices::evaluateRegisteredPlugins() */
#include "drizzled/probes.h"
#include "drizzled/session_list.h"
#include "drizzled/charset.h"
#include "plugin/myisam/myisam.h"
#include "drizzled/drizzled.h"

#include <google/protobuf/stubs/common.h>

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

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#include <sys/socket.h>


#include <errno.h>
#include <sys/stat.h>
#include "drizzled/option.h"
#ifdef HAVE_SYSENT_H
#include <sysent.h>
#endif
#include <pwd.h>				// For getpwent
#include <grp.h>

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

#if defined(__FreeBSD__) && defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif /* __FreeBSD__ && HAVE_IEEEFP_H */

#ifdef HAVE_FPU_CONTROL_H
#include <fpu_control.h>
#endif

#ifdef HAVE_SYS_FPU_H
/* for IRIX to use set_fpc_csr() */
#include <sys/fpu.h>
#endif

#include "drizzled/internal/my_pthread.h"			// For thr_setconcurency()

#include <drizzled/gettext.h>


#ifdef HAVE_purify
#define IF_PURIFY(A,B) (A)
#else
#define IF_PURIFY(A,B) (B)
#endif

#define MAX_MEM_TABLE_SIZE SIZE_MAX

using namespace std;

namespace drizzled
{

#define mysqld_charset &my_charset_utf8_general_ci
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

#ifdef SOLARIS
extern "C" int gethostname(char *name, int namelen);
#endif

/* Constants */
static const char *tc_heuristic_recover_names[]=
{
  "COMMIT", "ROLLBACK", NULL
};
static TYPELIB tc_heuristic_recover_typelib=
{
  array_elements(tc_heuristic_recover_names)-1,"",
  tc_heuristic_recover_names, NULL
};

const char *first_keyword= "first";
const char * const DRIZZLE_CONFIG_NAME= "drizzled";
#define GET_HA_ROWS GET_ULL

const char *tx_isolation_names[] =
{ "READ-UNCOMMITTED", "READ-COMMITTED", "REPEATABLE-READ", "SERIALIZABLE",
  NULL};

TYPELIB tx_isolation_typelib= {array_elements(tx_isolation_names)-1,"",
                               tx_isolation_names, NULL};

/*
  Used with --help for detailed option
*/
bool opt_help= false;
bool opt_help_extended= false;

arg_cmp_func Arg_comparator::comparator_matrix[5][2] =
{{&Arg_comparator::compare_string,     &Arg_comparator::compare_e_string},
 {&Arg_comparator::compare_real,       &Arg_comparator::compare_e_real},
 {&Arg_comparator::compare_int_signed, &Arg_comparator::compare_e_int},
 {&Arg_comparator::compare_row,        &Arg_comparator::compare_e_row},
 {&Arg_comparator::compare_decimal,    &Arg_comparator::compare_e_decimal}};

/* static variables */

static bool opt_debugging= 0;
static uint32_t wake_thread;
static char *drizzled_chroot;
static char *language_ptr;
static const char *default_character_set_name;
static const char *character_set_filesystem_name;
static char *lc_time_names_name;
static char *default_collation_name;
static char *default_storage_engine_str;
static const char *compiled_default_collation_name= "utf8_general_ci";

/* Global variables */

bool volatile ready_to_exit;
char *drizzled_user;
bool volatile select_thread_in_use;
bool volatile abort_loop;
bool volatile shutdown_in_progress;
char *opt_scheduler_default;
char *opt_scheduler= NULL;

size_t my_thread_stack_size= 65536;

/*
  Legacy global plugin::StorageEngine. These will be removed (please do not add more).
*/
plugin::StorageEngine *heap_engine;
plugin::StorageEngine *myisam_engine;

char* opt_secure_file_priv= 0;

bool calling_initgroups= false; /**< Used in SIGSEGV handler. */

uint32_t drizzled_bind_timeout;
std::bitset<12> test_flags;
uint32_t dropping_tables, ha_open_options;
uint32_t tc_heuristic_recover= 0;
uint64_t session_startup_options;
uint32_t back_log;
uint32_t server_id;
uint64_t table_cache_size;
size_t table_def_size;
uint64_t max_connect_errors;
uint32_t global_thread_id= 1UL;
pid_t current_pid;

extern const double log_10[309];

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

char drizzle_home[FN_REFLEN], pidfile_name[FN_REFLEN], system_time_zone[30];
char *default_tz_name;
char glob_hostname[FN_REFLEN];
char data_home_real[FN_REFLEN],
     language[FN_REFLEN], 
     *opt_tc_log_file;
char data_home_real_unpacked[FN_REFLEN];
const key_map key_map_empty(0);
key_map key_map_full(0);                        // Will be initialized later

uint32_t data_home_len;
char data_home_buff[2], *data_home=data_home_real;
char *drizzle_tmpdir= NULL;
char *opt_drizzle_tmpdir= NULL;

/** name of reference on left espression in rewritten IN subquery */
const char *in_left_expr_name= "<left expr>";
/** name of additional condition */
const char *in_additional_cond= "<IN COND>";
const char *in_having_cond= "<IN HAVING>";

my_decimal decimal_zero;
/* classes for comparation parsing/processing */

FILE *stderror_file=0;

struct system_variables global_system_variables;
struct system_variables max_system_variables;
struct system_status_var global_status_var;
struct global_counters current_global_counters;

const CHARSET_INFO *system_charset_info, *files_charset_info ;
const CHARSET_INFO *table_alias_charset;
const CHARSET_INFO *character_set_filesystem;

MY_LOCALE *my_default_lc_time_names;

SHOW_COMP_OPTION have_symlink;

/* Thread specific variables */

pthread_key_t THR_Mem_root;
pthread_key_t THR_Session;
pthread_mutex_t LOCK_create_db;
pthread_mutex_t LOCK_open;
pthread_mutex_t LOCK_thread_count;
pthread_mutex_t LOCK_status;
pthread_mutex_t LOCK_global_read_lock;
pthread_mutex_t LOCK_global_system_variables;

pthread_rwlock_t	LOCK_system_variables_hash;
pthread_cond_t COND_refresh, COND_thread_count, COND_global_read_lock;
pthread_t signal_thread;
pthread_cond_t  COND_server_end;

/* Static variables */

int cleanup_done;
static char *drizzle_home_ptr, *pidfile_name_ptr;
static int defaults_argc;
static char **defaults_argv;

passwd *user_info;

/**
  Number of currently active user connections. The variable is protected by
  LOCK_thread_count.
*/
atomic<uint32_t> connection_count;

/** 
  Refresh value. We use to test this to find out if a refresh even has happened recently.
*/
uint64_t refresh_version;  /* Increments on each reload */

/* Function declarations */
bool drizzle_rm_tmp_tables();

static void drizzle_init_variables(void);
static void get_options(int *argc,char **argv);
int drizzled_get_one_option(int, const struct option *, char *);
static int init_thread_environment();
static const char *get_relative_path(const char *path);
static void fix_paths(string &progname);

static void usage(void);
void close_connections(void);
 
/****************************************************************************
** Code to end drizzled
****************************************************************************/

void close_connections(void)
{
  /* Abort listening to new connections */
  plugin::Listen::shutdown();

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

  (void) pthread_mutex_lock(&LOCK_thread_count); // For unlink from list

  for( SessionList::iterator it= getSessionList().begin(); it != getSessionList().end(); ++it )
  {
    tmp= *it;
    tmp->killed= Session::KILL_CONNECTION;
    tmp->scheduler->killSession(tmp);
    DRIZZLE_CONNECTION_DONE(tmp->thread_id);
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
    if (getSessionList().empty())
    {
      (void) pthread_mutex_unlock(&LOCK_thread_count);
      break;
    }
    tmp= getSessionList().front();
    /* Close before unlock, avoiding crash. See LP bug#436685 */
    tmp->client->close();
    (void) pthread_mutex_unlock(&LOCK_thread_count);
  }
}

/**
  cleanup all memory and end program nicely.

    If SIGNALS_DONT_BREAK_READ is defined, this function is called
    by the main thread. To get Drizzle to shut down nicely in this case
    (Mac OS X) we have to call exit() instead if pthread_exit().

  @note
    This function never returns.
*/
void unireg_end(void)
{
  clean_up(1);
  internal::my_thread_end();
#if defined(SIGNALS_DONT_BREAK_READ)
  exit(0);
#else
  pthread_exit(0);				// Exit is in main thread
#endif
}


void unireg_abort(int exit_code)
{

  if (exit_code)
    errmsg_printf(ERRMSG_LVL_ERROR, _("Aborting\n"));
  else if (opt_help || opt_help_extended)
    usage();
  clean_up(!opt_help && (exit_code));
  clean_up_mutexes();
  internal::my_end();
  exit(exit_code);
}


void clean_up(bool print_message)
{
  if (cleanup_done++)
    return;

  table_cache_free();
  TableShare::cacheStop();
  set_var_free();
  free_charsets();
  plugin::Registry &plugins= plugin::Registry::singleton();
  plugin_shutdown(plugins);
  xid_cache_free();
  free_status_vars();
  if (defaults_argv)
    internal::free_defaults(defaults_argv);
  free(drizzle_tmpdir);
  if (opt_secure_file_priv)
    free(opt_secure_file_priv);

  deinit_temporal_formats();

#if GOOGLE_PROTOBUF_VERSION >= 2001000
  google::protobuf::ShutdownProtobufLibrary();
#endif

  (void) unlink(pidfile_name);	// This may not always exist

  if (print_message && server_start_time)
    errmsg_printf(ERRMSG_LVL_INFO, _(ER(ER_SHUTDOWN_COMPLETE)),internal::my_progname);
  (void) pthread_mutex_lock(&LOCK_thread_count);
  ready_to_exit=1;
  /* do the broadcast inside the lock to ensure that my_end() is not called */
  (void) pthread_cond_broadcast(&COND_server_end);
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  /*
    The following lines may never be executed as the main thread may have
    killed us
  */
} /* clean_up */


void clean_up_mutexes()
{
  (void) pthread_mutex_destroy(&LOCK_create_db);
  (void) pthread_mutex_destroy(&LOCK_open);
  (void) pthread_mutex_destroy(&LOCK_thread_count);
  (void) pthread_mutex_destroy(&LOCK_status);
  (void) pthread_mutex_destroy(&LOCK_global_system_variables);
  (void) pthread_rwlock_destroy(&LOCK_system_variables_hash);
  (void) pthread_mutex_destroy(&LOCK_global_read_lock);
  (void) pthread_cond_destroy(&COND_thread_count);
  (void) pthread_cond_destroy(&COND_server_end);
  (void) pthread_cond_destroy(&COND_refresh);
  (void) pthread_cond_destroy(&COND_global_read_lock);
}


/* Change to run as another user if started with --user */

passwd *check_user(const char *user)
{
  passwd *tmp_user_info;
  uid_t user_id= geteuid();

  // Don't bother if we aren't superuser
  if (user_id)
  {
    if (user)
    {
      /* Don't give a warning, if real user is same as given with --user */
      tmp_user_info= getpwnam(user);
      if ((!tmp_user_info || user_id != tmp_user_info->pw_uid) &&
          global_system_variables.log_warnings)
            errmsg_printf(ERRMSG_LVL_WARN, _("One can only use the --user switch "
                            "if running as root\n"));
    }
    return NULL;
  }
  if (!user)
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Fatal error: Please read \"Security\" section of "
                      "the manual to find out how to run drizzled as root!\n"));
    unireg_abort(1);
  }
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

err:
  errmsg_printf(ERRMSG_LVL_ERROR, _("Fatal error: Can't change to run as user '%s' ;  "
                    "Please check that the user exists!\n"),user);
  unireg_abort(1);

#ifdef PR_SET_DUMPABLE
  if (test_flags.test(TEST_CORE_ON_SIGNAL))
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

void set_user(const char *user, passwd *user_info_arg)
{
  assert(user_info_arg != 0);
  /*
    We can get a SIGSEGV when calling initgroups() on some systems when NSS
    is configured to use LDAP and the server is statically linked.  We set
    calling_initgroups as a flag to the SIGSEGV handler that is then used to
    output a specific message to help the user resolve this problem.
  */
  calling_initgroups= true;
  initgroups((char*) user, user_info_arg->pw_gid);
  calling_initgroups= false;
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


/*
  Unlink session from global list of available connections and free session

  SYNOPSIS
    Session::unlink()
    session		 Thread handler

  NOTES
    LOCK_thread_count is locked and left locked
*/

void Session::unlink(Session *session)
{
  connection_count.decrement();

  session->cleanup();

  (void) pthread_mutex_lock(&LOCK_thread_count);
  pthread_mutex_lock(&session->LOCK_delete);

  getSessionList().erase(remove(getSessionList().begin(),
                         getSessionList().end(),
                         session));

  delete session;
  (void) pthread_mutex_unlock(&LOCK_thread_count);

  return;
}


#ifdef THREAD_SPECIFIC_SIGPIPE
/**

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


#ifndef SA_RESETHAND
#define SA_RESETHAND 0
#endif
#ifndef SA_NODEFER
#define SA_NODEFER 0
#endif




const char *load_default_groups[]= 
{
  DRIZZLE_CONFIG_NAME, "server", 0, 0
};

static int show_starttime(drizzle_show_var *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (time(NULL) - server_start_time);
  return 0;
}

static int show_flushstatustime(drizzle_show_var *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (time(NULL) - flush_status_time);
  return 0;
}

static st_show_var_func_container show_starttime_cont= { &show_starttime };

static st_show_var_func_container show_flushstatustime_cont= { &show_flushstatustime };

static drizzle_show_var status_vars[]= {
  {"Aborted_clients",          (char*) &current_global_counters.aborted_threads, SHOW_LONGLONG},
  {"Aborted_connects",         (char*) &current_global_counters.aborted_connects, SHOW_LONGLONG},
  {"Bytes_received",           (char*) offsetof(system_status_var, bytes_received), SHOW_LONGLONG_STATUS},
  {"Bytes_sent",               (char*) offsetof(system_status_var, bytes_sent), SHOW_LONGLONG_STATUS},
  {"Connections",              (char*) &global_thread_id, SHOW_INT_NOFLUSH},
  {"Created_tmp_disk_tables",  (char*) offsetof(system_status_var, created_tmp_disk_tables), SHOW_LONG_STATUS},
  {"Created_tmp_tables",       (char*) offsetof(system_status_var, created_tmp_tables), SHOW_LONG_STATUS},
  {"Flush_commands",           (char*) &refresh_version,    SHOW_INT_NOFLUSH},
  {"Handler_commit",           (char*) offsetof(system_status_var, ha_commit_count), SHOW_LONG_STATUS},
  {"Handler_delete",           (char*) offsetof(system_status_var, ha_delete_count), SHOW_LONG_STATUS},
  {"Handler_prepare",          (char*) offsetof(system_status_var, ha_prepare_count),  SHOW_LONG_STATUS},
  {"Handler_read_first",       (char*) offsetof(system_status_var, ha_read_first_count), SHOW_LONG_STATUS},
  {"Handler_read_key",         (char*) offsetof(system_status_var, ha_read_key_count), SHOW_LONG_STATUS},
  {"Handler_read_next",        (char*) offsetof(system_status_var, ha_read_next_count), SHOW_LONG_STATUS},
  {"Handler_read_prev",        (char*) offsetof(system_status_var, ha_read_prev_count), SHOW_LONG_STATUS},
  {"Handler_read_rnd",         (char*) offsetof(system_status_var, ha_read_rnd_count), SHOW_LONG_STATUS},
  {"Handler_read_rnd_next",    (char*) offsetof(system_status_var, ha_read_rnd_next_count), SHOW_LONG_STATUS},
  {"Handler_rollback",         (char*) offsetof(system_status_var, ha_rollback_count), SHOW_LONG_STATUS},
  {"Handler_savepoint",        (char*) offsetof(system_status_var, ha_savepoint_count), SHOW_LONG_STATUS},
  {"Handler_savepoint_rollback",(char*) offsetof(system_status_var, ha_savepoint_rollback_count), SHOW_LONG_STATUS},
  {"Handler_update",           (char*) offsetof(system_status_var, ha_update_count), SHOW_LONG_STATUS},
  {"Handler_write",            (char*) offsetof(system_status_var, ha_write_count), SHOW_LONG_STATUS},
  {"Key_blocks_not_flushed",   (char*) offsetof(KEY_CACHE, global_blocks_changed), SHOW_KEY_CACHE_LONG},
  {"Key_blocks_unused",        (char*) offsetof(KEY_CACHE, blocks_unused), SHOW_KEY_CACHE_LONG},
  {"Key_blocks_used",          (char*) offsetof(KEY_CACHE, blocks_used), SHOW_KEY_CACHE_LONG},
  {"Key_read_requests",        (char*) offsetof(KEY_CACHE, global_cache_r_requests), SHOW_KEY_CACHE_LONGLONG},
  {"Key_reads",                (char*) offsetof(KEY_CACHE, global_cache_read), SHOW_KEY_CACHE_LONGLONG},
  {"Key_write_requests",       (char*) offsetof(KEY_CACHE, global_cache_w_requests), SHOW_KEY_CACHE_LONGLONG},
  {"Key_writes",               (char*) offsetof(KEY_CACHE, global_cache_write), SHOW_KEY_CACHE_LONGLONG},
  {"Last_query_cost",          (char*) offsetof(system_status_var, last_query_cost), SHOW_DOUBLE_STATUS},
  {"Max_used_connections",     (char*) &current_global_counters.max_used_connections,  SHOW_INT},
  {"Questions",                (char*) offsetof(system_status_var, questions), SHOW_LONG_STATUS},
  {"Select_full_join",         (char*) offsetof(system_status_var, select_full_join_count), SHOW_LONG_STATUS},
  {"Select_full_range_join",   (char*) offsetof(system_status_var, select_full_range_join_count), SHOW_LONG_STATUS},
  {"Select_range",             (char*) offsetof(system_status_var, select_range_count), SHOW_LONG_STATUS},
  {"Select_range_check",       (char*) offsetof(system_status_var, select_range_check_count), SHOW_LONG_STATUS},
  {"Select_scan",	       (char*) offsetof(system_status_var, select_scan_count), SHOW_LONG_STATUS},
  {"Slow_queries",             (char*) offsetof(system_status_var, long_query_count), SHOW_LONG_STATUS},
  {"Sort_merge_passes",	       (char*) offsetof(system_status_var, filesort_merge_passes), SHOW_LONG_STATUS},
  {"Sort_range",	       (char*) offsetof(system_status_var, filesort_range_count), SHOW_LONG_STATUS},
  {"Sort_rows",		       (char*) offsetof(system_status_var, filesort_rows), SHOW_LONG_STATUS},
  {"Sort_scan",		       (char*) offsetof(system_status_var, filesort_scan_count), SHOW_LONG_STATUS},
  {"Table_locks_immediate",    (char*) &current_global_counters.locks_immediate,        SHOW_INT},
  {"Table_locks_waited",       (char*) &current_global_counters.locks_waited,           SHOW_INT},
  {"Threads_connected",        (char*) &connection_count,       SHOW_INT},
  {"Uptime",                   (char*) &show_starttime_cont,         SHOW_FUNC},
  {"Uptime_since_flush_status",(char*) &show_flushstatustime_cont,   SHOW_FUNC},
  {NULL, NULL, SHOW_LONGLONG}
};

int init_common_variables(const char *conf_file_name, int argc,
                          char **argv, const char **groups)
{
  time_t curr_time;
  umask(((~internal::my_umask) & 0666));
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
  strcpy(internal::fn_ext(pidfile_name),".pid");		// Add proper extension

  if (add_status_vars(status_vars))
    return 1; // an error was already reported

  internal::load_defaults(conf_file_name, groups, &argc, &argv);
  defaults_argv=argv;
  defaults_argc=argc;
  get_options(&defaults_argc, defaults_argv);

  current_pid= getpid();		/* Save for later ref */
  init_time();				/* Init time-functions (read zone) */

  if (item_create_init())
    return 1;
  if (set_var_init())
    return 1;
  /* Creates static regex matching for temporal values */
  if (! init_temporal_formats())
    return 1;

  if (!(default_charset_info=
        get_charset_by_csname(default_character_set_name, MY_CS_PRIMARY)))
  {
    return 1;                           // Eof of the list
  }

  if (default_collation_name)
  {
    const CHARSET_INFO * const default_collation= get_charset_by_name(default_collation_name);
    if (not default_collation)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _(ER(ER_UNKNOWN_COLLATION)), default_collation_name);
      return 1;
    }
    if (not my_charset_same(default_charset_info, default_collation))
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

  if (not (character_set_filesystem=
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

  /* Reset table_alias_charset */
  table_alias_charset= files_charset_info;

  return 0;
}


static int init_thread_environment()
{
   pthread_mutexattr_t attr; 
   pthread_mutexattr_init(&attr);

  (void) pthread_mutex_init(&LOCK_create_db, NULL);
  (void) pthread_mutex_init(&LOCK_open, NULL);

  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); 
  (void) pthread_mutex_init(&LOCK_thread_count, &attr);
  (void) pthread_mutex_init(&LOCK_global_system_variables, &attr);

  (void) pthread_mutex_init(&LOCK_status, MY_MUTEX_INIT_FAST);
  (void) pthread_rwlock_init(&LOCK_system_variables_hash, NULL);
  (void) pthread_mutex_init(&LOCK_global_read_lock, MY_MUTEX_INIT_FAST);
  (void) pthread_cond_init(&COND_thread_count,NULL);
  (void) pthread_cond_init(&COND_server_end,NULL);
  (void) pthread_cond_init(&COND_refresh,NULL);
  (void) pthread_cond_init(&COND_global_read_lock,NULL);

  pthread_mutexattr_destroy(&attr);

  if (pthread_key_create(&THR_Session,NULL) ||
      pthread_key_create(&THR_Mem_root,NULL))
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Can't create thread-keys"));
    return 1;
  }
  return 0;
}


int init_server_components(plugin::Registry &plugins)
{
  /*
    We need to call each of these following functions to ensure that
    all things are initialized so that unireg_abort() doesn't fail
  */
  if (table_cache_init())
    unireg_abort(1);
  TableShare::cacheStart();

  setup_fpu();
  init_thr_lock();

  /* Setup logs */

  if (xid_cache_init())
  {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Out of memory"));
    unireg_abort(1);
  }

  /* Allow storage engine to give real error messages */
  ha_init_errors();

  if (plugin_init(plugins, &defaults_argc, defaults_argv,
                  ((opt_help) ? true : false)))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to initialize plugins."));
    unireg_abort(1);
  }

  if (opt_help || opt_help_extended)
    unireg_abort(0);

  /* we do want to exit if there are any other unknown options */
  if (defaults_argc > 1)
  {
    int ho_error;
    char **tmp_argv= defaults_argv;
    struct option no_opts[]=
    {
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
    };
    /*
      We need to eat any 'loose' arguments first before we conclude
      that there are unprocessed options.
      But we need to preserve defaults_argv pointer intact for
      internal::free_defaults() to work. Thus we use a copy here.
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
              internal::my_progname, *tmp_argv);
      unireg_abort(1);
    }
  }

  string scheduler_name;
  if (opt_scheduler)
  {
    scheduler_name= opt_scheduler;
  }
  else
  {
    scheduler_name= opt_scheduler_default;
    opt_scheduler= opt_scheduler_default; 
  }

  if (plugin::Scheduler::setPlugin(scheduler_name))
  {
      errmsg_printf(ERRMSG_LVL_ERROR,
                   _("No scheduler found, cannot continue!\n"));
      unireg_abort(1);
  }

  /*
    This is entirely for legacy. We will create a new "disk based" engine and a
    "memory" engine which will be configurable longterm.
  */
  const std::string myisam_engine_name("MyISAM");
  const std::string heap_engine_name("MEMORY");
  myisam_engine= plugin::StorageEngine::findByName(myisam_engine_name);
  heap_engine= plugin::StorageEngine::findByName(heap_engine_name);

  /*
    Check that the default storage engine is actually available.
  */
  if (default_storage_engine_str)
  {
    const std::string name(default_storage_engine_str);
    plugin::StorageEngine *engine;

    engine= plugin::StorageEngine::findByName(name);
    if (engine == NULL)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Unknown/unsupported storage engine: %s"),
                    default_storage_engine_str);
      unireg_abort(1);
    }
    global_system_variables.storage_engine= engine;
  }

  if (plugin::XaResourceManager::recoverAllXids(0))
  {
    unireg_abort(1);
  }

  init_update_queries();

  return(0);
}


/****************************************************************************
  Handle start options
******************************************************************************/

enum options_drizzled
{
  OPT_SOCKET=256,
  OPT_BIND_ADDRESS,            
  OPT_PID_FILE,
  OPT_STORAGE_ENGINE,          
  OPT_INIT_FILE,
  OPT_WANT_CORE,
  OPT_MEMLOCK,
  OPT_SERVER_ID,
  OPT_TC_HEURISTIC_RECOVER,
  OPT_TEMP_POOL, OPT_TX_ISOLATION, OPT_COMPLETION_TYPE,
  OPT_SKIP_STACK_TRACE, OPT_SKIP_SYMLINKS,
  OPT_DO_PSTACK,
  OPT_LOCAL_INFILE,
  OPT_BACK_LOG,
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
  OPT_NET_BUFFER_LENGTH,
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
  OPT_PROTOCOL,
  OPT_OPTIMIZER_PRUNE_LEVEL,
  OPT_AUTO_INCREMENT, OPT_AUTO_INCREMENT_OFFSET,
  OPT_ENABLE_LARGE_PAGES,
  OPT_TIMED_MUTEXES,
  OPT_TABLE_LOCK_WAIT_TIMEOUT,
  OPT_PLUGIN_ADD,
  OPT_PLUGIN_REMOVE,
  OPT_PLUGIN_LOAD,
  OPT_PLUGIN_DIR,
  OPT_PORT_OPEN_TIMEOUT,
  OPT_SECURE_FILE_PRIV,
  OPT_MIN_EXAMINED_ROW_LIMIT
};


struct option my_long_options[] =
{
  {"help", '?', N_("Display this help and exit."),
   (char**) &opt_help, (char**) &opt_help, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"help-extended", '?',
   N_("Display this help and exit after initializing plugins."),
   (char**) &opt_help_extended, (char**) &opt_help_extended,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
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
   (char**) &data_home,
   (char**) &data_home, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-storage-engine", OPT_STORAGE_ENGINE,
   N_("Set the default storage engine (table type) for tables."),
   (char**)&default_storage_engine_str, (char**)&default_storage_engine_str,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-time-zone", OPT_DEFAULT_TIME_ZONE,
   N_("Set the default time zone."),
   (char**) &default_tz_name, (char**) &default_tz_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
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
  {"language", 'L',
   N_("(IGNORED)"),
   (char**) &language_ptr, (char**) &language_ptr, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"lc-time-names", OPT_LC_TIME_NAMES,
   N_("Set the language used for the month names and the days of the week."),
   (char**) &lc_time_names_name,
   (char**) &lc_time_names_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"log-warnings", 'W',
   N_("Log some not critical warnings to the log file."),
   (char**) &global_system_variables.log_warnings,
   (char**) &max_system_variables.log_warnings, 0, GET_BOOL, OPT_ARG, 1, 0, 0,
   0, 0, 0},
  {"pid-file", OPT_PID_FILE,
   N_("Pid file used by safe_mysqld."),
   (char**) &pidfile_name_ptr, (char**) &pidfile_name_ptr, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port-open-timeout", OPT_PORT_OPEN_TIMEOUT,
   N_("Maximum time in seconds to wait for the port to become free. "
      "(Default: no wait)"),
   (char**) &drizzled_bind_timeout,
   (char**) &drizzled_bind_timeout, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
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
   (char**) &internal::my_use_symdir, (char**) &internal::my_use_symdir, 0, GET_BOOL, NO_ARG,
   /*
     The system call realpath() produces warnings under valgrind and
     purify. These are not suppressed: instead we disable symlinks
     option if compiled with valgrind support.
   */
   IF_PURIFY(0,1), 0, 0, 0, 0, 0},
  {"timed_mutexes", OPT_TIMED_MUTEXES,
   N_("Specify whether to time mutexes (only InnoDB mutexes are currently "
      "supported)"),
   (char**) &internal::timed_mutexes, (char**) &internal::timed_mutexes, 0, GET_BOOL, NO_ARG, 0,
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
   0, GET_UINT, OPT_ARG, 0, 0, MAX_TABLES+2, 0, 1, 0},
  {"plugin_dir", OPT_PLUGIN_DIR,
   N_("Directory for plugins."),
   (char**) &opt_plugin_dir_ptr, (char**) &opt_plugin_dir_ptr, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin_add", OPT_PLUGIN_ADD,
   N_("Optional comma separated list of plugins to load at startup in addition "
      "to the default list of plugins. "
      "[for example: --plugin_add=crc32,logger_gearman]"),
   (char**) &opt_plugin_add, (char**) &opt_plugin_add, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin_remove", OPT_PLUGIN_ADD,
   N_("Optional comma separated list of plugins to not load at startup. Effectively "
      "removes a plugin from the list of plugins to be loaded. "
      "[for example: --plugin_remove=crc32,logger_gearman]"),
   (char**) &opt_plugin_remove, (char**) &opt_plugin_remove, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin_load", OPT_PLUGIN_LOAD,
   N_("Optional comma separated list of plugins to load at starup instead of "
      "the default plugin load list. "
      "[for example: --plugin_load=crc32,logger_gearman]"),
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
   N_("Select scheduler to be used (by default multi-thread)."),
   (char**)&opt_scheduler, (char**)&opt_scheduler,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  /* x8 compared to MySQL's x2. We have UTF8 to consider. */
  {"sort_buffer_size", OPT_SORT_BUFFER,
   N_("Each thread that needs to do a sort allocates a buffer of this size."),
   (char**) &global_system_variables.sortbuff_size,
   (char**) &max_system_variables.sortbuff_size, 0, GET_SIZE, REQUIRED_ARG,
   MAX_SORT_MEMORY, MIN_SORT_MEMORY+MALLOC_OVERHEAD*8, SIZE_MAX,
   MALLOC_OVERHEAD, 1, 0},
  {"table_definition_cache", OPT_TABLE_DEF_CACHE,
   N_("The number of cached table definitions."),
   (char**) &table_def_size, (char**) &table_def_size,
   0, GET_SIZE, REQUIRED_ARG, 128, 1, 512*1024L, 0, 1, 0},
  {"table_open_cache", OPT_TABLE_OPEN_CACHE,
   N_("The number of cached open tables."),
   (char**) &table_cache_size, (char**) &table_cache_size, 0, GET_UINT64,
   REQUIRED_ARG, TABLE_OPEN_CACHE_DEFAULT, TABLE_OPEN_CACHE_MIN, 512*1024L, 0, 1, 0},
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
   UINT32_C(1024*512), SIZE_MAX, 0, 1024, 0},
  {"tmp_table_size", OPT_TMP_TABLE_SIZE,
   N_("If an internal in-memory temporary table exceeds this size, Drizzle will"
      " automatically convert it to an on-disk MyISAM table."),
   (char**) &global_system_variables.tmp_table_size,
   (char**) &max_system_variables.tmp_table_size, 0, GET_ULL,
   REQUIRED_ARG, 16*1024*1024L, 1024, MAX_MEM_TABLE_SIZE, 0, 1, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void print_version(void)
{
  /*
    Note: the instance manager keys off the string 'Ver' so it can find the
    version from the output of 'drizzled --version', so don't change it!
  */
  printf("%s  Ver %s for %s-%s on %s (%s)\n",internal::my_progname,
	 PANDORA_RELEASE_VERSION, HOST_VENDOR, HOST_OS, HOST_CPU,
         COMPILATION_COMMENT);
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

  printf(_("Usage: %s [OPTIONS]\n"), internal::my_progname);
  {
     internal::print_defaults(DRIZZLE_CONFIG_NAME,load_default_groups);
     puts("");
 
     /* Print out all the options including plugin supplied options */
     my_print_help_inc_plugins(my_long_options);
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
  opt_tc_log_file= (char *)"tc.log";      // no hostname in tc_log file name !
  opt_secure_file_priv= 0;
  cleanup_done= 0;
  defaults_argc= 0;
  defaults_argv= 0;
  dropping_tables= ha_open_options=0;
  test_flags.reset();
  wake_thread=0;
  abort_loop= select_thread_in_use= false;
  ready_to_exit= shutdown_in_progress= 0;
  drizzled_user= drizzled_chroot= 0;
  memset(&global_status_var, 0, sizeof(global_status_var));
  memset(&current_global_counters, 0, sizeof(current_global_counters));
  key_map_full.set();

  /* Character sets */
  system_charset_info= &my_charset_utf8_general_ci;
  files_charset_info= &my_charset_utf8_general_ci;
  table_alias_charset= &my_charset_bin;
  character_set_filesystem= &my_charset_bin;

  /* Things with default values that are not zero */
  drizzle_home_ptr= drizzle_home;
  pidfile_name_ptr= pidfile_name;
  language_ptr= language;
  data_home= data_home_real;
  session_startup_options= (OPTION_AUTO_IS_NULL | OPTION_SQL_NOTES);
  refresh_version= 1L;	/* Increments on each reload */
  global_thread_id= 1UL;
  getSessionList().clear();

  /* Set directory paths */
  strncpy(language, LANGUAGE, sizeof(language)-1);
  strncpy(data_home_real, get_relative_path(LOCALSTATEDIR),
          sizeof(data_home_real)-1);
  data_home_buff[0]=FN_CURLIB;	// all paths are relative from here
  data_home_buff[1]=0;
  data_home_len= 2;

  /* Variables in libraries */
  default_character_set_name= "utf8";
  default_collation_name= (char *)compiled_default_collation_name;
  character_set_filesystem_name= "binary";
  lc_time_names_name= (char*) "en_US";
  /* Set default values for some option variables */
  default_storage_engine_str= (char*) "innodb";
  global_system_variables.storage_engine= NULL;
  global_system_variables.tx_isolation= ISO_REPEATABLE_READ;
  global_system_variables.select_limit= (uint64_t) HA_POS_ERROR;
  max_system_variables.select_limit=    (uint64_t) HA_POS_ERROR;
  global_system_variables.max_join_size= (uint64_t) HA_POS_ERROR;
  max_system_variables.max_join_size=   (uint64_t) HA_POS_ERROR;
  opt_scheduler_default= (char*) "multi_thread";

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


int drizzled_get_one_option(int optid, const struct option *opt,
                             char *argument)
{
  switch(optid) {
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
    strncpy(data_home_real,argument, sizeof(data_home_real)-1);
    /* Correct pointer set by my_getopt (for embedded library) */
    data_home= data_home_real;
    data_home_len= strlen(data_home);
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
    if (argument)
    {
      test_flags.set((uint32_t) atoi(argument));
    }
    break;
  case (int) OPT_WANT_CORE:
    test_flags.set(TEST_CORE_ON_SIGNAL);
    break;
  case (int) OPT_SKIP_STACK_TRACE:
    test_flags.set(TEST_NO_STACKTRACE);
    break;
  case (int) OPT_SKIP_SYMLINKS:
    internal::my_use_symdir=0;
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
        return EXIT_ARGUMENT_INVALID;
      }

      if (res_lst->ai_next)
      {
          errmsg_printf(ERRMSG_LVL_ERROR, _("Can't start server: bind-address refers to "
                          "multiple interfaces!"));
        return EXIT_ARGUMENT_INVALID;
      }
      freeaddrinfo(res_lst);
    }
    break;
  case (int) OPT_PID_FILE:
    strncpy(pidfile_name, argument, sizeof(pidfile_name)-1);
    break;
  case OPT_SERVER_ID:
    break;
  case OPT_TX_ISOLATION:
    {
      int type;
      type= find_type_or_exit(argument, &tx_isolation_typelib, opt->name);
      global_system_variables.tx_isolation= (type-1);
      break;
    }
  case OPT_TC_HEURISTIC_RECOVER:
    tc_heuristic_recover= find_type_or_exit(argument,
                                            &tc_heuristic_recover_typelib,
                                            opt->name);
    break;
  }

  return 0;
}

static void option_error_reporter(enum loglevel level, const char *format, ...)
{
  va_list args;
  va_start(args, format);

  /* Don't print warnings for --loose options during bootstrap */
  if (level == ERROR_LEVEL || global_system_variables.log_warnings)
  {
    plugin::ErrorMessage::vprintf(current_session, ERROR_LEVEL, format, args);
  }
  va_end(args);
}


/**
  @todo
  - FIXME add EXIT_TOO_MANY_ARGUMENTS to "drizzled/error.h" and return that code?
*/
static void get_options(int *argc,char **argv)
{
  int ho_error;

  my_getopt_error_reporter= option_error_reporter;

  string progname(argv[0]);

  /* Skip unknown options so that they may be processed later by plugins */
  my_getopt_skip_unknown= true;

  if ((ho_error= handle_options(argc, &argv, my_long_options,
                                drizzled_get_one_option)))
    exit(ho_error);
  (*argc)++; /* add back one for the progname handle_options removes */
             /* no need to do this for argv as we are discarding it. */

#if defined(HAVE_BROKEN_REALPATH)
  internal::my_use_symdir=0;
  internal::my_disable_symlinks=1;
  have_symlink=SHOW_OPTION_NO;
#else
  if (!internal::my_use_symdir)
  {
    internal::my_disable_symlinks=1;
    have_symlink=SHOW_OPTION_DISABLED;
  }
#endif
  if (opt_debugging)
  {
    /* Allow break with SIGINT, no core or stack trace */
    test_flags.set(TEST_SIGINT);
    test_flags.set(TEST_NO_STACKTRACE);
    test_flags.reset(TEST_CORE_ON_SIGNAL);
  }

  if (drizzled_chroot)
    set_root(drizzled_chroot);
  fix_paths(progname);

  /*
    Set some global variables from the global_system_variables
    In most cases the global variables will not be used
  */
  internal::my_default_record_cache_size=global_system_variables.read_buff_size;
}


static const char *get_relative_path(const char *path)
{
  if (internal::test_if_hard_path(path) &&
      (strncmp(path, PREFIX, strlen(PREFIX)) == 0) &&
      strcmp(PREFIX,FN_ROOTDIR))
  {
    if (strlen(PREFIX) < strlen(path))
      path+=(size_t) strlen(PREFIX);
    while (*path == FN_LIBCHAR)
      path++;
  }
  return path;
}


static void fix_paths(string &progname)
{
  char buff[FN_REFLEN],*pos,rp_buff[PATH_MAX];
  internal::convert_dirname(drizzle_home,drizzle_home,NULL);
  /* Resolve symlinks to allow 'drizzle_home' to be a relative symlink */
#if defined(HAVE_BROKEN_REALPATH)
   internal::my_load_path(drizzle_home, drizzle_home, NULL);
#else
  if (!realpath(drizzle_home,rp_buff))
    internal::my_load_path(rp_buff, drizzle_home, NULL);
  rp_buff[FN_REFLEN-1]= '\0';
  strcpy(drizzle_home,rp_buff);
  /* Ensure that drizzle_home ends in FN_LIBCHAR */
  pos= strchr(drizzle_home, '\0');
#endif
  if (pos[-1] != FN_LIBCHAR)
  {
    pos[0]= FN_LIBCHAR;
    pos[1]= 0;
  }
  internal::convert_dirname(data_home_real,data_home_real,NULL);
  (void) internal::fn_format(buff, data_home_real, "", "",
                   (MY_RETURN_REAL_PATH|MY_RESOLVE_SYMLINKS));
  (void) internal::unpack_dirname(data_home_real_unpacked, buff);
  internal::convert_dirname(language,language,NULL);
  (void) internal::my_load_path(drizzle_home, drizzle_home,""); // Resolve current dir
  (void) internal::my_load_path(data_home_real, data_home_real,drizzle_home);
  (void) internal::my_load_path(pidfile_name, pidfile_name,data_home_real);

  if (opt_plugin_dir_ptr == NULL)
  {
    /* No plugin dir has been specified. Figure out where the plugins are */
    if (progname[0] != FN_LIBCHAR)
    {
      /* We have a relative path and need to find the absolute */
      char working_dir[FN_REFLEN];
      char *working_dir_ptr= working_dir;
      working_dir_ptr= getcwd(working_dir_ptr, FN_REFLEN);
      string new_path(working_dir);
      if (*(new_path.end()-1) != '/')
        new_path.push_back('/');
      if (progname[0] == '.' && progname[1] == '/')
        new_path.append(progname.substr(2));
      else
        new_path.append(progname);
      progname.swap(new_path);
    }

    /* Now, trim off the exe name */
    string progdir(progname.substr(0, progname.rfind(FN_LIBCHAR)+1));
    if (progdir.rfind(".libs/") != string::npos)
    {
      progdir.assign(progdir.substr(0, progdir.rfind(".libs/")));
    }
    string testlofile(progdir);
    testlofile.append("drizzled.lo");
    string testofile(progdir);
    testofile.append("drizzled.o");
    struct stat testfile_stat;
    if (stat(testlofile.c_str(), &testfile_stat) && stat(testofile.c_str(), &testfile_stat))
    {
      /* neither drizzled.lo or drizzled.o exist - we are not in a source dir.
       * Go on as usual
       */
      (void) internal::my_load_path(opt_plugin_dir, get_relative_path(PKGPLUGINDIR),
                                          drizzle_home);
    }
    else
    {
      /* We are in a source dir! Plugin dir is ../plugin/.libs */
      size_t last_libchar_pos= progdir.rfind(FN_LIBCHAR,progdir.size()-2)+1;
      string source_plugindir(progdir.substr(0,last_libchar_pos));
      source_plugindir.append("plugin/.libs");
      (void) internal::my_load_path(opt_plugin_dir, source_plugindir.c_str(), "");
    }
  }
  else
  {
    (void) internal::my_load_path(opt_plugin_dir, opt_plugin_dir_ptr, drizzle_home);
  }
  opt_plugin_dir_ptr= opt_plugin_dir;

  const char *sharedir= get_relative_path(PKGDATADIR);
  if (internal::test_if_hard_path(sharedir))
    strncpy(buff,sharedir,sizeof(buff)-1);
  else
  {
    strcpy(buff, drizzle_home);
    strncat(buff, sharedir, sizeof(buff)-strlen(drizzle_home)-1);
  }
  internal::convert_dirname(buff,buff,NULL);
  (void) internal::my_load_path(language,language,buff);

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
    internal::convert_dirname(buff, opt_secure_file_priv, NULL);
    free(opt_secure_file_priv);
    opt_secure_file_priv= strdup(buff);
    if (opt_secure_file_priv == NULL)
      exit(1);
  }
}

} /* namespace drizzled */

