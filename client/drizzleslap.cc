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


/*
  Drizzle Slap

  A simple program designed to work as if multiple clients querying the database,
  then reporting the timing of each stage.

  Drizzle slap runs three stages:
  1) Create schema,table, and optionally any SP or data you want to beign
  the test with. (single client)
  2) Load test (many clients)
  3) Cleanup (disconnection, drop table if specified, single client)

  Examples:

  Supply your own create and query SQL statements, with 50 clients
  querying (200 selects for each):

  drizzleslap --delimiter=";" \
  --create="CREATE TABLE A (a int);INSERT INTO A VALUES (23)" \
  --query="SELECT * FROM A" --concurrency=50 --iterations=200

  Let the program build the query SQL statement with a table of two int
  columns, three varchar columns, five clients querying (20 times each),
  don't create the table or insert the data (using the previous test's
  schema and data):

  drizzleslap --concurrency=5 --iterations=20 \
  --number-int-cols=2 --number-char-cols=3 \
  --auto-generate-sql

  Tell the program to load the create, insert and query SQL statements from
  the specified files, where the create.sql file has multiple table creation
  statements delimited by ';' and multiple insert statements delimited by ';'.
  The --query file will have multiple queries delimited by ';', run all the
  load statements, and then run all the queries in the query file
  with five clients (five times each):

  drizzleslap --concurrency=5 \
  --iterations=5 --query=query.sql --create=create.sql \
  --delimiter=";"

  TODO:
  Add language for better tests
  String length for files and those put on the command line are not
  setup to handle binary data.
  More stats
  Break up tests and run them on multiple hosts at once.
  Allow output to be fed into a database directly.

*/

#define SLAP_VERSION "1.5"

#define HUGE_STRING_LENGTH 8196
#define RAND_STRING_SIZE 126
#define DEFAULT_BLOB_SIZE 1024

#include "client_priv.h"
#include <signal.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <fcntl.h>
#include <math.h>
#include <ctype.h>
#include <cassert>
#include <cstdlib>
#include <string>

#include <pthread.h>

/* Added this for string translation. */
#include <drizzled/gettext.h>

using namespace std;
using namespace drizzled;

#ifdef HAVE_SMEM
static char *shared_memory_base_name=0;
#endif

/* Global Thread counter */
uint32_t thread_counter;
pthread_mutex_t counter_mutex;
pthread_cond_t count_threshhold;
uint32_t master_wakeup;
pthread_mutex_t sleeper_mutex;
pthread_cond_t sleep_threshhold;

/* Global Thread timer */
static bool timer_alarm= false;
pthread_mutex_t timer_alarm_mutex;
pthread_cond_t timer_alarm_threshold;

static char **defaults_argv;

char **primary_keys;
/* This gets passed to malloc, so lets set it to an arch-dependant size */
size_t primary_keys_number_of;

static char *host= NULL, *opt_password= NULL, *user= NULL,
  *user_supplied_query= NULL,
  *user_supplied_pre_statements= NULL,
  *user_supplied_post_statements= NULL,
  *default_engine= NULL,
  *pre_system= NULL,
  *post_system= NULL;

const char *delimiter= "\n";

const char *create_schema_string= "drizzleslap";

static bool opt_mysql= false;
static bool opt_preserve= true;
static bool opt_only_print= false;
static bool opt_burnin= false;
static bool opt_ignore_sql_errors= false;
static bool tty_password= false,
  opt_silent= false,
  auto_generate_sql_autoincrement= false,
  auto_generate_sql_guid_primary= false,
  auto_generate_sql= false;
const char *opt_auto_generate_sql_type= "mixed";

static int verbose, delimiter_length;
static uint32_t commit_rate;
static uint32_t detach_rate;
static uint32_t opt_timer_length;
static uint32_t opt_delayed_start;
const char *num_int_cols_opt;
const char *num_char_cols_opt;
const char *num_blob_cols_opt;
const char *opt_label;
static unsigned int opt_set_random_seed;

const char *auto_generate_selected_columns_opt;

/* Yes, we do set defaults here */
static unsigned int num_int_cols= 1;
static unsigned int num_char_cols= 1;
static unsigned int num_blob_cols= 0;
static unsigned int num_blob_cols_size;
static unsigned int num_blob_cols_size_min;
static unsigned int num_int_cols_index= 0;
static unsigned int num_char_cols_index= 0;
static unsigned int iterations;
static uint64_t actual_queries= 0;
static uint64_t auto_actual_queries;
static uint64_t auto_generate_sql_unique_write_number;
static uint64_t auto_generate_sql_unique_query_number;
static unsigned int auto_generate_sql_secondary_indexes;
static uint64_t num_of_query;
static uint64_t auto_generate_sql_number;
const char *concurrency_str= NULL;
static char *create_string;
uint32_t *concurrency;

const char *default_dbug_option= "d:t:o,/tmp/drizzleslap.trace";
const char *opt_csv_str;
int csv_file;

static int get_options(int *argc,char ***argv);
static uint32_t opt_drizzle_port= 0;

static const char *load_default_groups[]= { "drizzleslap","client",0 };

/* Types */
typedef enum {
  SELECT_TYPE= 0,
  UPDATE_TYPE= 1,
  INSERT_TYPE= 2,
  UPDATE_TYPE_REQUIRES_PREFIX= 3,
  CREATE_TABLE_TYPE= 4,
  SELECT_TYPE_REQUIRES_PREFIX= 5,
  DELETE_TYPE_REQUIRES_PREFIX= 6
} slap_query_type;

typedef struct statement statement;

struct statement {
  char *string;
  size_t length;
  slap_query_type type;
  char *option;
  size_t option_length;
  statement *next;
};

typedef struct option_string option_string;

struct option_string {
  char *string;
  size_t length;
  char *option;
  size_t option_length;
  option_string *next;
};

typedef struct stats stats;

struct stats {
  long int timing;
  uint32_t users;
  uint32_t real_users;
  uint64_t rows;
  long int create_timing;
  uint64_t create_count;
};

typedef struct thread_context thread_context;

struct thread_context {
  statement *stmt;
  uint64_t limit;
};

typedef struct conclusions conclusions;

struct conclusions {
  char *engine;
  long int avg_timing;
  long int max_timing;
  long int min_timing;
  uint32_t users;
  uint32_t real_users;
  uint64_t avg_rows;
  long int sum_of_time;
  long int std_dev;
  /* These are just for create time stats */
  long int create_avg_timing;
  long int create_max_timing;
  long int create_min_timing;
  uint64_t create_count;
  /* The following are not used yet */
  uint64_t max_rows;
  uint64_t min_rows;
};

static option_string *engine_options= NULL;
static option_string *query_options= NULL;
static statement *pre_statements= NULL;
static statement *post_statements= NULL;
static statement *create_statements= NULL;

static statement **query_statements= NULL;
static unsigned int query_statements_count;


/* Prototypes */
void print_conclusions(conclusions *con);
void print_conclusions_csv(conclusions *con);
void generate_stats(conclusions *con, option_string *eng, stats *sptr);
uint32_t parse_comma(const char *string, uint32_t **range);
uint32_t parse_delimiter(const char *script, statement **stmt, char delm);
uint32_t parse_option(const char *origin, option_string **stmt, char delm);
static int drop_schema(drizzle_con_st *con, const char *db);
uint32_t get_random_string(char *buf, size_t size);
static statement *build_table_string(void);
static statement *build_insert_string(void);
static statement *build_update_string(void);
static statement * build_select_string(bool key);
static int generate_primary_key_list(drizzle_con_st *con, option_string *engine_stmt);
static int drop_primary_key_list(void);
static int create_schema(drizzle_con_st *con, const char *db, statement *stmt,
                         option_string *engine_stmt, stats *sptr);
static int run_scheduler(stats *sptr, statement **stmts, uint32_t concur,
                         uint64_t limit);
extern "C" pthread_handler_t run_task(void *p);
extern "C" pthread_handler_t timer_thread(void *p);
void statement_cleanup(statement *stmt);
void option_cleanup(option_string *stmt);
void concurrency_loop(drizzle_con_st *con, uint32_t current, option_string *eptr);
static int run_statements(drizzle_con_st *con, statement *stmt);
void slap_connect(drizzle_con_st *con, bool connect_to_schema);
void slap_close(drizzle_con_st *con);
static int run_query(drizzle_con_st *con, drizzle_result_st *result, const char *query, int len);
void standard_deviation (conclusions *con, stats *sptr);

static const char ALPHANUMERICS[]=
"0123456789ABCDEFGHIJKLMNOPQRSTWXYZabcdefghijklmnopqrstuvwxyz";

#define ALPHANUMERICS_SIZE (sizeof(ALPHANUMERICS)-1)


static long int timedif(struct timeval a, struct timeval b)
{
  register int us, s;

  us = a.tv_usec - b.tv_usec;
  us /= 1000;
  s = a.tv_sec - b.tv_sec;
  s *= 1000;
  return s + us;
}

