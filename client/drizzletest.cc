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

/*
  drizzletest

  Tool used for executing a .test file

  See the "DRIZZLE Test framework manual" for more information
  http://dev.mysql.com/doc/drizzletest/en/index.html

  Please keep the test framework tools identical in all versions!

  Written by:
  Sasha Pachev <sasha@mysql.com>
  Matt Wagner  <matt@mysql.com>
  Monty
  Jani
  Holyfoot
*/

#define MTEST_VERSION "3.3"

#include <config.h>
#include <client/get_password.h>
#include <libdrizzle/libdrizzle.hpp>

#include <queue>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <cassert>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <boost/array.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <boost/smart_ptr.hpp>

#include PCRE_HEADER

#include <stdarg.h>
#include <boost/unordered_map.hpp>

/* Added this for string translation. */
#include <drizzled/gettext.h>

#include <drizzled/definitions.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/type/time.h>
#include <drizzled/charset.h>
#include <drizzled/typelib.h>
#include <drizzled/configmake.h>
#include <drizzled/util/find_ptr.h>

#define PTR_BYTE_DIFF(A,B) (ptrdiff_t) (reinterpret_cast<const unsigned char*>(A) - reinterpret_cast<const unsigned char*>(B))

#ifndef DRIZZLE_RETURN_SERVER_GONE
#define DRIZZLE_RETURN_HANDSHAKE_FAILED DRIZZLE_RETURN_ERROR_CODE
#endif
namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

unsigned char *get_var_key(const unsigned char* var, size_t *len, bool);

int get_one_option(int optid, const struct option *, char *argument);

#define MAX_VAR_NAME_LENGTH    256
#define MAX_COLUMNS            256
#define MAX_DELIMITER_LENGTH 16
/* Flags controlling send and reap */
#define QUERY_SEND_FLAG  1
#define QUERY_REAP_FLAG  2

typedef boost::unordered_map<std::string, uint32_t> ErrorCodes;
ErrorCodes global_error_names;

enum {
  OPT_PS_PROTOCOL, OPT_SP_PROTOCOL, OPT_CURSOR_PROTOCOL, OPT_VIEW_PROTOCOL,
  OPT_MAX_CONNECT_RETRIES, OPT_MARK_PROGRESS, OPT_LOG_DIR, OPT_TAIL_LINES,
  OPT_TESTDIR
};

static int record= 0, opt_sleep= -1;
static char *opt_pass= NULL;
const char *unix_sock= NULL;
static uint32_t opt_port= 0;
static uint32_t opt_max_connect_retries;
static bool silent= false, verbose= false;
static bool opt_mark_progress= false;
static bool parsing_disabled= false;
static bool display_result_vertically= false,
  display_metadata= false, display_result_sorted= false;
static bool disable_query_log= false, disable_result_log= false;
static bool disable_warnings= false;
static bool disable_info= true;
static bool abort_on_error= true;
static bool server_initialized= false;
static bool is_windows= false;
static bool use_drizzle_protocol= false;
static char line_buffer[MAX_DELIMITER_LENGTH], *line_buffer_pos= line_buffer;
static void free_all_replace();

std::string opt_basedir,
  opt_charsets_dir,
  opt_db,
  opt_host,
  opt_include,
  opt_testdir,
  opt_logdir,
  password,
  opt_password,
  result_file_name,
  opt_user,
  opt_protocol;

static uint32_t start_lineno= 0; /* Start line of current command */

/* Number of lines of the result to include in failure report */
static uint32_t opt_tail_lines= 0;

static char delimiter[MAX_DELIMITER_LENGTH]= ";";
static uint32_t delimiter_length= 1;

static char TMPDIR[FN_REFLEN];

/* Block stack */
enum block_cmd {
  cmd_none,
  cmd_if,
  cmd_while
};

struct st_block
{
  int             line; /* Start line of block */
  bool         ok;   /* Should block be executed */
  enum block_cmd  cmd;  /* Command owning the block */
};

static struct st_block block_stack[32];
static struct st_block *cur_block, *block_stack_end;

/* Open file stack */
struct st_test_file
{
  FILE* file;
  const char *file_name;
  uint32_t lineno; /* Current line in file */
};

static boost::array<st_test_file, 16> file_stack;
static st_test_file* cur_file;

static const charset_info_st *charset_info= &my_charset_utf8_general_ci; /* Default charset */

/*
  Timer related variables
  See the timer_output() definition for details
*/
static char *timer_file = NULL;
static uint64_t timer_start;
static void timer_output();
static uint64_t timer_now();

static uint64_t progress_start= 0;

vector<struct st_command*> q_lines;

struct parser_st
{
  int read_lines;
  int current_line;
} parser;

struct master_pos_st
{
  char file[FN_REFLEN];
  uint32_t pos;
};

master_pos_st master_pos;

/* if set, all results are concated and compared against this file */

class VAR
{
public:
  char *name;
  int name_len;
  char *str_val;
  int str_val_len;
  int int_val;
  int alloced_len;
  int int_dirty; /* do not update string if int is updated until first read */
  int alloced;
  char *env_s;
};

/*Perl/shell-like variable registers */
boost::array<VAR, 10> var_reg;

typedef boost::unordered_map<string, VAR *> var_hash_t;
var_hash_t var_hash;

class st_connection
{
public:
  st_connection() : con(drizzle)
  {
    drizzle_con_add_options(*this, use_drizzle_protocol ? DRIZZLE_CON_EXPERIMENTAL : DRIZZLE_CON_MYSQL);
  }

  operator drizzle::connection_c&()
  {
    return con;
  }

  operator drizzle_con_st*()
  {
    return &con.b_;
  }

  drizzle::drizzle_c drizzle;
  drizzle::connection_c con;
};

typedef map<string, st_connection*> connections_t;
connections_t g_connections;
st_connection* cur_con= NULL;

/*
  List of commands in drizzletest
  Must match the "command_names" array
  Add new commands before Q_UNKNOWN!
*/
enum enum_commands 
{
  Q_CONNECTION=1,     Q_QUERY,
  Q_CONNECT,      Q_SLEEP, Q_REAL_SLEEP,
  Q_INC,        Q_DEC,
  Q_SOURCE,      Q_DISCONNECT,
  Q_LET,        Q_ECHO,
  Q_WHILE,      Q_END_BLOCK,
  Q_SYSTEM,      Q_RESULT,
  Q_REQUIRE,      Q_SAVE_MASTER_POS,
  Q_SYNC_WITH_MASTER,
  Q_SYNC_SLAVE_WITH_MASTER,
  Q_ERROR,
  Q_SEND,        Q_REAP,
  Q_DIRTY_CLOSE,      Q_REPLACE, Q_REPLACE_COLUMN,
  Q_PING,        Q_EVAL,
  Q_EVAL_RESULT,
  Q_ENABLE_QUERY_LOG, Q_DISABLE_QUERY_LOG,
  Q_ENABLE_RESULT_LOG, Q_DISABLE_RESULT_LOG,
  Q_WAIT_FOR_SLAVE_TO_STOP,
  Q_ENABLE_WARNINGS, Q_DISABLE_WARNINGS,
  Q_ENABLE_INFO, Q_DISABLE_INFO,
  Q_ENABLE_METADATA, Q_DISABLE_METADATA,
  Q_EXEC, Q_DELIMITER,
  Q_DISABLE_ABORT_ON_ERROR, Q_ENABLE_ABORT_ON_ERROR,
  Q_DISPLAY_VERTICAL_RESULTS, Q_DISPLAY_HORIZONTAL_RESULTS,
  Q_QUERY_VERTICAL, Q_QUERY_HORIZONTAL, Q_SORTED_RESULT,
  Q_START_TIMER, Q_END_TIMER,
  Q_CHARACTER_SET,
  Q_DISABLE_RECONNECT, Q_ENABLE_RECONNECT,
  Q_IF,
  Q_DISABLE_PARSING, Q_ENABLE_PARSING,
  Q_REPLACE_REGEX, Q_REMOVE_FILE, Q_FILE_EXIST,
  Q_WRITE_FILE, Q_COPY_FILE, Q_PERL, Q_DIE, Q_EXIT, Q_SKIP,
  Q_CHMOD_FILE, Q_APPEND_FILE, Q_CAT_FILE, Q_DIFF_FILES,
  Q_SEND_QUIT, Q_CHANGE_USER, Q_MKDIR, Q_RMDIR,

  Q_UNKNOWN,             /* Unknown command.   */
  Q_COMMENT,             /* Comments, ignored. */
  Q_COMMENT_WITH_COMMAND
};


const char *command_names[]=
{
  "connection",
  "query",
  "connect",
  "sleep",
  "real_sleep",
  "inc",
  "dec",
  "source",
  "disconnect",
  "let",
  "echo",
  "while",
  "end",
  "system",
  "result",
  "require",
  "save_master_pos",
  "sync_with_master",
  "sync_slave_with_master",
  "error",
  "send",
  "reap",
  "dirty_close",
  "replace_result",
  "replace_column",
  "ping",
  "eval",
  "eval_result",
  /* Enable/disable that the _query_ is logged to result file */
  "enable_query_log",
  "disable_query_log",
  /* Enable/disable that the _result_ from a query is logged to result file */
  "enable_result_log",
  "disable_result_log",
  "wait_for_slave_to_stop",
  "enable_warnings",
  "disable_warnings",
  "enable_info",
  "disable_info",
  "enable_metadata",
  "disable_metadata",
  "exec",
  "delimiter",
  "disable_abort_on_error",
  "enable_abort_on_error",
  "vertical_results",
  "horizontal_results",
  "query_vertical",
  "query_horizontal",
  "sorted_result",
  "start_timer",
  "end_timer",
  "character_set",
  "disable_reconnect",
  "enable_reconnect",
  "if",
  "disable_parsing",
  "enable_parsing",
  "replace_regex",
  "remove_file",
  "file_exists",
  "write_file",
  "copy_file",
  "perl",
  "die",

  /* Don't execute any more commands, compare result */
  "exit",
  "skip",
  "chmod",
  "append_file",
  "cat_file",
  "diff_files",
  "send_quit",
  "change_user",
  "mkdir",
  "rmdir",

  0
};


/*
  The list of error codes to --error are stored in an internal array of
  structs. This struct can hold numeric SQL error codes, error names or
  SQLSTATE codes as strings. The element next to the last active element
  in the list is set to type ERR_EMPTY. When an SQL statement returns an
  error, we use this list to check if this is an expected error.
*/
enum match_err_type
{
  ERR_EMPTY= 0,
  ERR_ERRNO,
  ERR_SQLSTATE
};

struct st_match_err
{
  enum match_err_type type;
  union
  {
    uint32_t errnum;
    char sqlstate[DRIZZLE_MAX_SQLSTATE_SIZE+1];  /* \0 terminated string */
  } code;
};

struct st_expected_errors
{
  struct st_match_err err[10];
  uint32_t count;
};

static st_expected_errors saved_expected_errors;

class st_command
{
public:
  char *query, *query_buf,*first_argument,*last_argument,*end;
  int first_word_len, query_len;
  bool abort_on_error;
  st_expected_errors expected_errors;
  string require_file;
  enum_commands type;

  st_command()
    : query(NULL), query_buf(NULL), first_argument(NULL), last_argument(NULL),
      end(NULL), first_word_len(0), query_len(0), abort_on_error(false),
      require_file(""), type(Q_CONNECTION)
  {
    memset(&expected_errors, 0, sizeof(st_expected_errors));
  }

  ~st_command()
  {
    free(query_buf);
  }
};

TYPELIB command_typelib= {array_elements(command_names),"",
                          command_names, 0};

string ds_res, ds_progress, ds_warning_messages;

char builtin_echo[FN_REFLEN];

void die(const char *fmt, ...)
  __attribute__((format(printf, 1, 2)));
void abort_not_supported_test(const char *fmt, ...)
  __attribute__((format(printf, 1, 2)));
void verbose_msg(const char *fmt, ...)
  __attribute__((format(printf, 1, 2)));
void warning_msg(const char *fmt, ...)
  __attribute__((format(printf, 1, 2)));
void log_msg(const char *fmt, ...)
  __attribute__((format(printf, 1, 2)));

VAR* var_from_env(const char *, const char *);
VAR* var_init(VAR* v, const char *name, int name_len, const char *val,
              int val_len);
VAR* var_get(const char *var_name, const char** var_name_end,
             bool raw, bool ignore_not_existing);
void eval_expr(VAR* v, const char *p, const char** p_end);
bool match_delimiter(int c, const char *delim, uint32_t length);
void dump_result_to_reject_file(char *buf, int size);
void dump_result_to_log_file(const char *buf, int size);
void dump_warning_messages();
void dump_progress();

void do_eval(string *query_eval, const char *query,
             const char *query_end, bool pass_through_escape_chars);
void str_to_file(const char *fname, const char *str, int size);
void str_to_file2(const char *fname, const char *str, int size, bool append);

/* For replace_column */
static char *replace_column[MAX_COLUMNS];
static uint32_t max_replace_column= 0;
void do_get_replace_column(st_command*);
void free_replace_column();

/* For replace */
void do_get_replace(st_command* command);
void free_replace();

/* For replace_regex */
void do_get_replace_regex(st_command* command);

void replace_append_mem(string& ds, const char *val, int len);
void replace_append(string *ds, const char *val);
void replace_append_uint(string& ds, uint32_t val);
void append_sorted(string& ds, const string& ds_input);

void handle_error(st_command*,
                  unsigned int err_errno, const char *err_error,
                  const char *err_sqlstate, string *ds);
void handle_no_error(st_command*);


void do_eval(string *query_eval, const char *query,
             const char *query_end, bool pass_through_escape_chars)
{
  char c, next_c;
  int escaped = 0;

  for (const char *p= query; (c= *p) && p < query_end; ++p)
  {
    switch(c) 
    {
    case '$':
      if (escaped)
      {
        escaped= 0;
        query_eval->append(p, 1);
      }
      else
      {
        VAR* v= var_get(p, &p, 0, 0);
        if (not v)
          die("Bad variable in eval");
        query_eval->append(v->str_val, v->str_val_len);
      }
      break;
    case '\\':
      next_c= *(p+1);
      if (escaped)
      {
        escaped= 0;
        query_eval->append(p, 1);
      }
      else if (next_c == '\\' || next_c == '$' || next_c == '"')
      {
        /* Set escaped only if next char is \, " or $ */
        escaped= 1;

        if (pass_through_escape_chars)
        {
          /* The escape char should be added to the output string. */
          query_eval->append(p, 1);
        }
      }
      else
        query_eval->append(p, 1);
      break;
    default:
      escaped= 0;
      query_eval->append(p, 1);
    }
  }
}


/*
  Concatenates any number of strings, escapes any OS quote in the result then
  surround the whole affair in another set of quotes which is finally appended
  to specified string.  This function is especially useful when
  building strings to be executed with the system() function.

  @param str string which will have addtional strings appended.
  @param append string to be appended.
  @param ... Optional. Additional string(s) to be appended.

  @note The final argument in the list must be NULL even if no additional
  options are passed.
*/

static void append_os_quoted(string *str, const char *append, ...)
{
  const char *quote_str= "\'";
  const uint32_t  quote_len= 1;

  va_list dirty_text;

  str->append(quote_str, quote_len); /* Leading quote */
  va_start(dirty_text, append);
  while (append != NULL)
  {
    const char  *cur_pos= append;
    const char *next_pos= cur_pos;

    /* Search for quote in each string and replace with escaped quote */
    while ((next_pos= strrchr(cur_pos, quote_str[0])) != NULL)
    {
      str->append(cur_pos, next_pos - cur_pos);
      str->append("\\", 1);
      str->append(quote_str, quote_len);
      cur_pos= next_pos + 1;
    }
    str->append(cur_pos);
    append= va_arg(dirty_text, char *);
  }
  va_end(dirty_text);
  str->append(quote_str, quote_len); /* Trailing quote */
}


/*
  Run query and dump the result to stdout in vertical format

  NOTE! This function should be safe to call when an error
  has occured and thus any further errors will be ignored(although logged)

  SYNOPSIS
  show_query
  drizzle - connection to use
  query - query to run

*/

static int dt_query_log(drizzle::connection_c& con, drizzle::result_c& res, const std::string& query)
{
  if (drizzle_return_t ret= con.query(res, query))
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      log_msg("Error running query '%s': %d %s", query.c_str(), res.error_code(), res.error());
    }
    else
    {
      log_msg("Error running query '%s': %d %s", query.c_str(), ret, con.error());
    }
    return 1;
  }
  return res.column_count() == 0;
}

static void show_query(drizzle::connection_c& con, const char* query)
{
  drizzle::result_c res;
  if (dt_query_log(con, res, query))
    return;

  unsigned int row_num= 0;
  unsigned int num_fields= res.column_count();

  fprintf(stderr, "=== %s ===\n", query);
  while (drizzle_row_t row= res.row_next())
  {
    size_t *lengths= res.row_field_sizes();
    row_num++;

    fprintf(stderr, "---- %d. ----\n", row_num);
    res.column_seek(0);
    for (unsigned int i= 0; i < num_fields; i++)
    {
      drizzle_column_st* column= res.column_next();
      fprintf(stderr, "%s\t%.*s\n", drizzle_column_name(column), (int)lengths[i], row[i] ? row[i] : "NULL");
    }
  }
  for (size_t i= 0; i < strlen(query)+8; i++)
    fprintf(stderr, "=");
  fprintf(stderr, "\n\n");
}


/*
  Show any warnings just before the error. Since the last error
  is added to the warning stack, only print @@warning_count-1 warnings.

  NOTE! This function should be safe to call when an error
  has occured and this any further errors will be ignored(although logged)

  SYNOPSIS
  show_warnings_before_error
  drizzle - connection to use

*/

static void show_warnings_before_error(drizzle::connection_c& con)
{
  drizzle::result_c res;
  if (dt_query_log(con, res, "show warnings"))
    return;

  if (res.row_count() >= 2) /* Don't display the last row, it's "last error" */
  {
    unsigned int row_num= 0;
    unsigned int num_fields= res.column_count();

    fprintf(stderr, "\nWarnings from just before the error:\n");
    while (drizzle_row_t row= res.row_next())
    {
      size_t *lengths= res.row_field_sizes();

      if (++row_num >= res.row_count())
      {
        /* Don't display the last row, it's "last error" */
        break;
      }

      for (uint32_t i= 0; i < num_fields; i++)
      {
        fprintf(stderr, "%.*s ", (int)lengths[i], row[i] ? row[i] : "NULL");
      }
      fprintf(stderr, "\n");
    }
  }
}

enum arg_type
{
  ARG_STRING,
  ARG_REST
};

struct command_arg 
{
  const char *argname;       /* Name of argument   */
  enum arg_type type;        /* Type of argument   */
  bool required;          /* Argument required  */
  string *ds;        /* Storage for argument */
  const char *description;   /* Description of the argument */
};


static void check_command_args(st_command* command,
                               const char *arguments,
                               const struct command_arg *args,
                               int num_args, const char delimiter_arg)
{
  const char *ptr= arguments;
  const char *start;

  for (int i= 0; i < num_args; i++)
  {
    const struct command_arg *arg= &args[i];
    arg->ds->clear();

    bool known_arg_type= true;
    switch (arg->type) {
      /* A string */
    case ARG_STRING:
      /* Skip leading spaces */
      while (*ptr && *ptr == ' ')
        ptr++;
      start= ptr;
      /* Find end of arg, terminated by "delimiter_arg" */
      while (*ptr && *ptr != delimiter_arg)
        ptr++;
      if (ptr > start)
      {
        do_eval(arg->ds, start, ptr, false);
      }
      else
      {
        /* Empty string */
        arg->ds->erase();
      }
      command->last_argument= (char*)ptr;

      /* Step past the delimiter */
      if (*ptr && *ptr == delimiter_arg)
        ptr++;
      break;

      /* Rest of line */
    case ARG_REST:
      start= ptr;
      do_eval(arg->ds, start, command->end, false);
      command->last_argument= command->end;
      break;

    default:
      known_arg_type= false;
      break;
    }
    assert(known_arg_type);

    /* Check required arg */
    if (arg->ds->length() == 0 && arg->required)
      die("Missing required argument '%s' to command '%.*s'", arg->argname,
          command->first_word_len, command->query);

  }
  /* Check for too many arguments passed */
  ptr= command->last_argument;
  while (ptr <= command->end)
  {
    if (*ptr && *ptr != ' ')
      die("Extra argument '%s' passed to '%.*s'",
          ptr, command->first_word_len, command->query);
    ptr++;
  }
  return;
}


static void handle_command_error(st_command* command, uint32_t error)
{
  if (error != 0)
  {
    if (command->abort_on_error)
      die("command \"%.*s\" failed with error %d", command->first_word_len, command->query, error);
    for (uint32_t i= 0; i < command->expected_errors.count; i++)
    {
      if (command->expected_errors.err[i].type == ERR_ERRNO &&
          command->expected_errors.err[i].code.errnum == error)
      {
        return;
      }
    }
    die("command \"%.*s\" failed with wrong error: %d",
        command->first_word_len, command->query, error);
  }
  else if (command->expected_errors.err[0].type == ERR_ERRNO &&
           command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    die("command \"%.*s\" succeeded - should have failed with errno %d...",
        command->first_word_len, command->query,
        command->expected_errors.err[0].code.errnum);
  }
}

static void cleanup_and_exit(int exit_code)
{
  if (!silent) 
  {
    switch (exit_code) 
    {
    case 1:
      printf("not ok\n");
      break;
    case 0:
      printf("ok\n");
      break;
    case 62:
      printf("skipped\n");
      break;
    default:
      printf("unknown exit code: %d\n", exit_code);
      assert(false);
    }
  }
  exit(exit_code);
}

void die(const char *fmt, ...)
{
  /*
    Protect against dying twice
    first time 'die' is called, try to write log files
    second time, just exit
  */
  static bool dying= false;
  if (dying)
    cleanup_and_exit(1);
  dying= true;

  /* Print the error message */
  fprintf(stderr, "drizzletest: ");
  if (cur_file && cur_file != file_stack.data())
    fprintf(stderr, "In included file \"%s\": ", cur_file->file_name);
  if (start_lineno > 0)
    fprintf(stderr, "At line %u: ", start_lineno);
  if (fmt)
  {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
  }
  else
    fprintf(stderr, "unknown error");
  fprintf(stderr, "\n");
  fflush(stderr);

  /* Show results from queries just before failure */
  if (ds_res.length() && opt_tail_lines)
  {
    int tail_lines= opt_tail_lines;
    const char* show_from= ds_res.c_str() + ds_res.length() - 1;
    while (show_from > ds_res.c_str() && tail_lines > 0 )
    {
      show_from--;
      if (*show_from == '\n')
        tail_lines--;
    }
    fprintf(stderr, "\nThe result from queries just before the failure was:\n");
    if (show_from > ds_res.c_str())
      fprintf(stderr, "< snip >");
    fprintf(stderr, "%s", show_from);
    fflush(stderr);
  }

  /* Dump the result that has been accumulated so far to .log file */
  if (! result_file_name.empty() && ds_res.length())
    dump_result_to_log_file(ds_res.c_str(), ds_res.length());

  /* Dump warning messages */
  if (! result_file_name.empty() && ds_warning_messages.length())
    dump_warning_messages();

  /*
    Help debugging by displaying any warnings that might have
    been produced prior to the error
  */
  if (cur_con)
    show_warnings_before_error(*cur_con);

  cleanup_and_exit(1);
}


