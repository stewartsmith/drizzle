/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/* drizzle command tool
 * Commands compatible with mSQL by David J. Hughes
 *
 * Written by:
 *   Michael 'Monty' Widenius
 *   Andi Gutmans  <andi@zend.com>
 *   Zeev Suraski  <zeev@zend.com>
 *   Jani Tolonen  <jani@mysql.com>
 *   Matt Wagner   <matt@mysql.com>
 *   Jeremy Cole   <jcole@mysql.com>
 *   Tonu Samuel   <tonu@mysql.com>
 *   Harrison Fisk <harrison@mysql.com>
 *
 **/

#include "client_priv.h"
#include <string>
#include <drizzled/gettext.h>
#include <iostream>
#include <map>
#include <algorithm>
#include <limits.h>
#include <cassert>
#include "drizzled/charset_info.h"
#include <stdarg.h>
#include <math.h>
#include "client/linebuffer.h"
#include <signal.h>
#include <sys/ioctl.h>
#include <drizzled/configmake.h>
#include "drizzled/charset.h"

#if defined(HAVE_CURSES_H) && defined(HAVE_TERM_H)
#include <curses.h>
#ifdef __sun
#undef clear
#undef erase
#endif
#include <term.h>
#else
#if defined(HAVE_TERMIOS_H)
#include <termios.h>
#include <unistd.h>
#elif defined(HAVE_TERMBITS_H)
#include <termbits.h>
#elif defined(HAVE_ASM_TERMBITS_H) && (!defined __GLIBC__ || !(__GLIBC__ > 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ > 0))
#include <asm/termbits.h>    // Standard linux
#endif
#if defined(HAVE_TERMCAP_H)
#include <termcap.h>
#else
#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#undef SYSV        // hack to avoid syntax error
#ifdef HAVE_TERM_H
#include <term.h>
#endif
#endif
#endif

#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)
#    include <readline.h>
#  else /* !defined(HAVE_READLINE_H) */
extern char *readline ();
#  endif /* !defined(HAVE_READLINE_H) */
char *cmdline = NULL;
#else /* !defined(HAVE_READLINE_READLINE_H) */
  /* no readline */
#  error Readline Required
#endif /* HAVE_LIBREADLINE */

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  else /* !defined(HAVE_HISTORY_H) */
extern void add_history ();
extern int write_history ();
extern int read_history ();
#  endif /* defined(HAVE_READLINE_HISTORY_H) */
    /* no history */
#endif /* HAVE_READLINE_HISTORY */

/**
 Make the old readline interface look like the new one.
*/
#ifndef HAVE_RL_COMPLETION
typedef char **rl_completion_func_t(const char *, int, int);
#define rl_completion_matches(str, func) \
  completion_matches((char *)str, (CPFunction *)func)
#endif

#ifdef HAVE_RL_COMPENTRY
# ifdef HAVE_WORKING_RL_COMPENTRY
typedef rl_compentry_func_t drizzle_compentry_func_t;
# else
/* Snow Leopard ships an rl_compentry which cannot be assigned to
 * rl_completion_entry_function. We must undo the complete and total
 * ass-bagery.
 */
typedef Function drizzle_compentry_func_t;
# endif
#else
typedef Function drizzle_compentry_func_t;
#endif

#if defined(HAVE_LOCALE_H)
#include <locale.h>
#endif



#if !defined(HAVE_VIDATTR)
#undef vidattr
#define vidattr(A) {}      // Can't get this to work
#endif

using namespace drizzled;
using namespace std;

const string VER("14.14");
/* Don't try to make a nice table if the data is too big */
const uint32_t MAX_COLUMN_LENGTH= 1024;

/* Buffer to hold 'version' and 'version_comment' */
const int MAX_SERVER_VERSION_LENGTH= 128;

#define PROMPT_CHAR '\\'
#define DEFAULT_DELIMITER ";"

typedef struct st_status
{
  int exit_status;
  uint32_t query_start_line;
  char *file_name;
  LineBuffer *line_buff;
  bool batch,add_to_history;
} STATUS;


static map<string, string>::iterator completion_iter;
static map<string, string>::iterator completion_end;
static map<string, string> completion_map;
static string completion_string;

static char **defaults_argv;

enum enum_info_type { INFO_INFO,INFO_ERROR,INFO_RESULT};
typedef enum enum_info_type INFO_TYPE;

static drizzle_st drizzle;      /* The library handle */
static drizzle_con_st con;      /* The connection */
static bool ignore_errors= false, quick= false,
  connected= false, opt_raw_data= false, unbuffered= false,
  output_tables= false, opt_rehash= true, skip_updates= false,
  safe_updates= false, one_database= false,
  opt_compress= false, opt_shutdown= false, opt_ping= false,
  vertical= false, line_numbers= true, column_names= true,
  opt_nopager= true, opt_outfile= false, named_cmds= false,
  tty_password= false, opt_nobeep= false, opt_reconnect= true,
  default_charset_used= false, opt_secure_auth= false,
  default_pager_set= false, opt_sigint_ignore= false,
  auto_vertical_output= false,
  show_warnings= false, executing_query= false, interrupted_query= false,
  opt_mysql= false;
static uint32_t  show_progress_size= 0;
static bool column_types_flag;
static bool preserve_comments= false;
static uint32_t opt_max_input_line, opt_drizzle_port= 0;
static int verbose= 0, opt_silent= 0, opt_local_infile= 0;
static drizzle_capabilities_t connect_flag= DRIZZLE_CAPABILITIES_NONE;
static char *current_host, *current_db, *current_user= NULL,
  *opt_password= NULL, *delimiter_str= NULL, *current_prompt= NULL;
