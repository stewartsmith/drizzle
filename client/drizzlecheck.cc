/* Copyright (C) 2008 Drizzle development team

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

/* By Jani Tolonen, 2001-04-20, MySQL, MYSQL Development Team */

#define CHECK_VERSION "2.5.0"

#include "client_priv.h"
#include <vector>
#include <string>
#include <mystrings/m_ctype.h>

/* Added this for string translation. */
#include <drizzled/gettext.h>


using namespace std;

template class vector<string>;

/* Exit codes */

#define EX_USAGE 1
#define EX_MYSQLERR 2

static drizzle_st drizzle;
static drizzle_con_st dcon;
static bool opt_alldbs= false, opt_check_only_changed= false,
            opt_extended= false, opt_databases= false,
            opt_fast= false, opt_medium_check= false, opt_quick= false,
            opt_all_in_1= false, opt_silent= false, opt_auto_repair= false,
            ignore_errors= false, tty_password= false, opt_frm= false,
            debug_info_flag= false, debug_check_flag= false,
            opt_fix_table_names= false, opt_fix_db_names= false,
            opt_upgrade= false, opt_write_binlog= true;
static uint32_t verbose= 0;
static uint32_t opt_drizzle_port= 0;
static int my_end_arg;
static char * opt_drizzle_unix_port= NULL;
static char *opt_password= NULL, *current_user= NULL,
      *default_charset= (char *)DRIZZLE_DEFAULT_CHARSET_NAME,
      *current_host= NULL;
static int first_error= 0;
vector<string> tables4repair;
static const CHARSET_INFO *charset_info= &my_charset_utf8_general_ci;

enum operations { DO_CHECK, DO_REPAIR, DO_ANALYZE, DO_OPTIMIZE, DO_UPGRADE };