void abort_not_supported_test(const char *fmt, ...)
{
  va_list args;
  st_test_file* err_file= cur_file;


  /* Print include filestack */
  fprintf(stderr, "The test '%s' is not supported by this installation\n",
          file_stack[0].file_name);
  fprintf(stderr, "Detected in file %s at line %d\n",
          err_file->file_name, err_file->lineno);
  while (err_file != file_stack.data())
  {
    err_file--;
    fprintf(stderr, "included from %s at line %d\n",
            err_file->file_name, err_file->lineno);
  }

  /* Print error message */
  va_start(args, fmt);
  if (fmt)
  {
    fprintf(stderr, "reason: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  va_end(args);

  cleanup_and_exit(62);
}


void verbose_msg(const char *fmt, ...)
{
  if (!verbose)
    return;
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "drizzletest: ");
  if (cur_file && cur_file != file_stack.data())
    fprintf(stderr, "In included file \"%s\": ", cur_file->file_name);
  if (start_lineno != 0)
    fprintf(stderr, "At line %u: ", start_lineno);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}


void warning_msg(const char *fmt, ...)
{
  va_list args;
  char buff[512];
  size_t len;

  va_start(args, fmt);
  ds_warning_messages += "drizzletest: ";
  if (start_lineno != 0)
  {
    ds_warning_messages += "Warning detected ";
    if (cur_file && cur_file != file_stack.data())
    {
      len= snprintf(buff, sizeof(buff), "in included file %s ", cur_file->file_name);
      ds_warning_messages.append(buff, len);
    }
    len= snprintf(buff, sizeof(buff), "at line %d: ", start_lineno);
    ds_warning_messages.append(buff, len);
  }

  len= vsnprintf(buff, sizeof(buff), fmt, args);
  ds_warning_messages.append(buff, len);

  ds_warning_messages += "\n";
  va_end(args);

  return;
}


void log_msg(const char *fmt, ...)
{
  va_list args;
  char buff[1024];

  va_start(args, fmt);
  size_t len= vsnprintf(buff, sizeof(buff)-1, fmt, args);
  va_end(args);

  ds_res.append(buff, len);
  ds_res += "\n";
}


/*
  Read a file and append it to ds

  SYNOPSIS
  cat_file
  ds - pointer to dynamic string where to add the files content
  filename - name of the file to read

*/

static void cat_file(string& ds, const char* filename)
{
  int fd= internal::my_open(filename, O_RDONLY, MYF(0));
  if (fd < 0)
    die("Failed to open file '%s'", filename);
  char buff[512];
  while (uint32_t len= internal::my_read(fd, (unsigned char*)&buff, sizeof(buff), MYF(0)))
  {
    char *p= buff, *start= buff;
    while (p < buff+len)
    {
      /* Convert cr/lf to lf */
      if (*p == '\r' && *(p+1) && *(p+1)== '\n')
      {
        /* Add fake newline instead of cr and output the line */
        *p= '\n';
        p++; /* Step past the "fake" newline */
        ds.append(start, p - start);
        p++; /* Step past the "fake" newline */
        start= p;
      }
      else
        p++;
    }
    /* Output any chars that might be left */
    ds.append(start, p - start);
  }
  internal::my_close(fd, MYF(0));
}


/*
  Run the specified command with popen

  SYNOPSIS
  run_command
  cmd - command to execute(should be properly quoted
  result - pointer to string where to store the result

*/

static int run_command(const char* cmd, string& result)
{
  FILE* res_file= popen(cmd, "r");
  if (not res_file)
    die("popen(\"%s\", \"r\") failed", cmd);

  char buf[512]= {0};
  while (fgets(buf, sizeof(buf), res_file))
  {
    /* Save the output of this command in the supplied string */
    result.append(buf);
  }
  int error= pclose(res_file);
  return WEXITSTATUS(error);
}


/*
  Run the specified tool with variable number of arguments

  SYNOPSIS
  run_tool
  tool_path - the name of the tool to run
  result - pointer to dynamic string where to store the result
  ... - variable number of arguments that will be properly
  quoted and appended after the tool's name

*/

static int run_tool(const char *tool_path, string& result, ...)
{
  string ds_cmdline;
  append_os_quoted(&ds_cmdline, tool_path, NULL);
  ds_cmdline += " ";

  va_list args;
  va_start(args, result);
  while (const char* arg= va_arg(args, char *))
  {
    /* Options should be os quoted */
    if (strncmp(arg, "--", 2) == 0)
      append_os_quoted(&ds_cmdline, arg, NULL);
    else
      ds_cmdline += arg;
    ds_cmdline += " ";
  }

  va_end(args);

  return run_command(ds_cmdline.c_str(), result);
}


/*
  Show the diff of two files using the systems builtin diff
  command. If no such diff command exist, just dump the content
  of the two files and inform about how to get "diff"

  SYNOPSIS
  show_diff
  ds - pointer to dynamic string where to add the diff(may be NULL)
  filename1 - name of first file
  filename2 - name of second file

*/

static void show_diff(string* ds, const char* filename1, const char* filename2)
{
  string ds_tmp;

  /* First try with unified diff */
  if (run_tool("diff",
               ds_tmp, /* Get output from diff in ds_tmp */
               "-u",
               filename1,
               filename2,
               "2>&1",
               NULL) > 1) /* Most "diff" tools return >1 if error */
  {

    /* Fallback to context diff with "diff -c" */
    if (run_tool("diff",
                 ds_tmp, /* Get output from diff in ds_tmp */
                 "-c",
                 filename1,
                 filename2,
                 "2>&1",
                 NULL) > 1) /* Most "diff" tools return >1 if error */
    {
      /*
        Fallback to dump both files to result file and inform
        about installing "diff"
      */
      ds_tmp=
                    "\n"
                    "The two files differ but it was not possible to execute 'diff' in\n"
                    "order to show only the difference, tried both 'diff -u' or 'diff -c'.\n"
                    "Instead the whole content of the two files was shown for you to diff manually. ;)\n"
                    "\n"
                    "To get a better report you should install 'diff' on your system, which you\n"
                    "for example can get from http://www.gnu.org/software/diffutils/diffutils.html\n"
                    "\n";

      ds_tmp += " --- ";
      ds_tmp += filename1;
      ds_tmp += " >>>\n";
      cat_file(ds_tmp, filename1);
      ds_tmp += "<<<\n --- ";
      ds_tmp += filename1;
      ds_tmp += " >>>\n";
      cat_file(ds_tmp, filename2);
      ds_tmp += "<<<<\n";
    }
  }

  if (ds)
  {
    /* Add the diff to output */
    *ds += ds_tmp;
  }
  else
  {
    /* Print diff directly to stdout */
    fprintf(stderr, "%s\n", ds_tmp.c_str());
  }

}

enum compare_files_result_enum 
{
  RESULT_OK= 0,
  RESULT_CONTENT_MISMATCH= 1,
  RESULT_LENGTH_MISMATCH= 2
};

/*
  Compare two files, given a fd to the first file and
  name of the second file

  SYNOPSIS
  compare_files2
  fd - Open file descriptor of the first file
  filename2 - Name of second file

  RETURN VALUES
  According to the values in "compare_files_result_enum"

*/

static int compare_files2(int fd, const char* filename2)
{
  int error= RESULT_OK;
  uint32_t len, len2;
  char buff[512], buff2[512];
  const char *fname= filename2;
  string tmpfile;

  int fd2= internal::my_open(fname, O_RDONLY, MYF(0));
  if (fd2 < 0)
  {
    internal::my_close(fd, MYF(0));
    if (! opt_testdir.empty())
    {
      tmpfile= opt_testdir;
      if (tmpfile[tmpfile.length()] != '/')
        tmpfile += "/";
      tmpfile += filename2;
      fname= tmpfile.c_str();
    }
    if ((fd2= internal::my_open(fname, O_RDONLY, MYF(0))) < 0)
    {
      internal::my_close(fd, MYF(0));
    
      die("Failed to open second file: '%s'", fname);
    }
  }
  while ((len= internal::my_read(fd, (unsigned char*)&buff,
                      sizeof(buff), MYF(0))) > 0)
  {
    if ((len2= internal::my_read(fd2, (unsigned char*)&buff2,
                       sizeof(buff2), MYF(0))) < len)
    {
      /* File 2 was smaller */
      error= RESULT_LENGTH_MISMATCH;
      break;
    }
    if (len2 > len)
    {
      /* File 1 was smaller */
      error= RESULT_LENGTH_MISMATCH;
      break;
    }
    if ((memcmp(buff, buff2, len)))
    {
      /* Content of this part differed */
      error= RESULT_CONTENT_MISMATCH;
      break;
    }
  }
  if (!error && internal::my_read(fd2, (unsigned char*)&buff2,
                        sizeof(buff2), MYF(0)) > 0)
  {
    /* File 1 was smaller */
    error= RESULT_LENGTH_MISMATCH;
  }

  internal::my_close(fd2, MYF(0));

  return error;
}


/*
  Compare two files, given their filenames

  SYNOPSIS
  compare_files
  filename1 - Name of first file
  filename2 - Name of second file

  RETURN VALUES
  See 'compare_files2'

*/

static int compare_files(const char* filename1, const char* filename2)
{
  int fd= internal::my_open(filename1, O_RDONLY, MYF(0));
  if (fd < 0)
    die("Failed to open first file: '%s'", filename1);
  int error= compare_files2(fd, filename2);
  internal::my_close(fd, MYF(0));
  return error;
}


/*
  Compare content of the string in ds to content of file fname

  SYNOPSIS
  string_cmp
  ds - Dynamic string containing the string o be compared
  fname - Name of file to compare with

  RETURN VALUES
  See 'compare_files2'
*/

static int string_cmp(const string& ds, const char *fname)
{
  char temp_file_path[FN_REFLEN];

  int fd= internal::create_temp_file(temp_file_path, TMPDIR, "tmp", MYF(MY_WME));
  if (fd < 0)
    die("Failed to create temporary file for ds");

  /* Write ds to temporary file and set file pos to beginning*/
  if (internal::my_write(fd, (unsigned char *) ds.data(), ds.length(), MYF(MY_FNABP | MY_WME)) ||
      lseek(fd, 0, SEEK_SET) == MY_FILEPOS_ERROR)
  {
    internal::my_close(fd, MYF(0));
    /* Remove the temporary file */
    internal::my_delete(temp_file_path, MYF(0));
    die("Failed to write file '%s'", temp_file_path);
  }

  int error= compare_files2(fd, fname);

  internal::my_close(fd, MYF(0));
  /* Remove the temporary file */
  internal::my_delete(temp_file_path, MYF(0));

  return error;
}


/*
  Check the content of ds against result file

  SYNOPSIS
  check_result
  ds - content to be checked

  RETURN VALUES
  error - the function will not return

*/

static void check_result(string& ds)
{
  const char* mess= "Result content mismatch\n";

  if (access(result_file_name.c_str(), F_OK) != 0)
    die("The specified result file does not exist: '%s'", result_file_name.c_str());

  switch (string_cmp(ds, result_file_name.c_str())) 
  {
  case RESULT_OK:
    break; /* ok */
  case RESULT_LENGTH_MISMATCH:
    mess= "Result length mismatch\n";
    /* Fallthrough */
  case RESULT_CONTENT_MISMATCH:
  {
    /*
      Result mismatched, dump results to .reject file
      and then show the diff
    */
    char reject_file[FN_REFLEN];
    size_t reject_length;
    internal::dirname_part(reject_file, result_file_name.c_str(), &reject_length);

    if (access(reject_file, W_OK) == 0)
    {
      /* Result file directory is writable, save reject file there */
      internal::fn_format(reject_file, result_file_name.c_str(), NULL, ".reject", MY_REPLACE_EXT);
    }
    else
    {
      /* Put reject file in opt_logdir */
      internal::fn_format(reject_file, result_file_name.c_str(), opt_logdir.c_str(), ".reject", MY_REPLACE_DIR | MY_REPLACE_EXT);
    }
    str_to_file(reject_file, ds.data(), ds.length());

    ds.erase(); /* Don't create a .log file */

    show_diff(NULL, result_file_name.c_str(), reject_file);
    die("%s",mess);
    break;
  }
  default: /* impossible */
    die("Unknown error code from dyn_string_cmp()");
  }
}


/*
  Check the content of ds against a require file
  If match fails, abort the test with special error code
  indicating that test is not supported

  SYNOPSIS
  check_require
  ds - content to be checked
  fname - name of file to check against

  RETURN VALUES
  error - the function will not return

*/

static void check_require(const string& ds, const string& fname)
{
  if (string_cmp(ds, fname.c_str()))
  {
    char reason[FN_REFLEN];
    internal::fn_format(reason, fname.c_str(), "", "", MY_REPLACE_EXT | MY_REPLACE_DIR);
    abort_not_supported_test("Test requires: '%s'", reason);
  }
}


/*
  Remove surrounding chars from string

  Return 1 if first character is found but not last
*/
static int strip_surrounding(char* str, char c1, char c2)
{
  char* ptr= str;

  /* Check if the first non space character is c1 */
  while (*ptr && my_isspace(charset_info, *ptr))
    ptr++;
  if (*ptr == c1)
  {
    /* Replace it with a space */
    *ptr= ' ';

    /* Last non space charecter should be c2 */
    ptr= strchr(str, '\0')-1;
    while (*ptr && my_isspace(charset_info, *ptr))
      ptr--;
    if (*ptr == c2)
    {
      /* Replace it with \0 */
      *ptr= 0;
    }
    else
    {
      /* Mismatch detected */
      return 1;
    }
  }
  return 0;
}


static void strip_parentheses(st_command* command)
{
  if (strip_surrounding(command->first_argument, '(', ')'))
    die("%.*s - argument list started with '%c' must be ended with '%c'",
        command->first_word_len, command->query, '(', ')');
}



VAR *var_init(VAR *v, const char *name, int name_len, const char *val,
              int val_len)
{
  if (!name_len && name)
    name_len = strlen(name);
  if (!val_len && val)
    val_len = strlen(val) ;
  VAR *tmp_var = v ? v : (VAR*)malloc(sizeof(*tmp_var) + name_len+1);

  tmp_var->name = name ? (char*)&tmp_var[1] : 0;
  tmp_var->alloced = (v == 0);

  int val_alloc_len = val_len + 16; /* room to grow */
  tmp_var->str_val = (char*)malloc(val_alloc_len+1);

  memcpy(tmp_var->name, name, name_len);
  if (val)
  {
    memcpy(tmp_var->str_val, val, val_len);
    tmp_var->str_val[val_len]= 0;
  }
  tmp_var->name_len = name_len;
  tmp_var->str_val_len = val_len;
  tmp_var->alloced_len = val_alloc_len;
  tmp_var->int_val = val ? atoi(val) : 0;
  tmp_var->int_dirty = false;
  tmp_var->env_s = 0;
  return tmp_var;
}

VAR* var_from_env(const char *name, const char *def_val)
{
  const char *tmp= getenv(name);
  if (!tmp)
    tmp = def_val;
  return var_hash[name] = var_init(0, name, strlen(name), tmp, strlen(tmp));
}

VAR* var_get(const char *var_name, const char **var_name_end, bool raw,
             bool ignore_not_existing)
{
  int digit;
  VAR *v;
  if (*var_name != '$')
    goto err;
  digit = *++var_name - '0';
  if (digit < 0 || digit >= 10)
  {
    const char *save_var_name = var_name, *end;
    uint32_t length;
    end = (var_name_end) ? *var_name_end : 0;
    while (my_isvar(charset_info,*var_name) && var_name != end)
      var_name++;
    if (var_name == save_var_name)
    {
      if (ignore_not_existing)
        return(0);
      die("Empty variable");
    }
    length= (uint32_t) (var_name - save_var_name);
    if (length >= MAX_VAR_NAME_LENGTH)
      die("Too long variable name: %s", save_var_name);

    string save_var_name_str(save_var_name, length);
    if (var_hash_t::mapped_type* ptr= find_ptr(var_hash, save_var_name_str))
      v= *ptr;
    else
    {
      char buff[MAX_VAR_NAME_LENGTH+1];
      strncpy(buff, save_var_name, length);
      buff[length]= '\0';
      v= var_from_env(buff, "");
    }
    var_name--;  /* Point at last character */
  }
  else
    v = &var_reg[digit];

  if (!raw && v->int_dirty)
  {
    sprintf(v->str_val, "%d", v->int_val);
    v->int_dirty = 0;
    v->str_val_len = strlen(v->str_val);
  }
  if (var_name_end)
    *var_name_end = var_name  ;
  return(v);
err:
  if (var_name_end)
    *var_name_end = 0;
  die("Unsupported variable name: %s", var_name);
  return(0);
}


static VAR *var_obtain(const char *name, int len)
{
  string var_name(name, len);
  if (var_hash_t::mapped_type* ptr= find_ptr(var_hash, var_name))
    return *ptr;
  return var_hash[var_name] = var_init(0, name, len, "", 0);
}


/*
  - if variable starts with a $ it is regarded as a local test varable
  - if not it is treated as a environment variable, and the corresponding
  environment variable will be updated
*/

static void var_set(const char *var_name, const char *var_name_end,
                    const char *var_val, const char *var_val_end)
{
  int digit, env_var= 0;
  VAR *v;

  if (*var_name != '$')
    env_var= 1;
  else
    var_name++;

  digit= *var_name - '0';
  if (!(digit < 10 && digit >= 0))
  {
    v= var_obtain(var_name, (uint32_t) (var_name_end - var_name));
  }
  else
    v= &var_reg[digit];

  eval_expr(v, var_val, (const char**) &var_val_end);

  if (env_var)
  {
    char buf[1024], *old_env_s= v->env_s;
    if (v->int_dirty)
    {
      sprintf(v->str_val, "%d", v->int_val);
      v->int_dirty= 0;
      v->str_val_len= strlen(v->str_val);
    }
    snprintf(buf, sizeof(buf), "%.*s=%.*s",
             v->name_len, v->name,
             v->str_val_len, v->str_val);
    v->env_s= strdup(buf);
    putenv(v->env_s);
    free(old_env_s);
  }
  return;
}


static void var_set_string(const char* name, const char* value)
{
  var_set(name, name + strlen(name), value, value + strlen(value));
}


static void var_set_int(const char* name, int value)
{
  char buf[21];
  snprintf(buf, sizeof(buf), "%d", value);
  var_set_string(name, buf);
}


/*
  Store an integer (typically the returncode of the last SQL)
  statement in the drizzletest builtin variable $drizzleclient_errno
*/

static void var_set_errno(int sql_errno)
{
  var_set_int("$drizzleclient_errno", sql_errno);
}


/*
  Update $drizzleclient_get_server_version variable with version
  of the currently connected server
*/

static void var_set_drizzleclient_get_server_version(drizzle_con_st *con)
{
  var_set_int("$drizzle_con_server_version", drizzle_con_server_version_number(con));
}


/*
  Set variable from the result of a query

  SYNOPSIS
  var_query_set()
  var          variable to set from query
  query       start of query string to execute
  query_end   end of the query string to execute


  DESCRIPTION
  let @<var_name> = `<query>`

  Execute the query and assign the first row of result to var as
  a tab separated strings

  Also assign each column of the result set to
  variable "$<var_name>_<column_name>"
  Thus the tab separated output can be read from $<var_name> and
  and each individual column can be read as $<var_name>_<col_name>

*/

static void dt_query(drizzle::connection_c& con, drizzle::result_c& res, const std::string& query)
{
  if (drizzle_return_t ret= con.query(res, query))
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      die("Error running query '%s': %d %s", query.c_str(), res.error_code(), res.error());
    }
    else
    {
      die("Error running query '%s': %d %s", query.c_str(), ret, con.error());
    }
  }
  if (res.column_count() == 0)
    die("Query '%s' didn't return a result set", query.c_str());
}

static void var_query_set(VAR *var, const char *query, const char** query_end)
{
  const char *end = ((query_end && *query_end) ? *query_end : query + strlen(query));
  drizzle::connection_c& con= *cur_con;

  while (end > query && *end != '`')
    --end;
  if (query == end)
    die("Syntax error in query, missing '`'");
  ++query;

  string ds_query;
  /* Eval the query, thus replacing all environment variables */
  do_eval(&ds_query, query, end, false);

  drizzle::result_c res;
  dt_query(con, res, ds_query);

  drizzle_row_t row= res.row_next();
  if (row && row[0])
  {
    /*
      Concatenate all fields in the first row with tab in between
      and assign that string to the $variable
    */
    string result;
    size_t* lengths= res.row_field_sizes();
    for (uint32_t i= 0; i < res.column_count(); i++)
    {
      if (row[i])
      {
        /* Add column to tab separated string */
        result.append(row[i], lengths[i]);
      }
      result += "\t";
    }
    end= result.c_str() + result.length() - 1;
    eval_expr(var, result.c_str(), (const char**) &end);
  }
  else
    eval_expr(var, "", 0);
}


/*
  Set variable from the result of a field in a query

  This function is useful when checking for a certain value
  in the output from a query that can't be restricted to only
  return some values. A very good example of that is most SHOW
  commands.

  SYNOPSIS
  var_set_query_get_value()

  DESCRIPTION
  let $variable= query_get_value(<query to run>,<column name>,<row no>);

  <query to run> -    The query that should be sent to the server
  <column name> -     Name of the column that holds the field be compared
  against the expected value
  <row no> -          Number of the row that holds the field to be
  compared against the expected value

*/

