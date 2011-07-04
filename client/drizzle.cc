/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Vijay Samuel
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

#include <config.h>
#include <libdrizzle/libdrizzle.h>

#include "server_detect.h"
#include "get_password.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#include <cerrno>
#include <string>
#include <drizzled/gettext.h>
#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <limits.h>
#include <cassert>
#include <stdarg.h>
#include <math.h>
#include <memory>
#include <client/linebuffer.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <drizzled/configmake.h>
#include <drizzled/utf8/utf8.h>
#include <cstdlib>

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
#include <boost/program_options.hpp>
#include <boost/scoped_ptr.hpp>
#include <drizzled/program_options/config_file.h>

#include "user_detect.h"

using namespace std;
namespace po=boost::program_options;
namespace dpo=drizzled::program_options;

/* Don't try to make a nice table if the data is too big */
const uint32_t MAX_COLUMN_LENGTH= 1024;

/* Buffer to hold 'version' and 'version_comment' */
const int MAX_SERVER_VERSION_LENGTH= 128;

/* Options used during connect */
drizzle_con_options_t global_con_options= DRIZZLE_CON_NONE;

#define PROMPT_CHAR '\\'

class Status
{
public:

  Status(int in_exit_status, 
         uint32_t in_query_start_line,
         char *in_file_name,
         LineBuffer *in_line_buff,
         bool in_batch,
         bool in_add_to_history)
    :
    exit_status(in_exit_status),
    query_start_line(in_query_start_line),
    file_name(in_file_name),
    line_buff(in_line_buff),
    batch(in_batch),
    add_to_history(in_add_to_history)
    {}

  Status() :
    exit_status(0),
    query_start_line(0),
    file_name(NULL),
    line_buff(NULL),
    batch(false),        
    add_to_history(false)
  {}
  
  int getExitStatus() const
  {
    return exit_status;
  }

  uint32_t getQueryStartLine() const
  {
    return query_start_line;
  }

  const char *getFileName() const
  {
    return file_name;
  }

  LineBuffer *getLineBuff() const
  {
    return line_buff;
  }

  bool getBatch() const
  {
    return batch;
  }

  bool getAddToHistory() const
  {
    return add_to_history;
  }

  void setExitStatus(int in_exit_status)
  {
    exit_status= in_exit_status;
  }

  void setQueryStartLine(uint32_t in_query_start_line)
  {
    query_start_line= in_query_start_line;
  }

  void setFileName(char *in_file_name)
  {
    file_name= in_file_name;
  }

  void setLineBuff(int max_size, FILE *file=NULL)
  {
    line_buff= new LineBuffer(max_size, file);
  }

  void setLineBuff(LineBuffer *in_line_buff)
  {
    line_buff= in_line_buff;
  }

  void setBatch(bool in_batch)
  {
    batch= in_batch;
  }

  void setAddToHistory(bool in_add_to_history)
  {
    add_to_history= in_add_to_history;
  }

private:
  int exit_status;
  uint32_t query_start_line;
  char *file_name;
  LineBuffer *line_buff;
  bool batch,add_to_history;
}; 

static map<string, string>::iterator completion_iter;
static map<string, string>::iterator completion_end;
static map<string, string> completion_map;
static string completion_string;


enum enum_info_type { INFO_INFO,INFO_ERROR,INFO_RESULT};
typedef enum enum_info_type INFO_TYPE;

static drizzle_st drizzle;      /* The library handle */
static drizzle_con_st con;      /* The connection */
static bool ignore_errors= false, quick= false,
  connected= false, opt_raw_data= false, unbuffered= false,
  output_tables= false, opt_rehash= true, skip_updates= false,
  safe_updates= false, one_database= false,
  opt_shutdown= false, opt_ping= false,
  vertical= false, line_numbers= true, column_names= true,
  opt_nopager= true, opt_outfile= false, named_cmds= false,
  opt_nobeep= false, opt_reconnect= true,
  opt_secure_auth= false,
  default_pager_set= false, opt_sigint_ignore= false,
  auto_vertical_output= false,
  show_warnings= false, executing_query= false, interrupted_query= false,
  use_drizzle_protocol= false, opt_local_infile;
static uint32_t opt_kill= 0;
static uint32_t show_progress_size= 0;
static bool column_types_flag;
static bool preserve_comments= false;
static uint32_t opt_max_input_line;
static uint32_t opt_drizzle_port= 0;
static int  opt_silent, verbose= 0;
static char *histfile;
static char *histfile_tmp;
static string *glob_buffer;
static string *processed_prompt= NULL;
static char *default_prompt= NULL;
static char *full_username= NULL,*part_username= NULL;
static Status status;
static uint32_t select_limit;
static uint32_t max_join_size;
static uint32_t opt_connect_timeout= 0;
static ServerDetect::server_type server_type= ServerDetect::SERVER_UNKNOWN_FOUND;
std::string current_db,
  delimiter_str,  
  current_host,
  current_prompt,
  current_user,
  opt_verbose,
  current_password,
  opt_password,
  opt_protocol;

static const char* get_day_name(int day_of_week)
{
  switch(day_of_week)
  {
  case 0:
    return _("Sun");
  case 1:
    return _("Mon");
  case 2:
    return _("Tue");
  case 3:
    return _("Wed");
  case 4:
    return _("Thu");
  case 5:
    return _("Fri");
  case 6:
    return _("Sat");
  }

  return NULL;
}

static const char* get_month_name(int month)
{
  switch(month)
  {
  case 0:
    return _("Jan");
  case 1:
    return _("Feb");
  case 2:
    return _("Mar");
  case 3:
    return _("Apr");
  case 4:
    return _("May");
  case 5:
    return _("Jun");
  case 6:
    return _("Jul");
  case 7:
    return _("Aug");
  case 8:
    return _("Sep");
  case 9:
    return _("Oct");
  case 10:
    return _("Nov");
  case 11:
    return _("Dec");
  }

  return NULL;
}

/* @TODO: Remove this */
#define FN_REFLEN 512

static string default_pager("");
static string pager("");
static string outfile("");
static FILE *PAGER, *OUTFILE;
static uint32_t prompt_counter;
static char *delimiter= NULL;
static uint32_t delimiter_length= 1;
unsigned short terminal_width= 80;

int drizzleclient_real_query_for_lazy(const char *buf, size_t length,
                                      drizzle_result_st *result,
                                      uint32_t *error_code);
int drizzleclient_store_result_for_lazy(drizzle_result_st *result);


void tee_fprintf(FILE *file, const char *fmt, ...);
void tee_fputs(const char *s, FILE *file);
void tee_puts(const char *s, FILE *file);
void tee_putc(int c, FILE *file);
static void tee_print_sized_data(const char *, unsigned int, unsigned int, bool);
/* The names of functions that actually do the manipulation. */
static int process_options(void);
static int com_quit(string *str,const char*),
  com_go(string *str,const char*), com_ego(string *str,const char*),
  com_print(string *str,const char*),
  com_help(string *str,const char*), com_clear(string *str,const char*),
  com_connect(string *str,const char*), com_status(string *str,const char*),
  com_use(string *str,const char*), com_source(string *str, const char*),
  com_shutdown(string *str,const char*),
  com_rehash(string *str, const char*), com_tee(string *str, const char*),
  com_notee(string *str, const char*),
  com_prompt(string *str, const char*), com_delimiter(string *str, const char*),
  com_warnings(string *str, const char*), com_nowarnings(string *str, const char*),
  com_nopager(string *str, const char*), com_pager(string *str, const char*);

static int read_and_execute(bool interactive);
static int sql_connect(const string &host, const string &database, const string &user, const string &password);
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
static const char * strcont(const char *str, const char *set);

/* A class which contains information on the commands this program
   can understand. */
class Commands
{
private:
  const char *name;        /* User printable name of the function. */
  char cmd_char;        /* msql command character */
public:
  Commands(const char *in_name,
           char in_cmd_char,
           int (*in_func)(string *str,const char *name),
           bool in_takes_params,
           const char *in_doc) :
    name(in_name),
    cmd_char(in_cmd_char),
    func(in_func),
    takes_params(in_takes_params),
    doc(in_doc)
  {}

  Commands() :
    name(),
    cmd_char(),
    func(NULL),
    takes_params(false),
    doc()
  {}

  int (*func)(string *str,const char *);/* Function to call to do the job. */

  const char *getName() const
  {
    return name;
  }

  char getCmdChar() const
  {
    return cmd_char;
  }

  bool getTakesParams() const
  {
    return takes_params;
  }

  const char *getDoc() const
  {
    return doc;
  }

  void setName(const char *in_name)
  {
     name= in_name;
  }

  void setCmdChar(char in_cmd_char)
  {
    cmd_char= in_cmd_char;
  }

  void setTakesParams(bool in_takes_params)
  {
    takes_params= in_takes_params;
  }

  void setDoc(const char *in_doc)
  {
    doc= in_doc;
  }

private:
  bool takes_params;        /* Max parameters for command */
  const char *doc;        /* Documentation for this function.  */
}; 


