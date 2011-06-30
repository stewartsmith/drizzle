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

  @todo
  Add language for better tests
  String length for files and those put on the command line are not
  setup to handle binary data.
  More stats
  Break up tests and run them on multiple hosts at once.
  Allow output to be fed into a database directly.

*/

#include <config.h>
#include "client_priv.h"

#include "option_string.h"
#include "stats.h"
#include "thread_context.h"
#include "conclusions.h"
#include "wakeup.h"

#include <signal.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <fcntl.h>
#include <math.h>
#include <cassert>
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <drizzled/configmake.h>
#include <memory>

/* Added this for string translation. */
#include <drizzled/gettext.h>

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/program_options.hpp>
#include <boost/scoped_ptr.hpp>
#include <drizzled/atomics.h>

#define SLAP_NAME "drizzleslap"
#define SLAP_VERSION "1.5"

#define HUGE_STRING_LENGTH 8196
#define RAND_STRING_SIZE 126
#define DEFAULT_BLOB_SIZE 1024

using namespace std;
using namespace drizzled;
namespace po= boost::program_options;

#ifdef HAVE_SMEM
static char *shared_memory_base_name=0;
#endif

client::Wakeup master_wakeup;

/* Global Thread timer */
static bool timer_alarm= false;
boost::mutex timer_alarm_mutex;
boost::condition_variable_any timer_alarm_threshold;

std::vector < std::string > primary_keys;

drizzled::atomic<size_t> connection_count;
drizzled::atomic<uint64_t> failed_update_for_transaction;

static string host, 
  opt_password, 
  user,
  user_supplied_query,
  user_supplied_pre_statements,
  user_supplied_post_statements,
  default_engine,
  pre_system,
  post_system;

static vector<string> user_supplied_queries;
static string opt_verbose;
std::string opt_protocol;
string delimiter;

string create_schema_string;

static bool use_drizzle_protocol= false;
static bool opt_preserve= true;
static bool opt_only_print;
static bool opt_burnin;
static bool opt_ignore_sql_errors= false;
static bool opt_silent,
  auto_generate_sql_autoincrement,
  auto_generate_sql_guid_primary,
  auto_generate_sql;
std::string opt_auto_generate_sql_type;

static int32_t verbose= 0;
static uint32_t delimiter_length;
static uint32_t commit_rate;
static uint32_t detach_rate;
static uint32_t opt_timer_length;
static uint32_t opt_delayed_start;
string num_blob_cols_opt,
  num_char_cols_opt,
  num_int_cols_opt;
string opt_label;
static uint32_t opt_set_random_seed;

string auto_generate_selected_columns_opt;

/* Yes, we do set defaults here */
static uint32_t num_int_cols= 1;
static uint32_t num_char_cols= 1;
static uint32_t num_blob_cols= 0;
static uint32_t num_blob_cols_size;
static uint32_t num_blob_cols_size_min;
static uint32_t num_int_cols_index= 0;
static uint32_t num_char_cols_index= 0;
static uint32_t iterations;
static uint64_t actual_queries= 0;
static uint64_t auto_actual_queries;
static uint64_t auto_generate_sql_unique_write_number;
static uint64_t auto_generate_sql_unique_query_number;
static uint32_t auto_generate_sql_secondary_indexes;
static uint64_t num_of_query;
static uint64_t auto_generate_sql_number;
string concurrency_str;
string create_string;
std::vector <uint32_t> concurrency;

std::string opt_csv_str;
int csv_file;

static int process_options(void);
static uint32_t opt_drizzle_port= 0;

static OptionString *engine_options= NULL;
static OptionString *query_options= NULL;
static Statement *pre_statements= NULL;
static Statement *post_statements= NULL;
static Statement *create_statements= NULL;

static std::vector <Statement *> query_statements;
static uint32_t query_statements_count;


/* Prototypes */
void print_conclusions(Conclusions &con);
void print_conclusions_csv(Conclusions &con);
void generate_stats(Conclusions *con, OptionString *eng, Stats *sptr);
uint32_t parse_comma(const char *string, std::vector <uint32_t> &range);
uint32_t parse_delimiter(const char *script, Statement **stmt, char delm);
uint32_t parse_option(const char *origin, OptionString **stmt, char delm);
static void drop_schema(drizzle_con_st &con, const char *db);
uint32_t get_random_string(char *buf, size_t size);
static Statement *build_table_string(void);
static Statement *build_insert_string(void);
static Statement *build_update_string(void);
static Statement * build_select_string(bool key);
static int generate_primary_key_list(drizzle_con_st &con, OptionString *engine_stmt);
static void create_schema(drizzle_con_st &con, const char *db, Statement *stmt, OptionString *engine_stmt, Stats *sptr);
static void run_scheduler(Stats *sptr, Statement **stmts, uint32_t concur, uint64_t limit);
void statement_cleanup(Statement *stmt);
void option_cleanup(OptionString *stmt);
void concurrency_loop(drizzle_con_st &con, uint32_t current, OptionString *eptr);
static void run_statements(drizzle_con_st &con, Statement *stmt);
void slap_connect(drizzle_con_st &con, bool connect_to_schema);
void slap_close(drizzle_con_st &con);
static int run_query(drizzle_con_st &con, drizzle_result_st *result, const char *query, int len);
void standard_deviation(Conclusions &con, Stats *sptr);

static const char ALPHANUMERICS[]=
"0123456789ABCDEFGHIJKLMNOPQRSTWXYZabcdefghijklmnopqrstuvwxyz";

#define ALPHANUMERICS_SIZE (sizeof(ALPHANUMERICS)-1)


static long int timedif(struct timeval a, struct timeval b)
{
  int us, s;

  us = a.tv_usec - b.tv_usec;
  us /= 1000;
  s = a.tv_sec - b.tv_sec;
  s *= 1000;
  return s + us;
}

static void combine_queries(vector<string> queries)
{
  user_supplied_query.erase();
  for (vector<string>::iterator it= queries.begin();
       it != queries.end();
       ++it)
  {
    user_supplied_query.append(*it);
    user_supplied_query.append(delimiter);
  }
}


static void run_task(ThreadContext *ctx)
{
  uint64_t counter= 0, queries;
  uint64_t detach_counter;
  uint32_t commit_counter;
  boost::scoped_ptr<drizzle_con_st> con_ap(new drizzle_con_st);
  drizzle_con_st &con= *con_ap.get();
  drizzle_result_st result;
  drizzle_row_t row;
  Statement *ptr;

  master_wakeup.wait();

  slap_connect(con, true);

  if (verbose >= 3)
    printf("connected!\n");
  queries= 0;

  commit_counter= 0;
  if (commit_rate)
    run_query(con, NULL, "SET AUTOCOMMIT=0", strlen("SET AUTOCOMMIT=0"));

limit_not_met:
  for (ptr= ctx->getStmt(), detach_counter= 0;
       ptr && ptr->getLength();
       ptr= ptr->getNext(), detach_counter++)
  {
    if (not opt_only_print && detach_rate && !(detach_counter % detach_rate))
    {
      slap_close(con);
      slap_connect(con, true);
    }

    /*
      We have to execute differently based on query type. This should become a function.
    */
    bool is_failed_update= false;
    if ((ptr->getType() == UPDATE_TYPE_REQUIRES_PREFIX) ||
        (ptr->getType() == SELECT_TYPE_REQUIRES_PREFIX))
    {
      int length;
      uint32_t key_val;
      char buffer[HUGE_STRING_LENGTH];

      /*
        This should only happen if some sort of new engine was
        implemented that didn't properly handle UPDATEs.

        Just in case someone runs this under an experimental engine we don't
        want a crash so the if() is placed here.
      */
      assert(primary_keys.size());
      if (primary_keys.size())
      {
        key_val= (uint32_t)(random() % primary_keys.size());
        const char *key;
        key= primary_keys[key_val].c_str();

        assert(key);

        length= snprintf(buffer, HUGE_STRING_LENGTH, "%.*s '%s'",
                         (int)ptr->getLength(), ptr->getString(), key);

        if (run_query(con, &result, buffer, length))
        {
          if ((ptr->getType() == UPDATE_TYPE_REQUIRES_PREFIX) and commit_rate)
          {
            // Expand to check to see if Innodb, if so we should restart the
            // transaction.  

            is_failed_update= true;
            failed_update_for_transaction.fetch_and_increment();
          }
          else
          {
            fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                    SLAP_NAME, (uint32_t)length, buffer, drizzle_con_error(&con));
            abort();
          }
        }
      }
    }
    else
    {
      if (run_query(con, &result, ptr->getString(), ptr->getLength()))
      {
        if ((ptr->getType() == UPDATE_TYPE_REQUIRES_PREFIX) and commit_rate)
        {
          // Expand to check to see if Innodb, if so we should restart the
          // transaction.

          is_failed_update= true;
          failed_update_for_transaction.fetch_and_increment();
        }
        else
        {
          fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                  SLAP_NAME, (uint32_t)ptr->getLength(), ptr->getString(), drizzle_con_error(&con));
          abort();
        }
      }
    }

    if (not opt_only_print and not is_failed_update)
    {
      while ((row = drizzle_row_next(&result)))
        counter++;
      drizzle_result_free(&result);
    }
    queries++;

    if (commit_rate && (++commit_counter == commit_rate) and not is_failed_update)
    {
      commit_counter= 0;
      run_query(con, NULL, "COMMIT", strlen("COMMIT"));
    }

    /* If the timer is set, and the alarm is not active then end */
    if (opt_timer_length && timer_alarm == false)
      goto end;

    /* If limit has been reached, and we are not in a timer_alarm just end */
    if (ctx->getLimit() && queries == ctx->getLimit() && timer_alarm == false)
      goto end;
  }

  if (opt_timer_length && timer_alarm == true)
    goto limit_not_met;

  if (ctx->getLimit() && queries < ctx->getLimit())
    goto limit_not_met;


