/* Copyright (C) 2008 Drizzle Open Source Development Team 

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
#include <pcrecpp.h>

#include "client_priv.h"
#include <drizzle_version.h>
#include <drizzled_error.h>
#include <my_dir.h>
#include <mysys/hash.h>
#include <stdarg.h>
#include <vio/violite.h>


#define MAX_VAR_NAME_LENGTH    256
#define MAX_COLUMNS            256
#define MAX_EMBEDDED_SERVER_ARGS 64
#define MAX_DELIMITER_LENGTH 16

/* Flags controlling send and reap */
#define QUERY_SEND_FLAG  1
#define QUERY_REAP_FLAG  2

enum {
  OPT_SKIP_SAFEMALLOC=OPT_MAX_CLIENT_OPTION,
  OPT_PS_PROTOCOL, OPT_SP_PROTOCOL, OPT_CURSOR_PROTOCOL, OPT_VIEW_PROTOCOL,
  OPT_MAX_CONNECT_RETRIES, OPT_MARK_PROGRESS, OPT_LOG_DIR, OPT_TAIL_LINES
};

static int record= 0, opt_sleep= -1;
static char *opt_db= 0, *opt_pass= 0;
const char *opt_user= 0, *opt_host= 0, *unix_sock= 0, *opt_basedir= "./";
const char *opt_logdir= "";
const char *opt_include= 0, *opt_charsets_dir;
static int opt_port= 0;
static int opt_max_connect_retries;
static bool opt_compress= 0, silent= 0, verbose= 0;
static bool debug_info_flag= 0, debug_check_flag= 0;
static bool tty_password= 0;
static bool opt_mark_progress= 0;
static bool parsing_disabled= 0;
static bool display_result_vertically= false,
  display_metadata= false, display_result_sorted= false;
static bool disable_query_log= 0, disable_result_log= 0;
static bool disable_warnings= 0;
static bool disable_info= 1;
static bool abort_on_error= 1;
static bool server_initialized= 0;
static bool is_windows= 0;
static char **default_argv;
static const char *load_default_groups[]= { "drizzletest", "client", 0 };
static char line_buffer[MAX_DELIMITER_LENGTH], *line_buffer_pos= line_buffer;

static uint start_lineno= 0; /* Start line of current command */
static uint my_end_arg= 0;

/* Number of lines of the result to include in failure report */
static uint opt_tail_lines= 0;

static char delimiter[MAX_DELIMITER_LENGTH]= ";";
static uint delimiter_length= 1;

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
  uint lineno; /* Current line in file */
};

static struct st_test_file file_stack[16];
static struct st_test_file* cur_file;
static struct st_test_file* file_stack_end;


static CHARSET_INFO *charset_info= &my_charset_latin1; /* Default charset */

static int embedded_server_arg_count=0;
static char *embedded_server_args[MAX_EMBEDDED_SERVER_ARGS];

/*
  Timer related variables
  See the timer_output() definition for details
*/
static char *timer_file = NULL;
static uint64_t timer_start;
static void timer_output(void);
static uint64_t timer_now(void);

static uint64_t progress_start= 0;

DYNAMIC_ARRAY q_lines;

typedef struct {
  int read_lines,current_line;
} parser_st;
parser_st parser;

typedef struct
{
  char file[FN_REFLEN];
  ulong pos;
} master_pos_st;

master_pos_st master_pos;

/* if set, all results are concated and compared against this file */
const char *result_file_name= 0;

typedef struct st_var
{
  char *name;
  int name_len;
  char *str_val;
  int str_val_len;
  int int_val;
  int alloced_len;
  int int_dirty; /* do not update string if int is updated until first read */
  int alloced;
  char *env_s;
} VAR;

/*Perl/shell-like variable registers */
VAR var_reg[10];

HASH var_hash;

struct st_connection
{
  DRIZZLE drizzle;
  /* Used when creating views and sp, to avoid implicit commit */
  DRIZZLE *util_drizzle;
  char *name;
};
struct st_connection connections[128];
struct st_connection* cur_con= NULL, *next_con, *connections_end;

/*
  List of commands in drizzletest
  Must match the "command_names" array
  Add new commands before Q_UNKNOWN!
*/
enum enum_commands {
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
  Q_CHARACTER_SET, Q_DISABLE_PS_PROTOCOL, Q_ENABLE_PS_PROTOCOL,
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
    uint errnum;
    char sqlstate[SQLSTATE_LENGTH+1];  /* \0 terminated string */
  } code;
};

struct st_expected_errors
{
  struct st_match_err err[10];
  uint count;
};
static struct st_expected_errors saved_expected_errors;

struct st_command
{
  char *query, *query_buf,*first_argument,*last_argument,*end;
  int first_word_len, query_len;
  bool abort_on_error;
  struct st_expected_errors expected_errors;
  char require_file[FN_REFLEN];
  enum enum_commands type;
};

TYPELIB command_typelib= {array_elements(command_names),"",
        command_names, 0};

DYNAMIC_STRING ds_res, ds_progress, ds_warning_messages;

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
void var_free(void* v);
VAR* var_get(const char *var_name, const char** var_name_end,
             bool raw, bool ignore_not_existing);
void eval_expr(VAR* v, const char *p, const char** p_end);
bool match_delimiter(int c, const char *delim, uint length);
void dump_result_to_reject_file(char *buf, int size);
void dump_result_to_log_file(char *buf, int size);
void dump_warning_messages(void);
void dump_progress(void);

void do_eval(DYNAMIC_STRING *query_eval, const char *query,
             const char *query_end, bool pass_through_escape_chars);
void str_to_file(const char *fname, char *str, int size);
void str_to_file2(const char *fname, char *str, int size, bool append);

/* For replace_column */
static char *replace_column[MAX_COLUMNS];
static uint max_replace_column= 0;
void do_get_replace_column(struct st_command*);
void free_replace_column(void);

/* For replace */
void do_get_replace(struct st_command *command);
void free_replace(void);

/* For replace_regex */
void do_get_replace_regex(struct st_command *command);
void free_replace_regex(void);


void free_all_replace(void);


void free_all_replace(void){
  free_replace();
  free_replace_regex();
  free_replace_column();
}

void replace_dynstr_append_mem(DYNAMIC_STRING *ds, const char *val,
                               int len);
void replace_dynstr_append(DYNAMIC_STRING *ds, const char *val);
void replace_dynstr_append_uint(DYNAMIC_STRING *ds, uint val);
void dynstr_append_sorted(DYNAMIC_STRING* ds, DYNAMIC_STRING* ds_input);

void handle_error(struct st_command*,
                  unsigned int err_errno, const char *err_error,
                  const char *err_sqlstate, DYNAMIC_STRING *ds);
void handle_no_error(struct st_command*);

#ifdef EMBEDDED_LIBRARY

/* attributes of the query thread */
pthread_attr_t cn_thd_attrib;

/*
  send_one_query executes query in separate thread, which is
  necessary in embedded library to run 'send' in proper way.
  This implementation doesn't handle errors returned
  by drizzle_send_query. It's technically possible, though
  I don't see where it is needed.
*/
pthread_handler_t send_one_query(void *arg)
{
  struct st_connection *cn= (struct st_connection*)arg;

  drizzle_thread_init();
  VOID(drizzle_send_query(&cn->drizzle, cn->cur_query, cn->cur_query_len));

  drizzle_thread_end();
  pthread_mutex_lock(&cn->mutex);
  cn->query_done= 1;
  VOID(pthread_cond_signal(&cn->cond));
  pthread_mutex_unlock(&cn->mutex);
  pthread_exit(0);
  return 0;
}

static int do_send_query(struct st_connection *cn, const char *q, int q_len,
                         int flags)
{
  pthread_t tid;

  if (flags & QUERY_REAP_FLAG)
    return drizzle_send_query(&cn->drizzle, q, q_len);

  if (pthread_mutex_init(&cn->mutex, NULL) ||
      pthread_cond_init(&cn->cond, NULL))
    die("Error in the thread library");

  cn->cur_query= q;
  cn->cur_query_len= q_len;
  cn->query_done= 0;
  if (pthread_create(&tid, &cn_thd_attrib, send_one_query, (void*)cn))
    die("Cannot start new thread for query");

  return 0;
}

static void wait_query_thread_end(struct st_connection *con)
{
  if (!con->query_done)
  {
    pthread_mutex_lock(&con->mutex);
    while (!con->query_done)
      pthread_cond_wait(&con->cond, &con->mutex);
    pthread_mutex_unlock(&con->mutex);
  }
}

#else /*EMBEDDED_LIBRARY*/

#define do_send_query(cn,q,q_len,flags) drizzle_send_query(&cn->drizzle, q, q_len)

#endif /*EMBEDDED_LIBRARY*/

void do_eval(DYNAMIC_STRING *query_eval, const char *query,
             const char *query_end, bool pass_through_escape_chars)
{
  const char *p;
  register char c, next_c;
  register int escaped = 0;
  VAR *v;


  for (p= query; (c= *p) && p < query_end; ++p)
  {
    switch(c) {
    case '$':
      if (escaped)
      {
  escaped= 0;
  dynstr_append_mem(query_eval, p, 1);
      }
      else
      {
  if (!(v= var_get(p, &p, 0, 0)))
    die("Bad variable in eval");
  dynstr_append_mem(query_eval, v->str_val, v->str_val_len);
      }
      break;
    case '\\':
      next_c= *(p+1);
      if (escaped)
      {
  escaped= 0;
  dynstr_append_mem(query_eval, p, 1);
      }
      else if (next_c == '\\' || next_c == '$' || next_c == '"')
      {
        /* Set escaped only if next char is \, " or $ */
  escaped= 1;

        if (pass_through_escape_chars)
        {
          /* The escape char should be added to the output string. */
          dynstr_append_mem(query_eval, p, 1);
        }
      }
      else
  dynstr_append_mem(query_eval, p, 1);
      break;
    default:
      escaped= 0;
      dynstr_append_mem(query_eval, p, 1);
      break;
    }
  }
  return;
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

static void show_query(DRIZZLE *drizzle, const char* query)
{
  DRIZZLE_RES* res;


  if (!drizzle)
    return;

  if (drizzle_query(drizzle, query))
  {
    log_msg("Error running query '%s': %d %s",
            query, drizzle_errno(drizzle), drizzle_error(drizzle));
    return;
  }

  if ((res= drizzle_store_result(drizzle)) == NULL)
  {
    /* No result set returned */
    return;
  }

  {
    DRIZZLE_ROW row;
    unsigned int i;
    unsigned int row_num= 0;
    unsigned int num_fields= drizzle_num_fields(res);
    DRIZZLE_FIELD *fields= drizzle_fetch_fields(res);

    fprintf(stderr, "=== %s ===\n", query);
    while ((row= drizzle_fetch_row(res)))
    {
      uint32_t *lengths= drizzle_fetch_lengths(res);
      row_num++;

      fprintf(stderr, "---- %d. ----\n", row_num);
      for(i= 0; i < num_fields; i++)
      {
        fprintf(stderr, "%s\t%.*s\n",
                fields[i].name,
                (int)lengths[i], row[i] ? row[i] : "NULL");
      }
    }
    for (i= 0; i < strlen(query)+8; i++)
      fprintf(stderr, "=");
    fprintf(stderr, "\n\n");
  }
  drizzle_free_result(res);

  return;
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

static void show_warnings_before_error(DRIZZLE *drizzle)
{
  DRIZZLE_RES* res;
  const char* query= "SHOW WARNINGS";


  if (!drizzle)
    return;

  if (drizzle_query(drizzle, query))
  {
    log_msg("Error running query '%s': %d %s",
            query, drizzle_errno(drizzle), drizzle_error(drizzle));
    return;
  }

  if ((res= drizzle_store_result(drizzle)) == NULL)
  {
    /* No result set returned */
    return;
  }

  if (drizzle_num_rows(res) <= 1)
  {
    /* Don't display the last row, it's "last error" */
  }
  else
  {
    DRIZZLE_ROW row;
    unsigned int row_num= 0;
    unsigned int num_fields= drizzle_num_fields(res);

    fprintf(stderr, "\nWarnings from just before the error:\n");
    while ((row= drizzle_fetch_row(res)))
    {
      uint32_t i;
      uint32_t *lengths= drizzle_fetch_lengths(res);

      if (++row_num >= drizzle_num_rows(res))
      {
        /* Don't display the last row, it's "last error" */
        break;
      }

      for(i= 0; i < num_fields; i++)
      {
        fprintf(stderr, "%.*s ", (int)lengths[i],
                row[i] ? row[i] : "NULL");
      }
      fprintf(stderr, "\n");
    }
  }
  drizzle_free_result(res);

  return;
}


enum arg_type
{
  ARG_STRING,
  ARG_REST
};

struct command_arg {
  const char *argname;       /* Name of argument   */
  enum arg_type type;        /* Type of argument   */
  bool required;          /* Argument required  */
  DYNAMIC_STRING *ds;        /* Storage for argument */
  const char *description;   /* Description of the argument */
};


static void check_command_args(struct st_command *command,
                               const char *arguments,
                               const struct command_arg *args,
                               int num_args, const char delimiter_arg)
{
  int i;
  const char *ptr= arguments;
  const char *start;

  for (i= 0; i < num_args; i++)
  {
    const struct command_arg *arg= &args[i];

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
        init_dynamic_string(arg->ds, 0, ptr-start, 32);
        do_eval(arg->ds, start, ptr, false);
      }
      else
      {
        /* Empty string */
        init_dynamic_string(arg->ds, "", 0, 0);
      }
      command->last_argument= (char*)ptr;

      /* Step past the delimiter */
      if (*ptr && *ptr == delimiter_arg)
        ptr++;
      break;

      /* Rest of line */
    case ARG_REST:
      start= ptr;
      init_dynamic_string(arg->ds, 0, command->query_len, 256);
      do_eval(arg->ds, start, command->end, false);
      command->last_argument= command->end;
      break;

    default:
      assert("Unknown argument type");
      break;
    }

    /* Check required arg */
    if (arg->ds->length == 0 && arg->required)
      die("Missing required argument '%s' to command '%.*s'", arg->argname,
          command->first_word_len, command->query);

  }
  /* Check for too many arguments passed */
  ptr= command->last_argument;
  while(ptr <= command->end)
  {
    if (*ptr && *ptr != ' ')
      die("Extra argument '%s' passed to '%.*s'",
          ptr, command->first_word_len, command->query);
    ptr++;
  }
  return;
}