static struct my_option my_long_options[] =
{
  {"all-databases", 'A',
   "Check all the databases. This will be same as  --databases with all databases selected.",
   (char**) &opt_alldbs, (char**) &opt_alldbs, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"analyze", 'a', "Analyze given tables.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"all-in-1", '1',
   "Instead of issuing one query for each table, use one query per database, naming all tables in the database in a comma-separated list.",
   (char**) &opt_all_in_1, (char**) &opt_all_in_1, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"auto-repair", OPT_AUTO_REPAIR,
   "If a checked table is corrupted, automatically fix it. Repairing will be done after all tables have been checked, if corrupted ones were found.",
   (char**) &opt_auto_repair, (char**) &opt_auto_repair, 0, GET_BOOL, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"check", 'c', "Check table for errors.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"check-only-changed", 'C',
   "Check only tables that have changed since last check or haven't been closed properly.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"check-upgrade", 'g',
   "Check tables for version-dependent changes. May be used with --auto-repair to correct tables requiring version-dependent updates.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"databases", 'B',
   "To check several databases. Note the difference in usage; In this case no tables are given. All name arguments are regarded as databasenames.",
   (char**) &opt_databases, (char**) &opt_databases, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"debug-check", OPT_DEBUG_CHECK, "Check memory and open file usage at exit.",
   (char**) &debug_check_flag, (char**) &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", OPT_DEBUG_INFO, "Print some debug info at exit.",
   (char**) &debug_info_flag, (char**) &debug_info_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", (char**) &default_charset,
   (char**) &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fast",'F', "Check only tables that haven't been closed properly.",
   (char**) &opt_fast, (char**) &opt_fast, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"fix-db-names", OPT_FIX_DB_NAMES, "Fix database names.",
    (char**) &opt_fix_db_names, (char**) &opt_fix_db_names,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"fix-table-names", OPT_FIX_TABLE_NAMES, "Fix table names.",
    (char**) &opt_fix_table_names, (char**) &opt_fix_table_names,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f', "Continue even if we get an sql-error.",
   (char**) &ignore_errors, (char**) &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"extended", 'e',
   "If you are using this option with CHECK TABLE, it will ensure that the table is 100 percent consistent, but will take a long time. If you are using this option with REPAIR TABLE, it will force using old slow repair with keycache method, instead of much faster repair by sorting.",
   (char**) &opt_extended, (char**) &opt_extended, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host",'h', "Connect to host.", (char**) &current_host,
   (char**) &current_host, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"medium-check", 'm',
   "Faster than extended-check, but only finds 99.99 percent of all errors. Should be good enough for most cases.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"write-binlog", OPT_WRITE_BINLOG,
   "Log ANALYZE, OPTIMIZE and REPAIR TABLE commands. Use --skip-write-binlog when commands should not be sent to replication slaves.",
   (char**) &opt_write_binlog, (char**) &opt_write_binlog, 0, GET_BOOL, NO_ARG,
   1, 0, 0, 0, 0, 0},
  {"optimize", 'o', "Optimize table.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"password", 'P',
   "Password to use when connecting to server. If password is not given it's solicited on the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'p', "Port number to use for connection or 0 for default to, in "
   "order of preference, drizzle.cnf, $DRIZZLE_TCP_PORT, "
   "built-in default (" STRINGIFY_ARG(DRIZZLE_PORT) ").",
   0, 0, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_DRIZZLE_PROTOCOL, "The protocol of connection (tcp,socket,pipe,memory).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q',
   "If you are using this option with CHECK TABLE, it prevents the check from scanning the rows to check for wrong links. This is the fastest check. If you are using this option with REPAIR TABLE, it will try to repair only the index tree. This is the fastest repair method for a table.",
   (char**) &opt_quick, (char**) &opt_quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"repair", 'r',
   "Can fix almost anything except unique keys that aren't unique.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Print only error messages.", (char**) &opt_silent,
   (char**) &opt_silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
   (char**) &opt_drizzle_unix_port, (char**) &opt_drizzle_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tables", OPT_TABLES, "Overrides option --databases (-B).", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"use-frm", OPT_FRM,
   "When used with REPAIR, get table structure from .frm file, so the table can be repaired even if .MYI header is corrupted.",
   (char**) &opt_frm, (char**) &opt_frm, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", (char**) &current_user,
   (char**) &current_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"verbose", 'v', "Print info about the various stages.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static const char *load_default_groups[] = { "mysqlcheck", "client", 0 };


static void print_version(void);
static void usage(void);
static int get_options(int *argc, char ***argv);
static int process_all_databases(void);
static int process_databases(char **db_names);
static int process_selected_tables(char *db, char **table_names, int tables);
static int process_all_tables_in_db(char *database);
static int process_one_db(char *database);
static int use_db(char *database);
static int handle_request_for_tables(const char *tables, uint32_t length);
static int dbConnect(char *host, char *user,char *passwd);
static void dbDisconnect(char *host);
static void DBerror(drizzle_con_st *con, const char *when);
static void safe_exit(int error);
static void print_result(drizzle_result_st *result);
static uint32_t fixed_name_length(const char *name);
static char *fix_table_name(char *dest, const char *src);
int what_to_do = 0;

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n", my_progname, CHECK_VERSION,
         drizzle_version(), SYSTEM_TYPE, MACHINE_TYPE);
} /* print_version */

static void usage(void)
{
  print_version();
  puts("By Jani Tolonen, 2001-04-20, MySQL Development Team\n");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n");
  puts("and you are welcome to modify and redistribute it under the GPL license.\n");
  puts("This program can be used to CHECK (-c,-m,-C), REPAIR (-r), ANALYZE (-a)");
  puts("or OPTIMIZE (-o) tables. Some of the options (like -e or -q) can be");
  puts("used at the same time. Not all options are supported by all storage engines.");
  puts("Please consult the Drizzle manual for latest information about the");
  puts("above. The options -c,-r,-a and -o are exclusive to each other, which");
  puts("means that the last option will be used, if several was specified.\n");
  puts("The option -c will be used by default, if none was specified. You");
  puts("can change the default behavior by making a symbolic link, or");
  puts("copying this file somewhere with another name, the alternatives are:");
  puts("mysqlrepair:   The default option will be -r");
  puts("mysqlanalyze:  The default option will be -a");
  puts("mysqloptimize: The default option will be -o\n");
  printf("Usage: %s [OPTIONS] database [tables]\n", my_progname);
  printf("OR     %s [OPTIONS] --databases DB1 [DB2 DB3...]\n",
   my_progname);
  printf("OR     %s [OPTIONS] --all-databases\n", my_progname);
  print_defaults("drizzle", load_default_groups);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
} /* usage */