end:
  if (commit_rate)
    run_query(con, NULL, "COMMIT", strlen("COMMIT"));

  slap_close(con);

  delete ctx;
}

/**
 * commandline_options is the set of all options that can only be called via the command line.

 * client_options is the set of all options that can be defined via both command line and via
 * the configuration file client.cnf

 * slap_options is the set of all drizzleslap specific options which behave in a manner 
 * similar to that of client_options. It's configuration file is drizzleslap.cnf

 * long_options is the union of commandline_options, slap_options and client_options.

 * There are two configuration files per set of options, one which is defined by the user
 * which is found at either $XDG_CONFIG_HOME/drizzle or ~/.config/drizzle directory and the other which 
 * is the system configuration file which is found in the SYSCONFDIR/drizzle directory.

 * The system configuration file is over ridden by the user's configuration file which
 * in turn is over ridden by the command line.
 */
int main(int argc, char **argv)
{
  char *password= NULL;
  try
  {
    po::options_description commandline_options("Options used only in command line");
    commandline_options.add_options()
      ("help,?","Display this help and exit")
      ("info","Gives information and exit")
      ("burnin",po::value<bool>(&opt_burnin)->default_value(false)->zero_tokens(),
       "Run full test case in infinite loop")
      ("ignore-sql-errors", po::value<bool>(&opt_ignore_sql_errors)->default_value(false)->zero_tokens(),
       "Ignore SQL errors in query run")
      ("create-schema",po::value<string>(&create_schema_string)->default_value("drizzleslap"),
       "Schema to run tests in")
      ("create",po::value<string>(&create_string)->default_value(""),
       "File or string to use to create tables")
      ("detach",po::value<uint32_t>(&detach_rate)->default_value(0),
       "Detach (close and re open) connections after X number of requests")
      ("iterations,i",po::value<uint32_t>(&iterations)->default_value(1),
       "Number of times to run the tests")
      ("label",po::value<string>(&opt_label)->default_value(""),
       "Label to use for print and csv")
      ("number-blob-cols",po::value<string>(&num_blob_cols_opt)->default_value(""),
       "Number of BLOB columns to create table with if specifying --auto-generate-sql. Example --number-blob-cols=3:1024/2048 would give you 3 blobs with a random size between 1024 and 2048. ")
      ("number-char-cols,x",po::value<string>(&num_char_cols_opt)->default_value(""),
       "Number of VARCHAR columns to create in table if specifying --auto-generate-sql.")
      ("number-int-cols,y",po::value<string>(&num_int_cols_opt)->default_value(""),
       "Number of INT columns to create in table if specifying --auto-generate-sql.")
      ("number-of-queries",
       po::value<uint64_t>(&num_of_query)->default_value(0),
       "Limit each client to this number of queries(this is not exact)") 
      ("only-print",po::value<bool>(&opt_only_print)->default_value(false)->zero_tokens(),
       "This causes drizzleslap to not connect to the database instead print out what it would have done instead")
      ("post-query", po::value<string>(&user_supplied_post_statements)->default_value(""),
       "Query to run or file containing query to execute after tests have completed.")
      ("post-system",po::value<string>(&post_system)->default_value(""),
       "system() string to execute after tests have completed")
      ("pre-query",
       po::value<string>(&user_supplied_pre_statements)->default_value(""),
       "Query to run or file containing query to execute before running tests.")
      ("pre-system",po::value<string>(&pre_system)->default_value(""),
       "system() string to execute before running tests.")
      ("query,q",po::value<vector<string> >(&user_supplied_queries)->composing()->notifier(&combine_queries),
       "Query to run or file containing query")
      ("verbose,v", po::value<string>(&opt_verbose)->default_value("v"), "Increase verbosity level by one.")
      ("version,V","Output version information and exit") 
      ;

    po::options_description slap_options("Options specific to drizzleslap");
    slap_options.add_options()
      ("auto-generate-sql-select-columns",
       po::value<string>(&auto_generate_selected_columns_opt)->default_value(""),
       "Provide a string to use for the select fields used in auto tests")
      ("auto-generate-sql,a",po::value<bool>(&auto_generate_sql)->default_value(false)->zero_tokens(),
       "Generate SQL where not supplied by file or command line")  
      ("auto-generate-sql-add-autoincrement",
       po::value<bool>(&auto_generate_sql_autoincrement)->default_value(false)->zero_tokens(),
       "Add an AUTO_INCREMENT column to auto-generated tables")
      ("auto-generate-sql-execute-number",
       po::value<uint64_t>(&auto_actual_queries)->default_value(0),
       "See this number and generate a set of queries to run")
      ("auto-generate-sql-guid-primary",
       po::value<bool>(&auto_generate_sql_guid_primary)->default_value(false)->zero_tokens(),
       "Add GUID based primary keys to auto-generated tables")
      ("auto-generate-sql-load-type",
       po::value<string>(&opt_auto_generate_sql_type)->default_value("mixed"),
       "Specify test load type: mixed, update, write, key or read; default is mixed")  
      ("auto-generate-sql-secondary-indexes",
       po::value<uint32_t>(&auto_generate_sql_secondary_indexes)->default_value(0),
       "Number of secondary indexes to add to auto-generated tables")
      ("auto-generated-sql-unique-query-number",
       po::value<uint64_t>(&auto_generate_sql_unique_query_number)->default_value(10),
       "Number of unique queries to generate for automatic tests")
      ("auto-generate-sql-unique-write-number",
       po::value<uint64_t>(&auto_generate_sql_unique_write_number)->default_value(10),
       "Number of unique queries to generate for auto-generate-sql-write-number")
      ("auto-generate-sql-write-number",
       po::value<uint64_t>(&auto_generate_sql_number)->default_value(100),
       "Number of row inserts to perform for each thread (default is 100).")
      ("commit",po::value<uint32_t>(&commit_rate)->default_value(0),
       "Commit records every X number of statements")
      ("concurrency,c",po::value<string>(&concurrency_str)->default_value(""),
       "Number of clients to simulate for query to run")
      ("csv",po::value<std::string>(&opt_csv_str)->default_value(""),
       "Generate CSV output to named file or to stdout if no file is name.")
      ("delayed-start",po::value<uint32_t>(&opt_delayed_start)->default_value(0),
       "Delay the startup of threads by a random number of microsends (the maximum of the delay")
      ("delimiter,F",po::value<string>(&delimiter)->default_value("\n"),
       "Delimiter to use in SQL statements supplied in file or command line")
      ("engine,e",po::value<string>(&default_engine)->default_value(""),
       "Storage engine to use for creating the table")
      ("set-random-seed",
       po::value<uint32_t>(&opt_set_random_seed)->default_value(0), 
       "Seed for random number generator (srandom(3)) ") 
      ("silent,s",po::value<bool>(&opt_silent)->default_value(false)->zero_tokens(),
       "Run program in silent mode - no output. ") 
      ("timer-length",po::value<uint32_t>(&opt_timer_length)->default_value(0),
       "Require drizzleslap to run each specific test a certain amount of time in seconds")  
      ;

    po::options_description client_options("Options specific to the client");
    client_options.add_options()
      ("host,h",po::value<string>(&host)->default_value("localhost"),"Connect to the host")
      ("password,P",po::value<char *>(&password),
       "Password to use when connecting to server. If password is not given it's asked from the tty")
      ("port,p",po::value<uint32_t>(), "Port number to use for connection")
      ("protocol",po::value<string>(&opt_protocol)->default_value("mysql"),
       "The protocol of connection (mysql or drizzle).")
      ("user,u",po::value<string>(&user)->default_value(""),
       "User for login if not current user")  
      ;

    po::options_description long_options("Allowed Options");
    long_options.add(commandline_options).add(slap_options).add(client_options);

    std::string system_config_dir_slap(SYSCONFDIR); 
    system_config_dir_slap.append("/drizzle/drizzleslap.cnf");

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

    uint64_t temp_drizzle_port= 0;
    boost::scoped_ptr<drizzle_con_st> con_ap(new drizzle_con_st);
    drizzle_con_st &con= *con_ap.get();
    OptionString *eptr;

    // Disable allow_guessing
    int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(long_options).
              style(style).extra_parser(parse_password_arg).run(), vm);

    std::string user_config_dir_slap(user_config_dir);
    user_config_dir_slap.append("/drizzle/drizzleslap.cnf"); 

    std::string user_config_dir_client(user_config_dir);
    user_config_dir_client.append("/drizzle/client.cnf");

    ifstream user_slap_ifs(user_config_dir_slap.c_str());
    po::store(parse_config_file(user_slap_ifs, slap_options), vm);

    ifstream user_client_ifs(user_config_dir_client.c_str());
    po::store(parse_config_file(user_client_ifs, client_options), vm);

    ifstream system_slap_ifs(system_config_dir_slap.c_str());
    store(parse_config_file(system_slap_ifs, slap_options), vm);

    ifstream system_client_ifs(system_config_dir_client.c_str());
    store(parse_config_file(system_client_ifs, client_options), vm);

    po::notify(vm);

    if (process_options())
      abort();

    if ( vm.count("help") || vm.count("info"))
    {
      printf("%s  Ver %s Distrib %s, for %s-%s (%s)\n",SLAP_NAME, SLAP_VERSION,
          drizzle_version(),HOST_VENDOR,HOST_OS,HOST_CPU);
      puts("Copyright (C) 2008 Sun Microsystems");
      puts("This software comes with ABSOLUTELY NO WARRANTY. "
           "This is free software,\n"
           "and you are welcome to modify and redistribute it under the GPL "
           "license\n");
      puts("Run a query multiple times against the server\n");
      cout << long_options << endl;
      abort();
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
        abort();
      }
    }
    if (vm.count("port")) 
    {
      temp_drizzle_port= vm["port"].as<uint32_t>();

      if ((temp_drizzle_port == 0) || (temp_drizzle_port > 65535))
      {
        fprintf(stderr, _("Value supplied for port is not valid.\n"));
        abort();
      }
      else
      {
        opt_drizzle_port= (uint32_t) temp_drizzle_port;
      }
    }

  if ( vm.count("password") )
  {
    if (not opt_password.empty())
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



    if ( vm.count("version") )
    {
      printf("%s  Ver %s Distrib %s, for %s-%s (%s)\n",SLAP_NAME, SLAP_VERSION,
          drizzle_version(),HOST_VENDOR,HOST_OS,HOST_CPU);
      abort();
    }

    /* Seed the random number generator if we will be using it. */
    if (auto_generate_sql)
    {
      if (opt_set_random_seed == 0)
        opt_set_random_seed= (uint32_t)time(NULL);
      srandom(opt_set_random_seed);
    }

    /* globals? Yes, so we only have to run strlen once */
    delimiter_length= delimiter.length();

    slap_connect(con, false);

    /* Main iterations loop */
burnin:
    eptr= engine_options;
    do
    {
      /* For the final stage we run whatever queries we were asked to run */
      uint32_t *current;

      if (verbose >= 2)
        printf("Starting Concurrency Test\n");

      if (concurrency.size())
      {
        for (current= &concurrency[0]; current && *current; current++)
          concurrency_loop(con, *current, eptr);
      }
      else
      {
        uint32_t infinite= 1;
        do {
          concurrency_loop(con, infinite, eptr);
        }
        while (infinite++);
      }

      if (not opt_preserve)
        drop_schema(con, create_schema_string.c_str());

    } while (eptr ? (eptr= eptr->getNext()) : 0);

    if (opt_burnin)
      goto burnin;

    slap_close(con);

    /* now free all the strings we created */
    if (not opt_password.empty())
      opt_password.erase();

    concurrency.clear();

    statement_cleanup(create_statements);
    for (uint32_t x= 0; x < query_statements_count; x++)
      statement_cleanup(query_statements[x]);
    query_statements.clear();
    statement_cleanup(pre_statements);
    statement_cleanup(post_statements);
    option_cleanup(engine_options);
    option_cleanup(query_options);

#ifdef HAVE_SMEM
    free(shared_memory_base_name);
#endif

  }

  catch(std::exception &err)
  {
    cerr<<"Error:"<<err.what()<<endl;
  }

  if (csv_file != fileno(stdout))
    close(csv_file);

  return 0;
}

