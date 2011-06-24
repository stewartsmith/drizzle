/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <config.h>

#include <drizzled/configmake.h>
#include <drizzled/atomics.h>
#include <drizzled/data_home.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <signal.h>
#include <limits.h>
#include <stdexcept>

#include <boost/program_options.hpp>
#include <drizzled/program_options/config_file.h>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/filesystem.hpp>
#include <boost/detail/atomic_count.hpp>

#include <drizzled/cached_directory.h>
#include <drizzled/charset.h>
#include <drizzled/data_home.h>
#include <drizzled/debug.h>
#include <drizzled/definition/cache.h>
#include <drizzled/drizzled.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/error.h>
#include <drizzled/global_buffer.h>
#include <drizzled/internal/my_bit.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/create.h>
#include <drizzled/message/cache.h>
#include <drizzled/module/load_list.h>
#include <drizzled/module/registry.h>
#include <drizzled/plugin/client.h>
#include <drizzled/plugin/error_message.h>
#include <drizzled/plugin/event_observer.h>
#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/monitored_in_transaction.h>
#include <drizzled/plugin/scheduler.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/plugin/xa_resource_manager.h>
#include <drizzled/probes.h>
#include <drizzled/replication_services.h> /* For ReplicationServices::evaluateRegisteredPlugins() */
#include <drizzled/session.h>
#include <drizzled/session/cache.h>
#include <drizzled/show.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_parse.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/table/cache.h>
#include <drizzled/temporal_format.h> /* For init_temporal_formats() */
#include <drizzled/unireg.h>
#include <plugin/myisam/myisam.h>
#include <drizzled/typelib.h>
#include <drizzled/visibility.h>
#include <drizzled/system_variables.h>
#include <drizzled/open_tables_state.h>

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
#include <drizzled/option.h>
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

#include <drizzled/internal/my_pthread.h>			// For thr_setconcurency()
#include <drizzled/constrained_value.h>

#include <drizzled/gettext.h>


#ifdef HAVE_VALGRIND
#define IF_PURIFY(A,B) (A)
#else
#define IF_PURIFY(A,B) (B)
#endif

#define MAX_MEM_TABLE_SIZE SIZE_MAX
#include <iostream>
#include <fstream>


using namespace std;
namespace fs=boost::filesystem;
namespace po=boost::program_options;
namespace dpo=drizzled::program_options;

bool opt_daemon= false;

namespace drizzled {

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

const char *first_keyword= "first";
const char * const DRIZZLE_CONFIG_NAME= "drizzled";

#define GET_HA_ROWS GET_ULL

const char *tx_isolation_names[] = {"READ-UNCOMMITTED", "READ-COMMITTED", "REPEATABLE-READ", "SERIALIZABLE", NULL};

TYPELIB tx_isolation_typelib= {array_elements(tx_isolation_names) - 1, "", tx_isolation_names, NULL};

/*
  Used with --help for detailed option
*/
bool opt_help= false;

arg_cmp_func Arg_comparator::comparator_matrix[5][2] =
{{&Arg_comparator::compare_string,     &Arg_comparator::compare_e_string},
 {&Arg_comparator::compare_real,       &Arg_comparator::compare_e_real},
 {&Arg_comparator::compare_int_signed, &Arg_comparator::compare_e_int},
 {&Arg_comparator::compare_row,        &Arg_comparator::compare_e_row},
 {&Arg_comparator::compare_decimal,    &Arg_comparator::compare_e_decimal}};

/* static variables */

static bool opt_debugging= false;
static uint32_t wake_thread;
static const char* drizzled_chroot;
static const char* default_character_set_name= "utf8";
static const char* character_set_filesystem_name= "binary";
static const char* lc_time_names_name= "en_US";
static const char* default_storage_engine_str= "innodb";
static const char* const compiled_default_collation_name= "utf8_general_ci";
static const char* default_collation_name= compiled_default_collation_name;

/* Global variables */

const char *drizzled_user;
bool volatile select_thread_in_use;
bool volatile abort_loop;
DRIZZLED_API bool volatile shutdown_in_progress;
const char* opt_scheduler= "multi_thread";

DRIZZLED_API size_t my_thread_stack_size= 0;

/*
  Legacy global plugin::StorageEngine. These will be removed (please do not add more).
*/
plugin::StorageEngine *heap_engine;
plugin::StorageEngine *myisam_engine;

bool calling_initgroups= false; /**< Used in SIGSEGV handler. */

uint32_t drizzled_bind_timeout;
uint32_t dropping_tables, ha_open_options;
uint32_t tc_heuristic_recover= 0;
uint64_t session_startup_options;
back_log_constraints back_log(SOMAXCONN);
DRIZZLED_API uint32_t server_id;
DRIZZLED_API string server_uuid;
uint64_t table_cache_size;
size_t table_def_size;
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

fs::path basedir(PREFIX);
fs::path pid_file;
fs::path secure_file_priv;
fs::path plugin_dir;
fs::path system_config_dir(SYSCONFDIR);

const char *opt_tc_log_file;
const key_map key_map_empty(0);
key_map key_map_full(0);                        // Will be initialized later

std::string drizzle_tmpdir;
char *opt_drizzle_tmpdir= NULL;

/** name of reference on left espression in rewritten IN subquery */
const char *in_left_expr_name= "<left expr>";
/** name of additional condition */
const char *in_additional_cond= "<IN COND>";
const char *in_having_cond= "<IN HAVING>";

/* classes for comparation parsing/processing */

FILE *stderror_file=0;

drizzle_system_variables global_system_variables;
drizzle_system_variables max_system_variables;
global_counters current_global_counters;

DRIZZLED_API const charset_info_st *system_charset_info;
const charset_info_st *files_charset_info;
const charset_info_st *table_alias_charset;
const charset_info_st *character_set_filesystem;

MY_LOCALE *my_default_lc_time_names;

SHOW_COMP_OPTION have_symlink;

boost::condition_variable_any COND_refresh;
boost::condition_variable COND_thread_count;
pthread_t signal_thread;

/* Static variables */

int cleanup_done;

passwd *user_info;

boost::detail::atomic_count connection_count(0);

global_buffer_constraint<uint64_t> global_sort_buffer(0);
global_buffer_constraint<uint64_t> global_join_buffer(0);
global_buffer_constraint<uint64_t> global_read_rnd_buffer(0);
global_buffer_constraint<uint64_t> global_read_buffer(0);

DRIZZLED_API size_t transaction_message_threshold;

static void drizzle_init_variables();
static void get_options();
static void fix_paths();

static void usage();
void close_connections();

fs::path base_plugin_dir(PKGPLUGINDIR);

po::options_description config_options(_("Config File Options"));
po::options_description long_options(_("Kernel Options"));
po::options_description plugin_load_options(_("Plugin Loading Options"));
po::options_description plugin_options(_("Plugin Options"));
po::options_description initial_options(_("Config and Plugin Loading"));
po::options_description full_options(_("Kernel and Plugin Loading and Plugin"));
vector<string> unknown_options;
vector<string> defaults_file_list;
po::variables_map vm;

po::variables_map &getVariablesMap()
{
  return vm;
}

static std::string g_hostname= "localhost";

const std::string& getServerHostname()
{
  return g_hostname;
}

/****************************************************************************
** Code to end drizzled
****************************************************************************/

void close_connections()
{
  /* Abort listening to new connections */
  plugin::Listen::shutdown();

  /* kill connection thread */
  {
    boost::mutex::scoped_lock scopedLock(session::Cache::mutex());

    while (select_thread_in_use)
    {
      boost::xtime xt;
      xtime_get(&xt, boost::TIME_UTC);
      xt.sec += 2;

      for (uint32_t tmp=0 ; tmp < 10 && select_thread_in_use; tmp++)
      {
        bool success= COND_thread_count.timed_wait(scopedLock, xt);
        if (not success)
          break;
      }
    }
  }


  /*
    First signal all threads that it's time to die
    This will give the threads some time to gracefully abort their
    statements and inform their clients that the server is about to die.
  */

  {
    boost::mutex::scoped_lock scopedLock(session::Cache::mutex());
    session::Cache::list list= session::Cache::getCache();

    BOOST_FOREACH(session::Cache::list::reference tmp, list)
    {
      tmp->setKilled(Session::KILL_CONNECTION);
      tmp->scheduler->killSession(tmp.get());
      DRIZZLE_CONNECTION_DONE(tmp->thread_id);

      tmp->lockOnSys();
    }
  }

  if (session::Cache::count())
    sleep(2);                                   // Give threads time to die

  /*
    Force remaining threads to die by closing the connection to the client
    This will ensure that threads that are waiting for a command from the
    client on a blocking read call are aborted.
  */
  for (;;)
  {
    boost::mutex::scoped_lock scopedLock(session::Cache::mutex());
    session::Cache::list list= session::Cache::getCache();

    if (list.empty())
    {
      break;
    }
    /* Close before unlock, avoiding crash. See LP bug#436685 */
    list.front()->getClient()->close();
  }
}


void unireg_abort(int exit_code)
{
  if (exit_code)
  {
    errmsg_printf(error::ERROR, _("Aborting"));
  }
  else if (opt_help)
  {
    usage();
  }

  clean_up(!opt_help && exit_code);
  internal::my_end();
  exit(exit_code);
}


void clean_up(bool print_message)
{
  if (cleanup_done++)
    return;

  table_cache_free();
  free_charsets();
  module::Registry &modules= module::Registry::singleton();
  modules.shutdownModules();

  deinit_temporal_formats();

#if GOOGLE_PROTOBUF_VERSION >= 2001000
  google::protobuf::ShutdownProtobufLibrary();
#endif

  (void) unlink(pid_file.file_string().c_str());	// This may not always exist

  if (print_message && server_start_time)
    errmsg_printf(drizzled::error::INFO, _(ER(ER_SHUTDOWN_COMPLETE)),internal::my_progname);

  session::Cache::shutdownFirst();

  /*
    The following lines may never be executed as the main thread may have
    killed us
  */
} /* clean_up */


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
            errmsg_printf(error::WARN, _("One can only use the --user switch "
                            "if running as root\n"));
    }
    return NULL;
  }
  if (not user)
  {
      errmsg_printf(error::ERROR, _("Fatal error: Please read \"Security\" section of "
                                    "the manual to find out how to run drizzled as root"));
    unireg_abort(1);
  }

  if (not strcmp(user, "root"))
    return NULL;                        // Avoid problem with dynamic libraries

  if (!(tmp_user_info= getpwnam(user)))
  {
    // Allow a numeric uid to be used
    const char *pos;
    for (pos= user; my_isdigit(&my_charset_utf8_general_ci,*pos); pos++) ;
    if (*pos)                                   // Not numeric id
      goto err;
    if (!(tmp_user_info= getpwuid(atoi(user))))
      goto err;
  }
  return tmp_user_info;