static void var_set_query_get_value(st_command* command, VAR *var)
{
  int col_no= -1;
  drizzle::connection_c& con= *cur_con;

  string ds_query;
  string ds_col;
  string ds_row;
  const struct command_arg query_get_value_args[] = {
    {"query", ARG_STRING, true, &ds_query, "Query to run"},
    {"column name", ARG_STRING, true, &ds_col, "Name of column"},
    {"row number", ARG_STRING, true, &ds_row, "Number for row"}
  };



  strip_parentheses(command);
  check_command_args(command, command->first_argument, query_get_value_args,
                     sizeof(query_get_value_args)/sizeof(struct command_arg),
                     ',');

  /* Convert row number to int */
  long row_no= atoi(ds_row.c_str());
  
  istringstream buff(ds_row);
  if ((buff >> row_no).fail())
    die("Invalid row number: '%s'", ds_row.c_str());

  /* Remove any surrounding "'s from the query - if there is any */
  // (Don't get me started on this)
  char* unstripped_query= strdup(ds_query.c_str());
  if (strip_surrounding(unstripped_query, '"', '"'))
    die("Mismatched \"'s around query '%s'", ds_query.c_str());
  ds_query= unstripped_query;

  drizzle::result_c res;
  dt_query(con, res, ds_query);

  {
    /* Find column number from the given column name */
    uint32_t num_fields= res.column_count();
    for (uint32_t i= 0; i < num_fields; i++)
    {
      drizzle_column_st* column= res.column_next();
      if (strcmp(drizzle_column_name(column), ds_col.c_str()) == 0 &&
          strlen(drizzle_column_name(column)) == ds_col.length())
      {
        col_no= i;
        break;
      }
    }
    if (col_no == -1)
    {
      die("Could not find column '%s' in the result of '%s'", ds_col.c_str(), ds_query.c_str());
    }
  }

  {
    /* Get the value */
    long rows= 0;
    const char* value= "No such row";

    while (drizzle_row_t row= res.row_next())
    {
      if (++rows == row_no)
      {
        /* Found the row to get */
        value= row[col_no] ? row[col_no] : "NULL";
        break;
      }
    }
    eval_expr(var, value, 0);
  }
}


static void var_copy(VAR *dest, VAR *src)
{
  dest->int_val= src->int_val;
  dest->int_dirty= src->int_dirty;

  /* Alloc/realloc data for str_val in dest */
  if (dest->alloced_len < src->alloced_len)
  {
    char *tmpptr= (char *)realloc(dest->str_val, src->alloced_len);
    dest->str_val= tmpptr;
  }
  else
    dest->alloced_len= src->alloced_len;

  /* Copy str_val data to dest */
  dest->str_val_len= src->str_val_len;
  if (src->str_val_len)
    memcpy(dest->str_val, src->str_val, src->str_val_len);
}


void eval_expr(VAR *v, const char *p, const char **p_end)
{
  if (*p == '$')
  {
    VAR *vp= var_get(p, p_end, 0, 0);
    if (vp)
      var_copy(v, vp);
    return;
  }

  if (*p == '`')
  {
    var_query_set(v, p, p_end);
    return;
  }

  {
    /* Check if this is a "let $var= query_get_value()" */
    const char* get_value_str= "query_get_value";
    const size_t len= strlen(get_value_str);
    if (strncmp(p, get_value_str, len)==0)
    {
      st_command command;
      command.query= (char*)p;
      command.first_word_len= len;
      command.first_argument= command.query + len;
      command.end= (char*)*p_end;
      var_set_query_get_value(&command, v);
      return;
    }
  }

  {
    int new_val_len = (p_end && *p_end) ?
      (int) (*p_end - p) : (int) strlen(p);
    if (new_val_len + 1 >= v->alloced_len)
    {
      static int MIN_VAR_ALLOC= 32;
      v->alloced_len = (new_val_len < MIN_VAR_ALLOC - 1) ?
        MIN_VAR_ALLOC : new_val_len + 1;
      char *tmpptr= (char *)realloc(v->str_val, v->alloced_len+1);
      v->str_val= tmpptr;
    }
    v->str_val_len = new_val_len;
    memcpy(v->str_val, p, new_val_len);
    v->str_val[new_val_len] = 0;
    v->int_val=atoi(p);
    v->int_dirty=0;
  }
  return;
}


static void open_file(const char *name)
{
  char buff[FN_REFLEN];

  if (!internal::test_if_hard_path(name))
  {
    snprintf(buff, sizeof(buff), "%s%s",opt_basedir.c_str(),name);
    name=buff;
  }
  internal::fn_format(buff, name, "", "", MY_UNPACK_FILENAME);

  cur_file++;
  if (cur_file == &*file_stack.end())
    die("Source directives are nesting too deep");
  if (!(cur_file->file= fopen(buff, "r")))
  {
    cur_file--;
    die("Could not open '%s' for reading", buff);
  }
  cur_file->file_name= strdup(buff);
  cur_file->lineno=1;
}


/*
  Source and execute the given file

  SYNOPSIS
  do_source()
  query  called command

  DESCRIPTION
  source <file_name>

  Open the file <file_name> and execute it

*/

static void do_source(st_command* command)
{
  string ds_filename;
  const struct command_arg source_args[] = {
    { "filename", ARG_STRING, true, &ds_filename, "File to source" }
  };


  check_command_args(command, command->first_argument, source_args,
                     sizeof(source_args)/sizeof(struct command_arg),
                     ' ');

  /*
    If this file has already been sourced, don't source it again.
    It's already available in the q_lines cache.
  */
  if (parser.current_line < (parser.read_lines - 1))
    ; /* Do nothing */
  else
  {
    if (! opt_testdir.empty())
    {
      string testdir(opt_testdir);
      if (testdir[testdir.length()] != '/')
        testdir += "/";
      testdir += ds_filename;
      ds_filename.swap(testdir);
    }
    open_file(ds_filename.c_str());
  }
}


static void init_builtin_echo()
{
  builtin_echo[0]= 0;
}


/*
  Replace a substring

  SYNOPSIS
  replace
  ds_str      The string to search and perform the replace in
  search_str  The string to search for
  search_len  Length of the string to search for
  replace_str The string to replace with
  replace_len Length of the string to replace with

  RETURN
  0 String replaced
  1 Could not find search_str in str
*/

static int replace(string *ds_str,
                   const char *search_str, uint32_t search_len,
                   const char *replace_str, uint32_t replace_len)
{
  string ds_tmp;
  const char *start= strstr(ds_str->c_str(), search_str);
  if (!start)
    return 1;
  ds_tmp.append(ds_str->c_str(), start - ds_str->c_str());
  ds_tmp.append(replace_str, replace_len);
  ds_tmp.append(start + search_len);
  *ds_str= ds_tmp;
  return 0;
}


/*
  Execute given command.

  SYNOPSIS
  do_exec()
  query  called command

  DESCRIPTION
  exec <command>

  Execute the text between exec and end of line in a subprocess.
  The error code returned from the subprocess is checked against the
  expected error array, previously set with the --error command.
  It can thus be used to execute a command that shall fail.

  NOTE
  Although drizzletest is executed from cygwin shell, the command will be
  executed in "cmd.exe". Thus commands like "rm" etc can NOT be used, use
  drizzletest commmand(s) like "remove_file" for that
*/

static void do_exec(st_command* command)
{
  int error;
  char buf[512];
  FILE *res_file;
  char *cmd= command->first_argument;
  string ds_cmd;

  /* Skip leading space */
  while (*cmd && my_isspace(charset_info, *cmd))
    cmd++;
  if (!*cmd)
    die("Missing argument in exec");
  command->last_argument= command->end;

  /* Eval the command, thus replacing all environment variables */
  do_eval(&ds_cmd, cmd, command->end, !is_windows);

  /* Check if echo should be replaced with "builtin" echo */
  if (builtin_echo[0] && strncmp(cmd, "echo", 4) == 0)
  {
    /* Replace echo with our "builtin" echo */
    replace(&ds_cmd, "echo", 4, builtin_echo, strlen(builtin_echo));
  }

  if (!(res_file= popen(ds_cmd.c_str(), "r")) && command->abort_on_error)
  {
    die("popen(\"%s\", \"r\") failed", command->first_argument);
  }

  while (fgets(buf, sizeof(buf), res_file))
  {
    if (disable_result_log)
    {
      buf[strlen(buf)-1]=0;
    }
    else
    {
      replace_append(&ds_res, buf);
    }
  }
  error= pclose(res_file);
  if (error > 0)
  {
    uint32_t status= WEXITSTATUS(error), i;
    bool ok= 0;

    if (command->abort_on_error)
    {
      log_msg("exec of '%s' failed, error: %d, status: %d, errno: %d", ds_cmd.c_str(), error, status, errno);
      die("command \"%s\" failed", command->first_argument);
    }

    for (i= 0; i < command->expected_errors.count; i++)
    {
      if ((command->expected_errors.err[i].type == ERR_ERRNO) &&
          (command->expected_errors.err[i].code.errnum == status))
      {
        ok= 1;
      }
    }
    if (!ok)
    {
      die("command \"%s\" failed with wrong error: %d",
          command->first_argument, status);
    }
  }
  else if (command->expected_errors.err[0].type == ERR_ERRNO &&
           command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    log_msg("exec of '%s failed, error: %d, errno: %d", ds_cmd.c_str(), error, errno);
    die("command \"%s\" succeeded - should have failed with errno %d...",
        command->first_argument, command->expected_errors.err[0].code.errnum);
  }

  return;
}

enum enum_operator
{
  DO_DEC,
  DO_INC
};


/*
  Decrease or increase the value of a variable

  SYNOPSIS
  do_modify_var()
  query  called command
  operator    operation to perform on the var

  DESCRIPTION
  dec $var_name
  inc $var_name

*/

static int do_modify_var(st_command* command,
                         enum enum_operator op)
{
  const char *p= command->first_argument;
  VAR* v;
  if (!*p)
    die("Missing argument to %.*s", command->first_word_len, command->query);
  if (*p != '$')
    die("The argument to %.*s must be a variable (start with $)",
        command->first_word_len, command->query);
  v= var_get(p, &p, 1, 0);
  switch (op) {
  case DO_DEC:
    v->int_val--;
    break;
  case DO_INC:
    v->int_val++;
    break;
  default:
    die("Invalid operator to do_modify_var");
    break;
  }
  v->int_dirty= 1;
  command->last_argument= (char*)++p;
  return 0;
}


/*
  SYNOPSIS
  do_system
  command  called command

  DESCRIPTION
  system <command>

  Eval the query to expand any $variables in the command.
  Execute the command with the "system" command.

*/

static void do_system(st_command* command)
{
  string ds_cmd;


  if (strlen(command->first_argument) == 0)
    die("Missing arguments to system, nothing to do!");

  /* Eval the system command, thus replacing all environment variables */
  do_eval(&ds_cmd, command->first_argument, command->end, !is_windows);

  if (system(ds_cmd.c_str()))
  {
    if (command->abort_on_error)
      die("system command '%s' failed", command->first_argument);

    /* If ! abort_on_error, log message and continue */
    ds_res += "system command '";
    replace_append(&ds_res, command->first_argument);
    ds_res += "' failed\n";
  }

  command->last_argument= command->end;
  return;
}


/*
  SYNOPSIS
  do_remove_file
  command  called command

  DESCRIPTION
  remove_file <file_name>
  Remove the file <file_name>
*/

static void do_remove_file(st_command* command)
{
  string ds_filename;
  const struct command_arg rm_args[] = {
    { "filename", ARG_STRING, true, &ds_filename, "File to delete" }
  };


  check_command_args(command, command->first_argument,
                     rm_args, sizeof(rm_args)/sizeof(struct command_arg),
                     ' ');

  int error= internal::my_delete(ds_filename.c_str(), MYF(0)) != 0;
  handle_command_error(command, error);
}


/*
  SYNOPSIS
  do_copy_file
  command  command handle

  DESCRIPTION
  copy_file <from_file> <to_file>
  Copy <from_file> to <to_file>

  NOTE! Will fail if <to_file> exists
*/

static void do_copy_file(st_command* command)
{
  string ds_from_file;
  string ds_to_file;
  const struct command_arg copy_file_args[] = {
    { "from_file", ARG_STRING, true, &ds_from_file, "Filename to copy from" },
    { "to_file", ARG_STRING, true, &ds_to_file, "Filename to copy to" }
  };


  check_command_args(command, command->first_argument,
                     copy_file_args,
                     sizeof(copy_file_args)/sizeof(struct command_arg),
                     ' ');

  int error= (internal::my_copy(ds_from_file.c_str(), ds_to_file.c_str(),
                  MYF(MY_DONT_OVERWRITE_FILE)) != 0);
  handle_command_error(command, error);
}


/*
  SYNOPSIS
  do_chmod_file
  command  command handle

  DESCRIPTION
  chmod <octal> <file_name>
  Change file permission of <file_name>

*/

static void do_chmod_file(st_command* command)
{
  long mode= 0;
  string ds_mode;
  string ds_file;
  const struct command_arg chmod_file_args[] = {
    { "mode", ARG_STRING, true, &ds_mode, "Mode of file(octal) ex. 0660"},
    { "filename", ARG_STRING, true, &ds_file, "Filename of file to modify" }
  };


  check_command_args(command, command->first_argument,
                     chmod_file_args,
                     sizeof(chmod_file_args)/sizeof(struct command_arg),
                     ' ');

  /* Parse what mode to set */
  istringstream buff(ds_mode);
  if (ds_mode.length() != 4 ||
      (buff >> oct >> mode).fail())
    die("You must write a 4 digit octal number for mode");

  handle_command_error(command, chmod(ds_file.c_str(), mode));
}


/*
  SYNOPSIS
  do_file_exists
  command  called command

  DESCRIPTION
  fiile_exist <file_name>
  Check if file <file_name> exists
*/

static void do_file_exist(st_command* command)
{
  string ds_filename;
  const struct command_arg file_exist_args[] = {
    { "filename", ARG_STRING, true, &ds_filename, "File to check if it exist" }
  };


  check_command_args(command, command->first_argument,
                     file_exist_args,
                     sizeof(file_exist_args)/sizeof(struct command_arg),
                     ' ');

  int error= access(ds_filename.c_str(), F_OK) != 0;
  handle_command_error(command, error);
}


/*
  SYNOPSIS
  do_mkdir
  command  called command

  DESCRIPTION
  mkdir <dir_name>
  Create the directory <dir_name>
*/

static void do_mkdir(st_command* command)
{
  string ds_dirname;
  const struct command_arg mkdir_args[] = {
    {"dirname", ARG_STRING, true, &ds_dirname, "Directory to create"}
  };


  check_command_args(command, command->first_argument,
                     mkdir_args, sizeof(mkdir_args)/sizeof(struct command_arg),
                     ' ');

  int error= mkdir(ds_dirname.c_str(), (0777 & internal::my_umask_dir)) != 0;
  handle_command_error(command, error);
}

/*
  SYNOPSIS
  do_rmdir
  command  called command

  DESCRIPTION
  rmdir <dir_name>
  Remove the empty directory <dir_name>
*/

static void do_rmdir(st_command* command)
{
  string ds_dirname;
  const struct command_arg rmdir_args[] = {
    {"dirname", ARG_STRING, true, &ds_dirname, "Directory to remove"}
  };


  check_command_args(command, command->first_argument,
                     rmdir_args, sizeof(rmdir_args)/sizeof(struct command_arg),
                     ' ');

  int error= rmdir(ds_dirname.c_str()) != 0;
  handle_command_error(command, error);
}


/*
  Read characters from line buffer or file. This is needed to allow
  my_ungetc() to buffer MAX_DELIMITER_LENGTH characters for a file

  NOTE:
  This works as long as one doesn't change files (with 'source file_name')
  when there is things pushed into the buffer.  This should however not
  happen for any tests in the test suite.
*/

static int my_getc(FILE *file)
{
  if (line_buffer_pos == line_buffer)
    return fgetc(file);
  return *--line_buffer_pos;
}


static void my_ungetc(int c)
{
  *line_buffer_pos++= (char) c;
}


static void read_until_delimiter(string *ds,
                                 string *ds_delimiter)
{
  if (ds_delimiter->length() > MAX_DELIMITER_LENGTH)
    die("Max delimiter length(%d) exceeded", MAX_DELIMITER_LENGTH);

  /* Read from file until delimiter is found */
  while (1)
  {
    char c= my_getc(cur_file->file);

    if (c == '\n')
    {
      cur_file->lineno++;

      /* Skip newline from the same line as the command */
      if (start_lineno == (cur_file->lineno - 1))
        continue;
    }
    else if (start_lineno == cur_file->lineno)
    {
      /*
        No characters except \n are allowed on
        the same line as the command
      */
      die("Trailing characters found after command");
    }

    if (feof(cur_file->file))
      die("End of file encountered before '%s' delimiter was found",
          ds_delimiter->c_str());

    if (match_delimiter(c, ds_delimiter->c_str(), ds_delimiter->length()))
      break;

    ds->push_back(c);
  }
}


static void do_write_file_command(st_command* command, bool append)
{
  string ds_content;
  string ds_filename;
  string ds_delimiter;
  const struct command_arg write_file_args[] = {
    { "filename", ARG_STRING, true, &ds_filename, "File to write to" },
    { "delimiter", ARG_STRING, false, &ds_delimiter, "Delimiter to read until" }
  };


  check_command_args(command,
                     command->first_argument,
                     write_file_args,
                     sizeof(write_file_args)/sizeof(struct command_arg),
                     ' ');

  /* If no delimiter was provided, use EOF */
  if (ds_delimiter.length() == 0)
    ds_delimiter += "EOF";

  if (!append && access(ds_filename.c_str(), F_OK) == 0)
  {
    /* The file should not be overwritten */
    die("File already exist: '%s'", ds_filename.c_str());
  }

  read_until_delimiter(&ds_content, &ds_delimiter);
  str_to_file2(ds_filename.c_str(), ds_content.c_str(), ds_content.length(), append);
}


/*
  SYNOPSIS
  do_write_file
  command  called command

  DESCRIPTION
  write_file <file_name> [<delimiter>];
  <what to write line 1>
  <...>
  < what to write line n>
  EOF

  --write_file <file_name>;
  <what to write line 1>
  <...>
  < what to write line n>
  EOF

  Write everything between the "write_file" command and 'delimiter'
  to "file_name"

  NOTE! Will fail if <file_name> exists

  Default <delimiter> is EOF

*/

static void do_write_file(st_command* command)
{
  do_write_file_command(command, false);
}


/*
  SYNOPSIS
  do_append_file
  command  called command

  DESCRIPTION
  append_file <file_name> [<delimiter>];
  <what to write line 1>
  <...>
  < what to write line n>
  EOF

  --append_file <file_name>;
  <what to write line 1>
  <...>
  < what to write line n>
  EOF

  Append everything between the "append_file" command
  and 'delimiter' to "file_name"

  Default <delimiter> is EOF

*/

static void do_append_file(st_command* command)
{
  do_write_file_command(command, true);
}


/*
  SYNOPSIS
  do_cat_file
  command  called command

  DESCRIPTION
  cat_file <file_name>;

  Print the given file to result log

*/

static void do_cat_file(st_command* command)
{
  static string ds_filename;
  const struct command_arg cat_file_args[] = {
    { "filename", ARG_STRING, true, &ds_filename, "File to read from" }
  };


  check_command_args(command,
                     command->first_argument,
                     cat_file_args,
                     sizeof(cat_file_args)/sizeof(struct command_arg),
                     ' ');

  cat_file(ds_res, ds_filename.c_str());
}


/*
  SYNOPSIS
  do_diff_files
  command  called command

  DESCRIPTION
  diff_files <file1> <file2>;

  Fails if the two files differ.

*/

static void do_diff_files(st_command* command)
{
  string ds_filename;
  string ds_filename2;
  const struct command_arg diff_file_args[] = {
    { "file1", ARG_STRING, true, &ds_filename, "First file to diff" },
    { "file2", ARG_STRING, true, &ds_filename2, "Second file to diff" }
  };


  check_command_args(command,
                     command->first_argument,
                     diff_file_args,
                     sizeof(diff_file_args)/sizeof(struct command_arg),
                     ' ');

  int error= compare_files(ds_filename.c_str(), ds_filename2.c_str());
  if (error)
  {
    /* Compare of the two files failed, append them to output
       so the failure can be analyzed
    */
    show_diff(&ds_res, ds_filename.c_str(), ds_filename2.c_str());
  }

  handle_command_error(command, error);
}

/*
  SYNOPSIS
  do_send_quit
  command  called command

  DESCRIPTION
  Sends a simple quit command to the server for the named connection.

*/

static void do_send_quit(st_command* command)
{
  char* p= command->first_argument;

  if (not *p)
    die("Missing connection name in send_quit");
  char* name= p;
  while (*p && !my_isspace(charset_info, *p))
    p++;

  if (*p)
    *p++= 0;
  command->last_argument= p;

  st_connection* con= find_ptr2(g_connections, name);
  if (not con)
    die("connection '%s' not found in connection pool", name);

  drizzle::result_c result;
  drizzle_return_t ret;
  drizzle_quit(*con, &result.b_, &ret);
}


/*
  SYNOPSIS
  do_change_user
  command       called command

  DESCRIPTION
  change_user [<user>], [<passwd>], [<db>]
  <user> - user to change to
  <passwd> - user password
  <db> - default database

  Changes the user and causes the database specified by db to become
  the default (current) database for the the current connection.

*/

static void do_change_user(st_command *)
{
  assert(0);
}

/*
  SYNOPSIS
  do_perl
  command  command handle

  DESCRIPTION
  perl [<delimiter>];
  <perlscript line 1>
  <...>
  <perlscript line n>
  EOF

  Execute everything after "perl" until <delimiter> as perl.
  Useful for doing more advanced things
  but still being able to execute it on all platforms.

  Default <delimiter> is EOF
*/

static void do_perl(st_command* command)
{
  char buf[FN_REFLEN];
  char temp_file_path[FN_REFLEN];
  string ds_script;
  string ds_delimiter;
  const command_arg perl_args[] = {
    { "delimiter", ARG_STRING, false, &ds_delimiter, "Delimiter to read until" }
  };


  check_command_args(command,
                     command->first_argument,
                     perl_args,
                     sizeof(perl_args)/sizeof(struct command_arg),
                     ' ');

  /* If no delimiter was provided, use EOF */
  if (ds_delimiter.length() == 0)
    ds_delimiter += "EOF";

  read_until_delimiter(&ds_script, &ds_delimiter);

  /* Create temporary file name */
  int fd= internal::create_temp_file(temp_file_path, getenv("MYSQLTEST_VARDIR"), "tmp", MYF(MY_WME));
  if (fd < 0)
    die("Failed to create temporary file for perl command");
  internal::my_close(fd, MYF(0));

  str_to_file(temp_file_path, ds_script.c_str(), ds_script.length());

  /* Format the "perl <filename>" command */
  snprintf(buf, sizeof(buf), "perl %s", temp_file_path);

  FILE* res_file= popen(buf, "r");
  if (not res_file && command->abort_on_error)
    die("popen(\"%s\", \"r\") failed", buf);

  while (fgets(buf, sizeof(buf), res_file))
  {
    if (disable_result_log)
      buf[strlen(buf)-1]=0;
    else
      replace_append(&ds_res, buf);
  }
  int error= pclose(res_file);

  /* Remove the temporary file */
  internal::my_delete(temp_file_path, MYF(0));

  handle_command_error(command, WEXITSTATUS(error));
}