static Commands commands[] = {
  Commands( "?",      '?', com_help,   0, N_("Synonym for `help'.") ),
  Commands( "clear",  'c', com_clear,  0, N_("Clear command.")),
  Commands( "connect",'r', com_connect,1,
    N_("Reconnect to the server. Optional arguments are db and host.")),
  Commands( "delimiter", 'd', com_delimiter,    1,
    N_("Set statement delimiter. NOTE: Takes the rest of the line as new delimiter.") ),
  Commands( "ego",    'G', com_ego,    0,
    N_("Send command to drizzle server, display result vertically.")),
  Commands( "exit",   'q', com_quit,   0, N_("Exit drizzle. Same as quit.")),
  Commands( "go",     'g', com_go,     0, N_("Send command to drizzle server.") ),
  Commands( "help",   'h', com_help,   0, N_("Display this help.") ),
  Commands( "nopager",'n', com_nopager,0, N_("Disable pager, print to stdout.") ),
  Commands( "notee",  't', com_notee,  0, N_("Don't write into outfile.") ),
  Commands( "pager",  'P', com_pager,  1,
    N_("Set PAGER [to_pager]. Print the query results via PAGER.") ),
  Commands( "print",  'p', com_print,  0, N_("Print current command.") ),
  Commands( "prompt", 'R', com_prompt, 1, N_("Change your drizzle prompt.")),
  Commands( "quit",   'q', com_quit,   0, N_("Quit drizzle.") ),
  Commands( "rehash", '#', com_rehash, 0, N_("Rebuild completion hash.") ),
  Commands( "source", '.', com_source, 1,
    N_("Execute an SQL script file. Takes a file name as an argument.")),
  Commands( "status", 's', com_status, 0, N_("Get status information from the server.")),
  Commands( "tee",    'T', com_tee,    1,
    N_("Set outfile [to_outfile]. Append everything into given outfile.") ),
  Commands( "use",    'u', com_use,    1,
    N_("Use another schema. Takes schema name as argument.") ),
  Commands( "shutdown",    'Q', com_shutdown,    false,
    N_("Shutdown the instance you are connected too.") ),
  Commands( "warnings", 'W', com_warnings,  false,
    N_("Show warnings after every statement.") ),
  Commands( "nowarning", 'w', com_nowarnings, 0,
    N_("Don't show warnings after every statement.") ),
  /* Get bash-like expansion for some commands */
  Commands( "create table",     0, 0, 0, ""),
  Commands( "create database",  0, 0, 0, ""),
  Commands( "show databases",   0, 0, 0, ""),
  Commands( "show fields from", 0, 0, 0, ""),
  Commands( "show keys from",   0, 0, 0, ""),
  Commands( "show tables",      0, 0, 0, ""),
  Commands( "load data from",   0, 0, 0, ""),
  Commands( "alter table",      0, 0, 0, ""),
  Commands( "set option",       0, 0, 0, ""),
  Commands( "lock tables",      0, 0, 0, ""),
  Commands( "unlock tables",    0, 0, 0, ""),
  /* generated 2006-12-28.  Refresh occasionally from lexer. */
  Commands( "ACTION", 0, 0, 0, ""),
  Commands( "ADD", 0, 0, 0, ""),
  Commands( "AFTER", 0, 0, 0, ""),
  Commands( "AGAINST", 0, 0, 0, ""),
  Commands( "AGGREGATE", 0, 0, 0, ""),
  Commands( "ALL", 0, 0, 0, ""),
  Commands( "ALGORITHM", 0, 0, 0, ""),
  Commands( "ALTER", 0, 0, 0, ""),
  Commands( "ANALYZE", 0, 0, 0, ""),
  Commands( "AND", 0, 0, 0, ""),
  Commands( "ANY", 0, 0, 0, ""),
  Commands( "AS", 0, 0, 0, ""),
  Commands( "ASC", 0, 0, 0, ""),
  Commands( "ASCII", 0, 0, 0, ""),
  Commands( "ASENSITIVE", 0, 0, 0, ""),
  Commands( "AUTO_INCREMENT", 0, 0, 0, ""),
  Commands( "AVG", 0, 0, 0, ""),
  Commands( "AVG_ROW_LENGTH", 0, 0, 0, ""),
  Commands( "BEFORE", 0, 0, 0, ""),
  Commands( "BEGIN", 0, 0, 0, ""),
  Commands( "BETWEEN", 0, 0, 0, ""),
  Commands( "BIGINT", 0, 0, 0, ""),
  Commands( "BINARY", 0, 0, 0, ""),
  Commands( "BIT", 0, 0, 0, ""),
  Commands( "BLOB", 0, 0, 0, ""),
  Commands( "BOOL", 0, 0, 0, ""),
  Commands( "BOOLEAN", 0, 0, 0, ""),
  Commands( "BOTH", 0, 0, 0, ""),
  Commands( "BTREE", 0, 0, 0, ""),
  Commands( "BY", 0, 0, 0, ""),
  Commands( "BYTE", 0, 0, 0, ""),
  Commands( "CACHE", 0, 0, 0, ""),
  Commands( "CALL", 0, 0, 0, ""),
  Commands( "CASCADE", 0, 0, 0, ""),
  Commands( "CASCADED", 0, 0, 0, ""),
  Commands( "CASE", 0, 0, 0, ""),
  Commands( "CHAIN", 0, 0, 0, ""),
  Commands( "CHANGE", 0, 0, 0, ""),
  Commands( "CHANGED", 0, 0, 0, ""),
  Commands( "CHAR", 0, 0, 0, ""),
  Commands( "CHARACTER", 0, 0, 0, ""),
  Commands( "CHECK", 0, 0, 0, ""),
  Commands( "CHECKSUM", 0, 0, 0, ""),
  Commands( "CLIENT", 0, 0, 0, ""),
  Commands( "CLOSE", 0, 0, 0, ""),
  Commands( "COLLATE", 0, 0, 0, ""),
  Commands( "COLLATION", 0, 0, 0, ""),
  Commands( "COLUMN", 0, 0, 0, ""),
  Commands( "COLUMNS", 0, 0, 0, ""),
  Commands( "COMMENT", 0, 0, 0, ""),
  Commands( "COMMIT", 0, 0, 0, ""),
  Commands( "COMMITTED", 0, 0, 0, ""),
  Commands( "COMPACT", 0, 0, 0, ""),
  Commands( "COMPRESSED", 0, 0, 0, ""),
  Commands( "CONCURRENT", 0, 0, 0, ""),
  Commands( "CONDITION", 0, 0, 0, ""),
  Commands( "CONNECTION", 0, 0, 0, ""),
  Commands( "CONSISTENT", 0, 0, 0, ""),
  Commands( "CONSTRAINT", 0, 0, 0, ""),
  Commands( "CONTAINS", 0, 0, 0, ""),
  Commands( "CONTINUE", 0, 0, 0, ""),
  Commands( "CONVERT", 0, 0, 0, ""),
  Commands( "CREATE", 0, 0, 0, ""),
  Commands( "CROSS", 0, 0, 0, ""),
  Commands( "CUBE", 0, 0, 0, ""),
  Commands( "CURRENT_DATE", 0, 0, 0, ""),
  Commands( "CURRENT_TIMESTAMP", 0, 0, 0, ""),
  Commands( "CURRENT_USER", 0, 0, 0, ""),
  Commands( "CURSOR", 0, 0, 0, ""),
  Commands( "DATA", 0, 0, 0, ""),
  Commands( "DATABASE", 0, 0, 0, ""),
  Commands( "DATABASES", 0, 0, 0, ""),
  Commands( "DATE", 0, 0, 0, ""),
  Commands( "DATETIME", 0, 0, 0, ""),
  Commands( "DAY", 0, 0, 0, ""),
  Commands( "DAY_HOUR", 0, 0, 0, ""),
  Commands( "DAY_MICROSECOND", 0, 0, 0, ""),
  Commands( "DAY_MINUTE", 0, 0, 0, ""),
  Commands( "DAY_SECOND", 0, 0, 0, ""),
  Commands( "DEALLOCATE", 0, 0, 0, ""),
  Commands( "DEC", 0, 0, 0, ""),
  Commands( "DECIMAL", 0, 0, 0, ""),
  Commands( "DECLARE", 0, 0, 0, ""),
  Commands( "DEFAULT", 0, 0, 0, ""),
  Commands( "DEFINER", 0, 0, 0, ""),
  Commands( "DELAYED", 0, 0, 0, ""),
  Commands( "DELETE", 0, 0, 0, ""),
  Commands( "DESC", 0, 0, 0, ""),
  Commands( "DESCRIBE", 0, 0, 0, ""),
  Commands( "DETERMINISTIC", 0, 0, 0, ""),
  Commands( "DISABLE", 0, 0, 0, ""),
  Commands( "DISCARD", 0, 0, 0, ""),
  Commands( "DISTINCT", 0, 0, 0, ""),
  Commands( "DISTINCTROW", 0, 0, 0, ""),
  Commands( "DIV", 0, 0, 0, ""),
  Commands( "DOUBLE", 0, 0, 0, ""),
  Commands( "DROP", 0, 0, 0, ""),
  Commands( "DUMPFILE", 0, 0, 0, ""),
  Commands( "DUPLICATE", 0, 0, 0, ""),
  Commands( "DYNAMIC", 0, 0, 0, ""),
  Commands( "EACH", 0, 0, 0, ""),
  Commands( "ELSE", 0, 0, 0, ""),
  Commands( "ELSEIF", 0, 0, 0, ""),
  Commands( "ENABLE", 0, 0, 0, ""),
  Commands( "ENCLOSED", 0, 0, 0, ""),
  Commands( "END", 0, 0, 0, ""),
  Commands( "ENGINE", 0, 0, 0, ""),
  Commands( "ENGINES", 0, 0, 0, ""),
  Commands( "ENUM", 0, 0, 0, ""),
  Commands( "ERRORS", 0, 0, 0, ""),
  Commands( "ESCAPE", 0, 0, 0, ""),
  Commands( "ESCAPED", 0, 0, 0, ""),
  Commands( "EXISTS", 0, 0, 0, ""),
  Commands( "EXIT", 0, 0, 0, ""),
  Commands( "EXPLAIN", 0, 0, 0, ""),
  Commands( "EXTENDED", 0, 0, 0, ""),
  Commands( "FALSE", 0, 0, 0, ""),
  Commands( "FAST", 0, 0, 0, ""),
  Commands( "FETCH", 0, 0, 0, ""),
  Commands( "FIELDS", 0, 0, 0, ""),
  Commands( "FILE", 0, 0, 0, ""),
  Commands( "FIRST", 0, 0, 0, ""),
  Commands( "FIXED", 0, 0, 0, ""),
  Commands( "FLOAT", 0, 0, 0, ""),
  Commands( "FLOAT4", 0, 0, 0, ""),
  Commands( "FLOAT8", 0, 0, 0, ""),
  Commands( "FLUSH", 0, 0, 0, ""),
  Commands( "FOR", 0, 0, 0, ""),
  Commands( "FORCE", 0, 0, 0, ""),
  Commands( "FOREIGN", 0, 0, 0, ""),
  Commands( "FOUND", 0, 0, 0, ""),
  Commands( "FRAC_SECOND", 0, 0, 0, ""),
  Commands( "FROM", 0, 0, 0, ""),
  Commands( "FULL", 0, 0, 0, ""),
  Commands( "FUNCTION", 0, 0, 0, ""),
  Commands( "GLOBAL", 0, 0, 0, ""),
  Commands( "GRANT", 0, 0, 0, ""),
  Commands( "GRANTS", 0, 0, 0, ""),
  Commands( "GROUP", 0, 0, 0, ""),
  Commands( "HANDLER", 0, 0, 0, ""),
  Commands( "HASH", 0, 0, 0, ""),
  Commands( "HAVING", 0, 0, 0, ""),
  Commands( "HELP", 0, 0, 0, ""),
  Commands( "HIGH_PRIORITY", 0, 0, 0, ""),
  Commands( "HOSTS", 0, 0, 0, ""),
  Commands( "HOUR", 0, 0, 0, ""),
  Commands( "HOUR_MICROSECOND", 0, 0, 0, ""),
  Commands( "HOUR_MINUTE", 0, 0, 0, ""),
  Commands( "HOUR_SECOND", 0, 0, 0, ""),
  Commands( "IDENTIFIED", 0, 0, 0, ""),
  Commands( "IF", 0, 0, 0, ""),
  Commands( "IGNORE", 0, 0, 0, ""),
  Commands( "IMPORT", 0, 0, 0, ""),
  Commands( "IN", 0, 0, 0, ""),
  Commands( "INDEX", 0, 0, 0, ""),
  Commands( "INDEXES", 0, 0, 0, ""),
  Commands( "INFILE", 0, 0, 0, ""),
  Commands( "INNER", 0, 0, 0, ""),
  Commands( "INNOBASE", 0, 0, 0, ""),
  Commands( "INNODB", 0, 0, 0, ""),
  Commands( "INOUT", 0, 0, 0, ""),
  Commands( "INSENSITIVE", 0, 0, 0, ""),
  Commands( "INSERT", 0, 0, 0, ""),
  Commands( "INSERT_METHOD", 0, 0, 0, ""),
  Commands( "INT", 0, 0, 0, ""),
  Commands( "INT1", 0, 0, 0, ""),
  Commands( "INT2", 0, 0, 0, ""),
  Commands( "INT3", 0, 0, 0, ""),
  Commands( "INT4", 0, 0, 0, ""),
  Commands( "INT8", 0, 0, 0, ""),
  Commands( "INTEGER", 0, 0, 0, ""),
  Commands( "INTERVAL", 0, 0, 0, ""),
  Commands( "INTO", 0, 0, 0, ""),
  Commands( "IO_THREAD", 0, 0, 0, ""),
  Commands( "IS", 0, 0, 0, ""),
  Commands( "ISOLATION", 0, 0, 0, ""),
  Commands( "ISSUER", 0, 0, 0, ""),
  Commands( "ITERATE", 0, 0, 0, ""),
  Commands( "INVOKER", 0, 0, 0, ""),
  Commands( "JOIN", 0, 0, 0, ""),
  Commands( "KEY", 0, 0, 0, ""),
  Commands( "KEYS", 0, 0, 0, ""),
  Commands( "KILL", 0, 0, 0, ""),
  Commands( "LANGUAGE", 0, 0, 0, ""),
  Commands( "LAST", 0, 0, 0, ""),
  Commands( "LEADING", 0, 0, 0, ""),
  Commands( "LEAVE", 0, 0, 0, ""),
  Commands( "LEAVES", 0, 0, 0, ""),
  Commands( "LEFT", 0, 0, 0, ""),
  Commands( "LEVEL", 0, 0, 0, ""),
  Commands( "LIKE", 0, 0, 0, ""),
  Commands( "LIMIT", 0, 0, 0, ""),
  Commands( "LINES", 0, 0, 0, ""),
  Commands( "LINESTRING", 0, 0, 0, ""),
  Commands( "LOAD", 0, 0, 0, ""),
  Commands( "LOCAL", 0, 0, 0, ""),
  Commands( "LOCALTIMESTAMP", 0, 0, 0, ""),
  Commands( "LOCK", 0, 0, 0, ""),
  Commands( "LOCKS", 0, 0, 0, ""),
  Commands( "LOGS", 0, 0, 0, ""),
  Commands( "LONG", 0, 0, 0, ""),
  Commands( "LOOP", 0, 0, 0, ""),
  Commands( "MATCH", 0, 0, 0, ""),
  Commands( "MAX_CONNECTIONS_PER_HOUR", 0, 0, 0, ""),
  Commands( "MAX_QUERIES_PER_HOUR", 0, 0, 0, ""),
  Commands( "MAX_ROWS", 0, 0, 0, ""),
  Commands( "MAX_UPDATES_PER_HOUR", 0, 0, 0, ""),
  Commands( "MAX_USER_CONNECTIONS", 0, 0, 0, ""),
  Commands( "MEDIUM", 0, 0, 0, ""),
  Commands( "MERGE", 0, 0, 0, ""),
  Commands( "MICROSECOND", 0, 0, 0, ""),
  Commands( "MIGRATE", 0, 0, 0, ""),
  Commands( "MINUTE", 0, 0, 0, ""),
  Commands( "MINUTE_MICROSECOND", 0, 0, 0, ""),
  Commands( "MINUTE_SECOND", 0, 0, 0, ""),
  Commands( "MIN_ROWS", 0, 0, 0, ""),
  Commands( "MOD", 0, 0, 0, ""),
  Commands( "MODE", 0, 0, 0, ""),
  Commands( "MODIFIES", 0, 0, 0, ""),
  Commands( "MODIFY", 0, 0, 0, ""),
  Commands( "MONTH", 0, 0, 0, ""),
  Commands( "MULTILINESTRING", 0, 0, 0, ""),
  Commands( "MULTIPOINT", 0, 0, 0, ""),
  Commands( "MULTIPOLYGON", 0, 0, 0, ""),
  Commands( "MUTEX", 0, 0, 0, ""),
  Commands( "NAME", 0, 0, 0, ""),
  Commands( "NAMES", 0, 0, 0, ""),
  Commands( "NATIONAL", 0, 0, 0, ""),
  Commands( "NATURAL", 0, 0, 0, ""),
  Commands( "NCHAR", 0, 0, 0, ""),
  Commands( "NEW", 0, 0, 0, ""),
  Commands( "NEXT", 0, 0, 0, ""),
  Commands( "NO", 0, 0, 0, ""),
  Commands( "NONE", 0, 0, 0, ""),
  Commands( "NOT", 0, 0, 0, ""),
  Commands( "NULL", 0, 0, 0, ""),
  Commands( "NUMERIC", 0, 0, 0, ""),
  Commands( "NVARCHAR", 0, 0, 0, ""),
  Commands( "OFFSET", 0, 0, 0, ""),
  Commands( "ON", 0, 0, 0, ""),
  Commands( "ONE", 0, 0, 0, ""),
  Commands( "ONE_SHOT", 0, 0, 0, ""),
  Commands( "OPEN", 0, 0, 0, ""),
  Commands( "OPTIMIZE", 0, 0, 0, ""),
  Commands( "OPTION", 0, 0, 0, ""),
  Commands( "OPTIONALLY", 0, 0, 0, ""),
  Commands( "OR", 0, 0, 0, ""),
  Commands( "ORDER", 0, 0, 0, ""),
  Commands( "OUT", 0, 0, 0, ""),
  Commands( "OUTER", 0, 0, 0, ""),
  Commands( "OUTFILE", 0, 0, 0, ""),
  Commands( "PACK_KEYS", 0, 0, 0, ""),
  Commands( "PARTIAL", 0, 0, 0, ""),
  Commands( "PASSWORD", 0, 0, 0, ""),
  Commands( "PHASE", 0, 0, 0, ""),
  Commands( "PRECISION", 0, 0, 0, ""),
  Commands( "PREPARE", 0, 0, 0, ""),
  Commands( "PREV", 0, 0, 0, ""),
  Commands( "PRIMARY", 0, 0, 0, ""),
  Commands( "PRIVILEGES", 0, 0, 0, ""),
  Commands( "PROCEDURE", 0, 0, 0, ""),
  Commands( "PROCESS", 0, 0, 0, ""),
  Commands( "PROCESSLIST", 0, 0, 0, ""),
  Commands( "PURGE", 0, 0, 0, ""),
  Commands( "QUARTER", 0, 0, 0, ""),
  Commands( "QUERY", 0, 0, 0, ""),
  Commands( "QUICK", 0, 0, 0, ""),
  Commands( "READ", 0, 0, 0, ""),
  Commands( "READS", 0, 0, 0, ""),
  Commands( "REAL", 0, 0, 0, ""),
  Commands( "RECOVER", 0, 0, 0, ""),
  Commands( "REDUNDANT", 0, 0, 0, ""),
  Commands( "REFERENCES", 0, 0, 0, ""),
  Commands( "REGEXP", 0, 0, 0, ""),
  Commands( "RELEASE", 0, 0, 0, ""),
  Commands( "RELOAD", 0, 0, 0, ""),
  Commands( "RENAME", 0, 0, 0, ""),
  Commands( "REPAIR", 0, 0, 0, ""),
  Commands( "REPEATABLE", 0, 0, 0, ""),
  Commands( "REPLACE", 0, 0, 0, ""),
  Commands( "REPEAT", 0, 0, 0, ""),
  Commands( "REQUIRE", 0, 0, 0, ""),
  Commands( "RESET", 0, 0, 0, ""),
  Commands( "RESTORE", 0, 0, 0, ""),
  Commands( "RESTRICT", 0, 0, 0, ""),
  Commands( "RESUME", 0, 0, 0, ""),
  Commands( "RETURN", 0, 0, 0, ""),
  Commands( "RETURNS", 0, 0, 0, ""),
  Commands( "REVOKE", 0, 0, 0, ""),
  Commands( "RIGHT", 0, 0, 0, ""),
  Commands( "RLIKE", 0, 0, 0, ""),
  Commands( "ROLLBACK", 0, 0, 0, ""),
  Commands( "ROLLUP", 0, 0, 0, ""),
  Commands( "ROUTINE", 0, 0, 0, ""),
  Commands( "ROW", 0, 0, 0, ""),
  Commands( "ROWS", 0, 0, 0, ""),
  Commands( "ROW_FORMAT", 0, 0, 0, ""),
  Commands( "RTREE", 0, 0, 0, ""),
  Commands( "SAVEPOINT", 0, 0, 0, ""),
  Commands( "SCHEMA", 0, 0, 0, ""),
  Commands( "SCHEMAS", 0, 0, 0, ""),
  Commands( "SECOND", 0, 0, 0, ""),
  Commands( "SECOND_MICROSECOND", 0, 0, 0, ""),
  Commands( "SECURITY", 0, 0, 0, ""),
  Commands( "SELECT", 0, 0, 0, ""),
  Commands( "SENSITIVE", 0, 0, 0, ""),
  Commands( "SEPARATOR", 0, 0, 0, ""),
  Commands( "SERIAL", 0, 0, 0, ""),
  Commands( "SERIALIZABLE", 0, 0, 0, ""),
  Commands( "SESSION", 0, 0, 0, ""),
  Commands( "SET", 0, 0, 0, ""),
  Commands( "SHARE", 0, 0, 0, ""),
  Commands( "SHOW", 0, 0, 0, ""),
  Commands( "SHUTDOWN", 0, 0, 0, ""),
  Commands( "SIGNED", 0, 0, 0, ""),
  Commands( "SIMPLE", 0, 0, 0, ""),
  Commands( "SLAVE", 0, 0, 0, ""),
  Commands( "SNAPSHOT", 0, 0, 0, ""),
  Commands( "SOME", 0, 0, 0, ""),
  Commands( "SONAME", 0, 0, 0, ""),
  Commands( "SOUNDS", 0, 0, 0, ""),
  Commands( "SPATIAL", 0, 0, 0, ""),
  Commands( "SPECIFIC", 0, 0, 0, ""),
  Commands( "SQL", 0, 0, 0, ""),
  Commands( "SQLEXCEPTION", 0, 0, 0, ""),
  Commands( "SQLSTATE", 0, 0, 0, ""),
  Commands( "SQLWARNING", 0, 0, 0, ""),
  Commands( "SQL_BIG_RESULT", 0, 0, 0, ""),
  Commands( "SQL_BUFFER_RESULT", 0, 0, 0, ""),
  Commands( "SQL_CACHE", 0, 0, 0, ""),
  Commands( "SQL_CALC_FOUND_ROWS", 0, 0, 0, ""),
  Commands( "SQL_NO_CACHE", 0, 0, 0, ""),
  Commands( "SQL_SMALL_RESULT", 0, 0, 0, ""),
  Commands( "SQL_THREAD", 0, 0, 0, ""),
  Commands( "SQL_TSI_FRAC_SECOND", 0, 0, 0, ""),
  Commands( "SQL_TSI_SECOND", 0, 0, 0, ""),
  Commands( "SQL_TSI_MINUTE", 0, 0, 0, ""),
  Commands( "SQL_TSI_HOUR", 0, 0, 0, ""),
  Commands( "SQL_TSI_DAY", 0, 0, 0, ""),
  Commands( "SQL_TSI_WEEK", 0, 0, 0, ""),
  Commands( "SQL_TSI_MONTH", 0, 0, 0, ""),
  Commands( "SQL_TSI_QUARTER", 0, 0, 0, ""),
  Commands( "SQL_TSI_YEAR", 0, 0, 0, ""),
  Commands( "SSL", 0, 0, 0, ""),
  Commands( "START", 0, 0, 0, ""),
  Commands( "STARTING", 0, 0, 0, ""),
  Commands( "STATUS", 0, 0, 0, ""),
  Commands( "STOP", 0, 0, 0, ""),
  Commands( "STORAGE", 0, 0, 0, ""),
  Commands( "STRAIGHT_JOIN", 0, 0, 0, ""),
  Commands( "STRING", 0, 0, 0, ""),
  Commands( "STRIPED", 0, 0, 0, ""),
  Commands( "SUBJECT", 0, 0, 0, ""),
  Commands( "SUPER", 0, 0, 0, ""),
  Commands( "SUSPEND", 0, 0, 0, ""),
  Commands( "TABLE", 0, 0, 0, ""),
  Commands( "TABLES", 0, 0, 0, ""),
  Commands( "TABLESPACE", 0, 0, 0, ""),
  Commands( "TEMPORARY", 0, 0, 0, ""),
  Commands( "TEMPTABLE", 0, 0, 0, ""),
  Commands( "TERMINATED", 0, 0, 0, ""),
  Commands( "TEXT", 0, 0, 0, ""),
  Commands( "THEN", 0, 0, 0, ""),
  Commands( "TIMESTAMP", 0, 0, 0, ""),
  Commands( "TIMESTAMPADD", 0, 0, 0, ""),
  Commands( "TIMESTAMPDIFF", 0, 0, 0, ""),
  Commands( "TO", 0, 0, 0, ""),
  Commands( "TRAILING", 0, 0, 0, ""),
  Commands( "TRANSACTION", 0, 0, 0, ""),
  Commands( "TRUE", 0, 0, 0, ""),
  Commands( "TRUNCATE", 0, 0, 0, ""),
  Commands( "TYPE", 0, 0, 0, ""),
  Commands( "TYPES", 0, 0, 0, ""),
  Commands( "UNCOMMITTED", 0, 0, 0, ""),
  Commands( "UNDEFINED", 0, 0, 0, ""),
  Commands( "UNDO", 0, 0, 0, ""),
  Commands( "UNICODE", 0, 0, 0, ""),
  Commands( "UNION", 0, 0, 0, ""),
  Commands( "UNIQUE", 0, 0, 0, ""),
  Commands( "UNKNOWN", 0, 0, 0, ""),
  Commands( "UNLOCK", 0, 0, 0, ""),
  Commands( "UNTIL", 0, 0, 0, ""),
  Commands( "UPDATE", 0, 0, 0, ""),
  Commands( "UPGRADE", 0, 0, 0, ""),
  Commands( "USAGE", 0, 0, 0, ""),
  Commands( "USE", 0, 0, 0, ""),
  Commands( "USER", 0, 0, 0, ""),
  Commands( "USER_RESOURCES", 0, 0, 0, ""),
  Commands( "USING", 0, 0, 0, ""),
  Commands( "UTC_DATE", 0, 0, 0, ""),
  Commands( "UTC_TIMESTAMP", 0, 0, 0, ""),
  Commands( "VALUE", 0, 0, 0, ""),
  Commands( "VALUES", 0, 0, 0, ""),
  Commands( "VARBINARY", 0, 0, 0, ""),
  Commands( "VARCHAR", 0, 0, 0, ""),
  Commands( "VARCHARACTER", 0, 0, 0, ""),
  Commands( "VARIABLES", 0, 0, 0, ""),
  Commands( "VARYING", 0, 0, 0, ""),
  Commands( "WARNINGS", 0, 0, 0, ""),
  Commands( "WEEK", 0, 0, 0, ""),
  Commands( "WHEN", 0, 0, 0, ""),
  Commands( "WHERE", 0, 0, 0, ""),
  Commands( "WHILE", 0, 0, 0, ""),
  Commands( "VIEW", 0, 0, 0, ""),
  Commands( "WITH", 0, 0, 0, ""),
  Commands( "WORK", 0, 0, 0, ""),
  Commands( "WRITE", 0, 0, 0, ""),
  Commands( "XOR", 0, 0, 0, ""),
  Commands( "XA", 0, 0, 0, ""),
  Commands( "YEAR", 0, 0, 0, ""),
  Commands( "YEAR_MONTH", 0, 0, 0, ""),
  Commands( "ZEROFILL", 0, 0, 0, ""),
  Commands( "ABS", 0, 0, 0, ""),
  Commands( "ACOS", 0, 0, 0, ""),
  Commands( "ADDDATE", 0, 0, 0, ""),
  Commands( "AREA", 0, 0, 0, ""),
  Commands( "ASIN", 0, 0, 0, ""),
  Commands( "ASBINARY", 0, 0, 0, ""),
  Commands( "ASTEXT", 0, 0, 0, ""),
  Commands( "ATAN", 0, 0, 0, ""),
  Commands( "ATAN2", 0, 0, 0, ""),
  Commands( "BENCHMARK", 0, 0, 0, ""),
  Commands( "BIN", 0, 0, 0, ""),
  Commands( "BIT_OR", 0, 0, 0, ""),
  Commands( "BIT_AND", 0, 0, 0, ""),
  Commands( "BIT_XOR", 0, 0, 0, ""),
  Commands( "CAST", 0, 0, 0, ""),
  Commands( "CEIL", 0, 0, 0, ""),
  Commands( "CEILING", 0, 0, 0, ""),
  Commands( "CENTROID", 0, 0, 0, ""),
  Commands( "CHAR_LENGTH", 0, 0, 0, ""),
  Commands( "CHARACTER_LENGTH", 0, 0, 0, ""),
  Commands( "COALESCE", 0, 0, 0, ""),
  Commands( "COERCIBILITY", 0, 0, 0, ""),
  Commands( "COMPRESS", 0, 0, 0, ""),
  Commands( "CONCAT", 0, 0, 0, ""),
  Commands( "CONCAT_WS", 0, 0, 0, ""),
  Commands( "CONNECTION_ID", 0, 0, 0, ""),
  Commands( "CONV", 0, 0, 0, ""),
  Commands( "CONVERT_TZ", 0, 0, 0, ""),
  Commands( "COUNT", 0, 0, 0, ""),
  Commands( "COS", 0, 0, 0, ""),
  Commands( "COT", 0, 0, 0, ""),
  Commands( "CRC32", 0, 0, 0, ""),
  Commands( "CROSSES", 0, 0, 0, ""),
  Commands( "CURDATE", 0, 0, 0, ""),
  Commands( "DATE_ADD", 0, 0, 0, ""),
  Commands( "DATEDIFF", 0, 0, 0, ""),
  Commands( "DATE_FORMAT", 0, 0, 0, ""),
  Commands( "DATE_SUB", 0, 0, 0, ""),
  Commands( "DAYNAME", 0, 0, 0, ""),
  Commands( "DAYOFMONTH", 0, 0, 0, ""),
  Commands( "DAYOFWEEK", 0, 0, 0, ""),
  Commands( "DAYOFYEAR", 0, 0, 0, ""),
  Commands( "DECODE", 0, 0, 0, ""),
  Commands( "DEGREES", 0, 0, 0, ""),
  Commands( "DES_ENCRYPT", 0, 0, 0, ""),
  Commands( "DES_DECRYPT", 0, 0, 0, ""),
  Commands( "DIMENSION", 0, 0, 0, ""),
  Commands( "DISJOINT", 0, 0, 0, ""),
  Commands( "ELT", 0, 0, 0, ""),
  Commands( "ENCODE", 0, 0, 0, ""),
  Commands( "ENCRYPT", 0, 0, 0, ""),
  Commands( "ENDPOINT", 0, 0, 0, ""),
  Commands( "ENVELOPE", 0, 0, 0, ""),
  Commands( "EQUALS", 0, 0, 0, ""),
  Commands( "EXTERIORRING", 0, 0, 0, ""),
  Commands( "EXTRACT", 0, 0, 0, ""),
  Commands( "EXP", 0, 0, 0, ""),
  Commands( "EXPORT_SET", 0, 0, 0, ""),
  Commands( "FIELD", 0, 0, 0, ""),
  Commands( "FIND_IN_SET", 0, 0, 0, ""),
  Commands( "FLOOR", 0, 0, 0, ""),
  Commands( "FORMAT", 0, 0, 0, ""),
  Commands( "FOUND_ROWS", 0, 0, 0, ""),
  Commands( "FROM_DAYS", 0, 0, 0, ""),
  Commands( "FROM_UNIXTIME", 0, 0, 0, ""),
  Commands( "GET_LOCK", 0, 0, 0, ""),
  Commands( "GLENGTH", 0, 0, 0, ""),
  Commands( "GREATEST", 0, 0, 0, ""),
  Commands( "GROUP_CONCAT", 0, 0, 0, ""),
  Commands( "GROUP_UNIQUE_USERS", 0, 0, 0, ""),
  Commands( "HEX", 0, 0, 0, ""),
  Commands( "IFNULL", 0, 0, 0, ""),
  Commands( "INSTR", 0, 0, 0, ""),
  Commands( "INTERIORRINGN", 0, 0, 0, ""),
  Commands( "INTERSECTS", 0, 0, 0, ""),
  Commands( "ISCLOSED", 0, 0, 0, ""),
  Commands( "ISEMPTY", 0, 0, 0, ""),
  Commands( "ISNULL", 0, 0, 0, ""),
  Commands( "IS_FREE_LOCK", 0, 0, 0, ""),
  Commands( "IS_USED_LOCK", 0, 0, 0, ""),
  Commands( "LAST_INSERT_ID", 0, 0, 0, ""),
  Commands( "ISSIMPLE", 0, 0, 0, ""),
  Commands( "LAST_DAY", 0, 0, 0, ""),
  Commands( "LCASE", 0, 0, 0, ""),
  Commands( "LEAST", 0, 0, 0, ""),
  Commands( "LENGTH", 0, 0, 0, ""),
  Commands( "LN", 0, 0, 0, ""),
  Commands( "LOAD_FILE", 0, 0, 0, ""),
  Commands( "LOCATE", 0, 0, 0, ""),
  Commands( "LOG", 0, 0, 0, ""),
  Commands( "LOG2", 0, 0, 0, ""),
  Commands( "LOG10", 0, 0, 0, ""),
  Commands( "LOWER", 0, 0, 0, ""),
  Commands( "LPAD", 0, 0, 0, ""),
  Commands( "LTRIM", 0, 0, 0, ""),
  Commands( "MAKE_SET", 0, 0, 0, ""),
  Commands( "MAKEDATE", 0, 0, 0, ""),
  Commands( "MASTER_POS_WAIT", 0, 0, 0, ""),
  Commands( "MAX", 0, 0, 0, ""),
  Commands( "MBRCONTAINS", 0, 0, 0, ""),
  Commands( "MBRDISJOINT", 0, 0, 0, ""),
  Commands( "MBREQUAL", 0, 0, 0, ""),
  Commands( "MBRINTERSECTS", 0, 0, 0, ""),
  Commands( "MBROVERLAPS", 0, 0, 0, ""),
  Commands( "MBRTOUCHES", 0, 0, 0, ""),
  Commands( "MBRWITHIN", 0, 0, 0, ""),
  Commands( "MD5", 0, 0, 0, ""),
  Commands( "MID", 0, 0, 0, ""),
  Commands( "MIN", 0, 0, 0, ""),
  Commands( "MONTHNAME", 0, 0, 0, ""),
  Commands( "NAME_CONST", 0, 0, 0, ""),
  Commands( "NOW", 0, 0, 0, ""),
  Commands( "NULLIF", 0, 0, 0, ""),
  Commands( "NUMPOINTS", 0, 0, 0, ""),
  Commands( "OCTET_LENGTH", 0, 0, 0, ""),
  Commands( "OCT", 0, 0, 0, ""),
  Commands( "ORD", 0, 0, 0, ""),
  Commands( "OVERLAPS", 0, 0, 0, ""),
  Commands( "PERIOD_ADD", 0, 0, 0, ""),
  Commands( "PERIOD_DIFF", 0, 0, 0, ""),
  Commands( "PI", 0, 0, 0, ""),
  Commands( "POINTN", 0, 0, 0, ""),
  Commands( "POSITION", 0, 0, 0, ""),
  Commands( "POW", 0, 0, 0, ""),
  Commands( "POWER", 0, 0, 0, ""),
  Commands( "QUOTE", 0, 0, 0, ""),
  Commands( "RADIANS", 0, 0, 0, ""),
  Commands( "RAND", 0, 0, 0, ""),
  Commands( "RELEASE_LOCK", 0, 0, 0, ""),
  Commands( "REVERSE", 0, 0, 0, ""),
  Commands( "ROUND", 0, 0, 0, ""),
  Commands( "ROW_COUNT", 0, 0, 0, ""),
  Commands( "RPAD", 0, 0, 0, ""),
  Commands( "RTRIM", 0, 0, 0, ""),
  Commands( "SESSION_USER", 0, 0, 0, ""),
  Commands( "SUBDATE", 0, 0, 0, ""),
  Commands( "SIGN", 0, 0, 0, ""),
  Commands( "SIN", 0, 0, 0, ""),
  Commands( "SHA", 0, 0, 0, ""),
  Commands( "SHA1", 0, 0, 0, ""),
  Commands( "SLEEP", 0, 0, 0, ""),
  Commands( "SOUNDEX", 0, 0, 0, ""),
  Commands( "SPACE", 0, 0, 0, ""),
  Commands( "SQRT", 0, 0, 0, ""),
  Commands( "SRID", 0, 0, 0, ""),
  Commands( "STARTPOINT", 0, 0, 0, ""),
  Commands( "STD", 0, 0, 0, ""),
  Commands( "STDDEV", 0, 0, 0, ""),
  Commands( "STDDEV_POP", 0, 0, 0, ""),
  Commands( "STDDEV_SAMP", 0, 0, 0, ""),
  Commands( "STR_TO_DATE", 0, 0, 0, ""),
  Commands( "STRCMP", 0, 0, 0, ""),
  Commands( "SUBSTR", 0, 0, 0, ""),
  Commands( "SUBSTRING", 0, 0, 0, ""),
  Commands( "SUBSTRING_INDEX", 0, 0, 0, ""),
  Commands( "SUM", 0, 0, 0, ""),
  Commands( "SYSDATE", 0, 0, 0, ""),
  Commands( "SYSTEM_USER", 0, 0, 0, ""),
  Commands( "TAN", 0, 0, 0, ""),
  Commands( "TIME_FORMAT", 0, 0, 0, ""),
  Commands( "TO_DAYS", 0, 0, 0, ""),
  Commands( "TOUCHES", 0, 0, 0, ""),
  Commands( "TRIM", 0, 0, 0, ""),
  Commands( "UCASE", 0, 0, 0, ""),
  Commands( "UNCOMPRESS", 0, 0, 0, ""),
  Commands( "UNCOMPRESSED_LENGTH", 0, 0, 0, ""),
  Commands( "UNHEX", 0, 0, 0, ""),
  Commands( "UNIQUE_USERS", 0, 0, 0, ""),
  Commands( "UNIX_TIMESTAMP", 0, 0, 0, ""),
  Commands( "UPPER", 0, 0, 0, ""),
  Commands( "UUID", 0, 0, 0, ""),
  Commands( "VARIANCE", 0, 0, 0, ""),
  Commands( "VAR_POP", 0, 0, 0, ""),
  Commands( "VAR_SAMP", 0, 0, 0, ""),
  Commands( "VERSION", 0, 0, 0, ""),
  Commands( "WEEKDAY", 0, 0, 0, ""),
  Commands( "WEEKOFYEAR", 0, 0, 0, ""),
  Commands( "WITHIN", 0, 0, 0, ""),
  Commands( "X", 0, 0, 0, ""),
  Commands( "Y", 0, 0, 0, ""),
  Commands( "YEARWEEK", 0, 0, 0, ""),
  /* end sentinel */
  Commands((char *)NULL,       0, 0, 0, "")
};