err:
  errmsg_printf(error::ERROR, _("Fatal error: Can't change to run as user '%s' ;  "
                    "Please check that the user exists!\n"),user);
  unireg_abort(1);

#ifdef PR_SET_DUMPABLE
  if (getDebug().test(debug::CORE_ON_SIGNAL))
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
  initgroups(user, user_info_arg->pw_gid);
  if (setgid(user_info_arg->pw_gid) == -1)
  {
    sql_perror(_("Set process group ID failed"));
    unireg_abort(1);
  }
  if (setuid(user_info_arg->pw_uid) == -1)
  {
    sql_perror(_("Set process user ID failed"));
    unireg_abort(1);
  }
}



/** Change root user if started with @c --chroot . */
static void set_root(const char *path)
{
  if ((chroot(path) == -1) || !chdir("/"))
  {
    sql_perror(_("Process chroot failed"));
    unireg_abort(1);
  }
}


/*
  Unlink session from global list of available connections and free session

  SYNOPSIS
    Session::unlink()
    session		 Thread handler
*/

void Session::unlink(session_id_t &session_id)
{
  Session::shared_ptr session= session::Cache::find(session_id);

  if (session)
    unlink(session);
}

void Session::unlink(const Session::shared_ptr& session)
{
  --connection_count;

  session->cleanup();

  boost::mutex::scoped_lock scopedLock(session::Cache::mutex());

  if (unlikely(plugin::EventObserver::disconnectSession(*session)))
  {
    // We should do something about an error...
  }
  session::Cache::erase(session);
}


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

static void find_plugin_dir(string progname)
{
  fs::path full_progname(fs::system_complete(progname));

  fs::path progdir(full_progname.parent_path());
  if (progdir.filename() == ".libs")
  {
    progdir= progdir.parent_path();
  }

  if (fs::exists(progdir / "drizzled.lo") || fs::exists(progdir / "drizzled.o"))
  {
    /* We are in a source dir! Plugin dir is ../plugin/.libs */
    base_plugin_dir= progdir.parent_path();
    base_plugin_dir /= "plugin";
    base_plugin_dir /= ".libs";
  }

  if (plugin_dir.root_directory() == "")
  {
    fs::path full_plugin_dir(fs::system_complete(base_plugin_dir));
    full_plugin_dir /= plugin_dir;
    plugin_dir= full_plugin_dir;
  }
}

static void notify_plugin_dir(fs::path in_plugin_dir)
{
  plugin_dir= in_plugin_dir;
  if (plugin_dir.root_directory() == "")
  {
    fs::path full_plugin_dir(fs::system_complete(basedir));
    full_plugin_dir /= plugin_dir;
    plugin_dir= full_plugin_dir;
  }
}

static void expand_secure_file_priv(fs::path in_secure_file_priv)
{
  secure_file_priv= fs::system_complete(in_secure_file_priv);
}

static void check_limits_aii(uint64_t in_auto_increment_increment)
{
  global_system_variables.auto_increment_increment= 1;
  if (in_auto_increment_increment < 1 || in_auto_increment_increment > UINT64_MAX)
  {
    cout << _("Error: Invalid Value for auto_increment_increment");
    exit(-1);
  }
  global_system_variables.auto_increment_increment= in_auto_increment_increment;
}

static void check_limits_aio(uint64_t in_auto_increment_offset)
{
  global_system_variables.auto_increment_offset= 1;
  if (in_auto_increment_offset < 1 || in_auto_increment_offset > UINT64_MAX)
  {
    cout << _("Error: Invalid Value for auto_increment_offset");
    exit(-1);
  }
  global_system_variables.auto_increment_offset= in_auto_increment_offset;
}

static void check_limits_completion_type(uint32_t in_completion_type)
{
  global_system_variables.completion_type= 0;
  if (in_completion_type > 2)
  {
    cout << _("Error: Invalid Value for completion_type");
    exit(-1);
  }
  global_system_variables.completion_type= in_completion_type;
}


static void check_limits_dpi(uint32_t in_div_precincrement)
{
  global_system_variables.div_precincrement= 4;
  if (in_div_precincrement > DECIMAL_MAX_SCALE)
  {
    cout << _("Error: Invalid Value for div-precision-increment");
    exit(-1);
  }
  global_system_variables.div_precincrement= in_div_precincrement;
}

static void check_limits_gcml(uint64_t in_group_concat_max_len)
{
  global_system_variables.group_concat_max_len= 1024;
  if (in_group_concat_max_len > ULONG_MAX || in_group_concat_max_len < 4)
  {
    cout << _("Error: Invalid Value for group_concat_max_len");
    exit(-1);
  }
  global_system_variables.group_concat_max_len= in_group_concat_max_len;
}

static void check_limits_join_buffer_size(uint64_t in_join_buffer_size)
{
  global_system_variables.join_buff_size= (128*1024L);
  if (in_join_buffer_size < IO_SIZE*2 || in_join_buffer_size > ULONG_MAX)
  {
    cout << _("Error: Invalid Value for join_buffer_size");
    exit(-1);
  }
  in_join_buffer_size-= in_join_buffer_size % IO_SIZE;
  global_system_variables.join_buff_size= in_join_buffer_size;
}

static void check_limits_map(uint32_t in_max_allowed_packet)
{
  global_system_variables.max_allowed_packet= (64*1024*1024L);
  if (in_max_allowed_packet < 1024 || in_max_allowed_packet > 1024*1024L*1024L)
  {
    cout << _("Error: Invalid Value for max_allowed_packet");
    exit(-1);
  }
  in_max_allowed_packet-= in_max_allowed_packet % 1024;
  global_system_variables.max_allowed_packet= in_max_allowed_packet;
}

static void check_limits_max_err_cnt(uint64_t in_max_error_count)
{
  global_system_variables.max_error_count= DEFAULT_ERROR_COUNT;
  if (in_max_error_count > 65535)
  {
    cout << _("Error: Invalid Value for max_error_count");
    exit(-1);
  }
  global_system_variables.max_error_count= in_max_error_count;
}

static void check_limits_mhts(uint64_t in_max_heap_table_size)
{
  global_system_variables.max_heap_table_size= (16*1024*1024L);
  if (in_max_heap_table_size < 16384 || in_max_heap_table_size > MAX_MEM_TABLE_SIZE)
  {
    cout << _("Error: Invalid Value for max_heap_table_size");
    exit(-1);
  }
  in_max_heap_table_size-= in_max_heap_table_size % 1024;
  global_system_variables.max_heap_table_size= in_max_heap_table_size;
}

static void check_limits_merl(uint64_t in_min_examined_row_limit)
{
  global_system_variables.min_examined_row_limit= 0;
  if (in_min_examined_row_limit > ULONG_MAX)
  {
    cout << _("Error: Invalid Value for min_examined_row_limit");
    exit(-1);
  }
  global_system_variables.min_examined_row_limit= in_min_examined_row_limit;
}

static void check_limits_max_join_size(ha_rows in_max_join_size)
{
  global_system_variables.max_join_size= INT32_MAX;
  if ((uint64_t)in_max_join_size < 1 || (uint64_t)in_max_join_size > INT32_MAX)
  {
    cout << _("Error: Invalid Value for max_join_size");
    exit(-1);
  }
  global_system_variables.max_join_size= in_max_join_size;
}

static void check_limits_mlfsd(int64_t in_max_length_for_sort_data)
{
  global_system_variables.max_length_for_sort_data= 1024;
  if (in_max_length_for_sort_data < 4 || in_max_length_for_sort_data > 8192*1024L)
  {
    cout << _("Error: Invalid Value for max_length_for_sort_data");
    exit(-1);
  }
  global_system_variables.max_length_for_sort_data= in_max_length_for_sort_data;
}