static char *histfile;
static char *histfile_tmp;
static string *glob_buffer;
static string *processed_prompt= NULL;
static char *default_prompt= NULL;
static char *full_username= NULL,*part_username= NULL;
static STATUS status;
static uint32_t select_limit;
static uint32_t max_join_size;
static uint32_t opt_connect_timeout= 0;
// TODO: Need to i18n these
static const char *day_names[]= {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *month_names[]= {"Jan","Feb","Mar","Apr","May","Jun","Jul",
                                  "Aug","Sep","Oct","Nov","Dec"};
static char default_pager[FN_REFLEN];
static char pager[FN_REFLEN], outfile[FN_REFLEN];
static FILE *PAGER, *OUTFILE;
static uint32_t prompt_counter;
static char delimiter[16]= DEFAULT_DELIMITER;
static uint32_t delimiter_length= 1;
unsigned short terminal_width= 80;

static const CHARSET_INFO *charset_info= &my_charset_utf8_general_ci;

int drizzleclient_real_query_for_lazy(const char *buf, int length,
                                      drizzle_result_st *result,
                                      uint32_t *error_code);
int drizzleclient_store_result_for_lazy(drizzle_result_st *result);


void tee_fprintf(FILE *file, const char *fmt, ...);
void tee_fputs(const char *s, FILE *file);
void tee_puts(const char *s, FILE *file);
void tee_putc(int c, FILE *file);
static void tee_print_sized_data(const char *, unsigned int, unsigned int, bool);
/* The names of functions that actually do the manipulation. */
static int get_options(int argc,char **argv);
static int com_quit(string *str,const char*),
  com_go(string *str,const char*), com_ego(string *str,const char*),
  com_print(string *str,const char*),
  com_help(string *str,const char*), com_clear(string *str,const char*),
  com_connect(string *str,const char*), com_status(string *str,const char*),
  com_use(string *str,const char*), com_source(string *str, const char*),
  com_rehash(string *str, const char*), com_tee(string *str, const char*),
  com_notee(string *str, const char*),
  com_prompt(string *str, const char*), com_delimiter(string *str, const char*),
  com_warnings(string *str, const char*), com_nowarnings(string *str, const char*),
  com_nopager(string *str, const char*), com_pager(string *str, const char*);

static int read_and_execute(bool interactive);
static int sql_connect(char *host,char *database,char *user,char *password,
                       uint32_t silent);
static const char *server_version_string(drizzle_con_st *con);
static int put_info(const char *str,INFO_TYPE info,uint32_t error,
                    const char *sql_state);
static int put_error(drizzle_con_st *con, drizzle_result_st *res);
static void safe_put_field(const char *pos,uint32_t length);
static void init_pager(void);
static void end_pager(void);
static void init_tee(const char *);
static void end_tee(void);
static const char* construct_prompt(void);
static char *get_arg(char *line, bool get_next_arg);
static void init_username(void);
static void add_int_to_prompt(int toadd);
static int get_result_width(drizzle_result_st *res);
static int get_field_disp_length(drizzle_column_st * field);
static const char * strcont(register const char *str, register const char *set);

/* A structure which contains information on the commands this program
   can understand. */
typedef struct {
  const char *name;        /* User printable name of the function. */
  char cmd_char;        /* msql command character */
  int (*func)(string *str,const char *); /* Function to call to do the job. */
  bool takes_params;        /* Max parameters for command */
  const char *doc;        /* Documentation for this function.  */
} COMMANDS;


static COMMANDS commands[] = {
  { "?",      '?', com_help,   0, N_("Synonym for `help'.") },
  { "clear",  'c', com_clear,  0, N_("Clear command.")},
  { "connect",'r', com_connect,1,
    N_("Reconnect to the server. Optional arguments are db and host.")},
  { "delimiter", 'd', com_delimiter,    1,
    N_("Set statement delimiter. NOTE: Takes the rest of the line as new delimiter.") },
  { "ego",    'G', com_ego,    0,
    N_("Send command to drizzle server, display result vertically.")},
  { "exit",   'q', com_quit,   0, N_("Exit drizzle. Same as quit.")},
  { "go",     'g', com_go,     0, N_("Send command to drizzle server.") },
  { "help",   'h', com_help,   0, N_("Display this help.") },
  { "nopager",'n', com_nopager,0, N_("Disable pager, print to stdout.") },
  { "notee",  't', com_notee,  0, N_("Don't write into outfile.") },
  { "pager",  'P', com_pager,  1,
    N_("Set PAGER [to_pager]. Print the query results via PAGER.") },
  { "print",  'p', com_print,  0, N_("Print current command.") },
  { "prompt", 'R', com_prompt, 1, N_("Change your drizzle prompt.")},
  { "quit",   'q', com_quit,   0, N_("Quit drizzle.") },
  { "rehash", '#', com_rehash, 0, N_("Rebuild completion hash.") },
  { "source", '.', com_source, 1,
    N_("Execute an SQL script file. Takes a file name as an argument.")},
  { "status", 's', com_status, 0, N_("Get status information from the server.")},
  { "tee",    'T', com_tee,    1,
    N_("Set outfile [to_outfile]. Append everything into given outfile.") },
  { "use",    'u', com_use,    1,
    N_("Use another database. Takes database name as argument.") },
  { "warnings", 'W', com_warnings,  0,
    N_("Show warnings after every statement.") },
  { "nowarning", 'w', com_nowarnings, 0,
    N_("Don't show warnings after every statement.") },
  /* Get bash-like expansion for some commands */
  { "create table",     0, 0, 0, ""},
  { "create database",  0, 0, 0, ""},
  { "show databases",   0, 0, 0, ""},
  { "show fields from", 0, 0, 0, ""},
  { "show keys from",   0, 0, 0, ""},
  { "show tables",      0, 0, 0, ""},
  { "load data from",   0, 0, 0, ""},
  { "alter table",      0, 0, 0, ""},
  { "set option",       0, 0, 0, ""},
  { "lock tables",      0, 0, 0, ""},
  { "unlock tables",    0, 0, 0, ""},
  /* generated 2006-12-28.  Refresh occasionally from lexer. */
  { "ACTION", 0, 0, 0, ""},
  { "ADD", 0, 0, 0, ""},
  { "AFTER", 0, 0, 0, ""},
  { "AGAINST", 0, 0, 0, ""},
  { "AGGREGATE", 0, 0, 0, ""},
  { "ALL", 0, 0, 0, ""},
  { "ALGORITHM", 0, 0, 0, ""},
  { "ALTER", 0, 0, 0, ""},
  { "ANALYZE", 0, 0, 0, ""},
  { "AND", 0, 0, 0, ""},
  { "ANY", 0, 0, 0, ""},
  { "AS", 0, 0, 0, ""},
  { "ASC", 0, 0, 0, ""},
  { "ASCII", 0, 0, 0, ""},
  { "ASENSITIVE", 0, 0, 0, ""},
  { "AUTO_INCREMENT", 0, 0, 0, ""},
  { "AVG", 0, 0, 0, ""},
  { "AVG_ROW_LENGTH", 0, 0, 0, ""},
  { "BACKUP", 0, 0, 0, ""},
  { "BDB", 0, 0, 0, ""},
  { "BEFORE", 0, 0, 0, ""},
  { "BEGIN", 0, 0, 0, ""},
  { "BERKELEYDB", 0, 0, 0, ""},
  { "BETWEEN", 0, 0, 0, ""},
  { "BIGINT", 0, 0, 0, ""},
  { "BINARY", 0, 0, 0, ""},
  { "BINLOG", 0, 0, 0, ""},
  { "BIT", 0, 0, 0, ""},
  { "BLOB", 0, 0, 0, ""},
  { "BOOL", 0, 0, 0, ""},
  { "BOOLEAN", 0, 0, 0, ""},
  { "BOTH", 0, 0, 0, ""},
  { "BTREE", 0, 0, 0, ""},
  { "BY", 0, 0, 0, ""},
  { "BYTE", 0, 0, 0, ""},
  { "CACHE", 0, 0, 0, ""},
  { "CALL", 0, 0, 0, ""},
  { "CASCADE", 0, 0, 0, ""},
  { "CASCADED", 0, 0, 0, ""},
  { "CASE", 0, 0, 0, ""},
  { "CHAIN", 0, 0, 0, ""},
  { "CHANGE", 0, 0, 0, ""},
  { "CHANGED", 0, 0, 0, ""},
  { "CHAR", 0, 0, 0, ""},
  { "CHARACTER", 0, 0, 0, ""},
  { "CHARSET", 0, 0, 0, ""},
  { "CHECK", 0, 0, 0, ""},
  { "CHECKSUM", 0, 0, 0, ""},
  { "CIPHER", 0, 0, 0, ""},
  { "CLIENT", 0, 0, 0, ""},
  { "CLOSE", 0, 0, 0, ""},
  { "CODE", 0, 0, 0, ""},
  { "COLLATE", 0, 0, 0, ""},
  { "COLLATION", 0, 0, 0, ""},
  { "COLUMN", 0, 0, 0, ""},
  { "COLUMNS", 0, 0, 0, ""},
  { "COMMENT", 0, 0, 0, ""},
  { "COMMIT", 0, 0, 0, ""},
  { "COMMITTED", 0, 0, 0, ""},
  { "COMPACT", 0, 0, 0, ""},
  { "COMPRESSED", 0, 0, 0, ""},
  { "CONCURRENT", 0, 0, 0, ""},
  { "CONDITION", 0, 0, 0, ""},
  { "CONNECTION", 0, 0, 0, ""},
  { "CONSISTENT", 0, 0, 0, ""},
  { "CONSTRAINT", 0, 0, 0, ""},
  { "CONTAINS", 0, 0, 0, ""},
  { "CONTINUE", 0, 0, 0, ""},
  { "CONVERT", 0, 0, 0, ""},
  { "CREATE", 0, 0, 0, ""},
  { "CROSS", 0, 0, 0, ""},
  { "CUBE", 0, 0, 0, ""},
  { "CURRENT_DATE", 0, 0, 0, ""},
  { "CURRENT_TIMESTAMP", 0, 0, 0, ""},
  { "CURRENT_USER", 0, 0, 0, ""},
  { "CURSOR", 0, 0, 0, ""},
  { "DATA", 0, 0, 0, ""},
  { "DATABASE", 0, 0, 0, ""},
  { "DATABASES", 0, 0, 0, ""},
  { "DATE", 0, 0, 0, ""},
  { "DATETIME", 0, 0, 0, ""},
  { "DAY", 0, 0, 0, ""},
  { "DAY_HOUR", 0, 0, 0, ""},
  { "DAY_MICROSECOND", 0, 0, 0, ""},
  { "DAY_MINUTE", 0, 0, 0, ""},
  { "DAY_SECOND", 0, 0, 0, ""},
  { "DEALLOCATE", 0, 0, 0, ""},
  { "DEC", 0, 0, 0, ""},
  { "DECIMAL", 0, 0, 0, ""},
  { "DECLARE", 0, 0, 0, ""},
  { "DEFAULT", 0, 0, 0, ""},
  { "DEFINER", 0, 0, 0, ""},
  { "DELAYED", 0, 0, 0, ""},
  { "DELAY_KEY_WRITE", 0, 0, 0, ""},
  { "DELETE", 0, 0, 0, ""},
  { "DESC", 0, 0, 0, ""},
  { "DESCRIBE", 0, 0, 0, ""},
  { "DES_KEY_FILE", 0, 0, 0, ""},
  { "DETERMINISTIC", 0, 0, 0, ""},
  { "DIRECTORY", 0, 0, 0, ""},
  { "DISABLE", 0, 0, 0, ""},
  { "DISCARD", 0, 0, 0, ""},
  { "DISTINCT", 0, 0, 0, ""},
  { "DISTINCTROW", 0, 0, 0, ""},
  { "DIV", 0, 0, 0, ""},
  { "DO", 0, 0, 0, ""},
  { "DOUBLE", 0, 0, 0, ""},
  { "DROP", 0, 0, 0, ""},
  { "DUAL", 0, 0, 0, ""},
  { "DUMPFILE", 0, 0, 0, ""},
  { "DUPLICATE", 0, 0, 0, ""},
  { "DYNAMIC", 0, 0, 0, ""},
  { "EACH", 0, 0, 0, ""},
  { "ELSE", 0, 0, 0, ""},
  { "ELSEIF", 0, 0, 0, ""},
  { "ENABLE", 0, 0, 0, ""},
  { "ENCLOSED", 0, 0, 0, ""},
  { "END", 0, 0, 0, ""},
  { "ENGINE", 0, 0, 0, ""},
  { "ENGINES", 0, 0, 0, ""},
  { "ENUM", 0, 0, 0, ""},
  { "ERRORS", 0, 0, 0, ""},
  { "ESCAPE", 0, 0, 0, ""},
  { "ESCAPED", 0, 0, 0, ""},
  { "EVENTS", 0, 0, 0, ""},
  { "EXECUTE", 0, 0, 0, ""},
  { "EXISTS", 0, 0, 0, ""},
  { "EXIT", 0, 0, 0, ""},
  { "EXPANSION", 0, 0, 0, ""},
  { "EXPLAIN", 0, 0, 0, ""},
  { "EXTENDED", 0, 0, 0, ""},
  { "FALSE", 0, 0, 0, ""},
  { "FAST", 0, 0, 0, ""},
  { "FETCH", 0, 0, 0, ""},
  { "FIELDS", 0, 0, 0, ""},
  { "FILE", 0, 0, 0, ""},
  { "FIRST", 0, 0, 0, ""},
  { "FIXED", 0, 0, 0, ""},
  { "FLOAT", 0, 0, 0, ""},
  { "FLOAT4", 0, 0, 0, ""},
  { "FLOAT8", 0, 0, 0, ""},
  { "FLUSH", 0, 0, 0, ""},
  { "FOR", 0, 0, 0, ""},
  { "FORCE", 0, 0, 0, ""},
  { "FOREIGN", 0, 0, 0, ""},
  { "FOUND", 0, 0, 0, ""},
  { "FRAC_SECOND", 0, 0, 0, ""},
  { "FROM", 0, 0, 0, ""},
  { "FULL", 0, 0, 0, ""},
  { "FULLTEXT", 0, 0, 0, ""},
  { "FUNCTION", 0, 0, 0, ""},
  { "GLOBAL", 0, 0, 0, ""},
  { "GRANT", 0, 0, 0, ""},
  { "GRANTS", 0, 0, 0, ""},
  { "GROUP", 0, 0, 0, ""},
  { "HANDLER", 0, 0, 0, ""},
  { "HASH", 0, 0, 0, ""},
  { "HAVING", 0, 0, 0, ""},
  { "HELP", 0, 0, 0, ""},
  { "HIGH_PRIORITY", 0, 0, 0, ""},
  { "HOSTS", 0, 0, 0, ""},
  { "HOUR", 0, 0, 0, ""},
  { "HOUR_MICROSECOND", 0, 0, 0, ""},
  { "HOUR_MINUTE", 0, 0, 0, ""},
  { "HOUR_SECOND", 0, 0, 0, ""},
  { "IDENTIFIED", 0, 0, 0, ""},
  { "IF", 0, 0, 0, ""},
  { "IGNORE", 0, 0, 0, ""},
  { "IMPORT", 0, 0, 0, ""},
  { "IN", 0, 0, 0, ""},
  { "INDEX", 0, 0, 0, ""},
  { "INDEXES", 0, 0, 0, ""},
  { "INFILE", 0, 0, 0, ""},
  { "INNER", 0, 0, 0, ""},
  { "INNOBASE", 0, 0, 0, ""},
  { "INNODB", 0, 0, 0, ""},
  { "INOUT", 0, 0, 0, ""},
  { "INSENSITIVE", 0, 0, 0, ""},
  { "INSERT", 0, 0, 0, ""},
  { "INSERT_METHOD", 0, 0, 0, ""},
  { "INT", 0, 0, 0, ""},
  { "INT1", 0, 0, 0, ""},
  { "INT2", 0, 0, 0, ""},
  { "INT3", 0, 0, 0, ""},
  { "INT4", 0, 0, 0, ""},
  { "INT8", 0, 0, 0, ""},
  { "INTEGER", 0, 0, 0, ""},
  { "INTERVAL", 0, 0, 0, ""},
  { "INTO", 0, 0, 0, ""},
  { "IO_THREAD", 0, 0, 0, ""},
  { "IS", 0, 0, 0, ""},
  { "ISOLATION", 0, 0, 0, ""},
  { "ISSUER", 0, 0, 0, ""},
  { "ITERATE", 0, 0, 0, ""},
  { "INVOKER", 0, 0, 0, ""},
  { "JOIN", 0, 0, 0, ""},
  { "KEY", 0, 0, 0, ""},
  { "KEYS", 0, 0, 0, ""},
  { "KILL", 0, 0, 0, ""},
  { "LANGUAGE", 0, 0, 0, ""},
  { "LAST", 0, 0, 0, ""},
  { "LEADING", 0, 0, 0, ""},
  { "LEAVE", 0, 0, 0, ""},
  { "LEAVES", 0, 0, 0, ""},
  { "LEFT", 0, 0, 0, ""},
  { "LEVEL", 0, 0, 0, ""},
  { "LIKE", 0, 0, 0, ""},
  { "LIMIT", 0, 0, 0, ""},
  { "LINES", 0, 0, 0, ""},
  { "LINESTRING", 0, 0, 0, ""},
  { "LOAD", 0, 0, 0, ""},
  { "LOCAL", 0, 0, 0, ""},
  { "LOCALTIMESTAMP", 0, 0, 0, ""},
  { "LOCK", 0, 0, 0, ""},
  { "LOCKS", 0, 0, 0, ""},
  { "LOGS", 0, 0, 0, ""},
  { "LONG", 0, 0, 0, ""},
  { "LONGTEXT", 0, 0, 0, ""},
  { "LOOP", 0, 0, 0, ""},
  { "LOW_PRIORITY", 0, 0, 0, ""},
  { "MASTER", 0, 0, 0, ""},
  { "MASTER_CONNECT_RETRY", 0, 0, 0, ""},
  { "MASTER_HOST", 0, 0, 0, ""},
  { "MASTER_LOG_FILE", 0, 0, 0, ""},
  { "MASTER_LOG_POS", 0, 0, 0, ""},
  { "MASTER_PASSWORD", 0, 0, 0, ""},
  { "MASTER_PORT", 0, 0, 0, ""},
  { "MASTER_SERVER_ID", 0, 0, 0, ""},
  { "MASTER_SSL", 0, 0, 0, ""},
  { "MASTER_SSL_CA", 0, 0, 0, ""},
  { "MASTER_SSL_CAPATH", 0, 0, 0, ""},
  { "MASTER_SSL_CERT", 0, 0, 0, ""},
  { "MASTER_SSL_CIPHER", 0, 0, 0, ""},
  { "MASTER_SSL_KEY", 0, 0, 0, ""},
  { "MASTER_USER", 0, 0, 0, ""},
  { "MATCH", 0, 0, 0, ""},
  { "MAX_CONNECTIONS_PER_HOUR", 0, 0, 0, ""},
  { "MAX_QUERIES_PER_HOUR", 0, 0, 0, ""},
  { "MAX_ROWS", 0, 0, 0, ""},
  { "MAX_UPDATES_PER_HOUR", 0, 0, 0, ""},
  { "MAX_USER_CONNECTIONS", 0, 0, 0, ""},
  { "MEDIUM", 0, 0, 0, ""},
  { "MEDIUMTEXT", 0, 0, 0, ""},
  { "MERGE", 0, 0, 0, ""},
  { "MICROSECOND", 0, 0, 0, ""},
  { "MIDDLEINT", 0, 0, 0, ""},
  { "MIGRATE", 0, 0, 0, ""},
  { "MINUTE", 0, 0, 0, ""},
  { "MINUTE_MICROSECOND", 0, 0, 0, ""},
  { "MINUTE_SECOND", 0, 0, 0, ""},
  { "MIN_ROWS", 0, 0, 0, ""},
  { "MOD", 0, 0, 0, ""},
  { "MODE", 0, 0, 0, ""},
  { "MODIFIES", 0, 0, 0, ""},
  { "MODIFY", 0, 0, 0, ""},
  { "MONTH", 0, 0, 0, ""},
  { "MULTILINESTRING", 0, 0, 0, ""},
  { "MULTIPOINT", 0, 0, 0, ""},
  { "MULTIPOLYGON", 0, 0, 0, ""},
  { "MUTEX", 0, 0, 0, ""},
  { "NAME", 0, 0, 0, ""},
  { "NAMES", 0, 0, 0, ""},
  { "NATIONAL", 0, 0, 0, ""},
  { "NATURAL", 0, 0, 0, ""},
  { "NDB", 0, 0, 0, ""},
  { "NDBCLUSTER", 0, 0, 0, ""},
  { "NCHAR", 0, 0, 0, ""},
  { "NEW", 0, 0, 0, ""},
  { "NEXT", 0, 0, 0, ""},
  { "NO", 0, 0, 0, ""},
  { "NONE", 0, 0, 0, ""},
  { "NOT", 0, 0, 0, ""},
  { "NO_WRITE_TO_BINLOG", 0, 0, 0, ""},
  { "NULL", 0, 0, 0, ""},
  { "NUMERIC", 0, 0, 0, ""},
  { "NVARCHAR", 0, 0, 0, ""},
  { "OFFSET", 0, 0, 0, ""},
  { "OLD_PASSWORD", 0, 0, 0, ""},
  { "ON", 0, 0, 0, ""},
  { "ONE", 0, 0, 0, ""},
  { "ONE_SHOT", 0, 0, 0, ""},
  { "OPEN", 0, 0, 0, ""},
  { "OPTIMIZE", 0, 0, 0, ""},
  { "OPTION", 0, 0, 0, ""},
  { "OPTIONALLY", 0, 0, 0, ""},
  { "OR", 0, 0, 0, ""},
  { "ORDER", 0, 0, 0, ""},
  { "OUT", 0, 0, 0, ""},
  { "OUTER", 0, 0, 0, ""},
  { "OUTFILE", 0, 0, 0, ""},
  { "PACK_KEYS", 0, 0, 0, ""},
  { "PARTIAL", 0, 0, 0, ""},
  { "PASSWORD", 0, 0, 0, ""},
  { "PHASE", 0, 0, 0, ""},
  { "POINT", 0, 0, 0, ""},
  { "POLYGON", 0, 0, 0, ""},
  { "PRECISION", 0, 0, 0, ""},
  { "PREPARE", 0, 0, 0, ""},
  { "PREV", 0, 0, 0, ""},
  { "PRIMARY", 0, 0, 0, ""},
  { "PRIVILEGES", 0, 0, 0, ""},
  { "PROCEDURE", 0, 0, 0, ""},
  { "PROCESS", 0, 0, 0, ""},
  { "PROCESSLIST", 0, 0, 0, ""},
  { "PURGE", 0, 0, 0, ""},
  { "QUARTER", 0, 0, 0, ""},
  { "QUERY", 0, 0, 0, ""},
  { "QUICK", 0, 0, 0, ""},
  { "READ", 0, 0, 0, ""},
  { "READS", 0, 0, 0, ""},
  { "REAL", 0, 0, 0, ""},
  { "RECOVER", 0, 0, 0, ""},
  { "REDUNDANT", 0, 0, 0, ""},
  { "REFERENCES", 0, 0, 0, ""},
  { "REGEXP", 0, 0, 0, ""},
  { "RELAY_LOG_FILE", 0, 0, 0, ""},
  { "RELAY_LOG_POS", 0, 0, 0, ""},
  { "RELAY_THREAD", 0, 0, 0, ""},
  { "RELEASE", 0, 0, 0, ""},
  { "RELOAD", 0, 0, 0, ""},
  { "RENAME", 0, 0, 0, ""},
  { "REPAIR", 0, 0, 0, ""},
  { "REPEATABLE", 0, 0, 0, ""},
  { "REPLACE", 0, 0, 0, ""},
  { "REPLICATION", 0, 0, 0, ""},
  { "REPEAT", 0, 0, 0, ""},
  { "REQUIRE", 0, 0, 0, ""},
  { "RESET", 0, 0, 0, ""},
  { "RESTORE", 0, 0, 0, ""},
  { "RESTRICT", 0, 0, 0, ""},
  { "RESUME", 0, 0, 0, ""},
  { "RETURN", 0, 0, 0, ""},
  { "RETURNS", 0, 0, 0, ""},
  { "REVOKE", 0, 0, 0, ""},
  { "RIGHT", 0, 0, 0, ""},
  { "RLIKE", 0, 0, 0, ""},
  { "ROLLBACK", 0, 0, 0, ""},
  { "ROLLUP", 0, 0, 0, ""},
  { "ROUTINE", 0, 0, 0, ""},
  { "ROW", 0, 0, 0, ""},
  { "ROWS", 0, 0, 0, ""},
  { "ROW_FORMAT", 0, 0, 0, ""},
  { "RTREE", 0, 0, 0, ""},
  { "SAVEPOINT", 0, 0, 0, ""},
  { "SCHEMA", 0, 0, 0, ""},
  { "SCHEMAS", 0, 0, 0, ""},
  { "SECOND", 0, 0, 0, ""},
  { "SECOND_MICROSECOND", 0, 0, 0, ""},
  { "SECURITY", 0, 0, 0, ""},
  { "SELECT", 0, 0, 0, ""},
  { "SENSITIVE", 0, 0, 0, ""},
  { "SEPARATOR", 0, 0, 0, ""},
  { "SERIAL", 0, 0, 0, ""},
  { "SERIALIZABLE", 0, 0, 0, ""},
  { "SESSION", 0, 0, 0, ""},
  { "SET", 0, 0, 0, ""},
  { "SHARE", 0, 0, 0, ""},
  { "SHOW", 0, 0, 0, ""},
  { "SHUTDOWN", 0, 0, 0, ""},
  { "SIGNED", 0, 0, 0, ""},
  { "SIMPLE", 0, 0, 0, ""},
  { "SLAVE", 0, 0, 0, ""},
  { "SNAPSHOT", 0, 0, 0, ""},
  { "SMALLINT", 0, 0, 0, ""},
  { "SOME", 0, 0, 0, ""},
  { "SONAME", 0, 0, 0, ""},
  { "SOUNDS", 0, 0, 0, ""},
  { "SPATIAL", 0, 0, 0, ""},
  { "SPECIFIC", 0, 0, 0, ""},
  { "SQL", 0, 0, 0, ""},
  { "SQLEXCEPTION", 0, 0, 0, ""},
  { "SQLSTATE", 0, 0, 0, ""},
  { "SQLWARNING", 0, 0, 0, ""},
  { "SQL_BIG_RESULT", 0, 0, 0, ""},
  { "SQL_BUFFER_RESULT", 0, 0, 0, ""},
  { "SQL_CACHE", 0, 0, 0, ""},
  { "SQL_CALC_FOUND_ROWS", 0, 0, 0, ""},
  { "SQL_NO_CACHE", 0, 0, 0, ""},
  { "SQL_SMALL_RESULT", 0, 0, 0, ""},
  { "SQL_THREAD", 0, 0, 0, ""},
  { "SQL_TSI_FRAC_SECOND", 0, 0, 0, ""},
  { "SQL_TSI_SECOND", 0, 0, 0, ""},
  { "SQL_TSI_MINUTE", 0, 0, 0, ""},
  { "SQL_TSI_HOUR", 0, 0, 0, ""},
  { "SQL_TSI_DAY", 0, 0, 0, ""},
  { "SQL_TSI_WEEK", 0, 0, 0, ""},
  { "SQL_TSI_MONTH", 0, 0, 0, ""},
  { "SQL_TSI_QUARTER", 0, 0, 0, ""},
  { "SQL_TSI_YEAR", 0, 0, 0, ""},
  { "SSL", 0, 0, 0, ""},
  { "START", 0, 0, 0, ""},
  { "STARTING", 0, 0, 0, ""},
  { "STATUS", 0, 0, 0, ""},
  { "STOP", 0, 0, 0, ""},
  { "STORAGE", 0, 0, 0, ""},
  { "STRAIGHT_JOIN", 0, 0, 0, ""},
  { "STRING", 0, 0, 0, ""},
  { "STRIPED", 0, 0, 0, ""},
  { "SUBJECT", 0, 0, 0, ""},
  { "SUPER", 0, 0, 0, ""},
  { "SUSPEND", 0, 0, 0, ""},
  { "TABLE", 0, 0, 0, ""},
  { "TABLES", 0, 0, 0, ""},
  { "TABLESPACE", 0, 0, 0, ""},
  { "TEMPORARY", 0, 0, 0, ""},
  { "TEMPTABLE", 0, 0, 0, ""},
  { "TERMINATED", 0, 0, 0, ""},
  { "TEXT", 0, 0, 0, ""},
  { "THEN", 0, 0, 0, ""},
  { "TIMESTAMP", 0, 0, 0, ""},
  { "TIMESTAMPADD", 0, 0, 0, ""},
  { "TIMESTAMPDIFF", 0, 0, 0, ""},
  { "TINYTEXT", 0, 0, 0, ""},
  { "TO", 0, 0, 0, ""},
  { "TRAILING", 0, 0, 0, ""},
  { "TRANSACTION", 0, 0, 0, ""},
  { "TRIGGER", 0, 0, 0, ""},
  { "TRIGGERS", 0, 0, 0, ""},
  { "TRUE", 0, 0, 0, ""},
  { "TRUNCATE", 0, 0, 0, ""},
  { "TYPE", 0, 0, 0, ""},
  { "TYPES", 0, 0, 0, ""},
  { "UNCOMMITTED", 0, 0, 0, ""},
  { "UNDEFINED", 0, 0, 0, ""},
  { "UNDO", 0, 0, 0, ""},
  { "UNICODE", 0, 0, 0, ""},
  { "UNION", 0, 0, 0, ""},
  { "UNIQUE", 0, 0, 0, ""},
  { "UNKNOWN", 0, 0, 0, ""},
  { "UNLOCK", 0, 0, 0, ""},
  { "UNSIGNED", 0, 0, 0, ""},
  { "UNTIL", 0, 0, 0, ""},
  { "UPDATE", 0, 0, 0, ""},
  { "UPGRADE", 0, 0, 0, ""},
  { "USAGE", 0, 0, 0, ""},
  { "USE", 0, 0, 0, ""},
  { "USER", 0, 0, 0, ""},
  { "USER_RESOURCES", 0, 0, 0, ""},
  { "USE_FRM", 0, 0, 0, ""},
  { "USING", 0, 0, 0, ""},
  { "UTC_DATE", 0, 0, 0, ""},
  { "UTC_TIMESTAMP", 0, 0, 0, ""},
  { "VALUE", 0, 0, 0, ""},
  { "VALUES", 0, 0, 0, ""},
  { "VARBINARY", 0, 0, 0, ""},
  { "VARCHAR", 0, 0, 0, ""},
  { "VARCHARACTER", 0, 0, 0, ""},
  { "VARIABLES", 0, 0, 0, ""},
  { "VARYING", 0, 0, 0, ""},
  { "WARNINGS", 0, 0, 0, ""},
  { "WEEK", 0, 0, 0, ""},
  { "WHEN", 0, 0, 0, ""},
  { "WHERE", 0, 0, 0, ""},
  { "WHILE", 0, 0, 0, ""},
  { "VIEW", 0, 0, 0, ""},
  { "WITH", 0, 0, 0, ""},
  { "WORK", 0, 0, 0, ""},
  { "WRITE", 0, 0, 0, ""},
  { "X509", 0, 0, 0, ""},
  { "XOR", 0, 0, 0, ""},
  { "XA", 0, 0, 0, ""},
  { "YEAR", 0, 0, 0, ""},
  { "YEAR_MONTH", 0, 0, 0, ""},
  { "ZEROFILL", 0, 0, 0, ""},
  { "ABS", 0, 0, 0, ""},
  { "ACOS", 0, 0, 0, ""},
  { "ADDDATE", 0, 0, 0, ""},
  { "AES_ENCRYPT", 0, 0, 0, ""},
  { "AES_DECRYPT", 0, 0, 0, ""},
  { "AREA", 0, 0, 0, ""},
  { "ASIN", 0, 0, 0, ""},
  { "ASBINARY", 0, 0, 0, ""},
  { "ASTEXT", 0, 0, 0, ""},
  { "ASWKB", 0, 0, 0, ""},
  { "ASWKT", 0, 0, 0, ""},
  { "ATAN", 0, 0, 0, ""},
  { "ATAN2", 0, 0, 0, ""},
  { "BENCHMARK", 0, 0, 0, ""},
  { "BIN", 0, 0, 0, ""},
  { "BIT_OR", 0, 0, 0, ""},
  { "BIT_AND", 0, 0, 0, ""},
  { "BIT_XOR", 0, 0, 0, ""},
  { "CAST", 0, 0, 0, ""},
  { "CEIL", 0, 0, 0, ""},
  { "CEILING", 0, 0, 0, ""},
  { "CENTROID", 0, 0, 0, ""},
  { "CHAR_LENGTH", 0, 0, 0, ""},
  { "CHARACTER_LENGTH", 0, 0, 0, ""},
  { "COALESCE", 0, 0, 0, ""},
  { "COERCIBILITY", 0, 0, 0, ""},
  { "COMPRESS", 0, 0, 0, ""},
  { "CONCAT", 0, 0, 0, ""},
  { "CONCAT_WS", 0, 0, 0, ""},
  { "CONNECTION_ID", 0, 0, 0, ""},
  { "CONV", 0, 0, 0, ""},
  { "CONVERT_TZ", 0, 0, 0, ""},
  { "COUNT", 0, 0, 0, ""},
  { "COS", 0, 0, 0, ""},
  { "COT", 0, 0, 0, ""},
  { "CRC32", 0, 0, 0, ""},
  { "CROSSES", 0, 0, 0, ""},
  { "CURDATE", 0, 0, 0, ""},
  { "DATE_ADD", 0, 0, 0, ""},
  { "DATEDIFF", 0, 0, 0, ""},
  { "DATE_FORMAT", 0, 0, 0, ""},
  { "DATE_SUB", 0, 0, 0, ""},
  { "DAYNAME", 0, 0, 0, ""},
  { "DAYOFMONTH", 0, 0, 0, ""},
  { "DAYOFWEEK", 0, 0, 0, ""},
  { "DAYOFYEAR", 0, 0, 0, ""},
  { "DECODE", 0, 0, 0, ""},
  { "DEGREES", 0, 0, 0, ""},
  { "DES_ENCRYPT", 0, 0, 0, ""},
  { "DES_DECRYPT", 0, 0, 0, ""},
  { "DIMENSION", 0, 0, 0, ""},
  { "DISJOINT", 0, 0, 0, ""},
  { "ELT", 0, 0, 0, ""},
  { "ENCODE", 0, 0, 0, ""},
  { "ENCRYPT", 0, 0, 0, ""},
  { "ENDPOINT", 0, 0, 0, ""},
  { "ENVELOPE", 0, 0, 0, ""},
  { "EQUALS", 0, 0, 0, ""},
  { "EXTERIORRING", 0, 0, 0, ""},
  { "EXTRACT", 0, 0, 0, ""},
  { "EXP", 0, 0, 0, ""},
  { "EXPORT_SET", 0, 0, 0, ""},
  { "FIELD", 0, 0, 0, ""},
  { "FIND_IN_SET", 0, 0, 0, ""},
  { "FLOOR", 0, 0, 0, ""},
  { "FORMAT", 0, 0, 0, ""},
  { "FOUND_ROWS", 0, 0, 0, ""},
  { "FROM_DAYS", 0, 0, 0, ""},
  { "FROM_UNIXTIME", 0, 0, 0, ""},
  { "GET_LOCK", 0, 0, 0, ""},
  { "GLENGTH", 0, 0, 0, ""},
  { "GREATEST", 0, 0, 0, ""},
  { "GROUP_CONCAT", 0, 0, 0, ""},
  { "GROUP_UNIQUE_USERS", 0, 0, 0, ""},
  { "HEX", 0, 0, 0, ""},
  { "IFNULL", 0, 0, 0, ""},
  { "INET_ATON", 0, 0, 0, ""},
  { "INET_NTOA", 0, 0, 0, ""},
  { "INSTR", 0, 0, 0, ""},
  { "INTERIORRINGN", 0, 0, 0, ""},
  { "INTERSECTS", 0, 0, 0, ""},
  { "ISCLOSED", 0, 0, 0, ""},
  { "ISEMPTY", 0, 0, 0, ""},
  { "ISNULL", 0, 0, 0, ""},
  { "IS_FREE_LOCK", 0, 0, 0, ""},
  { "IS_USED_LOCK", 0, 0, 0, ""},
  { "LAST_INSERT_ID", 0, 0, 0, ""},
  { "ISSIMPLE", 0, 0, 0, ""},
  { "LAST_DAY", 0, 0, 0, ""},
  { "LCASE", 0, 0, 0, ""},
  { "LEAST", 0, 0, 0, ""},
  { "LENGTH", 0, 0, 0, ""},
  { "LN", 0, 0, 0, ""},
  { "LINEFROMTEXT", 0, 0, 0, ""},
  { "LINEFROMWKB", 0, 0, 0, ""},
  { "LINESTRINGFROMTEXT", 0, 0, 0, ""},
  { "LINESTRINGFROMWKB", 0, 0, 0, ""},
  { "LOAD_FILE", 0, 0, 0, ""},
  { "LOCATE", 0, 0, 0, ""},
  { "LOG", 0, 0, 0, ""},
  { "LOG2", 0, 0, 0, ""},
  { "LOG10", 0, 0, 0, ""},
  { "LOWER", 0, 0, 0, ""},
  { "LPAD", 0, 0, 0, ""},
  { "LTRIM", 0, 0, 0, ""},
  { "MAKE_SET", 0, 0, 0, ""},
  { "MAKEDATE", 0, 0, 0, ""},
  { "MASTER_POS_WAIT", 0, 0, 0, ""},
  { "MAX", 0, 0, 0, ""},
  { "MBRCONTAINS", 0, 0, 0, ""},
  { "MBRDISJOINT", 0, 0, 0, ""},
  { "MBREQUAL", 0, 0, 0, ""},
  { "MBRINTERSECTS", 0, 0, 0, ""},
  { "MBROVERLAPS", 0, 0, 0, ""},
  { "MBRTOUCHES", 0, 0, 0, ""},
  { "MBRWITHIN", 0, 0, 0, ""},
  { "MD5", 0, 0, 0, ""},
  { "MID", 0, 0, 0, ""},
  { "MIN", 0, 0, 0, ""},
  { "MLINEFROMTEXT", 0, 0, 0, ""},
  { "MLINEFROMWKB", 0, 0, 0, ""},
  { "MPOINTFROMTEXT", 0, 0, 0, ""},
  { "MPOINTFROMWKB", 0, 0, 0, ""},
  { "MPOLYFROMTEXT", 0, 0, 0, ""},
  { "MPOLYFROMWKB", 0, 0, 0, ""},
  { "MONTHNAME", 0, 0, 0, ""},
  { "MULTILINESTRINGFROMTEXT", 0, 0, 0, ""},
  { "MULTILINESTRINGFROMWKB", 0, 0, 0, ""},
  { "MULTIPOINTFROMTEXT", 0, 0, 0, ""},
  { "MULTIPOINTFROMWKB", 0, 0, 0, ""},
  { "MULTIPOLYGONFROMTEXT", 0, 0, 0, ""},
  { "MULTIPOLYGONFROMWKB", 0, 0, 0, ""},
  { "NAME_CONST", 0, 0, 0, ""},
  { "NOW", 0, 0, 0, ""},
  { "NULLIF", 0, 0, 0, ""},
  { "NUMINTERIORRINGS", 0, 0, 0, ""},
  { "NUMPOINTS", 0, 0, 0, ""},
  { "OCTET_LENGTH", 0, 0, 0, ""},
  { "OCT", 0, 0, 0, ""},
  { "ORD", 0, 0, 0, ""},
  { "OVERLAPS", 0, 0, 0, ""},
  { "PERIOD_ADD", 0, 0, 0, ""},
  { "PERIOD_DIFF", 0, 0, 0, ""},
  { "PI", 0, 0, 0, ""},
  { "POINTFROMTEXT", 0, 0, 0, ""},
  { "POINTFROMWKB", 0, 0, 0, ""},
  { "POINTN", 0, 0, 0, ""},
  { "POLYFROMTEXT", 0, 0, 0, ""},
  { "POLYFROMWKB", 0, 0, 0, ""},
  { "POLYGONFROMTEXT", 0, 0, 0, ""},
  { "POLYGONFROMWKB", 0, 0, 0, ""},
  { "POSITION", 0, 0, 0, ""},
  { "POW", 0, 0, 0, ""},
  { "POWER", 0, 0, 0, ""},
  { "QUOTE", 0, 0, 0, ""},
  { "RADIANS", 0, 0, 0, ""},
  { "RAND", 0, 0, 0, ""},
  { "RELEASE_LOCK", 0, 0, 0, ""},
  { "REVERSE", 0, 0, 0, ""},
  { "ROUND", 0, 0, 0, ""},
  { "ROW_COUNT", 0, 0, 0, ""},
  { "RPAD", 0, 0, 0, ""},
  { "RTRIM", 0, 0, 0, ""},
  { "SESSION_USER", 0, 0, 0, ""},
  { "SUBDATE", 0, 0, 0, ""},
  { "SIGN", 0, 0, 0, ""},
  { "SIN", 0, 0, 0, ""},
  { "SHA", 0, 0, 0, ""},
  { "SHA1", 0, 0, 0, ""},
  { "SLEEP", 0, 0, 0, ""},
  { "SOUNDEX", 0, 0, 0, ""},
  { "SPACE", 0, 0, 0, ""},
  { "SQRT", 0, 0, 0, ""},
  { "SRID", 0, 0, 0, ""},
  { "STARTPOINT", 0, 0, 0, ""},
  { "STD", 0, 0, 0, ""},
  { "STDDEV", 0, 0, 0, ""},
  { "STDDEV_POP", 0, 0, 0, ""},
  { "STDDEV_SAMP", 0, 0, 0, ""},
  { "STR_TO_DATE", 0, 0, 0, ""},
  { "STRCMP", 0, 0, 0, ""},
  { "SUBSTR", 0, 0, 0, ""},
  { "SUBSTRING", 0, 0, 0, ""},
  { "SUBSTRING_INDEX", 0, 0, 0, ""},
  { "SUM", 0, 0, 0, ""},
  { "SYSDATE", 0, 0, 0, ""},
  { "SYSTEM_USER", 0, 0, 0, ""},
  { "TAN", 0, 0, 0, ""},
  { "TIME_FORMAT", 0, 0, 0, ""},
  { "TO_DAYS", 0, 0, 0, ""},
  { "TOUCHES", 0, 0, 0, ""},
  { "TRIM", 0, 0, 0, ""},
  { "UCASE", 0, 0, 0, ""},
  { "UNCOMPRESS", 0, 0, 0, ""},
  { "UNCOMPRESSED_LENGTH", 0, 0, 0, ""},
  { "UNHEX", 0, 0, 0, ""},
  { "UNIQUE_USERS", 0, 0, 0, ""},
  { "UNIX_TIMESTAMP", 0, 0, 0, ""},
  { "UPPER", 0, 0, 0, ""},
  { "UUID", 0, 0, 0, ""},
  { "VARIANCE", 0, 0, 0, ""},
  { "VAR_POP", 0, 0, 0, ""},
  { "VAR_SAMP", 0, 0, 0, ""},
  { "VERSION", 0, 0, 0, ""},
  { "WEEKDAY", 0, 0, 0, ""},
  { "WEEKOFYEAR", 0, 0, 0, ""},
  { "WITHIN", 0, 0, 0, ""},
  { "X", 0, 0, 0, ""},
  { "Y", 0, 0, 0, ""},
  { "YEARWEEK", 0, 0, 0, ""},
  /* end sentinel */
  { (char *)NULL,       0, 0, 0, ""}
};

static const char *load_default_groups[]= { "drizzle","client",0 };

int history_length;
static int not_in_history(const char *line);
static void initialize_readline (char *name);
static void fix_history(string *final_command);

static COMMANDS *find_command(const char *name,char cmd_name);
static bool add_line(string *buffer,char *line,char *in_string,
                     bool *ml_comment);
static void remove_cntrl(string *buffer);
static void print_table_data(drizzle_result_st *result);
static void print_tab_data(drizzle_result_st *result);
static void print_table_data_vertically(drizzle_result_st *result);
static void print_warnings(uint32_t error_code);
static uint32_t start_timer(void);
static void end_timer(uint32_t start_time,char *buff);
static void drizzle_end_timer(uint32_t start_time,char *buff);
static void nice_time(double sec,char *buff,bool part_second);
extern "C" void drizzle_end(int sig);
extern "C" void handle_sigint(int sig);
#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
static void window_resize(int sig);
#endif

/**
  Shutdown the server that we are currently connected to.

  @retval
    true success
  @retval
    false failure
*/
static bool server_shutdown(void)
{
  drizzle_result_st result;
  drizzle_return_t ret;

  if (verbose)
  {
    printf("shutting down drizzled");
    if (opt_drizzle_port > 0)
      printf(" on port %d", opt_drizzle_port);
    printf("... ");
  }

  if (drizzle_shutdown(&con, &result, DRIZZLE_SHUTDOWN_DEFAULT,
                       &ret) == NULL || ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, "shutdown failed; error: '%s'",
              drizzle_result_error(&result));
      drizzle_result_free(&result);
    }
    else
    {
      fprintf(stderr, "shutdown failed; error: '%s'",
              drizzle_con_error(&con));
    }
    return false;
  }

  drizzle_result_free(&result);

  if (verbose)
    printf("done\n");

  return true;
}