void concurrency_loop(drizzle_con_st &con, uint32_t current, OptionString *eptr)
{
  Stats *head_sptr;
  Stats *sptr;
  Conclusions conclusion;
  uint64_t client_limit;

  head_sptr= new Stats[iterations];
  if (head_sptr == NULL)
  {
    fprintf(stderr,"Error allocating memory in concurrency_loop\n");
    abort();
  }

  if (auto_actual_queries)
    client_limit= auto_actual_queries;
  else if (num_of_query)
    client_limit=  num_of_query / current;
  else
    client_limit= actual_queries;

  uint32_t x;
  for (x= 0, sptr= head_sptr; x < iterations; x++, sptr++)
  {
    /*
      We might not want to load any data, such as when we are calling
      a stored_procedure that doesn't use data, or we know we already have
      data in the table.
    */
    if (opt_preserve == false)
      drop_schema(con, create_schema_string.c_str());

    /* First we create */
    if (create_statements)
      create_schema(con, create_schema_string.c_str(), create_statements, eptr, sptr);

    /*
      If we generated GUID we need to build a list of them from creation that
      we can later use.
    */
    if (verbose >= 2)
      printf("Generating primary key list\n");
    if (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary)
      generate_primary_key_list(con, eptr);

    if (not pre_system.empty())
    {
      int ret= system(pre_system.c_str());
      assert(ret != -1);
    }

    /*
      Pre statements are always run after all other logic so they can
      correct/adjust any item that they want.
    */
    if (pre_statements)
      run_statements(con, pre_statements);

    run_scheduler(sptr, &query_statements[0], current, client_limit);

    if (post_statements)
      run_statements(con, post_statements);

    if (not post_system.empty())
    {
      int ret=  system(post_system.c_str());
      assert(ret !=-1);
    }

    /* We are finished with this run */
    if (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary)
      primary_keys.clear();
  }

  if (verbose >= 2)
    printf("Generating stats\n");

  generate_stats(&conclusion, eptr, head_sptr);

  if (not opt_silent)
    print_conclusions(conclusion);
  if (not opt_csv_str.empty())
    print_conclusions_csv(conclusion);

  delete [] head_sptr;
}