int history_length;
static int not_in_history(const char *line);
static void initialize_readline (char *name);
static void fix_history(string *final_command);

static Commands *find_command(const char *name,char cmd_name);
static bool add_line(string *buffer,char *line,char *in_string,
                     bool *ml_comment);
static void remove_cntrl(string *buffer);
static void print_table_data(drizzle_result_st *result);
static void print_tab_data(drizzle_result_st *result);
static void print_table_data_vertically(drizzle_result_st *result);
static void print_warnings(uint32_t error_code);
static boost::posix_time::ptime start_timer(void);
static void end_timer(boost::posix_time::ptime, string &buff);
static void drizzle_end_timer(boost::posix_time::ptime, string &buff);
static void nice_time(boost::posix_time::time_duration duration, string &buff);
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
    printf(_("shutting down drizzled"));
    if (opt_drizzle_port > 0)
      printf(_(" on port %d"), opt_drizzle_port);
    printf("... ");
  }

  if (drizzle_shutdown(&con, &result, DRIZZLE_SHUTDOWN_DEFAULT,
                       &ret) == NULL || ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, _("shutdown failed; error: '%s'"),
              drizzle_result_error(&result));
      drizzle_result_free(&result);
    }
    else
    {
      fprintf(stderr, _("shutdown failed; error: '%s'"),
              drizzle_con_error(&con));
    }
    return false;
  }

  drizzle_result_free(&result);

  if (verbose)
    printf(_("done\n"));

  return true;
}