/**
  Ping the server that we are currently connected to.

  @retval
    true success
  @retval
    false failure
*/
static bool server_ping(void)
{
  drizzle_result_st result;
  drizzle_return_t ret;

  if (drizzle_ping(&con, &result, &ret) != NULL && ret == DRIZZLE_RETURN_OK)
  {
    if (opt_silent < 2)
      printf("drizzled is alive\n");
  }
  else
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, "ping failed; error: '%s'",
              drizzle_result_error(&result));
      drizzle_result_free(&result);
    }
    else
    {
      fprintf(stderr, "drizzled won't answer to ping, error: '%s'",
              drizzle_con_error(&con));
    }
    return false;
  }
  drizzle_result_free(&result);
  return true;
}

/**
  Execute command(s) specified by the user.

  @param error  error status of command execution.
                If an error had occurred, this variable will be set
                to 1 whereas on success, it shall be set to 0. This
                value will be supplied to the exit() function used
                by the caller.

  @retval
    false no commands were executed
  @retval
    true  at least one command was executed
*/
static bool execute_commands(int *error)
{
  bool executed= false;
  *error= 0;

  if (opt_ping)
  {
    if (server_ping() == false)
      *error= 1;
    executed= true;
  }

  if (opt_shutdown)
  {
    if (server_shutdown() == false)
      *error= 1;
    executed= true;
  }
  return executed;
}

int main(int argc,char *argv[])
{
#if defined(ENABLE_NLS)
# if defined(HAVE_LOCALE_H)
  setlocale(LC_ALL, "");
# endif
  bindtextdomain("drizzle", LOCALEDIR);
  textdomain("drizzle");
#endif

  MY_INIT(argv[0]);
  delimiter_str= delimiter;
  default_prompt= strdup(getenv("DRIZZLE_PS1") ?
                         getenv("DRIZZLE_PS1") :
                         "drizzle> ");
  
  if (default_prompt == NULL)
  {
    fprintf(stderr, _("Memory allocation error while constructing initial "
                      "prompt. Aborting.\n"));
    exit(ENOMEM);
  }
  current_prompt= strdup(default_prompt);
  if (current_prompt == NULL)
  {
    fprintf(stderr, _("Memory allocation error while constructing initial "
                      "prompt. Aborting.\n"));
    exit(ENOMEM);
  }
  processed_prompt= new string();
  processed_prompt->reserve(32);

  prompt_counter=0;

  outfile[0]=0;      // no (default) outfile
  strcpy(pager, "stdout");  // the default, if --pager wasn't given
  {
    char *tmp=getenv("PAGER");
    if (tmp && strlen(tmp))
    {
      default_pager_set= 1;
      strcpy(default_pager, tmp);
    }
  }
  if (!isatty(0) || !isatty(1))
  {
    status.batch=1; opt_silent=1;
    ignore_errors=0;
  }
  else
    status.add_to_history=1;
  status.exit_status=1;

  {
    /*
      The file descriptor-layer may be out-of-sync with the file-number layer,
      so we make sure that "stdout" is really open.  If its file is closed then
      explicitly close the FD layer.
    */
    int stdout_fileno_copy;
    stdout_fileno_copy= dup(fileno(stdout)); /* Okay if fileno fails. */
    if (stdout_fileno_copy == -1)
      fclose(stdout);
    else
      close(stdout_fileno_copy);             /* Clean up dup(). */
  }

  internal::load_defaults("drizzle",load_default_groups,&argc,&argv);
  defaults_argv=argv;
  if (get_options(argc, (char **) argv))
  {
    internal::free_defaults(defaults_argv);
    internal::my_end();
    exit(1);
  }

  memset(&drizzle, 0, sizeof(drizzle));
  if (sql_connect(current_host,current_db,current_user,opt_password,
                  opt_silent))
  {
    quick= 1;          // Avoid history
    status.exit_status= 1;
    drizzle_end(-1);
  }

  int command_error;
  if (execute_commands(&command_error) != false)
  {
    /* we've executed a command so exit before we go into readline mode */
    internal::free_defaults(defaults_argv);
    internal::my_end();
    exit(command_error);
  }

  if (status.batch && !status.line_buff)
  {
    status.line_buff= new(std::nothrow) LineBuffer(opt_max_input_line, stdin);
    if (status.line_buff == NULL)
    {
      internal::free_defaults(defaults_argv);
      internal::my_end();
      exit(1);
    }
  }

  if (!status.batch)
    ignore_errors=1;        // Don't abort monitor

  if (opt_sigint_ignore)
    signal(SIGINT, SIG_IGN);
  else
    signal(SIGINT, handle_sigint);              // Catch SIGINT to clean up
  signal(SIGQUIT, drizzle_end);      // Catch SIGQUIT to clean up

#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
  /* Readline will call this if it installs a handler */
  signal(SIGWINCH, window_resize);
  /* call the SIGWINCH handler to get the default term width */
  window_resize(0);
#endif

  put_info(_("Welcome to the Drizzle client..  Commands end with ; or \\g."),
           INFO_INFO,0,0);

  glob_buffer= new string();
  glob_buffer->reserve(512);

	size_t output_buff_size = 512;
  char * output_buff= (char *)malloc(output_buff_size);
  memset(output_buff, '\0', output_buff_size);

  snprintf(output_buff, output_buff_size,
          _("Your Drizzle connection id is %u\nServer version: %s\n"),
          drizzle_con_thread_id(&con),
          server_version_string(&con));
  put_info(output_buff, INFO_INFO, 0, 0);

  initialize_readline(current_prompt);
  if (!status.batch && !quick)
  {
    /* read-history from file, default ~/.drizzle_history*/
    if (getenv("DRIZZLE_HISTFILE"))
      histfile= strdup(getenv("DRIZZLE_HISTFILE"));
    else if (getenv("HOME"))
    {
			size_t histfile_size = strlen(getenv("HOME")) +
				strlen("/.drizzle_history") + 2;
      histfile=(char*) malloc(histfile_size);
      if (histfile)
        snprintf(histfile, histfile_size, "%s/.drizzle_history",getenv("HOME"));
      char link_name[FN_REFLEN];
      ssize_t sym_link_size= readlink(histfile,link_name,FN_REFLEN-1);
      if (sym_link_size >= 0)
      {
        link_name[sym_link_size]= '\0';
        if (strncmp(link_name, "/dev/null", 10) == 0)
        {
          /* The .drizzle_history file is a symlink to /dev/null, don't use it */
          free(histfile);
          histfile= 0;
        }
      }
    }
    if (histfile)
    {
      if (verbose)
        tee_fprintf(stdout, _("Reading history-file %s\n"),histfile);
      read_history(histfile);
			size_t histfile_tmp_size = strlen(histfile) + 5;
      if (!(histfile_tmp= (char*) malloc(histfile_tmp_size)))
      {
        fprintf(stderr, _("Couldn't allocate memory for temp histfile!\n"));
        exit(1);
      }
      snprintf(histfile_tmp, histfile_tmp_size, "%s.TMP", histfile);
    }
  }

  put_info(_("Type 'help;' or '\\h' for help. "
             "Type '\\c' to clear the buffer.\n"),INFO_INFO,0,0);
  status.exit_status= read_and_execute(!status.batch);
  if (opt_outfile)
    end_tee();
  drizzle_end(0);

  return(0);        // Keep compiler happy
}