static void check_limits_msfk(uint64_t in_max_seeks_for_key)
{
  global_system_variables.max_seeks_for_key= ULONG_MAX;
  if (in_max_seeks_for_key < 1 || in_max_seeks_for_key > ULONG_MAX)
  {
    cout << _("Error: Invalid Value for max_seeks_for_key");
    exit(-1);
  }
  global_system_variables.max_seeks_for_key= in_max_seeks_for_key;
}

static void check_limits_max_sort_length(size_t in_max_sort_length)
{
  global_system_variables.max_sort_length= 1024;
  if ((int64_t)in_max_sort_length < 4 || (int64_t)in_max_sort_length > 8192*1024L)
  {
    cout << _("Error: Invalid Value for max_sort_length");
    exit(-1);
  }
  global_system_variables.max_sort_length= in_max_sort_length;
}

static void check_limits_osd(uint32_t in_optimizer_search_depth)
{
  global_system_variables.optimizer_search_depth= 0;
  if (in_optimizer_search_depth > MAX_TABLES + 2)
  {
    cout << _("Error: Invalid Value for optimizer_search_depth");
    exit(-1);
  }
  global_system_variables.optimizer_search_depth= in_optimizer_search_depth;
}

static void check_limits_pbs(uint64_t in_preload_buff_size)
{
  global_system_variables.preload_buff_size= (32*1024L);
  if (in_preload_buff_size < 1024 || in_preload_buff_size > 1024*1024*1024L)
  {
    cout << _("Error: Invalid Value for preload_buff_size");
    exit(-1);
  }
  global_system_variables.preload_buff_size= in_preload_buff_size;
}

static void check_limits_qabs(uint32_t in_query_alloc_block_size)
{
  global_system_variables.query_alloc_block_size= QUERY_ALLOC_BLOCK_SIZE;
  if (in_query_alloc_block_size < 1024)
  {
    cout << _("Error: Invalid Value for query_alloc_block_size");
    exit(-1);
  }
  in_query_alloc_block_size-= in_query_alloc_block_size % 1024;
  global_system_variables.query_alloc_block_size= in_query_alloc_block_size;
}

static void check_limits_qps(uint32_t in_query_prealloc_size)
{
  global_system_variables.query_prealloc_size= QUERY_ALLOC_PREALLOC_SIZE;
  if (in_query_prealloc_size < QUERY_ALLOC_PREALLOC_SIZE)
  {
    cout << _("Error: Invalid Value for query_prealloc_size");
    exit(-1);
  }
  in_query_prealloc_size-= in_query_prealloc_size % 1024;
  global_system_variables.query_prealloc_size= in_query_prealloc_size;
}

static void check_limits_rabs(size_t in_range_alloc_block_size)
{
  global_system_variables.range_alloc_block_size= RANGE_ALLOC_BLOCK_SIZE;
  if (in_range_alloc_block_size < RANGE_ALLOC_BLOCK_SIZE)
  {
    cout << _("Error: Invalid Value for range_alloc_block_size");
    exit(-1);
  }
  in_range_alloc_block_size-= in_range_alloc_block_size % 1024;
  global_system_variables.range_alloc_block_size= in_range_alloc_block_size;
}

static void check_limits_read_buffer_size(int32_t in_read_buff_size)
{
  global_system_variables.read_buff_size= (128*1024L);
  if (in_read_buff_size < IO_SIZE*2 || in_read_buff_size > INT32_MAX)
  {
    cout << _("Error: Invalid Value for read_buff_size");
    exit(-1);
  }
  in_read_buff_size-= in_read_buff_size % IO_SIZE;
  global_system_variables.read_buff_size= in_read_buff_size;
}

static void check_limits_read_rnd_buffer_size(uint32_t in_read_rnd_buff_size)
{
  global_system_variables.read_rnd_buff_size= (256*1024L);
  if (in_read_rnd_buff_size < 64 || in_read_rnd_buff_size > UINT32_MAX)
  {
    cout << _("Error: Invalid Value for read_rnd_buff_size");
    exit(-1);
  }
  global_system_variables.read_rnd_buff_size= in_read_rnd_buff_size;
}

static void check_limits_sort_buffer_size(size_t in_sortbuff_size)
{
  global_system_variables.sortbuff_size= MAX_SORT_MEMORY;
  if ((uint32_t)in_sortbuff_size < MIN_SORT_MEMORY)
  {
    cout << _("Error: Invalid Value for sort_buff_size");
    exit(-1);
  }
  global_system_variables.sortbuff_size= in_sortbuff_size;
}

static void check_limits_tdc(uint32_t in_table_def_size)
{
  table_def_size= 128;
  if (in_table_def_size < 1 || in_table_def_size > 512*1024L)
  {
    cout << _("Error: Invalid Value for table_def_size");
    exit(-1);
  }
  table_def_size= in_table_def_size;
}

static void check_limits_toc(uint32_t in_table_cache_size)
{
  table_cache_size= TABLE_OPEN_CACHE_DEFAULT;
  if (in_table_cache_size < TABLE_OPEN_CACHE_MIN || in_table_cache_size > 512*1024L)
  {
    cout << _("Error: Invalid Value for table_cache_size");
    exit(-1);
  }
  table_cache_size= in_table_cache_size;
}

static void check_limits_tlwt(uint64_t in_table_lock_wait_timeout)
{
  table_lock_wait_timeout= 50;
  if (in_table_lock_wait_timeout < 1 || in_table_lock_wait_timeout > 1024*1024*1024)
  {
    cout << _("Error: Invalid Value for table_lock_wait_timeout");
    exit(-1);
  }
  table_lock_wait_timeout= in_table_lock_wait_timeout;
}

static void check_limits_thread_stack(uint32_t in_my_thread_stack_size)
{
  my_thread_stack_size= in_my_thread_stack_size - (in_my_thread_stack_size % 1024);
}

static void check_limits_tmp_table_size(uint64_t in_tmp_table_size)
{
  global_system_variables.tmp_table_size= 16*1024*1024L;
  if (in_tmp_table_size < 1024 || in_tmp_table_size > MAX_MEM_TABLE_SIZE)
  {
    cout << _("Error: Invalid Value for table_lock_wait_timeout");
    exit(-1);
  }
  global_system_variables.tmp_table_size= in_tmp_table_size;
}

static void check_limits_transaction_message_threshold(size_t in_transaction_message_threshold)
{
  transaction_message_threshold= 1024*1024;
  if ((int64_t) in_transaction_message_threshold < 128*1024 || (int64_t)in_transaction_message_threshold > 1024*1024)
  {
    cout << _("Error: Invalid Value for transaction_message_threshold valid values are between 131072 - 1048576 bytes");
    exit(-1);
  }
  transaction_message_threshold= in_transaction_message_threshold;
}

static void process_defaults_files()
{
	BOOST_FOREACH(vector<string>::reference iter, defaults_file_list)
  {
    fs::path file_location= iter;

    ifstream input_defaults_file(file_location.file_string().c_str());

    po::parsed_options file_parsed= dpo::parse_config_file(input_defaults_file, full_options, true);
    vector<string> file_unknown= po::collect_unrecognized(file_parsed.options, po::include_positional);

    for (vector<string>::iterator it= file_unknown.begin(); it != file_unknown.end(); ++it)
    {
      string new_unknown_opt("--" + *it);
      ++it;
      if (it == file_unknown.end())
				break;
      if (*it != "true")
        new_unknown_opt += "=" + *it;
      unknown_options.push_back(new_unknown_opt);
    }
    store(file_parsed, vm);
  }
}

static void compose_defaults_file_list(const vector<string>& in_options)
{
	BOOST_FOREACH(const string& it, in_options)
  {
    fs::path p(it);
    if (fs::is_regular_file(p))
      defaults_file_list.push_back(it);
    else
    {
      errmsg_printf(error::ERROR, _("Defaults file '%s' not found\n"), it.c_str());
      unireg_abort(1);
    }
  }
}