int main(int argc, char **argv)
{
  drizzle_con_st con;
  option_string *eptr;
  unsigned int x;

  internal::my_init();

  MY_INIT(argv[0]);

  internal::load_defaults("drizzle",load_default_groups,&argc,&argv);
  defaults_argv=argv;
  if (get_options(&argc,&argv))
  {
    internal::free_defaults(defaults_argv);
    internal::my_end();
    exit(1);
  }

  /* Seed the random number generator if we will be using it. */
  if (auto_generate_sql)
  {
    if (opt_set_random_seed == 0)
      opt_set_random_seed= (unsigned int)time(NULL);
    srandom(opt_set_random_seed);
  }

  /* globals? Yes, so we only have to run strlen once */
  delimiter_length= strlen(delimiter);

  if (argc > 2)
  {
    fprintf(stderr,"%s: Too many arguments\n",internal::my_progname);
    internal::free_defaults(defaults_argv);
    internal::my_end();
    exit(1);
  }

  slap_connect(&con, false);

  pthread_mutex_init(&counter_mutex, NULL);
  pthread_cond_init(&count_threshhold, NULL);
  pthread_mutex_init(&sleeper_mutex, NULL);
  pthread_cond_init(&sleep_threshhold, NULL);
  pthread_mutex_init(&timer_alarm_mutex, NULL);
  pthread_cond_init(&timer_alarm_threshold, NULL);


  /* Main iterations loop */
burnin:
  eptr= engine_options;
  do
  {
    /* For the final stage we run whatever queries we were asked to run */
    uint32_t *current;

    if (verbose >= 2)
      printf("Starting Concurrency Test\n");

    if (*concurrency)
    {
      for (current= concurrency; current && *current; current++)
        concurrency_loop(&con, *current, eptr);
    }
    else
    {
      uint32_t infinite= 1;
      do {
        concurrency_loop(&con, infinite, eptr);
      }
      while (infinite++);
    }

    if (!opt_preserve)
      drop_schema(&con, create_schema_string);

  } while (eptr ? (eptr= eptr->next) : 0);

  if (opt_burnin)
    goto burnin;

  pthread_mutex_destroy(&counter_mutex);
  pthread_cond_destroy(&count_threshhold);
  pthread_mutex_destroy(&sleeper_mutex);
  pthread_cond_destroy(&sleep_threshhold);
  pthread_mutex_destroy(&timer_alarm_mutex);
  pthread_cond_destroy(&timer_alarm_threshold);

  slap_close(&con);

  /* now free all the strings we created */
  if (opt_password)
    free(opt_password);

  free(concurrency);

  statement_cleanup(create_statements);
  for (x= 0; x < query_statements_count; x++)
    statement_cleanup(query_statements[x]);
  free(query_statements);
  statement_cleanup(pre_statements);
  statement_cleanup(post_statements);
  option_cleanup(engine_options);
  option_cleanup(query_options);

#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    free(shared_memory_base_name);
#endif
  internal::free_defaults(defaults_argv);
  internal::my_end();

  return 0;
}

void concurrency_loop(drizzle_con_st *con, uint32_t current, option_string *eptr)
{
  unsigned int x;
  stats *head_sptr;
  stats *sptr;
  conclusions conclusion;
  uint64_t client_limit;

  head_sptr= (stats *)malloc(sizeof(stats) * iterations);
  if (head_sptr == NULL)
  {
    fprintf(stderr,"Error allocating memory in concurrency_loop\n");
    exit(1);
  }
  memset(head_sptr, 0, sizeof(stats) * iterations);

  memset(&conclusion, 0, sizeof(conclusions));

  if (auto_actual_queries)
    client_limit= auto_actual_queries;
  else if (num_of_query)
    client_limit=  num_of_query / current;
  else
    client_limit= actual_queries;

  for (x= 0, sptr= head_sptr; x < iterations; x++, sptr++)
  {
    /*
      We might not want to load any data, such as when we are calling
      a stored_procedure that doesn't use data, or we know we already have
      data in the table.
    */
    if (opt_preserve == false)
      drop_schema(con, create_schema_string);

    /* First we create */
    if (create_statements)
      create_schema(con, create_schema_string, create_statements, eptr, sptr);

    /*
      If we generated GUID we need to build a list of them from creation that
      we can later use.
    */
    if (verbose >= 2)
      printf("Generating primary key list\n");
    if (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary)
      generate_primary_key_list(con, eptr);

    if (commit_rate)
      run_query(con, NULL, "SET AUTOCOMMIT=0", strlen("SET AUTOCOMMIT=0"));

    if (pre_system)
    {
      int ret= system(pre_system);
      assert(ret != -1);
    }
       

    /*
      Pre statements are always run after all other logic so they can
      correct/adjust any item that they want.
    */
    if (pre_statements)
      run_statements(con, pre_statements);

    run_scheduler(sptr, query_statements, current, client_limit);

    if (post_statements)
      run_statements(con, post_statements);

    if (post_system)
    {
      int ret=  system(post_system);
      assert(ret !=-1);
    }

    /* We are finished with this run */
    if (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary)
      drop_primary_key_list();
  }

  if (verbose >= 2)
    printf("Generating stats\n");

  generate_stats(&conclusion, eptr, head_sptr);

  if (!opt_silent)
    print_conclusions(&conclusion);
  if (opt_csv_str)
    print_conclusions_csv(&conclusion);

  free(head_sptr);

}


static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"auto-generate-sql-select-columns", OPT_SLAP_AUTO_GENERATE_SELECT_COLUMNS,
   "Provide a string to use for the select fields used in auto tests.",
   (char**) &auto_generate_selected_columns_opt,
   (char**) &auto_generate_selected_columns_opt,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-generate-sql", 'a',
   "Generate SQL where not supplied by file or command line.",
   (char**) &auto_generate_sql, (char**) &auto_generate_sql,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-generate-sql-add-autoincrement", OPT_SLAP_AUTO_GENERATE_ADD_AUTO,
   "Add an AUTO_INCREMENT column to auto-generated tables.",
   (char**) &auto_generate_sql_autoincrement,
   (char**) &auto_generate_sql_autoincrement,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-generate-sql-execute-number", OPT_SLAP_AUTO_GENERATE_EXECUTE_QUERIES,
   "Set this number to generate a set number of queries to run.",
   (char**) &auto_actual_queries, (char**) &auto_actual_queries,
   0, GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-generate-sql-guid-primary", OPT_SLAP_AUTO_GENERATE_GUID_PRIMARY,
   "Add GUID based primary keys to auto-generated tables.",
   (char**) &auto_generate_sql_guid_primary,
   (char**) &auto_generate_sql_guid_primary,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-generate-sql-load-type", OPT_SLAP_AUTO_GENERATE_SQL_LOAD_TYPE,
   "Specify test load type: mixed, update, write, key, or read; default is mixed.",
   (char**) &opt_auto_generate_sql_type, (char**) &opt_auto_generate_sql_type,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-generate-sql-secondary-indexes",
   OPT_SLAP_AUTO_GENERATE_SECONDARY_INDEXES,
   "Number of secondary indexes to add to auto-generated tables.",
   (char**) &auto_generate_sql_secondary_indexes,
   (char**) &auto_generate_sql_secondary_indexes, 0,
   GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-generate-sql-unique-query-number",
   OPT_SLAP_AUTO_GENERATE_UNIQUE_QUERY_NUM,
   "Number of unique queries to generate for automatic tests.",
   (char**) &auto_generate_sql_unique_query_number,
   (char**) &auto_generate_sql_unique_query_number,
   0, GET_ULL, REQUIRED_ARG, 10, 0, 0, 0, 0, 0},
  {"auto-generate-sql-unique-write-number",
   OPT_SLAP_AUTO_GENERATE_UNIQUE_WRITE_NUM,
   "Number of unique queries to generate for auto-generate-sql-write-number.",
   (char**) &auto_generate_sql_unique_write_number,
   (char**) &auto_generate_sql_unique_write_number,
   0, GET_ULL, REQUIRED_ARG, 10, 0, 0, 0, 0, 0},
  {"auto-generate-sql-write-number", OPT_SLAP_AUTO_GENERATE_WRITE_NUM,
   "Number of row inserts to perform for each thread (default is 100).",
   (char**) &auto_generate_sql_number, (char**) &auto_generate_sql_number,
   0, GET_ULL, REQUIRED_ARG, 100, 0, 0, 0, 0, 0},
  {"burnin", OPT_SLAP_BURNIN, "Run full test case in infinite loop.",
   (char**) &opt_burnin, (char**) &opt_burnin, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"ignore-sql-errors", OPT_SLAP_IGNORE_SQL_ERRORS,
   "Ignore SQL erros in query run.",
   (char**) &opt_ignore_sql_errors,
   (char**) &opt_ignore_sql_errors,
   0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"commit", OPT_SLAP_COMMIT, "Commit records every X number of statements.",
   (char**) &commit_rate, (char**) &commit_rate, 0, GET_UINT, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"concurrency", 'c', "Number of clients to simulate for query to run.",
   (char**) &concurrency_str, (char**) &concurrency_str, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"create", OPT_SLAP_CREATE_STRING, "File or string to use create tables.",
   (char**) &create_string, (char**) &create_string, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"create-schema", OPT_CREATE_SLAP_SCHEMA, "Schema to run tests in.",
   (char**) &create_schema_string, (char**) &create_schema_string, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"csv", OPT_SLAP_CSV,
   "Generate CSV output to named file or to stdout if no file is named.",
   (char**) &opt_csv_str, (char**) &opt_csv_str, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"delayed-start", OPT_SLAP_DELAYED_START,
   "Delay the startup of threads by a random number of microsends (the maximum of the delay)",
   (char**) &opt_delayed_start, (char**) &opt_delayed_start, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"delimiter", 'F',
   "Delimiter to use in SQL statements supplied in file or command line.",
   (char**) &delimiter, (char**) &delimiter, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"detach", OPT_SLAP_DETACH,
   "Detach (close and reopen) connections after X number of requests.",
   (char**) &detach_rate, (char**) &detach_rate, 0, GET_UINT, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"engine", 'e', "Storage engine to use for creating the table.",
   (char**) &default_engine, (char**) &default_engine, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", (char**) &host, (char**) &host, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"iterations", 'i', "Number of times to run the tests.", (char**) &iterations,
   (char**) &iterations, 0, GET_UINT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
  {"label", OPT_SLAP_LABEL, "Label to use for print and csv output.",
   (char**) &opt_label, (char**) &opt_label, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"mysql", 'm', N_("Use MySQL Protocol."),
   (char**) &opt_mysql, (char**) &opt_mysql, 0, GET_BOOL, NO_ARG, 1, 0, 0,
   0, 0, 0},
  {"number-blob-cols", OPT_SLAP_BLOB_COL,
   "Number of BLOB columns to create table with if specifying --auto-generate-sql. Example --number-blob-cols=3:1024/2048 would give you 3 blobs with a random size between 1024 and 2048. ",
   (char**) &num_blob_cols_opt, (char**) &num_blob_cols_opt, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"number-char-cols", 'x',
   "Number of VARCHAR columns to create in table if specifying --auto-generate-sql.",
   (char**) &num_char_cols_opt, (char**) &num_char_cols_opt, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"number-int-cols", 'y',
   "Number of INT columns to create in table if specifying --auto-generate-sql.",
   (char**) &num_int_cols_opt, (char**) &num_int_cols_opt, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"number-of-queries", OPT_DRIZZLE_NUMBER_OF_QUERY,
   "Limit each client to this number of queries (this is not exact).",
   (char**) &num_of_query, (char**) &num_of_query, 0,
   GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"only-print", OPT_DRIZZLE_ONLY_PRINT,
   "This causes drizzleslap to not connect to the databases, but instead print "
   "out what it would have done instead.",
   (char**) &opt_only_print, (char**) &opt_only_print, 0, GET_BOOL,  NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"password", 'P',
   "Password to use when connecting to server. If password is not given it's "
   "asked from the tty.", 0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'p', "Port number to use for connection.",
   0, 0, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"post-query", OPT_SLAP_POST_QUERY,
   "Query to run or file containing query to execute after tests have completed.",
   (char**) &user_supplied_post_statements,
   (char**) &user_supplied_post_statements,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"post-system", OPT_SLAP_POST_SYSTEM,
   "system() string to execute after tests have completed.",
   (char**) &post_system,
   (char**) &post_system,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"pre-query", OPT_SLAP_PRE_QUERY,
   "Query to run or file containing query to execute before running tests.",
   (char**) &user_supplied_pre_statements,
   (char**) &user_supplied_pre_statements,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"pre-system", OPT_SLAP_PRE_SYSTEM,
   "system() string to execute before running tests.",
   (char**) &pre_system,
   (char**) &pre_system,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_DRIZZLE_PROTOCOL,
   "The protocol of connection (tcp,socket,pipe,memory).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"query", 'q', "Query to run or file containing query to run.",
   (char**) &user_supplied_query, (char**) &user_supplied_query,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"set-random-seed", OPT_SLAP_SET_RANDOM_SEED,
   "Seed for random number generator (srandom(3))",
   (char**)&opt_set_random_seed,
   (char**)&opt_set_random_seed,0,
   GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Run program in silent mode - no output.",
   (char**) &opt_silent, (char**) &opt_silent, 0, GET_BOOL,  NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"timer-length", OPT_SLAP_TIMER_LENGTH,
   "Require drizzleslap to run each specific test a certain amount of time in seconds.",
   (char**) &opt_timer_length, (char**) &opt_timer_length, 0, GET_UINT,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login if not current user.", (char**) &user,
   (char**) &user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v',
   "More verbose output; you can use this multiple times to get even more "
   "verbose output.", (char**) &verbose, (char**) &verbose, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s-%s (%s)\n",internal::my_progname, SLAP_VERSION,
         drizzle_version(),HOST_VENDOR,HOST_OS,HOST_CPU);
}