static void handle_command_error(struct st_command *command, uint error)
{

  if (error != 0)
  {
    uint i;

    if (command->abort_on_error)
      die("command \"%.*s\" failed with error %d",
          command->first_word_len, command->query, error);
    for (i= 0; i < command->expected_errors.count; i++)
    {
      if ((command->expected_errors.err[i].type == ERR_ERRNO) &&
          (command->expected_errors.err[i].code.errnum == error))
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
  return;
}


static void close_connections(void)
{

  for (--next_con; next_con >= connections; --next_con)
  {
    drizzle_close(&next_con->drizzle);
    if (next_con->util_drizzle)
      drizzle_close(next_con->util_drizzle);
    my_free(next_con->name, MYF(MY_ALLOW_ZERO_PTR));
  }
  return;
}


static void close_files(void)
{

  for (; cur_file >= file_stack; cur_file--)
  {
    if (cur_file->file && cur_file->file != stdin)
    {
      my_fclose(cur_file->file, MYF(0));
    }
    my_free((uchar*) cur_file->file_name, MYF(MY_ALLOW_ZERO_PTR));
    cur_file->file_name= 0;
  }
  return;
}


static void free_used_memory(void)
{
  uint i;


  close_connections();
  close_files();
  hash_free(&var_hash);

  for (i= 0 ; i < q_lines.elements ; i++)
  {
    struct st_command **q= dynamic_element(&q_lines, i, struct st_command**);
    my_free((*q)->query_buf,MYF(MY_ALLOW_ZERO_PTR));
    my_free((*q),MYF(0));
  }
  for (i= 0; i < 10; i++)
  {
    if (var_reg[i].alloced_len)
      my_free(var_reg[i].str_val, MYF(MY_WME));
  }
  while (embedded_server_arg_count > 1)
    my_free(embedded_server_args[--embedded_server_arg_count],MYF(0));
  delete_dynamic(&q_lines);
  dynstr_free(&ds_res);
  dynstr_free(&ds_progress);
  dynstr_free(&ds_warning_messages);
  free_all_replace();
  my_free(opt_pass,MYF(MY_ALLOW_ZERO_PTR));
  free_defaults(default_argv);

  /* Only call drizzle_server_end if drizzle_server_init has been called */
  if (server_initialized)
    drizzle_server_end();

  return;
}


static void cleanup_and_exit(int exit_code)
{
  free_used_memory();
  my_end(my_end_arg);

  if (!silent) {
    switch (exit_code) {
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
      assert(0);
    }
  }

  exit(exit_code);
}

void die(const char *fmt, ...)
{
  static int dying= 0;
  va_list args;

  /*
    Protect against dying twice
    first time 'die' is called, try to write log files
    second time, just exit
  */
  if (dying)
    cleanup_and_exit(1);
  dying= 1;

  /* Print the error message */
  fprintf(stderr, "drizzletest: ");
  if (cur_file && cur_file != file_stack)
    fprintf(stderr, "In included file \"%s\": ",
            cur_file->file_name);
  if (start_lineno > 0)
    fprintf(stderr, "At line %u: ", start_lineno);
  if (fmt)
  {
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
  }
  else
    fprintf(stderr, "unknown error");
  fprintf(stderr, "\n");
  fflush(stderr);

  /* Show results from queries just before failure */
  if (ds_res.length && opt_tail_lines)
  {
    int tail_lines= opt_tail_lines;
    char* show_from= ds_res.str + ds_res.length - 1;
    while(show_from > ds_res.str && tail_lines > 0 )
    {
      show_from--;
      if (*show_from == '\n')
        tail_lines--;
    }
    fprintf(stderr, "\nThe result from queries just before the failure was:\n");
    if (show_from > ds_res.str)
      fprintf(stderr, "< snip >");
    fprintf(stderr, "%s", show_from);
    fflush(stderr);
  }

  /* Dump the result that has been accumulated so far to .log file */
  if (result_file_name && ds_res.length)
    dump_result_to_log_file(ds_res.str, ds_res.length);

  /* Dump warning messages */
  if (result_file_name && ds_warning_messages.length)
    dump_warning_messages();

  /*
    Help debugging by displaying any warnings that might have
    been produced prior to the error
  */
  if (cur_con)
    show_warnings_before_error(&cur_con->drizzle);

  cleanup_and_exit(1);
}


void abort_not_supported_test(const char *fmt, ...)
{
  va_list args;
  struct st_test_file* err_file= cur_file;


  /* Print include filestack */
  fprintf(stderr, "The test '%s' is not supported by this installation\n",
          file_stack->file_name);
  fprintf(stderr, "Detected in file %s at line %d\n",
          err_file->file_name, err_file->lineno);
  while (err_file != file_stack)
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
  va_list args;

  if (!verbose)
    return;

  va_start(args, fmt);
  fprintf(stderr, "drizzletest: ");
  if (cur_file && cur_file != file_stack)
    fprintf(stderr, "In included file \"%s\": ",
            cur_file->file_name);
  if (start_lineno != 0)
    fprintf(stderr, "At line %u: ", start_lineno);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);

  return;
}


void warning_msg(const char *fmt, ...)
{
  va_list args;
  char buff[512];
  size_t len;


  va_start(args, fmt);
  dynstr_append(&ds_warning_messages, "drizzletest: ");
  if (start_lineno != 0)
  {
    dynstr_append(&ds_warning_messages, "Warning detected ");
    if (cur_file && cur_file != file_stack)
    {
      len= snprintf(buff, sizeof(buff), "in included file %s ",
                       cur_file->file_name);
      dynstr_append_mem(&ds_warning_messages,
                        buff, len);
    }
    len= snprintf(buff, sizeof(buff), "at line %d: ",
                     start_lineno);
    dynstr_append_mem(&ds_warning_messages,
                      buff, len);
  }

  len= vsnprintf(buff, sizeof(buff), fmt, args);
  dynstr_append_mem(&ds_warning_messages, buff, len);

  dynstr_append(&ds_warning_messages, "\n");
  va_end(args);

  return;
}


void log_msg(const char *fmt, ...)
{
  va_list args;
  char buff[1024];
  size_t len;


  va_start(args, fmt);
  len= vsnprintf(buff, sizeof(buff)-1, fmt, args);
  va_end(args);

  dynstr_append_mem(&ds_res, buff, len);
  dynstr_append(&ds_res, "\n");

  return;
}


/*
  Read a file and append it to ds

  SYNOPSIS
  cat_file
  ds - pointer to dynamic string where to add the files content
  filename - name of the file to read

*/

static void cat_file(DYNAMIC_STRING* ds, const char* filename)
{
  int fd;
  uint len;
  char buff[512];

  if ((fd= my_open(filename, O_RDONLY, MYF(0))) < 0)
    die("Failed to open file '%s'", filename);
  while((len= my_read(fd, (uchar*)&buff,
                      sizeof(buff), MYF(0))) > 0)
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
        dynstr_append_mem(ds, start, p-start);
        p++; /* Step past the "fake" newline */
        start= p;
      }
      else
        p++;
    }
    /* Output any chars that migh be left */
    dynstr_append_mem(ds, start, p-start);
  }
  my_close(fd, MYF(0));
}


/*
  Run the specified command with popen

  SYNOPSIS
  run_command
  cmd - command to execute(should be properly quoted
  ds_res- pointer to dynamic string where to store the result

*/

static int run_command(char* cmd,
                       DYNAMIC_STRING *ds_res)
{
  char buf[512]= {0};
  FILE *res_file;
  int error;

  if (!(res_file= popen(cmd, "r")))
    die("popen(\"%s\", \"r\") failed", cmd);

  while (fgets(buf, sizeof(buf), res_file))
  {
    if(ds_res)
    {
      /* Save the output of this command in the supplied string */
      dynstr_append(ds_res, buf);
    }
    else
    {
      /* Print it directly on screen */
      fprintf(stdout, "%s", buf);
    }
  }

  error= pclose(res_file);
  return WEXITSTATUS(error);
}


/*
  Run the specified tool with variable number of arguments

  SYNOPSIS
  run_tool
  tool_path - the name of the tool to run
  ds_res - pointer to dynamic string where to store the result
  ... - variable number of arguments that will be properly
        quoted and appended after the tool's name

*/