int init_basic_variables(int argc, char **argv)
{
  umask(((~internal::my_umask) & 0666));
  decimal_zero.set_zero(); // set decimal_zero constant;
  tzset();			// Set tzname

  time_t curr_time= time(NULL);
  if (curr_time == (time_t)-1)
    return 1;

  max_system_variables.pseudo_thread_id= UINT32_MAX;
  server_start_time= flush_status_time= curr_time;

  drizzle_init_variables();

  find_plugin_dir(argv[0]);

  char ret_hostname[FN_REFLEN];
  if (gethostname(ret_hostname,sizeof(ret_hostname)) < 0)
  {
    errmsg_printf(error::WARN, _("gethostname failed, using '%s' as hostname"), getServerHostname().c_str());
    pid_file= "drizzle";
  }
  else
  {
    g_hostname= ret_hostname;
    pid_file= getServerHostname();
  }
  pid_file.replace_extension(".pid");

  system_config_dir /= "drizzle";

  config_options.add_options()
  ("help,?", po::value<bool>(&opt_help)->default_value(false)->zero_tokens(),
  _("Display this help and exit."))
  ("daemon,d", po::value<bool>(&opt_daemon)->default_value(false)->zero_tokens(),
  _("Run as a daemon."))
  ("no-defaults", po::value<bool>()->default_value(false)->zero_tokens(),
  _("Configuration file defaults are not used if no-defaults is set"))
  ("defaults-file", po::value<vector<string> >()->composing()->notifier(&compose_defaults_file_list),
  _("Configuration file to use"))
  ("config-dir", po::value<fs::path>(&system_config_dir),
  _("Base location for config files"))
  ("plugin-dir", po::value<fs::path>(&plugin_dir)->notifier(&notify_plugin_dir),
  _("Directory for plugins."))
  ;

  plugin_load_options.add_options()
  ("plugin-add", po::value<vector<string> >()->composing()->notifier(&compose_plugin_add),
  _("Optional comma separated list of plugins to load at startup in addition "
     "to the default list of plugins. "
     "[for example: --plugin_add=crc32,logger_gearman]"))
  ("plugin-remove", po::value<vector<string> >()->composing()->notifier(&compose_plugin_remove),
  _("Optional comma separated list of plugins to not load at startup. Effectively "
     "removes a plugin from the list of plugins to be loaded. "
     "[for example: --plugin_remove=crc32,logger_gearman]"))
  ("plugin-load", po::value<string>()->notifier(&notify_plugin_load)->default_value(PANDORA_PLUGIN_LIST),
  _("Optional comma separated list of plugins to load at starup instead of "
     "the default plugin load list. "
     "[for example: --plugin_load=crc32,logger_gearman]"))
  ;

  long_options.add_options()
  ("auto-increment-increment", po::value<uint64_t>(&global_system_variables.auto_increment_increment)->default_value(1)->notifier(&check_limits_aii),
  _("Auto-increment columns are incremented by this"))
  ("auto-increment-offset", po::value<uint64_t>(&global_system_variables.auto_increment_offset)->default_value(1)->notifier(&check_limits_aio),
  _("Offset added to Auto-increment columns. Used when auto-increment-increment != 1"))
  ("basedir,b", po::value<fs::path>(&basedir),
  _("Path to installation directory. All paths are usually resolved "
     "relative to this."))
  ("chroot,r", po::value<string>(),
  _("Chroot drizzled daemon during startup."))
  ("collation-server", po::value<string>(),
  _("Set the default collation."))
  ("completion-type", po::value<uint32_t>(&global_system_variables.completion_type)->default_value(0)->notifier(&check_limits_completion_type),
  _("Default completion type."))
  ("core-file",  _("Write core on errors."))
  ("datadir", po::value<fs::path>(&getMutableDataHome()),
  _("Path to the database root."))
  ("default-storage-engine", po::value<string>(),
  _("Set the default storage engine for tables."))
  ("default-time-zone", po::value<string>(),
  _("Set the default time zone."))
  ("exit-info,T", po::value<long>(),
  _("Used for debugging;  Use at your own risk!"))
  ("gdb", po::value<bool>(&opt_debugging)->default_value(false)->zero_tokens(),
  _("Set up signals usable for debugging"))
  ("lc-time-name", po::value<string>(),
  _("Set the language used for the month names and the days of the week."))
  ("log-warnings,W", po::value<bool>(&global_system_variables.log_warnings)->default_value(false)->zero_tokens(),
  _("Log some not critical warnings to the log file."))
  ("pid-file", po::value<fs::path>(&pid_file),
  _("Pid file used by drizzled."))
  ("port-open-timeout", po::value<uint32_t>(&drizzled_bind_timeout)->default_value(0),
  _("Maximum time in seconds to wait for the port to become free. "))
  ("replicate-query", po::value<bool>(&global_system_variables.replicate_query)->default_value(false)->zero_tokens(),
  _("Include the SQL query in replicated protobuf messages."))
  ("secure-file-priv", po::value<fs::path>(&secure_file_priv)->notifier(expand_secure_file_priv),
  _("Limit LOAD DATA, SELECT ... OUTFILE, and LOAD_FILE() to files "
     "within specified directory"))
  ("server-id", po::value<uint32_t>(&server_id)->default_value(0),
  _("Uniquely identifies the server instance in the community of "
     "replication partners."))
  ("skip-stack-trace",
  _("Don't print a stack trace on failure."))
  ("symbolic-links,s", po::value<bool>(&internal::my_use_symdir)->default_value(IF_PURIFY(false,true))->zero_tokens(),
  _("Enable symbolic link support."))
  ("timed-mutexes", po::value<bool>(&internal::timed_mutexes)->default_value(false)->zero_tokens(),
  _("Specify whether to time mutexes (only InnoDB mutexes are currently "
     "supported)"))
  ("tmpdir,t", po::value<string>(),
  _("Path for temporary files."))
  ("transaction-isolation", po::value<string>(),
  _("Default transaction isolation level."))
  ("transaction-message-threshold", po::value<size_t>(&transaction_message_threshold)->default_value(1024*1024)->notifier(&check_limits_transaction_message_threshold),
  _("Max message size written to transaction log, valid values 131072 - 1048576 bytes."))
  ("user,u", po::value<string>(),
  _("Run drizzled daemon as user."))
  ("version,V",
  _("Output version information and exit."))
  ("back-log", po::value<back_log_constraints>(&back_log),
  _("The number of outstanding connection requests Drizzle can have. This "
     "comes into play when the main Drizzle thread gets very many connection "
     "requests in a very short time."))
  ("bulk-insert-buffer-size",
  po::value<uint64_t>(&global_system_variables.bulk_insert_buff_size)->default_value(8192*1024),
  _("Size of tree cache used in bulk insert optimization. Note that this is "
     "a limit per thread!"))
  ("div-precision-increment",  po::value<uint32_t>(&global_system_variables.div_precincrement)->default_value(4)->notifier(&check_limits_dpi),
  _("Precision of the result of '/' operator will be increased on that "
     "value."))
  ("group-concat-max-len", po::value<uint64_t>(&global_system_variables.group_concat_max_len)->default_value(1024)->notifier(&check_limits_gcml),
  _("The maximum length of the result of function  group_concat."))
  ("join-buffer-size", po::value<uint64_t>(&global_system_variables.join_buff_size)->default_value(128*1024L)->notifier(&check_limits_join_buffer_size),
  _("The size of the buffer that is used for full joins."))
  ("join-heap-threshold",
  po::value<uint64_t>()->default_value(0),
  _("A global cap on the amount of memory that can be allocated by session join buffers (0 means unlimited)"))
  ("max-allowed-packet", po::value<uint32_t>(&global_system_variables.max_allowed_packet)->default_value(64*1024*1024L)->notifier(&check_limits_map),
  _("Max packetlength to send/receive from to server."))
  ("max-error-count", po::value<uint64_t>(&global_system_variables.max_error_count)->default_value(DEFAULT_ERROR_COUNT)->notifier(&check_limits_max_err_cnt),
  _("Max number of errors/warnings to store for a statement."))
  ("max-heap-table-size", po::value<uint64_t>(&global_system_variables.max_heap_table_size)->default_value(16*1024*1024L)->notifier(&check_limits_mhts),
  _("Don't allow creation of heap tables bigger than this."))
  ("max-join-size", po::value<ha_rows>(&global_system_variables.max_join_size)->default_value(INT32_MAX)->notifier(&check_limits_max_join_size),
  _("Joins that are probably going to read more than max_join_size records "
     "return an error."))
  ("max-length-for-sort-data", po::value<uint64_t>(&global_system_variables.max_length_for_sort_data)->default_value(1024)->notifier(&check_limits_mlfsd),
  _("Max number of bytes in sorted records."))
  ("max-seeks-for-key", po::value<uint64_t>(&global_system_variables.max_seeks_for_key)->default_value(ULONG_MAX)->notifier(&check_limits_msfk),
  _("Limit assumed max number of seeks when looking up rows based on a key"))
  ("max-sort-length", po::value<size_t>(&global_system_variables.max_sort_length)->default_value(1024)->notifier(&check_limits_max_sort_length),
  _("The number of bytes to use when sorting BLOB or TEXT values "
     "(only the first max_sort_length bytes of each value are used; the "
     "rest are ignored)."))
  ("max-write-lock-count", po::value<uint64_t>(&max_write_lock_count)->default_value(UINT64_MAX),
  _("After this many write locks, allow some read locks to run in between."))
  ("min-examined-row-limit", po::value<uint64_t>(&global_system_variables.min_examined_row_limit)->default_value(0)->notifier(&check_limits_merl),
  _("Don't log queries which examine less than min_examined_row_limit "
     "rows to file."))
  ("disable-optimizer-prune",
  _("Do not apply any heuristic(s) during query optimization to prune, "
     "thus perform an exhaustive search from the optimizer search space."))
  ("optimizer-search-depth", po::value<uint32_t>(&global_system_variables.optimizer_search_depth)->default_value(0)->notifier(&check_limits_osd),
  _("Maximum depth of search performed by the query optimizer. Values "
     "larger than the number of relations in a query result in better query "
     "plans, but take longer to compile a query. Smaller values than the "
     "number of tables in a relation result in faster optimization, but may "
     "produce very bad query plans. If set to 0, the system will "
     "automatically pick a reasonable value; if set to MAX_TABLES+2, the "
     "optimizer will switch to the original find_best (used for "
     "testing/comparison)."))
  ("preload-buffer-size", po::value<uint64_t>(&global_system_variables.preload_buff_size)->default_value(32*1024L)->notifier(&check_limits_pbs),
  _("The size of the buffer that is allocated when preloading indexes"))
  ("query-alloc-block-size",
  po::value<uint32_t>(&global_system_variables.query_alloc_block_size)->default_value(QUERY_ALLOC_BLOCK_SIZE)->notifier(&check_limits_qabs),
  _("Allocation block size for query parsing and execution"))
  ("query-prealloc-size",
  po::value<uint32_t>(&global_system_variables.query_prealloc_size)->default_value(QUERY_ALLOC_PREALLOC_SIZE)->notifier(&check_limits_qps),
  _("Persistent buffer for query parsing and execution"))
  ("range-alloc-block-size",
  po::value<size_t>(&global_system_variables.range_alloc_block_size)->default_value(RANGE_ALLOC_BLOCK_SIZE)->notifier(&check_limits_rabs),
  _("Allocation block size for storing ranges during optimization"))
  ("read-buffer-size",
  po::value<uint32_t>(&global_system_variables.read_buff_size)->default_value(128*1024L)->notifier(&check_limits_read_buffer_size),
  _("Each thread that does a sequential scan allocates a buffer of this "
      "size for each table it scans. If you do many sequential scans, you may "
      "want to increase this value."))
  ("read-buffer-threshold",
  po::value<uint64_t>()->default_value(0),
  _("A global cap on the size of read-buffer-size (0 means unlimited)"))
  ("read-rnd-buffer-size",
  po::value<uint32_t>(&global_system_variables.read_rnd_buff_size)->default_value(256*1024L)->notifier(&check_limits_read_rnd_buffer_size),
  _("When reading rows in sorted order after a sort, the rows are read "
     "through this buffer to avoid a disk seeks. If not set, then it's set "
     "to the value of record_buffer."))
  ("read-rnd-threshold",
  po::value<uint64_t>()->default_value(0),
  _("A global cap on the size of read-rnd-buffer-size (0 means unlimited)"))
  ("scheduler", po::value<string>(),
  _("Select scheduler to be used (by default multi-thread)."))
  ("sort-buffer-size",
  po::value<size_t>(&global_system_variables.sortbuff_size)->default_value(MAX_SORT_MEMORY)->notifier(&check_limits_sort_buffer_size),
  _("Each thread that needs to do a sort allocates a buffer of this size."))
  ("sort-heap-threshold",
  po::value<uint64_t>()->default_value(0),
  _("A global cap on the amount of memory that can be allocated by session sort buffers (0 means unlimited)"))
  ("table-definition-cache", po::value<size_t>(&table_def_size)->default_value(128)->notifier(&check_limits_tdc),
  _("The number of cached table definitions."))
  ("table-open-cache", po::value<uint64_t>(&table_cache_size)->default_value(TABLE_OPEN_CACHE_DEFAULT)->notifier(&check_limits_toc),
  _("The number of cached open tables."))
  ("table-lock-wait-timeout", po::value<uint64_t>(&table_lock_wait_timeout)->default_value(50)->notifier(&check_limits_tlwt),
  _("Timeout in seconds to wait for a table level lock before returning an "
     "error. Used only if the connection has active cursors."))
  ("thread-stack", po::value<size_t>(&my_thread_stack_size)->default_value(DEFAULT_THREAD_STACK)->notifier(&check_limits_thread_stack),
  _("The stack size for each thread."))
  ("tmp-table-size",
  po::value<uint64_t>(&global_system_variables.tmp_table_size)->default_value(16*1024*1024L)->notifier(&check_limits_tmp_table_size),
  _("If an internal in-memory temporary table exceeds this size, Drizzle will"
     " automatically convert it to an on-disk MyISAM table."))
  ("verbose", po::value<std::string>()->default_value(error::verbose_string())->notifier(&error::check_verbosity),
  _("The verbosity of messages from drizzled.  Possible values are INSPECT, INFO, WARN or ERROR"))
  ;

  full_options.add(long_options);
  full_options.add(plugin_load_options);

  initial_options.add(config_options);
  initial_options.add(plugin_load_options);

  int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;
  /* Get options about where config files and the like are */
  po::parsed_options parsed= po::command_line_parser(argc, argv).style(style).
    options(initial_options).allow_unregistered().run();
  unknown_options=
    po::collect_unrecognized(parsed.options, po::include_positional);

  try
  {
    po::store(parsed, vm);
  }
  catch (std::exception&)
  {
    errmsg_printf(error::ERROR, _("Duplicate entry for command line option\n"));
    unireg_abort(1);
  }

  if (not vm["no-defaults"].as<bool>())
  {
    fs::path system_config_file_drizzle(system_config_dir);
    system_config_file_drizzle /= "drizzled.cnf";
    defaults_file_list.insert(defaults_file_list.begin(), system_config_file_drizzle.file_string());

    fs::path config_conf_d_location(system_config_dir);
    config_conf_d_location /= "conf.d";

    CachedDirectory config_conf_d(config_conf_d_location.file_string());
    if (not config_conf_d.fail())
    {
			BOOST_FOREACH(CachedDirectory::Entries::const_reference iter, config_conf_d.getEntries())
      {
        string file_entry(iter->filename);
        if (not file_entry.empty() && file_entry != "." && file_entry != "..")
          defaults_file_list.push_back((config_conf_d_location / file_entry).file_string());
      }
    }
  }

  /* TODO: here is where we should add a process_env_vars */

  /* We need a notify here so that plugin_init will work properly */
  try
  {
    po::notify(vm);
  }
  catch (po::validation_error &err)
  {
    errmsg_printf(error::ERROR, _("%s: %s.\nUse --help to get a list of available options\n"), internal::my_progname, err.what());
    unireg_abort(1);
  }

  process_defaults_files();

  /* Process with notify a second time because a config file may contain
     plugin loader options */

  try
  {
    po::notify(vm);
  }
  catch (po::validation_error &err)
  {
    errmsg_printf(error::ERROR, _("%s: %s.\nUse --help to get a list of available options\n"), internal::my_progname, err.what());
    unireg_abort(1);
  }

  return 0;
}