static void usage(void)
{
  print_version();
  puts("Copyright (C) 2008 Sun Microsystems");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\
       \nand you are welcome to modify and redistribute it under the GPL \
       license\n");
  puts("Run a query multiple times against the server\n");
  printf("Usage: %s [OPTIONS]\n",internal::my_progname);
  internal::print_defaults("drizzle",load_default_groups);
  my_print_help(my_long_options);
}

static int get_one_option(int optid, const struct my_option *, char *argument)
{
  char *endchar= NULL;
  uint64_t temp_drizzle_port= 0;

  switch(optid) {
  case 'v':
    verbose++;
    break;
  case 'p':
    temp_drizzle_port= (uint64_t) strtoul(argument, &endchar, 10);
    /* if there is an alpha character this is not a valid port */
    if (strlen(endchar) != 0)
    {
      fprintf(stderr, _("Non-integer value supplied for port.  If you are trying to enter a password please use --password instead.\n"));
      return EXIT_ARGUMENT_INVALID;
    }
    /* If the port number is > 65535 it is not a valid port
       This also helps with potential data loss casting unsigned long to a
       uint32_t. */
    if ((temp_drizzle_port == 0) || (temp_drizzle_port > 65535))
    {
      fprintf(stderr, _("Value supplied for port is not valid.\n"));
      return EXIT_ARGUMENT_INVALID;
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
        return EXIT_OUT_OF_MEMORY;
      }
      while (*argument)
      {
        /* Overwriting password with 'x' */
        *argument++= 'x';
      }
      if (*start)
      {
        /* Cut length of argument */
        start[1]= 0;
      }
      tty_password= 0;
    }
    else
      tty_password= 1;
    break;
  case 'V':
    print_version();
    exit(0);
  case '?':
  case 'I':          /* Info */
    usage();
    exit(0);
  }
  return(0);
}


uint
get_random_string(char *buf, size_t size)
{
  char *buf_ptr= buf;
  size_t x;

  for (x= size; x > 0; x--)
    *buf_ptr++= ALPHANUMERICS[random() % ALPHANUMERICS_SIZE];
  return(buf_ptr - buf);
}


/*
  build_table_string

  This function builds a create table query if the user opts to not supply
  a file or string containing a create table statement
*/
static statement *
build_table_string(void)
{
  char       buf[HUGE_STRING_LENGTH];
  unsigned int        col_count;
  statement *ptr;
  string table_string;

  table_string.reserve(HUGE_STRING_LENGTH);

  table_string= "CREATE TABLE `t1` (";

  if (auto_generate_sql_autoincrement)
  {
    table_string.append("id serial");

    if (num_int_cols || num_char_cols)
      table_string.append(",");
  }

  if (auto_generate_sql_guid_primary)
  {
    table_string.append("id varchar(128) primary key");

    if (num_int_cols || num_char_cols || auto_generate_sql_guid_primary)
      table_string.append(",");
  }

  if (auto_generate_sql_secondary_indexes)
  {
    unsigned int count;

    for (count= 0; count < auto_generate_sql_secondary_indexes; count++)
    {
      if (count) /* Except for the first pass we add a comma */
        table_string.append(",");

      if (snprintf(buf, HUGE_STRING_LENGTH, "id%d varchar(32) unique key", count)
          > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in create table\n");
        exit(1);
      }
      table_string.append(buf);
    }

    if (num_int_cols || num_char_cols)
      table_string.append(",");
  }

  if (num_int_cols)
    for (col_count= 1; col_count <= num_int_cols; col_count++)
    {
      if (num_int_cols_index)
      {
        if (snprintf(buf, HUGE_STRING_LENGTH, "intcol%d INT, INDEX(intcol%d)",
                     col_count, col_count) > HUGE_STRING_LENGTH)
        {
          fprintf(stderr, "Memory Allocation error in create table\n");
          exit(1);
        }
      }
      else
      {
        if (snprintf(buf, HUGE_STRING_LENGTH, "intcol%d INT ", col_count)
            > HUGE_STRING_LENGTH)
        {
          fprintf(stderr, "Memory Allocation error in create table\n");
          exit(1);
        }
      }
      table_string.append(buf);

      if (col_count < num_int_cols || num_char_cols > 0)
        table_string.append(",");
    }

  if (num_char_cols)
    for (col_count= 1; col_count <= num_char_cols; col_count++)
    {
      if (num_char_cols_index)
      {
        if (snprintf(buf, HUGE_STRING_LENGTH,
                     "charcol%d VARCHAR(128), INDEX(charcol%d) ",
                     col_count, col_count) > HUGE_STRING_LENGTH)
        {
          fprintf(stderr, "Memory Allocation error in creating table\n");
          exit(1);
        }
      }
      else
      {
        if (snprintf(buf, HUGE_STRING_LENGTH, "charcol%d VARCHAR(128)",
                     col_count) > HUGE_STRING_LENGTH)
        {
          fprintf(stderr, "Memory Allocation error in creating table\n");
          exit(1);
        }
      }
      table_string.append(buf);

      if (col_count < num_char_cols || num_blob_cols > 0)
        table_string.append(",");
    }

  if (num_blob_cols)
    for (col_count= 1; col_count <= num_blob_cols; col_count++)
    {
      if (snprintf(buf, HUGE_STRING_LENGTH, "blobcol%d blob",
                   col_count) > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in creating table\n");
        exit(1);
      }
      table_string.append(buf);

      if (col_count < num_blob_cols)
        table_string.append(",");
    }

  table_string.append(")");
  ptr= (statement *)malloc(sizeof(statement));
  if (ptr == NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating table\n");
    exit(1);
  }
  memset(ptr, 0, sizeof(statement));
  ptr->string = (char *)malloc(table_string.length()+1);
  if (ptr->string == NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating table\n");
    exit(1);
  }
  memset(ptr->string, 0, table_string.length()+1);
  ptr->length= table_string.length()+1;
  ptr->type= CREATE_TABLE_TYPE;
  strcpy(ptr->string, table_string.c_str());
  return(ptr);
}