/*
  Print the content between echo and <delimiter> to result file.
  Evaluate all variables in the string before printing, allow
  for variable names to be escaped using        \

  SYNOPSIS
  do_echo()
  command  called command

  DESCRIPTION
  echo text
  Print the text after echo until end of command to result file

  echo $<var_name>
  Print the content of the variable <var_name> to result file

  echo Some text $<var_name>
  Print "Some text" plus the content of the variable <var_name> to
  result file

  echo Some text \$<var_name>
  Print "Some text" plus $<var_name> to result file
*/

static void do_echo(st_command* command)
{
  string ds_echo;
  do_eval(&ds_echo, command->first_argument, command->end, false);
  ds_res += ds_echo;
  ds_res += "\n";
  command->last_argument= command->end;
}

static void do_wait_for_slave_to_stop()
{
  static int SLAVE_POLL_INTERVAL= 300000;
  drizzle::connection_c& con= *cur_con;
  for (;;)
  {
    drizzle::result_c res;
    dt_query(con, res, "show status like 'Slave_running'");
    drizzle_row_t row= res.row_next();
    if (!row || !row[1])
    {
      die("Strange result from query while probing slave for stop");
    }
    if (!strcmp(row[1], "OFF"))
      break;
    usleep(SLAVE_POLL_INTERVAL);
  }
}

static void do_sync_with_master2(long offset)
{
  drizzle::connection_c& con= *cur_con;
  char query_buf[FN_REFLEN+128];
  int tries= 0;

  if (!master_pos.file[0])
    die("Calling 'sync_with_master' without calling 'save_master_pos'");

  snprintf(query_buf, sizeof(query_buf), "select master_pos_wait('%s', %ld)", master_pos.file,
          master_pos.pos + offset);

wait_for_position:

  drizzle::result_c res;
  dt_query(con, res, query_buf);

  drizzle_row_t row= res.row_next();
  if (!row)
  {
    die("empty result in %s", query_buf);
  }
  if (!row[0])
  {
    /*
      It may be that the slave SQL thread has not started yet, though START
      SLAVE has been issued ?
    */
    if (tries++ == 30)
    {
      show_query(con, "SHOW MASTER STATUS");
      show_query(con, "SHOW SLAVE STATUS");
      die("could not sync with master ('%s' returned NULL)", query_buf);
    }
    sleep(1); /* So at most we will wait 30 seconds and make 31 tries */
    goto wait_for_position;
  }
}

static void do_sync_with_master(st_command* command)
{
  long offset= 0;
  char *p= command->first_argument;
  const char *offset_start= p;
  if (*offset_start)
  {
    for (; my_isdigit(charset_info, *p); p++)
      offset = offset * 10 + *p - '0';

    if(*p && !my_isspace(charset_info, *p))
      die("Invalid integer argument \"%s\"", offset_start);
    command->last_argument= p;
  }
  do_sync_with_master2(offset);
  return;
}


/*
  when ndb binlog is on, this call will wait until last updated epoch
  (locally in the drizzled) has been received into the binlog
*/
static void do_save_master_pos()
{
  drizzle::result_c res;
  dt_query(*cur_con, res, "show master status");
  drizzle_row_t row= res.row_next();
  if (!row)
    die("empty result in show master status");
  strncpy(master_pos.file, row[0], sizeof(master_pos.file)-1);
  master_pos.pos = strtoul(row[1], (char**) 0, 10);
}


/*
  Assign the variable <var_name> with <var_val>

  SYNOPSIS
  do_let()
  query  called command

  DESCRIPTION
  let $<var_name>=<var_val><delimiter>

  <var_name>  - is the string string found between the $ and =
  <var_val>   - is the content between the = and <delimiter>, it may span
  multiple line and contain any characters except <delimiter>
  <delimiter> - is a string containing of one or more chars, default is ;

  RETURN VALUES
  Program will die if error detected
*/

static void do_let(st_command* command)
{
  char *p= command->first_argument;
  char *var_name, *var_name_end;
  string let_rhs_expr;


  /* Find <var_name> */
  if (!*p)
    die("Missing arguments to let");
  var_name= p;
  while (*p && (*p != '=') && !my_isspace(charset_info,*p))
    p++;
  var_name_end= p;
  if (var_name == var_name_end ||
      (var_name+1 == var_name_end && *var_name == '$'))
    die("Missing variable name in let");
  while (my_isspace(charset_info,*p))
    p++;
  if (*p++ != '=')
    die("Missing assignment operator in let");

  /* Find start of <var_val> */
  while (*p && my_isspace(charset_info,*p))
    p++;

  do_eval(&let_rhs_expr, p, command->end, false);

  command->last_argument= command->end;
  /* Assign var_val to var_name */
  var_set(var_name, var_name_end, let_rhs_expr.c_str(),
          (let_rhs_expr.c_str() + let_rhs_expr.length()));
  return;
}


/*
  Sleep the number of specified seconds

  SYNOPSIS
  do_sleep()
  q         called command
  real_sleep   use the value from opt_sleep as number of seconds to sleep
  if real_sleep is false

  DESCRIPTION
  sleep <seconds>
  real_sleep <seconds>

  The difference between the sleep and real_sleep commands is that sleep
  uses the delay from the --sleep command-line option if there is one.
  (If the --sleep option is not given, the sleep command uses the delay
  specified by its argument.) The real_sleep command always uses the
  delay specified by its argument.  The logic is that sometimes delays are
  cpu-dependent, and --sleep can be used to set this delay.  real_sleep is
  used for cpu-independent delays.
*/

static void do_sleep(st_command* command, bool real_sleep)
{
  char *p= command->first_argument;
  char *sleep_start, *sleep_end= command->end;
  double sleep_val= 0;

  while (my_isspace(charset_info, *p))
    p++;
  if (!*p)
    die("Missing argument to %.*s", command->first_word_len, command->query);
  sleep_start= p;
  /* Check that arg starts with a digit, not handled by internal::my_strtod */
  if (!my_isdigit(charset_info, *sleep_start))
    die("Invalid argument to %.*s \"%s\"", command->first_word_len,
        command->query,command->first_argument);
  string buff_str(sleep_start, sleep_end-sleep_start);
  istringstream buff(buff_str);
  buff >> sleep_val;
  if (buff.fail())
    die("Invalid argument to %.*s \"%s\"", command->first_word_len, command->query, command->first_argument);

  /* Fixed sleep time selected by --sleep option */
  if (opt_sleep >= 0 && !real_sleep)
    sleep_val= opt_sleep;

  if (sleep_val)
    usleep(sleep_val * 1000000);
  command->last_argument= sleep_end;
}


static void do_get_file_name(st_command* command, string &dest)
{
  char *p= command->first_argument;
  if (!*p)
    die("Missing file name argument");
  char *name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  if (*p)
    *p++= 0;
  command->last_argument= p;
  if (! opt_testdir.empty())
  {
    dest= opt_testdir;
    if (dest[dest.length()] != '/')
      dest += "/";
  }
  dest.append(name);
}


static void do_set_charset(st_command* command)
{
  char *charset_name= command->first_argument;
  char *p;

  if (!charset_name || !*charset_name)
    die("Missing charset name in 'character_set'");
  /* Remove end space */
  p= charset_name;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  if(*p)
    *p++= 0;
  command->last_argument= p;
  charset_info= get_charset_by_csname(charset_name, MY_CS_PRIMARY);
  if (!charset_info)
    abort_not_supported_test("Test requires charset '%s'", charset_name);
}

static void fill_global_error_names()
{
  drizzle::connection_c& con= *cur_con;

  global_error_names.clear();

  drizzle::result_c res;
  dt_query(con, res, "select error_name, error_code from data_dictionary.errors");
  while (drizzle_row_t row= res.row_next())
  {
    if (not row[0])
      break;
    /*
      Concatenate all fields in the first row with tab in between
      and assign that string to the $variable
    */
    size_t *lengths= res.row_field_sizes();
    try
    {
      global_error_names[string(row[0], lengths[0])] = boost::lexical_cast<uint32_t>(string(row[1], lengths[1]));
    }
    catch (boost::bad_lexical_cast &ex)
    {
      die("Invalid error_code from Drizzle: %s", ex.what());
    }
  }
}

static uint32_t get_errcode_from_name(const char *error_name, const char *error_end)
{
  string error_name_s(error_name, error_end);

  if (ErrorCodes::mapped_type* ptr= find_ptr(global_error_names, error_name_s))
    return *ptr;

  die("Unknown SQL error name '%s'", error_name_s.c_str());
  return 0;
}

static void do_get_errcodes(st_command* command)
{
  struct st_match_err *to= saved_expected_errors.err;
  char *p= command->first_argument;
  uint32_t count= 0;



  if (!*p)
    die("Missing argument(s) to 'error'");

  do
  {
    char *end;

    /* Skip leading spaces */
    while (*p && *p == ' ')
      p++;

    /* Find end */
    end= p;
    while (*end && *end != ',' && *end != ' ')
      end++;

    if (*p == 'S')
    {
      char *to_ptr= to->code.sqlstate;

      /*
        SQLSTATE string
        - Must be DRIZZLE_MAX_SQLSTATE_SIZE long
        - May contain only digits[0-9] and _uppercase_ letters
      */
      p++; /* Step past the S */
      if ((end - p) != DRIZZLE_MAX_SQLSTATE_SIZE)
        die("The sqlstate must be exactly %d chars long", DRIZZLE_MAX_SQLSTATE_SIZE);

      /* Check sqlstate string validity */
      while (*p && p < end)
      {
        if (my_isdigit(charset_info, *p) || my_isupper(charset_info, *p))
          *to_ptr++= *p++;
        else
          die("The sqlstate may only consist of digits[0-9] "   \
              "and _uppercase_ letters");
      }

      *to_ptr= 0;
      to->type= ERR_SQLSTATE;
    }
    else if (*p == 's')
    {
      die("The sqlstate definition must start with an uppercase S");
    }
    else if (*p == 'E')
    {
      /* Error name string */

      to->code.errnum= get_errcode_from_name(p, end);
      to->type= ERR_ERRNO;
    }
    else if (*p == 'e')
    {
      die("The error name definition must start with an uppercase E");
    }
    else if (*p == 'H')
    {
      /* Error name string */

      to->code.errnum= get_errcode_from_name(p, end);
      to->type= ERR_ERRNO;
    }
    else
    {
      die ("You must either use the SQLSTATE or built in drizzle error label, numbers are not accepted");
    }
    to++;
    count++;

    if (count >= (sizeof(saved_expected_errors.err) /
                  sizeof(struct st_match_err)))
      die("Too many errorcodes specified");

    /* Set pointer to the end of the last error code */
    p= end;

    /* Find next ',' */
    while (*p && *p != ',')
      p++;

    if (*p)
      p++; /* Step past ',' */

  } while (*p);

  command->last_argument= p;
  to->type= ERR_EMPTY;                        /* End of data */

  saved_expected_errors.count= count;
  return;
}


/*
  Get a string;  Return ptr to end of string
  Strings may be surrounded by " or '

  If string is a '$variable', return the value of the variable.
*/

static char *get_string(char **to_ptr, char **from_ptr,
                        st_command* command)
{
  char c, sep;
  char *to= *to_ptr, *from= *from_ptr, *start=to;


  /* Find separator */
  if (*from == '"' || *from == '\'')
    sep= *from++;
  else
    sep=' ';        /* Separated with space */

  for ( ; (c=*from) ; from++)
  {
    if (c == '\\' && from[1])
    {          /* Escaped character */
      /* We can't translate \0 -> ASCII 0 as replace can't handle ASCII 0 */
      switch (*++from) {
      case 'n':
        *to++= '\n';
        break;
      case 't':
        *to++= '\t';
        break;
      case 'r':
        *to++ = '\r';
        break;
      case 'b':
        *to++ = '\b';
        break;
      case 'Z':        /* ^Z must be escaped on Win32 */
        *to++='\032';
        break;
      default:
        *to++ = *from;
        break;
      }
    }
    else if (c == sep)
    {
      if (c == ' ' || c != *++from)
        break;        /* Found end of string */
      *to++=c;        /* Copy duplicated separator */
    }
    else
      *to++=c;
  }
  if (*from != ' ' && *from)
    die("Wrong string argument in %s", command->query);

  while (my_isspace(charset_info,*from))  /* Point to next string */
    from++;

  *to =0;        /* End of string marker */
  *to_ptr= to+1;      /* Store pointer to end */
  *from_ptr= from;

  /* Check if this was a variable */
  if (*start == '$')
  {
    const char *end= to;
    VAR *var=var_get(start, &end, 0, 1);
    if (var && to == (char*) end+1)
      return(var->str_val);  /* return found variable value */
  }
  return(start);
}


static void set_reconnect(drizzle_con_st *con, int val)
{
  (void) con;
  (void) val;
/* XXX
  bool reconnect= val;

  drizzleclient_options(drizzle, DRIZZLE_OPT_RECONNECT, (char *)&reconnect);
*/
}


static void select_connection_name(const char *name)
{
  if (!(cur_con= find_ptr2(g_connections, name)))
    die("connection '%s' not found in connection pool", name);

  /* Update $drizzleclient_get_server_version to that of current connection */
  var_set_drizzleclient_get_server_version(*cur_con);
}


static void select_connection(st_command* command)
{
  char *p= command->first_argument;
  if (!*p)
    die("Missing connection name in connect");
  char* name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  if (*p)
    *p++= 0;
  command->last_argument= p;
  select_connection_name(name);
}


static void do_close_connection(st_command* command)
{
  char* p= command->first_argument;
  if (!*p)
    die("Missing connection name in disconnect");
  char* name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;

  if (*p)
    *p++= 0;
  command->last_argument= p;

  st_connection* con= find_ptr2(g_connections, name);
  if (!con)
    die("connection '%s' not found in connection pool", name);
  g_connections.erase(name);
  delete con;
}


/*
  Connect to a server doing several retries if needed.

  SYNOPSIS
  safe_connect()
  con               - connection structure to be used
  host, user, pass, - connection parameters
  db, port, sock

  NOTE

  Sometimes in a test the client starts before
  the server - to solve the problem, we try again
  after some sleep if connection fails the first
  time

  This function will try to connect to the given server
  "opt_max_connect_retries" times and sleep "connection_retry_sleep"
  seconds between attempts before finally giving up.
  This helps in situation when the client starts
  before the server (which happens sometimes).
  It will only ignore connection errors during these retries.

*/

static st_connection* safe_connect(const char *name, const string host, const string user, const char *pass, const string db, uint32_t port)
{
  uint32_t failed_attempts= 0;
  st_connection* con0= new st_connection;
  drizzle_con_st* con= *con0;
  drizzle_con_set_tcp(con, host.c_str(), port);
  drizzle_con_set_auth(con, user.c_str(), pass);
  drizzle_con_set_db(con, db.c_str());
  while (drizzle_return_t ret= drizzle_con_connect(con))
  {
    /*
      Connect failed

      Only allow retry if this was an error indicating the server
      could not be contacted. Error code differs depending
      on protocol/connection type
    */

    if ((ret == DRIZZLE_RETURN_GETADDRINFO ||
         ret == DRIZZLE_RETURN_COULD_NOT_CONNECT) &&
        failed_attempts < opt_max_connect_retries)
    {
      verbose_msg("Connect attempt %d/%d failed: %d: %s", failed_attempts, opt_max_connect_retries, ret, drizzle_con_error(con));
      usleep(100000);
    }
    else
    {
      if (failed_attempts > 0)
        die("Could not open connection '%s' after %d attempts: %d %s", name, failed_attempts, ret, drizzle_con_error(con));
      else
        die("Could not open connection '%s': %d %s", name, ret, drizzle_con_error(con));
    }
    failed_attempts++;
  }
  return con0;
}


/*
  Connect to a server and handle connection errors in case they occur.

  SYNOPSIS
  connect_n_handle_errors()
  q                 - context of connect "query" (command)
  con               - connection structure to be used
  host, user, pass, - connection parameters
  db, port, sock

  DESCRIPTION
  This function will try to establish a connection to server and handle
  possible errors in the same manner as if "connect" was usual SQL-statement
  (If error is expected it will ignore it once it occurs and log the
  "statement" to the query log).
  Unlike safe_connect() it won't do several attempts.

  RETURN VALUES
  1 - Connected
  0 - Not connected

*/

static int connect_n_handle_errors(st_command* command,
                                   drizzle_con_st *con, const char* host,
                                   const char* user, const char* pass,
                                   const char* db, int port, const char* sock)
{
  /* Only log if an error is expected */
  if (!command->abort_on_error &&
      !disable_query_log)
  {
    /*
      Log the connect to result log
    */
    ds_res += "connect(";
    replace_append(&ds_res, host);
    ds_res += ",";
    replace_append(&ds_res, user);
    ds_res += ",";
    replace_append(&ds_res, pass);
    ds_res += ",";
    if (db)
      replace_append(&ds_res, db);
    ds_res += ",";
    replace_append_uint(ds_res, port);
    ds_res += ",";
    if (sock)
      replace_append(&ds_res, sock);
    ds_res += ")";
    ds_res += delimiter;
    ds_res += "\n";
  }
  drizzle_con_set_tcp(con, host, port);
  drizzle_con_set_auth(con, user, pass);
  drizzle_con_set_db(con, db);
  if (drizzle_return_t ret= drizzle_con_connect(con))
  {
    if (ret == DRIZZLE_RETURN_HANDSHAKE_FAILED)
    {
      var_set_errno(drizzle_con_error_code(con));
      handle_error(command, drizzle_con_error_code(con), drizzle_con_error(con), drizzle_con_sqlstate(con), &ds_res);
    }
    else
    {
      var_set_errno(ret);
      handle_error(command, ret, drizzle_con_error(con), "", &ds_res);
    }
    return 0; /* Not connected */
  }
  var_set_errno(0);
  handle_no_error(command);
  return 1; /* Connected */
}


/*
  Open a new connection to DRIZZLE Server with the parameters
  specified. Make the new connection the current connection.

  SYNOPSIS
  do_connect()
  q         called command

  DESCRIPTION
  connect(<name>,<host>,<user>,[<pass>,[<db>,[<port>,<sock>[<opts>]]]]);
  connect <name>,<host>,<user>,[<pass>,[<db>,[<port>,<sock>[<opts>]]]];

  <name> - name of the new connection
  <host> - hostname of server
  <user> - user to connect as
  <pass> - password used when connecting
  <db>   - initial db when connected
  <port> - server port
  <sock> - server socket
  <opts> - options to use for the connection
  * SSL - use SSL if available
  * COMPRESS - use compression if available

  */

static void do_connect(st_command* command)
{
  uint32_t con_port= opt_port;

  string ds_connection_name;
  string ds_host;
  string ds_user;
  string ds_password;
  string ds_database;
  string ds_port;
  string ds_sock;
  string ds_options;
  const struct command_arg connect_args[] = {
    { "connection name", ARG_STRING, true, &ds_connection_name, "Name of the connection" },
    { "host", ARG_STRING, true, &ds_host, "Host to connect to" },
    { "user", ARG_STRING, false, &ds_user, "User to connect as" },
    { "passsword", ARG_STRING, false, &ds_password, "Password used when connecting" },
    { "database", ARG_STRING, false, &ds_database, "Database to select after connect" },
    { "port", ARG_STRING, false, &ds_port, "Port to connect to" },
    { "socket", ARG_STRING, false, &ds_sock, "Socket to connect with" },
    { "options", ARG_STRING, false, &ds_options, "Options to use while connecting" }
  };

  strip_parentheses(command);
  check_command_args(command, command->first_argument, connect_args,
                     sizeof(connect_args)/sizeof(struct command_arg),
                     ',');

  /* Port */
  if (ds_port.length())
  {
    con_port= atoi(ds_port.c_str());
    if (con_port == 0)
      die("Illegal argument for port: '%s'", ds_port.c_str());
  }

  /* Sock */
  if (!ds_sock.empty())
  {
    /*
      If the socket is specified just as a name without path
      append tmpdir in front
    */
    if (*ds_sock.c_str() != FN_LIBCHAR)
    {
      char buff[FN_REFLEN];
      internal::fn_format(buff, ds_sock.c_str(), TMPDIR, "", 0);
      ds_sock= buff;
    }
  }

  /* Options */
  const char* con_options= ds_options.c_str();
  while (*con_options)
  {
    /* Step past any spaces in beginning of option*/
    while (*con_options && my_isspace(charset_info, *con_options))
      con_options++;
    /* Find end of this option */
    const char* end= con_options;
    while (*end && !my_isspace(charset_info, *end))
      end++;
    die("Illegal option to connect: %.*s", (int) (end - con_options), con_options);
    /* Process next option */
    con_options= end;
  }

  if (find_ptr2(g_connections, ds_connection_name))
    die("Connection %s already exists", ds_connection_name.c_str());

  st_connection* con_slot= new st_connection;

  /* Use default db name */
  if (ds_database.empty())
    ds_database= opt_db;

  /* Special database to allow one to connect without a database name */
  if (ds_database == "*NO-ONE*")
    ds_database.clear();

  if (connect_n_handle_errors(command, *con_slot, ds_host.c_str(), ds_user.c_str(), 
    ds_password.c_str(), ds_database.c_str(), con_port, ds_sock.c_str()))
  {
    g_connections[ds_connection_name]= con_slot;
    cur_con= con_slot;
  }

  /* Update $drizzleclient_get_server_version to that of current connection */
  var_set_drizzleclient_get_server_version(*cur_con);
}


static void do_done(st_command* command)
{
  /* Check if empty block stack */
  if (cur_block == block_stack)
  {
    if (*command->query != '}')
      die("Stray 'end' command - end of block before beginning");
    die("Stray '}' - end of block before beginning");
  }

  /* Test if inner block has been executed */
  if (cur_block->ok && cur_block->cmd == cmd_while)
  {
    /* Pop block from stack, re-execute outer block */
    cur_block--;
    parser.current_line = cur_block->line;
  }
  else
  {
    /* Pop block from stack, goto next line */
    cur_block--;
    parser.current_line++;
  }
}


/*
  Process start of a "if" or "while" statement

  SYNOPSIS
  do_block()
  cmd        Type of block
  q         called command

  DESCRIPTION
  if ([!]<expr>)
  {
  <block statements>
  }

  while ([!]<expr>)
  {
  <block statements>
  }

  Evaluates the <expr> and if it evaluates to
  greater than zero executes the following code block.
  A '!' can be used before the <expr> to indicate it should
  be executed if it evaluates to zero.

*/