extern "C"
bool get_one_option(int optid, const struct my_option *, char *argument)
{
  char *endchar= NULL;
  uint64_t temp_drizzle_port= 0;

  switch(optid) {
  case 'a':
    what_to_do = DO_ANALYZE;
    break;
  case 'c':
    what_to_do = DO_CHECK;
    break;
  case 'C':
    what_to_do = DO_CHECK;
    opt_check_only_changed = 1;
    break;
  case 'I': /* Fall through */
  case '?':
    usage();
    exit(0);
  case 'm':
    what_to_do = DO_CHECK;
    opt_medium_check = 1;
    break;
  case 'o':
    what_to_do = DO_OPTIMIZE;
    break;
  case OPT_FIX_DB_NAMES:
    what_to_do= DO_UPGRADE;
    default_charset= (char*) "utf8";
    opt_databases= 1;
    break;
  case OPT_FIX_TABLE_NAMES:
    what_to_do= DO_UPGRADE;
    default_charset= (char*) "utf8";
    break;
  case 'p':
    temp_drizzle_port= (uint64_t) strtoul(argument, &endchar, 10);
    /* if there is an alpha character this is not a valid port */
    if (strlen(endchar) != 0)
    {
      fprintf(stderr, _("Non-integer value supplied for port.  If you are trying to enter a password please use --password instead.\n"));
      exit(EX_USAGE);
    }
    /* If the port number is > 65535 it is not a valid port
       This also helps with potential data loss casting unsigned long to a
       uint32_t. */
    if ((temp_drizzle_port == 0) || (temp_drizzle_port > 65535))
    {
      fprintf(stderr, _("Value supplied for port is not valid.\n"));
      exit(EX_USAGE);
    }
    else
    {
      opt_drizzle_port= (uint32_t) temp_drizzle_port;
    }
    break;
  case 'P':
    if (argument)
    {
      char *start= argument;
      if (opt_password)
        free(opt_password);
      opt_password = strdup(argument);
      if (opt_password == NULL)
      {
        fprintf(stderr, "Memory allocation error while copying password. "
                        "Aborting.\n");
        exit(ENOMEM);
      }
      while (*argument)
      {
        /* Overwriting password with 'x' */
        *argument++= 'x';
      }
      if (*start)
      {
        /* Cut length of argument */
        start[1] = 0;
      }
      tty_password= 0;
    }
    else
      tty_password = 1;
    break;
  case 'r':
    what_to_do = DO_REPAIR;
    break;
  case 'g':
    what_to_do= DO_CHECK;
    opt_upgrade= 1;
    break;
  case OPT_TABLES:
    opt_databases = 0;
    break;
  case 'v':
    verbose++;
    break;
  case 'V': print_version(); exit(0);
  case OPT_DRIZZLE_PROTOCOL:
    break;
  }
  return 0;
}