uint32_t get_random_string(char *buf, size_t size)
{
  char *buf_ptr= buf;

  for (size_t x= size; x > 0; x--)
    *buf_ptr++= ALPHANUMERICS[random() % ALPHANUMERICS_SIZE];
  return(buf_ptr - buf);
}


/*
  build_table_string

  This function builds a create table query if the user opts to not supply
  a file or string containing a create table statement
*/
static Statement *
build_table_string(void)
{
  char       buf[HUGE_STRING_LENGTH];
  uint32_t        col_count;
  Statement *ptr;
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
    for (uint32_t count= 0; count < auto_generate_sql_secondary_indexes; count++)
    {
      if (count) /* Except for the first pass we add a comma */
        table_string.append(",");

      if (snprintf(buf, HUGE_STRING_LENGTH, "id%d varchar(32) unique key", count)
          > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in create table\n");
        abort();
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
          abort();
        }
      }
      else
      {
        if (snprintf(buf, HUGE_STRING_LENGTH, "intcol%d INT ", col_count)
            > HUGE_STRING_LENGTH)
        {
          fprintf(stderr, "Memory Allocation error in create table\n");
          abort();
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
          abort();
        }
      }
      else
      {
        if (snprintf(buf, HUGE_STRING_LENGTH, "charcol%d VARCHAR(128)",
                     col_count) > HUGE_STRING_LENGTH)
        {
          fprintf(stderr, "Memory Allocation error in creating table\n");
          abort();
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
        abort();
      }
      table_string.append(buf);

      if (col_count < num_blob_cols)
        table_string.append(",");
    }

  table_string.append(")");
  ptr= new Statement;
  ptr->setString(table_string.length());
  if (ptr->getString()==NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating table\n");
    abort();
  }
  ptr->setType(CREATE_TABLE_TYPE);
  strcpy(ptr->getString(), table_string.c_str());
  return(ptr);
}

/*
  build_update_string()

  This function builds insert statements when the user opts to not supply
  an insert file or string containing insert data
*/
static Statement *
build_update_string(void)
{
  char       buf[HUGE_STRING_LENGTH];
  uint32_t        col_count;
  Statement *ptr;
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
        abort();
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
        abort();
      }
      update_string.append(buf);

      if (col_count < num_char_cols)
        update_string.append(",", 1);
    }

  if (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary)
    update_string.append(" WHERE id = ");


  ptr= new Statement;

  ptr->setString(update_string.length());
  if (ptr->getString() == NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating update\n");
    abort();
  }
  if (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary)
    ptr->setType(UPDATE_TYPE_REQUIRES_PREFIX);
  else
    ptr->setType(UPDATE_TYPE);
  strncpy(ptr->getString(), update_string.c_str(), ptr->getLength());
  return(ptr);
}


/*
  build_insert_string()

  This function builds insert statements when the user opts to not supply
  an insert file or string containing insert data
*/
static Statement *
build_insert_string(void)
{
  char       buf[HUGE_STRING_LENGTH];
  uint32_t        col_count;
  Statement *ptr;
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
    uint32_t count;

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
        abort();
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
    vector <char> blob_ptr;

    blob_ptr.resize(num_blob_cols_size);

    for (col_count= 1; col_count <= num_blob_cols; col_count++)
    {
      uint32_t buf_len;
      uint32_t size;
      uint32_t difference= num_blob_cols_size - num_blob_cols_size_min;

      size= difference ? (num_blob_cols_size_min + (random() % difference)) :
        num_blob_cols_size;

      buf_len= get_random_string(&blob_ptr[0], size);

      insert_string.append("'", 1);
      insert_string.append(&blob_ptr[0], buf_len);
      insert_string.append("'", 1);

      if (col_count < num_blob_cols)
        insert_string.append(",", 1);
    }
  }

  insert_string.append(")", 1);

  ptr= new Statement;
  ptr->setString(insert_string.length());
  if (ptr->getString()==NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating select\n");
    abort();
  }
  ptr->setType(INSERT_TYPE);
  strcpy(ptr->getString(), insert_string.c_str());
  return(ptr);
}