static void do_block(enum block_cmd cmd, st_command* command)
{
  char *p= command->first_argument;
  const char *expr_start, *expr_end;
  const char *cmd_name= (cmd == cmd_while ? "while" : "if");
  bool not_expr= false;

  /* Check stack overflow */
  if (cur_block == block_stack_end)
    die("Nesting too deeply");

  /* Set way to find outer block again, increase line counter */
  cur_block->line= parser.current_line++;

  /* If this block is ignored */
  if (!cur_block->ok)
  {
    /* Inner block should be ignored too */
    cur_block++;
    cur_block->cmd= cmd;
    cur_block->ok= false;
    return;
  }

  /* Parse and evaluate test expression */
  expr_start= strchr(p, '(');
  if (!expr_start++)
    die("missing '(' in %s", cmd_name);

  /* Check for !<expr> */
  if (*expr_start == '!')
  {
    not_expr= true;
    expr_start++; /* Step past the '!' */
  }
  /* Find ending ')' */
  expr_end= strrchr(expr_start, ')');
  if (!expr_end)
    die("missing ')' in %s", cmd_name);
  p= (char*)expr_end+1;

  while (*p && my_isspace(charset_info, *p))
    p++;
  if (*p && *p != '{')
    die("Missing '{' after %s. Found \"%s\"", cmd_name, p);

  VAR v;
  var_init(&v,0,0,0,0);
  eval_expr(&v, expr_start, &expr_end);

  /* Define inner block */
  cur_block++;
  cur_block->cmd= cmd;
  cur_block->ok= (v.int_val ? true : false);

  if (not_expr)
    cur_block->ok = !cur_block->ok;

  free(v.str_val);
  free(v.env_s);
}


static void do_delimiter(st_command* command)
{
  char* p= command->first_argument;

  while (*p && my_isspace(charset_info, *p))
    p++;

  if (!(*p))
    die("Can't set empty delimiter");

  strncpy(delimiter, p, sizeof(delimiter) - 1);
  delimiter_length= strlen(delimiter);

  command->last_argument= p + delimiter_length;
}


bool match_delimiter(int c, const char *delim, uint32_t length)
{
  uint32_t i;
  char tmp[MAX_DELIMITER_LENGTH];

  if (c != *delim)
    return 0;

  for (i= 1; i < length &&
         (c= my_getc(cur_file->file)) == *(delim + i);
       i++)
    tmp[i]= c;

  if (i == length)
    return 1;          /* Found delimiter */

  /* didn't find delimiter, push back things that we read */
  my_ungetc(c);
  while (i > 1)
    my_ungetc(tmp[--i]);
  return 0;
}


static bool end_of_query(int c)
{
  return match_delimiter(c, delimiter, delimiter_length);
}


/*
  Read one "line" from the file

  SYNOPSIS
  read_line
  buf     buffer for the read line
  size    size of the buffer i.e max size to read

  DESCRIPTION
  This function actually reads several lines and adds them to the
  buffer buf. It continues to read until it finds what it believes
  is a complete query.

  Normally that means it will read lines until it reaches the
  "delimiter" that marks end of query. Default delimiter is ';'
  The function should be smart enough not to detect delimiter's
  found inside strings surrounded with '"' and '\'' escaped strings.

  If the first line in a query starts with '#' or '-' this line is treated
  as a comment. A comment is always terminated when end of line '\n' is
  reached.

*/


static int my_strnncoll_simple(const charset_info_st * const  cs, const unsigned char *s, size_t slen,
                               const unsigned char *t, size_t tlen,
                               bool t_is_prefix)
{
  size_t len = ( slen > tlen ) ? tlen : slen;
  unsigned char *map= cs->sort_order;
  if (t_is_prefix && slen > tlen)
    slen=tlen;
  while (len--)
  {
    if (map[*s++] != map[*t++])
      return ((int) map[s[-1]] - (int) map[t[-1]]);
  }
  /*
    We can't use (slen - tlen) here as the result may be outside of the
    precision of a signed int
  */
  return slen > tlen ? 1 : slen < tlen ? -1 : 0 ;
}

static int read_line(char *buf, int size)
{
  char c, last_quote= 0;
  char *p= buf, *buf_end= buf + size - 1;
  int skip_char= 0;
  enum {R_NORMAL, R_Q, R_SLASH_IN_Q,
        R_COMMENT, R_LINE_START} state= R_LINE_START;


  start_lineno= cur_file->lineno;
  for (; p < buf_end ;)
  {
    skip_char= 0;
    c= my_getc(cur_file->file);
    if (feof(cur_file->file))
    {
  found_eof:
      if (cur_file->file != stdin)
      {
        fclose(cur_file->file);
        cur_file->file= 0;
      }
      free((unsigned char*) cur_file->file_name);
      cur_file->file_name= 0;
      if (cur_file == file_stack.data())
      {
        /* We're back at the first file, check if
           all { have matching }
        */
        if (cur_block != block_stack)
          die("Missing end of block");

        *p= 0;
        return(1);
      }
      cur_file--;
      start_lineno= cur_file->lineno;
      continue;
    }

    if (c == '\n')
    {
      /* Line counting is independent of state */
      cur_file->lineno++;

      /* Convert cr/lf to lf */
      if (p != buf && *(p-1) == '\r')
        p--;
    }

    switch(state) {
    case R_NORMAL:
      if (end_of_query(c))
      {
        *p= 0;
        return(0);
      }
      else if ((c == '{' &&
                (!my_strnncoll_simple(charset_info, (const unsigned char*) "while", 5,
                                      (unsigned char*) buf, min((ptrdiff_t)5, p - buf), 0) ||
                 !my_strnncoll_simple(charset_info, (const unsigned char*) "if", 2,
                                      (unsigned char*) buf, min((ptrdiff_t)2, p - buf), 0))))
      {
        /* Only if and while commands can be terminated by { */
        *p++= c;
        *p= 0;
        return(0);
      }
      else if (c == '\'' || c == '"' || c == '`')
      {
        last_quote= c;
        state= R_Q;
      }
      break;

    case R_COMMENT:
      if (c == '\n')
      {
        /* Comments are terminated by newline */
        *p= 0;
        return(0);
      }
      break;

    case R_LINE_START:
      if (c == '#' || c == '-')
      {
        /* A # or - in the first position of the line - this is a comment */
        state = R_COMMENT;
      }
      else if (my_isspace(charset_info, c))
      {
        /* Skip all space at begining of line */
        if (c == '\n')
        {
          /* Query hasn't started yet */
          start_lineno= cur_file->lineno;
        }
        skip_char= 1;
      }
      else if (end_of_query(c))
      {
        *p= 0;
        return(0);
      }
      else if (c == '}')
      {
        /* A "}" need to be by itself in the begining of a line to terminate */
        *p++= c;
        *p= 0;
        return(0);
      }
      else if (c == '\'' || c == '"' || c == '`')
      {
        last_quote= c;
        state= R_Q;
      }
      else
        state= R_NORMAL;
      break;

    case R_Q:
      if (c == last_quote)
        state= R_NORMAL;
      else if (c == '\\')
        state= R_SLASH_IN_Q;
      break;

    case R_SLASH_IN_Q:
      state= R_Q;
      break;

    }

    if (!skip_char)
    {
      /* Could be a multibyte character */
      /* This code is based on the code in "sql_load.cc" */
      int charlen = my_mbcharlen(charset_info, c);
      /* We give up if multibyte character is started but not */
      /* completed before we pass buf_end */
      if ((charlen > 1) && (p + charlen) <= buf_end)
      {
        int i;
        char* mb_start = p;

        *p++ = c;

        for (i= 1; i < charlen; i++)
        {
          if (feof(cur_file->file))
            goto found_eof;
          c= my_getc(cur_file->file);
          *p++ = c;
        }
        if (! my_ismbchar(charset_info, mb_start, p))
        {
          /* It was not a multiline char, push back the characters */
          /* We leave first 'c', i.e. pretend it was a normal char */
          while (p > mb_start)
            my_ungetc(*--p);
        }
      }
      else
        *p++= c;
    }
  }
  die("The input buffer is too small for this query.x\n"        \
      "check your query or increase MAX_QUERY and recompile");
  return(0);
}


/*
  Convert the read query to result format version 1

  That is: After newline, all spaces need to be skipped
  unless the previous char was a quote

  This is due to an old bug that has now been fixed, but the
  version 1 output format is preserved by using this function

*/

static void convert_to_format_v1(char* query)
{
  int last_c_was_quote= 0;
  char *p= query, *to= query;
  char *end= strchr(query, '\0');
  char last_c;

  while (p <= end)
  {
    if (*p == '\n' && !last_c_was_quote)
    {
      *to++ = *p++; /* Save the newline */

      /* Skip any spaces on next line */
      while (*p && my_isspace(charset_info, *p))
        p++;

      last_c_was_quote= 0;
    }
    else if (*p == '\'' || *p == '"' || *p == '`')
    {
      last_c= *p;
      *to++ = *p++;

      /* Copy anything until the next quote of same type */
      while (*p && *p != last_c)
        *to++ = *p++;

      *to++ = *p++;

      last_c_was_quote= 1;
    }
    else
    {
      *to++ = *p++;
      last_c_was_quote= 0;
    }
  }
}


/*
  Check a command that is about to be sent (or should have been
  sent if parsing was enabled) to DRIZZLE server for
  suspicious things and generate warnings.
*/

static void scan_command_for_warnings(st_command* command)
{
  const char *ptr= command->query;

  while (*ptr)
  {
    /*
      Look for query's that lines that start with a -- comment
      and has a drizzletest command
    */
    if (ptr[0] == '\n' &&
        ptr[1] && ptr[1] == '-' &&
        ptr[2] && ptr[2] == '-' &&
        ptr[3])
    {
      uint32_t type;
      char save;
      char *end, *start= (char*)ptr+3;
      /* Skip leading spaces */
      while (*start && my_isspace(charset_info, *start))
        start++;
      end= start;
      /* Find end of command(next space) */
      while (*end && !my_isspace(charset_info, *end))
        end++;
      save= *end;
      *end= 0;
      type= command_typelib.find_type(start, TYPELIB::e_default);
      if (type)
        warning_msg("Embedded drizzletest command '--%s' detected in query '%s' was this intentional? ", start, command->query);
      *end= save;
    }
    ptr++;
  }
}

/*
  Check for unexpected "junk" after the end of query
  This is normally caused by missing delimiters or when
  switching between different delimiters
*/

static void check_eol_junk_line(const char *line)
{
  const char *p= line;

  /* Check for extra delimiter */
  if (*p && !strncmp(p, delimiter, delimiter_length))
    die("Extra delimiter \"%s\" found", delimiter);

  /* Allow trailing # comment */
  if (*p && *p != '#')
  {
    if (*p == '\n')
      die("Missing delimiter");
    die("End of line junk detected: \"%s\"", p);
  }
  return;
}

static void check_eol_junk(const char *eol)
{
  const char *p= eol;

  /* Skip past all spacing chars and comments */
  while (*p && (my_isspace(charset_info, *p) || *p == '#' || *p == '\n'))
  {
    /* Skip past comments started with # and ended with newline */
    if (*p && *p == '#')
    {
      p++;
      while (*p && *p != '\n')
        p++;
    }

    /* Check this line */
    if (*p && *p == '\n')
      check_eol_junk_line(p);

    if (*p)
      p++;
  }

  check_eol_junk_line(p);

  return;
}

/*
  Create a command from a set of lines

  SYNOPSIS
  read_command()
  command_ptr pointer where to return the new query

  DESCRIPTION
  Converts lines returned by read_line into a command, this involves
  parsing the first word in the read line to find the command type.

  A -- comment may contain a valid query as the first word after the
  comment start. Thus it's always checked to see if that is the case.
  The advantage with this approach is to be able to execute commands
  terminated by new line '\n' regardless how many "delimiter" it contain.
*/

#define MAX_QUERY (768*1024*2) /* 256K -- a test in sp-big is >128K */
static char read_command_buf[MAX_QUERY];

static int read_command(st_command** command_ptr)
{
  char *p= read_command_buf;
  st_command* command;


  if (parser.current_line < parser.read_lines)
  {
    *command_ptr= q_lines[parser.current_line];
    return(0);
  }
  *command_ptr= command= new st_command;
  q_lines.push_back(command);
  command->type= Q_UNKNOWN;

  read_command_buf[0]= 0;
  if (read_line(read_command_buf, sizeof(read_command_buf)))
  {
    check_eol_junk(read_command_buf);
    return(1);
  }

  convert_to_format_v1(read_command_buf);

  if (*p == '#')
  {
    command->type= Q_COMMENT;
  }
  else if (p[0] == '-' && p[1] == '-')
  {
    command->type= Q_COMMENT_WITH_COMMAND;
    p+= 2; /* Skip past -- */
  }

  /* Skip leading spaces */
  while (*p && my_isspace(charset_info, *p))
    p++;

  command->query_buf= command->query= strdup(p);

  /* Calculate first word length(the command), terminated by space or ( */
  p= command->query;
  while (*p && !my_isspace(charset_info, *p) && *p != '(')
    p++;
  command->first_word_len= (uint32_t) (p - command->query);

  /* Skip spaces between command and first argument */
  while (*p && my_isspace(charset_info, *p))
    p++;
  command->first_argument= p;

  command->end= strchr(command->query, '\0');
  command->query_len= (command->end - command->query);
  parser.read_lines++;
  return(0);
}

/*
  Write the content of str into file

  SYNOPSIS
  str_to_file2
  fname - name of file to truncate/create and write to
  str - content to write to file
  size - size of content witten to file
  append - append to file instead of overwriting old file
*/

void str_to_file2(const char *fname, const char *str, int size, bool append)
{
  char buff[FN_REFLEN];
  if (!internal::test_if_hard_path(fname))
  {
    snprintf(buff, sizeof(buff), "%s%s",opt_basedir.c_str(),fname);
    fname= buff;
  }
  internal::fn_format(buff, fname, "", "", MY_UNPACK_FILENAME);

  int flags= O_WRONLY | O_CREAT;
  if (!append)
    flags|= O_TRUNC;
  int fd= internal::my_open(buff, flags, MYF(MY_WME | MY_FFNF));
  if (fd < 0)
    die("Could not open '%s' for writing: errno = %d", buff, errno);
  if (append && lseek(fd, 0, SEEK_END) == MY_FILEPOS_ERROR)
    die("Could not find end of file '%s': errno = %d", buff, errno);
  if (internal::my_write(fd, (unsigned char*)str, size, MYF(MY_WME|MY_FNABP)))
    die("write failed");
  internal::my_close(fd, MYF(0));
}

/*
  Write the content of str into file

  SYNOPSIS
  str_to_file
  fname - name of file to truncate/create and write to
  str - content to write to file
  size - size of content witten to file
*/

void str_to_file(const char *fname, const char *str, int size)
{
  str_to_file2(fname, str, size, false);
}


void dump_result_to_log_file(const char *buf, int size)
{
  char log_file[FN_REFLEN];
  str_to_file(internal::fn_format(log_file, result_file_name.c_str(), opt_logdir.c_str(), ".log",
                        ! opt_logdir.empty() ? MY_REPLACE_DIR | MY_REPLACE_EXT :
                        MY_REPLACE_EXT),
              buf, size);
  fprintf(stderr, "\nMore results from queries before failure can be found in %s\n", log_file);
}

void dump_progress()
{
  char progress_file[FN_REFLEN];
  str_to_file(internal::fn_format(progress_file, result_file_name.c_str(),
                        opt_logdir.c_str(), ".progress",
                        ! opt_logdir.empty() ? MY_REPLACE_DIR | MY_REPLACE_EXT :
                        MY_REPLACE_EXT),
              ds_progress.c_str(), ds_progress.length());
}

void dump_warning_messages()
{
  char warn_file[FN_REFLEN];

  str_to_file(internal::fn_format(warn_file, result_file_name.c_str(), opt_logdir.c_str(), ".warnings",
                        ! opt_logdir.empty() ? MY_REPLACE_DIR | MY_REPLACE_EXT :
                        MY_REPLACE_EXT),
              ds_warning_messages.c_str(), ds_warning_messages.length());
}


/*
  Append the result for one field to the dynamic string ds
*/

static void append_field(string *ds, uint32_t col_idx, drizzle_column_st *column,
                         const char* val, uint64_t len, bool is_null)
{
  if (col_idx < max_replace_column && replace_column[col_idx])
  {
    val= replace_column[col_idx];
    len= strlen(val);
  }
  else if (is_null)
  {
    val= "NULL";
    len= 4;
  }

  if (!display_result_vertically)
  {
    if (col_idx)
      ds->append("\t");
    replace_append_mem(*ds, val, (int)len);
  }
  else
  {
    ds->append(drizzle_column_name(column));
    ds->append("\t");
    replace_append_mem(*ds, val, (int)len);
    ds->append("\n");
  }
}


/*
  Append all results to the dynamic string separated with '\t'
  Values may be converted with 'replace_column'
*/

static void append_result(string *ds, drizzle::result_c& res)
{
  uint32_t num_fields= res.column_count();
  while (drizzle_row_t row = res.row_next())
  {
    size_t* lengths = res.row_field_sizes();
    res.column_seek(0);
    for (uint32_t i = 0; i < num_fields; i++)
    {
      drizzle_column_st* column= res.column_next();
      if (row[i] && drizzle_column_type(column) == DRIZZLE_COLUMN_TYPE_TINY)
      {
        if (boost::lexical_cast<uint32_t>(row[i]))
        {
          if (drizzle_column_flags(column) & DRIZZLE_COLUMN_FLAGS_UNSIGNED)
          {
            append_field(ds, i, column, "YES", 3, false);
          }
          else
          {
            append_field(ds, i, column, "TRUE", 4, false);
          }
        }
        else
        {
          if (drizzle_column_flags(column) & DRIZZLE_COLUMN_FLAGS_UNSIGNED)
          {
            append_field(ds, i, column, "NO", 2, false);
          }
          else
          {
            append_field(ds, i, column, "FALSE", 5, false);
          }
        }
      }
      else
      {
        append_field(ds, i, column, (const char*)row[i], lengths[i], !row[i]);
      }
    }
    if (!display_result_vertically)
      ds->append("\n");
  }
}


/*
  Append metadata for fields to output
*/

static void append_metadata(string& ds, drizzle::result_c& res)
{
  ds += "Catalog\tDatabase\tTable\tTable_alias\tColumn\tColumn_alias\tType\tLength\tMax length\tIs_null\tFlags\tDecimals\tCharsetnr\n";
  res.column_seek(0);
  while (drizzle_column_st* column= res.column_next())
  {
    ds += drizzle_column_catalog(column);
    ds += "\t";
    ds += drizzle_column_db(column);
    ds += "\t";
    ds += drizzle_column_orig_table(column);
    ds += "\t";
    ds += drizzle_column_table(column);
    ds += "\t";
    ds += drizzle_column_orig_name(column);
    ds += "\t";
    ds += drizzle_column_name(column);
    ds += "\t";
    replace_append_uint(ds, drizzle_column_type_drizzle(column));
    ds += "\t";
    replace_append_uint(ds, drizzle_column_size(column));
    ds += "\t";
    replace_append_uint(ds, drizzle_column_type(column) == DRIZZLE_COLUMN_TYPE_TINY ? 1 : drizzle_column_max_size(column));
    ds += "\t";
    ds += drizzle_column_flags(column) & DRIZZLE_COLUMN_FLAGS_NOT_NULL ? "N" : "Y";
    ds += "\t";
    replace_append_uint(ds, drizzle_column_flags(column));
    ds += "\t";
    replace_append_uint(ds, drizzle_column_decimals(column));
    ds += "\t";
    replace_append_uint(ds, drizzle_column_charset(column));
    ds += "\n";
  }
}


/*
  Append affected row count and other info to output
*/

static void append_info(string *ds, uint64_t affected_rows,
                        const char *info)
{
  ostringstream buf;
  buf << "affected rows: " << affected_rows << endl;
  ds->append(buf.str());
  if (info && strcmp(info, ""))
  {
    ds->append("info: ");
    ds->append(info);
    ds->append("\n", 1);
  }
}


/*
  Display the table headings with the names tab separated
*/

static void append_table_headings(string& ds, drizzle::result_c& res)
{
  uint32_t col_idx= 0;
  res.column_seek(0);
  while (drizzle_column_st* column= res.column_next())
  {
    if (col_idx)
      ds += "\t";
    replace_append(&ds, drizzle_column_name(column));
    col_idx++;
  }
  ds += "\n";
}

/*
  Fetch warnings from server and append to ds

  RETURN VALUE
  Number of warnings appended to ds
*/

static int append_warnings(string& ds, drizzle::connection_c& con, drizzle::result_c& res)
{
  uint32_t count= drizzle_result_warning_count(&res.b_);
  if (!count)
    return 0;

  drizzle::result_c warn_res;
  dt_query(con, warn_res, "show warnings");
  append_result(&ds, warn_res);
  return count;
}


/*
  Run query using DRIZZLE C API

  SYNOPSIS
  run_query_normal()
  drizzle  DRIZZLE handle
  command  current command pointer
  flags  flags indicating if we should SEND and/or REAP
  query  query string to execute
  query_len  length query string to execute
  ds    output buffer where to store result form query
*/

