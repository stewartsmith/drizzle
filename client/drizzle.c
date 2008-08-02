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
#include <mystrings/m_ctype.h>
#include <stdarg.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__          // Skip warnings in getopt.h
#endif
#include <readline/history.h>
#include "my_readline.h"
#include <signal.h>
#include <vio/violite.h>
#include <sys/ioctl.h>


#if defined(USE_LIBEDIT_INTERFACE) && defined(HAVE_LOCALE_H)
#include <locale.h>
#endif

#include <libdrizzle/gettext.h>

const char *VER= "14.14";

/* Don't try to make a nice table if the data is too big */
#define MAX_COLUMN_LENGTH       1024

/* Buffer to hold 'version' and 'version_comment' */
#define MAX_SERVER_VERSION_LENGTH     128

/* Array of options to pass to libdrizzled */
#define MAX_SERVER_ARGS               64

void* sql_alloc(unsigned size);       // Don't use drizzled alloc for these
void sql_element_free(void *ptr);

#if defined(HAVE_CURSES_H) && defined(HAVE_TERM_H)
#include <curses.h>
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
#undef VOID
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

#undef bcmp				// Fix problem with new readline

#ifdef HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif
#include <readline/readline.h>

/**
 Make the old readline interface look like the new one.
*/
#ifndef USE_NEW_READLINE_INTERFACE
typedef CPPFunction rl_completion_func_t;
typedef Function rl_compentry_func_t;
#define rl_completion_matches(str, func) \
  completion_matches((char *)str, (CPFunction *)func)
#endif


#if !defined(HAVE_VIDATTR)
#undef vidattr
#define vidattr(A) {}      // Can't get this to work
#endif

#ifdef FN_NO_CASE_SENCE
#define cmp_database(cs,A,B) my_strcasecmp((cs), (A), (B))
#else
#define cmp_database(cs,A,B) strcmp((A),(B))
#endif

#include "completion_hash.h"

#define PROMPT_CHAR '\\'
#define DEFAULT_DELIMITER ";"

typedef struct st_status
{
  int exit_status;
  uint32_t query_start_line;
  char *file_name;
  LINE_BUFFER *line_buff;
  bool batch,add_to_history;
} STATUS;


static HashTable ht;
static char **defaults_argv;

enum enum_info_type { INFO_INFO,INFO_ERROR,INFO_RESULT};
typedef enum enum_info_type INFO_TYPE;

static DRIZZLE drizzle;      /* The connection */
static bool ignore_errors=0,quick=0,
  connected=0,opt_raw_data=0,unbuffered=0,output_tables=0,
  opt_rehash=1,skip_updates=0,safe_updates=0,one_database=0,
  opt_compress=0, using_opt_local_infile=0,
  vertical=0, line_numbers=1, column_names=1,opt_html=0,
  opt_xml=0,opt_nopager=1, opt_outfile=0, named_cmds= 0,
  tty_password= 0, opt_nobeep=0, opt_reconnect=1,
  default_charset_used= 0, opt_secure_auth= 0,
  default_pager_set= 0, opt_sigint_ignore= 0,
  auto_vertical_output= 0,
  show_warnings= 0, executing_query= 0, interrupted_query= 0;
static bool debug_info_flag, debug_check_flag;
static bool column_types_flag;
static bool preserve_comments= 0;
static ulong opt_max_allowed_packet, opt_net_buffer_length;
static int verbose=0,opt_silent=0,opt_drizzle_port=0, opt_local_infile=0;
static uint my_end_arg;
static char * opt_drizzle_unix_port=0;
static int connect_flag=CLIENT_INTERACTIVE;
static char *current_host,*current_db,*current_user=0,*opt_password=0,
  *delimiter_str= 0,*current_prompt=0,
  *default_charset= (char*) MYSQL_DEFAULT_CHARSET_NAME;