void drizzle_end(int sig)
{
  drizzle_con_free(&con);
  drizzle_free(&drizzle);
  if (!status.batch && !quick && histfile)
  {
    /* write-history */
    if (verbose)
      tee_fprintf(stdout, _("Writing history-file %s\n"),histfile);
    if (!write_history(histfile_tmp))
      internal::my_rename(histfile_tmp, histfile, MYF(MY_WME));
  }
  delete status.line_buff;
  status.line_buff= 0;

  if (sig >= 0)
    put_info(sig ? _("Aborted") : _("Bye"), INFO_RESULT,0,0);
  if (glob_buffer)
    delete glob_buffer;
  if (processed_prompt)
    delete processed_prompt;
  free(opt_password);
  free(histfile);
  free(histfile_tmp);
  free(current_db);
  free(current_host);
  free(current_user);
  free(full_username);
  free(part_username);
  free(default_prompt);
  free(current_prompt);
  internal::free_defaults(defaults_argv);
  internal::my_end();
  exit(status.exit_status);
}


/*
  This function handles sigint calls
  If query is in process, kill query
  no query in process, terminate like previous behavior
*/
extern "C"
void handle_sigint(int sig)
{
  char kill_buffer[40];
  drizzle_con_st kill_drizzle;
  drizzle_result_st res;
  drizzle_return_t ret;

  /* terminate if no query being executed, or we already tried interrupting */
  if (!executing_query || interrupted_query) {
    goto err;
  }

  if (drizzle_con_add_tcp(&drizzle, &kill_drizzle, current_host,
                          opt_drizzle_port, current_user, opt_password, NULL,
                          opt_mysql ? DRIZZLE_CON_MYSQL : DRIZZLE_CON_NONE) == NULL)
  {
    goto err;
  }

  /* kill_buffer is always big enough because max length of %lu is 15 */
  snprintf(kill_buffer, sizeof(kill_buffer), "KILL /*!50000 QUERY */ %u",
          drizzle_con_thread_id(&con));

  if (drizzle_query_str(&kill_drizzle, &res, kill_buffer, &ret) != NULL)
    drizzle_result_free(&res);

  drizzle_con_free(&kill_drizzle);
  tee_fprintf(stdout, _("Query aborted by Ctrl+C\n"));

  interrupted_query= 1;

  return;

err:
  drizzle_end(sig);
}


#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
void window_resize(int)
{
  struct winsize window_size;

  if (ioctl(fileno(stdin), TIOCGWINSZ, &window_size) == 0)
    terminal_width= window_size.ws_col;
}
#endif