static void run_query_normal(st_connection& cn,
                             st_command* command,
                             int flags, char *query, int query_len,
                             string *ds, string& ds_warnings)
{
  drizzle_return_t ret;
  drizzle_con_st *con= cn;
  int err= 0;

  drizzle_con_add_options(con, DRIZZLE_CON_NO_RESULT_READ);

  drizzle::result_c res;
  if (flags & QUERY_SEND_FLAG)
  {
    /*
     * Send the query
     */

    (void) drizzle_query(con, &res.b_, query, query_len, &ret);
    if (ret != DRIZZLE_RETURN_OK)
    {
      if (ret == DRIZZLE_RETURN_ERROR_CODE ||
          ret == DRIZZLE_RETURN_HANDSHAKE_FAILED)
      {
        err= res.error_code();
        handle_error(command, err, res.error(), drizzle_result_sqlstate(&res.b_), ds);
      }
      else
      {
        handle_error(command, ret, drizzle_con_error(con), "", ds);
        err= ret;
      }
      goto end;
    }
  }
  if (!(flags & QUERY_REAP_FLAG))
    return;

  {
    /*
     * Read the result packet
     */
    if (drizzle_result_read(con, &res.b_, &ret) == NULL ||
        ret != DRIZZLE_RETURN_OK)
    {
      if (ret == DRIZZLE_RETURN_ERROR_CODE)
      {
        handle_error(command, res.error_code(), res.error(), drizzle_result_sqlstate(&res.b_), ds);
      }
      else
        handle_error(command, ret, drizzle_con_error(con), "", ds);
      err= ret;
      goto end;
    }

    /*
      Store the result of the query if it will return any fields
    */
    if (res.column_count() &&
        (ret= drizzle_result_buffer(&res.b_)) != DRIZZLE_RETURN_OK)
    {
      if (ret == DRIZZLE_RETURN_ERROR_CODE)
      {
        handle_error(command, res.error_code(), res.error(), drizzle_result_sqlstate(&res.b_), ds);
      }
      else
        handle_error(command, ret, drizzle_con_error(con), "", ds);
      err= ret;
      goto end;
    }

    if (!disable_result_log)
    {
      uint64_t affected_rows= 0;    /* Ok to be undef if 'disable_info' is set */

      if (res.column_count())
      {
        if (display_metadata)
          append_metadata(*ds, res);

        if (!display_result_vertically)
          append_table_headings(*ds, res);

        append_result(ds, res);
      }

      /*
        Need to call drizzle_result_affected_rows() before the "new"
        query to find the warnings
      */
      if (!disable_info)
        affected_rows= drizzle_result_affected_rows(&res.b_);

      /*
        Add all warnings to the result. We can't do this if we are in
        the middle of processing results from multi-statement, because
        this will break protocol.
      */
      if (!disable_warnings)
      {
        drizzle_con_remove_options(con, DRIZZLE_CON_NO_RESULT_READ);
        if (append_warnings(ds_warnings, cn, res) || not ds_warnings.empty())
        {
          ds->append("Warnings:\n", 10);
          *ds += ds_warnings;
        }
      }

      if (!disable_info)
        append_info(ds, affected_rows, drizzle_result_info(&res.b_));
    }

  }

  /* If we come here the query is both executed and read successfully */
  handle_no_error(command);

end:

  /*
    We save the return code (drizzleclient_errno(drizzle)) from the last call sent
    to the server into the drizzletest builtin variable $drizzleclient_errno. This
    variable then can be used from the test case itself.
  */
  drizzle_con_remove_options(con, DRIZZLE_CON_NO_RESULT_READ);
  var_set_errno(err);
}


/*
  Handle errors which occurred during execution

  SYNOPSIS
  handle_error()
  q     - query context
  err_errno - error number
  err_error - error message
  err_sqlstate - sql state
  ds    - dynamic string which is used for output buffer

  NOTE
  If there is an unexpected error this function will abort drizzletest
  immediately.
*/

void handle_error(st_command* command,
                  unsigned int err_errno, const char *err_error,
                  const char *err_sqlstate, string *ds)
{
  if (! command->require_file.empty())
  {
    /*
      The query after a "--require" failed. This is fine as long the server
      returned a valid reponse. Don't allow 2013 or 2006 to trigger an
      abort_not_supported_test
    */
    if (err_errno == DRIZZLE_RETURN_SERVER_GONE)
      die("require query '%s' failed: %d: %s", command->query, err_errno, err_error);

    /* Abort the run of this test, pass the failed query as reason */
    abort_not_supported_test("Query '%s' failed, required functionality not supported", command->query);
  }

  if (command->abort_on_error)
    die("query '%s' failed: %d: %s", command->query, err_errno, err_error);

  uint32_t i= 0;
  for (; i < command->expected_errors.count; i++)
  {
    if (((command->expected_errors.err[i].type == ERR_ERRNO) &&
         (command->expected_errors.err[i].code.errnum == err_errno)) ||
        ((command->expected_errors.err[i].type == ERR_SQLSTATE) &&
         (strncmp(command->expected_errors.err[i].code.sqlstate,
                  err_sqlstate, DRIZZLE_MAX_SQLSTATE_SIZE) == 0)))
    {
      if (!disable_result_log)
      {
        if (command->expected_errors.count == 1)
        {
          /* Only log error if there is one possible error */
          ds->append("ERROR ", 6);
          replace_append(ds, err_sqlstate);
          ds->append(": ", 2);
          replace_append(ds, err_error);
          ds->append("\n",1);
        }
        /* Don't log error if we may not get an error */
        else if (command->expected_errors.err[0].type == ERR_SQLSTATE ||
                 (command->expected_errors.err[0].type == ERR_ERRNO &&
                  command->expected_errors.err[0].code.errnum != 0))
          ds->append("Got one of the listed errors\n");
      }
      /* OK */
      return;
    }
  }

  if (!disable_result_log)
  {
    ds->append("ERROR ",6);
    replace_append(ds, err_sqlstate);
    ds->append(": ", 2);
    replace_append(ds, err_error);
    ds->append("\n", 1);
  }

  if (i)
  {
    if (command->expected_errors.err[0].type == ERR_ERRNO)
      die("query '%s' failed with wrong errno %d: '%s', instead of %d...",
          command->query, err_errno, err_error,
          command->expected_errors.err[0].code.errnum);
    else
      die("query '%s' failed with wrong sqlstate %s: '%s', instead of %s...",
          command->query, err_sqlstate, err_error,
          command->expected_errors.err[0].code.sqlstate);
  }
}


/*
  Handle absence of errors after execution

  SYNOPSIS
  handle_no_error()
  q - context of query

  RETURN VALUE
  error - function will not return
*/

void handle_no_error(st_command* command)
{
  if (command->expected_errors.err[0].type == ERR_ERRNO &&
      command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    die("query '%s' succeeded - should have failed with errno %d...", command->query, command->expected_errors.err[0].code.errnum);
  }
  else if (command->expected_errors.err[0].type == ERR_SQLSTATE &&
           strcmp(command->expected_errors.err[0].code.sqlstate,"00000") != 0)
  {
    /* SQLSTATE we wanted was != "00000", i.e. not an expected success */
    die("query '%s' succeeded - should have failed with sqlstate %s...", command->query, command->expected_errors.err[0].code.sqlstate);
  }
}


/*
  Run query

  SYNPOSIS
  run_query()
  drizzle  DRIZZLE handle
  command  currrent command pointer

  flags control the phased/stages of query execution to be performed
  if QUERY_SEND_FLAG bit is on, the query will be sent. If QUERY_REAP_FLAG
  is on the result will be read - for regular query, both bits must be on
*/

static void run_query(st_connection& cn,
                      st_command* command,
                      int flags)
{
  string eval_query;
  char *query;
  int query_len;


  /* Scan for warning before sending to server */
  scan_command_for_warnings(command);

  /*
    Evaluate query if this is an eval command
  */
  if (command->type == Q_EVAL)
  {
    do_eval(&eval_query, command->query, command->end, false);
    query = strdup(eval_query.c_str());
    query_len = eval_query.length();
  }
  else
  {
    query = command->query;
    query_len = strlen(query);
  }

  /*
    When command->require_file is set the output of _this_ query
    should be compared with an already existing file
    Create a temporary dynamic string to contain the output from
    this query.
  */
  string ds_result;
  string* ds= command->require_file.empty() ? &ds_res : &ds_result;
  /*
    Log the query into the output buffer
  */
  if (!disable_query_log && (flags & QUERY_SEND_FLAG))
  {
    replace_append_mem(*ds, query, query_len);
    ds->append(delimiter, delimiter_length);
    ds->append("\n");
  }

  string* save_ds= NULL;
  string ds_sorted;
  if (display_result_sorted)
  {
    /*
      Collect the query output in a separate string
      that can be sorted before it's added to the
      global result string
    */
    save_ds= ds; /* Remember original ds */
    ds= &ds_sorted;
  }

  /*
    Always run with normal C API if it's not a complete
    SEND + REAP
  */
  string ds_warnings;
  run_query_normal(cn, command, flags, query, query_len, ds, ds_warnings);

  if (display_result_sorted)
  {
    /* Sort the result set and append it to result */
    append_sorted(*save_ds, ds_sorted);
    ds= save_ds;
  }

  if (! command->require_file.empty())
  {
    /* A result file was specified for _this_ query
       and the output should be checked against an already
       existing file which has been specified using --require or --result
    */
    check_require(*ds, command->require_file);
  }
}


/****************************************************************************/

static void get_command_type(st_command* command)
{
  if (*command->query == '}')
  {
    command->type = Q_END_BLOCK;
    return;
  }

  char save= command->query[command->first_word_len];
  command->query[command->first_word_len]= 0;
  uint32_t type= command_typelib.find_type(command->query, TYPELIB::e_default);
  command->query[command->first_word_len]= save;
  if (type > 0)
  {
    command->type=(enum enum_commands) type;    /* Found command */

    /*
      Look for case where "query" was explicitly specified to
      force command being sent to server
    */
    if (type == Q_QUERY)
    {
      /* Skip the "query" part */
      command->query= command->first_argument;
    }
  }
  else
  {
    /* No drizzletest command matched */

    if (command->type != Q_COMMENT_WITH_COMMAND)
    {
      /* A query that will sent to drizzled */
      command->type= Q_QUERY;
    }
    else
    {
      /* -- comment that didn't contain a drizzletest command */
      command->type= Q_COMMENT;
      warning_msg("Suspicious command '--%s' detected, was this intentional? " \
                  "Use # instead of -- to avoid this warning",
                  command->query);

      if (command->first_word_len &&
          strcmp(command->query + command->first_word_len - 1, delimiter) == 0)
      {
        /*
          Detect comment with command using extra delimiter
          Ex --disable_query_log;
          ^ Extra delimiter causing the command
          to be skipped
        */
        save= command->query[command->first_word_len-1];
        command->query[command->first_word_len-1]= 0;
        if (command_typelib.find_type(command->query, TYPELIB::e_default) > 0)
          die("Extra delimiter \";\" found");
        command->query[command->first_word_len-1]= save;

      }
    }
  }

  /* Set expected error on command */
  memcpy(&command->expected_errors, &saved_expected_errors, sizeof(saved_expected_errors));
  command->abort_on_error= (command->expected_errors.count == 0 && abort_on_error);
}



/*
  Record how many milliseconds it took to execute the test file
  up until the current line and save it in the dynamic string ds_progress.

  The ds_progress will be dumped to <test_name>.progress when
  test run completes

*/

static void mark_progress(st_command*, int line)
{
  uint64_t timer= timer_now();
  if (!progress_start)
    progress_start= timer;
  timer-= progress_start;

  ostringstream buf;
  /* Milliseconds since start */
  buf << timer << "\t";

  /* Parser line number */
  buf << line << "\t";

  /* Filename */
  buf << cur_file->file_name << ":";

  /* Line in file */
  buf << cur_file->lineno << endl;

  ds_progress += buf.str();

}

static void check_retries(uint32_t in_opt_max_connect_retries)
{
  if (in_opt_max_connect_retries > 10000 || opt_max_connect_retries<1)
  {
    cout << N_("Error: Invalid Value for opt_max_connect_retries"); 
    exit(-1);
  }
  opt_max_connect_retries= in_opt_max_connect_retries;
}

static void check_tail_lines(uint32_t in_opt_tail_lines)
{
  if (in_opt_tail_lines > 10000)
  {
    cout << N_("Error: Invalid Value for opt_tail_lines"); 
    exit(-1);
  }
  opt_tail_lines= in_opt_tail_lines;
}

static void check_sleep(int32_t in_opt_sleep)
{
  if (in_opt_sleep < -1)
  {
    cout << N_("Error: Invalid Value for opt_sleep"); 
    exit(-1);
  }
  opt_sleep= in_opt_sleep;
}

int main(int argc, char **argv)
{
try
{
  bool q_send_flag= 0, abort_flag= 0;
  uint32_t command_executed= 0, last_command_executed= 0;
  string save_file;

  TMPDIR[0]= 0;

  internal::my_init();

  po::options_description commandline_options("Options used only in command line");
  commandline_options.add_options()
  ("help,?", "Display this help and exit.")
  ("mark-progress", po::value<bool>(&opt_mark_progress)->default_value(false)->zero_tokens(),
  "Write linenumber and elapsed time to <testname>.progress ")
  ("sleep,T", po::value<int32_t>(&opt_sleep)->default_value(-1)->notifier(&check_sleep),
  "Sleep always this many seconds on sleep commands.")
  ("test-file,x", po::value<string>(),
  "Read test from/in this file (default stdin).")
  ("timer-file,f", po::value<string>(),
  "File where the timing in micro seconds is stored.")
  ("tmpdir,t", po::value<string>(),
  "Temporary directory where sockets are put.")
  ("verbose,v", po::value<bool>(&verbose)->default_value(false),
  "Write more.")
  ("version,V", "Output version information and exit.")
  ("no-defaults", po::value<bool>()->default_value(false)->zero_tokens(),
  "Configuration file defaults are not used if no-defaults is set")
  ;

  po::options_description test_options("Options specific to the drizzleimport");
  test_options.add_options()
  ("basedir,b", po::value<string>(&opt_basedir)->default_value(""),
  "Basedir for tests.")
  ("character-sets-dir", po::value<string>(&opt_charsets_dir)->default_value(""),
  "Directory where character sets are.")
  ("database,D", po::value<string>(&opt_db)->default_value(""),
  "Database to use.")
  ("include,i", po::value<string>(&opt_include)->default_value(""),
  "Include SQL before each test case.")  
  ("testdir", po::value<string>(&opt_testdir)->default_value(""),
  "Path to use to search for test files")
  ("logdir", po::value<string>(&opt_logdir)->default_value(""),
  "Directory for log files")
  ("max-connect-retries", po::value<uint32_t>(&opt_max_connect_retries)->default_value(500)->notifier(&check_retries),
  "Max number of connection attempts when connecting to server")
  ("quiet,s", po::value<bool>(&silent)->default_value(false)->zero_tokens(),
  "Suppress all normal output.")
  ("record,r", "Record output of test_file into result file.")
  ("result-file,R", po::value<string>(&result_file_name)->default_value(""),
  "Read/Store result from/in this file.")
  ("silent,s", po::value<bool>(&silent)->default_value(false)->zero_tokens(),
  "Suppress all normal output. Synonym for --quiet.")
  ("tail-lines", po::value<uint32_t>(&opt_tail_lines)->default_value(0)->notifier(&check_tail_lines),
  "Number of lines of the resul to include in a failure report")
  ;

  po::options_description client_options("Options specific to the client");
  client_options.add_options()

  ("host,h", po::value<string>(&opt_host)->default_value("localhost"),
  "Connect to host.")
  ("password,P", po::value<string>(&password)->default_value("PASSWORD_SENTINEL"),
  "Password to use when connecting to server.")
  ("port,p", po::value<uint32_t>(&opt_port)->default_value(0),
  "Port number to use for connection or 0 for default")
  ("protocol", po::value<string>(&opt_protocol),
  "The protocol of connection (mysql or drizzle).")
  ("user,u", po::value<string>(&opt_user)->default_value(""),
  "User for login.")
  ;

  po::positional_options_description p;
  p.add("database", 1);

  po::options_description long_options("Allowed Options");
  long_options.add(commandline_options).add(test_options).add(client_options);

  std::string system_config_dir_test(SYSCONFDIR); 
  system_config_dir_test += "/drizzle/drizzletest.cnf";

  std::string system_config_dir_client(SYSCONFDIR); 
  system_config_dir_client += "/drizzle/client.cnf";

  std::string user_config_dir((getenv("XDG_CONFIG_HOME")? getenv("XDG_CONFIG_HOME"):"~/.config"));

  if (user_config_dir.compare(0, 2, "~/") == 0)
  {
    const char *homedir= getenv("HOME");
    if (homedir != NULL)
      user_config_dir.replace(0, 1, homedir);
  }

  po::variables_map vm;

  // Disable allow_guessing
  int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;

  po::store(po::command_line_parser(argc, argv).options(long_options).
            style(style).positional(p).extra_parser(parse_password_arg).run(),
            vm);

  if (! vm["no-defaults"].as<bool>())
  {
    std::string user_config_dir_test(user_config_dir);
    user_config_dir_test += "/drizzle/drizzletest.cnf"; 

    std::string user_config_dir_client(user_config_dir);
    user_config_dir_client += "/drizzle/client.cnf";

    ifstream user_test_ifs(user_config_dir_test.c_str());
    po::store(parse_config_file(user_test_ifs, test_options), vm);

    ifstream user_client_ifs(user_config_dir_client.c_str());
    po::store(parse_config_file(user_client_ifs, client_options), vm);

    ifstream system_test_ifs(system_config_dir_test.c_str());
    store(parse_config_file(system_test_ifs, test_options), vm);

    ifstream system_client_ifs(system_config_dir_client.c_str());
    po::store(parse_config_file(system_client_ifs, client_options), vm);
  }

  po::notify(vm);

  /* Init expected errors */
  memset(&saved_expected_errors, 0, sizeof(saved_expected_errors));

  /* Init file stack */
  memset(file_stack.data(), 0, sizeof(file_stack));
  cur_file= file_stack.data();

  /* Init block stack */
  memset(block_stack, 0, sizeof(block_stack));
  block_stack_end=
    block_stack + (sizeof(block_stack)/sizeof(struct st_block)) - 1;
  cur_block= block_stack;
  cur_block->ok= true; /* Outer block should always be executed */
  cur_block->cmd= cmd_none;

  var_set_string("$DRIZZLE_SERVER_VERSION", drizzle_version());

  memset(&master_pos, 0, sizeof(master_pos));

  parser.current_line= parser.read_lines= 0;
  memset(&var_reg, 0, sizeof(var_reg));

  init_builtin_echo();

  ds_res.reserve(65536);
  ds_progress.reserve(2048);
  ds_warning_messages.reserve(2048);

 
  if (vm.count("record"))
  {
    record = 1;
  }

  if (vm.count("test-file"))
  {
    string tmp= vm["test-file"].as<string>();
    char buff[FN_REFLEN];
    if (!internal::test_if_hard_path(tmp.c_str()))
    {
      snprintf(buff, sizeof(buff), "%s%s",opt_basedir.c_str(),tmp.c_str());
      tmp= buff;
    }
    internal::fn_format(buff, tmp.c_str(), "", "", MY_UNPACK_FILENAME);
    assert(cur_file == file_stack.data() && cur_file->file == 0);
    if (!(cur_file->file= fopen(buff, "r")))
    {
      fprintf(stderr, _("Could not open '%s' for reading: errno = %d"), buff, errno);
      return EXIT_ARGUMENT_INVALID;
    }
    cur_file->file_name= strdup(buff);
    cur_file->lineno= 1;
  }

  if (vm.count("timer-file"))
  {
    string tmp= vm["timer-file"].as<string>().c_str();
    static char buff[FN_REFLEN];
    if (!internal::test_if_hard_path(tmp.c_str()))
    {
      snprintf(buff, sizeof(buff), "%s%s",opt_basedir.c_str(),tmp.c_str());
      tmp= buff;
    }
    internal::fn_format(buff, tmp.c_str(), "", "", MY_UNPACK_FILENAME);
    timer_file= buff;
    unlink(timer_file);       /* Ignore error, may not exist */
  }

  if (vm.count("protocol"))
  {
    std::transform(opt_protocol.begin(), opt_protocol.end(),
      opt_protocol.begin(), ::tolower);

    if (not opt_protocol.compare("mysql"))
      use_drizzle_protocol=false;
    else if (not opt_protocol.compare("drizzle"))
      use_drizzle_protocol=true;
    else
    {
      cout << _("Error: Unknown protocol") << " '" << opt_protocol << "'" << endl;
      exit(-1);
    }
  }

  if (vm.count("port"))
  {
    /* If the port number is > 65535 it is not a valid port
       This also helps with potential data loss casting unsigned long to a
       uint32_t. */
    if (opt_port > 65535)
    {
      fprintf(stderr, _("Value supplied for port is not valid.\n"));
      exit(EXIT_ARGUMENT_INVALID);
    }
  }

  if( vm.count("password") )
  {
    if (!opt_password.empty())
      opt_password.erase();
    if (password == PASSWORD_SENTINEL)
    {
      opt_password= "";
    }
    else
    {
      opt_password= password;
      tty_password= false;
    }
  }
  else
  {
      tty_password= true;
  }

  if (vm.count("tmpdir"))
  {
    strncpy(TMPDIR, vm["tmpdir"].as<string>().c_str(), sizeof(TMPDIR));
  }

  if (vm.count("version"))
  {
    printf("%s  Ver %s Distrib %s, for %s-%s (%s)\n",internal::my_progname,MTEST_VERSION,
    drizzle_version(),HOST_VENDOR,HOST_OS,HOST_CPU);
    exit(0);
  }
  
  if (vm.count("help"))
  {
    printf("%s  Ver %s Distrib %s, for %s-%s (%s)\n",internal::my_progname,MTEST_VERSION,
    drizzle_version(),HOST_VENDOR,HOST_OS,HOST_CPU);
    printf("MySQL AB, by Sasha, Matt, Monty & Jani\n");
    printf("Drizzle version modified by Brian, Jay, Monty Taylor, PatG and Stewart\n");
    printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
    printf("Runs a test against the DRIZZLE server and compares output with a results file.\n\n");
    printf("Usage: %s [OPTIONS] [database] < test_file\n", internal::my_progname);
    exit(0);
  }

  if (tty_password)
  {
    opt_pass= client_get_tty_password(NULL);          /* purify tested */
  }

  server_initialized= 1;
  if (cur_file == file_stack.data() && cur_file->file == 0)
  {
    cur_file->file= stdin;
    cur_file->file_name= strdup("<stdin>");
    cur_file->lineno= 1;
  }
  cur_con= safe_connect("default", opt_host, opt_user, opt_pass, opt_db, opt_port);
  g_connections["default"] = cur_con;

  fill_global_error_names();

  /* Use all time until exit if no explicit 'start_timer' */
  timer_start= timer_now();

  /*
    Initialize $drizzleclient_errno with -1, so we can
    - distinguish it from valid values ( >= 0 ) and
    - detect if there was never a command sent to the server
  */
  var_set_errno(-1);

  /* Update $drizzleclient_get_server_version to that of current connection */
  var_set_drizzleclient_get_server_version(*cur_con);

  if (! opt_include.empty())
  {
    open_file(opt_include.c_str());
  }

  st_command* command;
  while (!read_command(&command) && !abort_flag)
  {
    int current_line_inc = 1, processed = 0;
    if (command->type == Q_UNKNOWN || command->type == Q_COMMENT_WITH_COMMAND)
      get_command_type(command);

    if (parsing_disabled &&
        command->type != Q_ENABLE_PARSING &&
        command->type != Q_DISABLE_PARSING)
    {
      command->type= Q_COMMENT;
      scan_command_for_warnings(command);
    }

    if (cur_block->ok)
    {
      command->last_argument= command->first_argument;
      processed = 1;
      switch (command->type) {
      case Q_CONNECT:
        do_connect(command);
        break;
      case Q_CONNECTION:
        select_connection(command);
        break;
      case Q_DISCONNECT:
      case Q_DIRTY_CLOSE:
        do_close_connection(command); break;
      case Q_ENABLE_QUERY_LOG:   disable_query_log=0; break;
      case Q_DISABLE_QUERY_LOG:  disable_query_log=1; break;
      case Q_ENABLE_ABORT_ON_ERROR:  abort_on_error=1; break;
      case Q_DISABLE_ABORT_ON_ERROR: abort_on_error=0; break;
      case Q_ENABLE_RESULT_LOG:  disable_result_log=0; break;
      case Q_DISABLE_RESULT_LOG: disable_result_log=1; break;
      case Q_ENABLE_WARNINGS:    disable_warnings=0; break;
      case Q_DISABLE_WARNINGS:   disable_warnings=1; break;
      case Q_ENABLE_INFO:        disable_info=0; break;
      case Q_DISABLE_INFO:       disable_info=1; break;
      case Q_ENABLE_METADATA:    display_metadata=1; break;
      case Q_DISABLE_METADATA:   display_metadata=0; break;
      case Q_SOURCE: do_source(command); break;
      case Q_SLEEP: do_sleep(command, 0); break;
      case Q_REAL_SLEEP: do_sleep(command, 1); break;
      case Q_WAIT_FOR_SLAVE_TO_STOP: do_wait_for_slave_to_stop(); break;
      case Q_INC: do_modify_var(command, DO_INC); break;
      case Q_DEC: do_modify_var(command, DO_DEC); break;
      case Q_ECHO: do_echo(command); command_executed++; break;
      case Q_SYSTEM: do_system(command); break;
      case Q_REMOVE_FILE: do_remove_file(command); break;
      case Q_MKDIR: do_mkdir(command); break;
      case Q_RMDIR: do_rmdir(command); break;
      case Q_FILE_EXIST: do_file_exist(command); break;
      case Q_WRITE_FILE: do_write_file(command); break;
      case Q_APPEND_FILE: do_append_file(command); break;
      case Q_DIFF_FILES: do_diff_files(command); break;
      case Q_SEND_QUIT: do_send_quit(command); break;
      case Q_CHANGE_USER: do_change_user(command); break;
      case Q_CAT_FILE: do_cat_file(command); break;
      case Q_COPY_FILE: do_copy_file(command); break;
      case Q_CHMOD_FILE: do_chmod_file(command); break;
      case Q_PERL: do_perl(command); break;
      case Q_DELIMITER:
        do_delimiter(command);
        break;
      case Q_DISPLAY_VERTICAL_RESULTS:
        display_result_vertically= true;
        break;
      case Q_DISPLAY_HORIZONTAL_RESULTS:
        display_result_vertically= false;
        break;
      case Q_SORTED_RESULT:
        /*
          Turn on sorting of result set, will be reset after next
          command
        */
        display_result_sorted= true;
        break;
      case Q_LET: do_let(command); break;
      case Q_EVAL_RESULT:
        die("'eval_result' command  is deprecated");
      case Q_EVAL:
      case Q_QUERY_VERTICAL:
      case Q_QUERY_HORIZONTAL:
        if (command->query == command->query_buf)
        {
          /* Skip the first part of command, i.e query_xxx */
          command->query= command->first_argument;
          command->first_word_len= 0;
        }
        /* fall through */
      case Q_QUERY:
      case Q_REAP:
      {
        bool old_display_result_vertically= display_result_vertically;
        /* Default is full query, both reap and send  */
        int flags= QUERY_REAP_FLAG | QUERY_SEND_FLAG;

        if (q_send_flag)
        {
          /* Last command was an empty 'send' */
          flags= QUERY_SEND_FLAG;
          q_send_flag= 0;
        }
        else if (command->type == Q_REAP)
        {
          flags= QUERY_REAP_FLAG;
        }

        /* Check for special property for this query */
        display_result_vertically|= (command->type == Q_QUERY_VERTICAL);

        if (! save_file.empty())
        {
          command->require_file= save_file;
          save_file.clear();
        }
        run_query(*cur_con, command, flags);
        command_executed++;
        command->last_argument= command->end;

        /* Restore settings */
        display_result_vertically= old_display_result_vertically;

        break;
      }
      case Q_SEND:
        if (!*command->first_argument)
        {
          /*
            This is a send without arguments, it indicates that _next_ query
            should be send only
          */
          q_send_flag= 1;
          break;
        }

        /* Remove "send" if this is first iteration */
        if (command->query == command->query_buf)
          command->query= command->first_argument;

        /*
          run_query() can execute a query partially, depending on the flags.
          QUERY_SEND_FLAG flag without QUERY_REAP_FLAG tells it to just send
          the query and read the result some time later when reap instruction
          is given on this connection.
        */
        run_query(*cur_con, command, QUERY_SEND_FLAG);
        command_executed++;
        command->last_argument= command->end;
        break;
      case Q_REQUIRE:
        do_get_file_name(command, save_file);
        break;
      case Q_ERROR:
        do_get_errcodes(command);
        break;
      case Q_REPLACE:
        do_get_replace(command);
        break;
      case Q_REPLACE_REGEX:
        do_get_replace_regex(command);
        break;
      case Q_REPLACE_COLUMN:
        do_get_replace_column(command);
        break;
      case Q_SAVE_MASTER_POS: do_save_master_pos(); break;
      case Q_SYNC_WITH_MASTER: do_sync_with_master(command); break;
      case Q_SYNC_SLAVE_WITH_MASTER:
      {
        do_save_master_pos();
        if (*command->first_argument)
          select_connection(command);
        else
          select_connection_name("slave");
        do_sync_with_master2(0);
        break;
      }
      case Q_COMMENT:        /* Ignore row */
        command->last_argument= command->end;
        break;
      case Q_PING:
        {
          drizzle::result_c result;
          drizzle_return_t ret;
          (void) drizzle_ping(*cur_con, &result.b_, &ret);
        }
        break;
      case Q_EXEC:
        do_exec(command);
        command_executed++;
        break;
      case Q_START_TIMER:
        /* Overwrite possible earlier start of timer */
        timer_start= timer_now();
        break;
      case Q_END_TIMER:
        /* End timer before ending drizzletest */
        timer_output();
        break;
      case Q_CHARACTER_SET:
        do_set_charset(command);
        break;
      case Q_DISABLE_RECONNECT:
        set_reconnect(*cur_con, 0);
        break;
      case Q_ENABLE_RECONNECT:
        set_reconnect(*cur_con, 1);
        break;
      case Q_DISABLE_PARSING:
        if (parsing_disabled == 0)
          parsing_disabled= 1;
        else
          die("Parsing is already disabled");
        break;
      case Q_ENABLE_PARSING:
        /*
          Ensure we don't get parsing_disabled < 0 as this would accidentally
          disable code we don't want to have disabled
        */
        if (parsing_disabled == 1)
          parsing_disabled= 0;
        else
          die("Parsing is already enabled");
        break;
      case Q_DIE:
        /* Abort test with error code and error message */
        die("%s", command->first_argument);
        break;
      case Q_EXIT:
        /* Stop processing any more commands */
        abort_flag= 1;
        break;
      case Q_SKIP:
        abort_not_supported_test("%s", command->first_argument);
        break;

      case Q_RESULT:
        die("result, deprecated command");
        break;

      default:
        processed= 0;
        break;
      }
    }

    if (!processed)
    {
      current_line_inc= 0;
      switch (command->type) {
      case Q_WHILE: do_block(cmd_while, command); break;
      case Q_IF: do_block(cmd_if, command); break;
      case Q_END_BLOCK: do_done(command); break;
      default: current_line_inc = 1; break;
      }
    }
    else
      check_eol_junk(command->last_argument);

    if (command->type != Q_ERROR &&
        command->type != Q_COMMENT)
    {
      /*
        As soon as any non "error" command or comment has been executed,
        the array with expected errors should be cleared
      */
      memset(&saved_expected_errors, 0, sizeof(saved_expected_errors));
    }

    if (command_executed != last_command_executed)
    {
      /*
        As soon as any command has been executed,
        the replace structures should be cleared
      */
      free_all_replace();

      /* Also reset "sorted_result" */
      display_result_sorted= false;
    }
    last_command_executed= command_executed;

    parser.current_line += current_line_inc;
    if ( opt_mark_progress )
      mark_progress(command, parser.current_line);
  }

  start_lineno= 0;

  if (parsing_disabled)
    die("Test ended with parsing disabled");

  /*
    The whole test has been executed _sucessfully_.
    Time to compare result or save it to record file.
    The entire output from test is now kept in ds_res.
  */
  if (ds_res.length())
  {
    if (not result_file_name.empty())
    {
      /* A result file has been specified */

      if (record)
      {
        /* Recording - dump the output from test to result file */
        str_to_file(result_file_name.c_str(), ds_res.c_str(), ds_res.length());
      }
      else
      {
        /* Check that the output from test is equal to result file
           - detect missing result file
           - detect zero size result file
        */
        check_result(ds_res);
      }
    }
    else
    {
      /* No result_file_name specified to compare with, print to stdout */
      printf("%s", ds_res.c_str());
    }
  }
  else
  {
    die("The test didn't produce any output");
  }

  struct stat res_info;
  if (!command_executed && not result_file_name.empty() && not stat(result_file_name.c_str(), &res_info))
  {
    /*
      my_stat() successful on result file. Check if we have not run a
      single query, but we do have a result file that contains data.
      Note that we don't care, if my_stat() fails. For example, for a
      non-existing or non-readable file, we assume it's fine to have
      no query output from the test file, e.g. regarded as no error.
    */
    die("No queries executed but result file found!");
  }

  if ( opt_mark_progress && ! result_file_name.empty() )
    dump_progress();

  /* Dump warning messages */
  if (! result_file_name.empty() && ds_warning_messages.length())
    dump_warning_messages();

  timer_output();
  /* Yes, if we got this far the test has suceeded! Sakila smiles */
  cleanup_and_exit(0);
}

  catch(exception &err)
  {
    cerr<<err.what()<<endl;
  }

  return 0; /* Keep compiler happy too */
}