int init_remaining_variables(module::Registry &plugins)
{
  int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;

  current_pid= getpid();		/* Save for later ref */

  /* At this point, we've read all the options we need to read from files and
     collected most of them into unknown options - now let's load everything
  */

  if (plugin_init(plugins, plugin_options))
  {
    errmsg_printf(error::ERROR, _("Failed to initialize plugins\n"));
    unireg_abort(1);
  }

  full_options.add(plugin_options);

  vector<string> final_unknown_options;
  try
  {
    po::parsed_options final_parsed=
      po::command_line_parser(unknown_options).style(style).
      options(full_options).extra_parser(dpo::parse_size_arg).run();

    final_unknown_options=
      po::collect_unrecognized(final_parsed.options, po::include_positional);

    po::store(final_parsed, vm);

  }
  catch (po::validation_error &err)
  {
    errmsg_printf(error::ERROR,
                  _("%s: %s.\n"
                    "Use --help to get a list of available options\n"),
                  internal::my_progname, err.what());
    unireg_abort(1);
  }
  catch (po::invalid_command_line_syntax &err)
  {
    errmsg_printf(error::ERROR,
                  _("%s: %s.\n"
                    "Use --help to get a list of available options\n"),
                  internal::my_progname, err.what());
    unireg_abort(1);
  }
  catch (po::unknown_option &err)
  {
    errmsg_printf(error::ERROR,
                  _("%s\nUse --help to get a list of available options\n"),
                  err.what());
    unireg_abort(1);
  }

  try
  {
    po::notify(vm);
  }
  catch (po::validation_error &err)
  {
    errmsg_printf(error::ERROR,
                  _("%s: %s.\n"
                    "Use --help to get a list of available options\n"),
                  internal::my_progname, err.what());
    unireg_abort(1);
  }

  get_options();

  /* Inverted Booleans */

  global_system_variables.optimizer_prune_level= not vm.count("disable-optimizer-prune");

  if (! vm["help"].as<bool>())
  {
    if ((user_info= check_user(drizzled_user)))
    {
      set_user(drizzled_user, user_info);
    }
  }

  fix_paths();

  init_time();				/* Init time-functions (read zone) */

  item_create_init();
  if (sys_var_init())
    return 1;
  /* Creates static regex matching for temporal values */
  if (! init_temporal_formats())
    return 1;

  if (!(default_charset_info=
        get_charset_by_csname(default_character_set_name, MY_CS_PRIMARY)))
  {
    errmsg_printf(error::ERROR, _("Error getting default charset"));
    return 1;                           // Eof of the list
  }

  if (vm.count("scheduler"))
    opt_scheduler= vm["scheduler"].as<string>().c_str();

  if (default_collation_name)
  {
    const charset_info_st * const default_collation= get_charset_by_name(default_collation_name);
    if (not default_collation)
    {
      errmsg_printf(error::ERROR, _(ER(ER_UNKNOWN_COLLATION)), default_collation_name);
      return 1;
    }
    if (not my_charset_same(default_charset_info, default_collation))
    {
      errmsg_printf(error::ERROR, _(ER(ER_COLLATION_CHARSET_MISMATCH)),
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
  {
    errmsg_printf(error::ERROR, _("Error setting collation"));
    return 1;
  }
  global_system_variables.character_set_filesystem= character_set_filesystem;

  if (!(my_default_lc_time_names=
        my_locale_by_name(lc_time_names_name)))
  {
    errmsg_printf(error::ERROR, _("Unknown locale: '%s'"), lc_time_names_name);
    return 1;
  }
  global_system_variables.lc_time_names= my_default_lc_time_names;

  /* Reset table_alias_charset */
  table_alias_charset= files_charset_info;

  return 0;
}


void init_server_components(module::Registry &plugins)
{
  /*
    We need to call each of these following functions to ensure that
    all things are initialized so that unireg_abort() doesn't fail
  */

  // Resize the definition Cache at startup
  table::Cache::rehash(table_def_size);
  definition::Cache::rehash(table_def_size);
  message::Cache::singleton().rehash(table_def_size);

  setup_fpu();

  /* Allow storage engine to give real error messages */
  ha_init_errors();

  if (opt_help)
    unireg_abort(0);

  if (plugin_finalize(plugins))
  {
    unireg_abort(1);
  }

  if (plugin::Scheduler::setPlugin(opt_scheduler))
  {
      errmsg_printf(error::ERROR, _("No scheduler found, cannot continue!\n"));
      unireg_abort(1);
  }

  /*
    This is entirely for legacy. We will create a new "disk based" engine and a
    "memory" engine which will be configurable longterm.
  */
  myisam_engine= plugin::StorageEngine::findByName("MyISAM");
  heap_engine= plugin::StorageEngine::findByName("MEMORY");

  /*
    Check that the default storage engine is actually available.
  */
  if (default_storage_engine_str)
  {
    plugin::StorageEngine *engine= plugin::StorageEngine::findByName(default_storage_engine_str);
    if (engine == NULL)
    {
      errmsg_printf(error::ERROR, _("Unknown/unsupported storage engine: %s\n"), default_storage_engine_str);
      unireg_abort(1);
    }
    global_system_variables.storage_engine= engine;
  }

  if (plugin::XaResourceManager::recoverAllXids())
  {
    /* This function alredy generates error messages */
    unireg_abort(1);
  }

  init_update_queries();
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
  OPT_MAX_ALLOWED_PACKET,
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
  OPT_MIN_EXAMINED_ROW_LIMIT,
  OPT_PRINT_DEFAULTS
};


struct option my_long_options[] =
{

  {"help", '?', N_("Display this help and exit."),
   (char**) &opt_help, NULL, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"daemon", 'd', N_("Run as daemon."),
   (char**) &opt_daemon, NULL, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"auto-increment-increment", OPT_AUTO_INCREMENT,
   N_("Auto-increment columns are incremented by this"),
   (char**) &global_system_variables.auto_increment_increment,
   NULL, 0, GET_ULL,
   OPT_ARG, 1, 1, INT64_MAX, 0, 1, 0 },
  {"auto-increment-offset", OPT_AUTO_INCREMENT_OFFSET,
   N_("Offset added to Auto-increment columns. Used when "
      "auto-increment-increment != 1"),
   (char**) &global_system_variables.auto_increment_offset,
   NULL, 0, GET_ULL, OPT_ARG,
   1, 1, INT64_MAX, 0, 1, 0 },
  {"basedir", 'b',
   N_("Path to installation directory. All paths are usually resolved "
      "relative to this."),
   NULL, NULL, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"chroot", 'r',
   N_("Chroot drizzled daemon during startup."),
   (char**) &drizzled_chroot, NULL, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"collation-server", OPT_DEFAULT_COLLATION,
   N_("Set the default collation."),
   (char**) &default_collation_name, NULL,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  {"completion-type", OPT_COMPLETION_TYPE,
   N_("Default completion type."),
   (char**) &global_system_variables.completion_type,
   NULL, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, 2, 0, 1, 0},
  {"core-file", OPT_WANT_CORE,
   N_("Write core on errors."),
   0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"datadir", 'h',
   N_("Path to the database root."),
   NULL, NULL, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  /* See how it's handled in get_one_option() */
  {"exit-info", 'T',
   N_("Used for debugging;  Use at your own risk!"),
   0, 0, 0, GET_LONG, OPT_ARG, 0, 0, 0, 0, 0, 0},
  /* We must always support the next option to make scripts like mysqltest
     easier to do */
  {"gdb", OPT_DEBUGGING,
   N_("Set up signals usable for debugging"),
   (char**) &opt_debugging, NULL,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"log-warnings", 'W',
   N_("Log some not critical warnings to the log file."),
   (char**) &global_system_variables.log_warnings,
   NULL, 0, GET_BOOL, OPT_ARG, 1, 0, 0,
   0, 0, 0},
  {"pid-file", OPT_PID_FILE,
   N_("Pid file used by drizzled."),
   NULL, NULL, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port-open-timeout", OPT_PORT_OPEN_TIMEOUT,
   N_("Maximum time in seconds to wait for the port to become free. "
      "(Default: no wait)"),
   (char**) &drizzled_bind_timeout,
   NULL, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"secure-file-priv", OPT_SECURE_FILE_PRIV,
   N_("Limit LOAD DATA, SELECT ... OUTFILE, and LOAD_FILE() to files "
      "within specified directory"),
   NULL, NULL, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-id",	OPT_SERVER_ID,
   N_("Uniquely identifies the server instance in the community of "
      "replication partners."),
   (char**) &server_id, NULL, 0, GET_UINT32, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"skip-stack-trace", OPT_SKIP_STACK_TRACE,
   N_("Don't print a stack trace on failure."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"symbolic-links", 's',
   N_("Enable symbolic link support."),
   (char**) &internal::my_use_symdir, NULL, 0, GET_BOOL, NO_ARG,
   /*
     The system call realpath() produces warnings under valgrind and
     purify. These are not suppressed: instead we disable symlinks
     option if compiled with valgrind support.
   */
   IF_PURIFY(0,1), 0, 0, 0, 0, 0},
  {"timed_mutexes", OPT_TIMED_MUTEXES,
   N_("Specify whether to time mutexes (only InnoDB mutexes are currently "
      "supported)"),
   (char**) &internal::timed_mutexes, NULL, 0, GET_BOOL, NO_ARG, 0,
    0, 0, 0, 0, 0},
  {"transaction-isolation", OPT_TX_ISOLATION,
   N_("Default transaction isolation level."),
   0, 0, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"user", 'u',
   N_("Run drizzled daemon as user."),
   0, 0, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"back_log", OPT_BACK_LOG,
   N_("The number of outstanding connection requests Drizzle can have. This "
      "comes into play when the main Drizzle thread gets very many connection "
      "requests in a very short time."),
    (char**) &back_log, NULL, 0, GET_UINT,
    REQUIRED_ARG, 50, 1, 65535, 0, 1, 0 },
  { "bulk_insert_buffer_size", OPT_BULK_INSERT_BUFFER_SIZE,
    N_("Size of tree cache used in bulk insert optimization. Note that this is "
       "a limit per thread!"),
    (char**) &global_system_variables.bulk_insert_buff_size,
    NULL,
    0, GET_ULL, REQUIRED_ARG, 8192*1024, 0, ULONG_MAX, 0, 1, 0},
  { "div_precision_increment", OPT_DIV_PRECINCREMENT,
   N_("Precision of the result of '/' operator will be increased on that "
      "value."),
   (char**) &global_system_variables.div_precincrement,
   NULL, 0, GET_UINT,
   REQUIRED_ARG, 4, 0, DECIMAL_MAX_SCALE, 0, 0, 0},
  { "join_buffer_size", OPT_JOIN_BUFF_SIZE,
    N_("The size of the buffer that is used for full joins."),
   (char**) &global_system_variables.join_buff_size,
   NULL, 0, GET_UINT64,
   REQUIRED_ARG, 128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, ULONG_MAX,
   MALLOC_OVERHEAD, IO_SIZE, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET,
   N_("Max packetlength to send/receive from to server."),
   (char**) &global_system_variables.max_allowed_packet,
   NULL, 0, GET_UINT32,
   REQUIRED_ARG, 64*1024*1024L, 1024, 1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"max_heap_table_size", OPT_MAX_HEP_TABLE_SIZE,
   N_("Don't allow creation of heap tables bigger than this."),
   (char**) &global_system_variables.max_heap_table_size,
   NULL, 0, GET_ULL,
   REQUIRED_ARG, 16*1024*1024L, 16384, (int64_t)MAX_MEM_TABLE_SIZE,
   MALLOC_OVERHEAD, 1024, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   N_("Joins that are probably going to read more than max_join_size records "
      "return an error."),
   (char**) &global_system_variables.max_join_size,
   NULL, 0, GET_HA_ROWS, REQUIRED_ARG,
   INT32_MAX, 1, INT32_MAX, 0, 1, 0},
  {"max_length_for_sort_data", OPT_MAX_LENGTH_FOR_SORT_DATA,
   N_("Max number of bytes in sorted records."),
   (char**) &global_system_variables.max_length_for_sort_data,
   NULL, 0, GET_ULL,
   REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  { "max_seeks_for_key", OPT_MAX_SEEKS_FOR_KEY,
    N_("Limit assumed max number of seeks when looking up rows based on a key"),
    (char**) &global_system_variables.max_seeks_for_key,
    NULL, 0, GET_UINT64,
    REQUIRED_ARG, ULONG_MAX, 1, ULONG_MAX, 0, 1, 0 },
  {"max_sort_length", OPT_MAX_SORT_LENGTH,
   N_("The number of bytes to use when sorting BLOB or TEXT values "
      "(only the first max_sort_length bytes of each value are used; the "
      "rest are ignored)."),
   (char**) &global_system_variables.max_sort_length,
   NULL, 0, GET_SIZE,
   REQUIRED_ARG, 1024, 4, 8192*1024L, 0, 1, 0},
  {"max_write_lock_count", OPT_MAX_WRITE_LOCK_COUNT,
   N_("After this many write locks, allow some read locks to run in between."),
   (char**) &max_write_lock_count, NULL, 0, GET_ULL,
   REQUIRED_ARG, ULONG_MAX, 1, ULONG_MAX, 0, 1, 0},
  {"min_examined_row_limit", OPT_MIN_EXAMINED_ROW_LIMIT,
   N_("Don't log queries which examine less than min_examined_row_limit "
      "rows to file."),
   (char**) &global_system_variables.min_examined_row_limit,
   NULL, 0, GET_ULL,
   REQUIRED_ARG, 0, 0, ULONG_MAX, 0, 1L, 0},
  {"optimizer_prune_level", OPT_OPTIMIZER_PRUNE_LEVEL,
    N_("Controls the heuristic(s) applied during query optimization to prune "
       "less-promising partial plans from the optimizer search space. Meaning: "
       "false - do not apply any heuristic, thus perform exhaustive search; "
       "true - prune plans based on number of retrieved rows."),
    (char**) &global_system_variables.optimizer_prune_level,
    NULL,
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
   NULL,
   0, GET_UINT, OPT_ARG, 0, 0, MAX_TABLES+2, 0, 1, 0},
  {"plugin_dir", OPT_PLUGIN_DIR,
   N_("Directory for plugins."),
   NULL, NULL, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin_add", OPT_PLUGIN_ADD,
   N_("Optional comma separated list of plugins to load at startup in addition "
      "to the default list of plugins. "
      "[for example: --plugin_add=crc32,logger_gearman]"),
   NULL, NULL, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin_remove", OPT_PLUGIN_ADD,
   N_("Optional comma separated list of plugins to not load at startup. Effectively "
      "removes a plugin from the list of plugins to be loaded. "
      "[for example: --plugin_remove=crc32,logger_gearman]"),
   NULL, NULL, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin_load", OPT_PLUGIN_LOAD,
   N_("Optional comma separated list of plugins to load at starup instead of "
      "the default plugin load list. "
      "[for example: --plugin_load=crc32,logger_gearman]"),
   NULL, NULL, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"preload_buffer_size", OPT_PRELOAD_BUFFER_SIZE,
   N_("The size of the buffer that is allocated when preloading indexes"),
   (char**) &global_system_variables.preload_buff_size,
   NULL, 0, GET_ULL,
   REQUIRED_ARG, 32*1024L, 1024, 1024*1024*1024L, 0, 1, 0},
  {"query_alloc_block_size", OPT_QUERY_ALLOC_BLOCK_SIZE,
   N_("Allocation block size for query parsing and execution"),
   (char**) &global_system_variables.query_alloc_block_size,
   NULL, 0, GET_UINT,
   REQUIRED_ARG, QUERY_ALLOC_BLOCK_SIZE, 1024, ULONG_MAX, 0, 1024, 0},
  {"query_prealloc_size", OPT_QUERY_PREALLOC_SIZE,
   N_("Persistent buffer for query parsing and execution"),
   (char**) &global_system_variables.query_prealloc_size,
   NULL, 0, GET_UINT,
   REQUIRED_ARG, QUERY_ALLOC_PREALLOC_SIZE, QUERY_ALLOC_PREALLOC_SIZE,
   ULONG_MAX, 0, 1024, 0},
  {"range_alloc_block_size", OPT_RANGE_ALLOC_BLOCK_SIZE,
   N_("Allocation block size for storing ranges during optimization"),
   (char**) &global_system_variables.range_alloc_block_size,
   NULL, 0, GET_SIZE,
   REQUIRED_ARG, RANGE_ALLOC_BLOCK_SIZE, RANGE_ALLOC_BLOCK_SIZE, (int64_t)SIZE_MAX,
   0, 1024, 0},
  {"read_buffer_size", OPT_RECORD_BUFFER,
    N_("Each thread that does a sequential scan allocates a buffer of this "
       "size for each table it scans. If you do many sequential scans, you may "
       "want to increase this value."),
    (char**) &global_system_variables.read_buff_size,
    NULL,0, GET_UINT, REQUIRED_ARG,
    128*1024L, IO_SIZE*2+MALLOC_OVERHEAD, INT32_MAX, MALLOC_OVERHEAD, IO_SIZE,
    0},
  {"read_rnd_buffer_size", OPT_RECORD_RND_BUFFER,
   N_("When reading rows in sorted order after a sort, the rows are read "
      "through this buffer to avoid a disk seeks. If not set, then it's set "
      "to the value of record_buffer."),
   (char**) &global_system_variables.read_rnd_buff_size,
   NULL, 0,
   GET_UINT, REQUIRED_ARG, 256*1024L, 64 /*IO_SIZE*2+MALLOC_OVERHEAD*/ ,
   UINT32_MAX, MALLOC_OVERHEAD, 1 /* Small lower limit to be able to test MRR */, 0},
  /* x8 compared to MySQL's x2. We have UTF8 to consider. */
  {"sort_buffer_size", OPT_SORT_BUFFER,
   N_("Each thread that needs to do a sort allocates a buffer of this size."),
   (char**) &global_system_variables.sortbuff_size,
   NULL, 0, GET_SIZE, REQUIRED_ARG,
   MAX_SORT_MEMORY, MIN_SORT_MEMORY+MALLOC_OVERHEAD*8, (int64_t)SIZE_MAX,
   MALLOC_OVERHEAD, 1, 0},
  {"table_definition_cache", OPT_TABLE_DEF_CACHE,
   N_("The number of cached table definitions."),
   (char**) &table_def_size, NULL,
   0, GET_SIZE, REQUIRED_ARG, 128, 1, 512*1024L, 0, 1, 0},
  {"table_open_cache", OPT_TABLE_OPEN_CACHE,
   N_("The number of cached open tables."),
   (char**) &table_cache_size, NULL, 0, GET_UINT64,
   REQUIRED_ARG, TABLE_OPEN_CACHE_DEFAULT, TABLE_OPEN_CACHE_MIN, 512*1024L, 0, 1, 0},
  {"table_lock_wait_timeout", OPT_TABLE_LOCK_WAIT_TIMEOUT,
   N_("Timeout in seconds to wait for a table level lock before returning an "
      "error. Used only if the connection has active cursors."),
   (char**) &table_lock_wait_timeout, NULL,
   0, GET_ULL, REQUIRED_ARG, 50, 1, 1024 * 1024 * 1024, 0, 1, 0},
  {"thread_stack", OPT_THREAD_STACK,
   N_("The stack size for each thread."),
   (char**) &my_thread_stack_size,
   NULL, 0, GET_SIZE,
   REQUIRED_ARG,DEFAULT_THREAD_STACK,
   UINT32_C(1024*512), (int64_t)SIZE_MAX, 0, 1024, 0},
  {"tmp_table_size", OPT_TMP_TABLE_SIZE,
   N_("If an internal in-memory temporary table exceeds this size, Drizzle will"
      " automatically convert it to an on-disk MyISAM table."),
   (char**) &global_system_variables.tmp_table_size,
   NULL, 0, GET_ULL,
   REQUIRED_ARG, 16*1024*1024L, 1024, (int64_t)MAX_MEM_TABLE_SIZE, 0, 1, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void print_version()
{
  /*
    Note: the instance manager keys off the string 'Ver' so it can find the
    version from the output of 'drizzled --version', so don't change it!
  */
  printf("%s  Ver %s for %s-%s on %s (%s)\n", internal::my_progname,
	 PANDORA_RELEASE_VERSION, HOST_VENDOR, HOST_OS, HOST_CPU, COMPILATION_COMMENT);
}

static void usage()
{
  if (!(default_charset_info= get_charset_by_csname(default_character_set_name, MY_CS_PRIMARY)))
    exit(1);
  if (!default_collation_name)
    default_collation_name= default_charset_info->name;
  print_version();
  puts(_("Copyright (C) 2008 Sun Microsystems\n"
         "This software comes with ABSOLUTELY NO WARRANTY. "
         "This is free software,\n"
         "and you are welcome to modify and redistribute it under the GPL "
         "license\n\n"));


  printf(_("Usage: %s [OPTIONS]\n"), internal::my_progname);

  po::options_description all_options("Drizzled Options");
  all_options.add(config_options);
  all_options.add(plugin_load_options);
  all_options.add(long_options);
  all_options.add(plugin_options);
  cout << all_options << endl;

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

static void drizzle_init_variables()
{
  /* Things reset to zero */
  opt_tc_log_file= (char *)"tc.log";      // no hostname in tc_log file name !
  cleanup_done= 0;
  dropping_tables= ha_open_options=0;
  getDebug().reset();
  wake_thread=0;
  abort_loop= select_thread_in_use= false;
  shutdown_in_progress= 0;
  drizzled_user= drizzled_chroot= 0;
  memset(&current_global_counters, 0, sizeof(current_global_counters));
  key_map_full.set();

  /* Character sets */
  system_charset_info= &my_charset_utf8_general_ci;
  files_charset_info= &my_charset_utf8_general_ci;
  table_alias_charset= &my_charset_bin;
  character_set_filesystem= &my_charset_bin;

  /* Things with default values that are not zero */
  session_startup_options= (OPTION_AUTO_IS_NULL | OPTION_SQL_NOTES);
  global_thread_id= 1;
  session::Cache::getCache().clear();

  /* Set default values for some option variables */
  global_system_variables.storage_engine= NULL;
  global_system_variables.tx_isolation= ISO_REPEATABLE_READ;
  global_system_variables.select_limit= (uint64_t) HA_POS_ERROR;
  max_system_variables.select_limit=    (uint64_t) HA_POS_ERROR;
  global_system_variables.max_join_size= (uint64_t) HA_POS_ERROR;
  max_system_variables.max_join_size=   (uint64_t) HA_POS_ERROR;
  max_system_variables.auto_increment_increment= UINT64_MAX;
  max_system_variables.auto_increment_offset= UINT64_MAX;
  max_system_variables.completion_type= 2;
  max_system_variables.log_warnings= true;
  max_system_variables.bulk_insert_buff_size= ULONG_MAX;
  max_system_variables.div_precincrement= DECIMAL_MAX_SCALE;
  max_system_variables.group_concat_max_len= ULONG_MAX;
  max_system_variables.join_buff_size= ULONG_MAX;
  max_system_variables.max_allowed_packet= 1024L*1024L*1024L;
  max_system_variables.max_error_count= 65535;
  max_system_variables.max_heap_table_size= MAX_MEM_TABLE_SIZE;
  max_system_variables.max_join_size= INT32_MAX;
  max_system_variables.max_length_for_sort_data= 8192*1024L;
  max_system_variables.max_seeks_for_key= ULONG_MAX;
  max_system_variables.max_sort_length= 8192*1024L;
  max_system_variables.min_examined_row_limit= ULONG_MAX;
  max_system_variables.optimizer_prune_level= 1;
  max_system_variables.optimizer_search_depth= MAX_TABLES+2;
  max_system_variables.preload_buff_size= 1024*1024*1024L;
  max_system_variables.query_alloc_block_size= UINT32_MAX;
  max_system_variables.query_prealloc_size= UINT32_MAX;
  max_system_variables.range_alloc_block_size= SIZE_MAX;
  max_system_variables.read_buff_size= INT32_MAX;
  max_system_variables.read_rnd_buff_size= UINT32_MAX;
  max_system_variables.sortbuff_size= SIZE_MAX;
  max_system_variables.tmp_table_size= MAX_MEM_TABLE_SIZE;

  /* Variables that depends on compile options */
#ifdef HAVE_BROKEN_REALPATH
  have_symlink=SHOW_OPTION_NO;
#else
  have_symlink=SHOW_OPTION_YES;
#endif
}


/**
  @todo
  - FIXME add EXIT_TOO_MANY_ARGUMENTS to "drizzled/error.h" and return that code?
*/
static void get_options()
{
  setDataHomeCatalog(getDataHome() / "local");

  if (vm.count("user"))
  {
    if (! drizzled_user || ! strcmp(drizzled_user, vm["user"].as<string>().c_str()))
      drizzled_user= (char *)vm["user"].as<string>().c_str();

    else
      errmsg_printf(error::WARN, _("Ignoring user change to '%s' because the user was "
                                       "set to '%s' earlier on the command line\n"),
                    vm["user"].as<string>().c_str(), drizzled_user);
  }

  if (vm.count("version"))
  {
    print_version();
    exit(0);
  }

  if (vm.count("sort-heap-threshold"))
  {
    if ((vm["sort-heap-threshold"].as<uint64_t>() > 0) and
      (vm["sort-heap-threshold"].as<uint64_t>() <
      global_system_variables.sortbuff_size))
    {
      cout << _("Error: sort-heap-threshold cannot be less than sort-buffer-size") << endl;
      exit(-1);
    }

    global_sort_buffer.setMaxSize(vm["sort-heap-threshold"].as<uint64_t>());
  }

  if (vm.count("join-heap-threshold"))
  {
    if ((vm["join-heap-threshold"].as<uint64_t>() > 0) and
      (vm["join-heap-threshold"].as<uint64_t>() <
      global_system_variables.join_buff_size))
    {
      cout << _("Error: join-heap-threshold cannot be less than join-buffer-size") << endl;
      exit(-1);
    }

    global_join_buffer.setMaxSize(vm["join-heap-threshold"].as<uint64_t>());
  }

  if (vm.count("read-rnd-threshold"))
  {
    if ((vm["read-rnd-threshold"].as<uint64_t>() > 0) and
      (vm["read-rnd-threshold"].as<uint64_t>() <
      global_system_variables.read_rnd_buff_size))
    {
      cout << _("Error: read-rnd-threshold cannot be less than read-rnd-buffer-size") << endl;
      exit(-1);
    }

    global_read_rnd_buffer.setMaxSize(vm["read-rnd-threshold"].as<uint64_t>());
  }

  if (vm.count("read-buffer-threshold"))
  {
    if ((vm["read-buffer-threshold"].as<uint64_t>() > 0) and
      (vm["read-buffer-threshold"].as<uint64_t>() <
      global_system_variables.read_buff_size))
    {
      cout << _("Error: read-buffer-threshold cannot be less than read-buffer-size") << endl;
      exit(-1);
    }

    global_read_buffer.setMaxSize(vm["read-buffer-threshold"].as<uint64_t>());
  }

  if (vm.count("exit-info"))
  {
    if (vm["exit-info"].as<long>())
    {
      getDebug().set((uint32_t) vm["exit-info"].as<long>());
    }
  }

  if (vm.count("want-core"))
  {
    getDebug().set(debug::CORE_ON_SIGNAL);
  }

  if (vm.count("skip-stack-trace"))
  {
    getDebug().set(debug::NO_STACKTRACE);
  }

  if (vm.count("skip-symlinks"))
  {
    internal::my_use_symdir=0;
  }

  if (vm.count("transaction-isolation"))
  {
    int type= tx_isolation_typelib.find_type_or_exit(vm["transaction-isolation"].as<string>().c_str(), "transaction-isolation");
    global_system_variables.tx_isolation= type - 1;
  }

  /* @TODO Make this all strings */
  if (vm.count("default-storage-engine"))
  {
    default_storage_engine_str= vm["default-storage-engine"].as<string>().c_str();
  }


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
    getDebug().set(debug::ALLOW_SIGINT);
    getDebug().set(debug::NO_STACKTRACE);
    getDebug().reset(debug::CORE_ON_SIGNAL);
  }

  if (drizzled_chroot)
    set_root(drizzled_chroot);

  /*
    Set some global variables from the global_system_variables
    In most cases the global variables will not be used
  */
  internal::my_default_record_cache_size=global_system_variables.read_buff_size;
}


static void fix_paths()
{
  fs::path pid_file_path(pid_file);
  if (pid_file_path.root_path().string() == "")
  {
    pid_file_path= getDataHome();
    pid_file_path /= pid_file;
  }
  pid_file= fs::system_complete(pid_file_path);

  if (not opt_help)
  {
    const char *tmp_string= getenv("TMPDIR");
    struct stat buf;
    drizzle_tmpdir.clear();

    if (vm.count("tmpdir"))
    {
      drizzle_tmpdir.append(vm["tmpdir"].as<string>());
    }
    else if (tmp_string == NULL)
    {
      drizzle_tmpdir.append(getDataHome().file_string());
      drizzle_tmpdir.push_back(FN_LIBCHAR);
      drizzle_tmpdir.append(GLOBAL_TEMPORARY_EXT);
    }
    else
    {
      drizzle_tmpdir.append(tmp_string);
    }

    drizzle_tmpdir= fs::path(fs::system_complete(fs::path(drizzle_tmpdir))).file_string();
    assert(drizzle_tmpdir.size());

    if (mkdir(drizzle_tmpdir.c_str(), 0777) == -1)
    {
      if (errno != EEXIST)
      {
        errmsg_printf(error::ERROR, _("There was an error creating the '%s' part of the path '%s'.  Please check the path exists and is writable.\n"), fs::path(drizzle_tmpdir).leaf().c_str(), drizzle_tmpdir.c_str());
        exit(1);
      }
    }

    if (stat(drizzle_tmpdir.c_str(), &buf) || not S_ISDIR(buf.st_mode))
    {
      errmsg_printf(error::ERROR, _("There was an error opening the path '%s', please check the path exists and is writable.\n"), drizzle_tmpdir.c_str());
      exit(1);
    }
  }

}

} /* namespace drizzled */