static bool kill_query(uint32_t query_id)
{
  drizzle_result_st result;
  drizzle_return_t ret;

  if (verbose)
  {
    printf(_("killing query %u"), query_id);
    printf("... ");
  }

  if (drizzle_kill(&con, &result, query_id,
                   &ret) == NULL || ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, _("kill failed; error: '%s'"),
              drizzle_result_error(&result));
      drizzle_result_free(&result);
    }
    else
    {
      fprintf(stderr, _("kill failed; error: '%s'"),
              drizzle_con_error(&con));
    }
    return false;
  }

  drizzle_result_free(&result);

  if (verbose)
    printf(_("done\n"));

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
      printf(_("drizzled is alive\n"));
  }
  else
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, _("ping failed; error: '%s'"),
              drizzle_result_error(&result));
      drizzle_result_free(&result);
    }
    else
    {
      fprintf(stderr, _("drizzled won't answer to ping, error: '%s'"),
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

  if (opt_kill)
  {
    if (kill_query(opt_kill) == false)
    {
      *error= 1;
    }
    executed= true;
  }

  return executed;
}

static void check_timeout_value(uint32_t in_connect_timeout)
{
  opt_connect_timeout= 0;
  if (in_connect_timeout > 3600*12)
  {
    cout << _("Error: Invalid Value for connect_timeout"); 
    exit(-1);
  }
  opt_connect_timeout= in_connect_timeout;
}

static void check_max_input_line(uint32_t in_max_input_line)
{
  opt_max_input_line= 0;
  if (in_max_input_line < 4096 || in_max_input_line > (int64_t)2*1024L*1024L*1024L)
  {
    cout << _("Error: Invalid Value for max_input_line");
    exit(-1);
  }
  opt_max_input_line= in_max_input_line - (in_max_input_line % 1024);
}

int main(int argc,char *argv[])
{
try
{

#if defined(ENABLE_NLS)
# if defined(HAVE_LOCALE_H)
  setlocale(LC_ALL, "");
# endif
  bindtextdomain("drizzle7", LOCALEDIR);
  textdomain("drizzle7");
#endif

  po::options_description commandline_options(_("Options used only in command line"));
  commandline_options.add_options()
  ("help,?",_("Displays this help and exit."))
  ("batch,B",_("Don't use history file. Disable interactive behavior. (Enables --silent)"))
  ("column-type-info", po::value<bool>(&column_types_flag)->default_value(false)->zero_tokens(),
  _("Display column type information."))
  ("comments,c", po::value<bool>(&preserve_comments)->default_value(false)->zero_tokens(),
  _("Preserve comments. Send comments to the server. The default is --skip-comments (discard comments), enable with --comments"))
  ("vertical,E", po::value<bool>(&vertical)->default_value(false)->zero_tokens(),
  _("Print the output of a query (rows) vertically."))
  ("force,f", po::value<bool>(&ignore_errors)->default_value(false)->zero_tokens(),
  _("Continue even if we get an sql error."))
  ("named-commands,G", po::value<bool>(&named_cmds)->default_value(false)->zero_tokens(),
  _("Enable named commands. Named commands mean this program's internal commands; see drizzle> help . When enabled, the named commands can be used from any line of the query, otherwise only from the first line, before an enter."))
  ("no-beep,b", po::value<bool>(&opt_nobeep)->default_value(false)->zero_tokens(),
  _("Turn off beep on error."))
  ("disable-line-numbers", _("Do not write line numbers for errors."))
  ("disable-column-names", _("Do not write column names in results."))
  ("skip-column-names,N", 
  _("Don't write column names in results. WARNING: -N is deprecated, use long version of this options instead."))
  ("set-variable,O", po::value<string>(),
  _("Change the value of a variable. Please note that this option is deprecated; you can set variables directly with --variable-name=value."))
  ("table,t", po::value<bool>(&output_tables)->default_value(false)->zero_tokens(),
  _("Output in table format.")) 
  ("safe-updates,U", po::value<bool>(&safe_updates)->default_value(false)->zero_tokens(),
  _("Only allow UPDATE and DELETE that uses keys."))
  ("i-am-a-dummy,U", po::value<bool>(&safe_updates)->default_value(false)->zero_tokens(),
  _("Synonym for option --safe-updates, -U."))
  ("verbose,v", po::value<string>(&opt_verbose)->default_value(""),
  _("-v vvv implies that verbose= 3, Used to specify verbose"))
  ("version,V", _("Output version information and exit."))
  ("secure-auth", po::value<bool>(&opt_secure_auth)->default_value(false)->zero_tokens(),
  _("Refuse client connecting to server if it uses old (pre-4.1.1) protocol"))
  ("show-warnings", po::value<bool>(&show_warnings)->default_value(false)->zero_tokens(),
  _("Show warnings after every statement."))
  ("show-progress-size", po::value<uint32_t>(&show_progress_size)->default_value(0),
  _("Number of lines before each import progress report."))
  ("ping", po::value<bool>(&opt_ping)->default_value(false)->zero_tokens(),
  _("Ping the server to check if it's alive."))
  ("no-defaults", po::value<bool>()->default_value(false)->zero_tokens(),
  _("Configuration file defaults are not used if no-defaults is set"))
  ;

  po::options_description drizzle_options(_("Options specific to the drizzle client"));
  drizzle_options.add_options()
  ("disable-auto-rehash,A",
  _("Disable automatic rehashing. One doesn't need to use 'rehash' to get table and field completion, but startup and reconnecting may take a longer time."))
  ("auto-vertical-output", po::value<bool>(&auto_vertical_output)->default_value(false)->zero_tokens(),
  _("Automatically switch to vertical output mode if the result is wider than the terminal width."))
  ("database,D", po::value<string>(&current_db)->default_value(""),
  _("Database to use."))
  ("default-character-set",po::value<string>(),
  _("(not used)"))
  ("delimiter", po::value<string>(&delimiter_str)->default_value(";"),
  _("Delimiter to be used."))
  ("execute,e", po::value<string>(),
  _("Execute command and quit. (Disables --force and history file)"))
  ("local-infile", po::value<bool>(&opt_local_infile)->default_value(false)->zero_tokens(),
  _("Enable LOAD DATA LOCAL INFILE."))
  ("unbuffered,n", po::value<bool>(&unbuffered)->default_value(false)->zero_tokens(),
  _("Flush buffer after each query."))
  ("sigint-ignore", po::value<bool>(&opt_sigint_ignore)->default_value(false)->zero_tokens(),
  _("Ignore SIGINT (CTRL-C)"))
  ("one-database,o", po::value<bool>(&one_database)->default_value(false)->zero_tokens(),
  _("Only update the default database. This is useful for skipping updates to other database in the update log."))
  ("pager", po::value<string>(),
  _("Pager to use to display results. If you don't supply an option the default pager is taken from your ENV variable PAGER. Valid pagers are less, more, cat [> filename], etc. See interactive help (\\h) also. This option does not work in batch mode. Disable with --disable-pager. This option is disabled by default."))
  ("disable-pager", po::value<bool>(&opt_nopager)->default_value(false)->zero_tokens(),
  _("Disable pager and print to stdout. See interactive help (\\h) also."))
  ("prompt", po::value<string>(&current_prompt)->default_value(""),  
  _("Set the drizzle prompt to this value."))
  ("quick,q", po::value<bool>(&quick)->default_value(false)->zero_tokens(),
  _("Don't cache result, print it row by row. This may slow down the server if the output is suspended. Doesn't use history file."))
  ("raw,r", po::value<bool>(&opt_raw_data)->default_value(false)->zero_tokens(),
  _("Write fields without conversion. Used with --batch.")) 
  ("disable-reconnect", _("Do not reconnect if the connection is lost."))
  ("shutdown", po::value<bool>()->zero_tokens(),
  _("Shutdown the server"))
  ("silent,s", _("Be more silent. Print results with a tab as separator, each row on new line."))
  ("kill", po::value<uint32_t>(&opt_kill)->default_value(0),
  _("Kill a running query."))
  ("tee", po::value<string>(),
  _("Append everything into outfile. See interactive help (\\h) also. Does not work in batch mode. Disable with --disable-tee. This option is disabled by default."))
  ("disable-tee", po::value<bool>()->default_value(false)->zero_tokens(), 
  _("Disable outfile. See interactive help (\\h) also."))
  ("connect-timeout", po::value<uint32_t>(&opt_connect_timeout)->default_value(0)->notifier(&check_timeout_value),
  _("Number of seconds before connection timeout."))
  ("max-input-line", po::value<uint32_t>(&opt_max_input_line)->default_value(16*1024L*1024L)->notifier(&check_max_input_line),
  _("Max length of input line"))
  ("select-limit", po::value<uint32_t>(&select_limit)->default_value(1000L),
  _("Automatic limit for SELECT when using --safe-updates"))
  ("max-join-size", po::value<uint32_t>(&max_join_size)->default_value(1000000L),
  _("Automatic limit for rows in a join when using --safe-updates"))
  ;

  po::options_description client_options(_("Options specific to the client"));
  client_options.add_options()
  ("host,h", po::value<string>(&current_host)->default_value("localhost"),
  _("Connect to host"))
  ("password,P", po::value<string>(&current_password)->default_value(PASSWORD_SENTINEL),
  _("Password to use when connecting to server. If password is not given it's asked from the tty."))
  ("port,p", po::value<uint32_t>()->default_value(0),
  _("Port number to use for connection or 0 for default to, in order of preference, drizzle.cnf, $DRIZZLE_TCP_PORT, built-in default"))
  ("user,u", po::value<string>(&current_user)->default_value(UserDetect().getUser()),
  _("User for login if not current user."))
  ("protocol",po::value<string>(&opt_protocol)->default_value("mysql"),
  _("The protocol of connection (mysql, mysql-plugin-auth, or drizzle)."))
  ;
  po::options_description long_options(_("Allowed Options"));
  long_options.add(commandline_options).add(drizzle_options).add(client_options);

  std::string system_config_dir_drizzle(SYSCONFDIR); 
  system_config_dir_drizzle.append("/drizzle/drizzle.cnf");

  std::string system_config_dir_client(SYSCONFDIR); 
  system_config_dir_client.append("/drizzle/client.cnf");

  std::string user_config_dir((getenv("XDG_CONFIG_HOME")? getenv("XDG_CONFIG_HOME"):"~/.config"));
 
  if (user_config_dir.compare(0, 2, "~/") == 0)
  {
    char *homedir;
    homedir= getenv("HOME");
    if (homedir != NULL)
      user_config_dir.replace(0, 1, homedir);
  }
 
  po::variables_map vm;

  po::positional_options_description p;
  p.add("database", 1);

  // Disable allow_guessing
  int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;

  po::store(po::command_line_parser(argc, argv).options(long_options).
            style(style).positional(p).extra_parser(parse_password_arg).run(),
            vm);

  if (! vm["no-defaults"].as<bool>())
  {
    std::string user_config_dir_drizzle(user_config_dir);
    user_config_dir_drizzle.append("/drizzle/drizzle.cnf"); 

    std::string user_config_dir_client(user_config_dir);
    user_config_dir_client.append("/drizzle/client.cnf");

    ifstream user_drizzle_ifs(user_config_dir_drizzle.c_str());
    po::store(dpo::parse_config_file(user_drizzle_ifs, drizzle_options), vm);

    ifstream user_client_ifs(user_config_dir_client.c_str());
    po::store(dpo::parse_config_file(user_client_ifs, client_options), vm);

    ifstream system_drizzle_ifs(system_config_dir_drizzle.c_str());
    store(dpo::parse_config_file(system_drizzle_ifs, drizzle_options), vm);
 
    ifstream system_client_ifs(system_config_dir_client.c_str());
    po::store(dpo::parse_config_file(system_client_ifs, client_options), vm);
  }

  po::notify(vm);

  default_prompt= strdup(getenv("DRIZZLE_PS1") ?
                         getenv("DRIZZLE_PS1") :
                         "drizzle> ");
  if (default_prompt == NULL)
  {
    fprintf(stderr, _("Memory allocation error while constructing initial "
                      "prompt. Aborting.\n"));
    exit(ENOMEM);
  }

  if (current_prompt.empty())
    current_prompt= strdup(default_prompt);

  if (current_prompt.empty())
  {
    fprintf(stderr, _("Memory allocation error while constructing initial "
                      "prompt. Aborting.\n"));
    exit(ENOMEM);
  }
  processed_prompt= new string();
  processed_prompt->reserve(32);

  prompt_counter=0;

  outfile.clear();      // no (default) outfile
  pager.assign("stdout");  // the default, if --pager wasn't given
  {
    const char *tmp= getenv("PAGER");
    if (tmp && strlen(tmp))
    {
      default_pager_set= 1;
      default_pager.assign(tmp);
    }
  }
  if (! isatty(0) || ! isatty(1))
  {
    status.setBatch(1); opt_silent=1;
  }
  else
    status.setAddToHistory(1);
  status.setExitStatus(1);

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

  /* Inverted Booleans */

  line_numbers= not vm.count("disable-line-numbers");
  column_names= not vm.count("disable-column-names");
  opt_rehash= not vm.count("disable-auto-rehash");
  opt_reconnect= not vm.count("disable-reconnect");

  /* Don't rehash with --shutdown */
  if (vm.count("shutdown"))
  {
    opt_rehash= false;
    opt_shutdown= true;
  }

  if (vm.count("delimiter"))
  {
    /* Check that delimiter does not contain a backslash */
    if (! strstr(delimiter_str.c_str(), "\\"))
    {
      delimiter= (char *)delimiter_str.c_str();  
    }
    else
    {
      put_info(_("DELIMITER cannot contain a backslash character"),
      INFO_ERROR,0,0);
      exit(-1);
    }
   
    delimiter_length= (uint32_t)strlen(delimiter);
  }
  if (vm.count("tee"))
  { 
    if (vm["tee"].as<string>().empty())
    {
      if (opt_outfile)
        end_tee();
    }
    else
      init_tee(vm["tee"].as<string>().c_str());
  }
  if (vm["disable-tee"].as<bool>())
  {
    if (opt_outfile)
      end_tee();
  }
  if (vm.count("pager"))
  {
    if (vm["pager"].as<string>().empty())
      opt_nopager= 1;
    else
    {
      opt_nopager= 0;
      if (vm[pager].as<string>().length())
      {
        default_pager_set= 1;
        pager.assign(vm["pager"].as<string>());
        default_pager.assign(pager);
      }
      else if (default_pager_set)
        pager.assign(default_pager);
      else
        opt_nopager= 1;
    }
  }
  if (vm.count("disable-pager"))
  {
    opt_nopager= 1;
  }

  if (vm.count("no-auto-rehash"))
    opt_rehash= 0;

  if (vm.count("skip-column-names"))
    column_names= 0;
    
  if (vm.count("execute"))
  {  
    status.setBatch(1);
    status.setAddToHistory(1);
    if (status.getLineBuff() == NULL)
      status.setLineBuff(opt_max_input_line,NULL);
    if (status.getLineBuff() == NULL)
    {
      exit(1);
    }
    status.getLineBuff()->addString(vm["execute"].as<string>().c_str());
  }

  if (one_database)
    skip_updates= true;

  if (vm.count("protocol"))
  {
    std::transform(opt_protocol.begin(), opt_protocol.end(), 
      opt_protocol.begin(), ::tolower);

    if (not opt_protocol.compare("mysql"))
    {

      global_con_options= (drizzle_con_options_t)(DRIZZLE_CON_MYSQL|DRIZZLE_CON_INTERACTIVE);
      use_drizzle_protocol= false;
    }
    else if (not opt_protocol.compare("mysql-plugin-auth"))
    {
      global_con_options= (drizzle_con_options_t)(DRIZZLE_CON_MYSQL|DRIZZLE_CON_INTERACTIVE|DRIZZLE_CON_AUTH_PLUGIN);
      use_drizzle_protocol= false;
    }
    else if (not opt_protocol.compare("drizzle"))
    {
      global_con_options= (drizzle_con_options_t)(DRIZZLE_CON_EXPERIMENTAL);
      use_drizzle_protocol= true;
    }
    else
    {
      cout << _("Error: Unknown protocol") << " '" << opt_protocol << "'" << endl;
      exit(-1);
    }
  }
 
  if (vm.count("port"))
  {
    opt_drizzle_port= vm["port"].as<uint32_t>();

    /* If the port number is > 65535 it is not a valid port
       This also helps with potential data loss casting unsigned long to a
       uint32_t. */
    if (opt_drizzle_port > 65535)
    {
      printf(_("Error: Value of %" PRIu32 " supplied for port is not valid.\n"), opt_drizzle_port);
      exit(-1);
    }
  }

  if (vm.count("password"))
  {
    if (!opt_password.empty())
      opt_password.erase();
    if (current_password == PASSWORD_SENTINEL)
    {
      opt_password= "";
    }
    else
    {
      opt_password= current_password;
      tty_password= false;
    }
  }
  else
  {
      tty_password= true;
  }
  

  if (!opt_verbose.empty())
  {
    verbose= opt_verbose.length();
  }

  if (vm.count("batch"))
  {
    status.setBatch(1);
    status.setAddToHistory(0);
    if (opt_silent < 1)
    {
      opt_silent= 1;
    }
  }
  if (vm.count("silent"))
  {
    opt_silent= 2;
  }
  
  if (vm.count("help") || vm.count("version"))
  {
    printf(_("Drizzle client %s build %s, for %s-%s (%s) using readline %s\n"),
           drizzle_version(), VERSION,
           HOST_VENDOR, HOST_OS, HOST_CPU,
           rl_library_version);
    if (vm.count("version"))
      exit(0);
    printf(_("Copyright (C) 2008 Sun Microsystems\n"
           "This software comes with ABSOLUTELY NO WARRANTY. "
           "This is free software,\n"
           "and you are welcome to modify and redistribute it "
           "under the GPL license\n"));
    printf(_("Usage: drizzle [OPTIONS] [schema]\n"));
    cout << long_options;
    exit(0);
  }
 

  if (process_options())
  {
    exit(1);
  }

  memset(&drizzle, 0, sizeof(drizzle));
  if (sql_connect(current_host, current_db, current_user, opt_password))
  {
    quick= 1;          // Avoid history
    status.setExitStatus(1);
    drizzle_end(-1);
  }

  int command_error;
  if (execute_commands(&command_error) != false)
  {
    /* we've executed a command so exit before we go into readline mode */
    exit(command_error);
  }

  if (status.getBatch() && !status.getLineBuff())
  {
    status.setLineBuff(opt_max_input_line, stdin);
    if (status.getLineBuff() == NULL)
    {
      exit(1);
    }
  }

  if (!status.getBatch())
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
  std::vector<char> output_buff;
  output_buff.resize(512);

  snprintf(&output_buff[0], output_buff.size(), 
           _("Welcome to the Drizzle client..  Commands end with %s or \\g."), 
           delimiter);

  put_info(&output_buff[0], INFO_INFO, 0, 0);

  glob_buffer= new string();
  glob_buffer->reserve(512);

  snprintf(&output_buff[0], output_buff.size(),
          _("Your Drizzle connection id is %u\nConnection protocol: %s\nServer version: %s\n"),
          drizzle_con_thread_id(&con),
          opt_protocol.c_str(),
          server_version_string(&con));
  put_info(&output_buff[0], INFO_INFO, 0, 0);


  initialize_readline((char *)current_prompt.c_str());
  if (!status.getBatch() && !quick)
  {
    /* read-history from file, default ~/.drizzle_history*/
    if (getenv("DRIZZLE_HISTFILE"))
      histfile= strdup(getenv("DRIZZLE_HISTFILE"));
    else if (getenv("HOME"))
    {
      histfile=(char*) malloc(strlen(getenv("HOME")) + strlen("/.drizzle_history") + 2);
      if (histfile)
        sprintf(histfile,"%s/.drizzle_history",getenv("HOME"));
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
      histfile_tmp= (char*) malloc((uint32_t) strlen(histfile) + 5);
      sprintf(histfile_tmp, "%s.TMP", histfile);
    }
  }

  put_info(_("Type 'help;' or '\\h' for help. "
             "Type '\\c' to clear the buffer.\n"),INFO_INFO,0,0);
  status.setExitStatus(read_and_execute(!status.getBatch()));
  if (opt_outfile)
    end_tee();
  drizzle_end(0);
}

  catch(exception &err)
  {
    cerr << _("Error:") << err.what() << endl;
  }
  return(0);        // Keep compiler happy
}

void drizzle_end(int sig)
{
  drizzle_con_free(&con);
  drizzle_free(&drizzle);
  if (!status.getBatch() && !quick && histfile)
  {
    /* write-history */
    if (verbose)
      tee_fprintf(stdout, _("Writing history-file %s\n"),histfile);
    if (!write_history(histfile_tmp))
      rename(histfile_tmp, histfile);
  }
  delete status.getLineBuff();
  status.setLineBuff(0);

  if (sig >= 0)
    put_info(sig ? _("Aborted") : _("Bye"), INFO_RESULT,0,0);
  delete glob_buffer;
  delete processed_prompt;
  opt_password.erase();
  free(histfile);
  free(histfile_tmp);
  current_db.erase();
  current_host.erase();
  current_user.erase();
  free(full_username);
  free(part_username);
  free(default_prompt);
  current_prompt.erase();
  exit(status.getExitStatus());
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
  boost::scoped_ptr<drizzle_con_st> kill_drizzle(new drizzle_con_st);
  drizzle_result_st res;
  drizzle_return_t ret;

  /* terminate if no query being executed, or we already tried interrupting */
  if (!executing_query || interrupted_query)
  {
    goto err;
  }

  if (drizzle_con_add_tcp(&drizzle, kill_drizzle.get(), current_host.c_str(),
    opt_drizzle_port, current_user.c_str(), opt_password.c_str(), NULL,
    use_drizzle_protocol ? DRIZZLE_CON_EXPERIMENTAL : DRIZZLE_CON_MYSQL) == NULL)
  {
    goto err;
  }

  /* kill_buffer is always big enough because max length of %lu is 15 */
  sprintf(kill_buffer, "KILL /*!50000 QUERY */ %u",
          drizzle_con_thread_id(&con));

  if (drizzle_query_str(kill_drizzle.get(), &res, kill_buffer, &ret) != NULL)
    drizzle_result_free(&res);

  drizzle_con_free(kill_drizzle.get());
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



static int process_options(void)
{
  char *tmp, *pagpoint;
  

  tmp= (char *) getenv("DRIZZLE_HOST");
  if (tmp)
    current_host.assign(tmp);

  pagpoint= getenv("PAGER");
  if (!((char*) (pagpoint)))
  {
    pager.assign("stdout");
    opt_nopager= 1;
  }
  else
  {
    pager.assign(pagpoint);
  }
  default_pager.assign(pager);

  //

  if (status.getBatch()) /* disable pager and outfile in this case */
  {
    default_pager.assign("stdout");
    pager.assign("stdout");
    opt_nopager= 1;
    default_pager_set= 0;
    opt_outfile= 0;
    opt_reconnect= 0;
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
  Commands *com;
  status.setExitStatus(1);

  for (;;)
  {
    if (!interactive)
    {
      if (status.getLineBuff())
        line= status.getLineBuff()->readline();
      else
        line= 0;

      line_number++;
      if (show_progress_size > 0)
      {
        if ((line_number % show_progress_size) == 0)
          fprintf(stderr, _("Processing line: %"PRIu32"\n"), line_number);
      }
      if (!glob_buffer->empty())
        status.setQueryStartLine(line_number);
    }
    else
    {
      string prompt(ml_comment
                      ? "   /*> " 
                      : glob_buffer->empty()
                        ? construct_prompt()
                        : not in_string
                          ? "    -> "
                          : in_string == '\''
                            ? "    '> "
                            : in_string == '`'
                              ? "    `> "
                              : "    \"> ");
      if (opt_outfile && glob_buffer->empty())
        fflush(OUTFILE);

      if (opt_outfile)
        fputs(prompt.c_str(), OUTFILE);
      line= readline(prompt.c_str());
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
      status.setExitStatus(0);
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
      if (interactive && status.getAddToHistory() && not_in_history(line))
        add_history(line);
      continue;
    }
    if (add_line(glob_buffer,line,&in_string,&ml_comment))
      break;
  }
  /* if in batch mode, send last query even if it doesn't end with \g or go */

  if (!interactive && !status.getExitStatus())
  {
    remove_cntrl(glob_buffer);
    if (!glob_buffer->empty())
    {
      status.setExitStatus(1);
      if (com_go(glob_buffer,line) <= 0)
        status.setExitStatus(0);
    }
  }

  return status.getExitStatus();
}


static Commands *find_command(const char *name,char cmd_char)
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
    while (isspace(*name))
      name++;
    /*
      If there is an \\g in the row or if the row has a delimiter but
      this is not a delimiter command, let add_line() take care of
      parsing the row and calling find_command()
    */
    if (strstr(name, "\\g") || (strstr(name, delimiter) &&
                                !(strlen(name) >= 9 &&
                                  !strcmp(name, "delimiter"))))
      return(NULL);
    if ((end=strcont(name," \t")))
    {
      len=(uint32_t) (end - name);
      while (isspace(*end))
        end++;
      if (!*end)
        end=0;          // no arguments to function
    }
    else
      len=(uint32_t) strlen(name);
  }

  for (uint32_t i= 0; commands[i].getName(); i++)
  {
    if (commands[i].func &&
        ((name && !strncmp(name, commands[i].getName(), len)
          && !commands[i].getName()[len] && (!end || (end && commands[i].getTakesParams()))) || (!name && commands[i].getCmdChar() == cmd_char)))
    {
      return(&commands[i]);
    }
  }
  return(NULL);
}


static bool add_line(string *buffer, char *line, char *in_string,
                        bool *ml_comment)
{
  unsigned char inchar;
  char *pos, *out;
  Commands *com;
  bool need_space= 0;
  bool ss_comment= 0;


  if (!line[0] && (buffer->empty()))
    return(0);
  if (status.getAddToHistory() && line[0] && not_in_history(line))
    add_history(line);

  for (pos=out=line ; (inchar= (unsigned char) *pos) ; pos++)
  {
    if (!preserve_comments)
    {
      // Skip spaces at the beggining of a statement
      if (isspace(inchar) && (out == line) &&
          (buffer->empty()))
        continue;
    }

    // Accept multi-byte characters as-is
    if (not drizzled::utf8::is_single(*pos))
    {
      int length;
      if ((length= drizzled::utf8::sequence_length(*pos)))
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
        if (com->getTakesParams())
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
    else if (!*ml_comment && !*in_string && !strncmp(pos, delimiter,
                                                     strlen(delimiter)))
    {
      // Found a statement. Continue parsing after the delimiter
      pos+= delimiter_length;

      if (preserve_comments)
      {
        while (isspace(*pos))
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
                                 isspace(pos[2]))))
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
                         && isspace(pos[2])))))
    {
      // Flush previously accepted characters
      if (out != line)
      {
        buffer->append(line, (out - line));
        out= line;
      }

      // comment to end of line
      if (preserve_comments)
      {
        bool started_with_nothing= !buffer->empty();
        buffer->append(pos);

        /*
          A single-line comment by itself gets sent immediately so that
          client commands (delimiter, status, etc) will be interpreted on
          the next line.
        */
        if (started_with_nothing)
        {
          if (com_go(buffer, 0) > 0)             // < 0 is not fatal
           return 1;
          buffer->clear();
        }
      }  

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
        if (need_space && !isspace((char)inchar))
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
    if ((buffer->length() + length) > opt_max_input_line)
    {
      status.setExitStatus(1);
      put_info(_("Not found a delimiter within max_input_line of input"), INFO_ERROR, 0, 0);
      return 1;
    }
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
  if (!status.getBatch() && !quick)
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
  Commands *cmd=commands;
  drizzle_return_t ret;
  drizzle_result_st databases,tables,fields;
  drizzle_row_t database_row,table_row;
  string tmp_str, tmp_str_lower;
  std::string query;

  if (status.getBatch() || quick || current_db.empty())
    return;      // We don't need completion in batches
  if (!rehash)
    return;

  completion_map.clear();

  /* hash this file's known subset of SQL commands */
  while (cmd->getName()) {
    tmp_str= cmd->getName();
    tmp_str_lower= lower_string(tmp_str);
    completion_map[tmp_str_lower]= tmp_str;
    cmd++;
  }

  /* hash Drizzle functions (to be implemented) */

  /* hash all database names */
  if (drizzle_query_str(&con, &databases, "select schema_name from information_schema.schemata", &ret) != NULL)
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

  query= "select table_name, column_name from information_schema.columns where table_schema='";
  query.append(current_db);
  query.append("' order by table_name");
  
  if (drizzle_query(&con, &fields, query.c_str(), query.length(),
                    &ret) != NULL)
  {
    if (ret == DRIZZLE_RETURN_OK &&
        drizzle_result_buffer(&fields) == DRIZZLE_RETURN_OK)
    {
      if (drizzle_result_row_count(&tables) > 0 && !opt_silent && write_info)
      {
        tee_fprintf(stdout,
                    _("Reading table information for completion of "
                      "table and column names\n"
                      "You can turn off this feature to get a quicker "
                      "startup with -A\n\n"));
      }

      std::string table_name;
      while ((table_row=drizzle_row_next(&fields)))
      {
        if (table_name.compare(table_row[0]) != 0)
        {
          tmp_str= table_row[0];
          tmp_str_lower= lower_string(tmp_str);
          completion_map[tmp_str_lower]= tmp_str;
          table_name= table_row[0];
        }
        tmp_str= table_row[0];
        tmp_str.append(".");
        tmp_str.append(table_row[1]);
        tmp_str_lower= lower_string(tmp_str);
        completion_map[tmp_str_lower]= tmp_str;

        tmp_str= table_row[1];
        tmp_str_lower= lower_string(tmp_str);
        completion_map[tmp_str_lower]= tmp_str;
      }
    }
  }
  drizzle_result_free(&fields);
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

  current_db.erase();
  current_db= "";
  /* In case of error below current_db will be NULL */
  if (drizzle_query_str(&con, &res, "SELECT DATABASE()", &ret) != NULL)
  {
    if (ret == DRIZZLE_RETURN_OK &&
        drizzle_result_buffer(&res) == DRIZZLE_RETURN_OK)
    {
      drizzle_row_t row= drizzle_row_next(&res);
      if (row[0])
        current_db.assign(row[0]);
      drizzle_result_free(&res);
    }
  }
}