/*
  A primitive timer that give results in milliseconds if the
  --timer-file=<filename> is given. The timer result is written
  to that file when the result is available. To not confuse
  mysql-test-run with an old obsolete result, we remove the file
  before executing any commands. The time we measure is

  - If no explicit 'start_timer' or 'end_timer' is given in the
  test case, the timer measure how long we execute in drizzletest.

  - If only 'start_timer' is given we measure how long we execute
  from that point until we terminate drizzletest.

  - If only 'end_timer' is given we measure how long we execute
  from that we enter drizzletest to the 'end_timer' is command is
  executed.

  - If both 'start_timer' and 'end_timer' are given we measure
  the time between executing the two commands.
*/

void timer_output()
{
  if (timer_file)
  {
    ostringstream buf;
    uint64_t timer= timer_now() - timer_start;
    buf << timer;
    str_to_file(timer_file,buf.str().c_str(), buf.str().size() );
    /* Timer has been written to the file, don't use it anymore */
    timer_file= 0;
  }
}


uint64_t timer_now()
{
#if defined(HAVE_GETHRTIME)
  return gethrtime()/1000/1000;
#else
  uint64_t newtime;
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0)
  {}
  newtime= (uint64_t)t.tv_sec * 1000000 + t.tv_usec;
  return newtime/1000;
#endif  /* defined(HAVE_GETHRTIME) */
}


/*
  Get arguments for replace_columns. The syntax is:
  replace-column column_number to_string [column_number to_string ...]
  Where each argument may be quoted with ' or "
  A argument may also be a variable, in which case the value of the
  variable is replaced.
*/

void do_get_replace_column(st_command* command)
{
  char *from= command->first_argument;
  char *buff, *start;


  free_replace_column();
  if (!*from)
    die("Missing argument in %s", command->query);

  /* Allocate a buffer for results */
  start= buff= (char *)malloc(strlen(from)+1);
  while (*from)
  {
    uint32_t column_number;

    char *to= get_string(&buff, &from, command);
    if (!(column_number= atoi(to)) || column_number > MAX_COLUMNS)
      die("Wrong column number to replace_column in '%s'", command->query);
    if (!*from)
      die("Wrong number of arguments to replace_column in '%s'", command->query);
    to= get_string(&buff, &from, command);
    free(replace_column[column_number-1]);
    replace_column[column_number-1]= strdup(to);
    set_if_bigger(max_replace_column, column_number);
  }
  free(start);
  command->last_argument= command->end;
}


void free_replace_column()
{
  for (uint32_t i= 0; i < max_replace_column; i++)
  {
    free(replace_column[i]);
    replace_column[i]= 0;
  }
  max_replace_column= 0;
}


/****************************************************************************/
/*
  Replace functions
*/

/* Definitions for replace result */

class POINTER_ARRAY
{    /* when using array-strings */
public:
  ~POINTER_ARRAY();
  int insert(char* name);

  POINTER_ARRAY()
  {
    memset(this, 0, sizeof(*this));
  }

  TYPELIB typelib;        /* Pointer to strings */
  unsigned char *str;          /* Strings is here */
  uint8_t* flag;          /* Flag about each var. */
  uint32_t array_allocs;
  uint32_t max_count;
  uint32_t length;
  uint32_t max_length;
};

struct st_replace;
struct st_replace *init_replace(const char **from, const char **to, uint32_t count,
                                char *word_end_chars);

void replace_strings_append(struct st_replace *rep, string& ds, const char *from, int len);

st_replace *glob_replace= NULL;
// boost::scoped_ptr<st_replace> glob_replace;

/*
  Get arguments for replace. The syntax is:
  replace from to [from to ...]
  Where each argument may be quoted with ' or "
  A argument may also be a variable, in which case the value of the
  variable is replaced.
*/

POINTER_ARRAY::~POINTER_ARRAY()
{
  if (!typelib.count)
    return;
  typelib.count= 0;
  free((char*) typelib.type_names);
  typelib.type_names=0;
  free(str);
}

void do_get_replace(st_command* command)
{
  char *from= command->first_argument;
  if (!*from)
    die("Missing argument in %s", command->query);
  free_replace();
  POINTER_ARRAY to_array, from_array;
  char* start= (char*)malloc(strlen(from) + 1);
  char* buff= start;
  while (*from)
  {
    char *to= get_string(&buff, &from, command);
    if (!*from)
      die("Wrong number of arguments to replace_result in '%s'", command->query);
    from_array.insert(to);
    to= get_string(&buff, &from, command);
    to_array.insert(to);
  }
  char word_end_chars[256];
  char* pos= word_end_chars;
  for (int i= 1; i < 256; i++)
  {
    if (my_isspace(charset_info, i))
      *pos++= i;
  }
  *pos=0;          /* End pointer */
  if (!(glob_replace= init_replace(from_array.typelib.type_names,
                                   to_array.typelib.type_names,
                                   from_array.typelib.count,
                                   word_end_chars)))
    die("Can't initialize replace from '%s'", command->query);
  free(start);
  command->last_argument= command->end;
  return;
}


void free_replace()
{
  free(glob_replace);
  glob_replace=0;
}


typedef struct st_replace {
  bool found;
  struct st_replace *next[256];
} REPLACE;

typedef struct st_replace_found {
  bool found;
  char *replace_string;
  uint32_t to_offset;
  int from_offset;
} REPLACE_STRING;


void replace_strings_append(REPLACE *rep, string& ds, const char *str, int len)
{
  REPLACE_STRING *rep_str;
  const char* start= str;
  const char* from= str;

  REPLACE* rep_pos=rep+1;
  for (;;)
  {
    /* Loop through states */
    while (!rep_pos->found)
      rep_pos= rep_pos->next[(unsigned char) *from++];

    /* Does this state contain a string to be replaced */
    if (!(rep_str = ((REPLACE_STRING*) rep_pos))->replace_string)
    {
      /* No match found */
      ds.append(start, from - start - 1);
      return;
    }

    /* Append part of original string before replace string */
    ds.append(start, (from - rep_str->to_offset) - start);

    /* Append replace string */
    ds.append(rep_str->replace_string, strlen(rep_str->replace_string));

    if (!*(from-=rep_str->from_offset) && rep_pos->found != 2)
      return;

    assert(from <= str+len);
    start= from;
    rep_pos=rep;
  }
}


/*
  Regex replace  functions
*/


/* Stores regex substitutions */

struct st_regex
{
  char* pattern; /* Pattern to be replaced */
  char* replace; /* String or expression to replace the pattern with */
  int icase; /* true if the match is case insensitive */
  int global; /* true if the match should be global -- 
                 i.e. repeat the matching until the end of the string */
};

class st_replace_regex
{
public:
  st_replace_regex(char* expr);
  int multi_reg_replace(char* val);

  /*
    Temporary storage areas for substitutions. To reduce unnessary copying
    and memory freeing/allocation, we pre-allocate two buffers, and alternate
    their use, one for input/one for output, the roles changing on the next
    st_regex substition. At the end of substitutions  buf points to the
    one containing the final result.
  */
  typedef vector<st_regex> regex_arr_t;

  char* buf_;
  char* even_buf;
  char* odd_buf;
  int even_buf_len;
  int odd_buf_len;
  boost::array<char, 8 << 10> buf0_;
  boost::array<char, 8 << 10> buf1_;
  regex_arr_t regex_arr;
};

boost::scoped_ptr<st_replace_regex> glob_replace_regex;

int reg_replace(char** buf_p, int* buf_len_p, char *pattern, char *replace,
                char *string, int icase, int global);



/*
  Finds the next (non-escaped) '/' in the expression.
  (If the character '/' is needed, it can be escaped using '\'.)
*/

#define PARSE_REGEX_ARG                         \
  while (p < expr_end)                          \
  {                                             \
    char c= *p;                                 \
    if (c == '/')                               \
    {                                           \
      if (last_c == '\\')                       \
      {                                         \
        buf_p[-1]= '/';                         \
      }                                         \
      else                                      \
      {                                         \
        *buf_p++ = 0;                           \
        break;                                  \
      }                                         \
    }                                           \
    else                                        \
      *buf_p++ = c;                             \
                                                \
    last_c= c;                                  \
    p++;                                        \
  }                                             \
                                                \
/*
  Initializes the regular substitution expression to be used in the
  result output of test.

  Returns: st_replace_regex struct with pairs of substitutions
*/

st_replace_regex::st_replace_regex(char* expr)
{
  uint32_t expr_len= strlen(expr);
  char last_c = 0;
  st_regex reg;

  char* buf= new char[expr_len];
  char* expr_end= expr + expr_len;
  char* p= expr;
  char* buf_p= buf;

  /* for each regexp substitution statement */
  while (p < expr_end)
  {
    memset(&reg, 0, sizeof(reg));
    /* find the start of the statement */
    while (p < expr_end)
    {
      if (*p == '/')
        break;
      p++;
    }

    if (p == expr_end || ++p == expr_end)
    {
      if (!regex_arr.empty())
        break;
      else
        goto err;
    }
    /* we found the start */
    reg.pattern= buf_p;

    /* Find first argument -- pattern string to be removed */
    PARSE_REGEX_ARG

      if (p == expr_end || ++p == expr_end)
        goto err;

    /* buf_p now points to the replacement pattern terminated with \0 */
    reg.replace= buf_p;

    /* Find second argument -- replace string to replace pattern */
    PARSE_REGEX_ARG

      if (p == expr_end)
        goto err;

    /* skip the ending '/' in the statement */
    p++;

    /* Check if we should do matching case insensitive */
    if (p < expr_end && *p == 'i')
    {
      p++;
      reg.icase= 1;
    }

    /* Check if we should do matching globally */
    if (p < expr_end && *p == 'g')
    {
      p++;
      reg.global= 1;
    }
    regex_arr.push_back(reg);
  }
  odd_buf_len= even_buf_len= buf0_.size();
  even_buf= buf0_.data();
  odd_buf= buf1_.data();
  buf_= even_buf;

  return;

err:
  die("Error parsing replace_regex \"%s\"", expr);
}

/*
  Execute all substitutions on val.

  Returns: true if substituition was made, false otherwise
  Side-effect: Sets r->buf to be the buffer with all substitutions done.

  IN:
  struct st_replace_regex* r
  char* val
  Out:
  struct st_replace_regex* r
  r->buf points at the resulting buffer
  r->even_buf and r->odd_buf might have been reallocated
  r->even_buf_len and r->odd_buf_len might have been changed

  TODO:  at some point figure out if there is a way to do everything
  in one pass
*/

int st_replace_regex::multi_reg_replace(char* val)
{
  char* in_buf= val;
  char* out_buf= even_buf;
  int* buf_len_p= &even_buf_len;
  buf_= 0;

  /* For each substitution, do the replace */
  BOOST_FOREACH(regex_arr_t::const_reference i, regex_arr)
  {
    char* save_out_buf= out_buf;
    if (!reg_replace(&out_buf, buf_len_p, i.pattern, i.replace,
                     in_buf, i.icase, i.global))
    {
      /* if the buffer has been reallocated, make adjustements */
      if (save_out_buf != out_buf)
      {
        if (save_out_buf == even_buf)
          even_buf= out_buf;
        else
          odd_buf= out_buf;
      }
      buf_= out_buf;
      if (in_buf == val)
        in_buf= odd_buf;
      std::swap(in_buf, out_buf);
      buf_len_p= (out_buf == even_buf) ? &even_buf_len : &odd_buf_len;
    }
  }
  return buf_ == 0;
}

/*
  Parse the regular expression to be used in all result files
  from now on.

  The syntax is --replace_regex /from/to/i /from/to/i ...
  i means case-insensitive match. If omitted, the match is
  case-sensitive

*/
void do_get_replace_regex(st_command* command)
{
  char *expr= command->first_argument;
  glob_replace_regex.reset(new st_replace_regex(expr));
  command->last_argument= command->end;
}