static int get_options(int *argc, char ***argv)
{
  int ho_error;

  if (*argc == 1)
  {
    usage();
    exit(0);
  }

  load_defaults("drizzle", load_default_groups, argc, argv);

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (!what_to_do)
  {
    int pnlen = strlen(my_progname);

    if (pnlen < 6) /* name too short */
      what_to_do = DO_CHECK;
    else if (!strcmp("repair", my_progname + pnlen - 6))
      what_to_do = DO_REPAIR;
    else if (!strcmp("analyze", my_progname + pnlen - 7))
      what_to_do = DO_ANALYZE;
    else if  (!strcmp("optimize", my_progname + pnlen - 8))
      what_to_do = DO_OPTIMIZE;
    else
      what_to_do = DO_CHECK;
  }

  /* TODO: This variable is not yet used */
  if (strcmp(default_charset, charset_info->csname) &&
      !(charset_info= get_charset_by_csname(default_charset, MY_CS_PRIMARY)))
      exit(1);
  if (*argc > 0 && opt_alldbs)
  {
    printf("You should give only options, no arguments at all, with option\n");
    printf("--all-databases. Please see %s --help for more information.\n",
     my_progname);
    return 1;
  }
  if (*argc < 1 && !opt_alldbs)
  {
    printf("You forgot to give the arguments! Please see %s --help\n",
     my_progname);
    printf("for more information.\n");
    return 1;
  }
  if (tty_password)
    opt_password = client_get_tty_password(NULL);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;
  return(0);
} /* get_options */


static int process_all_databases()
{
  drizzle_row_t row;
  drizzle_result_st result;
  drizzle_return_t ret;
  int error = 0;

  if (drizzle_query_str(&dcon, &result, "SHOW DATABASES", &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK ||
      drizzle_result_buffer(&result) != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, "Error: Couldn't execute 'SHOW DATABASES': %s",
              drizzle_result_error(&result));
      drizzle_result_free(&result);
    }
    else
    {
      fprintf(stderr, "Error: Couldn't execute 'SHOW DATABASES': %s",
              drizzle_con_error(&dcon));
    }

    return 1;
  }
  while ((row = drizzle_row_next(&result)))
  {
    if (process_one_db((char *)row[0]))
      error = 1;
  }
  drizzle_result_free(&result);
  return error;
}
/* process_all_databases */


static int process_databases(char **db_names)
{
  int result = 0;
  for ( ; *db_names ; db_names++)
  {
    if (process_one_db(*db_names))
      result = 1;
  }
  return result;
} /* process_databases */


static int process_selected_tables(char *db, char **table_names, int tables)
{
  if (use_db(db))
    return 1;
  if (opt_all_in_1)
  {
    /*
      We need table list in form `a`, `b`, `c`
      that's why we need 2 more chars added to to each table name
      space is for more readable output in logs and in case of error
    */
    char *table_names_comma_sep, *end;
    int i, tot_length = 0;

    for (i = 0; i < tables; i++)
      tot_length+= fixed_name_length(*(table_names + i)) + 2;

    if (!(table_names_comma_sep = (char *)
          malloc((sizeof(char) * tot_length) + 4)))
      return 1;

    for (end = table_names_comma_sep + 1; tables > 0;
         tables--, table_names++)
    {
      end= fix_table_name(end, *table_names);
      *end++= ',';
    }
    *--end = 0;
    handle_request_for_tables(table_names_comma_sep + 1, tot_length - 1);
    free(table_names_comma_sep);
  }
  else
    for (; tables > 0; tables--, table_names++)
      handle_request_for_tables(*table_names, fixed_name_length(*table_names));
  return 0;
} /* process_selected_tables */


static uint32_t fixed_name_length(const char *name)
{
  const char *p;
  uint32_t extra_length= 2;  /* count the first/last backticks */

  for (p= name; *p; p++)
  {
    if (*p == '`')
      extra_length++;
    else if (*p == '.')
      extra_length+= 2;
  }
  return (p - name) + extra_length;
}


static char *fix_table_name(char *dest, const char *src)
{
  *dest++= '`';
  for (; *src; src++)
  {
    switch (*src) {
    case '.':            /* add backticks around '.' */
      *dest++= '`';
      *dest++= '.';
      *dest++= '`';
      break;
    case '`':            /* escape backtick character */
      *dest++= '`';
      /* fall through */
    default:
      *dest++= *src;
    }
  }
  *dest++= '`';
  return dest;
}