static char *histfile;
static char *histfile_tmp;
static DYNAMIC_STRING *glob_buffer;
static DYNAMIC_STRING *processed_prompt= NULL;
static char *default_prompt= NULL;
static char *full_username=0,*part_username=0;
static STATUS status;
static uint32_t select_limit;
static uint32_t max_join_size;
static ulong opt_connect_timeout= 0;
static char drizzle_charsets_dir[FN_REFLEN+1];
static const char *xmlmeta[] = {
  "&", "&amp;",
  "<", "&lt;",
  ">", "&gt;",
  "\"", "&quot;",
  0, 0
};
// TODO: Need to i18n these
static const char *day_names[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *month_names[]={"Jan","Feb","Mar","Apr","May","Jun","Jul",
                                  "Aug","Sep","Oct","Nov","Dec"};
static char default_pager[FN_REFLEN];
static char pager[FN_REFLEN], outfile[FN_REFLEN];
static FILE *PAGER, *OUTFILE;
static MEM_ROOT hash_mem_root;
static uint prompt_counter;
static char delimiter[16]= DEFAULT_DELIMITER;
static uint delimiter_length= 1;
unsigned short terminal_width= 80;

static uint opt_protocol= DRIZZLE_PROTOCOL_TCP;
static CHARSET_INFO *charset_info= &my_charset_latin1;

const char *default_dbug_option="d:t:o,/tmp/drizzle.trace";
int drizzle_real_query_for_lazy(const char *buf, int length);
int drizzle_store_result_for_lazy(DRIZZLE_RES **result);


void tee_fprintf(FILE *file, const char *fmt, ...);
void tee_fputs(const char *s, FILE *file);
void tee_puts(const char *s, FILE *file);
void tee_putc(int c, FILE *file);
static void tee_print_sized_data(const char *, unsigned int, unsigned int, bool);
/* The names of functions that actually do the manipulation. */
static int get_options(int argc,char **argv);
bool get_one_option(int optid, const struct my_option *opt,
                    char *argument);
static int com_quit(DYNAMIC_STRING *str,char*),
  com_go(DYNAMIC_STRING *str,char*), com_ego(DYNAMIC_STRING *str,char*),
  com_print(DYNAMIC_STRING *str,char*),
  com_help(DYNAMIC_STRING *str,char*), com_clear(DYNAMIC_STRING *str,char*),
  com_connect(DYNAMIC_STRING *str,char*), com_status(DYNAMIC_STRING *str,char*),
  com_use(DYNAMIC_STRING *str,char*), com_source(DYNAMIC_STRING *str, char*),
  com_rehash(DYNAMIC_STRING *str, char*), com_tee(DYNAMIC_STRING *str, char*),
  com_notee(DYNAMIC_STRING *str, char*), com_charset(DYNAMIC_STRING *str,char*),
  com_prompt(DYNAMIC_STRING *str, char*), com_delimiter(DYNAMIC_STRING *str, char*),
  com_warnings(DYNAMIC_STRING *str, char*), com_nowarnings(DYNAMIC_STRING *str, char*),
  com_nopager(DYNAMIC_STRING *str, char*), com_pager(DYNAMIC_STRING *str, char*);

static int read_and_execute(bool interactive);
static int sql_connect(char *host,char *database,char *user,char *password,
                       uint silent);
static const char *server_version_string(DRIZZLE *drizzle);
static int put_info(const char *str,INFO_TYPE info,uint error,
                    const char *sql_state);
static int put_error(DRIZZLE *drizzle);
static void safe_put_field(const char *pos,ulong length);
static void xmlencode_print(const char *src, uint length);
static void init_pager(void);
static void end_pager(void);
static void init_tee(const char *);
static void end_tee(void);
static const char* construct_prompt(void);
static char *get_arg(char *line, bool get_next_arg);
static void init_username(void);
static void add_int_to_prompt(int toadd);
static int get_result_width(DRIZZLE_RES *res);
static int get_field_disp_length(DRIZZLE_FIELD * field);

/* A structure which contains information on the commands this program
   can understand. */
typedef struct {
  const char *name;        /* User printable name of the function. */
  char cmd_char;        /* msql command character */
  int (*func)(DYNAMIC_STRING *str,char *); /* Function to call to do the job. */
  bool takes_params;        /* Max parameters for command */
  const char *doc;        /* Documentation for this function.  */
} COMMANDS;


static COMMANDS commands[] = {
  { "?",      '?', com_help,   1, gettext_noop("Synonym for `help'.") },
  { "clear",  'c', com_clear,  0, gettext_noop("Clear command.")},
  { "connect",'r', com_connect,1,
    gettext_noop("Reconnect to the server. Optional arguments are db and host." }),
  { "delimiter", 'd', com_delimiter,    1,
    gettext_noop("Set statement delimiter. NOTE: Takes the rest of the line as new delimiter.") },
  { "ego",    'G', com_ego,    0,
    gettext_noop("Send command to drizzle server, display result vertically.")},
  { "exit",   'q', com_quit,   0, gettext_noop("Exit drizzle. Same as quit.")},
  { "go",     'g', com_go,     0, gettext_noop("Send command to drizzle server.") },
  { "help",   'h', com_help,   1, gettext_noop("Display this help.") },
  { "nopager",'n', com_nopager,0, gettext_noop("Disable pager, print to stdout.") },
  { "notee",  't', com_notee,  0, gettext_noop("Don't write into outfile.") },
  { "pager",  'P', com_pager,  1,
    gettext_noop("Set PAGER [to_pager]. Print the query results via PAGER.") },
  { "print",  'p', com_print,  0, gettext_noop("Print current command.") },
  { "prompt", 'R', com_prompt, 1, gettext_noop("Change your drizzle prompt.")},
  { "quit",   'q', com_quit,   0, gettext_noop("Quit drizzle.") },
  { "rehash", '#', com_rehash, 0, gettext_noop("Rebuild completion hash.") },
  { "source", '.', com_source, 1,
    gettext_noop("Execute an SQL script file. Takes a file name as an argument.")},
  { "status", 's', com_status, 0, gettext_noop("Get status information from the server.")},
  { "tee",    'T', com_tee,    1,
    gettext_noop("Set outfile [to_outfile]. Append everything into given outfile.") },
  { "use",    'u', com_use,    1,
    gettext_noop("Use another database. Takes database name as argument.") },
  { "charset",    'C', com_charset,    1,
    gettext_noop("Switch to another charset. Might be needed for processing binlog with multi-byte charsets.") },
  { "warnings", 'W', com_warnings,  0,
    gettext_noop("Show warnings after every statement.") },
  { "nowarning", 'w', com_nowarnings, 0,
    gettext_noop("Don't show warnings after every statement.") },
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
  { "CURRENT_TIME", 0, 0, 0, ""},
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
  { "GET_FORMAT", 0, 0, 0, ""},
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
  { "LOCALTIME", 0, 0, 0, ""},
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
  { "RAID0", 0, 0, 0, ""},
  { "RAID_CHUNKS", 0, 0, 0, ""},
  { "RAID_CHUNKSIZE", 0, 0, 0, ""},
  { "RAID_TYPE", 0, 0, 0, ""},
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
  { "TIME", 0, 0, 0, ""},
  { "TIMESTAMP", 0, 0, 0, ""},
  { "TIMESTAMPADD", 0, 0, 0, ""},
  { "TIMESTAMPDIFF", 0, 0, 0, ""},
  { "TINYINT", 0, 0, 0, ""},
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
  { "UTC_TIME", 0, 0, 0, ""},
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
  { "ADDTIME", 0, 0, 0, ""},
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
  { "BIT_COUNT", 0, 0, 0, ""},
  { "BIT_OR", 0, 0, 0, ""},
  { "BIT_AND", 0, 0, 0, ""},
  { "BIT_XOR", 0, 0, 0, ""},
  { "CAST", 0, 0, 0, ""},
  { "CEIL", 0, 0, 0, ""},
  { "CEILING", 0, 0, 0, ""},
  { "BIT_LENGTH", 0, 0, 0, ""},
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
  { "CURTIME", 0, 0, 0, ""},
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
  { "MAKETIME", 0, 0, 0, ""},
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
  { "SEC_TO_TIME", 0, 0, 0, ""},
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
  { "SUBTIME", 0, 0, 0, ""},
  { "SUM", 0, 0, 0, ""},
  { "SYSDATE", 0, 0, 0, ""},
  { "SYSTEM_USER", 0, 0, 0, ""},
  { "TAN", 0, 0, 0, ""},
  { "TIME_FORMAT", 0, 0, 0, ""},
  { "TIME_TO_SEC", 0, 0, 0, ""},
  { "TIMEDIFF", 0, 0, 0, ""},
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

static int         embedded_server_arg_count= 0;
static char       *embedded_server_args[MAX_SERVER_ARGS];

int history_length;
static int not_in_history(const char *line);
static void initialize_readline (const char *name);
static void fix_history(DYNAMIC_STRING *final_command);

static COMMANDS *find_command(char *name,char cmd_name);
static bool add_line(DYNAMIC_STRING *buffer,char *line,char *in_string,
                     bool *ml_comment);
static void remove_cntrl(DYNAMIC_STRING *buffer);
static void print_table_data(DRIZZLE_RES *result);
static void print_table_data_html(DRIZZLE_RES *result);
static void print_table_data_xml(DRIZZLE_RES *result);
static void print_tab_data(DRIZZLE_RES *result);
static void print_table_data_vertically(DRIZZLE_RES *result);
static void print_warnings(void);
static ulong start_timer(void);
static void end_timer(ulong start_time,char *buff);
static void drizzle_end_timer(ulong start_time,char *buff);
static void nice_time(double sec,char *buff,bool part_second);
extern sig_handler drizzle_end(int sig);
extern sig_handler handle_sigint(int sig);
#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
static sig_handler window_resize(int sig);
#endif

int main(int argc,char *argv[])
{
  char buff[80];

  setlocale(LC_ALL, "");
  bindtextdomain("drizzle", LOCALEDIR);
  textdomain("drizzle");

  MY_INIT(argv[0]);
  delimiter_str= delimiter;
  default_prompt= my_strdup(getenv("DRIZZLE_PS1") ?
                            getenv("DRIZZLE_PS1") :
                            "drizzle>> ", MYF(0));
  current_prompt= my_strdup(default_prompt, MYF(0));

  processed_prompt= (DYNAMIC_STRING *)my_malloc(sizeof(DYNAMIC_STRING), MYF(0));
  init_dynamic_string(processed_prompt, "", 32, 32);

  prompt_counter=0;

  outfile[0]=0;      // no (default) outfile
  strmov(pager, "stdout");  // the default, if --pager wasn't given
  {
    char *tmp=getenv("PAGER");
    if (tmp && strlen(tmp))
    {
      default_pager_set= 1;
      strmov(default_pager, tmp);
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

  load_defaults("my",load_default_groups,&argc,&argv);
  defaults_argv=argv;
  if (get_options(argc, (char **) argv))
  {
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }
  if (status.batch && !status.line_buff &&
      !(status.line_buff=batch_readline_init(opt_max_allowed_packet+512,stdin)))
  {
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }
  completion_hash_init(&ht, 128);
  init_alloc_root(&hash_mem_root, 16384, 0);
  memset((char*) &drizzle, 0, sizeof(drizzle));
  if (sql_connect(current_host,current_db,current_user,opt_password,
                  opt_silent))
  {
    quick= 1;          // Avoid history
    status.exit_status= 1;
    drizzle_end(-1);
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

  glob_buffer= (DYNAMIC_STRING *)my_malloc(sizeof(DYNAMIC_STRING), MYF(0));
  init_dynamic_string(glob_buffer, "", 512, 512);

  /* this is a slight abuse of the DYNAMIC_STRING interface. deal. */
  sprintf(glob_buffer->str,
          _("Your Drizzle connection id is %u\nServer version: %s\n"),
          drizzle_thread_id(&drizzle), server_version_string(&drizzle));
  put_info(glob_buffer->str, INFO_INFO, 0, 0);
  dynstr_set(glob_buffer, NULL);

  initialize_readline(my_progname);
  if (!status.batch && !quick && !opt_html && !opt_xml)
  {
    /* read-history from file, default ~/.drizzle_history*/
    if (getenv("MYSQL_HISTFILE"))
      histfile= strdup(getenv("MYSQL_HISTFILE"));
    else if (getenv("HOME"))
    {
      histfile=(char*) my_malloc((uint) strlen(getenv("HOME"))
                                 + (uint) strlen("/.drizzle_history")+2,
                                 MYF(MY_WME));
      if (histfile)
        sprintf(histfile,"%s/.drizzle_history",getenv("HOME"));
      char link_name[FN_REFLEN];
      if (my_readlink(link_name, histfile, 0) == 0 &&
          strncmp(link_name, "/dev/null", 10) == 0)
      {
        /* The .drizzle_history file is a symlink to /dev/null, don't use it */
        my_free(histfile, MYF(MY_ALLOW_ZERO_PTR));
        histfile= 0;
      }
    }
    if (histfile)
    {
      if (verbose)
        tee_fprintf(stdout, _("Reading history-file %s\n"),histfile);
      read_history(histfile);
      if (!(histfile_tmp= (char*) my_malloc((uint) strlen(histfile) + 5,
                                            MYF(MY_WME))))
      {
        fprintf(stderr, _("Couldn't allocate memory for temp histfile!\n"));
        exit(1);
      }
      sprintf(histfile_tmp, "%s.TMP", histfile);
    }
  }
  sprintf(buff, "%s",
          _("Type 'help;' or '\\h' for help. Type '\\c' to clear the buffer.\n"));

  put_info(buff,INFO_INFO,0,0);
  status.exit_status= read_and_execute(!status.batch);
  if (opt_outfile)
    end_tee();
  drizzle_end(0);
#ifndef _lint
  return(0);        // Keep compiler happy
#endif
}

sig_handler drizzle_end(int sig)
{
  drizzle_close(&drizzle);
  if (!status.batch && !quick && !opt_html && !opt_xml && histfile)
  {
    /* write-history */
    if (verbose)
      tee_fprintf(stdout, _("Writing history-file %s\n"),histfile);
    if (!write_history(histfile_tmp))
      my_rename(histfile_tmp, histfile, MYF(MY_WME));
  }
  batch_readline_end(status.line_buff);
  completion_hash_free(&ht);
  free_root(&hash_mem_root,MYF(0));

  if (sig >= 0)
    put_info(sig ? _("Aborted") : _("Bye"), INFO_RESULT,0,0);
  if (glob_buffer)
    dynstr_free(glob_buffer);
  my_free(glob_buffer, MYF(MY_ALLOW_ZERO_PTR));
  if (processed_prompt)
    dynstr_free(processed_prompt);
  my_free(processed_prompt,MYF(MY_ALLOW_ZERO_PTR));
  my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
  my_free(opt_drizzle_unix_port,MYF(MY_ALLOW_ZERO_PTR));
  my_free(histfile,MYF(MY_ALLOW_ZERO_PTR));
  my_free(histfile_tmp,MYF(MY_ALLOW_ZERO_PTR));
  my_free(current_db,MYF(MY_ALLOW_ZERO_PTR));
  my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
  my_free(current_user,MYF(MY_ALLOW_ZERO_PTR));
  my_free(full_username,MYF(MY_ALLOW_ZERO_PTR));
  my_free(part_username,MYF(MY_ALLOW_ZERO_PTR));
  my_free(default_prompt,MYF(MY_ALLOW_ZERO_PTR));
  my_free(current_prompt,MYF(MY_ALLOW_ZERO_PTR));
  while (embedded_server_arg_count > 1)
    my_free(embedded_server_args[--embedded_server_arg_count],MYF(0));
  drizzle_server_end();
  free_defaults(defaults_argv);
  my_end(my_end_arg);
  exit(status.exit_status);
}


/*
  This function handles sigint calls
  If query is in process, kill query
  no query in process, terminate like previous behavior
*/
sig_handler handle_sigint(int sig)
{
  char kill_buffer[40];
  DRIZZLE *kill_drizzle= NULL;

  /* terminate if no query being executed, or we already tried interrupting */
  if (!executing_query || interrupted_query) {
    goto err;
  }

  kill_drizzle= drizzle_create(kill_drizzle);
  if (!drizzle_connect(kill_drizzle,current_host, current_user, opt_password,
                          "", opt_drizzle_port, opt_drizzle_unix_port,0))
  {
    goto err;
  }

  /* kill_buffer is always big enough because max length of %lu is 15 */
  sprintf(kill_buffer, "KILL /*!50000 QUERY */ %u", drizzle_thread_id(&drizzle));
  drizzle_real_query(kill_drizzle, kill_buffer, strlen(kill_buffer));
  drizzle_close(kill_drizzle);
  tee_fprintf(stdout, _("Query aborted by Ctrl+C\n"));

  interrupted_query= 1;

  return;

err:
  drizzle_end(sig);
}


#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
sig_handler window_resize(int sig __attribute__((unused)))
{
  struct winsize window_size;

  if (ioctl(fileno(stdin), TIOCGWINSZ, &window_size) == 0)
    terminal_width= window_size.ws_col;
}
#endif

static struct my_option my_long_options[] =
{
  {"help", '?', gettext_noop("Display this help and exit."), 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"help", 'I', gettext_noop("Synonym for -?"), 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"auto-rehash", OPT_AUTO_REHASH,
   gettext_noop("Enable automatic rehashing. One doesn't need to use 'rehash' to get table and field completion, but startup and reconnecting may take a longer time. Disable with --disable-auto-rehash."),
   (char**) &opt_rehash, (char**) &opt_rehash, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0,
   0, 0},
  {"no-auto-rehash", 'A',
   gettext_noop("No automatic rehashing. One has to use 'rehash' to get table and field completion. This gives a quicker start of DRIZZLE and disables rehashing on reconnect. WARNING: options deprecated; use --disable-auto-rehash instead."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-vertical-output", OPT_AUTO_VERTICAL_OUTPUT,
   gettext_noop("Automatically switch to vertical output mode if the result is wider than the terminal width."),
   (char**) &auto_vertical_output, (char**) &auto_vertical_output, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"batch", 'B',
   gettext_noop("Don't use history file. Disable interactive behavior. (Enables --silent)"), 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   gettext_noop("Directory where character sets are."), (char**) &charsets_dir,
   (char**) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"column-type-info", OPT_COLUMN_TYPES, gettext_noop("Display column type information."),
   (char**) &column_types_flag, (char**) &column_types_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"comments", 'c', gettext_noop("Preserve comments. Send comments to the server. The default is --skip-comments (discard comments), enable with --comments"),
   (char**) &preserve_comments, (char**) &preserve_comments,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', gettext_noop("Use compression in server/client protocol."),
   (char**) &opt_compress, (char**) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"debug-check", OPT_DEBUG_CHECK, gettext_noop("Check memory and open file usage at exit ."),
   (char**) &debug_check_flag, (char**) &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", 'T', gettext_noop("Print some debug info at exit."), (char**) &debug_info_flag,
   (char**) &debug_info_flag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"database", 'D', gettext_noop("Database to use."), (char**) &current_db,
   (char**) &current_db, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   gettext_noop("Set the default character set."), (char**) &default_charset,
   (char**) &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"delimiter", OPT_DELIMITER, gettext_noop("Delimiter to be used."), (char**) &delimiter_str,
   (char**) &delimiter_str, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"execute", 'e', gettext_noop("Execute command and quit. (Disables --force and history file)"), 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"vertical", 'E', gettext_noop("Print the output of a query (rows) vertically."),
   (char**) &vertical, (char**) &vertical, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"force", 'f', gettext_noop("Continue even if we get an sql error."),
   (char**) &ignore_errors, (char**) &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"named-commands", 'G',
   gettext_noop("Enable named commands. Named commands mean this program's internal commands; see drizzle> help . When enabled, the named commands can be used from any line of the query, otherwise only from the first line, before an enter. Disable with --disable-named-commands. This option is disabled by default."),
   (char**) &named_cmds, (char**) &named_cmds, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"no-named-commands", 'g',
   gettext_noop("Named commands are disabled. Use \\* form only, or use named commands only in the beginning of a line ending with a semicolon (;) Since version 10.9 the client now starts with this option ENABLED by default! Disable with '-G'. Long format commands still work from the first line. WARNING: option deprecated; use --disable-named-commands instead."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"ignore-spaces", 'i', gettext_noop("Ignore space after function names."), 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"local-infile", OPT_LOCAL_INFILE, gettext_noop("Enable/disable LOAD DATA LOCAL INFILE."),
   (char**) &opt_local_infile,
   (char**) &opt_local_infile, 0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"no-beep", 'b', gettext_noop("Turn off beep on error."), (char**) &opt_nobeep,
   (char**) &opt_nobeep, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', gettext_noop("Connect to host."), (char**) &current_host,
   (char**) &current_host, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"html", 'H', gettext_noop("Produce HTML output."), (char**) &opt_html, (char**) &opt_html,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"xml", 'X', gettext_noop("Produce XML output"), (char**) &opt_xml, (char**) &opt_xml, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"line-numbers", OPT_LINE_NUMBERS, gettext_noop("Write line numbers for errors."),
   (char**) &line_numbers, (char**) &line_numbers, 0, GET_BOOL,
   NO_ARG, 1, 0, 0, 0, 0, 0},
  {"skip-line-numbers", 'L', gettext_noop("Don't write line number for errors. WARNING: -L is deprecated, use long version of this option instead."), 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"unbuffered", 'n', gettext_noop("Flush buffer after each query."), (char**) &unbuffered,
   (char**) &unbuffered, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"column-names", OPT_COLUMN_NAMES, gettext_noop("Write column names in results."),
   (char**) &column_names, (char**) &column_names, 0, GET_BOOL,
   NO_ARG, 1, 0, 0, 0, 0, 0},
  {"skip-column-names", 'N',
   gettext_noop("Don't write column names in results. WARNING: -N is deprecated, use long version of this options instead."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"set-variable", 'O',
   gettext_noop("Change the value of a variable. Please note that this option is deprecated; you can set variables directly with --variable-name=value."),
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"sigint-ignore", OPT_SIGINT_IGNORE, gettext_noop("Ignore SIGINT (CTRL-C)"),
   (char**) &opt_sigint_ignore,  (char**) &opt_sigint_ignore, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"one-database", 'o',
   gettext_noop("Only update the default database. This is useful for skipping updates to other database in the update log."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"pager", OPT_PAGER,
   gettext_noop("Pager to use to display results. If you don't supply an option the default pager is taken from your ENV variable PAGER. Valid pagers are less, more, cat [> filename], etc. See interactive help (\\h) also. This option does not work in batch mode. Disable with --disable-pager. This option is disabled by default."),
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"no-pager", OPT_NOPAGER,
   gettext_noop("Disable pager and print to stdout. See interactive help (\\h) also. WARNING: option deprecated; use --disable-pager instead."),
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   gettext_noop("Password to use when connecting to server. If password is not given it's asked from the tty."),
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', gettext_noop("Port number to use for connection or 0 for default to, in order of preference, my.cnf, $MYSQL_TCP_PORT, ")
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   gettext_noop("built-in default") " (" STRINGIFY_ARG(MYSQL_PORT) ").",
   (char**) &opt_drizzle_port,
   (char**) &opt_drizzle_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0,  0},
  {"prompt", OPT_PROMPT, gettext_noop("Set the drizzle prompt to this value."),
   (char**) &current_prompt, (char**) &current_prompt, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_DRIZZLE_PROTOCOL, gettext_noop("The protocol of connection (tcp,socket,pipe,memory)."),
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q',
   gettext_noop("Don't cache result, print it row by row. This may slow down the server if the output is suspended. Doesn't use history file."),
   (char**) &quick, (char**) &quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"raw", 'r', gettext_noop("Write fields without conversion. Used with --batch."),
   (char**) &opt_raw_data, (char**) &opt_raw_data, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"reconnect", OPT_RECONNECT, gettext_noop("Reconnect if the connection is lost. Disable with --disable-reconnect. This option is enabled by default."),
   (char**) &opt_reconnect, (char**) &opt_reconnect, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"silent", 's', gettext_noop("Be more silent. Print results with a tab as separator, each row on new line."), 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"socket", 'S', gettext_noop("Socket file to use for connection."),
   (char**) &opt_drizzle_unix_port, (char**) &opt_drizzle_unix_port, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"table", 't', gettext_noop("Output in table format."), (char**) &output_tables,
   (char**) &output_tables, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"tee", OPT_TEE,
   gettext_noop("Append everything into outfile. See interactive help (\\h) also. Does not work in batch mode. Disable with --disable-tee. This option is disabled by default."),
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"no-tee", OPT_NOTEE, gettext_noop("Disable outfile. See interactive help (\\h) also. WARNING: option deprecated; use --disable-tee instead"), 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', gettext_noop("User for login if not current user."), (char**) &current_user,
   (char**) &current_user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"safe-updates", 'U', gettext_noop("Only allow UPDATE and DELETE that uses keys."),
   (char**) &safe_updates, (char**) &safe_updates, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"i-am-a-dummy", 'U', gettext_noop("Synonym for option --safe-updates, -U."),
   (char**) &safe_updates, (char**) &safe_updates, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"verbose", 'v', gettext_noop("Write more. (-v -v -v gives the table output format)."), 0,
   0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', gettext_noop("Output version information and exit."), 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"wait", 'w', gettext_noop("Wait and retry if connection is down."), 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT,
   gettext_noop("Number of seconds before connection timeout."),
   (char**) &opt_connect_timeout,
   (char**) &opt_connect_timeout, 0, GET_ULONG, REQUIRED_ARG, 0, 0, 3600*12, 0,
   0, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET,
   gettext_noop("Max packet length to send to, or receive from server"),
   (char**) &opt_max_allowed_packet, (char**) &opt_max_allowed_packet, 0,
   GET_ULONG, REQUIRED_ARG, 16 *1024L*1024L, 4096,
   (int64_t) 2*1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"net_buffer_length", OPT_NET_BUFFER_LENGTH,
   gettext_noop("Buffer for TCP/IP and socket communication"),
   (char**) &opt_net_buffer_length, (char**) &opt_net_buffer_length, 0, GET_ULONG,
   REQUIRED_ARG, 16384, 1024, 512*1024*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"select_limit", OPT_SELECT_LIMIT,
   gettext_noop("Automatic limit for SELECT when using --safe-updates"),
   (char**) &select_limit,
   (char**) &select_limit, 0, GET_ULONG, REQUIRED_ARG, 1000L, 1, ULONG_MAX,
   0, 1, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   gettext_noop("Automatic limit for rows in a join when using --safe-updates"),
   (char**) &max_join_size,
   (char**) &max_join_size, 0, GET_ULONG, REQUIRED_ARG, 1000000L, 1, ULONG_MAX,
   0, 1, 0},
  {"secure-auth", OPT_SECURE_AUTH, gettext_noop("Refuse client connecting to server if it uses old (pre-4.1.1) protocol"), (char**) &opt_secure_auth,
   (char**) &opt_secure_auth, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"server-arg", OPT_SERVER_ARG, gettext_noop("Send embedded server this as a parameter."),
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"show-warnings", OPT_SHOW_WARNINGS, gettext_noop("Show warnings after every statement."),
   (char**) &show_warnings, (char**) &show_warnings, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void usage(int version)
{
  const char* readline= "readline";

  printf(_("%s  Ver %s Distrib %s, for %s (%s) using %s %s\n"),
         my_progname, VER, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE,
         readline, rl_library_version);

  if (version)
    return;
  printf(_("\
Copyright (C) 2000-2008 MySQL AB\n                                      \
This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n \
and you are welcome to modify and redistribute it under the GPL license\n"));
  printf(_("Usage: %s [OPTIONS] [database]\n"), my_progname);
  my_print_help(my_long_options);
  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}


bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
               char *argument)
{
  switch(optid) {
  case OPT_CHARSETS_DIR:
    strmake(drizzle_charsets_dir, argument, sizeof(drizzle_charsets_dir) - 1);
    charsets_dir = drizzle_charsets_dir;
    break;
  case  OPT_DEFAULT_CHARSET:
    default_charset_used= 1;
    break;
  case OPT_DELIMITER:
    if (argument == disabled_my_option)
    {
      strmov(delimiter, DEFAULT_DELIMITER);
    }
    else
    {
      /* Check that delimiter does not contain a backslash */
      if (!strstr(argument, "\\"))
      {
        strmake(delimiter, argument, sizeof(delimiter) - 1);
      }
      else
      {
        put_info(_("DELIMITER cannot contain a backslash character"),
                 INFO_ERROR,0,0);
        return 0;
      }
    }
    delimiter_length= (uint)strlen(delimiter);
    delimiter_str= delimiter;
    break;
  case OPT_LOCAL_INFILE:
    using_opt_local_infile=1;
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
        strmake(pager, argument, sizeof(pager) - 1);
        strmov(default_pager, pager);
      }
      else if (default_pager_set)
        strmov(pager, default_pager);
      else
        opt_nopager= 1;
    }
    break;
  case OPT_NOPAGER:
    printf(_("WARNING: option deprecated; use --disable-pager instead.\n"));
    opt_nopager= 1;
    break;
  case OPT_DRIZZLE_PROTOCOL:
    opt_protocol= find_type_or_exit(argument, &sql_protocol_typelib,
                                    opt->name);
    break;
  case OPT_SERVER_ARG:
    printf(("WARNING: --server-arg option not supported in this configuration.\n"));
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
    if (!status.line_buff)
      ignore_errors= 0;                         // do it for the first -e only
    if (!(status.line_buff= batch_readline_command(status.line_buff, argument)))
      return 1;
    break;
  case 'o':
    if (argument == disabled_my_option)
      one_database= 0;
    else
      one_database= skip_updates= 1;
    break;
  case 'p':
    if (argument == disabled_my_option)
      argument= (char*) "";      // Don't require password
    if (argument)
    {
      char *start= argument;
      my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
      opt_password= strdup(argument);
      while (*argument) *argument++= 'x';        // Destroy argument
      if (*start)
        start[1]=0 ;
      tty_password= 0;
    }
    else
      tty_password= 1;
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
  const DRIZZLE_PARAMETERS *drizzle_params= drizzle_get_parameters();

  tmp= (char *) getenv("MYSQL_HOST");
  if (tmp)
    current_host= strdup(tmp);

  pagpoint= getenv("PAGER");
  if (!((char*) (pagpoint)))
  {
    strmov(pager, "stdout");
    opt_nopager= 1;
  }
  else
    strmov(pager, pagpoint);
  strmov(default_pager, pager);

  opt_max_allowed_packet= *drizzle_params->p_max_allowed_packet;
  opt_net_buffer_length= *drizzle_params->p_net_buffer_length;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  *drizzle_params->p_max_allowed_packet= opt_max_allowed_packet;
  *drizzle_params->p_net_buffer_length= opt_net_buffer_length;

  if (status.batch) /* disable pager and outfile in this case */
  {
    strmov(default_pager, "stdout");
    strmov(pager, "stdout");
    opt_nopager= 1;
    default_pager_set= 0;
    opt_outfile= 0;
    opt_reconnect= 0;
    connect_flag= 0; /* Not in interactive mode */
  }

  if (strcmp(default_charset, charset_info->csname) &&
      !(charset_info= get_charset_by_csname(default_charset,
                                            MY_CS_PRIMARY, MYF(MY_WME))))
    exit(1);
  if (argc > 1)
  {
    usage(0);
    exit(1);
  }
  if (argc == 1)
  {
    skip_updates= 0;
    my_free(current_db, MYF(MY_ALLOW_ZERO_PTR));
    current_db= strdup(*argv);
  }
  if (tty_password)
    opt_password= get_tty_password(NullS);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;
  return(0);
}

static int read_and_execute(bool interactive)
{
  char *line;
  char in_string=0;
  ulong line_number=0;
  bool ml_comment= 0;
  COMMANDS *com;
  status.exit_status=1;

  for (;;)
  {
    if (!interactive)
    {
      line=batch_readline(status.line_buff);
      /*
        Skip UTF8 Byte Order Marker (BOM) 0xEFBBBF.
        Editors like "notepad" put this marker in
        the very beginning of a text file when
        you save the file using "Unicode UTF-8" format.
      */
      if (!line_number &&
          (uchar) line[0] == 0xEF &&
          (uchar) line[1] == 0xBB &&
          (uchar) line[2] == 0xBF)
        line+= 3;
      line_number++;
      if (glob_buffer->length!=0)
        status.query_start_line=line_number;
    }
    else
    {
      char *prompt= (char*) (ml_comment ? "   /*> " :
                             (glob_buffer->length == 0) ?  construct_prompt() :
                             !in_string ? "    -> " :
                             in_string == '\'' ?
                             "    '> " : (in_string == '`' ?
                                          "    `> " :
                                          "    \"> "));
      if (opt_outfile && (glob_buffer->length==0))
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
    if ((named_cmds || (glob_buffer->length==0))
        && !ml_comment && !in_string && (com=find_command(line,0)))
    {
      if ((*com->func)(glob_buffer,line) > 0)
        break;
      // If buffer was emptied
      if (glob_buffer->length==0)
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
    if (glob_buffer->length != 0)
    {
      status.exit_status=1;
      if (com_go(glob_buffer,line) <= 0)
        status.exit_status=0;
    }
  }

  return status.exit_status;
}


static COMMANDS *find_command(char *name,char cmd_char)
{
  uint len;
  char *end;

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
                                                (uchar*) name, 9,
                                                (const uchar*) "delimiter",
                                                9))))
      return((COMMANDS *) 0);
    if ((end=strcont(name," \t")))
    {
      len=(uint) (end - name);
      while (my_isspace(charset_info,*end))
        end++;
      if (!*end)
        end=0;          // no arguments to function
    }
    else
      len=(uint) strlen(name);
  }

  for (uint i= 0; commands[i].name; i++)
  {
    if (commands[i].func &&
        ((name && !my_strnncoll(charset_info,(uchar*)name,len, (uchar*)commands[i].name,len) && !commands[i].name[len] && (!end || (end && commands[i].takes_params))) || (!name && commands[i].cmd_char == cmd_char)))
    {
      return(&commands[i]);
    }
  }
  return((COMMANDS *) 0);
}


static bool add_line(DYNAMIC_STRING *buffer,char *line,char *in_string,
                        bool *ml_comment)
{
  uchar inchar;
  char buff[80], *pos, *out;
  COMMANDS *com;
  bool need_space= 0;
  bool ss_comment= 0;


  if (!line[0] && (buffer->length==0))
    return(0);
  if (status.add_to_history && line[0] && not_in_history(line))
    add_history(line);
  char *end_of_line=line+(uint) strlen(line);

  for (pos=out=line ; (inchar= (uchar) *pos) ; pos++)
  {
    if (!preserve_comments)
    {
      // Skip spaces at the beggining of a statement
      if (my_isspace(charset_info,inchar) && (out == line) &&
          (buffer->length==0))
        continue;
    }

#ifdef USE_MB
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
#endif
        if (!*ml_comment && inchar == '\\' &&
        !(drizzle.server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES))
    {
      // Found possbile one character command like \c

      if (!(inchar = (uchar) *++pos))
        break;        // readline adds one '\'
      if (*in_string || inchar == 'N')  // \N is short for NULL
      {          // Don't allow commands in string
        *out++='\\';
        *out++= (char) inchar;
        continue;
      }
      if ((com=find_command(NullS,(char) inchar)))
      {
        // Flush previously accepted characters
        if (out != line)
        {
          dynstr_append_mem(buffer, line, (out-line));
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
                          !is_prefix(pos + 1, delimiter + 1)) ; pos++)
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
        sprintf(buff,_("Unknown command '\\%c'."),inchar);
        if (put_info(buff,INFO_ERROR,0,0) > 0)
          return(1);
        *out++='\\';
        *out++=(char) inchar;
        continue;
      }
    }
    else if (!*ml_comment && !*in_string &&
             (end_of_line - pos) >= 10 &&
             !my_strnncoll(charset_info, (uchar*) pos, 10,
                           (const uchar*) "delimiter ", 10))
    {
      // Flush previously accepted characters
      if (out != line)
      {
        dynstr_append_mem(buffer, line, (out - line));
        out= line;
      }

      // Flush possible comments in the buffer
      if (buffer->length != 0)
      {
        if (com_go(buffer, 0) > 0) // < 0 is not fatal
          return(1);
        assert(buffer!=NULL);
        dynstr_set(buffer, NULL);
      }

      /*
        Delimiter wants the get rest of the given line as argument to
        allow one to change ';' to ';;' and back
      */
      dynstr_append(buffer,pos);
      if (com_delimiter(buffer, pos) > 0)
        return(1);

      dynstr_set(buffer, NULL);
      break;
    }
    else if (!*ml_comment && !*in_string && is_prefix(pos, delimiter))
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
        dynstr_append_mem(buffer, line, (out-line));
        out= line;
      }

      if (preserve_comments && ((*pos == '#') ||
                                ((*pos == '-') &&
                                 (pos[1] == '-') &&
                                 my_isspace(charset_info, pos[2]))))
      {
        // Add trailing single line comments to this statement
        dynstr_append(buffer, pos);
        pos+= strlen(pos);
      }

      pos--;

      if ((com= find_command(buffer->str, 0)))
      {

        if ((*com->func)(buffer, buffer->str) > 0)
          return(1);                       // Quit
      }
      else
      {
        if (com_go(buffer, 0) > 0)             // < 0 is not fatal
          return(1);
      }
      dynstr_set(buffer, NULL);
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
        dynstr_append_mem(buffer, line, (out - line));
        out= line;
      }

      // comment to end of line
      if (preserve_comments)
        dynstr_append(buffer,pos);

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
        dynstr_append_mem(buffer, line, (out-line));
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
        dynstr_append_mem(buffer, line, (out - line));
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
  if (out != line || (buffer->length > 0))
  {
    *out++='\n';
    uint length=(uint) (out-line);
    if ((!*ml_comment || preserve_comments)
        && dynstr_append_mem(buffer, line, length))
      return(1);
  }
  return(0);
}

/*****************************************************************
            Interface to Readline Completion
******************************************************************/


static char **mysql_completion (const char *text, int start, int end);
static char *new_command_generator(const char *text, int);

/*
  Tell the GNU Readline library how to complete.  We want to try to complete
  on command names if this is the first word in the line, or on filenames
  if not.
*/
static char *no_completion(const char * a __attribute__((unused)),
                           int b __attribute__((unused)))
{
  /* No filename completion */
  return 0;
}


/* glues pieces of history back together if in pieces   */
static void fix_history(DYNAMIC_STRING *final_command)
{
  int total_lines = 1;
  const char *ptr = final_command->str;
  char str_char = '\0';  /* Character if we are in a string or not */

  /* Converted buffer */
  DYNAMIC_STRING *fixed_buffer=
    (DYNAMIC_STRING *)my_malloc(sizeof(DYNAMIC_STRING), MYF(0));

  init_dynamic_string(fixed_buffer, "", 512, 512);

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
      dynstr_append_mem(fixed_buffer, ptr, 1);
      break;
    case '\n':
      /*
        not in string, change to space
        if in string, leave it alone
      */
      dynstr_append(fixed_buffer,(str_char == '\0') ? " " : "\n");
      total_lines++;
      break;
    case '\\':
      dynstr_append(fixed_buffer, "\\");
      /* need to see if the backslash is escaping anything */
      if (str_char)
      {
        ptr++;
        /* special characters that need escaping */
        if (*ptr == '\'' || *ptr == '"' || *ptr == '\\')
          dynstr_append_mem(fixed_buffer, ptr, 1);
        else
          ptr--;
      }
      break;
    default:
      dynstr_append_mem(fixed_buffer, ptr, 1);
    }
    ptr++;
  }
  if (total_lines > 1)
    add_history(fixed_buffer->str);
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

static void initialize_readline (const char *name)
{
  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name= (char *)name;

  /* Tell the completer that we want a crack first. */
  rl_attempted_completion_function= (rl_completion_func_t*)&mysql_completion;
  rl_completion_entry_function= (rl_compentry_func_t*)&no_completion;
}


/*
  Attempt to complete on the contents of TEXT.  START and END show the
  region of TEXT that contains the word to complete.  We can use the
  entire line in case we want to do some simple parsing.  Return the
  array of matches, or NULL if there aren't any.
*/
char **mysql_completion (const char *text,
                        int start __attribute__((unused)),
                        int end __attribute__((unused)))
{
  if (!status.batch && !quick)
    return rl_completion_matches(text, new_command_generator);
  else
    return (char**) 0;
}


static char *new_command_generator(const char *text,int state)
{
  static int textlen;
  char *ptr;
  static Bucket *b;
  static entry *e;
  static uint i;

  if (!state)
    textlen=(uint) strlen(text);

  if (textlen>0)
  {            /* lookup in the hash */
    if (!state)
    {
      uint len;

      b = find_all_matches(&ht,text,(uint) strlen(text),&len);
      if (!b)
        return NullS;
      e = b->pData;
    }

    if (e)
    {
      ptr= strdup(e->str);
      e = e->pNext;
      return ptr;
    }
  }
  else
  { /* traverse the entire hash, ugly but works */

    if (!state)
    {
      /* find the first used bucket */
      for (i=0 ; i < ht.nTableSize ; i++)
      {
        if (ht.arBuckets[i])
        {
          b = ht.arBuckets[i];
          e = b->pData;
          break;
        }
      }
    }
    ptr= NullS;
    while (e && !ptr)
    {          /* find valid entry in bucket */
      if ((uint) strlen(e->str) == b->nKeyLength)
        ptr = strdup(e->str);
      /* find the next used entry */
      e = e->pNext;
      if (!e)
      { /* find the next used bucket */
        b = b->pNext;
        if (!b)
        {
          for (i++ ; i<ht.nTableSize; i++)
          {
            if (ht.arBuckets[i])
            {
              b = ht.arBuckets[i];
              e = b->pData;
              break;
            }
          }
        }
        else
          e = b->pData;
      }
    }
    if (ptr)
      return ptr;
  }
  return NullS;
}


/* Build up the completion hash */

static void build_completion_hash(bool rehash, bool write_info)
{
  COMMANDS *cmd=commands;
  DRIZZLE_RES *databases=0,*tables=0;
  DRIZZLE_RES *fields;
  static char ***field_names= 0;
  DRIZZLE_ROW database_row,table_row;
  DRIZZLE_FIELD *sql_field;
  char buf[NAME_LEN*2+2];     // table name plus field name plus 2
  int i,j,num_fields;


  if (status.batch || quick || !current_db)
    return;      // We don't need completion in batches
  if (!rehash)
    return;

  /* Free old used memory */
  if (field_names)
    field_names=0;
  completion_hash_clean(&ht);
  free_root(&hash_mem_root,MYF(0));

  /* hash this file's known subset of SQL commands */
  while (cmd->name) {
    add_word(&ht,(char*) cmd->name);
    cmd++;
  }

  /* hash Drizzle functions (to be implemented) */

  /* hash all database names */
  if (drizzle_query(&drizzle,"show databases") == 0)
  {
    if (!(databases = drizzle_store_result(&drizzle)))
      put_info(drizzle_error(&drizzle),INFO_INFO,0,0);
    else
    {
      while ((database_row=drizzle_fetch_row(databases)))
      {
        char *str=strdup_root(&hash_mem_root, (char*) database_row[0]);
        if (str)
          add_word(&ht,(char*) str);
      }
      drizzle_free_result(databases);
    }
  }
  /* hash all table names */
  if (drizzle_query(&drizzle,"show tables")==0)
  {
    if (!(tables = drizzle_store_result(&drizzle)))
      put_info(drizzle_error(&drizzle),INFO_INFO,0,0);
    else
    {
      if (drizzle_num_rows(tables) > 0 && !opt_silent && write_info)
      {
        tee_fprintf(stdout, _("\
Reading table information for completion of table and column names\n    \
You can turn off this feature to get a quicker startup with -A\n\n"));
      }
      while ((table_row=drizzle_fetch_row(tables)))
      {
        char *str=strdup_root(&hash_mem_root, (char*) table_row[0]);
        if (str &&
            !completion_hash_exists(&ht,(char*) str, (uint) strlen(str)))
          add_word(&ht,str);
      }
    }
  }

  /* hash all field names, both with the table prefix and without it */
  if (!tables)          /* no tables */
  {
    return;
  }
  drizzle_data_seek(tables,0);
  if (!(field_names= (char ***) alloc_root(&hash_mem_root,sizeof(char **) *
                                           (uint) (drizzle_num_rows(tables)+1))))
  {
    drizzle_free_result(tables);
    return;
  }
  i=0;
  while ((table_row=drizzle_fetch_row(tables)))
  {
    if ((fields=drizzle_list_fields(&drizzle,(const char*) table_row[0],NullS)))
    {
      num_fields=drizzle_num_fields(fields);
      if (!(field_names[i] = (char **) alloc_root(&hash_mem_root,
                                                  sizeof(char *) *
                                                  (num_fields*2+1))))
      {
        drizzle_free_result(fields);
        break;
      }
      field_names[i][num_fields*2]= '\0';
      j=0;
      while ((sql_field=drizzle_fetch_field(fields)))
      {
        sprintf(buf,"%.64s.%.64s",table_row[0],sql_field->name);
        field_names[i][j] = strdup_root(&hash_mem_root,buf);
        add_word(&ht,field_names[i][j]);
        field_names[i][num_fields+j] = strdup_root(&hash_mem_root,
                                                   sql_field->name);
        if (!completion_hash_exists(&ht,field_names[i][num_fields+j],
                                    (uint) strlen(field_names[i][num_fields+j])))
          add_word(&ht,field_names[i][num_fields+j]);
        j++;
      }
      drizzle_free_result(fields);
    }
    else
      field_names[i]= 0;

    i++;
  }
  drizzle_free_result(tables);
  field_names[i]=0;        // End pointer
  return;
}

/* for gnu readline */

#ifndef HAVE_INDEX
extern "C" {
  extern char *index(const char *,int c),*rindex(const char *,int);

  char *index(const char *s,int c)
  {
    for (;;)
    {
      if (*s == (char) c) return (char*) s;
      if (!*s++) return NullS;
    }
  }

  char *rindex(const char *s,int c)
  {
    register char *t;

    t = NullS;
    do if (*s == (char) c) t = (char*) s; while (*s++);
    return (char*) t;
  }
}
#endif


static int reconnect(void)
{
  /* purecov: begin tested */
  if (opt_reconnect)
  {
    put_info(_("No connection. Trying to reconnect..."),INFO_INFO,0,0);
    (void) com_connect((DYNAMIC_STRING *) 0, 0);
    if (opt_rehash)
      com_rehash(NULL, NULL);
  }
  if (!connected)
    return put_info(_("Can't connect to the server\n"),INFO_ERROR,0,0);
  /* purecov: end */
  return 0;
}

static void get_current_db(void)
{
  DRIZZLE_RES *res;

  my_free(current_db, MYF(MY_ALLOW_ZERO_PTR));
  current_db= NULL;
  /* In case of error below current_db will be NULL */
  if (!drizzle_query(&drizzle, "SELECT DATABASE()") &&
      (res= drizzle_use_result(&drizzle)))
  {
    DRIZZLE_ROW row= drizzle_fetch_row(res);
    if (row[0])
      current_db= strdup(row[0]);
    drizzle_free_result(res);
  }
}

/***************************************************************************
 The different commands
***************************************************************************/

int drizzle_real_query_for_lazy(const char *buf, int length)
{
  for (uint retry=0;; retry++)
  {
    int error;
    if (!drizzle_real_query(&drizzle,buf,length))
      return 0;
    error= put_error(&drizzle);
    if (drizzle_errno(&drizzle) != CR_SERVER_GONE_ERROR || retry > 1 ||
        !opt_reconnect)
      return error;
    if (reconnect())
      return error;
  }
}

int drizzle_store_result_for_lazy(DRIZZLE_RES **result)
{
  if ((*result=drizzle_store_result(&drizzle)))
    return 0;

  if (drizzle_error(&drizzle)[0])
    return put_error(&drizzle);
  return 0;
}

static void print_help_item(DRIZZLE_ROW *cur, int num_name, int num_cat, char *last_char)
{
  char ccat= (*cur)[num_cat][0];
  if (*last_char != ccat)
  {
    put_info(ccat == 'Y' ? _("categories:") : _("topics:"), INFO_INFO,0,0);
    *last_char= ccat;
  }
  tee_fprintf(PAGER, "   %s\n", (*cur)[num_name]);
}


static int com_server_help(DYNAMIC_STRING *buffer,
                           char *line __attribute__((unused)),
                           char *help_arg)
{
  DRIZZLE_ROW cur;
  const char *server_cmd= buffer->str;
  char cmd_buf[100];
  DRIZZLE_RES *result;
  int error;

  if (help_arg[0] != '\'')
  {
    char *end_arg= strend(help_arg);
    if(--end_arg)
    {
      while (my_isspace(charset_info,*end_arg))
        end_arg--;
      *++end_arg= '\0';
    }
    (void) strxnmov(cmd_buf, sizeof(cmd_buf), "help '", help_arg, "'", NullS);
    server_cmd= cmd_buf;
  }

  if (!connected && reconnect())
    return 1;

  if ((error= drizzle_real_query_for_lazy(server_cmd,(int)strlen(server_cmd))) ||
      (error= drizzle_store_result_for_lazy(&result)))
    return error;

  if (result)
  {
    unsigned int num_fields= drizzle_num_fields(result);
    uint64_t num_rows= drizzle_num_rows(result);
    drizzle_fetch_fields(result);
    if (num_fields==3 && num_rows==1)
    {
      if (!(cur= drizzle_fetch_row(result)))
      {
        error= -1;
        goto err;
      }

      init_pager();
      tee_fprintf(PAGER,   _("Name: \'%s\'\n"), cur[0]);
      tee_fprintf(PAGER,   _("Description:\n%s"), cur[1]);
      if (cur[2] && *((char*)cur[2]))
        tee_fprintf(PAGER, _("Examples:\n%s"), cur[2]);
      tee_fprintf(PAGER,   "\n");
      end_pager();
    }
    else if (num_fields >= 2 && num_rows)
    {
      init_pager();
      char last_char= 0;

      int num_name= 0, num_cat= 0;

      if (num_fields == 2)
      {
        put_info(_("Many help items for your request exist."), INFO_INFO,0,0);
        put_info(_("To make a more specific request, please type 'help <item>',\nwhere <item> is one of the following"), INFO_INFO,0,0);
        num_name= 0;
        num_cat= 1;
      }
      else if ((cur= drizzle_fetch_row(result)))
      {
        tee_fprintf(PAGER, _("You asked for help about help category: '%s'\n"), cur[0]);
        put_info(_("For more information, type 'help <item>', where <item> is one of the following"), INFO_INFO,0,0);
        num_name= 1;
        num_cat= 2;
        print_help_item(&cur,1,2,&last_char);
      }

      while ((cur= drizzle_fetch_row(result)))
        print_help_item(&cur,num_name,num_cat,&last_char);
      tee_fprintf(PAGER, "\n");
      end_pager();
    }
    else
    {
      put_info(_("\nNothing found"), INFO_INFO,0,0);
      put_info(_("Please try to run 'help contents' for a list of all accessible topics\n"), INFO_INFO,0,0);
    }
  }

err:
  drizzle_free_result(result);
  return error;
}

static int
com_help(DYNAMIC_STRING *buffer __attribute__((unused)),
         char *line __attribute__((unused)))
{
  register int i, j;
  char * help_arg= strchr(line,' '), buff[32], *end;
  if (help_arg)
  {
    while (my_isspace(charset_info,*help_arg))
      help_arg++;
    if (*help_arg)
      return com_server_help(buffer,line,help_arg);
  }

  put_info(_("List of all Drizzle commands:"), INFO_INFO,0,0);
  if (!named_cmds)
    put_info(_("Note that all text commands must be first on line and end with ';'"),INFO_INFO,0,0);
  for (i = 0; commands[i].name; i++)
  {
    end= strmov(buff, commands[i].name);
    for (j= (int)strlen(commands[i].name); j < 10; j++)
      end= strmov(end, " ");
    if (commands[i].func)
      tee_fprintf(stdout, "%s(\\%c) %s\n", buff,
                  commands[i].cmd_char, _(commands[i].doc));
  }
  if (connected && drizzle_get_server_version(&drizzle) >= 40100)
    put_info(_("\nFor server side help, type 'help contents'\n"), INFO_INFO,0,0);
  return 0;
}


static int
com_clear(DYNAMIC_STRING *buffer,char *line __attribute__((unused)))
{
  if (status.add_to_history)
    fix_history(buffer);
  dynstr_set(buffer, NULL);
  return 0;
}

static int
com_charset(DYNAMIC_STRING *buffer __attribute__((unused)), char *line)
{
  char buff[256], *param;
  CHARSET_INFO * new_cs;
  strmake(buff, line, sizeof(buff) - 1);
  param= get_arg(buff, 0);
  if (!param || !*param)
  {
    return put_info(_("Usage: \\C char_setname | charset charset_name"),
                    INFO_ERROR, 0, 0);
  }
  new_cs= get_charset_by_csname(param, MY_CS_PRIMARY, MYF(MY_WME));
  if (new_cs)
  {
    charset_info= new_cs;
    drizzle_set_character_set(&drizzle, charset_info->csname);
    default_charset= (char *)charset_info->csname;
    default_charset_used= 1;
    put_info(_("Charset changed"), INFO_INFO,0,0);
  }
  else put_info(_("Charset is not found"), INFO_INFO,0,0);
  return 0;
}

/*
  Execute command
  Returns: 0  if ok
  -1 if not fatal error
  1  if fatal error
*/
static int
com_go(DYNAMIC_STRING *buffer,
       char *line __attribute__((unused)))
{
  char          buff[200]; /* about 110 chars used so far */
  char          time_buff[52+3+1]; /* time max + space&parens + NUL */
  DRIZZLE_RES     *result;
  ulong         timer, warnings= 0;
  uint          error= 0;
  int           err= 0;

  interrupted_query= 0;

  /* Remove garbage for nicer messages */
  remove_cntrl(buffer);

  if (buffer->length == 0)
  {
    // Ignore empty quries
    if (status.batch)
      return 0;
    return put_info(_("No query specified\n"),INFO_ERROR,0,0);

  }
  if (!connected && reconnect())
  {
    // Remove query on error
    dynstr_set(buffer, NULL);
    return opt_reconnect ? -1 : 1;          // Fatal error
  }
  if (verbose)
    (void) com_print(buffer, 0);

  if (skip_updates &&
      ((buffer->length < 4) || !strncmp(buffer->str, "SET ", 4)))
  {
    (void) put_info(_("Ignoring query to other database"),INFO_INFO,0,0);
    return 0;
  }

  timer=start_timer();
  executing_query= 1;
  error= drizzle_real_query_for_lazy(buffer->str,buffer->length);

  if (status.add_to_history)
  {
    dynstr_append(buffer, vertical ? "\\G" : delimiter);
    /* Append final command onto history */
    fix_history(buffer);
  }

  dynstr_set(buffer, NULL);

  if (error)
    goto end;

  do
  {
    char *pos;

    if (quick)
    {
      if (!(result=drizzle_use_result(&drizzle)) && drizzle_field_count(&drizzle))
      {
        error= put_error(&drizzle);
        goto end;
      }
    }
    else
    {
      error= drizzle_store_result_for_lazy(&result);
      if (error)
        goto end;
    }

    if (verbose >= 3 || !opt_silent)
      drizzle_end_timer(timer,time_buff);
    else
      time_buff[0]= '\0';

    /* Every branch must truncate  buff . */
    if (result)
    {
      if (!drizzle_num_rows(result) && ! quick && !column_types_flag)
      {
        strmov(buff, _("Empty set"));
        if (opt_xml)
        {
          /*
            We must print XML header and footer
            to produce a well-formed XML even if
            the result set is empty (Bug#27608).
          */
          init_pager();
          print_table_data_xml(result);
          end_pager();
        }
      }
      else
      {
        init_pager();
        if (opt_html)
          print_table_data_html(result);
        else if (opt_xml)
          print_table_data_xml(result);
        else if (vertical || (auto_vertical_output && (terminal_width < get_result_width(result))))
          print_table_data_vertically(result);
        else if (opt_silent && verbose <= 2 && !output_tables)
          print_tab_data(result);
        else
          print_table_data(result);
        sprintf(buff,
                ngettext("%ld row in set","%ld rows in set",
                         (long) drizzle_num_rows(result)),
                (long) drizzle_num_rows(result));
        end_pager();
        if (drizzle_errno(&drizzle))
          error= put_error(&drizzle);
      }
    }
    else if (drizzle_affected_rows(&drizzle) == ~(uint64_t) 0)
      strmov(buff,_("Query OK"));
    else
      sprintf(buff, ngettext("Query OK, %ld row affected",
                             "Query OK, %ld rows affected",
                             (long) drizzle_affected_rows(&drizzle)),
              (long) drizzle_affected_rows(&drizzle));

    pos=strend(buff);
    if ((warnings= drizzle_warning_count(&drizzle)))
    {
      *pos++= ',';
      *pos++= ' ';
      pos=int10_to_str(warnings, pos, 10);
      pos=strmov(pos, " warning");
      if (warnings != 1)
        *pos++= 's';
    }
    strmov(pos, time_buff);
    put_info(buff,INFO_RESULT,0,0);
    if (drizzle_info(&drizzle))
      put_info(drizzle_info(&drizzle),INFO_RESULT,0,0);
    put_info("",INFO_RESULT,0,0);      // Empty row

    if (result && !drizzle_eof(result))  /* Something wrong when using quick */
      error= put_error(&drizzle);
    else if (unbuffered)
      fflush(stdout);
    drizzle_free_result(result);
  } while (!(err= drizzle_next_result(&drizzle)));
  if (err >= 1)
    error= put_error(&drizzle);

end:

  /* Show warnings if any or error occured */
  if (show_warnings == 1 && (warnings >= 1 || error))
    print_warnings();

  if (!error && !status.batch &&
      (drizzle.server_status & SERVER_STATUS_DB_DROPPED))
    get_current_db();

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
  if (!(new_outfile= my_fopen(file_name, O_APPEND | O_WRONLY, MYF(MY_WME))))
  {
    tee_fprintf(stdout, "Error logging to file '%s'\n", file_name);
    return;
  }
  OUTFILE = new_outfile;
  strmake(outfile, file_name, FN_REFLEN-1);
  tee_fprintf(stdout, "Logging to file '%s'\n", file_name);
  opt_outfile= 1;
  return;
}


static void end_tee()
{
  my_fclose(OUTFILE, MYF(0));
  OUTFILE= 0;
  opt_outfile= 0;
  return;
}


static int
com_ego(DYNAMIC_STRING *buffer,char *line)
{
  int result;
  bool oldvertical=vertical;
  vertical=1;
  result=com_go(buffer,line);
  vertical=oldvertical;
  return result;
}


static const char *fieldtype2str(enum enum_field_types type)
{
  switch (type) {
    case DRIZZLE_TYPE_BLOB:        return "BLOB";
    case DRIZZLE_TYPE_NEWDATE:        return "DATE";
    case DRIZZLE_TYPE_DATETIME:    return "DATETIME";
    case DRIZZLE_TYPE_NEWDECIMAL:  return "DECIMAL";
    case DRIZZLE_TYPE_DOUBLE:      return "DOUBLE";
    case DRIZZLE_TYPE_ENUM:        return "ENUM";
    case DRIZZLE_TYPE_LONG:        return "LONG";
    case DRIZZLE_TYPE_LONGLONG:    return "LONGLONG";
    case DRIZZLE_TYPE_NULL:        return "NULL";
    case DRIZZLE_TYPE_SET:         return "SET";
    case DRIZZLE_TYPE_SHORT:       return "SHORT";
    case DRIZZLE_TYPE_TIME:        return "TIME";
    case DRIZZLE_TYPE_TIMESTAMP:   return "TIMESTAMP";
    case DRIZZLE_TYPE_TINY:        return "TINY";
    default:                     return "?-unknown-?";
  }
}

static char *fieldflags2str(uint f) {
  static char buf[1024];
  char *s=buf;
  *s=0;
#define ff2s_check_flag(X)                                              \
  if (f & X ## _FLAG) { s=strmov(s, # X " "); f &= ~ X ## _FLAG; }
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
    sprintf(s, " unknows=0x%04x", f);
  return buf;
}

static void
print_field_types(DRIZZLE_RES *result)
{
  DRIZZLE_FIELD   *field;
  uint i=0;

  while ((field = drizzle_fetch_field(result)))
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
                field->name, field->catalog, field->db, field->table,
                field->org_table, fieldtype2str(field->type),
                get_charset_name(field->charsetnr), field->charsetnr,
                field->length, field->max_length, field->decimals,
                fieldflags2str(field->flags));
  }
  tee_puts("", PAGER);
}


static void
print_table_data(DRIZZLE_RES *result)
{
  DRIZZLE_ROW     cur;
  DRIZZLE_FIELD   *field;
  bool          *num_flag;
  DYNAMIC_STRING *separator=
    (DYNAMIC_STRING *)my_malloc(sizeof(DYNAMIC_STRING), MYF(0));
  init_dynamic_string(separator, "", 256, 256);

  num_flag=(bool*) my_malloc(sizeof(bool)*drizzle_num_fields(result),
                             MYF(MY_WME));
  if (column_types_flag)
  {
    print_field_types(result);
    if (!drizzle_num_rows(result))
      return;
    drizzle_field_seek(result,0);
  }
  dynstr_append(separator, "+");
  while ((field = drizzle_fetch_field(result)))
  {
    uint length= column_names ? field->name_length : 0;
    if (quick)
      length=max(length,field->length);
    else
      length=max(length,field->max_length);
    if (length < 4 && !IS_NOT_NULL(field->flags))
      // Room for "NULL"
      length=4;
    field->max_length=length;
    uint x;
    for (x=0; x< (length+2); x++)
      dynstr_append(separator, "-");
    dynstr_append(separator, "+");
  }

  tee_puts((char*) separator->str, PAGER);
  if (column_names)
  {
    drizzle_field_seek(result,0);
    (void) tee_fputs("|", PAGER);
    for (uint off=0; (field = drizzle_fetch_field(result)) ; off++)
    {
      uint name_length= (uint) strlen(field->name);
      uint numcells= charset_info->cset->numcells(charset_info,
                                                  field->name,
                                                  field->name + name_length);
      uint display_length= field->max_length + name_length - numcells;
      tee_fprintf(PAGER, " %-*s |",(int) min(display_length,
                                             MAX_COLUMN_LENGTH),
                  field->name);
      num_flag[off]= IS_NUM(field->type);
    }
    (void) tee_fputs("\n", PAGER);
    tee_puts((char*) separator->str, PAGER);
  }

  while ((cur= drizzle_fetch_row(result)))
  {
    if (interrupted_query)
      break;
    uint32_t *lengths= drizzle_fetch_lengths(result);
    (void) tee_fputs("| ", PAGER);
    drizzle_field_seek(result, 0);
    for (uint off= 0; off < drizzle_num_fields(result); off++)
    {
      const char *buffer;
      uint data_length;
      uint field_max_length;
      uint visible_length;
      uint extra_padding;

      if (cur[off] == NULL)
      {
        buffer= "NULL";
        data_length= 4;
      }
      else
      {
        buffer= cur[off];
        data_length= (uint) lengths[off];
      }

      field= drizzle_fetch_field(result);
      field_max_length= field->max_length;

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
        tee_print_sized_data(buffer, data_length, MAX_COLUMN_LENGTH+extra_padding, FALSE);
      else
      {
        if (num_flag[off] != 0) /* if it is numeric, we right-justify it */
          tee_print_sized_data(buffer, data_length, field_max_length+extra_padding, TRUE);
        else
          tee_print_sized_data(buffer, data_length,
                               field_max_length+extra_padding, FALSE);
      }
      tee_fputs(" | ", PAGER);
    }
    (void) tee_fputs("\n", PAGER);
  }
  tee_puts(separator->str, PAGER);
  my_free(num_flag, MYF(MY_ALLOW_ZERO_PTR));
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
static int get_field_disp_length(DRIZZLE_FIELD *field)
{
  uint length= column_names ? field->name_length : 0;

  if (quick)
    length= max(length, field->length);
  else
    length= max(length, field->max_length);

  if (length < 4 && !IS_NOT_NULL(field->flags))
    length= 4;        /* Room for "NULL" */

  return length;
}

/**
   For a new result, return the max number of characters that any
   upcoming row may return.

   @param  result  Pointer to the result to judge

   @returns  The max number of characters in any row of this result
*/
static int get_result_width(DRIZZLE_RES *result)
{
  unsigned int len= 0;
  DRIZZLE_FIELD *field;
  DRIZZLE_FIELD_OFFSET offset;

  offset= drizzle_field_tell(result);
  assert(offset == 0);

  while ((field= drizzle_fetch_field(result)) != NULL)
    len+= get_field_disp_length(field) + 3; /* plus bar, space, & final space */

  (void) drizzle_field_seek(result, offset);

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
print_table_data_html(DRIZZLE_RES *result)
{
  DRIZZLE_ROW  cur;
  DRIZZLE_FIELD  *field;

  drizzle_field_seek(result,0);
  (void) tee_fputs("<TABLE BORDER=1><TR>", PAGER);
  if (column_names)
  {
    while((field = drizzle_fetch_field(result)))
    {
      tee_fprintf(PAGER, "<TH>%s</TH>", (field->name ?
                                         (field->name[0] ? field->name :
                                          " &nbsp; ") : "NULL"));
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  while ((cur = drizzle_fetch_row(result)))
  {
    if (interrupted_query)
      break;
    uint32_t *lengths=drizzle_fetch_lengths(result);
    (void) tee_fputs("<TR>", PAGER);
    for (uint32_t i= 0; i < drizzle_num_fields(result); i++)
    {
      (void) tee_fputs("<TD>", PAGER);
      safe_put_field(cur[i],lengths[i]);
      (void) tee_fputs("</TD>", PAGER);
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  (void) tee_fputs("</TABLE>", PAGER);
}


static void
print_table_data_xml(DRIZZLE_RES *result)
{
  DRIZZLE_ROW   cur;
  const DRIZZLE_FIELD *fields;

  drizzle_field_seek(result,0);

  tee_fputs("<?xml version=\"1.0\"?>\n\n<resultset statement=\"", PAGER);
  xmlencode_print(glob_buffer->str, glob_buffer->length);
  tee_fputs("\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">",
            PAGER);

  fields = drizzle_fetch_fields(result);
  while ((cur = drizzle_fetch_row(result)))
  {
    if (interrupted_query)
      break;
    uint32_t *lengths=drizzle_fetch_lengths(result);
    (void) tee_fputs("\n  <row>\n", PAGER);
    for (uint i=0; i < drizzle_num_fields(result); i++)
    {
      tee_fprintf(PAGER, "\t<field name=\"");
      xmlencode_print(fields[i].name, (uint) strlen(fields[i].name));
      if (cur[i])
      {
        tee_fprintf(PAGER, "\">");
        xmlencode_print(cur[i], lengths[i]);
        tee_fprintf(PAGER, "</field>\n");
      }
      else
        tee_fprintf(PAGER, "\" xsi:nil=\"true\" />\n");
    }
    (void) tee_fputs("  </row>\n", PAGER);
  }
  (void) tee_fputs("</resultset>\n", PAGER);
}


static void
print_table_data_vertically(DRIZZLE_RES *result)
{
  DRIZZLE_ROW  cur;
  uint    max_length=0;
  DRIZZLE_FIELD  *field;

  while ((field = drizzle_fetch_field(result)))
  {
    uint length= field->name_length;
    if (length > max_length)
      max_length= length;
    field->max_length=length;
  }

  drizzle_field_seek(result,0);
  for (uint row_count=1; (cur= drizzle_fetch_row(result)); row_count++)
  {
    if (interrupted_query)
      break;
    drizzle_field_seek(result,0);
    tee_fprintf(PAGER,
                "*************************** %d. row ***************************\n", row_count);
    for (uint off=0; off < drizzle_num_fields(result); off++)
    {
      field= drizzle_fetch_field(result);
      tee_fprintf(PAGER, "%*s: ",(int) max_length,field->name);
      tee_fprintf(PAGER, "%s\n",cur[off] ? (char*) cur[off] : "NULL");
    }
  }
}


/* print_warnings should be called right after executing a statement */

static void print_warnings()
{
  const char   *query;
  DRIZZLE_RES    *result;
  DRIZZLE_ROW    cur;
  uint64_t num_rows;
 
  /* Save current error before calling "show warnings" */
  uint error= drizzle_errno(&drizzle);

  /* Get the warnings */
  query= "show warnings";
  drizzle_real_query_for_lazy(query, strlen(query));
  drizzle_store_result_for_lazy(&result);

  /* Bail out when no warnings */
  if (!(num_rows= drizzle_num_rows(result)))
    goto end;

  cur= drizzle_fetch_row(result);

  /*
    Don't print a duplicate of the current error.  It is possible for SHOW
    WARNINGS to return multiple errors with the same code, but different
    messages.  To be safe, skip printing the duplicate only if it is the only
    warning.
  */
  if (!cur || (num_rows == 1 && error == (uint) strtoul(cur[1], NULL, 10)))
    goto end;

  /* Print the warnings */
  init_pager();
  do
  {
    tee_fprintf(PAGER, "%s (Code %s): %s\n", cur[0], cur[1], cur[2]);
  } while ((cur= drizzle_fetch_row(result)));
  end_pager();

end:
  drizzle_free_result(result);
}


static const char *array_value(const char **array, char key)
{
  for (; *array; array+= 2)
    if (**array == key)
      return array[1];
  return 0;
}


static void
xmlencode_print(const char *src, uint length)
{
  if (!src)
    tee_fputs("NULL", PAGER);
  else
  {
    for (const char *p = src; *p && length; length--)
    {
      const char *t;
      if ((t = array_value(xmlmeta, *p++)))
        tee_fputs(t, PAGER);
      else
        tee_putc(*p, PAGER);
    }
  }
}


static void
safe_put_field(const char *pos,ulong length)
{
  if (!pos)
    tee_fputs("NULL", PAGER);
  else
  {
    if (opt_raw_data)
      tee_fputs(pos, PAGER);
    else for (const char *end=pos+length ; pos != end ; pos++)
         {
#ifdef USE_MB
           int l;
           if (use_mb(charset_info) &&
               (l = my_ismbchar(charset_info, pos, end)))
           {
             while (l--)
               tee_putc(*pos++, PAGER);
             pos--;
             continue;
           }
#endif
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
print_tab_data(DRIZZLE_RES *result)
{
  DRIZZLE_ROW  cur;
  DRIZZLE_FIELD  *field;
  uint32_t    *lengths;

  if (opt_silent < 2 && column_names)
  {
    int first=0;
    while ((field = drizzle_fetch_field(result)))
    {
      if (first++)
        (void) tee_fputs("\t", PAGER);
      (void) tee_fputs(field->name, PAGER);
    }
    (void) tee_fputs("\n", PAGER);
  }
  while ((cur = drizzle_fetch_row(result)))
  {
    lengths= drizzle_fetch_lengths(result);
    safe_put_field(cur[0],lengths[0]);
    for (uint off=1 ; off < drizzle_num_fields(result); off++)
    {
      (void) tee_fputs("\t", PAGER);
      safe_put_field(cur[off], lengths[off]);
    }
    (void) tee_fputs("\n", PAGER);
  }
}

static int
com_tee(DYNAMIC_STRING *buffer __attribute__((unused)), char *line )
{
  char file_name[FN_REFLEN], *end, *param;

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
  end= strmake(file_name, param, sizeof(file_name) - 1);
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
com_notee(DYNAMIC_STRING *buffer __attribute__((unused)),
          char *line __attribute__((unused)))
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
com_pager(DYNAMIC_STRING *buffer __attribute__((unused)),
          char *line __attribute__((unused)))
{
  char pager_name[FN_REFLEN], *end, *param;

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
      strmov(pager, "stdout");
      PAGER= stdout;
      return 0;
    }
    strmov(pager, default_pager);
  }
  else
  {
    end= strmake(pager_name, param, sizeof(pager_name)-1);
    while (end > pager_name && (my_isspace(charset_info,end[-1]) ||
                                my_iscntrl(charset_info,end[-1])))
      end--;
    end[0]=0;
    strmov(pager, pager_name);
    strmov(default_pager, pager_name);
  }
  opt_nopager=0;
  tee_fprintf(stdout, "PAGER set to '%s'\n", pager);
  return 0;
}


static int
com_nopager(DYNAMIC_STRING *buffer __attribute__((unused)),
            char *line __attribute__((unused)))
{
  strmov(pager, "stdout");
  opt_nopager=1;
  PAGER= stdout;
  tee_fprintf(stdout, "PAGER set to stdout\n");
  return 0;
}

/* If arg is given, exit without errors. This happens on command 'quit' */

static int
com_quit(DYNAMIC_STRING *buffer __attribute__((unused)),
         char *line __attribute__((unused)))
{
  /* let the screen auto close on a normal shutdown */
  status.exit_status=0;
  return 1;
}

static int
com_rehash(DYNAMIC_STRING *buffer __attribute__((unused)),
           char *line __attribute__((unused)))
{
  build_completion_hash(1, 0);
  return 0;
}



static int
com_print(DYNAMIC_STRING *buffer,char *line __attribute__((unused)))
{
  tee_puts("--------------", stdout);
  (void) tee_fputs(buffer->str, stdout);
  if ( (buffer->length == 0)
       || (buffer->str)[(buffer->length)-1] != '\n')
    tee_putc('\n', stdout);
  tee_puts("--------------\n", stdout);
  /* If empty buffer */
  return 0;
}

/* ARGSUSED */
static int
com_connect(DYNAMIC_STRING *buffer, char *line)
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
    tmp= strmake(buff, line, sizeof(buff)-2);
#ifdef EXTRA_DEBUG
    tmp[1]= 0;
#endif
    tmp= get_arg(buff, 0);
    if (tmp && *tmp)
    {
      my_free(current_db, MYF(MY_ALLOW_ZERO_PTR));
      current_db= strdup(tmp);
      tmp= get_arg(buff, 1);
      if (tmp)
      {
        my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
        current_host=strdup(tmp);
      }
    }
    else
    {
      /* Quick re-connect */
      opt_rehash= 0;                            /* purecov: tested */
    }
    // command used
    assert(buffer!=NULL);
    dynstr_set(buffer, NULL);
  }
  else
    opt_rehash= 0;
  error=sql_connect(current_host,current_db,current_user,opt_password,0);
  opt_rehash= save_rehash;

  if (connected)
  {
    sprintf(buff,"Connection id:    %u",drizzle_thread_id(&drizzle));
    put_info(buff,INFO_INFO,0,0);
    sprintf(buff,"Current database: %.128s\n",
            current_db ? current_db : "*** NONE ***");
    put_info(buff,INFO_INFO,0,0);
  }
  return error;
}


static int com_source(DYNAMIC_STRING *buffer __attribute__((unused)), char *line)
{
  char source_name[FN_REFLEN], *end, *param;
  LINE_BUFFER *line_buff;
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
  end=strmake(source_name,param,sizeof(source_name)-1);
  while (end > source_name && (my_isspace(charset_info,end[-1]) ||
                               my_iscntrl(charset_info,end[-1])))
    end--;
  end[0]=0;
  unpack_filename(source_name,source_name);
  /* open file name */
  if (!(sql_file = my_fopen(source_name, O_RDONLY | O_BINARY,MYF(0))))
  {
    char buff[FN_REFLEN+60];
    sprintf(buff,"Failed to open file '%s', error: %d", source_name,errno);
    return put_info(buff, INFO_ERROR, 0 ,0);
  }

  if (!(line_buff=batch_readline_init(opt_max_allowed_packet+512,sql_file)))
  {
    my_fclose(sql_file,MYF(0));
    return put_info("Can't initialize batch_readline", INFO_ERROR, 0 ,0);
  }

  /* Save old status */
  old_status=status;
  memset((char*) &status, 0, sizeof(status));

  // Run in batch mode
  status.batch=old_status.batch;
  status.line_buff=line_buff;
  status.file_name=source_name;
  // Empty command buffer
  assert(glob_buffer!=NULL);
  dynstr_set(glob_buffer, NULL);
  error= read_and_execute(false);
  // Continue as before
  status=old_status;
  my_fclose(sql_file,MYF(0));
  batch_readline_end(line_buff);
  return error;
}


/* ARGSUSED */
static int
com_delimiter(DYNAMIC_STRING *buffer __attribute__((unused)), char *line)
{
  char buff[256], *tmp;

  strmake(buff, line, sizeof(buff) - 1);
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
  strmake(delimiter, tmp, sizeof(delimiter) - 1);
  delimiter_length= (int)strlen(delimiter);
  delimiter_str= delimiter;
  return 0;
}

/* ARGSUSED */
static int
com_use(DYNAMIC_STRING *buffer __attribute__((unused)), char *line)
{
  char *tmp, buff[FN_REFLEN + 1];
  int select_db;

  memset(buff, 0, sizeof(buff));
  strmake(buff, line, sizeof(buff) - 1);
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

  if (!current_db || cmp_database(charset_info, current_db,tmp))
  {
    if (one_database)
    {
      skip_updates= 1;
      select_db= 0;    // don't do drizzle_select_db()
    }
    else
      select_db= 2;    // do drizzle_select_db() and build_completion_hash()
  }
  else
  {
    /*
      USE to the current db specified.
      We do need to send drizzle_select_db() to make server
      update database level privileges, which might
      change since last USE (see bug#10979).
      For performance purposes, we'll skip rebuilding of completion hash.
    */
    skip_updates= 0;
    select_db= 1;      // do only drizzle_select_db(), without completion
  }

  if (select_db)
  {
    /*
      reconnect once if connection is down or if connection was found to
      be down during query
    */
    if (!connected && reconnect())
      return opt_reconnect ? -1 : 1;                        // Fatal error
    if (drizzle_select_db(&drizzle,tmp))
    {
      if (drizzle_errno(&drizzle) != CR_SERVER_GONE_ERROR)
        return put_error(&drizzle);

      if (reconnect())
        return opt_reconnect ? -1 : 1;                      // Fatal error
      if (drizzle_select_db(&drizzle,tmp))
        return put_error(&drizzle);
    }
    my_free(current_db,MYF(MY_ALLOW_ZERO_PTR));
    current_db= strdup(tmp);
    if (select_db > 1)
      build_completion_hash(opt_rehash, 1);
  }

  put_info("Database changed",INFO_INFO, 0, 0);
  return 0;
}

static int
com_warnings(DYNAMIC_STRING *buffer __attribute__((unused)),
             char *line __attribute__((unused)))
{
  show_warnings = 1;
  put_info("Show warnings enabled.",INFO_INFO, 0, 0);
  return 0;
}

static int
com_nowarnings(DYNAMIC_STRING *buffer __attribute__((unused)),
               char *line __attribute__((unused)))
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
    return NullS;
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
      strmov(ptr, ptr+1);
    }
    else if ((!quoted && *ptr == ' ') || (quoted && *ptr == qtype))
    {
      *ptr= 0;
      break;
    }
  }
  valid_arg= ptr != start;
  return valid_arg ? start : NullS;
}


static int
sql_connect(char *host,char *database,char *user,char *password,
                 uint silent)
{
  if (connected)
  {
    connected= 0;
    drizzle_close(&drizzle);
  }
  drizzle_create(&drizzle);
  if (opt_connect_timeout)
  {
    uint timeout=opt_connect_timeout;
    drizzle_options(&drizzle,DRIZZLE_OPT_CONNECT_TIMEOUT,
                  (char*) &timeout);
  }
  if (opt_compress)
    drizzle_options(&drizzle,DRIZZLE_OPT_COMPRESS,NullS);
  if (opt_secure_auth)
    drizzle_options(&drizzle, DRIZZLE_SECURE_AUTH, (char *) &opt_secure_auth);
  if (using_opt_local_infile)
    drizzle_options(&drizzle,DRIZZLE_OPT_LOCAL_INFILE, (char*) &opt_local_infile);
  if (opt_protocol)
    drizzle_options(&drizzle,DRIZZLE_OPT_PROTOCOL,(char*)&opt_protocol);
  if (safe_updates)
  {
    char init_command[100];
    sprintf(init_command,
            "SET SQL_SAFE_UPDATES=1,SQL_SELECT_LIMIT=%"PRIu32
            ",SQL_MAX_JOIN_SIZE=%"PRIu32,
            select_limit, max_join_size);
    drizzle_options(&drizzle, DRIZZLE_INIT_COMMAND, init_command);
  }
  if (default_charset_used)
    drizzle_options(&drizzle, DRIZZLE_SET_CHARSET_NAME, default_charset);
  if (!drizzle_connect(&drizzle, host, user, password,
                          database, opt_drizzle_port, opt_drizzle_unix_port,
                          connect_flag | CLIENT_MULTI_STATEMENTS))
  {
    if (!silent ||
        (drizzle_errno(&drizzle) != CR_CONN_HOST_ERROR &&
         drizzle_errno(&drizzle) != CR_CONNECTION_ERROR))
    {
      (void) put_error(&drizzle);
      (void) fflush(stdout);
      return ignore_errors ? -1 : 1;    // Abort
    }
    return -1;          // Retryable
  }
  connected=1;
  drizzle.reconnect= debug_info_flag; // We want to know if this happens
  build_completion_hash(opt_rehash, 1);
  return 0;
}


static int
com_status(DYNAMIC_STRING *buffer __attribute__((unused)),
           char *line __attribute__((unused)))
{
  const char *status_str;
  char buff[40];
  uint64_t id;
  DRIZZLE_RES *result;

  tee_puts("--------------", stdout);
  usage(1);          /* Print version */
  if (connected)
  {
    tee_fprintf(stdout, "\nConnection id:\t\t%lu\n",drizzle_thread_id(&drizzle));
    /*
      Don't remove "limit 1",
      it is protection againts SQL_SELECT_LIMIT=0
    */
    if (!drizzle_query(&drizzle,"select DATABASE(), USER() limit 1") &&
        (result=drizzle_use_result(&drizzle)))
    {
      DRIZZLE_ROW cur=drizzle_fetch_row(result);
      if (cur)
      {
        tee_fprintf(stdout, "Current database:\t%s\n", cur[0] ? cur[0] : "");
        tee_fprintf(stdout, "Current user:\t\t%s\n", cur[1]);
      }
      drizzle_free_result(result);
    }
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
  tee_fprintf(stdout, "Server version:\t\t%s\n", server_version_string(&drizzle));
  tee_fprintf(stdout, "Protocol version:\t%d\n", drizzle_get_proto_info(&drizzle));
  tee_fprintf(stdout, "Connection:\t\t%s\n", drizzle_get_host_info(&drizzle));
  if ((id= drizzle_insert_id(&drizzle)))
    tee_fprintf(stdout, "Insert id:\t\t%s\n", llstr(id, buff));

  /* "limit 1" is protection against SQL_SELECT_LIMIT=0 */
  if (!drizzle_query(&drizzle,"select @@character_set_client, @@character_set_connection, @@character_set_server, @@character_set_database limit 1") &&
      (result=drizzle_use_result(&drizzle)))
  {
    DRIZZLE_ROW cur=drizzle_fetch_row(result);
    if (cur)
    {
      tee_fprintf(stdout, "Server characterset:\t%s\n", cur[2] ? cur[2] : "");
      tee_fprintf(stdout, "Db     characterset:\t%s\n", cur[3] ? cur[3] : "");
      tee_fprintf(stdout, "Client characterset:\t%s\n", cur[0] ? cur[0] : "");
      tee_fprintf(stdout, "Conn.  characterset:\t%s\n", cur[1] ? cur[1] : "");
    }
    drizzle_free_result(result);
  }
  else
  {
    /* Probably pre-4.1 server */
    tee_fprintf(stdout, "Client characterset:\t%s\n", charset_info->csname);
    tee_fprintf(stdout, "Server characterset:\t%s\n", drizzle.charset->csname);
  }

  if (strstr(drizzle_get_host_info(&drizzle),"TCP/IP") || ! drizzle.unix_socket)
    tee_fprintf(stdout, "TCP port:\t\t%d\n", drizzle.port);
  else
    tee_fprintf(stdout, "UNIX socket:\t\t%s\n", drizzle.unix_socket);
  if (drizzle.net.compress)
    tee_fprintf(stdout, "Protocol:\t\tCompressed\n");

  if ((status_str= drizzle_stat(&drizzle)) && !drizzle_error(&drizzle)[0])
  {
    ulong sec;
    const char *pos= strchr(status_str,' ');
    /* print label */
    tee_fprintf(stdout, "%.*s\t\t\t", (int) (pos-status_str), status_str);
    if ((status_str= str2int(pos,10,0,LONG_MAX,(long*) &sec)))
    {
      nice_time((double) sec,buff,0);
      tee_puts(buff, stdout);      /* print nice time */
      while (*status_str == ' ')
        status_str++;  /* to next info */
      tee_putc('\n', stdout);
      tee_puts(status_str, stdout);
    }
  }
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
server_version_string(DRIZZLE *con)
{
  static char buf[MAX_SERVER_VERSION_LENGTH] = "";

  /* Only one thread calls this, so no synchronization is needed */
  if (buf[0] == '\0')
  {
    char *bufp = buf;
    DRIZZLE_RES *result;

    bufp= strnmov(buf, drizzle_get_server_info(con), sizeof buf);

    /* "limit 1" is protection against SQL_SELECT_LIMIT=0 */
    if (!drizzle_query(con, "select @@version_comment limit 1") &&
        (result = drizzle_use_result(con)))
    {
      DRIZZLE_ROW cur = drizzle_fetch_row(result);
      if (cur && cur[0])
      {
        bufp = strxnmov(bufp, sizeof buf - (bufp - buf), " ", cur[0], NullS);
      }
      drizzle_free_result(result);
    }

    /* str*nmov doesn't guarantee NUL-termination */
    if (bufp == buf + sizeof buf)
      buf[sizeof buf - 1] = '\0';
  }

  return buf;
}

static int
put_info(const char *str,INFO_TYPE info_type, uint error, const char *sqlstate)
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
put_error(DRIZZLE *con)
{
  return put_info(drizzle_error(con), INFO_ERROR, drizzle_errno(con),
                  drizzle_sqlstate(con));
}


static void remove_cntrl(DYNAMIC_STRING *buffer)
{
  char *start=  buffer->str;
  char *end= start + (buffer->length);
  while (start < end && !my_isgraph(charset_info,end[-1]))
    end--;
  uint chars_to_truncate = end-start;
  if (buffer->length > chars_to_truncate)
    dynstr_trunc(buffer, chars_to_truncate);
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

static ulong start_timer(void)
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
  ulong tmp;
  if (sec >= 3600.0*24)
  {
    tmp=(ulong) floor(sec/(3600.0*24));
    sec-=3600.0*24*tmp;
    buff=int10_to_str((long) tmp, buff, 10);
    buff=strmov(buff,tmp > 1 ? " days " : " day ");
  }
  if (sec >= 3600.0)
  {
    tmp=(ulong) floor(sec/3600.0);
    sec-=3600.0*tmp;
    buff=int10_to_str((long) tmp, buff, 10);
    buff=strmov(buff,tmp > 1 ? " hours " : " hour ");
  }
  if (sec >= 60.0)
  {
    tmp=(ulong) floor(sec/60.0);
    sec-=60.0*tmp;
    buff=int10_to_str((long) tmp, buff, 10);
    buff=strmov(buff," min ");
  }
  if (part_second)
    sprintf(buff,"%.2f sec",sec);
  else
    sprintf(buff,"%d sec",(int) sec);
}


static void end_timer(ulong start_time,char *buff)
{
  nice_time((double) (start_timer() - start_time) /
            CLOCKS_PER_SEC,buff,1);
}


static void drizzle_end_timer(ulong start_time,char *buff)
{
  buff[0]=' ';
  buff[1]='(';
  end_timer(start_time,buff+2);
  strmov(strend(buff),")");
}

static const char * construct_prompt()
{
  // Erase the old prompt
  assert(processed_prompt!=NULL);
  dynstr_set(processed_prompt, NULL);

  // Get the date struct
  time_t  lclock = time(NULL);
  struct tm *t = localtime(&lclock);

  /* parse thru the settings for the prompt */
  for (char *c= current_prompt; *c; (void)*c++)
  {
    if (*c != PROMPT_CHAR)
    {
      dynstr_append_mem(processed_prompt, c, 1);
    }
    else
    {
      int getHour;
      int getYear;
      char* dateTime= NULL;
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
          dynstr_append(processed_prompt, drizzle_get_server_info(&drizzle));
        else
          dynstr_append(processed_prompt, "not_connected");
        break;
      case 'd':
        dynstr_append(processed_prompt, current_db ? current_db : "(none)");
        break;
      case 'h':
      {
        const char *prompt;
        prompt= connected ? drizzle_get_host_info(&drizzle) : "not_connected";
        if (strstr(prompt, "Localhost"))
          dynstr_append(processed_prompt, "localhost");
        else
        {
          const char *end=strcend(prompt,' ');
          dynstr_append_mem(processed_prompt, prompt, (end-prompt));
        }
        break;
      }
      case 'p':
      {
        if (!connected)
        {
          dynstr_append(processed_prompt, "not_connected");
          break;
        }

        const char *host_info = drizzle_get_host_info(&drizzle);
        if (strstr(host_info, "memory"))
        {
          dynstr_append(processed_prompt, drizzle.host);
        }
        else if (strstr(host_info,"TCP/IP") ||
                 !drizzle.unix_socket)
          add_int_to_prompt(drizzle.port);
        else
        {
          char *pos=strrchr(drizzle.unix_socket,'/');
          dynstr_append(processed_prompt, pos ? pos+1 : drizzle.unix_socket);
        }
      }
      break;
      case 'U':
        if (!full_username)
          init_username();
        dynstr_append(processed_prompt, full_username ? full_username :
                        (current_user ?  current_user : "(unknown)"));
        break;
      case 'u':
        if (!full_username)
          init_username();
        dynstr_append(processed_prompt, part_username ? part_username :
                        (current_user ?  current_user : "(unknown)"));
        break;
      case PROMPT_CHAR:
        {
          char c= PROMPT_CHAR;
          dynstr_append_mem(processed_prompt, &c, 1);
        }
        break;
      case 'n':
        {
          char c= '\n';
          dynstr_append_mem(processed_prompt, &c, 1);
        }
        break;
      case ' ':
      case '_':
        {
          char c= ' ';
          dynstr_append_mem(processed_prompt, &c, 1);
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
        dateTime = ctime(&lclock);
        dynstr_append(processed_prompt, strtok(dateTime,"\n"));
        break;
      case 's':
        if (t->tm_sec < 10)
          add_int_to_prompt(0);
        add_int_to_prompt(t->tm_sec);
        break;
      case 'w':
        dynstr_append(processed_prompt, (day_names[t->tm_wday]));
        break;
      case 'P':
        dynstr_append(processed_prompt, t->tm_hour < 12 ? "am" : "pm");
        break;
      case 'o':
        add_int_to_prompt(t->tm_mon+1);
        break;
      case 'O':
        dynstr_append(processed_prompt, month_names[t->tm_mon]);
        break;
      case '\'':
        dynstr_append(processed_prompt, "'");
        break;
      case '"':
        dynstr_append(processed_prompt, "\"");
        break;
      case 'S':
        dynstr_append(processed_prompt, ";");
        break;
      case 't':
        dynstr_append(processed_prompt, "\t");
        break;
      case 'l':
        dynstr_append(processed_prompt, delimiter_str);
        break;
      default:
        dynstr_append_mem(processed_prompt, c, 1);
      }
    }
  }
  return processed_prompt->str;
}


static void add_int_to_prompt(int toadd)
{
  char buffer[16];
  int10_to_str(toadd, buffer, 10);
  dynstr_append(processed_prompt, buffer);
}

static void init_username()
{
  my_free(full_username,MYF(MY_ALLOW_ZERO_PTR));
  my_free(part_username,MYF(MY_ALLOW_ZERO_PTR));

  DRIZZLE_RES *result;
  if (!drizzle_query(&drizzle,"select USER()") &&
      (result=drizzle_use_result(&drizzle)))
  {
    DRIZZLE_ROW cur=drizzle_fetch_row(result);
    full_username= strdup(cur[0]);
    part_username= strdup(strtok(cur[0],"@"));
    (void) drizzle_fetch_row(result);        // Read eof
  }
}

static int com_prompt(DYNAMIC_STRING *buffer __attribute__((unused)),
                      char *line)
{
  char *ptr=strchr(line, ' ');
  prompt_counter = 0;
  my_free(current_prompt,MYF(MY_ALLOW_ZERO_PTR));
  current_prompt= strdup(ptr ? ptr+1 : default_prompt);
  if (!ptr)
    tee_fprintf(stdout, "Returning to default PROMPT of %s\n",
                default_prompt);
  else
    tee_fprintf(stdout, "PROMPT set to '%s'\n", current_prompt);
  return 0;
}