/***************************************************************************
 The different commands
***************************************************************************/

int drizzleclient_real_query_for_lazy(const char *buf, size_t length,
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
  {
    int ret= put_error(&con, result);
    drizzle_result_free(result);
    return ret;
  }
  return 0;
}

static int
com_help(string *buffer, const char *)
{
  int i, j;
  char buff[32], *end;
  std::vector<char> output_buff;
  output_buff.resize(512);

  put_info(_("List of all Drizzle commands:"), INFO_INFO,0,0);
  if (!named_cmds)
  {
    snprintf(&output_buff[0], output_buff.size(),
             _("Note that all text commands must be first on line and end with '%s' or \\g"),
             delimiter);
    put_info(&output_buff[0], INFO_INFO, 0, 0);
  }
  for (i = 0; commands[i].getName(); i++)
  {
    end= strcpy(buff, commands[i].getName());
    end+= strlen(commands[i].getName());
    for (j= (int)strlen(commands[i].getName()); j < 10; j++)
      end= strcpy(end, " ")+1;
    if (commands[i].func)
      tee_fprintf(stdout, "%s(\\%c) %s\n", buff,
                  commands[i].getCmdChar(), _(commands[i].getDoc()));
  }
  tee_fprintf(stdout, "\n");
  buffer->clear();
  return 0;
}