static int process_all_tables_in_db(char *database)
{
  drizzle_result_st result;
  drizzle_row_t row;
  drizzle_return_t ret;
  uint32_t num_columns;

  if (use_db(database))
    return 1;
  if (drizzle_query_str(&dcon, &result, "SHOW /*!50002 FULL*/ TABLES",
       &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK ||
      drizzle_result_buffer(&result) != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
      drizzle_result_free(&result);
    return 1;
  }

  num_columns= drizzle_result_column_count(&result);

  if (opt_all_in_1)
  {
    /*
      We need table list in form `a`, `b`, `c`
      that's why we need 2 more chars added to to each table name
      space is for more readable output in logs and in case of error
     */

    char *tables, *end;
    uint32_t tot_length = 0;

    while ((row = drizzle_row_next(&result)))
      tot_length+= fixed_name_length((char *)row[0]) + 2;
    drizzle_row_seek(&result, 0);

    if (!(tables=(char *) malloc(sizeof(char)*tot_length+4)))
    {
      drizzle_result_free(&result);
      return 1;
    }
    for (end = tables + 1; (row = drizzle_row_next(&result)) ;)
    {
      if ((num_columns == 2) && (strcmp((char *)row[1], "VIEW") == 0))
        continue;

      end= fix_table_name(end, (char *)row[0]);
      *end++= ',';
    }
    *--end = 0;
    if (tot_length)
      handle_request_for_tables(tables + 1, tot_length - 1);
    free(tables);
  }
  else
  {
    while ((row = drizzle_row_next(&result)))
    {
      /* Skip views if we don't perform renaming. */
      if ((what_to_do != DO_UPGRADE) && (num_columns == 2) && (strcmp((char *)row[1], "VIEW") == 0))
        continue;

      handle_request_for_tables((char *)row[0],
                                fixed_name_length((char *)row[0]));
    }
  }
  drizzle_result_free(&result);
  return 0;
} /* process_all_tables_in_db */



static int fix_table_storage_name(const char *name)
{
  char qbuf[100 + DRIZZLE_MAX_COLUMN_NAME_SIZE*4];
  drizzle_result_st result;
  drizzle_return_t ret;
  int rc= 0;
  if (strncmp(name, "#mysql50#", 9))
    return 1;
  sprintf(qbuf, "RENAME TABLE `%s` TO `%s`", name, name + 9);
  if (drizzle_query_str(&dcon, &result, qbuf, &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    fprintf(stderr, "Failed to %s\n", qbuf);
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, "Error: %s\n", drizzle_result_error(&result));
      drizzle_result_free(&result);
    }
    else
      fprintf(stderr, "Error: %s\n", drizzle_con_error(&dcon));
    rc= 1;
  }
  else
    drizzle_result_free(&result);
  if (verbose)
    printf("%-50s %s\n", name, rc ? "FAILED" : "OK");
  return rc;
}

static int fix_database_storage_name(const char *name)
{
  char qbuf[100 + DRIZZLE_MAX_COLUMN_NAME_SIZE*4];
  drizzle_result_st result;
  drizzle_return_t ret;
  int rc= 0;
  if (strncmp(name, "#mysql50#", 9))
    return 1;
  sprintf(qbuf, "ALTER DATABASE `%s` UPGRADE DATA DIRECTORY NAME", name);
  if (drizzle_query_str(&dcon, &result, qbuf, &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    fprintf(stderr, "Failed to %s\n", qbuf);
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, "Error: %s\n", drizzle_result_error(&result));
      drizzle_result_free(&result);
    }
    else
      fprintf(stderr, "Error: %s\n", drizzle_con_error(&dcon));
    rc= 1;
  }
  else
    drizzle_result_free(&result);
  if (verbose)
    printf("%-50s %s\n", name, rc ? "FAILED" : "OK");
  return rc;
}