/*
  build_update_string()

  This function builds insert statements when the user opts to not supply
  an insert file or string containing insert data
*/
static statement *
build_update_string(void)
{
  char       buf[HUGE_STRING_LENGTH];
  unsigned int        col_count;
  statement *ptr;
  string update_string;

  update_string.reserve(HUGE_STRING_LENGTH);

  update_string= "UPDATE t1 SET ";

  if (num_int_cols)
    for (col_count= 1; col_count <= num_int_cols; col_count++)
    {
      if (snprintf(buf, HUGE_STRING_LENGTH, "intcol%d = %ld", col_count,
                   random()) > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in creating update\n");
        exit(1);
      }
      update_string.append(buf);

      if (col_count < num_int_cols || num_char_cols > 0)
        update_string.append(",", 1);
    }

  if (num_char_cols)
    for (col_count= 1; col_count <= num_char_cols; col_count++)
    {
      char rand_buffer[RAND_STRING_SIZE];
      int buf_len= get_random_string(rand_buffer, RAND_STRING_SIZE);

      if (snprintf(buf, HUGE_STRING_LENGTH, "charcol%d = '%.*s'", col_count,
                   buf_len, rand_buffer)
          > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in creating update\n");
        exit(1);
      }
      update_string.append(buf);

      if (col_count < num_char_cols)
        update_string.append(",", 1);
    }

  if (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary)
    update_string.append(" WHERE id = ");


  ptr= (statement *)malloc(sizeof(statement));
  if (ptr == NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating update\n");
    exit(1);
  }
  memset(ptr, 0, sizeof(statement));

  ptr->length= update_string.length()+1;
  ptr->string= (char *)malloc(ptr->length);
  if (ptr->string == NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating update\n");
    exit(1);
  }
  memset(ptr->string, 0, ptr->length);
  if (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary)
    ptr->type= UPDATE_TYPE_REQUIRES_PREFIX ;
  else
    ptr->type= UPDATE_TYPE;
  strncpy(ptr->string, update_string.c_str(), ptr->length);
  return(ptr);
}


/*
  build_insert_string()

  This function builds insert statements when the user opts to not supply
  an insert file or string containing insert data
*/
static statement *
build_insert_string(void)
{
  char       buf[HUGE_STRING_LENGTH];
  unsigned int        col_count;
  statement *ptr;
  string insert_string;

  insert_string.reserve(HUGE_STRING_LENGTH);

  insert_string= "INSERT INTO t1 VALUES (";

  if (auto_generate_sql_autoincrement)
  {
    insert_string.append("NULL");

    if (num_int_cols || num_char_cols)
      insert_string.append(",");
  }

  if (auto_generate_sql_guid_primary)
  {
    insert_string.append("uuid()");

    if (num_int_cols || num_char_cols)
      insert_string.append(",");
  }

  if (auto_generate_sql_secondary_indexes)
  {
    unsigned int count;

    for (count= 0; count < auto_generate_sql_secondary_indexes; count++)
    {
      if (count) /* Except for the first pass we add a comma */
        insert_string.append(",");

      insert_string.append("uuid()");
    }

    if (num_int_cols || num_char_cols)
      insert_string.append(",");
  }

  if (num_int_cols)
    for (col_count= 1; col_count <= num_int_cols; col_count++)
    {
      if (snprintf(buf, HUGE_STRING_LENGTH, "%ld", random()) > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in creating insert\n");
        exit(1);
      }
      insert_string.append(buf);

      if (col_count < num_int_cols || num_char_cols > 0)
        insert_string.append(",");
    }

  if (num_char_cols)
    for (col_count= 1; col_count <= num_char_cols; col_count++)
    {
      int buf_len= get_random_string(buf, RAND_STRING_SIZE);
      insert_string.append("'", 1);
      insert_string.append(buf, buf_len);
      insert_string.append("'", 1);

      if (col_count < num_char_cols || num_blob_cols > 0)
        insert_string.append(",", 1);
    }

  if (num_blob_cols)
  {
    char *blob_ptr;

    if (num_blob_cols_size > HUGE_STRING_LENGTH)
    {
      blob_ptr= (char *)malloc(sizeof(char)*num_blob_cols_size);
      if (!blob_ptr)
      {
        fprintf(stderr, "Memory Allocation error in creating select\n");
        exit(1);
      }
      memset(blob_ptr, 0, sizeof(char)*num_blob_cols_size);
    }
    else
    {
      blob_ptr= buf;
    }

    for (col_count= 1; col_count <= num_blob_cols; col_count++)
    {
      unsigned int buf_len;
      unsigned int size;
      unsigned int difference= num_blob_cols_size - num_blob_cols_size_min;

      size= difference ? (num_blob_cols_size_min + (random() % difference)) :
        num_blob_cols_size;

      buf_len= get_random_string(blob_ptr, size);

      insert_string.append("'", 1);
      insert_string.append(blob_ptr, buf_len);
      insert_string.append("'", 1);

      if (col_count < num_blob_cols)
        insert_string.append(",", 1);
    }

    if (num_blob_cols_size > HUGE_STRING_LENGTH)
      free(blob_ptr);
  }

  insert_string.append(")", 1);

  if (!(ptr= (statement *)malloc(sizeof(statement))))
  {
    fprintf(stderr, "Memory Allocation error in creating select\n");
    exit(1);
  }
  memset(ptr, 0, sizeof(statement));
  ptr->length= insert_string.length()+1;
  if (!(ptr->string= (char *)malloc(ptr->length)))
  {
    fprintf(stderr, "Memory Allocation error in creating select\n");
    exit(1);
  }
  memset(ptr->string, 0, ptr->length);
  ptr->type= INSERT_TYPE;
  strcpy(ptr->string, insert_string.c_str());
  return(ptr);
}


/*
  build_select_string()

  This function builds a query if the user opts to not supply a query
  statement or file containing a query statement
*/
static statement *
build_select_string(bool key)
{
  char       buf[HUGE_STRING_LENGTH];
  unsigned int        col_count;
  statement *ptr;
  string query_string;

  query_string.reserve(HUGE_STRING_LENGTH);

  query_string.append("SELECT ", 7);
  if (auto_generate_selected_columns_opt)
  {
    query_string.append(auto_generate_selected_columns_opt);
  }
  else
  {
    for (col_count= 1; col_count <= num_int_cols; col_count++)
    {
      if (snprintf(buf, HUGE_STRING_LENGTH, "intcol%d", col_count)
          > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in creating select\n");
        exit(1);
      }
      query_string.append(buf);

      if (col_count < num_int_cols || num_char_cols > 0)
        query_string.append(",", 1);

    }
    for (col_count= 1; col_count <= num_char_cols; col_count++)
    {
      if (snprintf(buf, HUGE_STRING_LENGTH, "charcol%d", col_count)
          > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in creating select\n");
        exit(1);
      }
      query_string.append(buf);

      if (col_count < num_char_cols || num_blob_cols > 0)
        query_string.append(",", 1);

    }
    for (col_count= 1; col_count <= num_blob_cols; col_count++)
    {
      if (snprintf(buf, HUGE_STRING_LENGTH, "blobcol%d", col_count)
          > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in creating select\n");
        exit(1);
      }
      query_string.append(buf);

      if (col_count < num_blob_cols)
        query_string.append(",", 1);
    }
  }
  query_string.append(" FROM t1");

  if ((key) &&
      (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary))
    query_string.append(" WHERE id = ");

  ptr= (statement *)malloc(sizeof(statement));
  if (ptr == NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating select\n");
    exit(1);
  }
  memset(ptr, 0, sizeof(statement));
  ptr->length= query_string.length()+1;
  ptr->string= (char *)malloc(ptr->length);
  if (ptr->string == NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating select\n");
    exit(1);
  }
  memset(ptr->string, 0, ptr->length);
  if ((key) &&
      (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary))
    ptr->type= SELECT_TYPE_REQUIRES_PREFIX;
  else
    ptr->type= SELECT_TYPE;
  strcpy(ptr->string, query_string.c_str());
  return(ptr);
}