static int
com_clear(string *buffer, const char *)
{
  if (status.getAddToHistory())
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
  drizzle_result_st result;
  drizzle_return_t ret;
  uint32_t      warnings= 0;
  boost::posix_time::ptime timer;
  uint32_t      error= 0;
  uint32_t      error_code= 0;
  int           err= 0;

  interrupted_query= 0;

  /* Remove garbage for nicer messages */
  remove_cntrl(buffer);

  if (buffer->empty())
  {
    // Ignore empty quries
    if (status.getBatch())
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

  if (status.getAddToHistory())
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

    string time_buff("");
    if (verbose >= 3 || !opt_silent)
      drizzle_end_timer(timer,time_buff);

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
        sprintf(buff,
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
      sprintf(buff, ngettext("Query OK, %ld row affected",
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
      sprintf(warnings_buff, "%d", warnings);
      strcpy(pos, warnings_buff);
      pos+= strlen(warnings_buff);
      pos= strcpy(pos, " warning")+8;
      if (warnings != 1)
        *pos++= 's';
    }
    strcpy(pos, time_buff.c_str());
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

  if (!error && !status.getBatch() &&
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
    if (!(PAGER= popen(pager.c_str(), "w")))
    {
      tee_fprintf(stdout,_( "popen() failed! defaulting PAGER to stdout!\n"));
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
    tee_fprintf(stdout, _("Error logging to file '%s'\n"), file_name);
    return;
  }
  OUTFILE = new_outfile;
  outfile.assign(file_name);
  tee_fprintf(stdout, _("Logging to file '%s'\n"), file_name);
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
    sprintf(s, " unknows=0x%04x", f);
  return buf;
}

static void
print_field_types(drizzle_result_st *result)
{
  drizzle_column_st   *field;
  uint32_t i=0;

  while ((field = drizzle_column_next(result)))
  {
    tee_fprintf(PAGER, _("Field %3u:  `%s`\n"
                "Catalog:    `%s`\n"
                "Schema:     `%s`\n"
                "Table:      `%s`\n"
                "Org_table:  `%s`\n"
                "Type:       UTF-8\n"
                "Collation:  %s (%u)\n"
                "Length:     %lu\n"
                "Max_length: %lu\n"
                "Decimals:   %u\n"
                "Flags:      %s\n\n"),
                ++i,
                drizzle_column_name(field), drizzle_column_catalog(field),
                drizzle_column_db(field), drizzle_column_table(field),
                drizzle_column_orig_table(field),
                fieldtype2str(drizzle_column_type(field)),
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
  std::vector<bool> num_flag;
  std::vector<bool> boolean_flag;
  std::vector<bool> ansi_boolean_flag;
  string separator;

  separator.reserve(256);

  num_flag.resize(drizzle_result_column_count(result));
  boolean_flag.resize(drizzle_result_column_count(result));
  ansi_boolean_flag.resize(drizzle_result_column_count(result));
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
      length= drizzled::utf8::char_length(drizzle_column_name(field));

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
    if ((length < 5) and 
      (server_type == ServerDetect::SERVER_DRIZZLE_FOUND) and
      (drizzle_column_type(field) == DRIZZLE_COLUMN_TYPE_TINY) and
      (drizzle_column_type(field) & DRIZZLE_COLUMN_FLAGS_UNSIGNED))
    {
      // Room for "FALSE"
      length= 5;
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
      uint32_t numcells= drizzled::utf8::char_length(drizzle_column_name(field));
      uint32_t display_length= drizzle_column_max_size(field) + name_length -
                               numcells;
      tee_fprintf(PAGER, " %-*s |",(int) min(display_length,
                                             MAX_COLUMN_LENGTH),
                  drizzle_column_name(field));
      num_flag[off]= ((drizzle_column_type(field) <= DRIZZLE_COLUMN_TYPE_LONGLONG) ||
                      (drizzle_column_type(field) == DRIZZLE_COLUMN_TYPE_NEWDECIMAL));
      if ((server_type == ServerDetect::SERVER_DRIZZLE_FOUND) and
        (drizzle_column_type(field) == DRIZZLE_COLUMN_TYPE_TINY))
      {
        if ((drizzle_column_flags(field) & DRIZZLE_COLUMN_FLAGS_UNSIGNED))
        {
          ansi_boolean_flag[off]= true;
        }
        else
        {
          ansi_boolean_flag[off]= false;
        }
        boolean_flag[off]= true;
        num_flag[off]= false;
      }
      else
      {
        boolean_flag[off]= false;
      }
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
      else if (boolean_flag[off])
      {
        if (strncmp(cur[off],"1", 1) == 0)
        {
          if (ansi_boolean_flag[off])
          {
            buffer= "YES";
            data_length= 3;
          }
          else
          {
            buffer= "TRUE";
            data_length= 4;
          }
        }
        else
        {
          if (ansi_boolean_flag[off])
          {
            buffer= "NO";
            data_length= 2;
          }
          else
          {
            buffer= "FALSE";
            data_length= 5;
          }
        }
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
      visible_length= drizzled::utf8::char_length(buffer);
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
  FILE *out;

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
  if (status.getBatch()) 
  {
    out= stderr;
  } 
  else 
  {
    init_pager();
    out= PAGER;
  }
  do
  {
    tee_fprintf(out, "%s (Code %s): %s\n", cur[0], cur[1], cur[2]);
  } while ((cur= drizzle_row_next(&result)));

  if (not status.getBatch())
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
      if ((l = drizzled::utf8::sequence_length(*pos)))
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
  std::vector<bool> boolean_flag;
  std::vector<bool> ansi_boolean_flag;

  boolean_flag.resize(drizzle_result_column_count(result));
  ansi_boolean_flag.resize(drizzle_result_column_count(result));

  int first=0;
  for (uint32_t off= 0; (field = drizzle_column_next(result)); off++)
  {
    if (opt_silent < 2 && column_names)
    {
      if (first++)
        (void) tee_fputs("\t", PAGER);
      (void) tee_fputs(drizzle_column_name(field), PAGER);
    }
    if ((server_type == ServerDetect::SERVER_DRIZZLE_FOUND) and
      (drizzle_column_type(field) == DRIZZLE_COLUMN_TYPE_TINY))
    {
      if ((drizzle_column_flags(field) & DRIZZLE_COLUMN_FLAGS_UNSIGNED))
      {
        ansi_boolean_flag[off]= true;
      }
      else
      {
        ansi_boolean_flag[off]= false;
      }
      boolean_flag[off]= true;
    }
    else
    {
      boolean_flag[off]= false;
    }
  }
  if (opt_silent < 2 && column_names)
  {
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
    drizzle_column_seek(result, 0);
    for (uint32_t off=0 ; off < drizzle_result_column_count(result); off++)
    {
      if (off != 0)
        (void) tee_fputs("\t", PAGER);
      if (boolean_flag[off])
      {
        if (strncmp(cur[off],"1", 1) == 0)
        {
          if (ansi_boolean_flag[off])
          {
            safe_put_field("YES", 3);
          }
          else
          {
            safe_put_field("TRUE", 4);
          }
        }
        else
        {
          if (ansi_boolean_flag[off])
          {
            safe_put_field("NO", 2);
          }
          else
          {
            safe_put_field("FALSE", 5);
          }
        }
      }
      else
      {
        safe_put_field(cur[off], lengths[off]);
      }
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

  if (status.getBatch())
    return 0;
  while (isspace(*line))
    line++;
  if (!(param =strchr(line, ' '))) // if outfile wasn't given, use the default
  {
    if (outfile.empty())
    {
      printf(_("No previous outfile available, you must give a filename!\n"));
      return 0;
    }
    else if (opt_outfile)
    {
      tee_fprintf(stdout, _("Currently logging to file '%s'\n"), outfile.c_str());
      return 0;
    }
    else
      param= outfile.c_str();      //resume using the old outfile
  }

  /* @TODO: Replace this with string methods */
  /* eliminate the spaces before the parameters */
  while (isspace(*param))
    param++;
  strncpy(file_name, param, sizeof(file_name) - 1);
  end= file_name + strlen(file_name);
  /* remove end space from command line */
  while (end > file_name && (isspace(end[-1]) ||
                             iscntrl(end[-1])))
    end--;
  end[0]= 0;
  if (end == file_name)
  {
    printf(_("No outfile specified!\n"));
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
  tee_fprintf(stdout, _("Outfile disabled.\n"));
  return 0;
}

/*
  Sorry, this command is not available in Windows.
*/

static int
com_pager(string *, const char *line)
{
  const char *param;

  if (status.getBatch())
    return 0;
  /* Skip spaces in front of the pager command */
  while (isspace(*line))
    line++;
  /* Skip the pager command */
  param= strchr(line, ' ');
  /* Skip the spaces between the command and the argument */
  while (param && isspace(*param))
    param++;
  if (!param || (*param == '\0')) // if pager was not given, use the default
  {
    if (!default_pager_set)
    {
      tee_fprintf(stdout, _("Default pager wasn't set, using stdout.\n"));
      opt_nopager=1;
      pager.assign("stdout");
      PAGER= stdout;
      return 0;
    }
    pager.assign(default_pager);
  }
  else
  {
    string pager_name(param);
    string::iterator end= pager_name.end();
    while (end > pager_name.begin() &&
           (isspace(*(end-1)) || iscntrl(*(end-1))))
      --end;
    pager_name.erase(end, pager_name.end());
    pager.assign(pager_name);
    default_pager.assign(pager_name);
  }
  opt_nopager=0;
  tee_fprintf(stdout, _("PAGER set to '%s'\n"), pager.c_str());
  return 0;
}


static int
com_nopager(string *, const char *)
{
  pager.assign("stdout");
  opt_nopager=1;
  PAGER= stdout;
  tee_fprintf(stdout, _("PAGER set to stdout\n"));
  return 0;
}

/* If arg is given, exit without errors. This happens on command 'quit' */

static int
com_quit(string *, const char *)
{
  /* let the screen auto close on a normal shutdown */
  status.setExitStatus(0);
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
      current_db.erase();
      current_db.assign(tmp);
      tmp= get_arg(buff, 1);
      if (tmp)
      {
        current_host.erase();
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
  error=sql_connect(current_host, current_db, current_user, opt_password);
  opt_rehash= save_rehash;

  if (connected)
  {
    sprintf(buff, _("Connection id:    %u"), drizzle_con_thread_id(&con));
    put_info(buff,INFO_INFO,0,0);
    sprintf(buff, _("Current schema: %.128s\n"),
            !current_db.empty() ? current_db.c_str() : _("*** NONE ***"));
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
  Status old_status;
  FILE *sql_file;

  /* Skip space from file name */
  while (isspace(*line))
    line++;
  if (!(param = strchr(line, ' ')))    // Skip command name
    return put_info(_("Usage: \\. <filename> | source <filename>"),
                    INFO_ERROR, 0,0);
  while (isspace(*param))
    param++;
  end= strncpy(source_name,param,sizeof(source_name)-1);
  end+= strlen(source_name);
  while (end > source_name && (isspace(end[-1]) ||
                               iscntrl(end[-1])))
    end--;
  end[0]=0;

  /* open file name */
  if (!(sql_file = fopen(source_name, "r")))
  {
    char buff[FN_REFLEN+60];
    sprintf(buff, _("Failed to open file '%s', error: %d"), source_name,errno);
    return put_info(buff, INFO_ERROR, 0 ,0);
  }

  line_buff= new LineBuffer(opt_max_input_line,sql_file);

  /* Save old status */
  old_status=status;
  memset(&status, 0, sizeof(status));

  // Run in batch mode
  status.setBatch(old_status.getBatch());
  status.setLineBuff(line_buff);
  status.setFileName(source_name);
  // Empty command buffer
  assert(glob_buffer!=NULL);
  glob_buffer->clear();
  error= read_and_execute(false);
  // Continue as before
  status=old_status;
  fclose(sql_file);
  delete status.getLineBuff();
  line_buff=0;
  status.setLineBuff(0);
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
    put_info(_("DELIMITER must be followed by a 'delimiter' character or string"),
             INFO_ERROR, 0, 0);
    return 0;
  }
  else
  {
    if (strstr(tmp, "\\"))
    {
      put_info(_("DELIMITER cannot contain a backslash character"),
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
    put_info(_("USE must be followed by a schema name"), INFO_ERROR, 0, 0);
    return 0;
  }
  /*
    We need to recheck the current database, because it may change
    under our feet, for example if DROP DATABASE or RENAME DATABASE
    (latter one not yet available by the time the comment was written)
  */
  get_current_db();

  if (current_db.empty() || strcmp(current_db.c_str(),tmp))
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
    current_db.erase();
    current_db.assign(tmp);
    if (select_db > 1)
      build_completion_hash(opt_rehash, 1);
  }

  put_info(_("Schema changed"),INFO_INFO, 0, 0);
  return 0;
}

static int com_shutdown(string *, const char *)
{
  drizzle_result_st result;
  drizzle_return_t ret;

  if (verbose)
  {
    printf(_("shutting down drizzled"));
    if (opt_drizzle_port > 0)
      printf(_(" on port %d"), opt_drizzle_port);
    printf("... ");
  }

  if (drizzle_shutdown(&con, &result, DRIZZLE_SHUTDOWN_DEFAULT,
                       &ret) == NULL || ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, _("shutdown failed; error: '%s'"),
              drizzle_result_error(&result));
      drizzle_result_free(&result);
    }
    else
    {
      fprintf(stderr, _("shutdown failed; error: '%s'"),
              drizzle_con_error(&con));
    }
    return false;
  }

  drizzle_result_free(&result);

  if (verbose)
    printf(_("done\n"));

  return false;
}

static int
com_warnings(string *, const char *)
{
  show_warnings = 1;
  put_info(_("Show warnings enabled."),INFO_INFO, 0, 0);
  return 0;
}

static int
com_nowarnings(string *, const char *)
{
  show_warnings = 0;
  put_info(_("Show warnings disabled."),INFO_INFO, 0, 0);
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
    while (isspace(*ptr))
      ptr++;
    if (*ptr == '\\') // short command was used
      ptr+= 2;
    else
      while (*ptr &&!isspace(*ptr)) // skip command
        ptr++;
  }
  if (!*ptr)
    return NULL;
  while (isspace(*ptr))
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
sql_connect(const string &host, const string &database, const string &user, const string &password)
{
  drizzle_return_t ret;
  if (connected)
  {
    connected= 0;
    drizzle_con_free(&con);
    drizzle_free(&drizzle);
  }
  drizzle_create(&drizzle);

  if (drizzle_con_add_tcp(&drizzle, &con, (char *)host.c_str(),
                          opt_drizzle_port, (char *)user.c_str(),
                          (char *)password.c_str(), (char *)database.c_str(),
                          global_con_options) == NULL)
  {
    (void) put_error(&con, NULL);
    (void) fflush(stdout);
    return 1;
  }

  if ((ret= drizzle_con_connect(&con)) != DRIZZLE_RETURN_OK)
  {

    if (opt_silent < 2)
    {
      (void) put_error(&con, NULL);
      (void) fflush(stdout);
      return ignore_errors ? -1 : 1;    // Abort
    }
    return -1;          // Retryable
  }
  connected= 1;

  ServerDetect server_detect(&con);
  server_type= server_detect.getServerType();

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
  printf(_("Drizzle client %s build %s, for %s-%s (%s) using readline %s\n"),
         drizzle_version(), VERSION,
         HOST_VENDOR, HOST_OS, HOST_CPU,
         rl_library_version);

  if (connected)
  {
    tee_fprintf(stdout, _("\nConnection id:\t\t%lu\n"),drizzle_con_thread_id(&con));
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
        tee_fprintf(stdout, _("Current schema:\t%s\n"), cur[0] ? cur[0] : "");
        tee_fprintf(stdout, _("Current user:\t\t%s\n"), cur[1]);
      }
      drizzle_result_free(&result);
    }
    else if (ret == DRIZZLE_RETURN_ERROR_CODE)
      drizzle_result_free(&result);
    tee_puts(_("SSL:\t\t\tNot in use"), stdout);
  }
  else
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, _("\nNo connection\n"));
    vidattr(A_NORMAL);
    return 0;
  }
  if (skip_updates)
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, _("\nAll updates ignored to this schema\n"));
    vidattr(A_NORMAL);
  }
  tee_fprintf(stdout, _("Current pager:\t\t%s\n"), pager.c_str());
  tee_fprintf(stdout, _("Using outfile:\t\t'%s'\n"), opt_outfile ? outfile.c_str() : "");
  tee_fprintf(stdout, _("Using delimiter:\t%s\n"), delimiter);
  tee_fprintf(stdout, _("Server version:\t\t%s\n"), server_version_string(&con));
  tee_fprintf(stdout, _("Protocol:\t\t%s\n"), opt_protocol.c_str());
  tee_fprintf(stdout, _("Protocol version:\t%d\n"), drizzle_con_protocol_version(&con));
  tee_fprintf(stdout, _("Connection:\t\t%s\n"), drizzle_con_host(&con));
/* XXX need to save this from result
  if ((id= drizzleclient_insert_id(&drizzle)))
    tee_fprintf(stdout, "Insert id:\t\t%s\n", internal::llstr(id, buff));
*/

  if (drizzle_con_uds(&con))
    tee_fprintf(stdout, _("UNIX socket:\t\t%s\n"), drizzle_con_uds(&con));
  else
    tee_fprintf(stdout, _("TCP port:\t\t%d\n"), drizzle_con_port(&con));

  if (safe_updates)
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, _("\nNote that you are running in safe_update_mode:\n"));
    vidattr(A_NORMAL);
    tee_fprintf(stdout, _("\
UPDATEs and DELETEs that don't use a key in the WHERE clause are not allowed.\n\
(One can force an UPDATE/DELETE by adding LIMIT # at the end of the command.)\n \
SELECT has an automatic 'LIMIT %lu' if LIMIT is not used.\n             \
Max number of examined row combination in a join is set to: %lu\n\n"),
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

  if (status.getBatch())
  {
    if (info_type == INFO_ERROR)
    {
      (void) fflush(file);
      fprintf(file,_("ERROR"));
      if (error)
      {
        if (sqlstate)
          (void) fprintf(file," %d (%s)",error, sqlstate);
        else
          (void) fprintf(file," %d",error);
      }
      if (status.getQueryStartLine() && line_numbers)
      {
        (void) fprintf(file," at line %"PRIu32,status.getQueryStartLine());
        if (status.getFileName())
          (void) fprintf(file," in file: '%s'", status.getFileName());
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
          (void) tee_fprintf(file, _("ERROR %d (%s): "), error, sqlstate);
        else
          (void) tee_fprintf(file, _("ERROR %d: "), error);
      }
      else
        tee_puts(_("ERROR: "), file);
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
  while (start < end && !isgraph(end[-1]))
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

static boost::posix_time::ptime start_timer(void)
{
  return boost::posix_time::microsec_clock::universal_time();
}

static void nice_time(boost::posix_time::time_duration duration, string &buff)
{
  ostringstream tmp_buff_str;

  if (duration.hours() > 0)
  {
    tmp_buff_str << duration.hours();
    if (duration.hours() > 1)
      tmp_buff_str << _(" hours ");
    else
      tmp_buff_str << _(" hour ");
  }
  if (duration.hours() > 0 || duration.minutes() > 0)
  {
    tmp_buff_str << duration.minutes() << _(" min ");
  }

  tmp_buff_str.precision(duration.num_fractional_digits());

  double seconds= duration.fractional_seconds();

  seconds/= pow(10.0,duration.num_fractional_digits());

  seconds+= duration.seconds();
  tmp_buff_str << seconds << _(" sec");

  buff.append(tmp_buff_str.str());
}

static void end_timer(boost::posix_time::ptime start_time, string &buff)
{
  boost::posix_time::ptime end_time= start_timer();
  boost::posix_time::time_period duration(start_time, end_time);

  nice_time(duration.length(), buff);
}


static void drizzle_end_timer(boost::posix_time::ptime start_time, string &buff)
{
  buff.append(" (");
  end_timer(start_time,buff);
  buff.append(")");
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
  string::iterator c= current_prompt.begin();
  while (c != current_prompt.end())
  {
    if (*c != PROMPT_CHAR)
    {
      processed_prompt->push_back(*c);
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
        --c;
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
        processed_prompt->append(not current_db.empty() ? current_db : "(none)");
        break;
      case 'h':
      {
        const char *prompt= connected ? drizzle_con_host(&con) : "not_connected";
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

        if (drizzle_con_uds(&con))
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
                                 (!current_user.empty() ?  current_user : "(unknown)"));
        break;
      case 'u':
        if (!full_username)
          init_username();
        processed_prompt->append(part_username ? part_username :
                                 (!current_user.empty() ?  current_user : _("(unknown)")));
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
        processed_prompt->append(get_day_name(t->tm_wday));
        break;
      case 'P':
        processed_prompt->append(t->tm_hour < 12 ? "am" : "pm");
        break;
      case 'o':
        add_int_to_prompt(t->tm_mon+1);
        break;
      case 'O':
        processed_prompt->append(get_month_name(t->tm_mon));
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
        processed_prompt->push_back(*c);
      }
    }
    ++c;
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
    tee_fprintf(stdout, _("Returning to default PROMPT of %s\n"),
                default_prompt);
  prompt_counter = 0;
  char * tmpptr= strdup(ptr ? ptr+1 : default_prompt);
  if (tmpptr == NULL)
    tee_fprintf(stdout, _("Memory allocation error. Not changing prompt\n"));
  else
  {
    current_prompt.erase();
    current_prompt= tmpptr;
    tee_fprintf(stdout, _("PROMPT set to '%s'\n"), current_prompt.c_str());
  }
  return 0;
}

/*
    strcont(str, set) if str contanies any character in the string set.
    The result is the position of the first found character in str, or NULL
    if there isn't anything found.
*/

static const char * strcont(const char *str, const char *set)
{
  const char * start = (const char *) set;

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