static int process_one_db(char *database)
{
  if (what_to_do == DO_UPGRADE)
  {
    int rc= 0;
    if (opt_fix_db_names && !strncmp(database,"#mysql50#", 9))
    {
      rc= fix_database_storage_name(database);
      database+= 9;
    }
    if (rc || !opt_fix_table_names)
      return rc;
  }
  return process_all_tables_in_db(database);
}


static int use_db(char *database)
{
  drizzle_result_st result;
  drizzle_return_t ret;
  if (drizzle_con_server_version_number(&dcon) >= 50003 &&
      !my_strcasecmp(&my_charset_utf8_general_ci, database, "information_schema"))
    return 1;
  if (drizzle_select_db(&dcon, &result, database, &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr,"Got error: %s when selecting the database",
              drizzle_result_error(&result));
      safe_exit(EX_MYSQLERR);
      drizzle_result_free(&result);
    }
    else
      DBerror(&dcon, "when selecting the database");
    return 1;
  }
  drizzle_result_free(&result);
  return 0;
} /* use_db */


static int handle_request_for_tables(const char *tables, uint32_t length)
{
  char *query, *end, options[100], message[100];
  uint32_t query_length= 0;
  const char *op = 0;
  drizzle_result_st result;
  drizzle_return_t ret;

  options[0] = 0;
  end = options;
  switch (what_to_do) {
  case DO_CHECK:
    op = "CHECK";
    if (opt_quick)              end = strcpy(end, " QUICK")+6;
    if (opt_fast)               end = strcpy(end, " FAST")+5;
    if (opt_medium_check)       end = strcpy(end, " MEDIUM")+7; /* Default */
    if (opt_extended)           end = strcpy(end, " EXTENDED")+9;
    if (opt_check_only_changed) end = strcpy(end, " CHANGED")+8;
    if (opt_upgrade)            end = strcpy(end, " FOR UPGRADE")+12;
    break;
  case DO_REPAIR:
    op= (opt_write_binlog) ? "REPAIR" : "REPAIR NO_WRITE_TO_BINLOG";
    if (opt_quick)              end = strcpy(end, " QUICK")+6;
    if (opt_extended)           end = strcpy(end, " EXTENDED")+9;
    if (opt_frm)                end = strcpy(end, " USE_FRM")+8;
    break;
  case DO_ANALYZE:
    op= (opt_write_binlog) ? "ANALYZE" : "ANALYZE NO_WRITE_TO_BINLOG";
    break;
  case DO_OPTIMIZE:
    op= (opt_write_binlog) ? "OPTIMIZE" : "OPTIMIZE NO_WRITE_TO_BINLOG";
    break;
  case DO_UPGRADE:
    return fix_table_storage_name(tables);
  }

  if (!(query =(char *) malloc((sizeof(char)*(length+110)))))
    return 1;
  if (opt_all_in_1)
  {
    /* No backticks here as we added them before */
    query_length= sprintf(query, "%s TABLE %s %s", op, tables, options);
  }
  else
  {
    char *ptr;
    ptr= query;
    ptr= strcpy(query, op)+strlen(op);
    ptr= strcpy(ptr, " TABLE ")+7;
    ptr= fix_table_name(ptr, tables);
    ptr+= sprintf(ptr," %s",options);
    query_length= (uint32_t) (ptr - query);
  }
  if (drizzle_query(&dcon, &result, query, query_length, &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK ||
      drizzle_result_buffer(&result) != DRIZZLE_RETURN_OK)
  {
    sprintf(message, "when executing '%s TABLE ... %s'", op, options);
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr,"Got error: %s %s",
              drizzle_result_error(&result), message);
      safe_exit(EX_MYSQLERR);
      drizzle_result_free(&result);
    }
    else
      DBerror(&dcon, message);
    return 1;
  }
  print_result(&result);
  drizzle_result_free(&result);
  free(query);
  return 0;
}