/*
  build_select_string()

  This function builds a query if the user opts to not supply a query
  statement or file containing a query statement
*/
static Statement *
build_select_string(bool key)
{
  char       buf[HUGE_STRING_LENGTH];
  uint32_t        col_count;
  Statement *ptr;
  string query_string;

  query_string.reserve(HUGE_STRING_LENGTH);

  query_string.append("SELECT ", 7);
  if (not auto_generate_selected_columns_opt.empty())
  {
    query_string.append(auto_generate_selected_columns_opt.c_str());
  }
  else
  {
    for (col_count= 1; col_count <= num_int_cols; col_count++)
    {
      if (snprintf(buf, HUGE_STRING_LENGTH, "intcol%d", col_count)
          > HUGE_STRING_LENGTH)
      {
        fprintf(stderr, "Memory Allocation error in creating select\n");
        abort();
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
        abort();
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
        abort();
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

  ptr= new Statement;
  ptr->setString(query_string.length());
  if (ptr->getString() == NULL)
  {
    fprintf(stderr, "Memory Allocation error in creating select\n");
    abort();
  }
  if ((key) &&
      (auto_generate_sql_autoincrement || auto_generate_sql_guid_primary))
    ptr->setType(SELECT_TYPE_REQUIRES_PREFIX);
  else
    ptr->setType(SELECT_TYPE);
  strcpy(ptr->getString(), query_string.c_str());
  return(ptr);
}

static int
process_options(void)
{
  struct stat sbuf;
  OptionString *sql_type;
  uint32_t sql_type_count= 0;
  ssize_t bytes_read= 0;
  
  if (user.empty())
    user= "root";

  verbose= opt_verbose.length();

  /* If something is created we clean it up, otherwise we leave schemas alone */
  if ( (not create_string.empty()) || auto_generate_sql)
    opt_preserve= false;

  if (auto_generate_sql && (not create_string.empty() || !user_supplied_query.empty()))
  {
    fprintf(stderr,
            "%s: Can't use --auto-generate-sql when create and query strings are specified!\n",
            SLAP_NAME);
    abort();
  }

  if (auto_generate_sql && auto_generate_sql_guid_primary &&
      auto_generate_sql_autoincrement)
  {
    fprintf(stderr,
            "%s: Either auto-generate-sql-guid-primary or auto-generate-sql-add-autoincrement can be used!\n",
            SLAP_NAME);
    abort();
  }

  if (auto_generate_sql && num_of_query && auto_actual_queries)
  {
    fprintf(stderr,
            "%s: Either auto-generate-sql-execute-number or number-of-queries can be used!\n",
            SLAP_NAME);
    abort();
  }

  parse_comma(not concurrency_str.empty() ? concurrency_str.c_str() : "1", concurrency);

  if (not opt_csv_str.empty())
  {
    opt_silent= true;

    if (opt_csv_str[0] == '-')
    {
      csv_file= fileno(stdout);
    }
    else
    {
      if ((csv_file= open(opt_csv_str.c_str(), O_CREAT|O_WRONLY|O_APPEND, 
                          S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
      {
        fprintf(stderr,"%s: Could not open csv file: %sn\n",
                SLAP_NAME, opt_csv_str.c_str());
        abort();
      }
    }
  }

  if (opt_only_print)
    opt_silent= true;

  if (not num_int_cols_opt.empty())
  {
    OptionString *str;
    parse_option(num_int_cols_opt.c_str(), &str, ',');
    num_int_cols= atoi(str->getString());
    if (str->getOption())
      num_int_cols_index= atoi(str->getOption());
    option_cleanup(str);
  }

  if (not num_char_cols_opt.empty())
  {
    OptionString *str;
    parse_option(num_char_cols_opt.c_str(), &str, ',');
    num_char_cols= atoi(str->getString());
    if (str->getOption())
      num_char_cols_index= atoi(str->getOption());
    else
      num_char_cols_index= 0;
    option_cleanup(str);
  }

  if (not num_blob_cols_opt.empty())
  {
    OptionString *str;
    parse_option(num_blob_cols_opt.c_str(), &str, ',');
    num_blob_cols= atoi(str->getString());
    if (str->getOption())
    {
      char *sep_ptr;

      if ((sep_ptr= strchr(str->getOption(), '/')))
      {
        num_blob_cols_size_min= atoi(str->getOption());
        num_blob_cols_size= atoi(sep_ptr+1);
      }
      else
      {
        num_blob_cols_size_min= num_blob_cols_size= atoi(str->getOption());
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
    Statement *ptr_statement;

    if (verbose >= 2)
      printf("Building Create Statements for Auto\n");

    create_statements= build_table_string();
    /*
      Pre-populate table
    */
    for (ptr_statement= create_statements, x= 0;
         x < auto_generate_sql_unique_write_number;
         x++, ptr_statement= ptr_statement->getNext())
    {
      ptr_statement->setNext(build_insert_string());
    }

    if (verbose >= 2)
      printf("Building Query Statements for Auto\n");

    if (opt_auto_generate_sql_type.empty())
      opt_auto_generate_sql_type= "mixed";

    query_statements_count=
      parse_option(opt_auto_generate_sql_type.c_str(), &query_options, ',');

    query_statements.resize(query_statements_count);

    sql_type= query_options;
    do
    {
      if (sql_type->getString()[0] == 'r')
      {
        if (verbose >= 2)
          printf("Generating SELECT Statements for Auto\n");

        query_statements[sql_type_count]= build_select_string(false);
        for (ptr_statement= query_statements[sql_type_count], x= 0;
             x < auto_generate_sql_unique_query_number;
             x++, ptr_statement= ptr_statement->getNext())
        {
          ptr_statement->setNext(build_select_string(false));
        }
      }
      else if (sql_type->getString()[0] == 'k')
      {
        if (verbose >= 2)
          printf("Generating SELECT for keys Statements for Auto\n");

        if ( auto_generate_sql_autoincrement == false &&
             auto_generate_sql_guid_primary == false)
        {
          fprintf(stderr,
                  "%s: Can't perform key test without a primary key!\n",
                  SLAP_NAME);
          abort();
        }

        query_statements[sql_type_count]= build_select_string(true);
        for (ptr_statement= query_statements[sql_type_count], x= 0;
             x < auto_generate_sql_unique_query_number;
             x++, ptr_statement= ptr_statement->getNext())
        {
          ptr_statement->setNext(build_select_string(true));
        }
      }
      else if (sql_type->getString()[0] == 'w')
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
             x++, ptr_statement= ptr_statement->getNext())
        {
          ptr_statement->setNext(build_insert_string());
        }
      }
      else if (sql_type->getString()[0] == 'u')
      {
        if ( auto_generate_sql_autoincrement == false &&
             auto_generate_sql_guid_primary == false)
        {
          fprintf(stderr,
                  "%s: Can't perform update test without a primary key!\n",
                  SLAP_NAME);
          abort();
        }

        query_statements[sql_type_count]= build_update_string();
        for (ptr_statement= query_statements[sql_type_count], x= 0;
             x < auto_generate_sql_unique_query_number;
             x++, ptr_statement= ptr_statement->getNext())
        {
          ptr_statement->setNext(build_update_string());
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
             x++, ptr_statement= ptr_statement->getNext())
        {
          if (coin)
          {
            ptr_statement->setNext(build_insert_string());
            coin= 0;
          }
          else
          {
            ptr_statement->setNext(build_select_string(true));
            coin= 1;
          }
        }
      }
      sql_type_count++;
    } while (sql_type ? (sql_type= sql_type->getNext()) : 0);
  }
  else
  {
    if (not create_string.empty() && !stat(create_string.c_str(), &sbuf))
    {
      int data_file;
      std::vector<char> tmp_string;
      if (not S_ISREG(sbuf.st_mode))
      {
        fprintf(stderr,"%s: Create file was not a regular file\n",
                SLAP_NAME);
        abort();
      }
      if ((data_file= open(create_string.c_str(), O_RDWR)) == -1)
      {
        fprintf(stderr,"%s: Could not open create file\n", SLAP_NAME);
        abort();
      }
      if ((uint64_t)(sbuf.st_size + 1) > SIZE_MAX)
      {
        fprintf(stderr, "Request for more memory than architecture supports\n");
        abort();
      }
      tmp_string.resize(sbuf.st_size + 1);
      bytes_read= read(data_file, (unsigned char*) &tmp_string[0],
                       (size_t)sbuf.st_size);
      close(data_file);
      if (bytes_read != sbuf.st_size)
      {
        fprintf(stderr, "Problem reading file: read less bytes than requested\n");
      }
      parse_delimiter(&tmp_string[0], &create_statements, delimiter[0]);
    }
    else if (not create_string.empty())
    {
      parse_delimiter(create_string.c_str(), &create_statements, delimiter[0]);
    }

    /* Set this up till we fully support options on user generated queries */
    if (not user_supplied_query.empty())
    {
      query_statements_count=
        parse_option("default", &query_options, ',');

      query_statements.resize(query_statements_count);
    }

    if (not user_supplied_query.empty() && !stat(user_supplied_query.c_str(), &sbuf))
    {
      int data_file;
      std::vector<char> tmp_string;

      if (not S_ISREG(sbuf.st_mode))
      {
        fprintf(stderr,"%s: User query supplied file was not a regular file\n",
                SLAP_NAME);
        abort();
      }
      if ((data_file= open(user_supplied_query.c_str(), O_RDWR)) == -1)
      {
        fprintf(stderr,"%s: Could not open query supplied file\n", SLAP_NAME);
        abort();
      }
      if ((uint64_t)(sbuf.st_size + 1) > SIZE_MAX)
      {
        fprintf(stderr, "Request for more memory than architecture supports\n");
        abort();
      }
      tmp_string.resize((size_t)(sbuf.st_size + 1));
      bytes_read= read(data_file, (unsigned char*) &tmp_string[0],
                       (size_t)sbuf.st_size);
      close(data_file);
      if (bytes_read != sbuf.st_size)
      {
        fprintf(stderr, "Problem reading file: read less bytes than requested\n");
      }
      if (not user_supplied_query.empty())
        actual_queries= parse_delimiter(&tmp_string[0], &query_statements[0],
                                        delimiter[0]);
    }
    else if (not user_supplied_query.empty())
    {
      actual_queries= parse_delimiter(user_supplied_query.c_str(), &query_statements[0],
                                      delimiter[0]);
    }
  }

  if (not user_supplied_pre_statements.empty()
      && !stat(user_supplied_pre_statements.c_str(), &sbuf))
  {
    int data_file;
    std::vector<char> tmp_string;

    if (not S_ISREG(sbuf.st_mode))
    {
      fprintf(stderr,"%s: User query supplied file was not a regular file\n",
              SLAP_NAME);
      abort();
    }
    if ((data_file= open(user_supplied_pre_statements.c_str(), O_RDWR)) == -1)
    {
      fprintf(stderr,"%s: Could not open query supplied file\n", SLAP_NAME);
      abort();
    }
    if ((uint64_t)(sbuf.st_size + 1) > SIZE_MAX)
    {
      fprintf(stderr, "Request for more memory than architecture supports\n");
      abort();
    }
    tmp_string.resize((size_t)(sbuf.st_size + 1));
    bytes_read= read(data_file, (unsigned char*) &tmp_string[0],
                     (size_t)sbuf.st_size);
    close(data_file);
    if (bytes_read != sbuf.st_size)
    {
      fprintf(stderr, "Problem reading file: read less bytes than requested\n");
    }
    if (not user_supplied_pre_statements.empty())
      (void)parse_delimiter(&tmp_string[0], &pre_statements,
                            delimiter[0]);
  }
  else if (not user_supplied_pre_statements.empty())
  {
    (void)parse_delimiter(user_supplied_pre_statements.c_str(),
                          &pre_statements,
                          delimiter[0]);
  }

  if (not user_supplied_post_statements.empty()
      && !stat(user_supplied_post_statements.c_str(), &sbuf))
  {
    int data_file;
    std::vector<char> tmp_string;

    if (not S_ISREG(sbuf.st_mode))
    {
      fprintf(stderr,"%s: User query supplied file was not a regular file\n",
              SLAP_NAME);
      abort();
    }
    if ((data_file= open(user_supplied_post_statements.c_str(), O_RDWR)) == -1)
    {
      fprintf(stderr,"%s: Could not open query supplied file\n", SLAP_NAME);
      abort();
    }

    if ((uint64_t)(sbuf.st_size + 1) > SIZE_MAX)
    {
      fprintf(stderr, "Request for more memory than architecture supports\n");
      abort();
    }
    tmp_string.resize((size_t)(sbuf.st_size + 1));

    bytes_read= read(data_file, (unsigned char*) &tmp_string[0],
                     (size_t)(sbuf.st_size));
    close(data_file);
    if (bytes_read != sbuf.st_size)
    {
      fprintf(stderr, "Problem reading file: read less bytes than requested\n");
    }
    if (not user_supplied_post_statements.empty())
      (void)parse_delimiter(&tmp_string[0], &post_statements,
                            delimiter[0]);
  }
  else if (not user_supplied_post_statements.empty())
  {
    (void)parse_delimiter(user_supplied_post_statements.c_str(), &post_statements,
                          delimiter[0]);
  }

  if (verbose >= 2)
    printf("Parsing engines to use.\n");

  if (not default_engine.empty())
    parse_option(default_engine.c_str(), &engine_options, ',');

  if (tty_password)
    opt_password= client_get_tty_password(NULL);
  return(0);
}


static int run_query(drizzle_con_st &con, drizzle_result_st *result,
                     const char *query, int len)
{
  drizzle_return_t ret;
  drizzle_result_st result_buffer;

  if (opt_only_print)
  {
    printf("/* CON: %" PRIu64 " */ %.*s;\n",
           (uint64_t)drizzle_context(drizzle_con_drizzle(&con)),
           len, query);
    return 0;
  }

  if (verbose >= 3)
    printf("%.*s;\n", len, query);

  if (result == NULL)
    result= &result_buffer;

  result= drizzle_query(&con, result, query, len, &ret);

  if (ret == DRIZZLE_RETURN_OK)
    ret= drizzle_result_buffer(result);

  if (result == &result_buffer)
    drizzle_result_free(result);
    
  return ret;
}


static int
generate_primary_key_list(drizzle_con_st &con, OptionString *engine_stmt)
{
  drizzle_result_st result;
  drizzle_row_t row;
  uint64_t counter;


  /*
    Blackhole is a special case, this allows us to test the upper end
    of the server during load runs.
  */
  if (opt_only_print || (engine_stmt &&
                         strstr(engine_stmt->getString(), "blackhole")))
  {
    /* Yes, we strdup a const string to simplify the interface */
    primary_keys.push_back("796c4422-1d94-102a-9d6d-00e0812d");
  }
  else
  {
    if (run_query(con, &result, "SELECT id from t1", strlen("SELECT id from t1")))
    {
      fprintf(stderr,"%s: Cannot select GUID primary keys. (%s)\n", SLAP_NAME,
              drizzle_con_error(&con));
      abort();
    }

    uint64_t num_rows_ret= drizzle_result_row_count(&result);
    if (num_rows_ret > SIZE_MAX)
    {
      fprintf(stderr, "More primary keys than than architecture supports\n");
      abort();
    }
    size_t primary_keys_number_of;
    primary_keys_number_of= (size_t)num_rows_ret;

    /* So why check this? Blackhole :) */
    if (primary_keys_number_of)
    {
      /*
        We create the structure and loop and create the items.
      */
      row= drizzle_row_next(&result);
      for (counter= 0; counter < primary_keys_number_of;
           counter++, row= drizzle_row_next(&result))
      {
        primary_keys.push_back(row[0]);
      }
    }

    drizzle_result_free(&result);
  }

  return(0);
}

static void create_schema(drizzle_con_st &con, const char *db, Statement *stmt, OptionString *engine_stmt, Stats *sptr)
{
  char query[HUGE_STRING_LENGTH];
  Statement *ptr;
  Statement *after_create;
  int len;
  struct timeval start_time, end_time;


  gettimeofday(&start_time, NULL);

  len= snprintf(query, HUGE_STRING_LENGTH, "CREATE SCHEMA `%s`", db);

  if (verbose >= 2)
    printf("Loading Pre-data\n");

  if (run_query(con, NULL, query, len))
  {
    fprintf(stderr,"%s: Cannot create schema %s : %s\n", SLAP_NAME, db,
            drizzle_con_error(&con));
    abort();
  }
  else
  {
    sptr->setCreateCount(sptr->getCreateCount()+1);
  }

  if (opt_only_print)
  {
    printf("/* CON: %" PRIu64 " */ use %s;\n",
           (uint64_t)drizzle_context(drizzle_con_drizzle(&con)),
           db);
  }
  else
  {
    drizzle_result_st result;
    drizzle_return_t ret;

    if (verbose >= 3)
      printf("%s;\n", query);

    if (drizzle_select_db(&con,  &result, db, &ret) == NULL ||
        ret != DRIZZLE_RETURN_OK)
    {
      fprintf(stderr,"%s: Cannot select schema '%s': %s\n",SLAP_NAME, db,
              ret == DRIZZLE_RETURN_ERROR_CODE ?
              drizzle_result_error(&result) : drizzle_con_error(&con));
      abort();
    }
    drizzle_result_free(&result);
    sptr->setCreateCount(sptr->getCreateCount()+1);
  }

  if (engine_stmt)
  {
    len= snprintf(query, HUGE_STRING_LENGTH, "set storage_engine=`%s`",
                  engine_stmt->getString());
    if (run_query(con, NULL, query, len))
    {
      fprintf(stderr,"%s: Cannot set default engine: %s\n", SLAP_NAME,
              drizzle_con_error(&con));
      abort();
    }
    sptr->setCreateCount(sptr->getCreateCount()+1);
  }

  uint64_t count= 0;
  after_create= stmt;

limit_not_met:
  for (ptr= after_create; ptr && ptr->getLength(); ptr= ptr->getNext(), count++)
  {
    if (auto_generate_sql && ( auto_generate_sql_number == count))
      break;

    if (engine_stmt && engine_stmt->getOption() && ptr->getType() == CREATE_TABLE_TYPE)
    {
      char buffer[HUGE_STRING_LENGTH];

      snprintf(buffer, HUGE_STRING_LENGTH, "%s %s", ptr->getString(),
               engine_stmt->getOption());
      if (run_query(con, NULL, buffer, strlen(buffer)))
      {
        fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                SLAP_NAME, (uint32_t)ptr->getLength(), ptr->getString(), drizzle_con_error(&con));
        if (not opt_ignore_sql_errors)
          abort();
      }
      sptr->setCreateCount(sptr->getCreateCount()+1);
    }
    else
    {
      if (run_query(con, NULL, ptr->getString(), ptr->getLength()))
      {
        fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                SLAP_NAME, (uint32_t)ptr->getLength(), ptr->getString(), drizzle_con_error(&con));
        if (not opt_ignore_sql_errors)
          abort();
      }
      sptr->setCreateCount(sptr->getCreateCount()+1);
    }
  }

  if (auto_generate_sql && (auto_generate_sql_number > count ))
  {
    /* Special case for auto create, we don't want to create tables twice */
    after_create= stmt->getNext();
    goto limit_not_met;
  }

  gettimeofday(&end_time, NULL);

  sptr->setCreateTiming(timedif(end_time, start_time));
}

static void drop_schema(drizzle_con_st &con, const char *db)
{
  char query[HUGE_STRING_LENGTH];
  int len;

  len= snprintf(query, HUGE_STRING_LENGTH, "DROP SCHEMA IF EXISTS `%s`", db);

  if (run_query(con, NULL, query, len))
  {
    fprintf(stderr,"%s: Cannot drop database '%s' ERROR : %s\n",
            SLAP_NAME, db, drizzle_con_error(&con));
    abort();
  }
}

static void run_statements(drizzle_con_st &con, Statement *stmt)
{
  for (Statement *ptr= stmt; ptr && ptr->getLength(); ptr= ptr->getNext())
  {
    if (run_query(con, NULL, ptr->getString(), ptr->getLength()))
    {
      fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
              SLAP_NAME, (uint32_t)ptr->getLength(), ptr->getString(), drizzle_con_error(&con));
      abort();
    }
  }
}


static void timer_thread()
{
  /*
    We lock around the initial call in case were we in a loop. This
    also keeps the value properly syncronized across call threads.
  */
  master_wakeup.wait();

  {
    boost::mutex::scoped_lock scopedLock(timer_alarm_mutex);

    boost::xtime xt; 
    xtime_get(&xt, boost::TIME_UTC); 
    xt.sec += opt_timer_length; 

    (void)timer_alarm_threshold.timed_wait(scopedLock, xt);
  }

  {
    boost::mutex::scoped_lock scopedLock(timer_alarm_mutex);
    timer_alarm= false;
  }
}

typedef boost::shared_ptr<boost::thread> Thread;
typedef std::vector <Thread> Threads;
static void run_scheduler(Stats *sptr, Statement **stmts, uint32_t concur, uint64_t limit)
{
  uint32_t real_concurrency;
  struct timeval start_time, end_time;

  Threads threads;

  {
    OptionString *sql_type;

    master_wakeup.reset();

    real_concurrency= 0;

    uint32_t y;
    for (y= 0, sql_type= query_options;
         y < query_statements_count;
         y++, sql_type= sql_type->getNext())
    {
      uint32_t options_loop= 1;

      if (sql_type->getOption())
      {
        options_loop= strtol(sql_type->getOption(),
                             (char **)NULL, 10);
        options_loop= options_loop ? options_loop : 1;
      }

      while (options_loop--)
      {
        for (uint32_t x= 0; x < concur; x++)
        {
          ThreadContext *con;
          con= new ThreadContext;
          if (con == NULL)
          {
            fprintf(stderr, "Memory Allocation error in scheduler\n");
            abort();
          }
          con->setStmt(stmts[y]);
          con->setLimit(limit);

          real_concurrency++;

          /* now you create the thread */
          Thread thread;
          thread= Thread(new boost::thread(boost::bind(&run_task, con)));
          threads.push_back(thread);

        }
      }
    }

    /*
      The timer_thread belongs to all threads so it too obeys the wakeup
      call that run tasks obey.
    */
    if (opt_timer_length)
    {
      {
        boost::mutex::scoped_lock alarmLock(timer_alarm_mutex);
        timer_alarm= true;
      }

      Thread thread;
      thread= Thread(new boost::thread(&timer_thread));
      threads.push_back(thread);
    }
  }

  master_wakeup.start();

  gettimeofday(&start_time, NULL);

  /*
    We loop until we know that all children have cleaned up.
  */
  for (Threads::iterator iter= threads.begin(); iter != threads.end(); iter++)
  {
    (*iter)->join();
  }

  gettimeofday(&end_time, NULL);

  sptr->setTiming(timedif(end_time, start_time));
  sptr->setUsers(concur);
  sptr->setRealUsers(real_concurrency);
  sptr->setRows(limit);
}

/*
  Parse records from comma seperated string. : is a reserved character and is used for options
  on variables.
*/
uint32_t parse_option(const char *origin, OptionString **stmt, char delm)
{
  char *string;
  char *begin_ptr;
  char *end_ptr;
  uint32_t length= strlen(origin);
  uint32_t count= 0; /* We know that there is always one */

  end_ptr= (char *)origin + length;

  OptionString *tmp;
  *stmt= tmp= new OptionString;

  for (begin_ptr= (char *)origin;
       begin_ptr != end_ptr;
       tmp= tmp->getNext())
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
      tmp->setOption(buffer_ptr);
    }

    tmp->setString(strdup(buffer));
    if (tmp->getString() == NULL)
    {
      fprintf(stderr,"Error allocating memory while parsing options\n");
      abort();
    }

    if (isspace(*begin_ptr))
      begin_ptr++;

    count++;

    if (begin_ptr != end_ptr)
    {
      tmp->setNext( new OptionString);
    }
    
  }

  return count;
}


/*
  Raw parsing interface. If you want the slap specific parser look at
  parse_option.
*/
uint32_t parse_delimiter(const char *script, Statement **stmt, char delm)
{
  char *retstr;
  char *ptr= (char *)script;
  Statement **sptr= stmt;
  Statement *tmp;
  uint32_t length= strlen(script);
  uint32_t count= 0; /* We know that there is always one */

  for (tmp= *sptr= new Statement;
       (retstr= strchr(ptr, delm));
       tmp->setNext(new Statement),
       tmp= tmp->getNext())
  {
    if (tmp == NULL)
    {
      fprintf(stderr,"Error allocating memory while parsing delimiter\n");
      abort();
    }

    count++;
    tmp->setString((size_t)(retstr - ptr));

    if (tmp->getString() == NULL)
    {
      fprintf(stderr,"Error allocating memory while parsing delimiter\n");
      abort();
    }

    memcpy(tmp->getString(), ptr, tmp->getLength());
    ptr+= retstr - ptr + 1;
    if (isspace(*ptr))
      ptr++;
  }

  if (ptr != script+length)
  {
    tmp->setString((size_t)((script + length) - ptr));
    if (tmp->getString() == NULL)
    {
      fprintf(stderr,"Error allocating memory while parsing delimiter\n");
      abort();
    }
    memcpy(tmp->getString(), ptr, tmp->getLength());
    count++;
  }

  return count;
}


/*
  Parse comma is different from parse_delimeter in that it parses
  number ranges from a comma seperated string.
  In restrospect, this is a lousy name from this function.
*/
uint32_t parse_comma(const char *string, std::vector <uint32_t> &range)
{
  uint32_t count= 1; /* We know that there is always one */
  char *retstr;
  char *ptr= (char *)string;
  uint32_t *nptr;

  for (;*ptr; ptr++)
    if (*ptr == ',') count++;

  /* One extra spot for the NULL */
  range.resize(count +1);
  nptr= &range[0];

  ptr= (char *)string;
  uint32_t x= 0;
  while ((retstr= strchr(ptr,',')))
  {
    nptr[x++]= atoi(ptr);
    ptr+= retstr - ptr + 1;
  }
  nptr[x++]= atoi(ptr);

  return count;
}

void print_conclusions(Conclusions &con)
{
  printf("Benchmark\n");
  if (con.getEngine())
    printf("\tRunning for engine %s\n", con.getEngine());

  if (not opt_label.empty() || !opt_auto_generate_sql_type.empty())
  {
    const char *ptr= opt_auto_generate_sql_type.c_str() ? opt_auto_generate_sql_type.c_str() : "query";
    printf("\tLoad: %s\n", !opt_label.empty() ? opt_label.c_str() : ptr);
  }
  printf("\tAverage Time took to generate schema and initial data: %ld.%03ld seconds\n",
         con.getCreateAvgTiming() / 1000, con.getCreateAvgTiming() % 1000);
  printf("\tAverage number of seconds to run all queries: %ld.%03ld seconds\n",
         con.getAvgTiming() / 1000, con.getAvgTiming() % 1000);
  printf("\tMinimum number of seconds to run all queries: %ld.%03ld seconds\n",
         con.getMinTiming() / 1000, con.getMinTiming() % 1000);
  printf("\tMaximum number of seconds to run all queries: %ld.%03ld seconds\n",
         con.getMaxTiming() / 1000, con.getMaxTiming() % 1000);
  printf("\tTotal time for tests: %ld.%03ld seconds\n",
         con.getSumOfTime() / 1000, con.getSumOfTime() % 1000);
  printf("\tStandard Deviation: %ld.%03ld\n", con.getStdDev() / 1000, con.getStdDev() % 1000);
  printf("\tNumber of queries in create queries: %"PRIu64"\n", con.getCreateCount());
  printf("\tNumber of clients running queries: %u/%u\n",
         con.getUsers(), con.getRealUsers());
  printf("\tNumber of times test was run: %u\n", iterations);
  printf("\tAverage number of queries per client: %"PRIu64"\n", con.getAvgRows());

  uint64_t temp_val= failed_update_for_transaction; 
  if (temp_val)
    printf("\tFailed number of updates %"PRIu64"\n", temp_val);

  printf("\n");
}

void print_conclusions_csv(Conclusions &con)
{
  char buffer[HUGE_STRING_LENGTH];
  char label_buffer[HUGE_STRING_LENGTH];
  size_t string_len;
  const char *temp_label= opt_label.c_str();

  memset(label_buffer, 0, sizeof(label_buffer));

  if (not opt_label.empty())
  {
    string_len= opt_label.length();

    for (uint32_t x= 0; x < string_len; x++)
    {
      if (temp_label[x] == ',')
        label_buffer[x]= '-';
      else
        label_buffer[x]= temp_label[x] ;
    }
  }
  else if (not opt_auto_generate_sql_type.empty())
  {
    string_len= opt_auto_generate_sql_type.length();

    for (uint32_t x= 0; x < string_len; x++)
    {
      if (opt_auto_generate_sql_type[x] == ',')
        label_buffer[x]= '-';
      else
        label_buffer[x]= opt_auto_generate_sql_type[x] ;
    }
  }
  else
  {
    snprintf(label_buffer, HUGE_STRING_LENGTH, "query");
  }

  snprintf(buffer, HUGE_STRING_LENGTH,
           "%s,%s,%ld.%03ld,%ld.%03ld,%ld.%03ld,%ld.%03ld,%ld.%03ld,"
           "%u,%u,%u,%"PRIu64"\n",
           con.getEngine() ? con.getEngine() : "", /* Storage engine we ran against */
           label_buffer, /* Load type */
           con.getAvgTiming() / 1000, con.getAvgTiming() % 1000, /* Time to load */
           con.getMinTiming() / 1000, con.getMinTiming() % 1000, /* Min time */
           con.getMaxTiming() / 1000, con.getMaxTiming() % 1000, /* Max time */
           con.getSumOfTime() / 1000, con.getSumOfTime() % 1000, /* Total time */
           con.getStdDev() / 1000, con.getStdDev() % 1000, /* Standard Deviation */
           iterations, /* Iterations */
           con.getUsers(), /* Children used max_timing */
           con.getRealUsers(), /* Children used max_timing */
           con.getAvgRows()  /* Queries run */
           );
  size_t buff_len= strlen(buffer);
  ssize_t write_ret= write(csv_file, (unsigned char*) buffer, buff_len);
  if (write_ret != (ssize_t)buff_len)
  {
    fprintf(stderr, _("Unable to fully write %"PRIu64" bytes. "
                      "Could only write %"PRId64"."), (uint64_t)write_ret,
                      (int64_t)buff_len);
    exit(-1);
  }
}

void generate_stats(Conclusions *con, OptionString *eng, Stats *sptr)
{
  Stats *ptr;
  uint32_t x;

  con->setMinTiming(sptr->getTiming());
  con->setMaxTiming(sptr->getTiming());
  con->setMinRows(sptr->getRows());
  con->setMaxRows(sptr->getRows());

  /* At the moment we assume uniform */
  con->setUsers(sptr->getUsers());
  con->setRealUsers(sptr->getRealUsers());
  con->setAvgRows(sptr->getRows());

  /* With no next, we know it is the last element that was malloced */
  for (ptr= sptr, x= 0; x < iterations; ptr++, x++)
  {
    con->setAvgTiming(ptr->getTiming()+con->getAvgTiming());

    if (ptr->getTiming() > con->getMaxTiming())
      con->setMaxTiming(ptr->getTiming());
    if (ptr->getTiming() < con->getMinTiming())
      con->setMinTiming(ptr->getTiming());
  }
  con->setSumOfTime(con->getAvgTiming());
  con->setAvgTiming(con->getAvgTiming()/iterations);

  if (eng && eng->getString())
    con->setEngine(eng->getString());
  else
    con->setEngine(NULL);

  standard_deviation(*con, sptr);

  /* Now we do the create time operations */
  con->setCreateMinTiming(sptr->getCreateTiming());
  con->setCreateMaxTiming(sptr->getCreateTiming());

  /* At the moment we assume uniform */
  con->setCreateCount(sptr->getCreateCount());

  /* With no next, we know it is the last element that was malloced */
  for (ptr= sptr, x= 0; x < iterations; ptr++, x++)
  {
    con->setCreateAvgTiming(ptr->getCreateTiming()+con->getCreateAvgTiming());

    if (ptr->getCreateTiming() > con->getCreateMaxTiming())
      con->setCreateMaxTiming(ptr->getCreateTiming());
    if (ptr->getCreateTiming() < con->getCreateMinTiming())
      con->setCreateMinTiming(ptr->getCreateTiming());
  }
  con->setCreateAvgTiming(con->getCreateAvgTiming()/iterations);
}

void
option_cleanup(OptionString *stmt)
{
  OptionString *ptr, *nptr;
  if (not stmt)
    return;

  for (ptr= stmt; ptr; ptr= nptr)
  {
    nptr= ptr->getNext();
    delete ptr;
  }
}

void statement_cleanup(Statement *stmt)
{
  Statement *ptr, *nptr;
  if (not stmt)
    return;

  for (ptr= stmt; ptr; ptr= nptr)
  {
    nptr= ptr->getNext();
    delete ptr;
  }
}

void slap_close(drizzle_con_st &con)
{
  drizzle_free(drizzle_con_drizzle(&con));
}

void slap_connect(drizzle_con_st &con, bool connect_to_schema)
{
  /* Connect to server */
  static uint32_t connection_retry_sleep= 100000; /* Microseconds */
  int connect_error= 1;
  drizzle_return_t ret;
  drizzle_st *drizzle;

  if (opt_delayed_start)
    usleep(random()%opt_delayed_start);

  if ((drizzle= drizzle_create(NULL)) == NULL ||
      drizzle_con_add_tcp(drizzle, &con, host.c_str(), opt_drizzle_port,
        user.c_str(),
        opt_password.c_str(),
        connect_to_schema ? create_schema_string.c_str() : NULL,
        use_drizzle_protocol ? DRIZZLE_CON_EXPERIMENTAL : DRIZZLE_CON_MYSQL) == NULL)
  {
    fprintf(stderr,"%s: Error creating drizzle object\n", SLAP_NAME);
    abort();
  }

  drizzle_set_context(drizzle, (void*)(connection_count.fetch_and_increment()));

  if (opt_only_print)
    return;

  for (uint32_t x= 0; x < 10; x++)
  {
    if ((ret= drizzle_con_connect(&con)) == DRIZZLE_RETURN_OK)
    {
      /* Connect suceeded */
      connect_error= 0;
      break;
    }
    usleep(connection_retry_sleep);
  }
  if (connect_error)
  {
    fprintf(stderr,"%s: Error when connecting to server: %d %s\n", SLAP_NAME,
            ret, drizzle_con_error(&con));
    abort();
  }
}

void standard_deviation(Conclusions &con, Stats *sptr)
{
  long int sum_of_squares;
  double the_catch;
  Stats *ptr;

  if (iterations == 1 || iterations == 0)
  {
    con.setStdDev(0);
    return;
  }

  uint32_t x;
  for (ptr= sptr, x= 0, sum_of_squares= 0; x < iterations; ptr++, x++)
  {
    long int deviation;

    deviation= ptr->getTiming() - con.getAvgTiming();
    sum_of_squares+= deviation*deviation;
  }

  the_catch= sqrt((double)(sum_of_squares/(iterations -1)));
  con.setStdDev((long int)the_catch);
}