static int
get_options(int *argc,char ***argv)
{
  int ho_error;
  char *tmp_string;
  struct stat sbuf;
  option_string *sql_type;
  unsigned int sql_type_count= 0;
  ssize_t bytes_read= 0;


  if ((ho_error= handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (!user)
    user= (char *)"root";

  /* If something is created we clean it up, otherwise we leave schemas alone */
  if (create_string || auto_generate_sql)
    opt_preserve= false;

  if (auto_generate_sql && (create_string || user_supplied_query))
  {
    fprintf(stderr,
            "%s: Can't use --auto-generate-sql when create and query strings are specified!\n",
            internal::my_progname);
    exit(1);
  }

  if (auto_generate_sql && auto_generate_sql_guid_primary &&
      auto_generate_sql_autoincrement)
  {
    fprintf(stderr,
            "%s: Either auto-generate-sql-guid-primary or auto-generate-sql-add-autoincrement can be used!\n",
            internal::my_progname);
    exit(1);
  }

  if (auto_generate_sql && num_of_query && auto_actual_queries)
  {
    fprintf(stderr,
            "%s: Either auto-generate-sql-execute-number or number-of-queries can be used!\n",
            internal::my_progname);
    exit(1);
  }

  parse_comma(concurrency_str ? concurrency_str : "1", &concurrency);

  if (opt_csv_str)
  {
    opt_silent= true;

    if (opt_csv_str[0] == '-')
    {
      csv_file= fileno(stdout);
    }
    else
    {
      if ((csv_file= open(opt_csv_str, O_CREAT|O_WRONLY|O_APPEND, 
                          S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
      {
        fprintf(stderr,"%s: Could not open csv file: %sn\n",
                internal::my_progname, opt_csv_str);
        exit(1);
      }
    }
  }

  if (opt_only_print)
    opt_silent= true;

  if (num_int_cols_opt)
  {
    option_string *str;
    parse_option(num_int_cols_opt, &str, ',');
    num_int_cols= atoi(str->string);
    if (str->option)
      num_int_cols_index= atoi(str->option);
    option_cleanup(str);
  }

  if (num_char_cols_opt)
  {
    option_string *str;
    parse_option(num_char_cols_opt, &str, ',');
    num_char_cols= atoi(str->string);
    if (str->option)
      num_char_cols_index= atoi(str->option);
    else
      num_char_cols_index= 0;
    option_cleanup(str);
  }

  if (num_blob_cols_opt)
  {
    option_string *str;
    parse_option(num_blob_cols_opt, &str, ',');
    num_blob_cols= atoi(str->string);
    if (str->option)
    {
      char *sep_ptr;

      if ((sep_ptr= strchr(str->option, '/')))
      {
        num_blob_cols_size_min= atoi(str->option);
        num_blob_cols_size= atoi(sep_ptr+1);
      }
      else
      {
        num_blob_cols_size_min= num_blob_cols_size= atoi(str->option);
      }
    }
    else
    {
      num_blob_cols_size= DEFAULT_BLOB_SIZE;
      num_blob_cols_size_min= DEFAULT_BLOB_SIZE;
    }
    option_cleanup(str);
  }


  if (auto_generate_sql)
  {
    uint64_t x= 0;
    statement *ptr_statement;

    if (verbose >= 2)
      printf("Building Create Statements for Auto\n");

    create_statements= build_table_string();
    /*
      Pre-populate table
    */
    for (ptr_statement= create_statements, x= 0;
         x < auto_generate_sql_unique_write_number;
         x++, ptr_statement= ptr_statement->next)
    {
      ptr_statement->next= build_insert_string();
    }

    if (verbose >= 2)
      printf("Building Query Statements for Auto\n");

    if (!opt_auto_generate_sql_type)
      opt_auto_generate_sql_type= "mixed";

    query_statements_count=
      parse_option(opt_auto_generate_sql_type, &query_options, ',');

    query_statements= (statement **)malloc(sizeof(statement *) * query_statements_count);
    if (query_statements == NULL)
    {
      fprintf(stderr, "Memory Allocation error in Building Query Statements\n");
      exit(1);
    }
    memset(query_statements, 0, sizeof(statement *) * query_statements_count);

    sql_type= query_options;
    do
    {
      if (sql_type->string[0] == 'r')
      {
        if (verbose >= 2)
          printf("Generating SELECT Statements for Auto\n");

        query_statements[sql_type_count]= build_select_string(false);
        for (ptr_statement= query_statements[sql_type_count], x= 0;
             x < auto_generate_sql_unique_query_number;
             x++, ptr_statement= ptr_statement->next)
        {
          ptr_statement->next= build_select_string(false);
        }
      }
      else if (sql_type->string[0] == 'k')
      {
        if (verbose >= 2)
          printf("Generating SELECT for keys Statements for Auto\n");

        if ( auto_generate_sql_autoincrement == false &&
             auto_generate_sql_guid_primary == false)
        {
          fprintf(stderr,
                  "%s: Can't perform key test without a primary key!\n",
                  internal::my_progname);
          exit(1);
        }

        query_statements[sql_type_count]= build_select_string(true);
        for (ptr_statement= query_statements[sql_type_count], x= 0;
             x < auto_generate_sql_unique_query_number;
             x++, ptr_statement= ptr_statement->next)
        {
          ptr_statement->next= build_select_string(true);
        }
      }
      else if (sql_type->string[0] == 'w')
      {
        /*
          We generate a number of strings in case the engine is
          Archive (since strings which were identical one after another
          would be too easily optimized).
        */
        if (verbose >= 2)
          printf("Generating INSERT Statements for Auto\n");
        query_statements[sql_type_count]= build_insert_string();
        for (ptr_statement= query_statements[sql_type_count], x= 0;
             x < auto_generate_sql_unique_query_number;
             x++, ptr_statement= ptr_statement->next)
        {
          ptr_statement->next= build_insert_string();
        }
      }
      else if (sql_type->string[0] == 'u')
      {
        if ( auto_generate_sql_autoincrement == false &&
             auto_generate_sql_guid_primary == false)
        {
          fprintf(stderr,
                  "%s: Can't perform update test without a primary key!\n",
                  internal::my_progname);
          exit(1);
        }

        query_statements[sql_type_count]= build_update_string();
        for (ptr_statement= query_statements[sql_type_count], x= 0;
             x < auto_generate_sql_unique_query_number;
             x++, ptr_statement= ptr_statement->next)
        {
          ptr_statement->next= build_update_string();
        }
      }
      else /* Mixed mode is default */
      {
        int coin= 0;

        query_statements[sql_type_count]= build_insert_string();
        /*
          This logic should be extended to do a more mixed load,
          at the moment it results in "every other".
        */
        for (ptr_statement= query_statements[sql_type_count], x= 0;
             x < auto_generate_sql_unique_query_number;
             x++, ptr_statement= ptr_statement->next)
        {
          if (coin)
          {
            ptr_statement->next= build_insert_string();
            coin= 0;
          }
          else
          {
            ptr_statement->next= build_select_string(true);
            coin= 1;
          }
        }
      }
      sql_type_count++;
    } while (sql_type ? (sql_type= sql_type->next) : 0);
  }
  else
  {
    if (create_string && !stat(create_string, &sbuf))
    {
      int data_file;
      if (!S_ISREG(sbuf.st_mode))
      {
        fprintf(stderr,"%s: Create file was not a regular file\n",
                internal::my_progname);
        exit(1);
      }
      if ((data_file= open(create_string, O_RDWR)) == -1)
      {
        fprintf(stderr,"%s: Could not open create file\n", internal::my_progname);
        exit(1);
      }
      if ((uint64_t)(sbuf.st_size + 1) > SIZE_MAX)
      {
        fprintf(stderr, "Request for more memory than architecture supports\n");
        exit(1);
      }
      tmp_string= (char *)malloc((size_t)(sbuf.st_size + 1));
      if (tmp_string == NULL)
      {
        fprintf(stderr, "Memory Allocation error in option processing\n");
        exit(1);
      }
      memset(tmp_string, 0, (size_t)(sbuf.st_size + 1));
      bytes_read= read(data_file, (unsigned char*) tmp_string,
                       (size_t)sbuf.st_size);
      tmp_string[sbuf.st_size]= '\0';
      close(data_file);
      if (bytes_read != sbuf.st_size)
      {
        fprintf(stderr, "Problem reading file: read less bytes than requested\n");
      }
      parse_delimiter(tmp_string, &create_statements, delimiter[0]);
      free(tmp_string);
    }
    else if (create_string)
    {
      parse_delimiter(create_string, &create_statements, delimiter[0]);
    }

    /* Set this up till we fully support options on user generated queries */
    if (user_supplied_query)
    {
      query_statements_count=
        parse_option("default", &query_options, ',');

      query_statements= (statement **)malloc(sizeof(statement *) * query_statements_count);
      if (query_statements == NULL)
      {
        fprintf(stderr, "Memory Allocation error in option processing\n");
        exit(1);
      }
      memset(query_statements, 0, sizeof(statement *) * query_statements_count); 
    }

    if (user_supplied_query && !stat(user_supplied_query, &sbuf))
    {
      int data_file;
      if (!S_ISREG(sbuf.st_mode))
      {
        fprintf(stderr,"%s: User query supplied file was not a regular file\n",
                internal::my_progname);
        exit(1);
      }
      if ((data_file= open(user_supplied_query, O_RDWR)) == -1)
      {
        fprintf(stderr,"%s: Could not open query supplied file\n", internal::my_progname);
        exit(1);
      }
      if ((uint64_t)(sbuf.st_size + 1) > SIZE_MAX)
      {
        fprintf(stderr, "Request for more memory than architecture supports\n");
        exit(1);
      }
      tmp_string= (char *)malloc((size_t)(sbuf.st_size + 1));
      if (tmp_string == NULL)
      {
        fprintf(stderr, "Memory Allocation error in option processing\n");
        exit(1);
      }
      memset(tmp_string, 0, (size_t)(sbuf.st_size + 1));
      bytes_read= read(data_file, (unsigned char*) tmp_string,
                       (size_t)sbuf.st_size);
      tmp_string[sbuf.st_size]= '\0';
      close(data_file);
      if (bytes_read != sbuf.st_size)
      {
        fprintf(stderr, "Problem reading file: read less bytes than requested\n");
      }
      if (user_supplied_query)
        actual_queries= parse_delimiter(tmp_string, &query_statements[0],
                                        delimiter[0]);
      free(tmp_string);
    }
    else if (user_supplied_query)
    {
      actual_queries= parse_delimiter(user_supplied_query, &query_statements[0],
                                      delimiter[0]);
    }
  }

  if (user_supplied_pre_statements
      && !stat(user_supplied_pre_statements, &sbuf))
  {
    int data_file;
    if (!S_ISREG(sbuf.st_mode))
    {
      fprintf(stderr,"%s: User query supplied file was not a regular file\n",
              internal::my_progname);
      exit(1);
    }
    if ((data_file= open(user_supplied_pre_statements, O_RDWR)) == -1)
    {
      fprintf(stderr,"%s: Could not open query supplied file\n", internal::my_progname);
      exit(1);
    }
    if ((uint64_t)(sbuf.st_size + 1) > SIZE_MAX)
    {
      fprintf(stderr, "Request for more memory than architecture supports\n");
      exit(1);
    }
    tmp_string= (char *)malloc((size_t)(sbuf.st_size + 1));
    if (tmp_string == NULL)
    {
      fprintf(stderr, "Memory Allocation error in option processing\n");
      exit(1);
    }
    memset(tmp_string, 0, (size_t)(sbuf.st_size + 1));
    bytes_read= read(data_file, (unsigned char*) tmp_string,
                     (size_t)sbuf.st_size);
    tmp_string[sbuf.st_size]= '\0';
    close(data_file);
    if (bytes_read != sbuf.st_size)
    {
      fprintf(stderr, "Problem reading file: read less bytes than requested\n");
    }
    if (user_supplied_pre_statements)
      (void)parse_delimiter(tmp_string, &pre_statements,
                            delimiter[0]);
    free(tmp_string);
  }
  else if (user_supplied_pre_statements)
  {
    (void)parse_delimiter(user_supplied_pre_statements,
                          &pre_statements,
                          delimiter[0]);
  }

  if (user_supplied_post_statements
      && !stat(user_supplied_post_statements, &sbuf))
  {
    int data_file;
    if (!S_ISREG(sbuf.st_mode))
    {
      fprintf(stderr,"%s: User query supplied file was not a regular file\n",
              internal::my_progname);
      exit(1);
    }
    if ((data_file= open(user_supplied_post_statements, O_RDWR)) == -1)
    {
      fprintf(stderr,"%s: Could not open query supplied file\n", internal::my_progname);
      exit(1);
    }

    if ((uint64_t)(sbuf.st_size + 1) > SIZE_MAX)
    {
      fprintf(stderr, "Request for more memory than architecture supports\n");
      exit(1);
    }
    tmp_string= (char *)malloc((size_t)(sbuf.st_size + 1));
    if (tmp_string == NULL)
    {
      fprintf(stderr, "Memory Allocation error in option processing\n");
      exit(1);
    }
    memset(tmp_string, 0, (size_t)(sbuf.st_size+1));

    bytes_read= read(data_file, (unsigned char*) tmp_string,
                     (size_t)(sbuf.st_size));
    tmp_string[sbuf.st_size]= '\0';
    close(data_file);
    if (bytes_read != sbuf.st_size)
    {
      fprintf(stderr, "Problem reading file: read less bytes than requested\n");
    }
    if (user_supplied_post_statements)
      (void)parse_delimiter(tmp_string, &post_statements,
                            delimiter[0]);
    free(tmp_string);
  }
  else if (user_supplied_post_statements)
  {
    (void)parse_delimiter(user_supplied_post_statements, &post_statements,
                          delimiter[0]);
  }

  if (verbose >= 2)
    printf("Parsing engines to use.\n");

  if (default_engine)
    parse_option(default_engine, &engine_options, ',');

  if (tty_password)
    opt_password= client_get_tty_password(NULL);
  return(0);
}


static int run_query(drizzle_con_st *con, drizzle_result_st *result,
                     const char *query, int len)
{
  drizzle_return_t ret;
  drizzle_result_st result_buffer;

  if (opt_only_print)
  {
    printf("%.*s;\n", len, query);
    return 0;
  }

  if (verbose >= 3)
    printf("%.*s;\n", len, query);

  if (result == NULL)
    result= &result_buffer;

  result= drizzle_query(con, result, query, len, &ret);

  if (ret == DRIZZLE_RETURN_OK)
    ret= drizzle_result_buffer(result);

  if (result == &result_buffer)
    drizzle_result_free(result);
    
  return ret;
}


static int
generate_primary_key_list(drizzle_con_st *con, option_string *engine_stmt)
{
  drizzle_result_st result;
  drizzle_row_t row;
  uint64_t counter;


  /*
    Blackhole is a special case, this allows us to test the upper end
    of the server during load runs.
  */
  if (opt_only_print || (engine_stmt &&
                         strstr(engine_stmt->string, "blackhole")))
  {
    primary_keys_number_of= 1;
    primary_keys= (char **)malloc((sizeof(char *) *
                                  primary_keys_number_of));
    if (primary_keys == NULL)
    {
      fprintf(stderr, "Memory Allocation error in option processing\n");
      exit(1);
    }
    
    memset(primary_keys, 0, (sizeof(char *) * primary_keys_number_of));
    /* Yes, we strdup a const string to simplify the interface */
    primary_keys[0]= strdup("796c4422-1d94-102a-9d6d-00e0812d");
    if (primary_keys[0] == NULL)
    {
      fprintf(stderr, "Memory Allocation error in option processing\n");
      exit(1);
    }
  }
  else
  {
    if (run_query(con, &result, "SELECT id from t1", strlen("SELECT id from t1")))
    {
      fprintf(stderr,"%s: Cannot select GUID primary keys. (%s)\n", internal::my_progname,
              drizzle_con_error(con));
      exit(1);
    }

    uint64_t num_rows_ret= drizzle_result_row_count(&result);
    if (num_rows_ret > SIZE_MAX)
    {
      fprintf(stderr, "More primary keys than than architecture supports\n");
      exit(1);
    }
    primary_keys_number_of= (size_t)num_rows_ret;

    /* So why check this? Blackhole :) */
    if (primary_keys_number_of)
    {
      /*
        We create the structure and loop and create the items.
      */
      primary_keys= (char **)malloc(sizeof(char *) *
                                    primary_keys_number_of);
      if (primary_keys == NULL)
      {
        fprintf(stderr, "Memory Allocation error in option processing\n");
        exit(1);
      }
      memset(primary_keys, 0, (size_t)(sizeof(char *) * primary_keys_number_of));
      row= drizzle_row_next(&result);
      for (counter= 0; counter < primary_keys_number_of;
           counter++, row= drizzle_row_next(&result))
      {
        primary_keys[counter]= strdup(row[0]);
        if (primary_keys[counter] == NULL)
        {
          fprintf(stderr, "Memory Allocation error in option processing\n");
          exit(1);
        }
      }
    }

    drizzle_result_free(&result);
  }

  return(0);
}

static int
drop_primary_key_list(void)
{
  uint64_t counter;

  if (primary_keys_number_of)
  {
    for (counter= 0; counter < primary_keys_number_of; counter++)
      free(primary_keys[counter]);

    free(primary_keys);
  }

  return 0;
}

static int
create_schema(drizzle_con_st *con, const char *db, statement *stmt,
              option_string *engine_stmt, stats *sptr)
{
  char query[HUGE_STRING_LENGTH];
  statement *ptr;
  statement *after_create;
  int len;
  uint64_t count;
  struct timeval start_time, end_time;


  gettimeofday(&start_time, NULL);

  len= snprintf(query, HUGE_STRING_LENGTH, "CREATE SCHEMA `%s`", db);

  if (verbose >= 2)
    printf("Loading Pre-data\n");

  if (run_query(con, NULL, query, len))
  {
    fprintf(stderr,"%s: Cannot create schema %s : %s\n", internal::my_progname, db,
            drizzle_con_error(con));
    exit(1);
  }
  else
  {
    sptr->create_count++;
  }

  if (opt_only_print)
  {
    printf("use %s;\n", db);
  }
  else
  {
    drizzle_result_st result;
    drizzle_return_t ret;

    if (verbose >= 3)
      printf("%s;\n", query);

    if (drizzle_select_db(con,  &result, db, &ret) == NULL ||
        ret != DRIZZLE_RETURN_OK)
    {
      fprintf(stderr,"%s: Cannot select schema '%s': %s\n",internal::my_progname, db,
              ret == DRIZZLE_RETURN_ERROR_CODE ?
              drizzle_result_error(&result) : drizzle_con_error(con));
      exit(1);
    }
    drizzle_result_free(&result);
    sptr->create_count++;
  }

  if (engine_stmt)
  {
    len= snprintf(query, HUGE_STRING_LENGTH, "set storage_engine=`%s`",
                  engine_stmt->string);
    if (run_query(con, NULL, query, len))
    {
      fprintf(stderr,"%s: Cannot set default engine: %s\n", internal::my_progname,
              drizzle_con_error(con));
      exit(1);
    }
    sptr->create_count++;
  }

  count= 0;
  after_create= stmt;

limit_not_met:
  for (ptr= after_create; ptr && ptr->length; ptr= ptr->next, count++)
  {
    if (auto_generate_sql && ( auto_generate_sql_number == count))
      break;

    if (engine_stmt && engine_stmt->option && ptr->type == CREATE_TABLE_TYPE)
    {
      char buffer[HUGE_STRING_LENGTH];

      snprintf(buffer, HUGE_STRING_LENGTH, "%s %s", ptr->string,
               engine_stmt->option);
      if (run_query(con, NULL, buffer, strlen(buffer)))
      {
        fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                internal::my_progname, (uint32_t)ptr->length, ptr->string, drizzle_con_error(con));
        if (!opt_ignore_sql_errors)
          exit(1);
      }
      sptr->create_count++;
    }
    else
    {
      if (run_query(con, NULL, ptr->string, ptr->length))
      {
        fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                internal::my_progname, (uint32_t)ptr->length, ptr->string, drizzle_con_error(con));
        if (!opt_ignore_sql_errors)
          exit(1);
      }
      sptr->create_count++;
    }
  }

  if (auto_generate_sql && (auto_generate_sql_number > count ))
  {
    /* Special case for auto create, we don't want to create tables twice */
    after_create= stmt->next;
    goto limit_not_met;
  }

  gettimeofday(&end_time, NULL);

  sptr->create_timing= timedif(end_time, start_time);

  return(0);
}

static int
drop_schema(drizzle_con_st *con, const char *db)
{
  char query[HUGE_STRING_LENGTH];
  int len;

  len= snprintf(query, HUGE_STRING_LENGTH, "DROP SCHEMA IF EXISTS `%s`", db);

  if (run_query(con, NULL, query, len))
  {
    fprintf(stderr,"%s: Cannot drop database '%s' ERROR : %s\n",
            internal::my_progname, db, drizzle_con_error(con));
    exit(1);
  }



  return(0);
}

static int
run_statements(drizzle_con_st *con, statement *stmt)
{
  statement *ptr;

  for (ptr= stmt; ptr && ptr->length; ptr= ptr->next)
  {
    if (run_query(con, NULL, ptr->string, ptr->length))
    {
      fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
              internal::my_progname, (uint32_t)ptr->length, ptr->string, drizzle_con_error(con));
      exit(1);
    }
  }

  return(0);
}

static int
run_scheduler(stats *sptr, statement **stmts, uint32_t concur, uint64_t limit)
{
  uint32_t x;
  uint32_t y;
  unsigned int real_concurrency;
  struct timeval start_time, end_time;
  option_string *sql_type;
  thread_context *con;
  pthread_t mainthread;            /* Thread descriptor */
  pthread_attr_t attr;          /* Thread attributes */


  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,
                              PTHREAD_CREATE_DETACHED);

  pthread_mutex_lock(&counter_mutex);
  thread_counter= 0;

  pthread_mutex_lock(&sleeper_mutex);
  master_wakeup= 1;
  pthread_mutex_unlock(&sleeper_mutex);

  real_concurrency= 0;

  for (y= 0, sql_type= query_options;
       y < query_statements_count;
       y++, sql_type= sql_type->next)
  {
    unsigned int options_loop= 1;

    if (sql_type->option)
    {
      options_loop= strtol(sql_type->option,
                           (char **)NULL, 10);
      options_loop= options_loop ? options_loop : 1;
    }

    while (options_loop--)
      for (x= 0; x < concur; x++)
      {
        con= (thread_context *)malloc(sizeof(thread_context));
        if (con == NULL)
        {
          fprintf(stderr, "Memory Allocation error in scheduler\n");
          exit(1);
        }
        con->stmt= stmts[y];
        con->limit= limit;

        real_concurrency++;
        /* now you create the thread */
        if (pthread_create(&mainthread, &attr, run_task,
                           (void *)con) != 0)
        {
          fprintf(stderr,"%s: Could not create thread\n", internal::my_progname);
          exit(1);
        }
        thread_counter++;
      }
  }

  /*
    The timer_thread belongs to all threads so it too obeys the wakeup
    call that run tasks obey.
  */
  if (opt_timer_length)
  {
    pthread_mutex_lock(&timer_alarm_mutex);
    timer_alarm= true;
    pthread_mutex_unlock(&timer_alarm_mutex);

    if (pthread_create(&mainthread, &attr, timer_thread,
                       (void *)&opt_timer_length) != 0)
    {
      fprintf(stderr,"%s: Could not create timer thread\n", internal::my_progname);
      exit(1);
    }
  }

  pthread_mutex_unlock(&counter_mutex);
  pthread_attr_destroy(&attr);

  pthread_mutex_lock(&sleeper_mutex);
  master_wakeup= 0;
  pthread_mutex_unlock(&sleeper_mutex);
  pthread_cond_broadcast(&sleep_threshhold);

  gettimeofday(&start_time, NULL);

  /*
    We loop until we know that all children have cleaned up.
  */
  pthread_mutex_lock(&counter_mutex);
  while (thread_counter)
  {
    struct timespec abstime;

    set_timespec(abstime, 3);
    pthread_cond_timedwait(&count_threshhold, &counter_mutex, &abstime);
  }
  pthread_mutex_unlock(&counter_mutex);

  gettimeofday(&end_time, NULL);


  sptr->timing= timedif(end_time, start_time);
  sptr->users= concur;
  sptr->real_users= real_concurrency;
  sptr->rows= limit;

  return(0);
}


pthread_handler_t timer_thread(void *p)
{
  uint32_t *timer_length= (uint32_t *)p;
  struct timespec abstime;


  /*
    We lock around the initial call in case were we in a loop. This
    also keeps the value properly syncronized across call threads.
  */
  pthread_mutex_lock(&sleeper_mutex);
  while (master_wakeup)
  {
    pthread_cond_wait(&sleep_threshhold, &sleeper_mutex);
  }
  pthread_mutex_unlock(&sleeper_mutex);

  set_timespec(abstime, *timer_length);

  pthread_mutex_lock(&timer_alarm_mutex);
  pthread_cond_timedwait(&timer_alarm_threshold, &timer_alarm_mutex, &abstime);
  pthread_mutex_unlock(&timer_alarm_mutex);

  pthread_mutex_lock(&timer_alarm_mutex);
  timer_alarm= false;
  pthread_mutex_unlock(&timer_alarm_mutex);

  return(0);
}

pthread_handler_t run_task(void *p)
{
  uint64_t counter= 0, queries;
  uint64_t detach_counter;
  unsigned int commit_counter;
  drizzle_con_st con;
  drizzle_result_st result;
  drizzle_row_t row;
  statement *ptr;
  thread_context *ctx= (thread_context *)p;

  pthread_mutex_lock(&sleeper_mutex);
  while (master_wakeup)
  {
    pthread_cond_wait(&sleep_threshhold, &sleeper_mutex);
  }
  pthread_mutex_unlock(&sleeper_mutex);

  slap_connect(&con, true);

  if (verbose >= 3)
    printf("connected!\n");
  queries= 0;

  commit_counter= 0;
  if (commit_rate)
    run_query(&con, NULL, "SET AUTOCOMMIT=0", strlen("SET AUTOCOMMIT=0"));

limit_not_met:
  for (ptr= ctx->stmt, detach_counter= 0;
       ptr && ptr->length;
       ptr= ptr->next, detach_counter++)
  {
    if (!opt_only_print && detach_rate && !(detach_counter % detach_rate))
    {
      slap_close(&con);
      slap_connect(&con, true);
    }

    /*
      We have to execute differently based on query type. This should become a function.
    */
    if ((ptr->type == UPDATE_TYPE_REQUIRES_PREFIX) ||
        (ptr->type == SELECT_TYPE_REQUIRES_PREFIX))
    {
      int length;
      unsigned int key_val;
      char *key;
      char buffer[HUGE_STRING_LENGTH];

      /*
        This should only happen if some sort of new engine was
        implemented that didn't properly handle UPDATEs.

        Just in case someone runs this under an experimental engine we don't
        want a crash so the if() is placed here.
      */
      assert(primary_keys_number_of);
      if (primary_keys_number_of)
      {
        key_val= (unsigned int)(random() % primary_keys_number_of);
        key= primary_keys[key_val];

        assert(key);

        length= snprintf(buffer, HUGE_STRING_LENGTH, "%.*s '%s'",
                         (int)ptr->length, ptr->string, key);

        if (run_query(&con, &result, buffer, length))
        {
          fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                  internal::my_progname, (uint32_t)length, buffer, drizzle_con_error(&con));
          exit(1);
        }
      }
    }
    else
    {
      if (run_query(&con, &result, ptr->string, ptr->length))
      {
        fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                internal::my_progname, (uint32_t)ptr->length, ptr->string, drizzle_con_error(&con));
        exit(1);
      }
    }

    if (!opt_only_print)
    {
      while ((row = drizzle_row_next(&result)))
        counter++;
      drizzle_result_free(&result);
    }
    queries++;

    if (commit_rate && (++commit_counter == commit_rate))
    {
      commit_counter= 0;
      run_query(&con, NULL, "COMMIT", strlen("COMMIT"));
    }

    /* If the timer is set, and the alarm is not active then end */
    if (opt_timer_length && timer_alarm == false)
      goto end;

    /* If limit has been reached, and we are not in a timer_alarm just end */
    if (ctx->limit && queries == ctx->limit && timer_alarm == false)
      goto end;
  }

  if (opt_timer_length && timer_alarm == true)
    goto limit_not_met;

  if (ctx->limit && queries < ctx->limit)
    goto limit_not_met;