static struct my_option my_long_options[] =
{
  {"help", '?', N_("Display this help and exit."), 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"help", 'I', N_("Synonym for -?"), 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"auto-rehash", OPT_AUTO_REHASH,
   N_("Enable automatic rehashing. One doesn't need to use 'rehash' to get table and field completion, but startup and reconnecting may take a longer time. Disable with --disable-auto-rehash."),
   (char**) &opt_rehash, (char**) &opt_rehash, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0,
   0, 0},
  {"no-auto-rehash", 'A',
   N_("No automatic rehashing. One has to use 'rehash' to get table and field completion. This gives a quicker start of drizzle_st and disables rehashing on reconnect. WARNING: options deprecated; use --disable-auto-rehash instead."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-vertical-output", OPT_AUTO_VERTICAL_OUTPUT,
   N_("Automatically switch to vertical output mode if the result is wider than the terminal width."),
   (char**) &auto_vertical_output, (char**) &auto_vertical_output, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"batch", 'B',
   N_("Don't use history file. Disable interactive behavior. (Enables --silent)"), 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"column-type-info", OPT_COLUMN_TYPES, N_("Display column type information."),
   (char**) &column_types_flag, (char**) &column_types_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"comments", 'c', N_("Preserve comments. Send comments to the server. The default is --skip-comments (discard comments), enable with --comments"),
   (char**) &preserve_comments, (char**) &preserve_comments,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', N_("Use compression in server/client protocol."),
   (char**) &opt_compress, (char**) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"database", 'D', N_("Database to use."), (char**) &current_db,
   (char**) &current_db, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   N_("(not used)"), 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"delimiter", OPT_DELIMITER, N_("Delimiter to be used."), (char**) &delimiter_str,
   (char**) &delimiter_str, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"execute", 'e', N_("Execute command and quit. (Disables --force and history file)"), 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"vertical", 'E', N_("Print the output of a query (rows) vertically."),
   (char**) &vertical, (char**) &vertical, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"force", 'f', N_("Continue even if we get an sql error."),
   (char**) &ignore_errors, (char**) &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"named-commands", 'G',
   N_("Enable named commands. Named commands mean this program's internal commands; see drizzle> help . When enabled, the named commands can be used from any line of the query, otherwise only from the first line, before an enter. Disable with --disable-named-commands. This option is disabled by default."),
   (char**) &named_cmds, (char**) &named_cmds, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"no-named-commands", 'g',
   N_("Named commands are disabled. Use \\* form only, or use named commands only in the beginning of a line ending with a semicolon (;) Since version 10.9 the client now starts with this option ENABLED by default! Disable with '-G'. Long format commands still work from the first line. WARNING: option deprecated; use --disable-named-commands instead."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"ignore-spaces", 'i', N_("Ignore space after function names."), 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"local-infile", OPT_LOCAL_INFILE, N_("Enable/disable LOAD DATA LOCAL INFILE."),
   (char**) &opt_local_infile,
   (char**) &opt_local_infile, 0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"no-beep", 'b', N_("Turn off beep on error."), (char**) &opt_nobeep,
   (char**) &opt_nobeep, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', N_("Connect to host."), (char**) &current_host,
   (char**) &current_host, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"line-numbers", OPT_LINE_NUMBERS, N_("Write line numbers for errors."),
   (char**) &line_numbers, (char**) &line_numbers, 0, GET_BOOL,
   NO_ARG, 1, 0, 0, 0, 0, 0},
  {"skip-line-numbers", 'L', N_("Don't write line number for errors. WARNING: -L is deprecated, use long version of this option instead."), 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"unbuffered", 'n', N_("Flush buffer after each query."), (char**) &unbuffered,
   (char**) &unbuffered, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"column-names", OPT_COLUMN_NAMES, N_("Write column names in results."),
   (char**) &column_names, (char**) &column_names, 0, GET_BOOL,
   NO_ARG, 1, 0, 0, 0, 0, 0},
  {"skip-column-names", 'N',
   N_("Don't write column names in results. WARNING: -N is deprecated, use long version of this options instead."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"set-variable", 'O',
   N_("Change the value of a variable. Please note that this option is deprecated; you can set variables directly with --variable-name=value."),
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"sigint-ignore", OPT_SIGINT_IGNORE, N_("Ignore SIGINT (CTRL-C)"),
   (char**) &opt_sigint_ignore,  (char**) &opt_sigint_ignore, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"one-database", 'o',
   N_("Only update the default database. This is useful for skipping updates to other database in the update log."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"pager", OPT_PAGER,
   N_("Pager to use to display results. If you don't supply an option the default pager is taken from your ENV variable PAGER. Valid pagers are less, more, cat [> filename], etc. See interactive help (\\h) also. This option does not work in batch mode. Disable with --disable-pager. This option is disabled by default."),
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"no-pager", OPT_NOPAGER,
   N_("Disable pager and print to stdout. See interactive help (\\h) also. WARNING: option deprecated; use --disable-pager instead."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'P',
   N_("Password to use when connecting to server. If password is not given it's asked from the tty."),
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'p', N_("Port number to use for connection or 0 for default to, in order of preference, drizzle.cnf, $DRIZZLE_TCP_PORT, ")
   N_("built-in default") " (" STRINGIFY_ARG(DRIZZLE_PORT) ").",
   0, 0, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"prompt", OPT_PROMPT, N_("Set the drizzle prompt to this value."),
   (char**) &current_prompt, (char**) &current_prompt, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q',
   N_("Don't cache result, print it row by row. This may slow down the server if the output is suspended. Doesn't use history file."),
   (char**) &quick, (char**) &quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"raw", 'r', N_("Write fields without conversion. Used with --batch."),
   (char**) &opt_raw_data, (char**) &opt_raw_data, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"reconnect", OPT_RECONNECT, N_("Reconnect if the connection is lost. Disable with --disable-reconnect. This option is enabled by default."),
   (char**) &opt_reconnect, (char**) &opt_reconnect, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"shutdown", OPT_SHUTDOWN, N_("Shutdown the server."),
   (char**) &opt_shutdown, (char**) &opt_shutdown, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', N_("Be more silent. Print results with a tab as separator, each row on new line."), 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"table", 't', N_("Output in table format."), (char**) &output_tables,
   (char**) &output_tables, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"tee", OPT_TEE,
   N_("Append everything into outfile. See interactive help (\\h) also. Does not work in batch mode. Disable with --disable-tee. This option is disabled by default."),
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"no-tee", OPT_NOTEE, N_("Disable outfile. See interactive help (\\h) also. WARNING: option deprecated; use --disable-tee instead"), 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', N_("User for login if not current user."), (char**) &current_user,
   (char**) &current_user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"safe-updates", 'U', N_("Only allow UPDATE and DELETE that uses keys."),
   (char**) &safe_updates, (char**) &safe_updates, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"i-am-a-dummy", 'U', N_("Synonym for option --safe-updates, -U."),
   (char**) &safe_updates, (char**) &safe_updates, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"verbose", 'v', N_("Write more. (-v -v -v gives the table output format)."), 0,
   0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', N_("Output version information and exit."), 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"wait", 'w', N_("Wait and retry if connection is down."), 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT,
   N_("Number of seconds before connection timeout."),
   (char**) &opt_connect_timeout,
   (char**) &opt_connect_timeout, 0, GET_UINT32, REQUIRED_ARG, 0, 0, 3600*12, 0,
   0, 0},
  {"max_input_line", OPT_MAX_INPUT_LINE,
   N_("Max length of input line"),
   (char**) &opt_max_input_line, (char**) &opt_max_input_line, 0,
   GET_UINT32, REQUIRED_ARG, 16 *1024L*1024L, 4096,
   (int64_t) 2*1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"select_limit", OPT_SELECT_LIMIT,
   N_("Automatic limit for SELECT when using --safe-updates"),
   (char**) &select_limit,
   (char**) &select_limit, 0, GET_UINT32, REQUIRED_ARG, 1000L, 1, ULONG_MAX,
   0, 1, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   N_("Automatic limit for rows in a join when using --safe-updates"),
   (char**) &max_join_size,
   (char**) &max_join_size, 0, GET_UINT32, REQUIRED_ARG, 1000000L, 1, ULONG_MAX,
   0, 1, 0},
  {"secure-auth", OPT_SECURE_AUTH, N_("Refuse client connecting to server if it uses old (pre-4.1.1) protocol"), (char**) &opt_secure_auth,
   (char**) &opt_secure_auth, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"show-warnings", OPT_SHOW_WARNINGS, N_("Show warnings after every statement."),
   (char**) &show_warnings, (char**) &show_warnings, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"show-progress-size", OPT_SHOW_PROGRESS_SIZE, N_("Number of lines before each import progress report."),
   (char**) &show_progress_size, (char**) &show_progress_size, 0, GET_UINT32, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"ping", OPT_PING, N_("Ping the server to check if it's alive."),
   (char**) &opt_ping, (char**) &opt_ping, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"mysql", 'm', N_("Use MySQL Protocol."),
   (char**) &opt_mysql, (char**) &opt_mysql, 0, GET_BOOL, NO_ARG, 1, 0, 0,
   0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void usage(int version)
{
  const char* readline= "readline";

  printf(_("%s  Ver %s Distrib %s, for %s-%s (%s) using %s %s\n"),
         internal::my_progname, VER.c_str(), drizzle_version(),
         HOST_VENDOR, HOST_OS, HOST_CPU,
         readline, rl_library_version);

  if (version)
    return;
  printf(_("Copyright (C) 2008 Sun Microsystems\n"
           "This software comes with ABSOLUTELY NO WARRANTY. "
           "This is free software,\n"
           "and you are welcome to modify and redistribute it "
           "under the GPL license\n"));
  printf(_("Usage: %s [OPTIONS] [database]\n"), internal::my_progname);
  my_print_help(my_long_options);
  internal::print_defaults("drizzle", load_default_groups);
  my_print_variables(my_long_options);
}


static bool get_one_option(int optid, const struct my_option *, char *argument)
{
  char *endchar= NULL;
  uint64_t temp_drizzle_port= 0;

  switch(optid) {
  case  OPT_DEFAULT_CHARSET:
    default_charset_used= 1;
    break;
  case OPT_DELIMITER:
    if (argument == disabled_my_option)
    {
      strcpy(delimiter, DEFAULT_DELIMITER);
    }
    else
    {
      /* Check that delimiter does not contain a backslash */
      if (!strstr(argument, "\\"))
      {
        strncpy(delimiter, argument, sizeof(delimiter) - 1);
      }
      else
      {
        put_info(_("DELIMITER cannot contain a backslash character"),
                 INFO_ERROR,0,0);
        return false;
      }
    }
    delimiter_length= (uint32_t)strlen(delimiter);
    delimiter_str= delimiter;
    break;
  case OPT_TEE:
    if (argument == disabled_my_option)
    {
      if (opt_outfile)
        end_tee();
    }
    else
      init_tee(argument);
    break;
  case OPT_NOTEE:
    printf(_("WARNING: option deprecated; use --disable-tee instead.\n"));
    if (opt_outfile)
      end_tee();
    break;
  case OPT_PAGER:
    if (argument == disabled_my_option)
      opt_nopager= 1;
    else
    {
      opt_nopager= 0;
      if (argument && strlen(argument))
      {
        default_pager_set= 1;
        strncpy(pager, argument, sizeof(pager) - 1);
        strcpy(default_pager, pager);
      }
      else if (default_pager_set)
        strcpy(pager, default_pager);
      else
        opt_nopager= 1;
    }
    break;
  case OPT_NOPAGER:
    printf(_("WARNING: option deprecated; use --disable-pager instead.\n"));
    opt_nopager= 1;
    break;
  case OPT_SERVER_ARG:
    printf(_("WARNING: --server-arg option not supported in this configuration.\n"));
    break;
  case 'A':
    opt_rehash= 0;
    break;
  case 'N':
    column_names= 0;
    break;
  case 'e':
    status.batch= 1;
    status.add_to_history= 0;
    if (status.line_buff == NULL)
      status.line_buff= new(std::nothrow) LineBuffer(opt_max_input_line,NULL);
    if (status.line_buff == NULL)
    {
      internal::my_end();
      exit(1);
    }
    status.line_buff->addString(argument);
    break;
  case 'o':
    if (argument == disabled_my_option)
      one_database= 0;
    else
      one_database= skip_updates= 1;
    break;
  case 'p':
    temp_drizzle_port= (uint64_t) strtoul(argument, &endchar, 10);
    /* if there is an alpha character this is not a valid port */
    if (strlen(endchar) != 0)
    {
      put_info(_("Non-integer value supplied for port.  If you are trying to enter a password please use --password instead."), INFO_ERROR, 0, 0);
      return false;
    }
    /* If the port number is > 65535 it is not a valid port
       This also helps with potential data loss casting unsigned long to a
       uint32_t. */
    if ((temp_drizzle_port == 0) || (temp_drizzle_port > 65535))
    {
      put_info(_("Value supplied for port is not valid."), INFO_ERROR, 0, 0);
      return false;
    }
    else
    {
      opt_drizzle_port= (uint32_t) temp_drizzle_port;
    }
    break;
  case 'P':
    /* Don't require password */
    if (argument == disabled_my_option)
    {
      argument= (char*) "";
    }
    if (argument)
    {
      char *start= argument;
      free(opt_password);
      opt_password= strdup(argument);
      while (*argument)
      {
        /* Overwriting password with 'x' */
        *argument++= 'x';
      }
      if (*start)
      {
        start[1]= 0;
      }
      tty_password= 0;
    }
    else
    {
      tty_password= 1;
    }
    break;
  case 's':
    if (argument == disabled_my_option)
      opt_silent= 0;
    else
      opt_silent++;
    break;
  case 'v':
    if (argument == disabled_my_option)
      verbose= 0;
    else
      verbose++;
    break;
  case 'B':
    status.batch= 1;
    status.add_to_history= 0;
    set_if_bigger(opt_silent,1);                         // more silent
    break;
  case 'V':
    usage(1);
    exit(0);
  case 'I':
  case '?':
    usage(0);
    exit(0);
  }
  return 0;
}


static int get_options(int argc, char **argv)
{
  char *tmp, *pagpoint;
  int ho_error;

  tmp= (char *) getenv("DRIZZLE_HOST");
  if (tmp)
    current_host= strdup(tmp);

  pagpoint= getenv("PAGER");
  if (!((char*) (pagpoint)))
  {
    strcpy(pager, "stdout");
    opt_nopager= 1;
  }
  else
    strcpy(pager, pagpoint);
  strcpy(default_pager, pager);

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (status.batch) /* disable pager and outfile in this case */
  {
    strcpy(default_pager, "stdout");
    strcpy(pager, "stdout");
    opt_nopager= 1;
    default_pager_set= 0;
    opt_outfile= 0;
    opt_reconnect= 0;
    connect_flag= DRIZZLE_CAPABILITIES_NONE; /* Not in interactive mode */
  }

  if (argc > 1)
  {
    usage(0);
    exit(1);
  }
  if (argc == 1)
  {
    skip_updates= 0;
    free(current_db);
    current_db= strdup(*argv);
  }
  if (tty_password)
    opt_password= client_get_tty_password(NULL);

  return(0);
}

static int read_and_execute(bool interactive)
{
  char *line;
  char in_string=0;
  uint32_t line_number=0;
  bool ml_comment= 0;
  COMMANDS *com;
  status.exit_status=1;

  for (;;)
  {
    if (!interactive)
    {
      if (status.line_buff)
        line= status.line_buff->readline();
      else
        line= 0;

      line_number++;
      if (show_progress_size > 0)
      {
        if ((line_number % show_progress_size) == 0)
          fprintf(stderr, _("Processing line: %"PRIu32"\n"), line_number);
      }
      if (!glob_buffer->empty())
        status.query_start_line= line_number;
    }
    else
    {
      const char *prompt= (const char*) (ml_comment ? "   /*> " :
                                         (glob_buffer->empty())
                                         ?  construct_prompt()
                                         : !in_string ? "    -> " :
                                         in_string == '\'' ?
                                         "    '> " : (in_string == '`' ?
                                                      "    `> " :
                                                      "    \"> "));
      if (opt_outfile && glob_buffer->empty())
        fflush(OUTFILE);

      if (opt_outfile)
        fputs(prompt, OUTFILE);
      line= readline(prompt);
      /*
        When Ctrl+d or Ctrl+z is pressed, the line may be NULL on some OS
        which may cause coredump.
      */
      if (opt_outfile && line)
        fprintf(OUTFILE, "%s\n", line);
    }
    // End of file
    if (!line)
    {
      status.exit_status=0;
      break;
    }

    /*
      Check if line is a drizzle command line
      (We want to allow help, print and clear anywhere at line start
    */
    if ((named_cmds || (glob_buffer->empty()))
        && !ml_comment && !in_string && (com=find_command(line,0)))
    {
      if ((*com->func)(glob_buffer,line) > 0)
        break;
      // If buffer was emptied
      if (glob_buffer->empty())
        in_string=0;
      if (interactive && status.add_to_history && not_in_history(line))
        add_history(line);
      continue;
    }
    if (add_line(glob_buffer,line,&in_string,&ml_comment))
      break;
  }
  /* if in batch mode, send last query even if it doesn't end with \g or go */

  if (!interactive && !status.exit_status)
  {
    remove_cntrl(glob_buffer);
    if (!glob_buffer->empty())
    {
      status.exit_status=1;
      if (com_go(glob_buffer,line) <= 0)
        status.exit_status=0;
    }
  }

  return status.exit_status;
}


static COMMANDS *find_command(const char *name,char cmd_char)
{
  uint32_t len;
  const char *end;

  if (!name)
  {
    len=0;
    end=0;
  }
  else
  {
    while (my_isspace(charset_info,*name))
      name++;
    /*
      If there is an \\g in the row or if the row has a delimiter but
      this is not a delimiter command, let add_line() take care of
      parsing the row and calling find_command()
    */
    if (strstr(name, "\\g") || (strstr(name, delimiter) &&
                                !(strlen(name) >= 9 &&
                                  !my_strnncoll(charset_info,
                                                (unsigned char*) name, 9,
                                                (const unsigned char*) "delimiter",
                                                9))))
      return((COMMANDS *) 0);
    if ((end=strcont(name," \t")))
    {
      len=(uint32_t) (end - name);
      while (my_isspace(charset_info,*end))
        end++;
      if (!*end)
        end=0;          // no arguments to function
    }
    else
      len=(uint32_t) strlen(name);
  }

  for (uint32_t i= 0; commands[i].name; i++)
  {
    if (commands[i].func &&
        ((name && !my_strnncoll(charset_info,(const unsigned char*)name,len, (const unsigned char*)commands[i].name,len) && !commands[i].name[len] && (!end || (end && commands[i].takes_params))) || (!name && commands[i].cmd_char == cmd_char)))
    {
      return(&commands[i]);
    }
  }
  return((COMMANDS *) 0);
}


static bool add_line(string *buffer, char *line, char *in_string,
                        bool *ml_comment)
{
  unsigned char inchar;
  char *pos, *out;
  COMMANDS *com;
  bool need_space= 0;
  bool ss_comment= 0;


  if (!line[0] && (buffer->empty()))
    return(0);
  if (status.add_to_history && line[0] && not_in_history(line))
    add_history(line);
  char *end_of_line=line+(uint32_t) strlen(line);

  for (pos=out=line ; (inchar= (unsigned char) *pos) ; pos++)
  {
    if (!preserve_comments)
    {
      // Skip spaces at the beggining of a statement
      if (my_isspace(charset_info,inchar) && (out == line) &&
          (buffer->empty()))
        continue;
    }

    // Accept multi-byte characters as-is
    int length;
    if (use_mb(charset_info) &&
        (length= my_ismbchar(charset_info, pos, end_of_line)))
    {
      if (!*ml_comment || preserve_comments)
      {
        while (length--)
          *out++ = *pos++;
        pos--;
      }
      else
        pos+= length - 1;
      continue;
    }
    if (!*ml_comment && inchar == '\\' &&
        !(*in_string && (drizzle_con_status(&con) & DRIZZLE_CON_STATUS_NO_BACKSLASH_ESCAPES)))
    {
      // Found possbile one character command like \c

      if (!(inchar = (unsigned char) *++pos))
        break;        // readline adds one '\'
      if (*in_string || inchar == 'N')  // \N is short for NULL
      {          // Don't allow commands in string
        *out++='\\';
        *out++= (char) inchar;
        continue;
      }
      if ((com=find_command(NULL,(char) inchar)))
      {
        // Flush previously accepted characters
        if (out != line)
        {
          buffer->append(line, (out-line));
          out= line;
        }

        if ((*com->func)(buffer,pos-1) > 0)
          return(1);                       // Quit
        if (com->takes_params)
        {
          if (ss_comment)
          {
            /*
              If a client-side macro appears inside a server-side comment,
              discard all characters in the comment after the macro (that is,
              until the end of the comment rather than the next delimiter)
            */
            for (pos++; *pos && (*pos != '*' || *(pos + 1) != '/'); pos++)
              ;
            pos--;
          }
          else
          {
            for (pos++ ;
                 *pos && (*pos != *delimiter ||
                          strncmp(pos + 1, delimiter + 1,
                                  strlen(delimiter + 1))) ; pos++)
              ;  // Remove parameters
            if (!*pos)
              pos--;
            else
              pos+= delimiter_length - 1; // Point at last delim char
          }
        }
      }
      else
      {
        string buff(_("Unknown command: "));
        buff.push_back('\'');
        buff.push_back(inchar);
        buff.push_back('\'');
        buff.push_back('.');
        if (put_info(buff.c_str(),INFO_ERROR,0,0) > 0)
          return(1);
        *out++='\\';
        *out++=(char) inchar;
        continue;
      }
    }
    else if (!*ml_comment && !*in_string &&
             (end_of_line - pos) >= 10 &&
             !my_strnncoll(charset_info, (unsigned char*) pos, 10,
                           (const unsigned char*) "delimiter ", 10))
    {
      // Flush previously accepted characters
      if (out != line)
      {
        buffer->append(line, (out - line));
        out= line;
      }

      // Flush possible comments in the buffer
      if (!buffer->empty())
      {
        if (com_go(buffer, 0) > 0) // < 0 is not fatal
          return(1);
        assert(buffer!=NULL);
        buffer->clear();
      }

      /*
        Delimiter wants the get rest of the given line as argument to
        allow one to change ';' to ';;' and back
      */
      buffer->append(pos);
      if (com_delimiter(buffer, pos) > 0)
        return(1);

      buffer->clear();
      break;
    }
    else if (!*ml_comment && !*in_string && !strncmp(pos, delimiter,
                                                     strlen(delimiter)))
    {
      // Found a statement. Continue parsing after the delimiter
      pos+= delimiter_length;

      if (preserve_comments)
      {
        while (my_isspace(charset_info, *pos))
          *out++= *pos++;
      }
      // Flush previously accepted characters
      if (out != line)
      {
        buffer->append(line, (out-line));
        out= line;
      }

      if (preserve_comments && ((*pos == '#') ||
                                ((*pos == '-') &&
                                 (pos[1] == '-') &&
                                 my_isspace(charset_info, pos[2]))))
      {
        // Add trailing single line comments to this statement
        buffer->append(pos);
        pos+= strlen(pos);
      }

      pos--;

      if ((com= find_command(buffer->c_str(), 0)))
      {

        if ((*com->func)(buffer, buffer->c_str()) > 0)
          return(1);                       // Quit
      }
      else
      {
        if (com_go(buffer, 0) > 0)             // < 0 is not fatal
          return(1);
      }
      buffer->clear();
    }
    else if (!*ml_comment
             && (!*in_string
                 && (inchar == '#'
                     || (inchar == '-'
                         && pos[1] == '-'
                         && my_isspace(charset_info,pos[2])))))
    {
      // Flush previously accepted characters
      if (out != line)
      {
        buffer->append(line, (out - line));
        out= line;
      }

      // comment to end of line
      if (preserve_comments)
        buffer->append(pos);

      break;
    }
    else if (!*in_string && inchar == '/' && *(pos+1) == '*' &&
             *(pos+2) != '!')
    {
      if (preserve_comments)
      {
        *out++= *pos++;                       // copy '/'
        *out++= *pos;                         // copy '*'
      }
      else
        pos++;
      *ml_comment= 1;
      if (out != line)
      {
        buffer->append(line, (out-line));
        out=line;
      }
    }
    else if (*ml_comment && !ss_comment && inchar == '*' && *(pos + 1) == '/')
    {
      if (preserve_comments)
      {
        *out++= *pos++;                       // copy '*'
        *out++= *pos;                         // copy '/'
      }
      else
        pos++;
      *ml_comment= 0;
      if (out != line)
      {
        buffer->append(line, (out - line));
        out= line;
      }
      // Consumed a 2 chars or more, and will add 1 at most,
      // so using the 'line' buffer to edit data in place is ok.
      need_space= 1;
    }
    else
    {
      // Add found char to buffer
      if (!*in_string && inchar == '/' && *(pos + 1) == '*' &&
          *(pos + 2) == '!')
        ss_comment= 1;
      else if (!*in_string && ss_comment && inchar == '*' && *(pos + 1) == '/')
        ss_comment= 0;
      if (inchar == *in_string)
        *in_string= 0;
      else if (!*ml_comment && !*in_string &&
               (inchar == '\'' || inchar == '"' || inchar == '`'))
        *in_string= (char) inchar;
      if (!*ml_comment || preserve_comments)
      {
        if (need_space && !my_isspace(charset_info, (char)inchar))
          *out++= ' ';
        need_space= 0;
        *out++= (char) inchar;
      }
    }
  }
  if (out != line || (buffer->length() > 0))
  {
    *out++='\n';
    uint32_t length=(uint32_t) (out-line);
    if ((!*ml_comment || preserve_comments))
      buffer->append(line, length);
  }
  return(0);
}

/*****************************************************************
            Interface to Readline Completion
******************************************************************/


static char **mysql_completion (const char *text, int start, int end);
extern "C" char *new_command_generator(const char *text, int);

/*
  Tell the GNU Readline library how to complete.  We want to try to complete
  on command names if this is the first word in the line, or on filenames
  if not.
*/
static char *no_completion(const char *, int)
{
  /* No filename completion */
  return 0;
}


/* glues pieces of history back together if in pieces   */
static void fix_history(string *final_command)
{
  int total_lines = 1;
  const char *ptr = final_command->c_str();
  char str_char = '\0';  /* Character if we are in a string or not */

  /* Converted buffer */
  string fixed_buffer;
  fixed_buffer.reserve(512);

  /* find out how many lines we have and remove newlines */
  while (*ptr != '\0')
  {
    switch (*ptr) {
      /* string character */
    case '"':
    case '\'':
    case '`':
      // open string
      if (str_char == '\0')
        str_char = *ptr;
      else if (str_char == *ptr)   /* close string */
        str_char = '\0';
      fixed_buffer.append(ptr, 1);
      break;
    case '\n':
      /*
        not in string, change to space
        if in string, leave it alone
      */
      fixed_buffer.append((str_char == '\0') ? " " : "\n");
      total_lines++;
      break;
    case '\\':
      fixed_buffer.append("\\");
      /* need to see if the backslash is escaping anything */
      if (str_char)
      {
        ptr++;
        /* special characters that need escaping */
        if (*ptr == '\'' || *ptr == '"' || *ptr == '\\')
          fixed_buffer.append(ptr, 1);
        else
          ptr--;
      }
      break;
    default:
      fixed_buffer.append(ptr, 1);
    }
    ptr++;
  }
  if (total_lines > 1)
    add_history(fixed_buffer.c_str());
}

/*
  returns 0 if line matches the previous history entry
  returns 1 if the line doesn't match the previous history entry
*/
static int not_in_history(const char *line)
{
  HIST_ENTRY *oldhist = history_get(history_length);

  if (oldhist == 0)
    return 1;
  if (strcmp(oldhist->line,line) == 0)
    return 0;
  return 1;
}

static void initialize_readline (char *name)
{
  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name= name;

  /* Tell the completer that we want a crack first. */
  rl_attempted_completion_function= (rl_completion_func_t*)&mysql_completion;
  rl_completion_entry_function= (drizzle_compentry_func_t*)&no_completion;
}


/*
  Attempt to complete on the contents of TEXT.  START and END show the
  region of TEXT that contains the word to complete.  We can use the
  entire line in case we want to do some simple parsing.  Return the
  array of matches, or NULL if there aren't any.
*/
char **mysql_completion (const char *text, int, int)
{
  if (!status.batch && !quick)
    return rl_completion_matches(text, new_command_generator);
  else
    return (char**) 0;
}

inline string lower_string(const string &from_string)
{
  string to_string= from_string;
  transform(to_string.begin(), to_string.end(),
            to_string.begin(), ::tolower);
  return to_string;
}
inline string lower_string(const char * from_string)
{
  string to_string= from_string;
  return lower_string(to_string);
}

template <class T>
class CompletionMatch :
  public unary_function<const string&, bool>
{
  string match_text; 
  T match_func;
public:
  CompletionMatch(string text) : match_text(text) {}
  inline bool operator() (const pair<string,string> &match_against) const
  {
    string sub_match=
      lower_string(match_against.first.substr(0,match_text.size()));
    return match_func(sub_match,match_text);
  }
};



extern "C"
char *new_command_generator(const char *text, int state)
{

  if (!state)
  {
    completion_string= lower_string(text);
    if (completion_string.size() == 0)
    {
      completion_iter= completion_map.begin();
      completion_end= completion_map.end();
    }
    else
    {
      completion_iter= find_if(completion_map.begin(), completion_map.end(),
                               CompletionMatch<equal_to<string> >(completion_string));
      completion_end= find_if(completion_iter, completion_map.end(),
                              CompletionMatch<not_equal_to<string> >(completion_string));
    }
  }
  if (completion_iter == completion_end || (size_t)state > completion_map.size())
    return NULL;
  char *result= (char *)malloc((*completion_iter).second.size()+1);
  strcpy(result, (*completion_iter).second.c_str());
  completion_iter++;
  return result;
}

/* Build up the completion hash */

static void build_completion_hash(bool rehash, bool write_info)
{
  COMMANDS *cmd=commands;
  drizzle_return_t ret;
  drizzle_result_st databases,tables,fields;
  drizzle_row_t database_row,table_row;
  drizzle_column_st *sql_field;
  string tmp_str, tmp_str_lower;

  if (status.batch || quick || !current_db)
    return;      // We don't need completion in batches
  if (!rehash)
    return;

  completion_map.clear();

  /* hash this file's known subset of SQL commands */
  while (cmd->name) {
    tmp_str= cmd->name;
    tmp_str_lower= lower_string(tmp_str);
    completion_map[tmp_str_lower]= tmp_str;
    cmd++;
  }

  /* hash Drizzle functions (to be implemented) */

  /* hash all database names */
  if (drizzle_query_str(&con, &databases, "show databases", &ret) != NULL)
  {
    if (ret == DRIZZLE_RETURN_OK)
    {
      if (drizzle_result_buffer(&databases) != DRIZZLE_RETURN_OK)
        put_info(drizzle_error(&drizzle),INFO_INFO,0,0);
      else
      {
        while ((database_row=drizzle_row_next(&databases)))
        {
          tmp_str= database_row[0];
          tmp_str_lower= lower_string(tmp_str);
          completion_map[tmp_str_lower]= tmp_str;
        }
      }
    }

    drizzle_result_free(&databases);
  }

  /* hash all table names */
  if (drizzle_query_str(&con, &tables, "show tables", &ret) != NULL)
  {
    if (ret != DRIZZLE_RETURN_OK)
    {
      drizzle_result_free(&tables);
      return;
    }

    if (drizzle_result_buffer(&tables) != DRIZZLE_RETURN_OK)
      put_info(drizzle_error(&drizzle),INFO_INFO,0,0);
    else
    {
      if (drizzle_result_row_count(&tables) > 0 && !opt_silent && write_info)
      {
        tee_fprintf(stdout, _("\
Reading table information for completion of table and column names\n    \
You can turn off this feature to get a quicker startup with -A\n\n"));
      }
      while ((table_row=drizzle_row_next(&tables)))
      {
        tmp_str= table_row[0];
        tmp_str_lower= lower_string(tmp_str);
        completion_map[tmp_str_lower]= tmp_str;
      }
    }
  }
  else
    return;

  /* hash all field names, both with the table prefix and without it */
  if (drizzle_result_row_count(&tables) == 0)
  {
    drizzle_result_free(&tables);
    return;
  }

  drizzle_row_seek(&tables, 0);

  while ((table_row=drizzle_row_next(&tables)))
  {
    string query;

    query.append("show fields in '");
    query.append(table_row[0]);
    query.append("'");
    
    if (drizzle_query(&con, &fields, query.c_str(), query.length(),
                      &ret) != NULL)
    {
      if (ret == DRIZZLE_RETURN_OK &&
          drizzle_result_buffer(&fields) == DRIZZLE_RETURN_OK)
      {
        while ((sql_field=drizzle_column_next(&fields)))
        {
          tmp_str=table_row[0];
          tmp_str.append(".");
          tmp_str.append(drizzle_column_name(sql_field));
          tmp_str_lower= lower_string(tmp_str);
          completion_map[tmp_str_lower]= tmp_str;

          tmp_str=drizzle_column_name(sql_field);
          tmp_str_lower= lower_string(tmp_str);
          completion_map[tmp_str_lower]= tmp_str;
        }
      }
      drizzle_result_free(&fields);
    }
  }
  drizzle_result_free(&tables);
  completion_iter= completion_map.begin();
}

/* for gnu readline */


static int reconnect(void)
{
  if (opt_reconnect)
  {
    put_info(_("No connection. Trying to reconnect..."),INFO_INFO,0,0);
    (void) com_connect((string *)0, 0);
    if (opt_rehash && connected)
      com_rehash(NULL, NULL);
  }
  if (! connected)
    return put_info(_("Can't connect to the server\n"),INFO_ERROR,0,0);
  return 0;
}

static void get_current_db(void)
{
  drizzle_return_t ret;
  drizzle_result_st res;

  free(current_db);
  current_db= NULL;
  /* In case of error below current_db will be NULL */
  if (drizzle_query_str(&con, &res, "SELECT DATABASE()", &ret) != NULL)
  {
    if (ret == DRIZZLE_RETURN_OK &&
        drizzle_result_buffer(&res) == DRIZZLE_RETURN_OK)
    {
      drizzle_row_t row= drizzle_row_next(&res);
      if (row[0])
        current_db= strdup(row[0]);
      drizzle_result_free(&res);
    }
  }
}

/***************************************************************************
 The different commands
***************************************************************************/

int drizzleclient_real_query_for_lazy(const char *buf, int length,
                                      drizzle_result_st *result,
                                      uint32_t *error_code)
{
  drizzle_return_t ret;

  for (uint32_t retry=0;; retry++)
  {
    int error;
    if (drizzle_query(&con,result,buf,length,&ret) != NULL &&
        ret == DRIZZLE_RETURN_OK)
    {
      return 0;
    }
    error= put_error(&con, result);

    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      *error_code= drizzle_result_error_code(result);
      drizzle_result_free(result);
    }

    if (ret != DRIZZLE_RETURN_SERVER_GONE || retry > 1 ||
        !opt_reconnect)
    {
      return error;
    }

    if (reconnect())
      return error;
  }
}

int drizzleclient_store_result_for_lazy(drizzle_result_st *result)
{
  if (drizzle_result_buffer(result) == DRIZZLE_RETURN_OK)
    return 0;

  if (drizzle_con_error(&con)[0])
    return put_error(&con, result);
  return 0;
}

static int
com_help(string *buffer, const char *)
{
  register int i, j;
  char buff[32], *end;

  put_info(_("List of all Drizzle commands:"), INFO_INFO,0,0);
  if (!named_cmds)
    put_info(_("Note that all text commands must be first on line and end with ';'"),INFO_INFO,0,0);
  for (i = 0; commands[i].name; i++)
  {
    end= strcpy(buff, commands[i].name);
    end+= strlen(commands[i].name);
    for (j= (int)strlen(commands[i].name); j < 10; j++)
      end= strcpy(end, " ")+1;
    if (commands[i].func)
      tee_fprintf(stdout, "%s(\\%c) %s\n", buff,
                  commands[i].cmd_char, _(commands[i].doc));
  }
  tee_fprintf(stdout, "\n");
  buffer->clear();
  return 0;
}


static int
com_clear(string *buffer, const char *)
{
  if (status.add_to_history)
    fix_history(buffer);
  buffer->clear();
  return 0;
}


/*
  Execute command
  Returns: 0  if ok
  -1 if not fatal error
  1  if fatal error
*/
static int
com_go(string *buffer, const char *)
{
  char          buff[200]; /* about 110 chars used so far */
  char          time_buff[52+3+1]; /* time max + space&parens + NUL */
  drizzle_result_st result;
  drizzle_return_t ret;
  uint32_t      timer, warnings= 0;
  uint32_t      error= 0;
  uint32_t      error_code= 0;
  int           err= 0;

  interrupted_query= 0;

  /* Remove garbage for nicer messages */
  remove_cntrl(buffer);

  if (buffer->empty())
  {
    // Ignore empty quries
    if (status.batch)
      return 0;
    return put_info(_("No query specified\n"),INFO_ERROR,0,0);

  }
  if (!connected && reconnect())
  {
    // Remove query on error
    buffer->clear();
    return opt_reconnect ? -1 : 1;          // Fatal error
  }
  if (verbose)
    (void) com_print(buffer, 0);

  if (skip_updates &&
      ((buffer->length() < 4) || (buffer->find( "SET ") != 0)))
  {
    (void) put_info(_("Ignoring query to other database"),INFO_INFO,0,0);
    return 0;
  }

  timer=start_timer();
  executing_query= 1;
  error= drizzleclient_real_query_for_lazy(buffer->c_str(),buffer->length(),&result, &error_code);

  if (status.add_to_history)
  {
    buffer->append(vertical ? "\\G" : delimiter);
    /* Append final command onto history */
    fix_history(buffer);
  }

  buffer->clear();

  if (error)
    goto end;

  do
  {
    char *pos;

    if (quick)
    {
      if (drizzle_column_buffer(&result) != DRIZZLE_RETURN_OK)
      {
        error= put_error(&con, &result);
        goto end;
      }
    }
    else
    {
      error= drizzleclient_store_result_for_lazy(&result);
      if (error)
        goto end;
    }

    if (verbose >= 3 || !opt_silent)
      drizzle_end_timer(timer,time_buff);
    else
      time_buff[0]= '\0';

    /* Every branch must truncate  buff . */
    if (drizzle_result_column_count(&result) > 0)
    {
      if (!quick && drizzle_result_row_count(&result) == 0 &&
          !column_types_flag)
      {
        strcpy(buff, _("Empty set"));
      }
      else
      {
        init_pager();
        if (vertical || (auto_vertical_output &&
                         (terminal_width < get_result_width(&result))))
          print_table_data_vertically(&result);
        else if (opt_silent && verbose <= 2 && !output_tables)
          print_tab_data(&result);
        else
          print_table_data(&result);
        snprintf(buff, sizeof(buff), 
                ngettext("%ld row in set","%ld rows in set",
                         (long) drizzle_result_row_count(&result)),
                (long) drizzle_result_row_count(&result));
        end_pager();
        if (drizzle_result_error_code(&result))
          error= put_error(&con, &result);
      }
    }
    else if (drizzle_result_affected_rows(&result) == ~(uint64_t) 0)
      strcpy(buff,_("Query OK"));
    else
      snprintf(buff, sizeof(buff), ngettext("Query OK, %ld row affected",
                             "Query OK, %ld rows affected",
                             (long) drizzle_result_affected_rows(&result)),
              (long) drizzle_result_affected_rows(&result));

    pos= strchr(buff, '\0');
    if ((warnings= drizzle_result_warning_count(&result)))
    {
      *pos++= ',';
      *pos++= ' ';
      char warnings_buff[20];
      memset(warnings_buff,0,20);
      snprintf(warnings_buff, sizeof(warnings_buff), "%d", warnings);
      strcpy(pos, warnings_buff);
      pos+= strlen(warnings_buff);
      pos= strcpy(pos, " warning")+8;
      if (warnings != 1)
        *pos++= 's';
    }
    strcpy(pos, time_buff);
    put_info(buff,INFO_RESULT,0,0);
    if (strcmp(drizzle_result_info(&result), ""))
      put_info(drizzle_result_info(&result),INFO_RESULT,0,0);
    put_info("",INFO_RESULT,0,0);      // Empty row

    if (unbuffered)
      fflush(stdout);
    drizzle_result_free(&result);

    if (drizzle_con_status(&con) & DRIZZLE_CON_STATUS_MORE_RESULTS_EXISTS)
    {
      if (drizzle_result_read(&con, &result, &ret) == NULL ||
          ret != DRIZZLE_RETURN_OK)
      {
        if (ret == DRIZZLE_RETURN_ERROR_CODE)
        {
          error_code= drizzle_result_error_code(&result);
          drizzle_result_free(&result);
        }

        error= put_error(&con, NULL);
        goto end;
      }
    }

  } while (drizzle_con_status(&con) & DRIZZLE_CON_STATUS_MORE_RESULTS_EXISTS);
  if (err >= 1)
    error= put_error(&con, NULL);

end:

  /* Show warnings if any or error occured */
  if (show_warnings == 1 && (warnings >= 1 || error))
    print_warnings(error_code);

  if (!error && !status.batch &&
      drizzle_con_status(&con) & DRIZZLE_CON_STATUS_DB_DROPPED)
  {
    get_current_db();
  }

  executing_query= 0;
  return error;        /* New command follows */
}


static void init_pager()
{
  if (!opt_nopager)
  {
    if (!(PAGER= popen(pager, "w")))
    {
      tee_fprintf(stdout, "popen() failed! defaulting PAGER to stdout!\n");
      PAGER= stdout;
    }
  }
  else
    PAGER= stdout;
}

static void end_pager()
{
  if (!opt_nopager)
    pclose(PAGER);
}


static void init_tee(const char *file_name)
{
  FILE* new_outfile;
  if (opt_outfile)
    end_tee();
  if (!(new_outfile= fopen(file_name, "a")))
  {
    tee_fprintf(stdout, "Error logging to file '%s'\n", file_name);
    return;
  }
  OUTFILE = new_outfile;
  strncpy(outfile, file_name, FN_REFLEN-1);
  tee_fprintf(stdout, "Logging to file '%s'\n", file_name);
  opt_outfile= 1;

  return;
}


static void end_tee()
{
  fclose(OUTFILE);
  OUTFILE= 0;
  opt_outfile= 0;
  return;
}


static int
com_ego(string *buffer,const char *line)
{
  int result;
  bool oldvertical=vertical;
  vertical=1;
  result=com_go(buffer,line);
  vertical=oldvertical;
  return result;
}


static const char *fieldtype2str(drizzle_column_type_t type)
{
  switch (type) {
    case DRIZZLE_COLUMN_TYPE_BLOB:        return "BLOB";
    case DRIZZLE_COLUMN_TYPE_DATE:        return "DATE";
    case DRIZZLE_COLUMN_TYPE_DATETIME:    return "DATETIME";
    case DRIZZLE_COLUMN_TYPE_NEWDECIMAL:  return "DECIMAL";
    case DRIZZLE_COLUMN_TYPE_DOUBLE:      return "DOUBLE";
    case DRIZZLE_COLUMN_TYPE_ENUM:        return "ENUM";
    case DRIZZLE_COLUMN_TYPE_LONG:        return "LONG";
    case DRIZZLE_COLUMN_TYPE_LONGLONG:    return "LONGLONG";
    case DRIZZLE_COLUMN_TYPE_NULL:        return "NULL";
    case DRIZZLE_COLUMN_TYPE_TIMESTAMP:   return "TIMESTAMP";
    default:                     return "?-unknown-?";
  }
}

static char *fieldflags2str(uint32_t f) {
  static char buf[1024];
  char *s=buf;
  *s=0;
#define ff2s_check_flag(X)                                              \
  if (f & DRIZZLE_COLUMN_FLAGS_ ## X) { s=strcpy(s, # X " ")+strlen(# X " "); \
                        f &= ~ DRIZZLE_COLUMN_FLAGS_ ## X; }
  ff2s_check_flag(NOT_NULL);
  ff2s_check_flag(PRI_KEY);
  ff2s_check_flag(UNIQUE_KEY);
  ff2s_check_flag(MULTIPLE_KEY);
  ff2s_check_flag(BLOB);
  ff2s_check_flag(UNSIGNED);
  ff2s_check_flag(BINARY);
  ff2s_check_flag(ENUM);
  ff2s_check_flag(AUTO_INCREMENT);
  ff2s_check_flag(TIMESTAMP);
  ff2s_check_flag(SET);
  ff2s_check_flag(NO_DEFAULT_VALUE);
  ff2s_check_flag(NUM);
  ff2s_check_flag(PART_KEY);
  ff2s_check_flag(GROUP);
  ff2s_check_flag(UNIQUE);
  ff2s_check_flag(BINCMP);
  ff2s_check_flag(ON_UPDATE_NOW);
#undef ff2s_check_flag
  if (f)
    snprintf(s, sizeof(buf), " unknows=0x%04x", f);
  return buf;
}

static void
print_field_types(drizzle_result_st *result)
{
  drizzle_column_st   *field;
  uint32_t i=0;

  while ((field = drizzle_column_next(result)))
  {
    tee_fprintf(PAGER, "Field %3u:  `%s`\n"
                "Catalog:    `%s`\n"
                "Database:   `%s`\n"
                "Table:      `%s`\n"
                "Org_table:  `%s`\n"
                "Type:       %s\n"
                "Collation:  %s (%u)\n"
                "Length:     %lu\n"
                "Max_length: %lu\n"
                "Decimals:   %u\n"
                "Flags:      %s\n\n",
                ++i,
                drizzle_column_name(field), drizzle_column_catalog(field),
                drizzle_column_db(field), drizzle_column_table(field),
                drizzle_column_orig_table(field),
                fieldtype2str(drizzle_column_type(field)),
                get_charset_name(drizzle_column_charset(field)),
                drizzle_column_charset(field), drizzle_column_size(field),
                drizzle_column_max_size(field), drizzle_column_decimals(field),
                fieldflags2str(drizzle_column_flags(field)));
  }
  tee_puts("", PAGER);
}


static void
print_table_data(drizzle_result_st *result)
{
  drizzle_row_t cur;
  drizzle_return_t ret;
  drizzle_column_st *field;
  bool *num_flag;
  string separator;

  separator.reserve(256);

  num_flag=(bool*) malloc(sizeof(bool)*drizzle_result_column_count(result));
  if (column_types_flag)
  {
    print_field_types(result);
    if (!drizzle_result_row_count(result))
      return;
    drizzle_column_seek(result,0);
  }
  separator.append("+");
  while ((field = drizzle_column_next(result)))
  {
    uint32_t x, length= 0;

    if (column_names)
    {
      uint32_t name_length= strlen(drizzle_column_name(field));

      /* Check if the max_byte value is really the maximum in terms
         of visual length since multibyte characters can affect the
         length of the separator. */
      length= charset_info->cset->numcells(charset_info,
                                           drizzle_column_name(field),
                                           drizzle_column_name(field) +
                                           name_length);

      if (name_length == drizzle_column_max_size(field))
      {
        if (length < drizzle_column_max_size(field))
          drizzle_column_set_max_size(field, length);
      }
      else
      {
        length= name_length;
      }
    }
  
    if (quick)
      length=max(length,drizzle_column_size(field));
    else
      length=max(length,(uint32_t)drizzle_column_max_size(field));
    if (length < 4 &&
        !(drizzle_column_flags(field) & DRIZZLE_COLUMN_FLAGS_NOT_NULL))
    {
      // Room for "NULL"
      length=4;
    }
    drizzle_column_set_max_size(field, length);

    for (x=0; x< (length+2); x++)
      separator.append("-");
    separator.append("+");
  }

  tee_puts((char*) separator.c_str(), PAGER);
  if (column_names)
  {
    drizzle_column_seek(result,0);
    (void) tee_fputs("|", PAGER);
    for (uint32_t off=0; (field = drizzle_column_next(result)) ; off++)
    {
      uint32_t name_length= (uint32_t) strlen(drizzle_column_name(field));
      uint32_t numcells= charset_info->cset->numcells(charset_info,
                                                  drizzle_column_name(field),
                                                  drizzle_column_name(field) +
                                                  name_length);
      uint32_t display_length= drizzle_column_max_size(field) + name_length -
                               numcells;
      tee_fprintf(PAGER, " %-*s |",(int) min(display_length,
                                             MAX_COLUMN_LENGTH),
                  drizzle_column_name(field));
      num_flag[off]= ((drizzle_column_type(field) <= DRIZZLE_COLUMN_TYPE_LONGLONG) ||
                      (drizzle_column_type(field) == DRIZZLE_COLUMN_TYPE_NEWDECIMAL));
    }
    (void) tee_fputs("\n", PAGER);
    tee_puts((char*) separator.c_str(), PAGER);
  }

  while (1)
  {
    if (quick)
    {
      cur= drizzle_row_buffer(result, &ret);
      if (ret != DRIZZLE_RETURN_OK)
      {
        (void)put_error(&con, result);
        break;
      }
    }
    else
      cur= drizzle_row_next(result);

    if (cur == NULL || interrupted_query)
      break;

    size_t *lengths= drizzle_row_field_sizes(result);
    (void) tee_fputs("| ", PAGER);
    drizzle_column_seek(result, 0);
    for (uint32_t off= 0; off < drizzle_result_column_count(result); off++)
    {
      const char *buffer;
      uint32_t data_length;
      uint32_t field_max_length;
      uint32_t visible_length;
      uint32_t extra_padding;

      if (cur[off] == NULL)
      {
        buffer= "NULL";
        data_length= 4;
      }
      else
      {
        buffer= cur[off];
        data_length= (uint32_t) lengths[off];
      }

      field= drizzle_column_next(result);
      field_max_length= drizzle_column_max_size(field);

      /*
        How many text cells on the screen will this string span?  If it contains
        multibyte characters, then the number of characters we occupy on screen
        will be fewer than the number of bytes we occupy in memory.

        We need to find how much screen real-estate we will occupy to know how
        many extra padding-characters we should send with the printing function.
      */
      visible_length= charset_info->cset->numcells(charset_info, buffer, buffer + data_length);
      extra_padding= data_length - visible_length;

      if (field_max_length > MAX_COLUMN_LENGTH)
        tee_print_sized_data(buffer, data_length, MAX_COLUMN_LENGTH+extra_padding, false);
      else
      {
        if (num_flag[off] != 0) /* if it is numeric, we right-justify it */
          tee_print_sized_data(buffer, data_length, field_max_length+extra_padding, true);
        else
          tee_print_sized_data(buffer, data_length,
                               field_max_length+extra_padding, false);
      }
      tee_fputs(" | ", PAGER);
    }
    (void) tee_fputs("\n", PAGER);
    if (quick)
      drizzle_row_free(result, cur);
  }
  tee_puts(separator.c_str(), PAGER);
  free(num_flag);
}

/**
   Return the length of a field after it would be rendered into text.

   This doesn't know or care about multibyte characters.  Assume we're
   using such a charset.  We can't know that all of the upcoming rows
   for this column will have bytes that each render into some fraction
   of a character.  It's at least possible that a row has bytes that
   all render into one character each, and so the maximum length is
   still the number of bytes.  (Assumption 1:  This can't be better
   because we can never know the number of characters that the DB is
   going to send -- only the number of bytes.  2: Chars <= Bytes.)

   @param  field  Pointer to a field to be inspected

   @returns  number of character positions to be used, at most
*/
static int get_field_disp_length(drizzle_column_st *field)
{
  uint32_t length= column_names ? strlen(drizzle_column_name(field)) : 0;

  if (quick)
    length= max(length, drizzle_column_size(field));
  else
    length= max(length, (uint32_t)drizzle_column_max_size(field));

  if (length < 4 &&
    !(drizzle_column_flags(field) & DRIZZLE_COLUMN_FLAGS_NOT_NULL))
  {
    length= 4;        /* Room for "NULL" */
  }

  return length;
}

/**
   For a new result, return the max number of characters that any
   upcoming row may return.

   @param  result  Pointer to the result to judge

   @returns  The max number of characters in any row of this result
*/
static int get_result_width(drizzle_result_st *result)
{
  unsigned int len= 0;
  drizzle_column_st *field;
  uint16_t offset;

  offset= drizzle_column_current(result);
  assert(offset == 0);

  while ((field= drizzle_column_next(result)) != NULL)
    len+= get_field_disp_length(field) + 3; /* plus bar, space, & final space */

  (void) drizzle_column_seek(result, offset);

  return len + 1; /* plus final bar. */
}

static void
tee_print_sized_data(const char *data, unsigned int data_length, unsigned int total_bytes_to_send, bool right_justified)
{
  /*
    For '\0's print ASCII spaces instead, as '\0' is eaten by (at
    least my) console driver, and that messes up the pretty table
    grid.  (The \0 is also the reason we can't use fprintf() .)
  */
  unsigned int i;
  const char *p;

  if (right_justified)
    for (i= data_length; i < total_bytes_to_send; i++)
      tee_putc((int)' ', PAGER);

  for (i= 0, p= data; i < data_length; i+= 1, p+= 1)
  {
    if (*p == '\0')
      tee_putc((int)' ', PAGER);
    else
      tee_putc((int)*p, PAGER);
  }

  if (! right_justified)
    for (i= data_length; i < total_bytes_to_send; i++)
      tee_putc((int)' ', PAGER);
}



static void
print_table_data_vertically(drizzle_result_st *result)
{
  drizzle_row_t cur;
  drizzle_return_t ret;
  uint32_t max_length=0;
  drizzle_column_st *field;

  while ((field = drizzle_column_next(result)))
  {
    uint32_t length= strlen(drizzle_column_name(field));
    if (length > max_length)
      max_length= length;
    drizzle_column_set_max_size(field, length);
  }

  for (uint32_t row_count=1;; row_count++)
  {
    if (quick)
    {
      cur= drizzle_row_buffer(result, &ret);
      if (ret != DRIZZLE_RETURN_OK)
      {
        (void)put_error(&con, result);
        break;
      }
    }
    else
      cur= drizzle_row_next(result);

    if (cur == NULL || interrupted_query)
      break;
    drizzle_column_seek(result,0);
    tee_fprintf(PAGER,
                "*************************** %d. row ***************************\n", row_count);
    for (uint32_t off=0; off < drizzle_result_column_count(result); off++)
    {
      field= drizzle_column_next(result);
      tee_fprintf(PAGER, "%*s: ",(int) max_length,drizzle_column_name(field));
      tee_fprintf(PAGER, "%s\n",cur[off] ? (char*) cur[off] : "NULL");
    }
    if (quick)
      drizzle_row_free(result, cur);
  }
}


/* print_warnings should be called right after executing a statement */

static void print_warnings(uint32_t error_code)
{
  const char *query;
  drizzle_result_st result;
  drizzle_row_t cur;
  uint64_t num_rows;
  uint32_t new_code= 0;

  /* Get the warnings */
  query= "show warnings";
  drizzleclient_real_query_for_lazy(query, strlen(query),&result,&new_code);
  drizzleclient_store_result_for_lazy(&result);

  /* Bail out when no warnings */
  if (!(num_rows= drizzle_result_row_count(&result)))
    goto end;

  cur= drizzle_row_next(&result);

  /*
    Don't print a duplicate of the current error.  It is possible for SHOW
    WARNINGS to return multiple errors with the same code, but different
    messages.  To be safe, skip printing the duplicate only if it is the only
    warning.
  */
  if (!cur || (num_rows == 1 &&
      error_code == (uint32_t) strtoul(cur[1], NULL, 10)))
  {
    goto end;
  }

  /* Print the warnings */
  init_pager();
  do
  {
    tee_fprintf(PAGER, "%s (Code %s): %s\n", cur[0], cur[1], cur[2]);
  } while ((cur= drizzle_row_next(&result)));
  end_pager();

end:
  drizzle_result_free(&result);
}


static void
safe_put_field(const char *pos,uint32_t length)
{
  if (!pos)
    tee_fputs("NULL", PAGER);
  else
  {
    if (opt_raw_data)
      tee_fputs(pos, PAGER);
    else for (const char *end=pos+length ; pos != end ; pos++)
    {
      int l;
      if (use_mb(charset_info) &&
          (l = my_ismbchar(charset_info, pos, end)))
      {
        while (l--)
          tee_putc(*pos++, PAGER);
        pos--;
        continue;
      }
      if (!*pos)
        tee_fputs("\\0", PAGER); // This makes everything hard
      else if (*pos == '\t')
        tee_fputs("\\t", PAGER); // This would destroy tab format
      else if (*pos == '\n')
        tee_fputs("\\n", PAGER); // This too
      else if (*pos == '\\')
        tee_fputs("\\\\", PAGER);
      else
        tee_putc(*pos, PAGER);
    }
  }
}


static void
print_tab_data(drizzle_result_st *result)
{
  drizzle_row_t cur;
  drizzle_return_t ret;
  drizzle_column_st *field;
  size_t *lengths;

  if (opt_silent < 2 && column_names)
  {
    int first=0;
    while ((field = drizzle_column_next(result)))
    {
      if (first++)
        (void) tee_fputs("\t", PAGER);
      (void) tee_fputs(drizzle_column_name(field), PAGER);
    }
    (void) tee_fputs("\n", PAGER);
  }
  while (1)
  {
    if (quick)
    {
      cur= drizzle_row_buffer(result, &ret);
      if (ret != DRIZZLE_RETURN_OK)
      {
        (void)put_error(&con, result);
        break;
      }
    }
    else
      cur= drizzle_row_next(result);

    if (cur == NULL)
      break;

    lengths= drizzle_row_field_sizes(result);
    safe_put_field(cur[0],lengths[0]);
    for (uint32_t off=1 ; off < drizzle_result_column_count(result); off++)
    {
      (void) tee_fputs("\t", PAGER);
      safe_put_field(cur[off], lengths[off]);
    }
    (void) tee_fputs("\n", PAGER);
    if (quick)
      drizzle_row_free(result, cur);
  }
}

static int
com_tee(string *, const char *line )
{
  char file_name[FN_REFLEN], *end;
  const char *param;

  if (status.batch)
    return 0;
  while (my_isspace(charset_info,*line))
    line++;
  if (!(param = strchr(line, ' '))) // if outfile wasn't given, use the default
  {
    if (!strlen(outfile))
    {
      printf("No previous outfile available, you must give a filename!\n");
      return 0;
    }
    else if (opt_outfile)
    {
      tee_fprintf(stdout, "Currently logging to file '%s'\n", outfile);
      return 0;
    }
    else
      param = outfile;      //resume using the old outfile
  }

  /* eliminate the spaces before the parameters */
  while (my_isspace(charset_info,*param))
    param++;
  strncpy(file_name, param, sizeof(file_name) - 1);
  end= file_name + strlen(file_name);
  /* remove end space from command line */
  while (end > file_name && (my_isspace(charset_info,end[-1]) ||
                             my_iscntrl(charset_info,end[-1])))
    end--;
  end[0]= 0;
  if (end == file_name)
  {
    printf("No outfile specified!\n");
    return 0;
  }
  init_tee(file_name);
  return 0;
}


static int
com_notee(string *, const char *)
{
  if (opt_outfile)
    end_tee();
  tee_fprintf(stdout, "Outfile disabled.\n");
  return 0;
}

/*
  Sorry, this command is not available in Windows.
*/

static int
com_pager(string *, const char *line)
{
  char pager_name[FN_REFLEN], *end;
  const char *param;

  if (status.batch)
    return 0;
  /* Skip spaces in front of the pager command */
  while (my_isspace(charset_info, *line))
    line++;
  /* Skip the pager command */
  param= strchr(line, ' ');
  /* Skip the spaces between the command and the argument */
  while (param && my_isspace(charset_info, *param))
    param++;
  if (!param || !strlen(param)) // if pager was not given, use the default
  {
    if (!default_pager_set)
    {
      tee_fprintf(stdout, "Default pager wasn't set, using stdout.\n");
      opt_nopager=1;
      strcpy(pager, "stdout");
      PAGER= stdout;
      return 0;
    }
    strcpy(pager, default_pager);
  }
  else
  {
    end= strncpy(pager_name, param, sizeof(pager_name)-1);
    end+= strlen(pager_name);
    while (end > pager_name && (my_isspace(charset_info,end[-1]) ||
                                my_iscntrl(charset_info,end[-1])))
      end--;
    end[0]=0;
    strcpy(pager, pager_name);
    strcpy(default_pager, pager_name);
  }
  opt_nopager=0;
  tee_fprintf(stdout, "PAGER set to '%s'\n", pager);
  return 0;
}


static int
com_nopager(string *, const char *)
{
  strcpy(pager, "stdout");
  opt_nopager=1;
  PAGER= stdout;
  tee_fprintf(stdout, "PAGER set to stdout\n");
  return 0;
}

/* If arg is given, exit without errors. This happens on command 'quit' */

static int
com_quit(string *, const char *)
{
  /* let the screen auto close on a normal shutdown */
  status.exit_status=0;
  return 1;
}

static int
com_rehash(string *, const char *)
{
  build_completion_hash(1, 0);
  return 0;
}



static int
com_print(string *buffer,const char *)
{
  tee_puts("--------------", stdout);
  (void) tee_fputs(buffer->c_str(), stdout);
  if ( (buffer->length() == 0)
       || (buffer->c_str())[(buffer->length())-1] != '\n')
    tee_putc('\n', stdout);
  tee_puts("--------------\n", stdout);
  /* If empty buffer */
  return 0;
}

/* ARGSUSED */
static int
com_connect(string *buffer, const char *line)
{
  char *tmp, buff[256];
  bool save_rehash= opt_rehash;
  int error;

  memset(buff, 0, sizeof(buff));
  if (buffer)
  {
    /*
      Two null bytes are needed in the end of buff to allow
      get_arg to find end of string the second time it's called.
    */
    tmp= strncpy(buff, line, sizeof(buff)-2);
#ifdef EXTRA_DEBUG
    tmp[1]= 0;
#endif
    tmp= get_arg(buff, 0);
    if (tmp && *tmp)
    {
      free(current_db);
      current_db= strdup(tmp);
      tmp= get_arg(buff, 1);
      if (tmp)
      {
        free(current_host);
        current_host=strdup(tmp);
      }
    }
    else
    {
      /* Quick re-connect */
      opt_rehash= 0;
    }
    // command used
    assert(buffer!=NULL);
    buffer->clear();
  }
  else
    opt_rehash= 0;
  error=sql_connect(current_host,current_db,current_user,opt_password,0);
  opt_rehash= save_rehash;

  if (connected)
  {
    snprintf(buff, sizeof(buff), "Connection id:    %u",drizzle_con_thread_id(&con));
    put_info(buff,INFO_INFO,0,0);
    snprintf(buff, sizeof(buff), "Current database: %.128s\n",
            current_db ? current_db : "*** NONE ***");
    put_info(buff,INFO_INFO,0,0);
  }
  return error;
}


static int com_source(string *, const char *line)
{
  char source_name[FN_REFLEN], *end;
  const char *param;
  LineBuffer *line_buff;
  int error;
  STATUS old_status;
  FILE *sql_file;

  /* Skip space from file name */
  while (my_isspace(charset_info,*line))
    line++;
  if (!(param = strchr(line, ' ')))    // Skip command name
    return put_info("Usage: \\. <filename> | source <filename>",
                    INFO_ERROR, 0,0);
  while (my_isspace(charset_info,*param))
    param++;
  end= strncpy(source_name,param,sizeof(source_name)-1);
  end+= strlen(source_name);
  while (end > source_name && (my_isspace(charset_info,end[-1]) ||
                               my_iscntrl(charset_info,end[-1])))
    end--;
  end[0]=0;
  internal::unpack_filename(source_name,source_name);
  /* open file name */
  if (!(sql_file = fopen(source_name, "r")))
  {
    char buff[FN_REFLEN+60];
    snprintf(buff, sizeof(buff), "Failed to open file '%s', error: %d", source_name,errno);
    return put_info(buff, INFO_ERROR, 0 ,0);
  }

  line_buff= new(std::nothrow) LineBuffer(opt_max_input_line,sql_file);
  if (line_buff == NULL)
  {
    fclose(sql_file);
    return put_info("Can't initialize LineBuffer", INFO_ERROR, 0, 0);
  }

  /* Save old status */
  old_status=status;
  memset(&status, 0, sizeof(status));

  // Run in batch mode
  status.batch=old_status.batch;
  status.line_buff=line_buff;
  status.file_name=source_name;
  // Empty command buffer
  assert(glob_buffer!=NULL);
  glob_buffer->clear();
  error= read_and_execute(false);
  // Continue as before
  status=old_status;
  fclose(sql_file);
  delete status.line_buff;
  line_buff= status.line_buff= 0;
  return error;
}


/* ARGSUSED */
static int
com_delimiter(string *, const char *line)
{
  char buff[256], *tmp;

  strncpy(buff, line, sizeof(buff) - 1);
  tmp= get_arg(buff, 0);

  if (!tmp || !*tmp)
  {
    put_info("DELIMITER must be followed by a 'delimiter' character or string",
             INFO_ERROR, 0, 0);
    return 0;
  }
  else
  {
    if (strstr(tmp, "\\"))
    {
      put_info("DELIMITER cannot contain a backslash character",
               INFO_ERROR, 0, 0);
      return 0;
    }
  }
  strncpy(delimiter, tmp, sizeof(delimiter) - 1);
  delimiter_length= (int)strlen(delimiter);
  delimiter_str= delimiter;
  return 0;
}

/* ARGSUSED */
static int
com_use(string *, const char *line)
{
  char *tmp, buff[FN_REFLEN + 1];
  int select_db;
  drizzle_result_st result;
  drizzle_return_t ret;

  memset(buff, 0, sizeof(buff));
  strncpy(buff, line, sizeof(buff) - 1);
  tmp= get_arg(buff, 0);
  if (!tmp || !*tmp)
  {
    put_info("USE must be followed by a database name", INFO_ERROR, 0, 0);
    return 0;
  }
  /*
    We need to recheck the current database, because it may change
    under our feet, for example if DROP DATABASE or RENAME DATABASE
    (latter one not yet available by the time the comment was written)
  */
  get_current_db();

  if (!current_db || strcmp(current_db,tmp))
  {
    if (one_database)
    {
      skip_updates= 1;
      select_db= 0;    // don't do drizzleclient_select_db()
    }
    else
      select_db= 2;    // do drizzleclient_select_db() and build_completion_hash()
  }
  else
  {
    /*
      USE to the current db specified.
      We do need to send drizzleclient_select_db() to make server
      update database level privileges, which might
      change since last USE (see bug#10979).
      For performance purposes, we'll skip rebuilding of completion hash.
    */
    skip_updates= 0;
    select_db= 1;      // do only drizzleclient_select_db(), without completion
  }

  if (select_db)
  {
    /*
      reconnect once if connection is down or if connection was found to
      be down during query
    */
    if (!connected && reconnect())
      return opt_reconnect ? -1 : 1;                        // Fatal error
    for (bool try_again= true; try_again; try_again= false)
    {
      if (drizzle_select_db(&con,&result,tmp,&ret) == NULL ||
          ret != DRIZZLE_RETURN_OK)
      {
        if (ret == DRIZZLE_RETURN_ERROR_CODE)
        {
          int error= put_error(&con, &result);
          drizzle_result_free(&result);
          return error;
        }

        if (ret != DRIZZLE_RETURN_SERVER_GONE || !try_again)
          return put_error(&con, NULL);

        if (reconnect())
          return opt_reconnect ? -1 : 1;                      // Fatal error
      }
      else
        drizzle_result_free(&result);
    }
    free(current_db);
    current_db= strdup(tmp);
    if (select_db > 1)
      build_completion_hash(opt_rehash, 1);
  }

  put_info("Database changed",INFO_INFO, 0, 0);
  return 0;
}

static int
com_warnings(string *, const char *)
{
  show_warnings = 1;
  put_info("Show warnings enabled.",INFO_INFO, 0, 0);
  return 0;
}

static int
com_nowarnings(string *, const char *)
{
  show_warnings = 0;
  put_info("Show warnings disabled.",INFO_INFO, 0, 0);
  return 0;
}

/*
  Gets argument from a command on the command line. If get_next_arg is
  not defined, skips the command and returns the first argument. The
  line is modified by adding zero to the end of the argument. If
  get_next_arg is defined, then the function searches for end of string
  first, after found, returns the next argument and adds zero to the
  end. If you ever wish to use this feature, remember to initialize all
  items in the array to zero first.
*/

char *get_arg(char *line, bool get_next_arg)
{
  char *ptr, *start;
  bool quoted= 0, valid_arg= 0;
  char qtype= 0;

  ptr= line;
  if (get_next_arg)
  {
    for (; *ptr; ptr++) ;
    if (*(ptr + 1))
      ptr++;
  }
  else
  {
    /* skip leading white spaces */
    while (my_isspace(charset_info, *ptr))
      ptr++;
    if (*ptr == '\\') // short command was used
      ptr+= 2;
    else
      while (*ptr &&!my_isspace(charset_info, *ptr)) // skip command
        ptr++;
  }
  if (!*ptr)
    return NULL;
  while (my_isspace(charset_info, *ptr))
    ptr++;
  if (*ptr == '\'' || *ptr == '\"' || *ptr == '`')
  {
    qtype= *ptr;
    quoted= 1;
    ptr++;
  }
  for (start=ptr ; *ptr; ptr++)
  {
    if (*ptr == '\\' && ptr[1]) // escaped character
    {
      // Remove the backslash
      strcpy(ptr, ptr+1);
    }
    else if ((!quoted && *ptr == ' ') || (quoted && *ptr == qtype))
    {
      *ptr= 0;
      break;
    }
  }
  valid_arg= ptr != start;
  return valid_arg ? start : NULL;
}


static int
sql_connect(char *host,char *database,char *user,char *password,
                 uint32_t silent)
{
  drizzle_return_t ret;

  if (connected)
  {
    connected= 0;
    drizzle_con_free(&con);
    drizzle_free(&drizzle);
  }
  drizzle_create(&drizzle);
  if (drizzle_con_add_tcp(&drizzle, &con, host, opt_drizzle_port, user,
                          password, database, opt_mysql ? DRIZZLE_CON_MYSQL : DRIZZLE_CON_NONE) == NULL)
  {
    (void) put_error(&con, NULL);
    (void) fflush(stdout);
    return 1;
  }

/* XXX add this back in
  if (opt_connect_timeout)
  {
    uint32_t timeout=opt_connect_timeout;
    drizzleclient_options(&drizzle,DRIZZLE_OPT_CONNECT_TIMEOUT,
                  (char*) &timeout);
  }
*/

/* XXX Do we need this?
  if (safe_updates)
  {
    char init_command[100];
    sprintf(init_command,
            "SET SQL_SAFE_UPDATES=1,SQL_SELECT_LIMIT=%"PRIu32
            ",MAX_JOIN_SIZE=%"PRIu32,
            select_limit, max_join_size);
    drizzleclient_options(&drizzle, DRIZZLE_INIT_COMMAND, init_command);
  }
*/
  if ((ret= drizzle_con_connect(&con)) != DRIZZLE_RETURN_OK)
  {
    if (!silent || (ret != DRIZZLE_RETURN_GETADDRINFO &&
                    ret != DRIZZLE_RETURN_COULD_NOT_CONNECT))
    {
      (void) put_error(&con, NULL);
      (void) fflush(stdout);
      return ignore_errors ? -1 : 1;    // Abort
    }
    return -1;          // Retryable
  }
  connected=1;

  build_completion_hash(opt_rehash, 1);
  return 0;
}


static int
com_status(string *, const char *)
{
/*
  char buff[40];
  uint64_t id;
*/
  drizzle_result_st result;
  drizzle_return_t ret;

  tee_puts("--------------", stdout);
  usage(1);          /* Print version */
  if (connected)
  {
    tee_fprintf(stdout, "\nConnection id:\t\t%lu\n",drizzle_con_thread_id(&con));
    /*
      Don't remove "limit 1",
      it is protection againts SQL_SELECT_LIMIT=0
    */
    if (drizzle_query_str(&con,&result,"select DATABASE(), USER() limit 1",
                          &ret) != NULL && ret == DRIZZLE_RETURN_OK &&
        drizzle_result_buffer(&result) == DRIZZLE_RETURN_OK)
    {
      drizzle_row_t cur=drizzle_row_next(&result);
      if (cur)
      {
        tee_fprintf(stdout, "Current database:\t%s\n", cur[0] ? cur[0] : "");
        tee_fprintf(stdout, "Current user:\t\t%s\n", cur[1]);
      }
      drizzle_result_free(&result);
    }
    else if (ret == DRIZZLE_RETURN_ERROR_CODE)
      drizzle_result_free(&result);
    tee_puts("SSL:\t\t\tNot in use", stdout);
  }
  else
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, "\nNo connection\n");
    vidattr(A_NORMAL);
    return 0;
  }
  if (skip_updates)
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, "\nAll updates ignored to this database\n");
    vidattr(A_NORMAL);
  }
  tee_fprintf(stdout, "Current pager:\t\t%s\n", pager);
  tee_fprintf(stdout, "Using outfile:\t\t'%s'\n", opt_outfile ? outfile : "");
  tee_fprintf(stdout, "Using delimiter:\t%s\n", delimiter);
  tee_fprintf(stdout, "Server version:\t\t%s\n", server_version_string(&con));
  tee_fprintf(stdout, "Protocol version:\t%d\n", drizzle_con_protocol_version(&con));
  tee_fprintf(stdout, "Connection:\t\t%s\n", drizzle_con_host(&con));
/* XXX need to save this from result
  if ((id= drizzleclient_insert_id(&drizzle)))
    tee_fprintf(stdout, "Insert id:\t\t%s\n", internal::llstr(id, buff));
*/

  if (strcmp(drizzle_con_uds(&con), ""))
    tee_fprintf(stdout, "UNIX socket:\t\t%s\n", drizzle_con_uds(&con));
  else
    tee_fprintf(stdout, "TCP port:\t\t%d\n", drizzle_con_port(&con));

  if (safe_updates)
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, "\nNote that you are running in safe_update_mode:\n");
    vidattr(A_NORMAL);
    tee_fprintf(stdout, "\
UPDATEs and DELETEs that don't use a key in the WHERE clause are not allowed.\n\
(One can force an UPDATE/DELETE by adding LIMIT # at the end of the command.)\n \
SELECT has an automatic 'LIMIT %lu' if LIMIT is not used.\n             \
Max number of examined row combination in a join is set to: %lu\n\n",
                select_limit, max_join_size);
  }
  tee_puts("--------------\n", stdout);
  return 0;
}

static const char *
server_version_string(drizzle_con_st *local_con)
{
  static string buf("");
  static bool server_version_string_reserved= false;

  if (!server_version_string_reserved)
  {
    buf.reserve(MAX_SERVER_VERSION_LENGTH);
    server_version_string_reserved= true;
  }
  /* Only one thread calls this, so no synchronization is needed */
  if (buf[0] == '\0')
  {
    drizzle_result_st result;
    drizzle_return_t ret;

    buf.append(drizzle_con_server_version(local_con));

    /* "limit 1" is protection against SQL_SELECT_LIMIT=0 */
    (void)drizzle_query_str(local_con, &result,
                            "select @@version_comment limit 1", &ret);
    if (ret == DRIZZLE_RETURN_OK &&
        drizzle_result_buffer(&result) == DRIZZLE_RETURN_OK)
    {
      drizzle_row_t cur = drizzle_row_next(&result);
      if (cur && cur[0])
      {
        buf.append(" ");
        buf.append(cur[0]);
      }
      drizzle_result_free(&result);
    }
    else if (ret == DRIZZLE_RETURN_ERROR_CODE)
      drizzle_result_free(&result);
  }

  return buf.c_str();
}

static int
put_info(const char *str,INFO_TYPE info_type, uint32_t error, const char *sqlstate)
{
  FILE *file= (info_type == INFO_ERROR ? stderr : stdout);
  static int inited=0;

  if (status.batch)
  {
    if (info_type == INFO_ERROR)
    {
      (void) fflush(file);
      fprintf(file,"ERROR");
      if (error)
      {
        if (sqlstate)
          (void) fprintf(file," %d (%s)",error, sqlstate);
        else
          (void) fprintf(file," %d",error);
      }
      if (status.query_start_line && line_numbers)
      {
        (void) fprintf(file," at line %"PRIu32,status.query_start_line);
        if (status.file_name)
          (void) fprintf(file," in file: '%s'", status.file_name);
      }
      (void) fprintf(file,": %s\n",str);
      (void) fflush(file);
      if (!ignore_errors)
        return 1;
    }
    else if (info_type == INFO_RESULT && verbose > 1)
      tee_puts(str, file);
    if (unbuffered)
      fflush(file);
    return info_type == INFO_ERROR ? -1 : 0;
  }
  if (!opt_silent || info_type == INFO_ERROR)
  {
    if (!inited)
    {
      inited=1;
#ifdef HAVE_SETUPTERM
      (void) setupterm((char *)0, 1, (int *) 0);
#endif
    }
    if (info_type == INFO_ERROR)
    {
      if (!opt_nobeep)
        /* This should make a bell */
        putchar('\a');
      vidattr(A_STANDOUT);
      if (error)
      {
        if (sqlstate)
          (void) tee_fprintf(file, "ERROR %d (%s): ", error, sqlstate);
        else
          (void) tee_fprintf(file, "ERROR %d: ", error);
      }
      else
        tee_puts("ERROR: ", file);
    }
    else
      vidattr(A_BOLD);
    (void) tee_puts(str, file);
    vidattr(A_NORMAL);
  }
  if (unbuffered)
    fflush(file);
  return info_type == INFO_ERROR ? -1 : 0;
}


static int
put_error(drizzle_con_st *local_con, drizzle_result_st *res)
{
  const char *error;

  if (res != NULL)
  {
    error= drizzle_result_error(res);
    if (!strcmp(error, ""))
      error= drizzle_con_error(local_con);
  }
  else
    error= drizzle_con_error(local_con);

  return put_info(error, INFO_ERROR,
                  res == NULL ? drizzle_con_error_code(local_con) :
                                drizzle_result_error_code(res),
                  res == NULL ? drizzle_con_sqlstate(local_con) :
                                drizzle_result_sqlstate(res));
}


static void remove_cntrl(string *buffer)
{
  const char *start=  buffer->c_str();
  const char *end= start + (buffer->length());
  while (start < end && !my_isgraph(charset_info,end[-1]))
    end--;
  uint32_t pos_to_truncate= (end-start);
  if (buffer->length() > pos_to_truncate)
    buffer->erase(pos_to_truncate);
}


void tee_fprintf(FILE *file, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  (void) vfprintf(file, fmt, args);
  va_end(args);

  if (opt_outfile)
  {
    va_start(args, fmt);
    (void) vfprintf(OUTFILE, fmt, args);
    va_end(args);
  }
}


void tee_fputs(const char *s, FILE *file)
{
  fputs(s, file);
  if (opt_outfile)
    fputs(s, OUTFILE);
}


void tee_puts(const char *s, FILE *file)
{
  fputs(s, file);
  fputc('\n', file);
  if (opt_outfile)
  {
    fputs(s, OUTFILE);
    fputc('\n', OUTFILE);
  }
}

void tee_putc(int c, FILE *file)
{
  putc(c, file);
  if (opt_outfile)
    putc(c, OUTFILE);
}

#include <sys/times.h>
#ifdef _SC_CLK_TCK        // For mit-pthreads
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC (sysconf(_SC_CLK_TCK))
#endif

static uint32_t start_timer(void)
{
  struct tms tms_tmp;
  return times(&tms_tmp);
}


/**
   Write as many as 52+1 bytes to buff, in the form of a legible
   duration of time.

   len("4294967296 days, 23 hours, 59 minutes, 60.00 seconds")  ->  52
*/
static void nice_time(double sec,char *buff,bool part_second)
{
  uint32_t tmp;
  ostringstream tmp_buff_str;

  if (sec >= 3600.0*24)
  {
    tmp=(uint32_t) floor(sec/(3600.0*24));
    sec-= 3600.0*24*tmp;
    tmp_buff_str << tmp;

    if (tmp > 1)
      tmp_buff_str << " days ";
    else
      tmp_buff_str << " day ";

  }
  if (sec >= 3600.0)
  {
    tmp=(uint32_t) floor(sec/3600.0);
    sec-=3600.0*tmp;
    tmp_buff_str << tmp;

    if (tmp > 1)
      tmp_buff_str << " hours ";
    else
      tmp_buff_str << " hour ";
  }
  if (sec >= 60.0)
  {
    tmp=(uint32_t) floor(sec/60.0);
    sec-=60.0*tmp;
    tmp_buff_str << tmp << " min ";
  }
  if (part_second)
    tmp_buff_str.precision(2);
  else
    tmp_buff_str.precision(0);
  tmp_buff_str << sec << " sec";
  strcpy(buff, tmp_buff_str.str().c_str());
}


static void end_timer(uint32_t start_time,char *buff)
{
  nice_time((double) (start_timer() - start_time) /
            CLOCKS_PER_SEC,buff,1);
}


static void drizzle_end_timer(uint32_t start_time,char *buff)
{
  buff[0]=' ';
  buff[1]='(';
  end_timer(start_time,buff+2);
  strcpy(strchr(buff, '\0'),")");
}

static const char * construct_prompt()
{
  // Erase the old prompt
  assert(processed_prompt!=NULL);
  processed_prompt->clear();

  // Get the date struct
  time_t  lclock = time(NULL);
  struct tm *t = localtime(&lclock);

  /* parse thru the settings for the prompt */
  for (char *c= current_prompt; *c; (void)*c++)
  {
    if (*c != PROMPT_CHAR)
    {
      processed_prompt->append(c, 1);
    }
    else
    {
      int getHour;
      int getYear;
      /* Room for Dow MMM DD HH:MM:SS YYYY */ 
      char dateTime[32];
      switch (*++c) {
      case '\0':
        // stop it from going beyond if ends with %
        c--;
        break;
      case 'c':
        add_int_to_prompt(++prompt_counter);
        break;
      case 'v':
        if (connected)
          processed_prompt->append(drizzle_con_server_version(&con));
        else
          processed_prompt->append("not_connected");
        break;
      case 'd':
        processed_prompt->append(current_db ? current_db : "(none)");
        break;
      case 'h':
      {
        const char *prompt;
        prompt= connected ? drizzle_con_host(&con) : "not_connected";
        if (strstr(prompt, "Localhost"))
          processed_prompt->append("localhost");
        else
        {
          const char *end=strrchr(prompt,' ');
          if (end != NULL)
            processed_prompt->append(prompt, (end-prompt));
        }
        break;
      }
      case 'p':
      {
        if (!connected)
        {
          processed_prompt->append("not_connected");
          break;
        }

        if (strcmp(drizzle_con_uds(&con), ""))
        {
          const char *pos=strrchr(drizzle_con_uds(&con),'/');
          processed_prompt->append(pos ? pos+1 : drizzle_con_uds(&con));
        }
        else
          add_int_to_prompt(drizzle_con_port(&con));
      }
      break;
      case 'U':
        if (!full_username)
          init_username();
        processed_prompt->append(full_username ? full_username :
                                 (current_user ?  current_user : "(unknown)"));
        break;
      case 'u':
        if (!full_username)
          init_username();
        processed_prompt->append(part_username ? part_username :
                                 (current_user ?  current_user : "(unknown)"));
        break;
      case PROMPT_CHAR:
        {
          processed_prompt->append(PROMPT_CHAR, 1);
        }
        break;
      case 'n':
        {
          processed_prompt->append('\n', 1);
        }
        break;
      case ' ':
      case '_':
        {
          processed_prompt->append(' ', 1);
        }
        break;
      case 'R':
        if (t->tm_hour < 10)
          add_int_to_prompt(0);
        add_int_to_prompt(t->tm_hour);
        break;
      case 'r':
        getHour = t->tm_hour % 12;
        if (getHour == 0)
          getHour=12;
        if (getHour < 10)
          add_int_to_prompt(0);
        add_int_to_prompt(getHour);
        break;
      case 'm':
        if (t->tm_min < 10)
          add_int_to_prompt(0);
        add_int_to_prompt(t->tm_min);
        break;
      case 'y':
        getYear = t->tm_year % 100;
        if (getYear < 10)
          add_int_to_prompt(0);
        add_int_to_prompt(getYear);
        break;
      case 'Y':
        add_int_to_prompt(t->tm_year+1900);
        break;
      case 'D':
        strftime(dateTime, 32, "%a %b %d %H:%M:%S %Y", localtime(&lclock));
        processed_prompt->append(dateTime);
        break;
      case 's':
        if (t->tm_sec < 10)
          add_int_to_prompt(0);
        add_int_to_prompt(t->tm_sec);
        break;
      case 'w':
        processed_prompt->append(day_names[t->tm_wday]);
        break;
      case 'P':
        processed_prompt->append(t->tm_hour < 12 ? "am" : "pm");
        break;
      case 'o':
        add_int_to_prompt(t->tm_mon+1);
        break;
      case 'O':
        processed_prompt->append(month_names[t->tm_mon]);
        break;
      case '\'':
        processed_prompt->append("'");
        break;
      case '"':
        processed_prompt->append("\"");
        break;
      case 'S':
        processed_prompt->append(";");
        break;
      case 't':
        processed_prompt->append("\t");
        break;
      case 'l':
        processed_prompt->append(delimiter_str);
        break;
      default:
        processed_prompt->append(c, 1);
      }
    }
  }
  return processed_prompt->c_str();
}


static void add_int_to_prompt(int toadd)
{
  ostringstream buffer;
  buffer << toadd;
  processed_prompt->append(buffer.str().c_str());
}

static void init_username()
{
/* XXX need this?
  free(full_username);
  free(part_username);

  drizzle_result_st *result;
  if (!drizzleclient_query(&drizzle,"select USER()") &&
      (result=drizzleclient_use_result(&drizzle)))
  {
    drizzle_row_t cur=drizzleclient_fetch_row(result);
    full_username= strdup(cur[0]);
    part_username= strdup(strtok(cur[0],"@"));
    (void) drizzleclient_fetch_row(result);        // Read eof
  }
*/
}

static int com_prompt(string *, const char *line)
{
  const char *ptr=strchr(line, ' ');
  if (ptr == NULL)
    tee_fprintf(stdout, "Returning to default PROMPT of %s\n",
                default_prompt);
  prompt_counter = 0;
  char * tmpptr= strdup(ptr ? ptr+1 : default_prompt);
  if (tmpptr == NULL)
    tee_fprintf(stdout, "Memory allocation error. Not changing prompt\n");
  else
  {
    free(current_prompt);
    current_prompt= tmpptr;
    tee_fprintf(stdout, "PROMPT set to '%s'\n", current_prompt);
  }
  return 0;
}

/*
    strcont(str, set) if str contanies any character in the string set.
    The result is the position of the first found character in str, or NULL
    if there isn't anything found.
*/

static const char * strcont(register const char *str, register const char *set)
{
  register const char * start = (const char *) set;

  while (*str)
  {
    while (*set)
    {
      if (*set++ == *str)
        return ((const char*) str);
    }
    set=start; str++;
  }
  return NULL;
} /* strcont */