/*
  Performs a regex substitution

  IN:

  buf_p - result buffer pointer. Will change if reallocated
  buf_len_p - result buffer length. Will change if the buffer is reallocated
  pattern - regexp pattern to match
  replace - replacement expression
  string - the string to perform substituions in
  icase - flag, if set to 1 the match is case insensitive
*/
int reg_replace(char** buf_p, int* buf_len_p, char *pattern,
                char *replace, char *in_string, int icase, int global)
{
  const char *error= NULL;
  int erroffset;
  int ovector[3];
  pcre *re= pcre_compile(pattern,
                         icase ? PCRE_CASELESS | PCRE_MULTILINE : PCRE_MULTILINE,
                         &error, &erroffset, NULL);
  if (re == NULL)
    return 1;

  if (! global)
  {

    int rc= pcre_exec(re, NULL, in_string, (int)strlen(in_string),
                      0, 0, ovector, 3);
    if (rc < 0)
    {
      pcre_free(re);
      return 1;
    }

    char *substring_to_replace= in_string + ovector[0];
    int substring_length= ovector[1] - ovector[0];
    *buf_len_p= strlen(in_string) - substring_length + strlen(replace);
    char* new_buf= (char*)malloc(*buf_len_p+1);

    memset(new_buf, 0, *buf_len_p+1);
    strncpy(new_buf, in_string, substring_to_replace-in_string);
    strncpy(new_buf+(substring_to_replace-in_string), replace, strlen(replace));
    strncpy(new_buf+(substring_to_replace-in_string)+strlen(replace),
            substring_to_replace + substring_length,
            strlen(in_string)
              - substring_length
              - (substring_to_replace-in_string));
    *buf_p= new_buf;

    pcre_free(re);
    return 0;
  }
  else
  {
    /* Repeatedly replace the string with the matched regex */
    string subject(in_string);
    size_t replace_length= strlen(replace);
    size_t length_of_replacement= strlen(replace);
    size_t current_position= 0;
    int rc= 0;

    while (true) 
    {
      rc= pcre_exec(re, NULL, subject.c_str(), subject.length(), 
                    current_position, 0, ovector, 3);
      if (rc < 0)
      {
        break;
      }

      current_position= static_cast<size_t>(ovector[0]);
      replace_length= static_cast<size_t>(ovector[1] - ovector[0]);
      subject.replace(current_position, replace_length, replace, length_of_replacement);
      current_position= current_position + length_of_replacement;
    }

    char* new_buf = (char*) malloc(subject.length() + 1);
    memset(new_buf, 0, subject.length() + 1);
    strncpy(new_buf, subject.c_str(), subject.length());
    *buf_len_p= subject.length() + 1;
    *buf_p= new_buf;
          
    pcre_free(re);
    return 0;
  }
}


#ifndef WORD_BIT
#define WORD_BIT (8*sizeof(uint32_t))
#endif

#define SET_MALLOC_HUNC 64
#define LAST_CHAR_CODE 259

class REP_SET
{
public:
  void internal_set_bit(uint32_t bit);
  void internal_clear_bit(uint32_t bit);
  void or_bits(const REP_SET *from);
  void copy_bits(const REP_SET *from);
  int cmp_bits(const REP_SET *set2) const;
  int get_next_bit(uint32_t lastpos) const;

  uint32_t  *bits;        /* Pointer to used sets */
  short next[LAST_CHAR_CODE];    /* Pointer to next sets */
  uint32_t  found_len;      /* Best match to date */
  int  found_offset;
  uint32_t  table_offset;
  uint32_t  size_of_bits;      /* For convinience */
};

class REP_SETS
{
public:
  int find_set(const REP_SET *find);
  void free_last_set();
  void free_sets();
  void make_sets_invisible();

  uint32_t    count;      /* Number of sets */
  uint32_t    extra;      /* Extra sets in buffer */
  uint32_t    invisible;    /* Sets not shown */
  uint32_t    size_of_bits;
  REP_SET  *set,*set_buffer;
  uint32_t    *bit_buffer;
};

struct FOUND_SET 
{
  uint32_t table_offset;
  int found_offset;
};

struct FOLLOWS
{
  int chr;
  uint32_t table_offset;
  uint32_t len;
};

void init_sets(REP_SETS *sets, uint32_t states);
REP_SET *make_new_set(REP_SETS *sets);
int find_found(FOUND_SET *found_set, uint32_t table_offset, int found_offset);

static uint32_t found_sets= 0;

static uint32_t replace_len(const char *str)
{
  uint32_t len=0;
  while (*str)
  {
    if (str[0] == '\\' && str[1])
      str++;
    str++;
    len++;
  }
  return len;
}

/* Return 1 if regexp starts with \b or ends with \b*/

static bool start_at_word(const char *pos)
{
  return (!memcmp(pos, "\\b",2) && pos[2]) || !memcmp(pos, "\\^", 2);
}

static bool end_of_word(const char *pos)
{
  const char *end= strchr(pos, '\0');
  return (end > pos+2 && !memcmp(end-2, "\\b", 2)) || (end >= pos+2 && !memcmp(end-2, "\\$",2));
}

/* Init a replace structure for further calls */

REPLACE *init_replace(const char **from, const char **to, uint32_t count, char *word_end_chars)
{
  const int SPACE_CHAR= 256;
  const int START_OF_LINE= 257;
  const int END_OF_LINE= 258;

  uint32_t i,j,states,set_nr,len,result_len,max_length,found_end,bits_set,bit_nr;
  int used_sets,chr,default_state;
  char used_chars[LAST_CHAR_CODE],is_word_end[256];
  char *to_pos, **to_array;

  /* Count number of states */
  for (i=result_len=max_length=0 , states=2; i < count; i++)
  {
    len=replace_len(from[i]);
    if (!len)
    {
      errno=EINVAL;
      return(0);
    }
    states+=len+1;
    result_len+=(uint32_t) strlen(to[i])+1;
    if (len > max_length)
      max_length=len;
  }
  memset(is_word_end, 0, sizeof(is_word_end));
  for (i=0; word_end_chars[i]; i++)
    is_word_end[(unsigned char) word_end_chars[i]]=1;

  REP_SETS sets;
  REP_SET *set,*start_states,*word_states,*new_set;
  REPLACE_STRING *rep_str;
  init_sets(&sets, states);
  found_sets=0;
  vector<FOUND_SET> found_set(max_length * count);
  make_new_set(&sets);      /* Set starting set */
  sets.make_sets_invisible();      /* Hide previus sets */
  used_sets=-1;
  word_states=make_new_set(&sets);    /* Start of new word */
  start_states=make_new_set(&sets);    /* This is first state */
  vector<FOLLOWS> follow(states + 2);
  FOLLOWS *follow_ptr= &follow[1];
  /* Init follow_ptr[] */
  for (i=0, states=1; i < count; i++)
  {
    if (from[i][0] == '\\' && from[i][1] == '^')
    {
      start_states->internal_set_bit(states + 1);
      if (!from[i][2])
      {
        start_states->table_offset=i;
        start_states->found_offset=1;
      }
    }
    else if (from[i][0] == '\\' && from[i][1] == '$')
    {
      start_states->internal_set_bit(states);
      word_states->internal_set_bit(states);
      if (!from[i][2] && start_states->table_offset == UINT32_MAX)
      {
        start_states->table_offset=i;
        start_states->found_offset=0;
      }
    }
    else
    {
      word_states->internal_set_bit(states);
      if (from[i][0] == '\\' && (from[i][1] == 'b' && from[i][2]))
        start_states->internal_set_bit(states + 1);
      else
        start_states->internal_set_bit(states);
    }
    const char *pos;
    for (pos= from[i], len=0; *pos; pos++)
    {
      if (*pos == '\\' && *(pos+1))
      {
        pos++;
        switch (*pos) {
        case 'b':
          follow_ptr->chr = SPACE_CHAR;
          break;
        case '^':
          follow_ptr->chr = START_OF_LINE;
          break;
        case '$':
          follow_ptr->chr = END_OF_LINE;
          break;
        case 'r':
          follow_ptr->chr = '\r';
          break;
        case 't':
          follow_ptr->chr = '\t';
          break;
        case 'v':
          follow_ptr->chr = '\v';
          break;
        default:
          follow_ptr->chr = (unsigned char) *pos;
          break;
        }
      }
      else
        follow_ptr->chr= (unsigned char) *pos;
      follow_ptr->table_offset=i;
      follow_ptr->len= ++len;
      follow_ptr++;
    }
    follow_ptr->chr=0;
    follow_ptr->table_offset=i;
    follow_ptr->len=len;
    follow_ptr++;
    states+=(uint32_t) len+1;
  }


  for (set_nr=0; set_nr < sets.count; set_nr++)
  {
    set=sets.set+set_nr;
    default_state= 0;        /* Start from beginning */

    /* If end of found-string not found or start-set with current set */

    for (i= UINT32_MAX; (i= set->get_next_bit(i));)
    {
      if (!follow[i].chr && !default_state)
        default_state= find_found(&found_set.front(), set->table_offset, set->found_offset+1);
    }
    sets.set[used_sets].copy_bits(set);    /* Save set for changes */
    if (!default_state)
      sets.set[used_sets].or_bits(sets.set);  /* Can restart from start */

    /* Find all chars that follows current sets */
    memset(used_chars, 0, sizeof(used_chars));
    for (i= UINT32_MAX; (i= sets.set[used_sets].get_next_bit(i));)
    {
      used_chars[follow[i].chr]=1;
      if ((follow[i].chr == SPACE_CHAR && !follow[i+1].chr &&
           follow[i].len > 1) || follow[i].chr == END_OF_LINE)
        used_chars[0]=1;
    }

    /* Mark word_chars used if \b is in state */
    if (used_chars[SPACE_CHAR])
      for (const char *pos= word_end_chars; *pos; pos++)
        used_chars[(int) (unsigned char) *pos] = 1;

    /* Handle other used characters */
    for (chr= 0; chr < 256; chr++)
    {
      if (! used_chars[chr])
        set->next[chr]= chr ? default_state : -1;
      else
      {
        new_set=make_new_set(&sets);
        set=sets.set+set_nr;      /* if realloc */
        new_set->table_offset=set->table_offset;
        new_set->found_len=set->found_len;
        new_set->found_offset=set->found_offset+1;
        found_end=0;

        for (i= UINT32_MAX; (i= sets.set[used_sets].get_next_bit(i));)
        {
          if (!follow[i].chr || follow[i].chr == chr ||
              (follow[i].chr == SPACE_CHAR &&
               (is_word_end[chr] ||
                (!chr && follow[i].len > 1 && ! follow[i+1].chr))) ||
              (follow[i].chr == END_OF_LINE && ! chr))
          {
            if ((! chr || (follow[i].chr && !follow[i+1].chr)) &&
                follow[i].len > found_end)
              found_end=follow[i].len;
            if (chr && follow[i].chr)
              new_set->internal_set_bit(i + 1);    /* To next set */
            else
              new_set->internal_set_bit(i);
          }
        }
        if (found_end)
        {
          new_set->found_len=0;      /* Set for testing if first */
          bits_set=0;
          for (i= UINT32_MAX; (i= new_set->get_next_bit(i));)
          {
            if ((follow[i].chr == SPACE_CHAR ||
                 follow[i].chr == END_OF_LINE) && ! chr)
              bit_nr=i+1;
            else
              bit_nr=i;
            if (follow[bit_nr-1].len < found_end ||
                (new_set->found_len &&
                 (chr == 0 || !follow[bit_nr].chr)))
              new_set->internal_clear_bit(i);
            else
            {
              if (chr == 0 || !follow[bit_nr].chr)
              {          /* best match  */
                new_set->table_offset=follow[bit_nr].table_offset;
                if (chr || (follow[i].chr == SPACE_CHAR ||
                            follow[i].chr == END_OF_LINE))
                  new_set->found_offset=found_end;  /* New match */
                new_set->found_len=found_end;
              }
              bits_set++;
            }
          }
          if (bits_set == 1)
          {
            set->next[chr] = find_found(&found_set.front(), new_set->table_offset, new_set->found_offset);
            sets.free_last_set();
          }
          else
            set->next[chr] = sets.find_set(new_set);
        }
        else
          set->next[chr] = sets.find_set(new_set);
      }
    }
  }

  /* Alloc replace structure for the replace-state-machine */

  REPLACE *replace= (REPLACE*)malloc(sizeof(REPLACE) * (sets.count)
    + sizeof(REPLACE_STRING) * (found_sets + 1) + sizeof(char*) * count + result_len);
  {
    memset(replace, 0, sizeof(REPLACE)*(sets.count)+
                       sizeof(REPLACE_STRING)*(found_sets+1)+
                       sizeof(char *)*count+result_len);
    rep_str=(REPLACE_STRING*) (replace+sets.count);
    to_array= (char **) (rep_str+found_sets+1);
    to_pos=(char *) (to_array+count);
    for (i=0; i < count; i++)
    {
      to_array[i]=to_pos;
      to_pos=strcpy(to_pos,to[i])+strlen(to[i])+1;
    }
    rep_str[0].found=1;
    rep_str[0].replace_string=0;
    for (i=1; i <= found_sets; i++)
    {
      const char *pos= from[found_set[i-1].table_offset];
      rep_str[i].found= !memcmp(pos, "\\^", 3) ? 2 : 1;
      rep_str[i].replace_string= to_array[found_set[i-1].table_offset];
      rep_str[i].to_offset= found_set[i-1].found_offset-start_at_word(pos);
      rep_str[i].from_offset= found_set[i-1].found_offset-replace_len(pos) + end_of_word(pos);
    }
    for (i=0; i < sets.count; i++)
    {
      for (j=0; j < 256; j++)
        if (sets.set[i].next[j] >= 0)
          replace[i].next[j]=replace+sets.set[i].next[j];
        else
          replace[i].next[j]=(REPLACE*) (rep_str+(-sets.set[i].next[j]-1));
    }
  }
  sets.free_sets();
  return replace;
}


void init_sets(REP_SETS *sets,uint32_t states)
{
  memset(sets, 0, sizeof(*sets));
  sets->size_of_bits=((states+7)/8);
  sets->set_buffer=(REP_SET*) malloc(sizeof(REP_SET) * SET_MALLOC_HUNC);
  sets->bit_buffer=(uint*) malloc(sizeof(uint32_t) * sets->size_of_bits * SET_MALLOC_HUNC);
}

/* Make help sets invisible for nicer codeing */

void REP_SETS::make_sets_invisible()
{
  invisible= count;
  set += count;
  count= 0;
}

REP_SET *make_new_set(REP_SETS *sets)
{
  REP_SET *set;
  if (sets->extra)
  {
    sets->extra--;
    set=sets->set+ sets->count++;
    memset(set->bits, 0, sizeof(uint32_t)*sets->size_of_bits);
    memset(&set->next[0], 0, sizeof(set->next[0])*LAST_CHAR_CODE);
    set->found_offset=0;
    set->found_len=0;
    set->table_offset= UINT32_MAX;
    set->size_of_bits=sets->size_of_bits;
    return set;
  }
  uint32_t count= sets->count + sets->invisible + SET_MALLOC_HUNC;
  set= (REP_SET*) realloc((unsigned char*) sets->set_buffer, sizeof(REP_SET)*count);
  sets->set_buffer=set;
  sets->set=set+sets->invisible;
  uint32_t* bit_buffer= (uint*) realloc((unsigned char*) sets->bit_buffer, (sizeof(uint32_t)*sets->size_of_bits)*count);
  sets->bit_buffer=bit_buffer;
  for (uint32_t i= 0; i < count; i++)
  {
    sets->set_buffer[i].bits=bit_buffer;
    bit_buffer+=sets->size_of_bits;
  }
  sets->extra=SET_MALLOC_HUNC;
  return make_new_set(sets);
}

void REP_SETS::free_last_set()
{
  count--;
  extra++;
}

void REP_SETS::free_sets()
{
  free(set_buffer);
  free(bit_buffer);
}

void REP_SET::internal_set_bit(uint32_t bit)
{
  bits[bit / WORD_BIT] |= 1 << (bit % WORD_BIT);
}

void REP_SET::internal_clear_bit(uint32_t bit)
{
  bits[bit / WORD_BIT] &= ~ (1 << (bit % WORD_BIT));
}


void REP_SET::or_bits(const REP_SET *from)
{
  for (uint32_t i= 0; i < size_of_bits; i++)
    bits[i]|=from->bits[i];
}

void REP_SET::copy_bits(const REP_SET *from)
{
  memcpy(bits, from->bits, sizeof(uint32_t) * size_of_bits);
}

int REP_SET::cmp_bits(const REP_SET *set2) const
{
  return memcmp(bits, set2->bits, sizeof(uint32_t) * size_of_bits);
}

/* Get next set bit from set. */

int REP_SET::get_next_bit(uint32_t lastpos) const
{
  uint32_t *start= bits + ((lastpos+1) / WORD_BIT);
  uint32_t *end= bits + size_of_bits;
  uint32_t bits0= start[0] & ~((1 << ((lastpos+1) % WORD_BIT)) -1);

  while (!bits0 && ++start < end)
    bits0= start[0];
  if (!bits0)
    return 0;
  uint32_t pos= (start - bits) * WORD_BIT;
  while (!(bits0 & 1))
  {
    bits0 >>=1;
    pos++;
  }
  return pos;
}

/* find if there is a same set in sets. If there is, use it and
   free given set, else put in given set in sets and return its
   position */

int REP_SETS::find_set(const REP_SET *find)
{
  uint32_t i= 0;
  for (; i < count - 1; i++)
  {
    if (!set[i].cmp_bits(find))
    {
      free_last_set();
      return i;
    }
  }
  return i;        /* return new postion */
}

/* find if there is a found_set with same table_offset & found_offset
   If there is return offset to it, else add new offset and return pos.
   Pos returned is -offset-2 in found_set_structure because it is
   saved in set->next and set->next[] >= 0 points to next set and
   set->next[] == -1 is reserved for end without replaces.
*/

int find_found(FOUND_SET *found_set, uint32_t table_offset, int found_offset)
{
  uint32_t i= 0;
  for (; i < found_sets; i++)
  {
    if (found_set[i].table_offset == table_offset &&
        found_set[i].found_offset == found_offset)
      return - i - 2;
  }
  found_set[i].table_offset= table_offset;
  found_set[i].found_offset= found_offset;
  found_sets++;
  return - i - 2; // return new postion
}

/****************************************************************************
 * Handle replacement of strings
 ****************************************************************************/

#define PC_MALLOC    256  /* Bytes for pointers */
#define PS_MALLOC    512  /* Bytes for data */

static int insert_pointer_name(POINTER_ARRAY* pa, char* name)
{
  uint32_t i,length,old_count;
  unsigned char *new_pos;
  const char **new_array;


  if (! pa->typelib.count)
  {
    pa->typelib.type_names=(const char **)
          malloc(((PC_MALLOC-MALLOC_OVERHEAD)/
                     (sizeof(char *)+sizeof(*pa->flag))*
                     (sizeof(char *)+sizeof(*pa->flag))));
    pa->str= (unsigned char*) malloc(PS_MALLOC-MALLOC_OVERHEAD);
    pa->max_count=(PC_MALLOC-MALLOC_OVERHEAD)/(sizeof(unsigned char*)+
                                               sizeof(*pa->flag));
    pa->flag= (uint8_t*) (pa->typelib.type_names+pa->max_count);
    pa->length=0;
    pa->max_length=PS_MALLOC-MALLOC_OVERHEAD;
    pa->array_allocs=1;
  }
  length=(uint32_t) strlen(name)+1;
  if (pa->length+length >= pa->max_length)
  {
    new_pos= (unsigned char*)realloc((unsigned char*)pa->str, (size_t)(pa->max_length+PS_MALLOC));
    if (new_pos != pa->str)
    {
      ptrdiff_t diff= PTR_BYTE_DIFF(new_pos,pa->str);
      for (i=0; i < pa->typelib.count; i++)
        pa->typelib.type_names[i]= ADD_TO_PTR(pa->typelib.type_names[i],diff,
                                              char*);
      pa->str=new_pos;
    }
    pa->max_length+=PS_MALLOC;
  }
  if (pa->typelib.count >= pa->max_count-1)
  {
    size_t len;
    pa->array_allocs++;
    len=(PC_MALLOC*pa->array_allocs - MALLOC_OVERHEAD);
    new_array= (const char **)realloc((unsigned char*) pa->typelib.type_names,
                                 len/
                                  (sizeof(unsigned char*)+sizeof(*pa->flag))*
                                  (sizeof(unsigned char*)+sizeof(*pa->flag)));
    pa->typelib.type_names=new_array;
    old_count=pa->max_count;
    pa->max_count=len/(sizeof(unsigned char*) + sizeof(*pa->flag));
    pa->flag= (uint8_t*) (pa->typelib.type_names+pa->max_count);
    memcpy(pa->flag, pa->typelib.type_names+old_count,
           old_count*sizeof(*pa->flag));
  }
  pa->flag[pa->typelib.count]=0;      /* Reset flag */
  pa->typelib.type_names[pa->typelib.count++]= (char*) pa->str+pa->length;
  pa->typelib.type_names[pa->typelib.count]= NULL;  /* Put end-mark */
  strcpy((char*) pa->str+pa->length,name);
  pa->length+=length;
  return(0);
} /* insert_pointer_name */

int POINTER_ARRAY::insert(char* name)
{
  return insert_pointer_name(this, name);
}


/* Functions that uses replace and replace_regex */

/* Append the string to ds, with optional replace */
void replace_append_mem(string& ds, const char *val, int len)
{
  char *v= strdup(val);

  if (glob_replace_regex && !glob_replace_regex->multi_reg_replace(v))
  {
    v= glob_replace_regex->buf_;
    len= strlen(v);
  }
  if (glob_replace)
  {
    /* Normal replace */
    replace_strings_append(glob_replace, ds, v, len);
  }
  else
    ds.append(v, len);
}


/* Append zero-terminated string to ds, with optional replace */
void replace_append(string *ds, const char *val)
{
  replace_append_mem(*ds, val, strlen(val));
}

/* Append uint32_t to ds, with optional replace */
void replace_append_uint(string& ds, uint32_t val)
{
  ostringstream buff;
  buff << val;
  replace_append_mem(ds, buff.str().c_str(), buff.str().size());

}



/*
  Build a list of pointer to each line in ds_input, sort
  the list and use the sorted list to append the strings
  sorted to the output ds

  SYNOPSIS
  dynstr_append_sorted
  ds - string where the sorted output will be appended
  ds_input - string to be sorted

*/


void append_sorted(string& ds, const string& ds_input)
{
  priority_queue<string, vector<string>, greater<string> > lines;

  if (ds_input.empty())
    return;  /* No input */

  unsigned long eol_pos= ds_input.find_first_of('\n', 0);
  if (eol_pos == string::npos)
    return; // We should have at least one header here

  ds.append(ds_input.substr(0, eol_pos+1));

  unsigned long start_pos= eol_pos+1;

  /* Insert line(s) in array */
  do {

    eol_pos= ds_input.find_first_of('\n', start_pos);
    /* Find end of line */
    lines.push(ds_input.substr(start_pos, eol_pos-start_pos+1));
    start_pos= eol_pos+1;

  } while ( eol_pos != string::npos);

  /* Create new result */
  while (!lines.empty()) 
  {
    ds.append(lines.top());
    lines.pop();
  }
}

static void free_all_replace()
{
  free_replace();
  glob_replace_regex.reset();
  free_replace_column();
}