end:
  if (commit_rate)
    run_query(&con, NULL, "COMMIT", strlen("COMMIT"));

  slap_close(&con);

  pthread_mutex_lock(&counter_mutex);
  thread_counter--;
  pthread_cond_signal(&count_threshhold);
  pthread_mutex_unlock(&counter_mutex);

  free(ctx);

  return(0);
}

/*
  Parse records from comma seperated string. : is a reserved character and is used for options
  on variables.
*/
uint
parse_option(const char *origin, option_string **stmt, char delm)
{
  char *string;
  char *begin_ptr;
  char *end_ptr;
  option_string **sptr= stmt;
  option_string *tmp;
  uint32_t length= strlen(origin);
  uint32_t count= 0; /* We know that there is always one */

  end_ptr= (char *)origin + length;

  tmp= *sptr= (option_string *)malloc(sizeof(option_string));
  if (tmp == NULL)
  {
    fprintf(stderr,"Error allocating memory while parsing options\n");
    exit(1);
  }
  memset(tmp, 0, sizeof(option_string));

  for (begin_ptr= (char *)origin;
       begin_ptr != end_ptr;
       tmp= tmp->next)
  {
    char buffer[HUGE_STRING_LENGTH];
    char *buffer_ptr;

    memset(buffer, 0, HUGE_STRING_LENGTH);

    string= strchr(begin_ptr, delm);

    if (string)
    {
      memcpy(buffer, begin_ptr, string - begin_ptr);
      begin_ptr= string+1;
    }
    else
    {
      size_t begin_len= strlen(begin_ptr);
      memcpy(buffer, begin_ptr, begin_len);
      begin_ptr= end_ptr;
    }

    if ((buffer_ptr= strchr(buffer, ':')))
    {
      /* Set a null so that we can get strlen() correct later on */
      buffer_ptr[0]= 0;
      buffer_ptr++;

      /* Move past the : and the first string */
      tmp->option_length= strlen(buffer_ptr);
      tmp->option= (char *)malloc(tmp->option_length + 1);
      if (tmp->option == NULL)
      {
        fprintf(stderr,"Error allocating memory while parsing options\n");
        exit(1);
      }
      memcpy(tmp->option, buffer_ptr, tmp->option_length);
      tmp->option[tmp->option_length]= 0; 
    }

    tmp->length= strlen(buffer);
    tmp->string= strdup(buffer);
    if (tmp->string == NULL)
    {
      fprintf(stderr,"Error allocating memory while parsing options\n");
      exit(1);
    }

    if (isspace(*begin_ptr))
      begin_ptr++;

    count++;

    if (begin_ptr != end_ptr)
    {
      tmp->next= (option_string *)malloc(sizeof(option_string));
      if (tmp->next == NULL)
      {
        fprintf(stderr,"Error allocating memory while parsing options\n");
        exit(1);
      }
      memset(tmp->next, 0, sizeof(option_string));
    }
    
  }

  return count;
}