static int run_tool(const char *tool_path, DYNAMIC_STRING *ds_res, ...)
{
  int ret;
  const char* arg;
  va_list args;
  DYNAMIC_STRING ds_cmdline;

  if (init_dynamic_string(&ds_cmdline, "", FN_REFLEN, FN_REFLEN))
    die("Out of memory");

  dynstr_append_os_quoted(&ds_cmdline, tool_path, NullS);
  dynstr_append(&ds_cmdline, " ");

  va_start(args, ds_res);

  while ((arg= va_arg(args, char *)))
  {
    /* Options should be os quoted */
    if (strncmp(arg, "--", 2) == 0)
      dynstr_append_os_quoted(&ds_cmdline, arg, NullS);
    else
      dynstr_append(&ds_cmdline, arg);
    dynstr_append(&ds_cmdline, " ");
  }

  va_end(args);

  ret= run_command(ds_cmdline.str, ds_res);
  dynstr_free(&ds_cmdline);
  return(ret);
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

static void show_diff(DYNAMIC_STRING* ds,
                      const char* filename1, const char* filename2)
{

  DYNAMIC_STRING ds_tmp;

  if (init_dynamic_string(&ds_tmp, "", 256, 256))
    die("Out of memory");

  /* First try with unified diff */
  if (run_tool("diff",
               &ds_tmp, /* Get output from diff in ds_tmp */
               "-u",
               filename1,
               filename2,
               "2>&1",
               NULL) > 1) /* Most "diff" tools return >1 if error */
  {
    dynstr_set(&ds_tmp, "");

    /* Fallback to context diff with "diff -c" */
    if (run_tool("diff",
                 &ds_tmp, /* Get output from diff in ds_tmp */
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
      dynstr_set(&ds_tmp, "");

      dynstr_append(&ds_tmp,
"\n"
"The two files differ but it was not possible to execute 'diff' in\n"
"order to show only the difference, tried both 'diff -u' or 'diff -c'.\n"
"Instead the whole content of the two files was shown for you to diff manually. ;)\n\n"
"To get a better report you should install 'diff' on your system, which you\n"
"for example can get from http://www.gnu.org/software/diffutils/diffutils.html\n"
"\n");

      dynstr_append(&ds_tmp, " --- ");
      dynstr_append(&ds_tmp, filename1);
      dynstr_append(&ds_tmp, " >>>\n");
      cat_file(&ds_tmp, filename1);
      dynstr_append(&ds_tmp, "<<<\n --- ");
      dynstr_append(&ds_tmp, filename1);
      dynstr_append(&ds_tmp, " >>>\n");
      cat_file(&ds_tmp, filename2);
      dynstr_append(&ds_tmp, "<<<<\n");
    }
  }

  if (ds)
  {
    /* Add the diff to output */
    dynstr_append_mem(ds, ds_tmp.str, ds_tmp.length);
  }
  else
  {
    /* Print diff directly to stdout */
    fprintf(stderr, "%s\n", ds_tmp.str);
  }

  dynstr_free(&ds_tmp);

}


enum compare_files_result_enum {
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

static int compare_files2(File fd, const char* filename2)
{
  int error= RESULT_OK;
  File fd2;
  uint len, len2;
  char buff[512], buff2[512];

  if ((fd2= my_open(filename2, O_RDONLY, MYF(0))) < 0)
  {
    my_close(fd, MYF(0));
    die("Failed to open second file: '%s'", filename2);
  }
  while((len= my_read(fd, (uchar*)&buff,
                      sizeof(buff), MYF(0))) > 0)
  {
    if ((len2= my_read(fd2, (uchar*)&buff2,
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
  if (!error && my_read(fd2, (uchar*)&buff2,
                        sizeof(buff2), MYF(0)) > 0)
  {
    /* File 1 was smaller */
    error= RESULT_LENGTH_MISMATCH;
  }

  my_close(fd2, MYF(0));

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
  File fd;
  int error;

  if ((fd= my_open(filename1, O_RDONLY, MYF(0))) < 0)
    die("Failed to open first file: '%s'", filename1);

  error= compare_files2(fd, filename2);

  my_close(fd, MYF(0));

  return error;
}


/*
  Compare content of the string in ds to content of file fname

  SYNOPSIS
  dyn_string_cmp
  ds - Dynamic string containing the string o be compared
  fname - Name of file to compare with

  RETURN VALUES
  See 'compare_files2'
*/

static int dyn_string_cmp(DYNAMIC_STRING* ds, const char *fname)
{
  int error;
  File fd;
  char temp_file_path[FN_REFLEN];

  if ((fd= create_temp_file(temp_file_path, NULL,
                            "tmp", O_CREAT | O_SHARE | O_RDWR,
                            MYF(MY_WME))) < 0)
    die("Failed to create temporary file for ds");

  /* Write ds to temporary file and set file pos to beginning*/
  if (my_write(fd, (uchar *) ds->str, ds->length,
               MYF(MY_FNABP | MY_WME)) ||
      my_seek(fd, 0, SEEK_SET, MYF(0)) == MY_FILEPOS_ERROR)
  {
    my_close(fd, MYF(0));
    /* Remove the temporary file */
    my_delete(temp_file_path, MYF(0));
    die("Failed to write file '%s'", temp_file_path);
  }

  error= compare_files2(fd, fname);

  my_close(fd, MYF(0));
  /* Remove the temporary file */
  my_delete(temp_file_path, MYF(0));

  return(error);
}


/*
  Check the content of ds against result file

  SYNOPSIS
  check_result
  ds - content to be checked

  RETURN VALUES
  error - the function will not return

*/

static void check_result(DYNAMIC_STRING* ds)
{
  const char* mess= "Result content mismatch\n";


  assert(result_file_name);

  if (access(result_file_name, F_OK) != 0)
    die("The specified result file does not exist: '%s'", result_file_name);

  switch (dyn_string_cmp(ds, result_file_name)) {
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
    dirname_part(reject_file, result_file_name, &reject_length);

    if (access(reject_file, W_OK) == 0)
    {
      /* Result file directory is writable, save reject file there */
      fn_format(reject_file, result_file_name, NULL,
                ".reject", MY_REPLACE_EXT);
    }
    else
    {
      /* Put reject file in opt_logdir */
      fn_format(reject_file, result_file_name, opt_logdir,
                ".reject", MY_REPLACE_DIR | MY_REPLACE_EXT);
    }
    str_to_file(reject_file, ds->str, ds->length);

    dynstr_set(ds, NULL); /* Don't create a .log file */

    show_diff(NULL, result_file_name, reject_file);
    die(mess);
    break;
  }
  default: /* impossible */
    die("Unknown error code from dyn_string_cmp()");
  }

  return;
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

static void check_require(DYNAMIC_STRING* ds, const char *fname)
{


  if (dyn_string_cmp(ds, fname))
  {
    char reason[FN_REFLEN];
    fn_format(reason, fname, "", "", MY_REPLACE_EXT | MY_REPLACE_DIR);
    abort_not_supported_test("Test requires: '%s'", reason);
  }
  return;
}


/*
   Remove surrounding chars from string

   Return 1 if first character is found but not last
*/
static int strip_surrounding(char* str, char c1, char c2)
{
  char* ptr= str;

  /* Check if the first non space character is c1 */
  while(*ptr && my_isspace(charset_info, *ptr))
    ptr++;
  if (*ptr == c1)
  {
    /* Replace it with a space */
    *ptr= ' ';

    /* Last non space charecter should be c2 */
    ptr= strend(str)-1;
    while(*ptr && my_isspace(charset_info, *ptr))
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


static void strip_parentheses(struct st_command *command)
{
  if (strip_surrounding(command->first_argument, '(', ')'))
      die("%.*s - argument list started with '%c' must be ended with '%c'",
          command->first_word_len, command->query, '(', ')');
}


static uchar *get_var_key(const uchar* var, size_t *len,
                          bool __attribute__((unused)) t)
{
  register char* key;
  key = ((VAR*)var)->name;
  *len = ((VAR*)var)->name_len;
  return (uchar*)key;
}


VAR *var_init(VAR *v, const char *name, int name_len, const char *val,
              int val_len)
{
  int val_alloc_len;
  VAR *tmp_var;
  if (!name_len && name)
    name_len = strlen(name);
  if (!val_len && val)
    val_len = strlen(val) ;
  val_alloc_len = val_len + 16; /* room to grow */
  if (!(tmp_var=v) && !(tmp_var = (VAR*)my_malloc(sizeof(*tmp_var)
                                                  + name_len+1, MYF(MY_WME))))
    die("Out of memory");

  tmp_var->name = (name) ? (char*) tmp_var + sizeof(*tmp_var) : 0;
  tmp_var->alloced = (v == 0);

  if (!(tmp_var->str_val = (char *)my_malloc(val_alloc_len+1, MYF(MY_WME))))
    die("Out of memory");

  memcpy(tmp_var->name, name, name_len);
  if (val)
  {
    memcpy(tmp_var->str_val, val, val_len);
    tmp_var->str_val[val_len]= 0;
  }
  tmp_var->name_len = name_len;
  tmp_var->str_val_len = val_len;
  tmp_var->alloced_len = val_alloc_len;
  tmp_var->int_val = (val) ? atoi(val) : 0;
  tmp_var->int_dirty = 0;
  tmp_var->env_s = 0;
  return tmp_var;
}


void var_free(void *v)
{
  my_free(((VAR*) v)->str_val, MYF(MY_WME));
  my_free(((VAR*) v)->env_s, MYF(MY_WME|MY_ALLOW_ZERO_PTR));
  if (((VAR*)v)->alloced)
    my_free(v, MYF(MY_WME));
}


VAR* var_from_env(const char *name, const char *def_val)
{
  const char *tmp;
  VAR *v;
  if (!(tmp = getenv(name)))
    tmp = def_val;

  v = var_init(0, name, strlen(name), tmp, strlen(tmp));
  my_hash_insert(&var_hash, (uchar*)v);
  return v;
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
    uint length;
    end = (var_name_end) ? *var_name_end : 0;
    while (my_isvar(charset_info,*var_name) && var_name != end)
      var_name++;
    if (var_name == save_var_name)
    {
      if (ignore_not_existing)
  return(0);
      die("Empty variable");
    }
    length= (uint) (var_name - save_var_name);
    if (length >= MAX_VAR_NAME_LENGTH)
      die("Too long variable name: %s", save_var_name);

    if (!(v = (VAR*) hash_search(&var_hash, (const uchar*) save_var_name,
                                            length)))
    {
      char buff[MAX_VAR_NAME_LENGTH+1];
      strmake(buff, save_var_name, length);
      v= var_from_env(buff, "");
    }
    var_name--;  /* Point at last character */
  }
  else
    v = var_reg + digit;

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
  VAR* v;
  if ((v = (VAR*)hash_search(&var_hash, (const uchar *) name, len)))
    return v;
  v = var_init(0, name, len, "", 0);
  my_hash_insert(&var_hash, (uchar*)v);
  return v;
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
    v= var_obtain(var_name, (uint) (var_name_end - var_name));
  }
  else
    v= var_reg + digit;

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
    if (!(v->env_s= my_strdup(buf, MYF(MY_WME))))
      die("Out of memory");
    putenv(v->env_s);
    my_free(old_env_s, MYF(MY_ALLOW_ZERO_PTR));
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
  statement in the drizzletest builtin variable $drizzle_errno
*/

static void var_set_errno(int sql_errno)
{
  var_set_int("$drizzle_errno", sql_errno);
}


/*
  Update $drizzle_get_server_version variable with version
  of the currently connected server
*/

static void var_set_drizzle_get_server_version(DRIZZLE *drizzle)
{
  var_set_int("$drizzle_get_server_version", drizzle_get_server_version(drizzle));
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

static void var_query_set(VAR *var, const char *query, const char** query_end)
{
  char *end = (char*)((query_end && *query_end) ?
          *query_end : query + strlen(query));
  DRIZZLE_RES *res;
  DRIZZLE_ROW row;
  DRIZZLE *drizzle= &cur_con->drizzle;
  DYNAMIC_STRING ds_query;


  while (end > query && *end != '`')
    --end;
  if (query == end)
    die("Syntax error in query, missing '`'");
  ++query;

  /* Eval the query, thus replacing all environment variables */
  init_dynamic_string(&ds_query, 0, (end - query) + 32, 256);
  do_eval(&ds_query, query, end, false);

  if (drizzle_real_query(drizzle, ds_query.str, ds_query.length))
    die("Error running query '%s': %d %s", ds_query.str,
  drizzle_errno(drizzle), drizzle_error(drizzle));
  if (!(res= drizzle_store_result(drizzle)))
    die("Query '%s' didn't return a result set", ds_query.str);
  dynstr_free(&ds_query);

  if ((row= drizzle_fetch_row(res)) && row[0])
  {
    /*
      Concatenate all fields in the first row with tab in between
      and assign that string to the $variable
    */
    DYNAMIC_STRING result;
    uint32_t i;
    uint32_t *lengths;

    init_dynamic_string(&result, "", 512, 512);
    lengths= drizzle_fetch_lengths(res);
    for (i= 0; i < drizzle_num_fields(res); i++)
    {
      if (row[i])
      {
        /* Add column to tab separated string */
  dynstr_append_mem(&result, row[i], lengths[i]);
      }
      dynstr_append_mem(&result, "\t", 1);
    }
    end= result.str + result.length-1;
    eval_expr(var, result.str, (const char**) &end);
    dynstr_free(&result);
  }
  else
    eval_expr(var, "", 0);

  drizzle_free_result(res);
  return;
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

static void var_set_query_get_value(struct st_command *command, VAR *var)
{
  long row_no;
  int col_no= -1;
  DRIZZLE_RES* res;
  DRIZZLE *drizzle= &cur_con->drizzle;

  static DYNAMIC_STRING ds_query;
  static DYNAMIC_STRING ds_col;
  static DYNAMIC_STRING ds_row;
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
  if (!str2int(ds_row.str, 10, (long) 0, (long) INT_MAX, &row_no))
    die("Invalid row number: '%s'", ds_row.str);
  dynstr_free(&ds_row);

  /* Remove any surrounding "'s from the query - if there is any */
  if (strip_surrounding(ds_query.str, '"', '"'))
    die("Mismatched \"'s around query '%s'", ds_query.str);

  /* Run the query */
  if (drizzle_real_query(drizzle, ds_query.str, ds_query.length))
    die("Error running query '%s': %d %s", ds_query.str,
  drizzle_errno(drizzle), drizzle_error(drizzle));
  if (!(res= drizzle_store_result(drizzle)))
    die("Query '%s' didn't return a result set", ds_query.str);

  {
    /* Find column number from the given column name */
    uint i;
    uint num_fields= drizzle_num_fields(res);
    DRIZZLE_FIELD *fields= drizzle_fetch_fields(res);

    for (i= 0; i < num_fields; i++)
    {
      if (strcmp(fields[i].name, ds_col.str) == 0 &&
          strlen(fields[i].name) == ds_col.length)
      {
        col_no= i;
        break;
      }
    }
    if (col_no == -1)
    {
      drizzle_free_result(res);
      die("Could not find column '%s' in the result of '%s'",
          ds_col.str, ds_query.str);
    }
  }
  dynstr_free(&ds_col);

  {
    /* Get the value */
    DRIZZLE_ROW row;
    long rows= 0;
    const char* value= "No such row";

    while ((row= drizzle_fetch_row(res)))
    {
      if (++rows == row_no)
      {

        /* Found the row to get */
        if (row[col_no])
          value= row[col_no];
        else
          value= "NULL";

        break;
      }
    }
    eval_expr(var, value, 0);
  }
  dynstr_free(&ds_query);
  drizzle_free_result(res);

  return;
}


static void var_copy(VAR *dest, VAR *src)
{
  dest->int_val= src->int_val;
  dest->int_dirty= src->int_dirty;

  /* Alloc/realloc data for str_val in dest */
  if (dest->alloced_len < src->alloced_len &&
      !(dest->str_val= dest->str_val
        ? (char *)my_realloc(dest->str_val, src->alloced_len, MYF(MY_WME))
        : (char *)my_malloc(src->alloced_len, MYF(MY_WME))))
    die("Out of memory");
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
    VAR *vp;
    if ((vp= var_get(p, p_end, 0, 0)))
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
      struct st_command command;
      memset(&command, 0, sizeof(command));
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
      if (!(v->str_val =
            v->str_val ? (char *)my_realloc(v->str_val, v->alloced_len+1,
                                    MYF(MY_WME)) :
            (char *)my_malloc(v->alloced_len+1, MYF(MY_WME))))
        die("Out of memory");
    }
    v->str_val_len = new_val_len;
    memcpy(v->str_val, p, new_val_len);
    v->str_val[new_val_len] = 0;
    v->int_val=atoi(p);
    v->int_dirty=0;
  }
  return;
}


static int open_file(const char *name)
{
  char buff[FN_REFLEN];

  if (!test_if_hard_path(name))
  {
    strxmov(buff, opt_basedir, name, NullS);
    name=buff;
  }
  fn_format(buff, name, "", "", MY_UNPACK_FILENAME);

  if (cur_file == file_stack_end)
    die("Source directives are nesting too deep");
  cur_file++;
  if (!(cur_file->file = my_fopen(buff, O_RDONLY | FILE_BINARY, MYF(0))))
  {
    cur_file--;
    die("Could not open '%s' for reading", buff);
  }
  cur_file->file_name= my_strdup(buff, MYF(MY_FAE));
  cur_file->lineno=1;
  return(0);
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

static void do_source(struct st_command *command)
{
  static DYNAMIC_STRING ds_filename;
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
    open_file(ds_filename.str);
  }

  dynstr_free(&ds_filename);
  return;
}


static FILE* my_popen(DYNAMIC_STRING *ds_cmd, const char *mode)
{
  return popen(ds_cmd->str, mode);
}


static void init_builtin_echo(void)
{
  builtin_echo[0]= 0;
  return;
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

static int replace(DYNAMIC_STRING *ds_str,
                   const char *search_str, ulong search_len,
                   const char *replace_str, ulong replace_len)
{
  DYNAMIC_STRING ds_tmp;
  const char *start= strstr(ds_str->str, search_str);
  if (!start)
    return 1;
  init_dynamic_string(&ds_tmp, "",
                      ds_str->length + replace_len, 256);
  dynstr_append_mem(&ds_tmp, ds_str->str, start - ds_str->str);
  dynstr_append_mem(&ds_tmp, replace_str, replace_len);
  dynstr_append(&ds_tmp, start + search_len);
  dynstr_set(ds_str, ds_tmp.str);
  dynstr_free(&ds_tmp);
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

static void do_exec(struct st_command *command)
{
  int error;
  char buf[512];
  FILE *res_file;
  char *cmd= command->first_argument;
  DYNAMIC_STRING ds_cmd;

  /* Skip leading space */
  while (*cmd && my_isspace(charset_info, *cmd))
    cmd++;
  if (!*cmd)
    die("Missing argument in exec");
  command->last_argument= command->end;

  init_dynamic_string(&ds_cmd, 0, command->query_len+256, 256);
  /* Eval the command, thus replacing all environment variables */
  do_eval(&ds_cmd, cmd, command->end, !is_windows);

  /* Check if echo should be replaced with "builtin" echo */
  if (builtin_echo[0] && strncmp(cmd, "echo", 4) == 0)
  {
    /* Replace echo with our "builtin" echo */
    replace(&ds_cmd, "echo", 4, builtin_echo, strlen(builtin_echo));
  }

  if (!(res_file= my_popen(&ds_cmd, "r")) && command->abort_on_error)
  {
    dynstr_free(&ds_cmd);
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
      replace_dynstr_append(&ds_res, buf);
    }
  }
  error= pclose(res_file);
  if (error > 0)
  {
    uint status= WEXITSTATUS(error), i;
    bool ok= 0;

    if (command->abort_on_error)
    {
      log_msg("exec of '%s' failed, error: %d, status: %d, errno: %d",
              ds_cmd.str, error, status, errno);
      dynstr_free(&ds_cmd);
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
      dynstr_free(&ds_cmd);
      die("command \"%s\" failed with wrong error: %d",
          command->first_argument, status);
    }
  }
  else if (command->expected_errors.err[0].type == ERR_ERRNO &&
           command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    log_msg("exec of '%s failed, error: %d, errno: %d",
            ds_cmd.str, error, errno);
    dynstr_free(&ds_cmd);
    die("command \"%s\" succeeded - should have failed with errno %d...",
        command->first_argument, command->expected_errors.err[0].code.errnum);
  }

  dynstr_free(&ds_cmd);
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

static int do_modify_var(struct st_command *command,
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
  Wrapper for 'system' function

  NOTE
  If drizzletest is executed from cygwin shell, the command will be
  executed in the "windows command interpreter" cmd.exe and we prepend "sh"
  to make it be executed by cygwins "bash". Thus commands like "rm",
  "mkdir" as well as shellscripts can executed by "system" in Windows.

*/

static int my_system(DYNAMIC_STRING* ds_cmd)
{
  return system(ds_cmd->str);
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

static void do_system(struct st_command *command)
{
  DYNAMIC_STRING ds_cmd;


  if (strlen(command->first_argument) == 0)
    die("Missing arguments to system, nothing to do!");

  init_dynamic_string(&ds_cmd, 0, command->query_len + 64, 256);

  /* Eval the system command, thus replacing all environment variables */
  do_eval(&ds_cmd, command->first_argument, command->end, !is_windows);

  if (my_system(&ds_cmd))
  {
    if (command->abort_on_error)
      die("system command '%s' failed", command->first_argument);

    /* If ! abort_on_error, log message and continue */
    dynstr_append(&ds_res, "system command '");
    replace_dynstr_append(&ds_res, command->first_argument);
    dynstr_append(&ds_res, "' failed\n");
  }

  command->last_argument= command->end;
  dynstr_free(&ds_cmd);
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

static void do_remove_file(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_filename;
  const struct command_arg rm_args[] = {
    { "filename", ARG_STRING, true, &ds_filename, "File to delete" }
  };


  check_command_args(command, command->first_argument,
                     rm_args, sizeof(rm_args)/sizeof(struct command_arg),
                     ' ');

  error= my_delete(ds_filename.str, MYF(0)) != 0;
  handle_command_error(command, error);
  dynstr_free(&ds_filename);
  return;
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

static void do_copy_file(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_from_file;
  static DYNAMIC_STRING ds_to_file;
  const struct command_arg copy_file_args[] = {
    { "from_file", ARG_STRING, true, &ds_from_file, "Filename to copy from" },
    { "to_file", ARG_STRING, true, &ds_to_file, "Filename to copy to" }
  };


  check_command_args(command, command->first_argument,
                     copy_file_args,
                     sizeof(copy_file_args)/sizeof(struct command_arg),
                     ' ');

  error= (my_copy(ds_from_file.str, ds_to_file.str,
                  MYF(MY_DONT_OVERWRITE_FILE)) != 0);
  handle_command_error(command, error);
  dynstr_free(&ds_from_file);
  dynstr_free(&ds_to_file);
  return;
}


/*
  SYNOPSIS
  do_chmod_file
  command  command handle

  DESCRIPTION
  chmod <octal> <file_name>
  Change file permission of <file_name>

*/

static void do_chmod_file(struct st_command *command)
{
  long mode= 0;
  static DYNAMIC_STRING ds_mode;
  static DYNAMIC_STRING ds_file;
  const struct command_arg chmod_file_args[] = {
    { "mode", ARG_STRING, true, &ds_mode, "Mode of file(octal) ex. 0660"},
    { "filename", ARG_STRING, true, &ds_file, "Filename of file to modify" }
  };


  check_command_args(command, command->first_argument,
                     chmod_file_args,
                     sizeof(chmod_file_args)/sizeof(struct command_arg),
                     ' ');

  /* Parse what mode to set */
  if (ds_mode.length != 4 ||
      str2int(ds_mode.str, 8, 0, INT_MAX, &mode) == NullS)
    die("You must write a 4 digit octal number for mode");

  handle_command_error(command, chmod(ds_file.str, mode));
  dynstr_free(&ds_mode);
  dynstr_free(&ds_file);
  return;
}


/*
  SYNOPSIS
  do_file_exists
  command  called command

  DESCRIPTION
  fiile_exist <file_name>
  Check if file <file_name> exists
*/

static void do_file_exist(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_filename;
  const struct command_arg file_exist_args[] = {
    { "filename", ARG_STRING, true, &ds_filename, "File to check if it exist" }
  };


  check_command_args(command, command->first_argument,
                     file_exist_args,
                     sizeof(file_exist_args)/sizeof(struct command_arg),
                     ' ');

  error= (access(ds_filename.str, F_OK) != 0);
  handle_command_error(command, error);
  dynstr_free(&ds_filename);
  return;
}


/*
  SYNOPSIS
  do_mkdir
  command  called command

  DESCRIPTION
  mkdir <dir_name>
  Create the directory <dir_name>
*/

static void do_mkdir(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_dirname;
  const struct command_arg mkdir_args[] = {
    {"dirname", ARG_STRING, true, &ds_dirname, "Directory to create"}
  };


  check_command_args(command, command->first_argument,
                     mkdir_args, sizeof(mkdir_args)/sizeof(struct command_arg),
                     ' ');

  error= my_mkdir(ds_dirname.str, 0777, MYF(0)) != 0;
  handle_command_error(command, error);
  dynstr_free(&ds_dirname);
  return;
}

/*
  SYNOPSIS
  do_rmdir
  command  called command

  DESCRIPTION
  rmdir <dir_name>
  Remove the empty directory <dir_name>
*/

static void do_rmdir(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_dirname;
  const struct command_arg rmdir_args[] = {
    {"dirname", ARG_STRING, true, &ds_dirname, "Directory to remove"}
  };


  check_command_args(command, command->first_argument,
                     rmdir_args, sizeof(rmdir_args)/sizeof(struct command_arg),
                     ' ');

  error= rmdir(ds_dirname.str) != 0;
  handle_command_error(command, error);
  dynstr_free(&ds_dirname);
  return;
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


static void read_until_delimiter(DYNAMIC_STRING *ds,
                                 DYNAMIC_STRING *ds_delimiter)
{
  char c;

  if (ds_delimiter->length > MAX_DELIMITER_LENGTH)
    die("Max delimiter length(%d) exceeded", MAX_DELIMITER_LENGTH);

  /* Read from file until delimiter is found */
  while (1)
  {
    c= my_getc(cur_file->file);

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
          ds_delimiter->str);

    if (match_delimiter(c, ds_delimiter->str, ds_delimiter->length))
      break;

    dynstr_append_mem(ds, (const char*)&c, 1);
  }
  return;
}


static void do_write_file_command(struct st_command *command, bool append)
{
  static DYNAMIC_STRING ds_content;
  static DYNAMIC_STRING ds_filename;
  static DYNAMIC_STRING ds_delimiter;
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
  if (ds_delimiter.length == 0)
    dynstr_set(&ds_delimiter, "EOF");

  if (!append && access(ds_filename.str, F_OK) == 0)
  {
    /* The file should not be overwritten */
    die("File already exist: '%s'", ds_filename.str);
  }

  init_dynamic_string(&ds_content, "", 1024, 1024);
  read_until_delimiter(&ds_content, &ds_delimiter);
  str_to_file2(ds_filename.str, ds_content.str, ds_content.length, append);
  dynstr_free(&ds_content);
  dynstr_free(&ds_filename);
  dynstr_free(&ds_delimiter);
  return;
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

static void do_write_file(struct st_command *command)
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

static void do_append_file(struct st_command *command)
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

static void do_cat_file(struct st_command *command)
{
  static DYNAMIC_STRING ds_filename;
  const struct command_arg cat_file_args[] = {
    { "filename", ARG_STRING, true, &ds_filename, "File to read from" }
  };


  check_command_args(command,
                     command->first_argument,
                     cat_file_args,
                     sizeof(cat_file_args)/sizeof(struct command_arg),
                     ' ');

  cat_file(&ds_res, ds_filename.str);

  dynstr_free(&ds_filename);
  return;
}


/*
  SYNOPSIS
  do_diff_files
  command  called command

  DESCRIPTION
  diff_files <file1> <file2>;

  Fails if the two files differ.

*/

static void do_diff_files(struct st_command *command)
{
  int error= 0;
  static DYNAMIC_STRING ds_filename;
  static DYNAMIC_STRING ds_filename2;
  const struct command_arg diff_file_args[] = {
    { "file1", ARG_STRING, true, &ds_filename, "First file to diff" },
    { "file2", ARG_STRING, true, &ds_filename2, "Second file to diff" }
  };


  check_command_args(command,
                     command->first_argument,
                     diff_file_args,
                     sizeof(diff_file_args)/sizeof(struct command_arg),
                     ' ');

  if ((error= compare_files(ds_filename.str, ds_filename2.str)))
  {
    /* Compare of the two files failed, append them to output
       so the failure can be analyzed
    */
    show_diff(&ds_res, ds_filename.str, ds_filename2.str);
  }

  dynstr_free(&ds_filename);
  dynstr_free(&ds_filename2);
  handle_command_error(command, error);
  return;
}


static struct st_connection * find_connection_by_name(const char *name)
{
  struct st_connection *con;
  for (con= connections; con < next_con; con++)
  {
    if (!strcmp(con->name, name))
    {
      return con;
    }
  }
  return 0; /* Connection not found */
}


/*
  SYNOPSIS
  do_send_quit
  command  called command

  DESCRIPTION
  Sends a simple quit command to the server for the named connection.

*/

static void do_send_quit(struct st_command *command)
{
  char *p= command->first_argument, *name;
  struct st_connection *con;

  if (!*p)
    die("Missing connection name in send_quit");
  name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;

  if (*p)
    *p++= 0;
  command->last_argument= p;

  if (!(con= find_connection_by_name(name)))
    die("connection '%s' not found in connection pool", name);

  simple_command(&con->drizzle,COM_QUIT,0,0,1);

  return;
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

static void do_change_user(struct st_command *command)
{
  DRIZZLE *drizzle= &cur_con->drizzle;
  /* static keyword to make the NetWare compiler happy. */
  static DYNAMIC_STRING ds_user, ds_passwd, ds_db;
  const struct command_arg change_user_args[] = {
    { "user", ARG_STRING, false, &ds_user, "User to connect as" },
    { "password", ARG_STRING, false, &ds_passwd, "Password used when connecting" },
    { "database", ARG_STRING, false, &ds_db, "Database to select after connect" },
  };



  check_command_args(command, command->first_argument,
                     change_user_args,
                     sizeof(change_user_args)/sizeof(struct command_arg),
                     ',');

  if (!ds_user.length)
    dynstr_set(&ds_user, drizzle->user);

  if (!ds_passwd.length)
    dynstr_set(&ds_passwd, drizzle->passwd);

  if (!ds_db.length)
    dynstr_set(&ds_db, drizzle->db);

  if (drizzle_change_user(drizzle, ds_user.str, ds_passwd.str, ds_db.str))
    die("change user failed: %s", drizzle_error(drizzle));

  dynstr_free(&ds_user);
  dynstr_free(&ds_passwd);
  dynstr_free(&ds_db);

  return;
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

static void do_perl(struct st_command *command)
{
  int error;
  File fd;
  FILE *res_file;
  char buf[FN_REFLEN];
  char temp_file_path[FN_REFLEN];
  static DYNAMIC_STRING ds_script;
  static DYNAMIC_STRING ds_delimiter;
  const struct command_arg perl_args[] = {
    { "delimiter", ARG_STRING, false, &ds_delimiter, "Delimiter to read until" }
  };


  check_command_args(command,
                     command->first_argument,
                     perl_args,
                     sizeof(perl_args)/sizeof(struct command_arg),
                     ' ');

  /* If no delimiter was provided, use EOF */
  if (ds_delimiter.length == 0)
    dynstr_set(&ds_delimiter, "EOF");

  init_dynamic_string(&ds_script, "", 1024, 1024);
  read_until_delimiter(&ds_script, &ds_delimiter);

  /* Create temporary file name */
  if ((fd= create_temp_file(temp_file_path, getenv("MYSQLTEST_VARDIR"),
                            "tmp", O_CREAT | O_SHARE | O_RDWR,
                            MYF(MY_WME))) < 0)
    die("Failed to create temporary file for perl command");
  my_close(fd, MYF(0));

  str_to_file(temp_file_path, ds_script.str, ds_script.length);

  /* Format the "perl <filename>" command */
  snprintf(buf, sizeof(buf), "perl %s", temp_file_path);

  if (!(res_file= popen(buf, "r")) && command->abort_on_error)
    die("popen(\"%s\", \"r\") failed", buf);

  while (fgets(buf, sizeof(buf), res_file))
  {
    if (disable_result_log)
      buf[strlen(buf)-1]=0;
    else
      replace_dynstr_append(&ds_res, buf);
  }
  error= pclose(res_file);

  /* Remove the temporary file */
  my_delete(temp_file_path, MYF(0));

  handle_command_error(command, WEXITSTATUS(error));
  dynstr_free(&ds_script);
  dynstr_free(&ds_delimiter);
  return;
}


/*
  Print the content between echo and <delimiter> to result file.
  Evaluate all variables in the string before printing, allow
  for variable names to be escaped using \

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

static int do_echo(struct st_command *command)
{
  DYNAMIC_STRING ds_echo;


  init_dynamic_string(&ds_echo, "", command->query_len, 256);
  do_eval(&ds_echo, command->first_argument, command->end, false);
  dynstr_append_mem(&ds_res, ds_echo.str, ds_echo.length);
  dynstr_append_mem(&ds_res, "\n", 1);
  dynstr_free(&ds_echo);
  command->last_argument= command->end;
  return(0);
}


static void
do_wait_for_slave_to_stop(struct st_command *c __attribute__((unused)))
{
  static int SLAVE_POLL_INTERVAL= 300000;
  DRIZZLE *drizzle= &cur_con->drizzle;
  for (;;)
  {
    DRIZZLE_RES *res= NULL;
    DRIZZLE_ROW row;
    int done;

    if (drizzle_query(drizzle,"show status like 'Slave_running'") ||
  !(res=drizzle_store_result(drizzle)))
      die("Query failed while probing slave for stop: %s",
    drizzle_error(drizzle));
    if (!(row=drizzle_fetch_row(res)) || !row[1])
    {
      drizzle_free_result(res);
      die("Strange result from query while probing slave for stop");
    }
    done = !strcmp(row[1],"OFF");
    drizzle_free_result(res);
    if (done)
      break;
    my_sleep(SLAVE_POLL_INTERVAL);
  }
  return;
}


static void do_sync_with_master2(long offset)
{
  DRIZZLE_RES *res;
  DRIZZLE_ROW row;
  DRIZZLE *drizzle= &cur_con->drizzle;
  char query_buf[FN_REFLEN+128];
  int tries= 0;

  if (!master_pos.file[0])
    die("Calling 'sync_with_master' without calling 'save_master_pos'");

  sprintf(query_buf, "select master_pos_wait('%s', %ld)", master_pos.file,
    master_pos.pos + offset);

wait_for_position:

  if (drizzle_query(drizzle, query_buf))
    die("failed in '%s': %d: %s", query_buf, drizzle_errno(drizzle),
        drizzle_error(drizzle));

  if (!(res= drizzle_store_result(drizzle)))
    die("drizzle_store_result() returned NULL for '%s'", query_buf);
  if (!(row= drizzle_fetch_row(res)))
  {
    drizzle_free_result(res);
    die("empty result in %s", query_buf);
  }
  if (!row[0])
  {
    /*
      It may be that the slave SQL thread has not started yet, though START
      SLAVE has been issued ?
    */
    drizzle_free_result(res);
    if (tries++ == 30)
    {
      show_query(drizzle, "SHOW MASTER STATUS");
      show_query(drizzle, "SHOW SLAVE STATUS");
      die("could not sync with master ('%s' returned NULL)", query_buf);
    }
    sleep(1); /* So at most we will wait 30 seconds and make 31 tries */
    goto wait_for_position;
  }
  drizzle_free_result(res);
  return;
}


static void do_sync_with_master(struct st_command *command)
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
static int do_save_master_pos(void)
{
  DRIZZLE_RES *res;
  DRIZZLE_ROW row;
  DRIZZLE *drizzle= &cur_con->drizzle;
  const char *query;


  if (drizzle_query(drizzle, query= "show master status"))
    die("failed in 'show master status': %d %s",
  drizzle_errno(drizzle), drizzle_error(drizzle));

  if (!(res = drizzle_store_result(drizzle)))
    die("drizzle_store_result() retuned NULL for '%s'", query);
  if (!(row = drizzle_fetch_row(res)))
    die("empty result in show master status");
  strnmov(master_pos.file, row[0], sizeof(master_pos.file)-1);
  master_pos.pos = strtoul(row[1], (char**) 0, 10);
  drizzle_free_result(res);
  return(0);
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

static void do_let(struct st_command *command)
{
  char *p= command->first_argument;
  char *var_name, *var_name_end;
  DYNAMIC_STRING let_rhs_expr;


  init_dynamic_string(&let_rhs_expr, "", 512, 2048);

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
  var_set(var_name, var_name_end, let_rhs_expr.str,
          (let_rhs_expr.str + let_rhs_expr.length));
  dynstr_free(&let_rhs_expr);
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

static int do_sleep(struct st_command *command, bool real_sleep)
{
  int error= 0;
  char *p= command->first_argument;
  char *sleep_start, *sleep_end= command->end;
  double sleep_val;

  while (my_isspace(charset_info, *p))
    p++;
  if (!*p)
    die("Missing argument to %.*s", command->first_word_len, command->query);
  sleep_start= p;
  /* Check that arg starts with a digit, not handled by my_strtod */
  if (!my_isdigit(charset_info, *sleep_start))
    die("Invalid argument to %.*s \"%s\"", command->first_word_len,
        command->query,command->first_argument);
  sleep_val= my_strtod(sleep_start, &sleep_end, &error);
  if (error)
    die("Invalid argument to %.*s \"%s\"", command->first_word_len,
        command->query, command->first_argument);

  /* Fixed sleep time selected by --sleep option */
  if (opt_sleep >= 0 && !real_sleep)
    sleep_val= opt_sleep;

  if (sleep_val)
    my_sleep((ulong) (sleep_val * 1000000L));
  command->last_argument= sleep_end;
  return 0;
}


static void do_get_file_name(struct st_command *command,
                      char* dest, uint dest_max_len)
{
  char *p= command->first_argument, *name;
  if (!*p)
    die("Missing file name argument");
  name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  if (*p)
    *p++= 0;
  command->last_argument= p;
  strmake(dest, name, dest_max_len - 1);
}


static void do_set_charset(struct st_command *command)
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
  charset_info= get_charset_by_csname(charset_name,MY_CS_PRIMARY,MYF(MY_WME));
  if (!charset_info)
    abort_not_supported_test("Test requires charset '%s'", charset_name);
}


/* List of error names to error codes, available from 5.0 */
typedef struct
{
  const char *name;
  uint        code;
} st_error;

static st_error global_error_names[] =
{
#include <drizzled_ername.h>
  { 0, 0 }
};

static uint get_errcode_from_name(char *error_name, char *error_end)
{
  /* SQL error as string */
  st_error *e= global_error_names;

  /* Loop through the array of known error names */
  for (; e->name; e++)
  {
    /*
      If we get a match, we need to check the length of the name we
      matched against in case it was longer than what we are checking
      (as in ER_WRONG_VALUE vs. ER_WRONG_VALUE_COUNT).
    */
    if (!strncmp(error_name, e->name, (int) (error_end - error_name)) &&
        (uint) strlen(e->name) == (uint) (error_end - error_name))
    {
      return(e->code);
    }
  }
  if (!e->name)
    die("Unknown SQL error name '%s'", error_name);
  return(0);
}

static void do_get_errcodes(struct st_command *command)
{
  struct st_match_err *to= saved_expected_errors.err;
  char *p= command->first_argument;
  uint count= 0;



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
        - Must be SQLSTATE_LENGTH long
        - May contain only digits[0-9] and _uppercase_ letters
      */
      p++; /* Step past the S */
      if ((end - p) != SQLSTATE_LENGTH)
        die("The sqlstate must be exactly %d chars long", SQLSTATE_LENGTH);

      /* Check sqlstate string validity */
      while (*p && p < end)
      {
        if (my_isdigit(charset_info, *p) || my_isupper(charset_info, *p))
          *to_ptr++= *p++;
        else
          die("The sqlstate may only consist of digits[0-9] " \
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
    else
    {
      long val;
      char *start= p;
      /* Check that the string passed to str2int only contain digits */
      while (*p && p != end)
      {
        if (!my_isdigit(charset_info, *p))
          die("Invalid argument to error: '%s' - "\
              "the errno may only consist of digits[0-9]",
              command->first_argument);
        p++;
      }

      /* Convert the sting to int */
      if (!str2int(start, 10, (long) INT_MIN, (long) INT_MAX, &val))
  die("Invalid argument to error: '%s'", command->first_argument);

      to->code.errnum= (uint) val;
      to->type= ERR_ERRNO;
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
                        struct st_command *command)
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


static void set_reconnect(DRIZZLE *drizzle, int val)
{
  bool reconnect= val;

  drizzle_options(drizzle, DRIZZLE_OPT_RECONNECT, (char *)&reconnect);

  return;
}


static int select_connection_name(const char *name)
{
  if (!(cur_con= find_connection_by_name(name)))
    die("connection '%s' not found in connection pool", name);

  /* Update $drizzle_get_server_version to that of current connection */
  var_set_drizzle_get_server_version(&cur_con->drizzle);

  return(0);
}


static int select_connection(struct st_command *command)
{
  char *name;
  char *p= command->first_argument;


  if (!*p)
    die("Missing connection name in connect");
  name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  if (*p)
    *p++= 0;
  command->last_argument= p;
  return(select_connection_name(name));
}


static void do_close_connection(struct st_command *command)
{
  char *p= command->first_argument, *name;
  struct st_connection *con;

  if (!*p)
    die("Missing connection name in disconnect");
  name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;

  if (*p)
    *p++= 0;
  command->last_argument= p;

  if (!(con= find_connection_by_name(name)))
    die("connection '%s' not found in connection pool", name);

  if (command->type == Q_DIRTY_CLOSE)
  {
    if (con->drizzle.net.vio)
    {
      vio_delete(con->drizzle.net.vio);
      con->drizzle.net.vio = 0;
    }
  }

  drizzle_close(&con->drizzle);

  if (con->util_drizzle)
    drizzle_close(con->util_drizzle);
  con->util_drizzle= 0;

  my_free(con->name, MYF(0));

  /*
    When the connection is closed set name to "-closed_connection-"
    to make it possible to reuse the connection name.
  */
  if (!(con->name = my_strdup("-closed_connection-", MYF(MY_WME))))
    die("Out of memory");

  return;
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

static void safe_connect(DRIZZLE *drizzle, const char *name, const char *host,
                  const char *user, const char *pass, const char *db,
                  int port)
{
  int failed_attempts= 0;
  static ulong connection_retry_sleep= 100000; /* Microseconds */


  while(!drizzle_connect(drizzle, host, user, pass, db, port, NULL,
                            CLIENT_MULTI_STATEMENTS | CLIENT_REMEMBER_OPTIONS))
  {
    /*
      Connect failed

      Only allow retry if this was an error indicating the server
      could not be contacted. Error code differs depending
      on protocol/connection type
    */

    if ((drizzle_errno(drizzle) == CR_CONN_HOST_ERROR ||
         drizzle_errno(drizzle) == CR_CONNECTION_ERROR) &&
        failed_attempts < opt_max_connect_retries)
    {
      verbose_msg("Connect attempt %d/%d failed: %d: %s", failed_attempts,
                  opt_max_connect_retries, drizzle_errno(drizzle),
                  drizzle_error(drizzle));
      my_sleep(connection_retry_sleep);
    }
    else
    {
      if (failed_attempts > 0)
        die("Could not open connection '%s' after %d attempts: %d %s", name,
            failed_attempts, drizzle_errno(drizzle), drizzle_error(drizzle));
      else
        die("Could not open connection '%s': %d %s", name,
            drizzle_errno(drizzle), drizzle_error(drizzle));
    }
    failed_attempts++;
  }
  return;
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

static int connect_n_handle_errors(struct st_command *command,
                            DRIZZLE *con, const char* host,
                            const char* user, const char* pass,
                            const char* db, int port, const char* sock)
{
  DYNAMIC_STRING *ds;

  ds= &ds_res;

  /* Only log if an error is expected */
  if (!command->abort_on_error &&
      !disable_query_log)
  {
    /*
      Log the connect to result log
    */
    dynstr_append_mem(ds, "connect(", 8);
    replace_dynstr_append(ds, host);
    dynstr_append_mem(ds, ",", 1);
    replace_dynstr_append(ds, user);
    dynstr_append_mem(ds, ",", 1);
    replace_dynstr_append(ds, pass);
    dynstr_append_mem(ds, ",", 1);
    if (db)
      replace_dynstr_append(ds, db);
    dynstr_append_mem(ds, ",", 1);
    replace_dynstr_append_uint(ds, port);
    dynstr_append_mem(ds, ",", 1);
    if (sock)
      replace_dynstr_append(ds, sock);
    dynstr_append_mem(ds, ")", 1);
    dynstr_append_mem(ds, delimiter, delimiter_length);
    dynstr_append_mem(ds, "\n", 1);
  }
  if (!drizzle_connect(con, host, user, pass, db, port, 0,
                          CLIENT_MULTI_STATEMENTS))
  {
    var_set_errno(drizzle_errno(con));
    handle_error(command, drizzle_errno(con), drizzle_error(con),
     drizzle_sqlstate(con), ds);
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

static void do_connect(struct st_command *command)
{
  int con_port= opt_port;
  char *con_options;
  bool con_ssl= 0, con_compress= 0;
  struct st_connection* con_slot;

  static DYNAMIC_STRING ds_connection_name;
  static DYNAMIC_STRING ds_host;
  static DYNAMIC_STRING ds_user;
  static DYNAMIC_STRING ds_password;
  static DYNAMIC_STRING ds_database;
  static DYNAMIC_STRING ds_port;
  static DYNAMIC_STRING ds_sock;
  static DYNAMIC_STRING ds_options;
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
  if (ds_port.length)
  {
    con_port= atoi(ds_port.str);
    if (con_port == 0)
      die("Illegal argument for port: '%s'", ds_port.str);
  }

  /* Sock */
  if (ds_sock.length)
  {
    /*
      If the socket is specified just as a name without path
      append tmpdir in front
    */
    if (*ds_sock.str != FN_LIBCHAR)
    {
      char buff[FN_REFLEN];
      fn_format(buff, ds_sock.str, TMPDIR, "", 0);
      dynstr_set(&ds_sock, buff);
    }
  }
  else
  {
    /* No socket specified, use default */
    dynstr_set(&ds_sock, unix_sock);
  }

  /* Options */
  con_options= ds_options.str;
  while (*con_options)
  {
    char* end;
    /* Step past any spaces in beginning of option*/
    while (*con_options && my_isspace(charset_info, *con_options))
     con_options++;
    /* Find end of this option */
    end= con_options;
    while (*end && !my_isspace(charset_info, *end))
      end++;
    if (!strncmp(con_options, "SSL", 3))
      con_ssl= 1;
    else if (!strncmp(con_options, "COMPRESS", 8))
      con_compress= 1;
    else
      die("Illegal option to connect: %.*s",
          (int) (end - con_options), con_options);
    /* Process next option */
    con_options= end;
  }

  if (find_connection_by_name(ds_connection_name.str))
    die("Connection %s already exists", ds_connection_name.str);
   
  if (next_con != connections_end)
    con_slot= next_con;
  else
  {
    if (!(con_slot= find_connection_by_name("-closed_connection-")))
      die("Connection limit exhausted, you can have max %d connections",
          (int) (sizeof(connections)/sizeof(struct st_connection)));
  }

#ifdef EMBEDDED_LIBRARY
  con_slot->query_done= 1;
#endif
  if (!drizzle_create(&con_slot->drizzle))
    die("Failed on drizzle_create()");
  if (opt_compress || con_compress)
    drizzle_options(&con_slot->drizzle, DRIZZLE_OPT_COMPRESS, NullS);
  drizzle_options(&con_slot->drizzle, DRIZZLE_OPT_LOCAL_INFILE, 0);
  drizzle_options(&con_slot->drizzle, DRIZZLE_SET_CHARSET_NAME,
                charset_info->csname);
  int opt_protocol= DRIZZLE_PROTOCOL_TCP;
  drizzle_options(&con_slot->drizzle,DRIZZLE_OPT_PROTOCOL,(char*)&opt_protocol);
  if (opt_charsets_dir)
    drizzle_options(&con_slot->drizzle, DRIZZLE_SET_CHARSET_DIR,
                  opt_charsets_dir);

  /* Use default db name */
  if (ds_database.length == 0)
    dynstr_set(&ds_database, opt_db);

  /* Special database to allow one to connect without a database name */
  if (ds_database.length && !strcmp(ds_database.str,"*NO-ONE*"))
    dynstr_set(&ds_database, "");

  if (connect_n_handle_errors(command, &con_slot->drizzle,
                              ds_host.str,ds_user.str,
                              ds_password.str, ds_database.str,
                              con_port, ds_sock.str))
  {
    if (!(con_slot->name= my_strdup(ds_connection_name.str, MYF(MY_WME))))
      die("Out of memory");
    cur_con= con_slot;
   
    if (con_slot == next_con)
      next_con++; /* if we used the next_con slot, advance the pointer */
  }

  /* Update $drizzle_get_server_version to that of current connection */
  var_set_drizzle_get_server_version(&cur_con->drizzle);

  dynstr_free(&ds_connection_name);
  dynstr_free(&ds_host);
  dynstr_free(&ds_user);
  dynstr_free(&ds_password);
  dynstr_free(&ds_database);
  dynstr_free(&ds_port);
  dynstr_free(&ds_sock);
  dynstr_free(&ds_options);
  return;
}


static int do_done(struct st_command *command)
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
  return 0;
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

static void do_block(enum block_cmd cmd, struct st_command* command)
{
  char *p= command->first_argument;
  const char *expr_start, *expr_end;
  VAR v;
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

  var_init(&v,0,0,0,0);
  eval_expr(&v, expr_start, &expr_end);

  /* Define inner block */
  cur_block++;
  cur_block->cmd= cmd;
  cur_block->ok= (v.int_val ? true : false);

  if (not_expr)
    cur_block->ok = !cur_block->ok;

  var_free(&v);
  return;
}


static void do_delimiter(struct st_command* command)
{
  char* p= command->first_argument;

  while (*p && my_isspace(charset_info, *p))
    p++;

  if (!(*p))
    die("Can't set empty delimiter");

  strmake(delimiter, p, sizeof(delimiter) - 1);
  delimiter_length= strlen(delimiter);

  command->last_argument= p + delimiter_length;
  return;
}


bool match_delimiter(int c, const char *delim, uint length)
{
  uint i;
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
  my_fclose(cur_file->file, MYF(0));
        cur_file->file= 0;
      }
      my_free((uchar*) cur_file->file_name, MYF(MY_ALLOW_ZERO_PTR));
      cur_file->file_name= 0;
      if (cur_file == file_stack)
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
                (!my_strnncoll_simple(charset_info, (const uchar*) "while", 5,
                                      (uchar*) buf, min(5, p - buf), 0) ||
                 !my_strnncoll_simple(charset_info, (const uchar*) "if", 2,
                                      (uchar*) buf, min(2, p - buf), 0))))
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
#ifdef USE_MB
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
#endif
  *p++= c;
    }
  }
  die("The input buffer is too small for this query.x\n" \
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
  char *end= strend(query);
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

static void scan_command_for_warnings(struct st_command *command)
{
  const char *ptr= command->query;

  while(*ptr)
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
      uint type;
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
      type= find_type(start, &command_typelib, 1+2);
      if (type)
        warning_msg("Embedded drizzletest command '--%s' detected in "
                    "query '%s' was this intentional? ",
                    start, command->query);
      *end= save;
    }

    ptr++;
  }
  return;
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

#define MAX_QUERY (256*1024*2) /* 256K -- a test in sp-big is >128K */
static char read_command_buf[MAX_QUERY];

static int read_command(struct st_command** command_ptr)
{
  char *p= read_command_buf;
  struct st_command* command;


  if (parser.current_line < parser.read_lines)
  {
    get_dynamic(&q_lines, (uchar*) command_ptr, parser.current_line) ;
    return(0);
  }
  if (!(*command_ptr= command=
        (struct st_command*) my_malloc(sizeof(*command),
                                       MYF(MY_WME|MY_ZEROFILL))) ||
      insert_dynamic(&q_lines, (uchar*) &command))
    die(NullS);
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

  if (!(command->query_buf= command->query= my_strdup(p, MYF(MY_WME))))
    die("Out of memory");

  /* Calculate first word length(the command), terminated by space or ( */
  p= command->query;
  while (*p && !my_isspace(charset_info, *p) && *p != '(')
    p++;
  command->first_word_len= (uint) (p - command->query);

  /* Skip spaces between command and first argument */
  while (*p && my_isspace(charset_info, *p))
    p++;
  command->first_argument= p;

  command->end= strend(command->query);
  command->query_len= (command->end - command->query);
  parser.read_lines++;
  return(0);
}


static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"basedir", 'b', "Basedir for tests.", (char**) &opt_basedir,
   (char**) &opt_basedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.", (char**) &opt_charsets_dir,
   (char**) &opt_charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use the compressed server/client protocol.",
   (char**) &opt_compress, (char**) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"database", 'D', "Database to use.", (char**) &opt_db, (char**) &opt_db, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-check", OPT_DEBUG_CHECK, "Check memory and open file usage at exit.",
   (char**) &debug_check_flag, (char**) &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", OPT_DEBUG_INFO, "Print some debug info at exit.",
   (char**) &debug_info_flag, (char**) &debug_info_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", (char**) &opt_host, (char**) &opt_host, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"include", 'i', "Include SQL before each test case.", (char**) &opt_include,
   (char**) &opt_include, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"logdir", OPT_LOG_DIR, "Directory for log files", (char**) &opt_logdir,
   (char**) &opt_logdir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"mark-progress", OPT_MARK_PROGRESS,
   "Write linenumber and elapsed time to <testname>.progress ",
   (char**) &opt_mark_progress, (char**) &opt_mark_progress, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"max-connect-retries", OPT_MAX_CONNECT_RETRIES,
   "Max number of connection attempts when connecting to server",
   (char**) &opt_max_connect_retries, (char**) &opt_max_connect_retries, 0,
   GET_INT, REQUIRED_ARG, 500, 1, 10000, 0, 0, 0},
  {"password", 'p', "Password to use when connecting to server.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   (char**) &opt_port,
   (char**) &opt_port, 0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"quiet", 's', "Suppress all normal output.", (char**) &silent,
   (char**) &silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"record", 'r', "Record output of test_file into result file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"result-file", 'R', "Read/Store result from/in this file.",
   (char**) &result_file_name, (char**) &result_file_name, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-arg", 'A', "Send option value to embedded server as a parameter.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-file", 'F', "Read embedded server arguments from file.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Suppress all normal output. Synonym for --quiet.",
   (char**) &silent, (char**) &silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sleep", 'T', "Sleep always this many seconds on sleep commands.",
   (char**) &opt_sleep, (char**) &opt_sleep, 0, GET_INT, REQUIRED_ARG, -1, -1, 0,
   0, 0, 0},
  {"tail-lines", OPT_TAIL_LINES,
   "Number of lines of the resul to include in a failure report",
   (char**) &opt_tail_lines, (char**) &opt_tail_lines, 0,
   GET_INT, REQUIRED_ARG, 0, 0, 10000, 0, 0, 0},
  {"test-file", 'x', "Read test from/in this file (default stdin).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"timer-file", 'm', "File where the timing in micro seconds is stored.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't', "Temporary directory where sockets are put.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login.", (char**) &opt_user, (char**) &opt_user, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Write more.", (char**) &verbose, (char**) &verbose, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,MTEST_VERSION,
   MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage(void)
{
  print_version();
  printf("DRIZZLE AB, by Sasha, Matt, Monty & Jani\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Runs a test against the DRIZZLE server and compares output with a results file.\n\n");
  printf("Usage: %s [OPTIONS] [database] < test_file\n", my_progname);
  my_print_help(my_long_options);
  printf("  --no-defaults       Don't read default options from any options file.\n");
  my_print_variables(my_long_options);
}

/*
  Read arguments for embedded server and put them into
  embedded_server_args[]
*/

static void read_embedded_server_arguments(const char *name)
{
  char argument[1024],buff[FN_REFLEN], *str=0;
  FILE *file;

  if (!test_if_hard_path(name))
  {
    strxmov(buff, opt_basedir, name, NullS);
    name=buff;
  }
  fn_format(buff, name, "", "", MY_UNPACK_FILENAME);

  if (!embedded_server_arg_count)
  {
    embedded_server_arg_count=1;
    embedded_server_args[0]= (char*) "";    /* Progname */
  }
  if (!(file=my_fopen(buff, O_RDONLY | FILE_BINARY, MYF(MY_WME))))
    die("Failed to open file '%s'", buff);

  while (embedded_server_arg_count < MAX_EMBEDDED_SERVER_ARGS &&
   (str=fgets(argument,sizeof(argument), file)))
  {
    *(strend(str)-1)=0;        /* Remove end newline */
    if (!(embedded_server_args[embedded_server_arg_count]=
    (char*) my_strdup(str,MYF(MY_WME))))
    {
      my_fclose(file,MYF(0));
      die("Out of memory");

    }
    embedded_server_arg_count++;
  }
  my_fclose(file,MYF(0));
  if (str)
    die("Too many arguments in option file: %s",name);

  return;
}


static bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
         char *argument)
{
  switch(optid) {
  case 'r':
    record = 1;
    break;
  case 'x':
  {
    char buff[FN_REFLEN];
    if (!test_if_hard_path(argument))
    {
      strxmov(buff, opt_basedir, argument, NullS);
      argument= buff;
    }
    fn_format(buff, argument, "", "", MY_UNPACK_FILENAME);
    assert(cur_file == file_stack && cur_file->file == 0);
    if (!(cur_file->file=
          my_fopen(buff, O_RDONLY | FILE_BINARY, MYF(0))))
      die("Could not open '%s' for reading: errno = %d", buff, errno);
    cur_file->file_name= my_strdup(buff, MYF(MY_FAE));
    cur_file->lineno= 1;
    break;
  }
  case 'm':
  {
    static char buff[FN_REFLEN];
    if (!test_if_hard_path(argument))
    {
      strxmov(buff, opt_basedir, argument, NullS);
      argument= buff;
    }
    fn_format(buff, argument, "", "", MY_UNPACK_FILENAME);
    timer_file= buff;
    unlink(timer_file);       /* Ignore error, may not exist */
    break;
  }
  case 'p':
    if (argument)
    {
      my_free(opt_pass, MYF(MY_ALLOW_ZERO_PTR));
      opt_pass= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';    /* Destroy argument */
      tty_password= 0;
    }
    else
      tty_password= 1;
    break;
  case 't':
    strnmov(TMPDIR, argument, sizeof(TMPDIR));
    break;
  case 'A':
    if (!embedded_server_arg_count)
    {
      embedded_server_arg_count=1;
      embedded_server_args[0]= (char*) "";
    }
    if (embedded_server_arg_count == MAX_EMBEDDED_SERVER_ARGS-1 ||
        !(embedded_server_args[embedded_server_arg_count++]=
          my_strdup(argument, MYF(MY_FAE))))
    {
      die("Can't use server argument");
    }
    break;
  case 'F':
    read_embedded_server_arguments(argument);
    break;
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}


static int parse_args(int argc, char **argv)
{
  load_defaults("my",load_default_groups,&argc,&argv);
  default_argv= argv;

  if ((handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(1);

  if (argc > 1)
  {
    usage();
    exit(1);
  }
  if (argc == 1)
    opt_db= *argv;
  if (tty_password)
    opt_pass= get_tty_password(NullS);          /* purify tested */
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;

  return 0;
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

void str_to_file2(const char *fname, char *str, int size, bool append)
{
  int fd;
  char buff[FN_REFLEN];
  int flags= O_WRONLY | O_CREAT;
  if (!test_if_hard_path(fname))
  {
    strxmov(buff, opt_basedir, fname, NullS);
    fname= buff;
  }
  fn_format(buff, fname, "", "", MY_UNPACK_FILENAME);

  if (!append)
    flags|= O_TRUNC;
  if ((fd= my_open(buff, flags,
                   MYF(MY_WME | MY_FFNF))) < 0)
    die("Could not open '%s' for writing: errno = %d", buff, errno);
  if (append && my_seek(fd, 0, SEEK_END, MYF(0)) == MY_FILEPOS_ERROR)
    die("Could not find end of file '%s': errno = %d", buff, errno);
  if (my_write(fd, (uchar*)str, size, MYF(MY_WME|MY_FNABP)))
    die("write failed");
  my_close(fd, MYF(0));
}

/*
  Write the content of str into file

  SYNOPSIS
  str_to_file
  fname - name of file to truncate/create and write to
  str - content to write to file
  size - size of content witten to file
*/

void str_to_file(const char *fname, char *str, int size)
{
  str_to_file2(fname, str, size, false);
}


void dump_result_to_log_file(char *buf, int size)
{
  char log_file[FN_REFLEN];
  str_to_file(fn_format(log_file, result_file_name, opt_logdir, ".log",
                        *opt_logdir ? MY_REPLACE_DIR | MY_REPLACE_EXT :
                        MY_REPLACE_EXT),
              buf, size);
  fprintf(stderr, "\nMore results from queries before failure can be found in %s\n",
          log_file);
}

void dump_progress(void)
{
  char progress_file[FN_REFLEN];
  str_to_file(fn_format(progress_file, result_file_name,
                        opt_logdir, ".progress",
                        *opt_logdir ? MY_REPLACE_DIR | MY_REPLACE_EXT :
                        MY_REPLACE_EXT),
              ds_progress.str, ds_progress.length);
}

void dump_warning_messages(void)
{
  char warn_file[FN_REFLEN];

  str_to_file(fn_format(warn_file, result_file_name, opt_logdir, ".warnings",
                        *opt_logdir ? MY_REPLACE_DIR | MY_REPLACE_EXT :
                        MY_REPLACE_EXT),
              ds_warning_messages.str, ds_warning_messages.length);
}


/*
  Append the result for one field to the dynamic string ds
*/

static void append_field(DYNAMIC_STRING *ds, uint col_idx, DRIZZLE_FIELD* field,
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
      dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_mem(ds, val, (int)len);
  }
  else
  {
    dynstr_append(ds, field->name);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_mem(ds, val, (int)len);
    dynstr_append_mem(ds, "\n", 1);
  }
}


/*
  Append all results to the dynamic string separated with '\t'
  Values may be converted with 'replace_column'
*/

static void append_result(DYNAMIC_STRING *ds, DRIZZLE_RES *res)
{
  DRIZZLE_ROW row;
  uint32_t num_fields= drizzle_num_fields(res);
  DRIZZLE_FIELD *fields= drizzle_fetch_fields(res);
  uint32_t *lengths;

  while ((row = drizzle_fetch_row(res)))
  {
    uint32_t i;
    lengths = drizzle_fetch_lengths(res);
    for (i = 0; i < num_fields; i++)
      append_field(ds, i, &fields[i],
                   (const char*)row[i], lengths[i], !row[i]);
    if (!display_result_vertically)
      dynstr_append_mem(ds, "\n", 1);
  }
}


/*
  Append metadata for fields to output
*/

static void append_metadata(DYNAMIC_STRING *ds,
                            DRIZZLE_FIELD *field,
                            uint num_fields)
{
  DRIZZLE_FIELD *field_end;
  dynstr_append(ds,"Catalog\tDatabase\tTable\tTable_alias\tColumn\t"
                "Column_alias\tType\tLength\tMax length\tIs_null\t"
                "Flags\tDecimals\tCharsetnr\n");

  for (field_end= field+num_fields ;
       field < field_end ;
       field++)
  {
    dynstr_append_mem(ds, field->catalog,
                      field->catalog_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->db, field->db_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->org_table,
                      field->org_table_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->table,
                      field->table_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->org_name,
                      field->org_name_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->name, field->name_length);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->type);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->length);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->max_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, (char*) (IS_NOT_NULL(field->flags) ?
                                   "N" : "Y"), 1);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->flags);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->decimals);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->charsetnr);
    dynstr_append_mem(ds, "\n", 1);
  }
}


/*
  Append affected row count and other info to output
*/

static void append_info(DYNAMIC_STRING *ds, uint64_t affected_rows,
                        const char *info)
{
  char buf[40], buff2[21];
  sprintf(buf,"affected rows: %s\n", llstr(affected_rows, buff2));
  dynstr_append(ds, buf);
  if (info)
  {
    dynstr_append(ds, "info: ");
    dynstr_append(ds, info);
    dynstr_append_mem(ds, "\n", 1);
  }
}


/*
  Display the table headings with the names tab separated
*/

static void append_table_headings(DYNAMIC_STRING *ds,
                                  DRIZZLE_FIELD *field,
                                  uint num_fields)
{
  uint col_idx;
  for (col_idx= 0; col_idx < num_fields; col_idx++)
  {
    if (col_idx)
      dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append(ds, field[col_idx].name);
  }
  dynstr_append_mem(ds, "\n", 1);
}

/*
  Fetch warnings from server and append to ds

  RETURN VALUE
  Number of warnings appended to ds
*/

static int append_warnings(DYNAMIC_STRING *ds, DRIZZLE *drizzle)
{
  uint count;
  DRIZZLE_RES *warn_res;


  if (!(count= drizzle_warning_count(drizzle)))
    return(0);

  /*
    If one day we will support execution of multi-statements
    through PS API we should not issue SHOW WARNINGS until
    we have not read all results...
  */
  assert(!drizzle_more_results(drizzle));

  if (drizzle_real_query(drizzle, "SHOW WARNINGS", 13))
    die("Error running query \"SHOW WARNINGS\": %s", drizzle_error(drizzle));

  if (!(warn_res= drizzle_store_result(drizzle)))
    die("Warning count is %u but didn't get any warnings",
  count);

  append_result(ds, warn_res);
  drizzle_free_result(warn_res);

  return(count);
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

static void run_query_normal(struct st_connection *cn,
                             struct st_command *command,
                             int flags, char *query, int query_len,
                             DYNAMIC_STRING *ds, DYNAMIC_STRING *ds_warnings)
{
  DRIZZLE_RES *res= 0;
  DRIZZLE *drizzle= &cn->drizzle;
  int err= 0, counter= 0;

  if (flags & QUERY_SEND_FLAG)
  {
    /*
      Send the query
    */
    if (do_send_query(cn, query, query_len, flags))
    {
      handle_error(command, drizzle_errno(drizzle), drizzle_error(drizzle),
       drizzle_sqlstate(drizzle), ds);
      goto end;
    }
  }
#ifdef EMBEDDED_LIBRARY
  /*
    Here we handle 'reap' command, so we need to check if the
    query's thread was finished and probably wait
  */
  else if (flags & QUERY_REAP_FLAG)
    wait_query_thread_end(cn);
#endif /*EMBEDDED_LIBRARY*/
  if (!(flags & QUERY_REAP_FLAG))
    return;

  do
  {
    /*
      When  on first result set, call drizzle_read_query_result to retrieve
      answer to the query sent earlier
    */
    if ((counter==0) && drizzle_read_query_result(drizzle))
    {
      handle_error(command, drizzle_errno(drizzle), drizzle_error(drizzle),
       drizzle_sqlstate(drizzle), ds);
      goto end;

    }

    /*
      Store the result of the query if it will return any fields
    */
    if (drizzle_field_count(drizzle) && ((res= drizzle_store_result(drizzle)) == 0))
    {
      handle_error(command, drizzle_errno(drizzle), drizzle_error(drizzle),
       drizzle_sqlstate(drizzle), ds);
      goto end;
    }

    if (!disable_result_log)
    {
      uint64_t affected_rows= 0;    /* Ok to be undef if 'disable_info' is set */

      if (res)
      {
  DRIZZLE_FIELD *fields= drizzle_fetch_fields(res);
  uint num_fields= drizzle_num_fields(res);

  if (display_metadata)
          append_metadata(ds, fields, num_fields);

  if (!display_result_vertically)
    append_table_headings(ds, fields, num_fields);

  append_result(ds, res);
      }

      /*
        Need to call drizzle_affected_rows() before the "new"
        query to find the warnings
      */
      if (!disable_info)
        affected_rows= drizzle_affected_rows(drizzle);

      /*
        Add all warnings to the result. We can't do this if we are in
        the middle of processing results from multi-statement, because
        this will break protocol.
      */
      if (!disable_warnings && !drizzle_more_results(drizzle))
      {
  if (append_warnings(ds_warnings, drizzle) || ds_warnings->length)
  {
    dynstr_append_mem(ds, "Warnings:\n", 10);
    dynstr_append_mem(ds, ds_warnings->str, ds_warnings->length);
  }
      }

      if (!disable_info)
  append_info(ds, affected_rows, drizzle_info(drizzle));
    }

    if (res)
    {
      drizzle_free_result(res);
      res= 0;
    }
    counter++;
  } while (!(err= drizzle_next_result(drizzle)));
  if (err > 0)
  {
    /* We got an error from drizzle_next_result, maybe expected */
    handle_error(command, drizzle_errno(drizzle), drizzle_error(drizzle),
     drizzle_sqlstate(drizzle), ds);
    goto end;
  }
  assert(err == -1); /* Successful and there are no more results */

  /* If we come here the query is both executed and read successfully */
  handle_no_error(command);

end:

  /*
    We save the return code (drizzle_errno(drizzle)) from the last call sent
    to the server into the drizzletest builtin variable $drizzle_errno. This
    variable then can be used from the test case itself.
  */
  var_set_errno(drizzle_errno(drizzle));
  return;
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

void handle_error(struct st_command *command,
                  unsigned int err_errno, const char *err_error,
                  const char *err_sqlstate, DYNAMIC_STRING *ds)
{
  uint i;



  if (command->require_file[0])
  {
    /*
      The query after a "--require" failed. This is fine as long the server
      returned a valid reponse. Don't allow 2013 or 2006 to trigger an
      abort_not_supported_test
    */
    if (err_errno == CR_SERVER_LOST ||
        err_errno == CR_SERVER_GONE_ERROR)
      die("require query '%s' failed: %d: %s", command->query,
          err_errno, err_error);

    /* Abort the run of this test, pass the failed query as reason */
    abort_not_supported_test("Query '%s' failed, required functionality " \
                             "not supported", command->query);
  }

  if (command->abort_on_error)
    die("query '%s' failed: %d: %s", command->query, err_errno, err_error);

  for (i= 0 ; (uint) i < command->expected_errors.count ; i++)
  {
    if (((command->expected_errors.err[i].type == ERR_ERRNO) &&
         (command->expected_errors.err[i].code.errnum == err_errno)) ||
        ((command->expected_errors.err[i].type == ERR_SQLSTATE) &&
         (strncmp(command->expected_errors.err[i].code.sqlstate,
                  err_sqlstate, SQLSTATE_LENGTH) == 0)))
    {
      if (!disable_result_log)
      {
        if (command->expected_errors.count == 1)
        {
          /* Only log error if there is one possible error */
          dynstr_append_mem(ds, "ERROR ", 6);
          replace_dynstr_append(ds, err_sqlstate);
          dynstr_append_mem(ds, ": ", 2);
          replace_dynstr_append(ds, err_error);
          dynstr_append_mem(ds,"\n",1);
        }
        /* Don't log error if we may not get an error */
        else if (command->expected_errors.err[0].type == ERR_SQLSTATE ||
                 (command->expected_errors.err[0].type == ERR_ERRNO &&
                  command->expected_errors.err[0].code.errnum != 0))
          dynstr_append(ds,"Got one of the listed errors\n");
      }
      /* OK */
      return;
    }
  }

  if (!disable_result_log)
  {
    dynstr_append_mem(ds, "ERROR ",6);
    replace_dynstr_append(ds, err_sqlstate);
    dynstr_append_mem(ds, ": ", 2);
    replace_dynstr_append(ds, err_error);
    dynstr_append_mem(ds, "\n", 1);
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

  return;
}


/*
  Handle absence of errors after execution

  SYNOPSIS
  handle_no_error()
  q - context of query

  RETURN VALUE
  error - function will not return
*/

void handle_no_error(struct st_command *command)
{


  if (command->expected_errors.err[0].type == ERR_ERRNO &&
      command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    die("query '%s' succeeded - should have failed with errno %d...",
        command->query, command->expected_errors.err[0].code.errnum);
  }
  else if (command->expected_errors.err[0].type == ERR_SQLSTATE &&
           strcmp(command->expected_errors.err[0].code.sqlstate,"00000") != 0)
  {
    /* SQLSTATE we wanted was != "00000", i.e. not an expected success */
    die("query '%s' succeeded - should have failed with sqlstate %s...",
        command->query, command->expected_errors.err[0].code.sqlstate);
  }

  return;
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

static void run_query(struct st_connection *cn,
                      struct st_command *command,
                      int flags)
{
  DYNAMIC_STRING *ds;
  DYNAMIC_STRING *save_ds= NULL;
  DYNAMIC_STRING ds_result;
  DYNAMIC_STRING ds_sorted;
  DYNAMIC_STRING ds_warnings;
  DYNAMIC_STRING eval_query;
  char *query;
  int query_len;


  init_dynamic_string(&ds_warnings, NULL, 0, 256);

  /* Scan for warning before sending to server */
  scan_command_for_warnings(command);

  /*
    Evaluate query if this is an eval command
  */
  if (command->type == Q_EVAL)
  {
    init_dynamic_string(&eval_query, "", command->query_len+256, 1024);
    do_eval(&eval_query, command->query, command->end, false);
    query = eval_query.str;
    query_len = eval_query.length;
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
  if (command->require_file[0])
  {
    init_dynamic_string(&ds_result, "", 1024, 1024);
    ds= &ds_result;
  }
  else
    ds= &ds_res;

  /*
    Log the query into the output buffer
  */
  if (!disable_query_log && (flags & QUERY_SEND_FLAG))
  {
    replace_dynstr_append_mem(ds, query, query_len);
    dynstr_append_mem(ds, delimiter, delimiter_length);
    dynstr_append_mem(ds, "\n", 1);
  }

  if (display_result_sorted)
  {
    /*
       Collect the query output in a separate string
       that can be sorted before it's added to the
       global result string
    */
    init_dynamic_string(&ds_sorted, "", 1024, 1024);
    save_ds= ds; /* Remember original ds */
    ds= &ds_sorted;
  }

  /*
    Always run with normal C API if it's not a complete
    SEND + REAP
  */
  run_query_normal(cn, command, flags, query, query_len,
                   ds, &ds_warnings);

  if (display_result_sorted)
  {
    /* Sort the result set and append it to result */
    dynstr_append_sorted(save_ds, &ds_sorted);
    ds= save_ds;
    dynstr_free(&ds_sorted);
  }

  if (command->require_file[0])
  {
    /* A result file was specified for _this_ query
       and the output should be checked against an already
       existing file which has been specified using --require or --result
    */
    check_require(ds, command->require_file);
  }

  dynstr_free(&ds_warnings);
  if (ds == &ds_result)
    dynstr_free(&ds_result);
  if (command->type == Q_EVAL)
    dynstr_free(&eval_query);
  return;
}


/****************************************************************************/

static void get_command_type(struct st_command* command)
{
  char save;
  uint type;


  if (*command->query == '}')
  {
    command->type = Q_END_BLOCK;
    return;
  }

  save= command->query[command->first_word_len];
  command->query[command->first_word_len]= 0;
  type= find_type(command->query, &command_typelib, 1+2);
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
      warning_msg("Suspicious command '--%s' detected, was this intentional? "\
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
        if (find_type(command->query, &command_typelib, 1+2) > 0)
          die("Extra delimiter \";\" found");
        command->query[command->first_word_len-1]= save;

      }
    }
  }

  /* Set expected error on command */
  memcpy(&command->expected_errors, &saved_expected_errors,
         sizeof(saved_expected_errors));
  command->abort_on_error= (command->expected_errors.count == 0 &&
                            abort_on_error);

  return;
}



/*
  Record how many milliseconds it took to execute the test file
  up until the current line and save it in the dynamic string ds_progress.

  The ds_progress will be dumped to <test_name>.progress when
  test run completes

*/

static void mark_progress(struct st_command* command __attribute__((unused)),
                          int line)
{
  char buf[32], *end;
  uint64_t timer= timer_now();
  if (!progress_start)
    progress_start= timer;
  timer-= progress_start;

  /* Milliseconds since start */
  end= int64_t2str(timer, buf, 10);
  dynstr_append_mem(&ds_progress, buf, (int)(end-buf));
  dynstr_append_mem(&ds_progress, "\t", 1);

  /* Parser line number */
  end= int10_to_str(line, buf, 10);
  dynstr_append_mem(&ds_progress, buf, (int)(end-buf));
  dynstr_append_mem(&ds_progress, "\t", 1);

  /* Filename */
  dynstr_append(&ds_progress, cur_file->file_name);
  dynstr_append_mem(&ds_progress, ":", 1);

  /* Line in file */
  end= int10_to_str(cur_file->lineno, buf, 10);
  dynstr_append_mem(&ds_progress, buf, (int)(end-buf));


  dynstr_append_mem(&ds_progress, "\n", 1);

}


int main(int argc, char **argv)
{
  struct st_command *command;
  bool q_send_flag= 0, abort_flag= 0;
  uint command_executed= 0, last_command_executed= 0;
  char save_file[FN_REFLEN];
  struct stat res_info;
  MY_INIT(argv[0]);

  save_file[0]= 0;
  TMPDIR[0]= 0;

  /* Init expected errors */
  memset(&saved_expected_errors, 0, sizeof(saved_expected_errors));

  /* Init connections */
  memset(connections, 0, sizeof(connections));
  connections_end= connections +
    (sizeof(connections)/sizeof(struct st_connection)) - 1;
  next_con= connections + 1;

#ifdef EMBEDDED_LIBRARY
  /* set appropriate stack for the 'query' threads */
  (void) pthread_attr_init(&cn_thd_attrib);
  pthread_attr_setstacksize(&cn_thd_attrib, DEFAULT_THREAD_STACK);
#endif /*EMBEDDED_LIBRARY*/

  /* Init file stack */
  memset(file_stack, 0, sizeof(file_stack));
  file_stack_end=
    file_stack + (sizeof(file_stack)/sizeof(struct st_test_file)) - 1;
  cur_file= file_stack;

  /* Init block stack */
  memset(block_stack, 0, sizeof(block_stack));
  block_stack_end=
    block_stack + (sizeof(block_stack)/sizeof(struct st_block)) - 1;
  cur_block= block_stack;
  cur_block->ok= true; /* Outer block should always be executed */
  cur_block->cmd= cmd_none;

  my_init_dynamic_array(&q_lines, sizeof(struct st_command*), 1024, 1024);

  if (hash_init(&var_hash, charset_info,
                1024, 0, 0, get_var_key, var_free, MYF(0)))
    die("Variable hash initialization failed");

  var_set_string("$MYSQL_SERVER_VERSION", MYSQL_SERVER_VERSION);

  memset(&master_pos, 0, sizeof(master_pos));

  parser.current_line= parser.read_lines= 0;
  memset(&var_reg, 0, sizeof(var_reg));

  init_builtin_echo();

  init_dynamic_string(&ds_res, "", 65536, 65536);
  init_dynamic_string(&ds_progress, "", 0, 2048);
  init_dynamic_string(&ds_warning_messages, "", 0, 2048);
  parse_args(argc, argv);

  server_initialized= 1;
  if (cur_file == file_stack && cur_file->file == 0)
  {
    cur_file->file= stdin;
    cur_file->file_name= my_strdup("<stdin>", MYF(MY_WME));
    cur_file->lineno= 1;
  }
  cur_con= connections;
  if (!( drizzle_create(&cur_con->drizzle)))
    die("Failed in drizzle_create()");
  if (opt_compress)
    drizzle_options(&cur_con->drizzle,DRIZZLE_OPT_COMPRESS,NullS);
  drizzle_options(&cur_con->drizzle, DRIZZLE_OPT_LOCAL_INFILE, 0);
  drizzle_options(&cur_con->drizzle, DRIZZLE_SET_CHARSET_NAME,
                charset_info->csname);
  int opt_protocol= DRIZZLE_PROTOCOL_TCP;
  drizzle_options(&cur_con->drizzle,DRIZZLE_OPT_PROTOCOL,(char*)&opt_protocol);
  if (opt_charsets_dir)
    drizzle_options(&cur_con->drizzle, DRIZZLE_SET_CHARSET_DIR,
                  opt_charsets_dir);

  if (!(cur_con->name = my_strdup("default", MYF(MY_WME))))
    die("Out of memory");

  safe_connect(&cur_con->drizzle, cur_con->name, opt_host, opt_user, opt_pass,
               opt_db, opt_port);

  /* Use all time until exit if no explicit 'start_timer' */
  timer_start= timer_now();

  /*
    Initialize $drizzle_errno with -1, so we can
    - distinguish it from valid values ( >= 0 ) and
    - detect if there was never a command sent to the server
  */
  var_set_errno(-1);

  /* Update $drizzle_get_server_version to that of current connection */
  var_set_drizzle_get_server_version(&cur_con->drizzle);

  if (opt_include)
  {
    open_file(opt_include);
  }

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
      case Q_CONNECTION: select_connection(command); break;
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
      case Q_WAIT_FOR_SLAVE_TO_STOP: do_wait_for_slave_to_stop(command); break;
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

  if (save_file[0])
  {
    strmake(command->require_file, save_file, sizeof(save_file) - 1);
    save_file[0]= 0;
  }
  run_query(cur_con, command, flags);
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
  run_query(cur_con, command, QUERY_SEND_FLAG);
  command_executed++;
        command->last_argument= command->end;
  break;
      case Q_REQUIRE:
  do_get_file_name(command, save_file, sizeof(save_file));
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
  (void) drizzle_ping(&cur_con->drizzle);
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
        set_reconnect(&cur_con->drizzle, 0);
        break;
      case Q_ENABLE_RECONNECT:
        set_reconnect(&cur_con->drizzle, 1);
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
  if (ds_res.length)
  {
    if (result_file_name)
    {
      /* A result file has been specified */

      if (record)
      {
  /* Recording - dump the output from test to result file */
  str_to_file(result_file_name, ds_res.str, ds_res.length);
      }
      else
      {
  /* Check that the output from test is equal to result file
     - detect missing result file
     - detect zero size result file
        */
  check_result(&ds_res);
      }
    }
    else
    {
      /* No result_file_name specified to compare with, print to stdout */
      printf("%s", ds_res.str);
    }
  }
  else
  {
    die("The test didn't produce any output");
  }

  if (!command_executed &&
      result_file_name && !stat(result_file_name, &res_info))
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

  if ( opt_mark_progress && result_file_name )
    dump_progress();

  /* Dump warning messages */
  if (result_file_name && ds_warning_messages.length)
    dump_warning_messages();

  timer_output();
  /* Yes, if we got this far the test has suceeded! Sakila smiles */
  cleanup_and_exit(0);
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

void timer_output(void)
{
  if (timer_file)
  {
    char buf[32], *end;
    uint64_t timer= timer_now() - timer_start;
    end= int64_t2str(timer, buf, 10);
    str_to_file(timer_file,buf, (int) (end-buf));
    /* Timer has been written to the file, don't use it anymore */
    timer_file= 0;
  }
}


uint64_t timer_now(void)
{
  return my_micro_time() / 1000;
}


/*
  Get arguments for replace_columns. The syntax is:
  replace-column column_number to_string [column_number to_string ...]
  Where each argument may be quoted with ' or "
  A argument may also be a variable, in which case the value of the
  variable is replaced.
*/

void do_get_replace_column(struct st_command *command)
{
  char *from= command->first_argument;
  char *buff, *start;


  free_replace_column();
  if (!*from)
    die("Missing argument in %s", command->query);

  /* Allocate a buffer for results */
  start= buff= (char *)my_malloc(strlen(from)+1,MYF(MY_WME | MY_FAE));
  while (*from)
  {
    char *to;
    uint column_number;

    to= get_string(&buff, &from, command);
    if (!(column_number= atoi(to)) || column_number > MAX_COLUMNS)
      die("Wrong column number to replace_column in '%s'", command->query);
    if (!*from)
      die("Wrong number of arguments to replace_column in '%s'", command->query);
    to= get_string(&buff, &from, command);
    my_free(replace_column[column_number-1], MY_ALLOW_ZERO_PTR);
    replace_column[column_number-1]= my_strdup(to, MYF(MY_WME | MY_FAE));
    set_if_bigger(max_replace_column, column_number);
  }
  my_free(start, MYF(0));
  command->last_argument= command->end;
}


void free_replace_column()
{
  uint i;
  for (i=0 ; i < max_replace_column ; i++)
  {
    if (replace_column[i])
    {
      my_free(replace_column[i], 0);
      replace_column[i]= 0;
    }
  }
  max_replace_column= 0;
}


/****************************************************************************/
/*
  Replace functions
*/

/* Definitions for replace result */

typedef struct st_pointer_array {    /* when using array-strings */
  TYPELIB typelib;        /* Pointer to strings */
  uchar  *str;          /* Strings is here */
  int7  *flag;          /* Flag about each var. */
  uint  array_allocs,max_count,length,max_length;
} POINTER_ARRAY;

struct st_replace;
struct st_replace *init_replace(char * *from, char * *to, uint count,
        char * word_end_chars);
int insert_pointer_name(POINTER_ARRAY *pa,char * name);
void replace_strings_append(struct st_replace *rep, DYNAMIC_STRING* ds,
                            const char *from, int len);
void free_pointer_array(POINTER_ARRAY *pa);

struct st_replace *glob_replace;

/*
  Get arguments for replace. The syntax is:
  replace from to [from to ...]
  Where each argument may be quoted with ' or "
  A argument may also be a variable, in which case the value of the
  variable is replaced.
*/

void do_get_replace(struct st_command *command)
{
  uint i;
  char *from= command->first_argument;
  char *buff, *start;
  char word_end_chars[256], *pos;
  POINTER_ARRAY to_array, from_array;


  free_replace();

  bzero((char*) &to_array,sizeof(to_array));
  bzero((char*) &from_array,sizeof(from_array));
  if (!*from)
    die("Missing argument in %s", command->query);
  start= buff= (char *)my_malloc(strlen(from)+1,MYF(MY_WME | MY_FAE));
  while (*from)
  {
    char *to= buff;
    to= get_string(&buff, &from, command);
    if (!*from)
      die("Wrong number of arguments to replace_result in '%s'",
          command->query);
    insert_pointer_name(&from_array,to);
    to= get_string(&buff, &from, command);
    insert_pointer_name(&to_array,to);
  }
  for (i= 1,pos= word_end_chars ; i < 256 ; i++)
    if (my_isspace(charset_info,i))
      *pos++= i;
  *pos=0;          /* End pointer */
  if (!(glob_replace= init_replace((char**) from_array.typelib.type_names,
          (char**) to_array.typelib.type_names,
          (uint) from_array.typelib.count,
          word_end_chars)))
    die("Can't initialize replace from '%s'", command->query);
  free_pointer_array(&from_array);
  free_pointer_array(&to_array);
  my_free(start, MYF(0));
  command->last_argument= command->end;
  return;
}


void free_replace()
{

  if (glob_replace)
  {
    my_free(glob_replace,MYF(0));
    glob_replace=0;
  }
  return;
}


typedef struct st_replace {
  bool found;
  struct st_replace *next[256];
} REPLACE;

typedef struct st_replace_found {
  bool found;
  char *replace_string;
  uint to_offset;
  int from_offset;
} REPLACE_STRING;


void replace_strings_append(REPLACE *rep, DYNAMIC_STRING* ds,
                            const char *str,
                            int len __attribute__((unused)))
{
  register REPLACE *rep_pos;
  register REPLACE_STRING *rep_str;
  const char *start, *from;


  start= from= str;
  rep_pos=rep+1;
  for (;;)
  {
    /* Loop through states */
    while (!rep_pos->found)
      rep_pos= rep_pos->next[(uchar) *from++];

    /* Does this state contain a string to be replaced */
    if (!(rep_str = ((REPLACE_STRING*) rep_pos))->replace_string)
    {
      /* No match found */
      dynstr_append_mem(ds, start, from - start - 1);
      return;
    }

    /* Append part of original string before replace string */
    dynstr_append_mem(ds, start, (from - rep_str->to_offset) - start);

    /* Append replace string */
    dynstr_append_mem(ds, rep_str->replace_string,
                      strlen(rep_str->replace_string));

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
};

struct st_replace_regex
{
  DYNAMIC_ARRAY regex_arr; /* stores a list of st_regex subsitutions */

  /*
    Temporary storage areas for substitutions. To reduce unnessary copying
    and memory freeing/allocation, we pre-allocate two buffers, and alternate
    their use, one for input/one for output, the roles changing on the next
    st_regex substition. At the end of substitutions  buf points to the
    one containing the final result.
  */
  char* buf;
  char* even_buf;
  char* odd_buf;
  int even_buf_len;
  int odd_buf_len;
};

struct st_replace_regex *glob_replace_regex= 0;

int reg_replace(char** buf_p, int* buf_len_p, char *pattern, char *replace,
                char *string, int icase);



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

static struct st_replace_regex* init_replace_regex(char* expr)
{
  struct st_replace_regex* res;
  char* buf,*expr_end;
  char* p;
  char* buf_p;
  uint expr_len= strlen(expr);
  char last_c = 0;
  struct st_regex reg;

  /* my_malloc() will die on fail with MY_FAE */
  res=(struct st_replace_regex*)my_malloc(
                                          sizeof(*res)+expr_len ,MYF(MY_FAE+MY_WME));
  my_init_dynamic_array(&res->regex_arr,sizeof(struct st_regex),128,128);

  buf= (char*)res + sizeof(*res);
  expr_end= expr + expr_len;
  p= expr;
  buf_p= buf;

  /* for each regexp substitution statement */
  while (p < expr_end)
  {
    bzero(&reg,sizeof(reg));
    /* find the start of the statement */
    while (p < expr_end)
    {
      if (*p == '/')
        break;
      p++;
    }

    if (p == expr_end || ++p == expr_end)
    {
      if (res->regex_arr.elements)
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
      reg.icase= 1;

    /* done parsing the statement, now place it in regex_arr */
    if (insert_dynamic(&res->regex_arr,(uchar*) &reg))
      die("Out of memory");
  }
  res->odd_buf_len= res->even_buf_len= 8192;
  res->even_buf= (char*)my_malloc(res->even_buf_len,MYF(MY_WME+MY_FAE));
  res->odd_buf= (char*)my_malloc(res->odd_buf_len,MYF(MY_WME+MY_FAE));
  res->buf= res->even_buf;

  return res;

err:
  my_free(res,0);
  die("Error parsing replace_regex \"%s\"", expr);
  return 0;
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

static int multi_reg_replace(struct st_replace_regex* r,char* val)
{
  uint i;
  char* in_buf, *out_buf;
  int* buf_len_p;

  in_buf= val;
  out_buf= r->even_buf;
  buf_len_p= &r->even_buf_len;
  r->buf= 0;

  /* For each substitution, do the replace */
  for (i= 0; i < r->regex_arr.elements; i++)
  {
    struct st_regex re;
    char* save_out_buf= out_buf;

    get_dynamic(&r->regex_arr,(uchar*)&re,i);

    if (!reg_replace(&out_buf, buf_len_p, re.pattern, re.replace,
                     in_buf, re.icase))
    {
      /* if the buffer has been reallocated, make adjustements */
      if (save_out_buf != out_buf)
      {
        if (save_out_buf == r->even_buf)
          r->even_buf= out_buf;
        else
          r->odd_buf= out_buf;
      }

      r->buf= out_buf;
      if (in_buf == val)
        in_buf= r->odd_buf;

      swap_variables(char*,in_buf,out_buf);

      buf_len_p= (out_buf == r->even_buf) ? &r->even_buf_len :
        &r->odd_buf_len;
    }
  }

  return (r->buf == 0);
}

/*
  Parse the regular expression to be used in all result files
  from now on.

  The syntax is --replace_regex /from/to/i /from/to/i ...
  i means case-insensitive match. If omitted, the match is
  case-sensitive

*/
void do_get_replace_regex(struct st_command *command)
{
  char *expr= command->first_argument;
  free_replace_regex();
  if (!(glob_replace_regex=init_replace_regex(expr)))
    die("Could not init replace_regex");
  command->last_argument= command->end;
}

void free_replace_regex()
{
  if (glob_replace_regex)
  {
    delete_dynamic(&glob_replace_regex->regex_arr);
    my_free(glob_replace_regex->even_buf,MYF(MY_ALLOW_ZERO_PTR));
    my_free(glob_replace_regex->odd_buf,MYF(MY_ALLOW_ZERO_PTR));
    my_free(glob_replace_regex,MYF(0));
    glob_replace_regex=0;
  }
}



/*
  auxiluary macro used by reg_replace
  makes sure the result buffer has sufficient length
*/
#define SECURE_REG_BUF   if (buf_len < need_buf_len)                    \
  {                                                                     \
    int off= res_p - buf;                                               \
    buf= (char*)my_realloc(buf,need_buf_len,MYF(MY_WME+MY_FAE));        \
    res_p= buf + off;                                                   \
    buf_len= need_buf_len;                                              \
  }                                                                     \
                                                                        \
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
                char *replace, char *in_string, int icase)
{
  string string_to_match(in_string);
  pcrecpp::RE_Options opt;

  if (icase)
    opt.set_caseless(true);

  if (!pcrecpp::RE(pattern, opt).Replace(replace,&string_to_match)){
    return 1;
  }

  const char * new_str= string_to_match.c_str();
  *buf_len_p= strlen(new_str);
  char * new_buf = (char *)malloc(*buf_len_p+1);
  if (new_buf == NULL)
  {
    return 1;
  }
  strcpy(new_buf, new_str);
  buf_p= &new_buf;

  return 0;
}


#ifndef WORD_BIT
#define WORD_BIT (8*sizeof(uint))
#endif

#define SET_MALLOC_HUNC 64
#define LAST_CHAR_CODE 259

typedef struct st_rep_set {
  uint  *bits;        /* Pointer to used sets */
  short next[LAST_CHAR_CODE];    /* Pointer to next sets */
  uint  found_len;      /* Best match to date */
  int  found_offset;
  uint  table_offset;
  uint  size_of_bits;      /* For convinience */
} REP_SET;

typedef struct st_rep_sets {
  uint    count;      /* Number of sets */
  uint    extra;      /* Extra sets in buffer */
  uint    invisible;    /* Sets not chown */
  uint    size_of_bits;
  REP_SET  *set,*set_buffer;
  uint    *bit_buffer;
} REP_SETS;

typedef struct st_found_set {
  uint table_offset;
  int found_offset;
} FOUND_SET;

typedef struct st_follow {
  int chr;
  uint table_offset;
  uint len;
} FOLLOWS;


int init_sets(REP_SETS *sets,uint states);
REP_SET *make_new_set(REP_SETS *sets);
void make_sets_invisible(REP_SETS *sets);
void free_last_set(REP_SETS *sets);
void free_sets(REP_SETS *sets);
void internal_set_bit(REP_SET *set, uint bit);
void internal_clear_bit(REP_SET *set, uint bit);
void or_bits(REP_SET *to,REP_SET *from);
void copy_bits(REP_SET *to,REP_SET *from);
int cmp_bits(REP_SET *set1,REP_SET *set2);
int get_next_bit(REP_SET *set,uint lastpos);
int find_set(REP_SETS *sets,REP_SET *find);
int find_found(FOUND_SET *found_set,uint table_offset,
               int found_offset);
uint start_at_word(char * pos);
uint end_of_word(char * pos);

static uint found_sets=0;


static uint replace_len(char * str)
{
  uint len=0;
  while (*str)
  {
    if (str[0] == '\\' && str[1])
      str++;
    str++;
    len++;
  }
  return len;
}

/* Init a replace structure for further calls */

REPLACE *init_replace(char * *from, char * *to,uint count,
          char * word_end_chars)
{
  static const int SPACE_CHAR= 256;
  static const int START_OF_LINE= 257;
  static const int END_OF_LINE= 258;

  uint i,j,states,set_nr,len,result_len,max_length,found_end,bits_set,bit_nr;
  int used_sets,chr,default_state;
  char used_chars[LAST_CHAR_CODE],is_word_end[256];
  char * pos, *to_pos, **to_array;
  REP_SETS sets;
  REP_SET *set,*start_states,*word_states,*new_set;
  FOLLOWS *follow,*follow_ptr;
  REPLACE *replace;
  FOUND_SET *found_set;
  REPLACE_STRING *rep_str;


  /* Count number of states */
  for (i=result_len=max_length=0 , states=2 ; i < count ; i++)
  {
    len=replace_len(from[i]);
    if (!len)
    {
      errno=EINVAL;
      return(0);
    }
    states+=len+1;
    result_len+=(uint) strlen(to[i])+1;
    if (len > max_length)
      max_length=len;
  }
  bzero((char*) is_word_end,sizeof(is_word_end));
  for (i=0 ; word_end_chars[i] ; i++)
    is_word_end[(uchar) word_end_chars[i]]=1;

  if (init_sets(&sets,states))
    return(0);
  found_sets=0;
  if (!(found_set= (FOUND_SET*) my_malloc(sizeof(FOUND_SET)*max_length*count,
            MYF(MY_WME))))
  {
    free_sets(&sets);
    return(0);
  }
  VOID(make_new_set(&sets));      /* Set starting set */
  make_sets_invisible(&sets);      /* Hide previus sets */
  used_sets=-1;
  word_states=make_new_set(&sets);    /* Start of new word */
  start_states=make_new_set(&sets);    /* This is first state */
  if (!(follow=(FOLLOWS*) my_malloc((states+2)*sizeof(FOLLOWS),MYF(MY_WME))))
  {
    free_sets(&sets);
    my_free(found_set,MYF(0));
    return(0);
  }

  /* Init follow_ptr[] */
  for (i=0, states=1, follow_ptr=follow+1 ; i < count ; i++)
  {
    if (from[i][0] == '\\' && from[i][1] == '^')
    {
      internal_set_bit(start_states,states+1);
      if (!from[i][2])
      {
  start_states->table_offset=i;
  start_states->found_offset=1;
      }
    }
    else if (from[i][0] == '\\' && from[i][1] == '$')
    {
      internal_set_bit(start_states,states);
      internal_set_bit(word_states,states);
      if (!from[i][2] && start_states->table_offset == (uint) ~0)
      {
  start_states->table_offset=i;
  start_states->found_offset=0;
      }
    }
    else
    {
      internal_set_bit(word_states,states);
      if (from[i][0] == '\\' && (from[i][1] == 'b' && from[i][2]))
  internal_set_bit(start_states,states+1);
      else
  internal_set_bit(start_states,states);
    }
    for (pos=from[i], len=0; *pos ; pos++)
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
    follow_ptr->chr = (uchar) *pos;
    break;
  }
      }
      else
  follow_ptr->chr= (uchar) *pos;
      follow_ptr->table_offset=i;
      follow_ptr->len= ++len;
      follow_ptr++;
    }
    follow_ptr->chr=0;
    follow_ptr->table_offset=i;
    follow_ptr->len=len;
    follow_ptr++;
    states+=(uint) len+1;
  }


  for (set_nr=0,pos=0 ; set_nr < sets.count ; set_nr++)
  {
    set=sets.set+set_nr;
    default_state= 0;        /* Start from beginning */

    /* If end of found-string not found or start-set with current set */

    for (i= (uint) ~0; (i=get_next_bit(set,i)) ;)
    {
      if (!follow[i].chr)
      {
  if (! default_state)
    default_state= find_found(found_set,set->table_offset,
            set->found_offset+1);
      }
    }
    copy_bits(sets.set+used_sets,set);    /* Save set for changes */
    if (!default_state)
      or_bits(sets.set+used_sets,sets.set);  /* Can restart from start */

    /* Find all chars that follows current sets */
    bzero((char*) used_chars,sizeof(used_chars));
    for (i= (uint) ~0; (i=get_next_bit(sets.set+used_sets,i)) ;)
    {
      used_chars[follow[i].chr]=1;
      if ((follow[i].chr == SPACE_CHAR && !follow[i+1].chr &&
     follow[i].len > 1) || follow[i].chr == END_OF_LINE)
  used_chars[0]=1;
    }

    /* Mark word_chars used if \b is in state */
    if (used_chars[SPACE_CHAR])
      for (pos= word_end_chars ; *pos ; pos++)
  used_chars[(int) (uchar) *pos] = 1;

    /* Handle other used characters */
    for (chr= 0 ; chr < 256 ; chr++)
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

  for (i= (uint) ~0 ; (i=get_next_bit(sets.set+used_sets,i)) ; )
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
        internal_set_bit(new_set,i+1);    /* To next set */
      else
        internal_set_bit(new_set,i);
    }
  }
  if (found_end)
  {
    new_set->found_len=0;      /* Set for testing if first */
    bits_set=0;
    for (i= (uint) ~0; (i=get_next_bit(new_set,i)) ;)
    {
      if ((follow[i].chr == SPACE_CHAR ||
     follow[i].chr == END_OF_LINE) && ! chr)
        bit_nr=i+1;
      else
        bit_nr=i;
      if (follow[bit_nr-1].len < found_end ||
    (new_set->found_len &&
     (chr == 0 || !follow[bit_nr].chr)))
        internal_clear_bit(new_set,i);
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
      set->next[chr] = find_found(found_set,
          new_set->table_offset,
          new_set->found_offset);
      free_last_set(&sets);
    }
    else
      set->next[chr] = find_set(&sets,new_set);
  }
  else
    set->next[chr] = find_set(&sets,new_set);
      }
    }
  }

  /* Alloc replace structure for the replace-state-machine */

  if ((replace=(REPLACE*) my_malloc(sizeof(REPLACE)*(sets.count)+
            sizeof(REPLACE_STRING)*(found_sets+1)+
            sizeof(char *)*count+result_len,
            MYF(MY_WME | MY_ZEROFILL))))
  {
    rep_str=(REPLACE_STRING*) (replace+sets.count);
    to_array= (char **) (rep_str+found_sets+1);
    to_pos=(char *) (to_array+count);
    for (i=0 ; i < count ; i++)
    {
      to_array[i]=to_pos;
      to_pos=strmov(to_pos,to[i])+1;
    }
    rep_str[0].found=1;
    rep_str[0].replace_string=0;
    for (i=1 ; i <= found_sets ; i++)
    {
      pos=from[found_set[i-1].table_offset];
      rep_str[i].found= !bcmp((const uchar*) pos,
            (const uchar*) "\\^", 3) ? 2 : 1;
      rep_str[i].replace_string=to_array[found_set[i-1].table_offset];
      rep_str[i].to_offset=found_set[i-1].found_offset-start_at_word(pos);
      rep_str[i].from_offset=found_set[i-1].found_offset-replace_len(pos)+
  end_of_word(pos);
    }
    for (i=0 ; i < sets.count ; i++)
    {
      for (j=0 ; j < 256 ; j++)
  if (sets.set[i].next[j] >= 0)
    replace[i].next[j]=replace+sets.set[i].next[j];
  else
    replace[i].next[j]=(REPLACE*) (rep_str+(-sets.set[i].next[j]-1));
    }
  }
  my_free(follow,MYF(0));
  free_sets(&sets);
  my_free(found_set,MYF(0));
  return(replace);
}


int init_sets(REP_SETS *sets,uint states)
{
  bzero((char*) sets,sizeof(*sets));
  sets->size_of_bits=((states+7)/8);
  if (!(sets->set_buffer=(REP_SET*) my_malloc(sizeof(REP_SET)*SET_MALLOC_HUNC,
                MYF(MY_WME))))
    return 1;
  if (!(sets->bit_buffer=(uint*) my_malloc(sizeof(uint)*sets->size_of_bits*
             SET_MALLOC_HUNC,MYF(MY_WME))))
  {
    my_free(sets->set,MYF(0));
    return 1;
  }
  return 0;
}

/* Make help sets invisible for nicer codeing */

void make_sets_invisible(REP_SETS *sets)
{
  sets->invisible=sets->count;
  sets->set+=sets->count;
  sets->count=0;
}

REP_SET *make_new_set(REP_SETS *sets)
{
  uint i,count,*bit_buffer;
  REP_SET *set;
  if (sets->extra)
  {
    sets->extra--;
    set=sets->set+ sets->count++;
    bzero((char*) set->bits,sizeof(uint)*sets->size_of_bits);
    bzero((char*) &set->next[0],sizeof(set->next[0])*LAST_CHAR_CODE);
    set->found_offset=0;
    set->found_len=0;
    set->table_offset= (uint) ~0;
    set->size_of_bits=sets->size_of_bits;
    return set;
  }
  count=sets->count+sets->invisible+SET_MALLOC_HUNC;
  if (!(set=(REP_SET*) my_realloc((uchar*) sets->set_buffer,
                                  sizeof(REP_SET)*count,
          MYF(MY_WME))))
    return 0;
  sets->set_buffer=set;
  sets->set=set+sets->invisible;
  if (!(bit_buffer=(uint*) my_realloc((uchar*) sets->bit_buffer,
              (sizeof(uint)*sets->size_of_bits)*count,
              MYF(MY_WME))))
    return 0;
  sets->bit_buffer=bit_buffer;
  for (i=0 ; i < count ; i++)
  {
    sets->set_buffer[i].bits=bit_buffer;
    bit_buffer+=sets->size_of_bits;
  }
  sets->extra=SET_MALLOC_HUNC;
  return make_new_set(sets);
}

void free_last_set(REP_SETS *sets)
{
  sets->count--;
  sets->extra++;
  return;
}

void free_sets(REP_SETS *sets)
{
  my_free(sets->set_buffer,MYF(0));
  my_free(sets->bit_buffer,MYF(0));
  return;
}

void internal_set_bit(REP_SET *set, uint bit)
{
  set->bits[bit / WORD_BIT] |= 1 << (bit % WORD_BIT);
  return;
}

void internal_clear_bit(REP_SET *set, uint bit)
{
  set->bits[bit / WORD_BIT] &= ~ (1 << (bit % WORD_BIT));
  return;
}


void or_bits(REP_SET *to,REP_SET *from)
{
  register uint i;
  for (i=0 ; i < to->size_of_bits ; i++)
    to->bits[i]|=from->bits[i];
  return;
}

void copy_bits(REP_SET *to,REP_SET *from)
{
  memcpy((uchar*) to->bits,(uchar*) from->bits,
   (size_t) (sizeof(uint) * to->size_of_bits));
}

int cmp_bits(REP_SET *set1,REP_SET *set2)
{
  return bcmp((uchar*) set1->bits,(uchar*) set2->bits,
        sizeof(uint) * set1->size_of_bits);
}


/* Get next set bit from set. */

int get_next_bit(REP_SET *set,uint lastpos)
{
  uint pos,*start,*end,bits;

  start=set->bits+ ((lastpos+1) / WORD_BIT);
  end=set->bits + set->size_of_bits;
  bits=start[0] & ~((1 << ((lastpos+1) % WORD_BIT)) -1);

  while (! bits && ++start < end)
    bits=start[0];
  if (!bits)
    return 0;
  pos=(uint) (start-set->bits)*WORD_BIT;
  while (! (bits & 1))
  {
    bits>>=1;
    pos++;
  }
  return pos;
}

/* find if there is a same set in sets. If there is, use it and
   free given set, else put in given set in sets and return its
   position */

int find_set(REP_SETS *sets,REP_SET *find)
{
  uint i;
  for (i=0 ; i < sets->count-1 ; i++)
  {
    if (!cmp_bits(sets->set+i,find))
    {
      free_last_set(sets);
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

int find_found(FOUND_SET *found_set,uint table_offset, int found_offset)
{
  int i;
  for (i=0 ; (uint) i < found_sets ; i++)
    if (found_set[i].table_offset == table_offset &&
  found_set[i].found_offset == found_offset)
      return -i-2;
  found_set[i].table_offset=table_offset;
  found_set[i].found_offset=found_offset;
  found_sets++;
  return -i-2;        /* return new postion */
}

/* Return 1 if regexp starts with \b or ends with \b*/

uint start_at_word(char * pos)
{
  return (((!bcmp((const uchar*) pos, (const uchar*) "\\b",2) && pos[2]) ||
           !bcmp((const uchar*) pos, (const uchar*) "\\^", 2)) ? 1 : 0);
}

uint end_of_word(char * pos)
{
  char * end=strend(pos);
  return ((end > pos+2 && !bcmp((const uchar*) end-2,
                                (const uchar*) "\\b", 2)) ||
    (end >= pos+2 && !bcmp((const uchar*) end-2,
                                (const uchar*) "\\$",2))) ? 1 : 0;
}

/****************************************************************************
 * Handle replacement of strings
 ****************************************************************************/

#define PC_MALLOC    256  /* Bytes for pointers */
#define PS_MALLOC    512  /* Bytes for data */

int insert_pointer_name(POINTER_ARRAY *pa,char * name)
{
  uint i,length,old_count;
  uchar *new_pos;
  const char **new_array;


  if (! pa->typelib.count)
  {
    if (!(pa->typelib.type_names=(const char **)
    my_malloc(((PC_MALLOC-MALLOC_OVERHEAD)/
         (sizeof(char *)+sizeof(*pa->flag))*
         (sizeof(char *)+sizeof(*pa->flag))),MYF(MY_WME))))
      return(-1);
    if (!(pa->str= (uchar*) my_malloc((uint) (PS_MALLOC-MALLOC_OVERHEAD),
             MYF(MY_WME))))
    {
      my_free((char*) pa->typelib.type_names,MYF(0));
      return (-1);
    }
    pa->max_count=(PC_MALLOC-MALLOC_OVERHEAD)/(sizeof(uchar*)+
                 sizeof(*pa->flag));
    pa->flag= (int7*) (pa->typelib.type_names+pa->max_count);
    pa->length=0;
    pa->max_length=PS_MALLOC-MALLOC_OVERHEAD;
    pa->array_allocs=1;
  }
  length=(uint) strlen(name)+1;
  if (pa->length+length >= pa->max_length)
  {
    if (!(new_pos= (uchar*) my_realloc((uchar*) pa->str,
              (uint) (pa->max_length+PS_MALLOC),
              MYF(MY_WME))))
      return(1);
    if (new_pos != pa->str)
    {
      my_ptrdiff_t diff=PTR_BYTE_DIFF(new_pos,pa->str);
      for (i=0 ; i < pa->typelib.count ; i++)
  pa->typelib.type_names[i]= ADD_TO_PTR(pa->typelib.type_names[i],diff,
                char*);
      pa->str=new_pos;
    }
    pa->max_length+=PS_MALLOC;
  }
  if (pa->typelib.count >= pa->max_count-1)
  {
    int len;
    pa->array_allocs++;
    len=(PC_MALLOC*pa->array_allocs - MALLOC_OVERHEAD);
    if (!(new_array=(const char **) my_realloc((uchar*) pa->typelib.type_names,
                 (uint) len/
                                               (sizeof(uchar*)+sizeof(*pa->flag))*
                                               (sizeof(uchar*)+sizeof(*pa->flag)),
                                               MYF(MY_WME))))
      return(1);
    pa->typelib.type_names=new_array;
    old_count=pa->max_count;
    pa->max_count=len/(sizeof(uchar*) + sizeof(*pa->flag));
    pa->flag= (int7*) (pa->typelib.type_names+pa->max_count);
    memcpy((uchar*) pa->flag,(char *) (pa->typelib.type_names+old_count),
     old_count*sizeof(*pa->flag));
  }
  pa->flag[pa->typelib.count]=0;      /* Reset flag */
  pa->typelib.type_names[pa->typelib.count++]= (char*) pa->str+pa->length;
  pa->typelib.type_names[pa->typelib.count]= NullS;  /* Put end-mark */
  VOID(strmov((char*) pa->str+pa->length,name));
  pa->length+=length;
  return(0);
} /* insert_pointer_name */


/* free pointer array */

void free_pointer_array(POINTER_ARRAY *pa)
{
  if (pa->typelib.count)
  {
    pa->typelib.count=0;
    my_free((char*) pa->typelib.type_names,MYF(0));
    pa->typelib.type_names=0;
    my_free(pa->str,MYF(0));
  }
} /* free_pointer_array */


/* Functions that uses replace and replace_regex */

/* Append the string to ds, with optional replace */
void replace_dynstr_append_mem(DYNAMIC_STRING *ds,
                               const char *val, int len)
{
  if (glob_replace_regex)
  {
    /* Regex replace */
    if (!multi_reg_replace(glob_replace_regex, (char*)val))
    {
      val= glob_replace_regex->buf;
      len= strlen(val);
    }
  }

  if (glob_replace)
  {
    /* Normal replace */
    replace_strings_append(glob_replace, ds, val, len);
  }
  else
    dynstr_append_mem(ds, val, len);
}


/* Append zero-terminated string to ds, with optional replace */
void replace_dynstr_append(DYNAMIC_STRING *ds, const char *val)
{
  replace_dynstr_append_mem(ds, val, strlen(val));
}

/* Append uint to ds, with optional replace */
void replace_dynstr_append_uint(DYNAMIC_STRING *ds, uint val)
{
  char buff[22]; /* This should be enough for any int */
  char *end= int64_t10_to_str(val, buff, 10);
  replace_dynstr_append_mem(ds, buff, end - buff);
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

static int comp_lines(const char **a, const char **b)
{
  return (strcmp(*a,*b));
}

void dynstr_append_sorted(DYNAMIC_STRING* ds, DYNAMIC_STRING *ds_input)
{
  unsigned i;
  char *start= ds_input->str;
  DYNAMIC_ARRAY lines;


  if (!*start)
    return;  /* No input */

  my_init_dynamic_array(&lines, sizeof(const char*), 32, 32);

  /* First line is result header, skip past it */
  while (*start && *start != '\n')
    start++;
  start++; /* Skip past \n */
  dynstr_append_mem(ds, ds_input->str, start - ds_input->str);

  /* Insert line(s) in array */
  while (*start)
  {
    char* line_end= (char*)start;

    /* Find end of line */
    while (*line_end && *line_end != '\n')
      line_end++;
    *line_end= 0;

    /* Insert pointer to the line in array */
    if (insert_dynamic(&lines, (uchar*) &start))
      die("Out of memory inserting lines to sort");

    start= line_end+1;
  }

  /* Sort array */
  qsort(lines.buffer, lines.elements,
        sizeof(char**), (qsort_cmp)comp_lines);

  /* Create new result */
  for (i= 0; i < lines.elements ; i++)
  {
    const char **line= dynamic_element(&lines, i, const char**);
    dynstr_append(ds, *line);
    dynstr_append(ds, "\n");
  }

  delete_dynamic(&lines);
  return;
}