static void print_result(drizzle_result_st *result)
{
  drizzle_row_t row;
  char prev[DRIZZLE_MAX_COLUMN_NAME_SIZE*2+2];
  uint32_t i;
  bool found_error=0;

  prev[0] = '\0';
  for (i = 0; (row = drizzle_row_next(result)); i++)
  {
    int changed = strcmp(prev, (char *)row[0]);
    bool status = !strcmp((char *)row[2], "status");

    if (status)
    {
      /*
        if there was an error with the table, we have --auto-repair set,
        and this isn't a repair op, then add the table to the tables4repair
        list
      */
      if (found_error && opt_auto_repair && what_to_do != DO_REPAIR &&
          strcmp((char *)row[3],"OK"))
        tables4repair.push_back(string(prev));
      found_error=0;
      if (opt_silent)
        continue;
    }
    if (status && changed)
      printf("%-50s %s", row[0], row[3]);
    else if (!status && changed)
    {
      printf("%s\n%-9s: %s", row[0], row[2], row[3]);
      if (strcmp((char *)row[2],"note"))
        found_error=1;
    }
    else
      printf("%-9s: %s", (char *)row[2], (char *)row[3]);
    strcpy(prev, (char *)row[0]);
    putchar('\n');
  }
  /* add the last table to be repaired to the list */
  if (found_error && opt_auto_repair && what_to_do != DO_REPAIR)
    tables4repair.push_back(string(prev));
}


static int dbConnect(char *host, char *user, char *passwd)
{

  if (verbose)
  {
    fprintf(stderr, "# Connecting to %s...\n", host ? host : "localhost");
  }
  drizzle_create(&drizzle);
  drizzle_con_create(&drizzle, &dcon);
  drizzle_con_set_tcp(&dcon, host, opt_drizzle_port);
  drizzle_con_set_auth(&dcon, user, passwd);
  if (drizzle_con_connect(&dcon) != DRIZZLE_RETURN_OK)
  {
    DBerror(&dcon, "when trying to connect");
    return 1;
  }
  return 0;
} /* dbConnect */


static void dbDisconnect(char *host)
{
  if (verbose)
    fprintf(stderr, "# Disconnecting from %s...\n", host ? host : "localhost");
  drizzle_free(&drizzle);
} /* dbDisconnect */


static void DBerror(drizzle_con_st *con, const char *when)
{
  fprintf(stderr,"Got error: %s %s", drizzle_con_error(con), when);
  safe_exit(EX_MYSQLERR);
  return;
} /* DBerror */


static void safe_exit(int error)
{
  if (!first_error)
    first_error= error;
  if (ignore_errors)
    return;
  drizzle_free(&drizzle);
  exit(error);
}


int main(int argc, char **argv)
{
  MY_INIT(argv[0]);
  /*
  ** Check out the args
  */
  if (get_options(&argc, &argv))
  {
    my_end(my_end_arg);
    exit(EX_USAGE);
  }
  if (dbConnect(current_host, current_user, opt_password))
    exit(EX_MYSQLERR);

  if (opt_auto_repair)
  {
    tables4repair.reserve(64);
    if (tables4repair.capacity() == 0)
    {
      first_error = 1;
      goto end;
    }
  }


  if (opt_alldbs)
    process_all_databases();
  /* Only one database and selected table(s) */
  else if (argc > 1 && !opt_databases)
    process_selected_tables(*argv, (argv + 1), (argc - 1));
  /* One or more databases, all tables */
  else
    process_databases(argv);
  if (opt_auto_repair)
  {

    if (!opt_silent && (tables4repair.size() > 0))
      puts("\nRepairing tables");
    what_to_do = DO_REPAIR;
    vector<string>::iterator i;
    for ( i= tables4repair.begin() ; i < tables4repair.end() ; i++)
    {
      const char *name= (*i).c_str();
      handle_request_for_tables(name, fixed_name_length(name));
    }
  }
 end:
  dbDisconnect(current_host);
  free(opt_password);
  my_end(my_end_arg);
  return(first_error!=0);
} /* main */