/*
  Raw parsing interface. If you want the slap specific parser look at
  parse_option.
*/
uint
parse_delimiter(const char *script, statement **stmt, char delm)
{
  char *retstr;
  char *ptr= (char *)script;
  statement **sptr= stmt;
  statement *tmp;
  uint32_t length= strlen(script);
  uint32_t count= 0; /* We know that there is always one */

  for (tmp= *sptr= (statement *)calloc(1, sizeof(statement));
       (retstr= strchr(ptr, delm));
       tmp->next=  (statement *)calloc(1, sizeof(statement)),
       tmp= tmp->next)
  {
    if (tmp == NULL)
    {
      fprintf(stderr,"Error allocating memory while parsing delimiter\n");
      exit(1);
    }

    count++;
    tmp->length= (size_t)(retstr - ptr);
    tmp->string= (char *)malloc(tmp->length + 1);

    if (tmp->string == NULL)
    {
      fprintf(stderr,"Error allocating memory while parsing delimiter\n");
      exit(1);
    }

    memcpy(tmp->string, ptr, tmp->length);
    tmp->string[tmp->length]= 0;
    ptr+= retstr - ptr + 1;
    if (isspace(*ptr))
      ptr++;
  }

  if (ptr != script+length)
  {
    tmp->length= (size_t)((script + length) - ptr);
    tmp->string= (char *)malloc(tmp->length + 1);
    if (tmp->string == NULL)
    {
      fprintf(stderr,"Error allocating memory while parsing delimiter\n");
      exit(1);
    }
    memcpy(tmp->string, ptr, tmp->length);
    tmp->string[tmp->length]= 0;
    count++;
  }

  return count;
}


/*
  Parse comma is different from parse_delimeter in that it parses
  number ranges from a comma seperated string.
  In restrospect, this is a lousy name from this function.
*/
uint
parse_comma(const char *string, uint32_t **range)
{
  unsigned int count= 1,x; /* We know that there is always one */
  char *retstr;
  char *ptr= (char *)string;
  unsigned int *nptr;

  for (;*ptr; ptr++)
    if (*ptr == ',') count++;

  /* One extra spot for the NULL */
  nptr= *range= (uint32_t *)malloc(sizeof(unsigned int) * (count + 1));
  memset(nptr, 0, sizeof(unsigned int) * (count + 1));

  ptr= (char *)string;
  x= 0;
  while ((retstr= strchr(ptr,',')))
  {
    nptr[x++]= atoi(ptr);
    ptr+= retstr - ptr + 1;
  }
  nptr[x++]= atoi(ptr);

  return count;
}

void
print_conclusions(conclusions *con)
{
  printf("Benchmark\n");
  if (con->engine)
    printf("\tRunning for engine %s\n", con->engine);
  if (opt_label || opt_auto_generate_sql_type)
  {
    const char *ptr= opt_auto_generate_sql_type ? opt_auto_generate_sql_type : "query";
    printf("\tLoad: %s\n", opt_label ? opt_label : ptr);
  }
  printf("\tAverage Time took to generate schema and initial data: %ld.%03ld seconds\n",
         con->create_avg_timing / 1000, con->create_avg_timing % 1000);
  printf("\tAverage number of seconds to run all queries: %ld.%03ld seconds\n",
         con->avg_timing / 1000, con->avg_timing % 1000);
  printf("\tMinimum number of seconds to run all queries: %ld.%03ld seconds\n",
         con->min_timing / 1000, con->min_timing % 1000);
  printf("\tMaximum number of seconds to run all queries: %ld.%03ld seconds\n",
         con->max_timing / 1000, con->max_timing % 1000);
  printf("\tTotal time for tests: %ld.%03ld seconds\n",
         con->sum_of_time / 1000, con->sum_of_time % 1000);
  printf("\tStandard Deviation: %ld.%03ld\n", con->std_dev / 1000, con->std_dev % 1000);
  printf("\tNumber of queries in create queries: %"PRIu64"\n", con->create_count);
  printf("\tNumber of clients running queries: %u/%u\n",
         con->users, con->real_users);
  printf("\tNumber of times test was run: %u\n", iterations);
  printf("\tAverage number of queries per client: %"PRIu64"\n", con->avg_rows);
  printf("\n");
}

void
print_conclusions_csv(conclusions *con)
{
  unsigned int x;
  char buffer[HUGE_STRING_LENGTH];
  char label_buffer[HUGE_STRING_LENGTH];
  size_t string_len;

  memset(label_buffer, 0, HUGE_STRING_LENGTH);

  if (opt_label)
  {
    string_len= strlen(opt_label);

    for (x= 0; x < string_len; x++)
    {
      if (opt_label[x] == ',')
        label_buffer[x]= '-';
      else
        label_buffer[x]= opt_label[x] ;
    }
  }
  else if (opt_auto_generate_sql_type)
  {
    string_len= strlen(opt_auto_generate_sql_type);

    for (x= 0; x < string_len; x++)
    {
      if (opt_auto_generate_sql_type[x] == ',')
        label_buffer[x]= '-';
      else
        label_buffer[x]= opt_auto_generate_sql_type[x] ;
    }
  }
  else
    snprintf(label_buffer, HUGE_STRING_LENGTH, "query");

  snprintf(buffer, HUGE_STRING_LENGTH,
           "%s,%s,%ld.%03ld,%ld.%03ld,%ld.%03ld,%ld.%03ld,%ld.%03ld,"
           "%u,%u,%u,%"PRIu64"\n",
           con->engine ? con->engine : "", /* Storage engine we ran against */
           label_buffer, /* Load type */
           con->avg_timing / 1000, con->avg_timing % 1000, /* Time to load */
           con->min_timing / 1000, con->min_timing % 1000, /* Min time */
           con->max_timing / 1000, con->max_timing % 1000, /* Max time */
           con->sum_of_time / 1000, con->sum_of_time % 1000, /* Total time */
           con->std_dev / 1000, con->std_dev % 1000, /* Standard Deviation */
           iterations, /* Iterations */
           con->users, /* Children used max_timing */
           con->real_users, /* Children used max_timing */
           con->avg_rows  /* Queries run */
           );
  internal::my_write(csv_file, (unsigned char*) buffer, (uint32_t)strlen(buffer), MYF(0));
}

void
generate_stats(conclusions *con, option_string *eng, stats *sptr)
{
  stats *ptr;
  unsigned int x;

  con->min_timing= sptr->timing;
  con->max_timing= sptr->timing;
  con->min_rows= sptr->rows;
  con->max_rows= sptr->rows;

  /* At the moment we assume uniform */
  con->users= sptr->users;
  con->real_users= sptr->real_users;
  con->avg_rows= sptr->rows;

  /* With no next, we know it is the last element that was malloced */
  for (ptr= sptr, x= 0; x < iterations; ptr++, x++)
  {
    con->avg_timing+= ptr->timing;

    if (ptr->timing > con->max_timing)
      con->max_timing= ptr->timing;
    if (ptr->timing < con->min_timing)
      con->min_timing= ptr->timing;
  }
  con->sum_of_time= con->avg_timing;
  con->avg_timing= con->avg_timing/iterations;

  if (eng && eng->string)
    con->engine= eng->string;
  else
    con->engine= NULL;

  standard_deviation(con, sptr);

  /* Now we do the create time operations */
  con->create_min_timing= sptr->create_timing;
  con->create_max_timing= sptr->create_timing;

  /* At the moment we assume uniform */
  con->create_count= sptr->create_count;

  /* With no next, we know it is the last element that was malloced */
  for (ptr= sptr, x= 0; x < iterations; ptr++, x++)
  {
    con->create_avg_timing+= ptr->create_timing;

    if (ptr->create_timing > con->create_max_timing)
      con->create_max_timing= ptr->create_timing;
    if (ptr->create_timing < con->create_min_timing)
      con->create_min_timing= ptr->create_timing;
  }
  con->create_avg_timing= con->create_avg_timing/iterations;
}

void
option_cleanup(option_string *stmt)
{
  option_string *ptr, *nptr;
  if (!stmt)
    return;

  for (ptr= stmt; ptr; ptr= nptr)
  {
    nptr= ptr->next;
    if (ptr->string)
      free(ptr->string);
    if (ptr->option)
      free(ptr->option);
    free(ptr);
  }
}

void
statement_cleanup(statement *stmt)
{
  statement *ptr, *nptr;
  if (!stmt)
    return;

  for (ptr= stmt; ptr; ptr= nptr)
  {
    nptr= ptr->next;
    if (ptr->string)
      free(ptr->string);
    free(ptr);
  }
}

void
slap_close(drizzle_con_st *con)
{
  if (opt_only_print)
    return;

  drizzle_free(drizzle_con_drizzle(con));
}

void
slap_connect(drizzle_con_st *con, bool connect_to_schema)
{
  /* Connect to server */
  static uint32_t connection_retry_sleep= 100000; /* Microseconds */
  int x, connect_error= 1;
  drizzle_return_t ret;
  drizzle_st *drizzle;

  if (opt_only_print)
    return;

  if (opt_delayed_start)
    usleep(random()%opt_delayed_start);

  if ((drizzle= drizzle_create(NULL)) == NULL ||
      drizzle_con_add_tcp(drizzle, con, host, opt_drizzle_port, user,
                          opt_password,
                          connect_to_schema ? create_schema_string : NULL,
                          opt_mysql ? DRIZZLE_CON_MYSQL : DRIZZLE_CON_NONE) == NULL)
  {
    fprintf(stderr,"%s: Error creating drizzle object\n", internal::my_progname);
    exit(1);
  }

  for (x= 0; x < 10; x++)
  {
    if ((ret= drizzle_con_connect(con)) == DRIZZLE_RETURN_OK)
    {
      /* Connect suceeded */
      connect_error= 0;
      break;
    }
    usleep(connection_retry_sleep);
  }
  if (connect_error)
  {
    fprintf(stderr,"%s: Error when connecting to server: %d %s\n", internal::my_progname,
            ret, drizzle_con_error(con));
    exit(1);
  }

  return;
}

void
standard_deviation (conclusions *con, stats *sptr)
{
  unsigned int x;
  long int sum_of_squares;
  double the_catch;
  stats *ptr;

  if (iterations == 1 || iterations == 0)
  {
    con->std_dev= 0;
    return;
  }

  for (ptr= sptr, x= 0, sum_of_squares= 0; x < iterations; ptr++, x++)
  {
    long int deviation;

    deviation= ptr->timing - con->avg_timing;
    sum_of_squares+= deviation*deviation;
  }

  the_catch= sqrt((double)(sum_of_squares/(iterations -1)));
  con->std_dev= (long int)the_catch;
}
