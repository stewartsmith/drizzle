/* Copyright 2000-2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
 * Copyright (C) 2010 Vijay Samuel
 * Copyright (C) 2010 Andrew Hutchings

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/* drizzledump.cc  - Dump a tables contents and format to an ASCII file

 * Derived from mysqldump, which originally came from:
 **
 ** The author's original notes follow :-
 **
 ** AUTHOR: Igor Romanenko (igor@frog.kiev.ua)
 ** DATE:   December 3, 1994
 ** WARRANTY: None, expressed, impressed, implied
 **          or other
 ** STATUS: Public domain

 * and more work by Monty, Jani & Sinisa
 * and all the MySQL developers over the years.
*/

#include "client_priv.h"
#include <string>
#include <iostream>
#include <stdarg.h>
#include <boost/unordered_set.hpp>
#include <algorithm>
#include <fstream>
#include <drizzled/gettext.h>
#include <drizzled/configmake.h>
#include <drizzled/error.h>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "drizzledump_data.h"
#include "drizzledump_mysql.h"
#include "drizzledump_drizzle.h"

#include "user_detect.h"

using namespace std;
using namespace drizzled;
namespace po= boost::program_options;

/* Exit codes */

#define EX_USAGE 1
#define EX_DRIZZLEERR 2
#define EX_EOF 5 /* ferror for output file was got */

bool  verbose= false;
static bool use_drizzle_protocol= false;
bool ignore_errors= false;
static bool flush_logs= false;
static bool create_options= true; 
static bool opt_quoted= false;
bool opt_databases= false; 
bool opt_alldbs= false; 
static bool opt_lock_all_tables= false;
static bool opt_dump_date= true;
bool opt_autocommit= false; 
static bool opt_single_transaction= false; 
static bool opt_comments;
static bool opt_compact;
bool opt_ignore= false;
bool opt_drop_database;
bool opt_no_create_info;
bool opt_no_data= false;
bool opt_create_db= false;
bool opt_disable_keys= true;
bool extended_insert= true;
bool opt_replace_into= false;
bool opt_drop= true; 
bool opt_data_is_mangled= false;
uint32_t show_progress_size= 0;
static string insert_pat;
static uint32_t opt_drizzle_port= 0;
static int first_error= 0;
static string extended_row;
FILE *md_result_file= 0;
FILE *stderror_file= 0;
std::vector<DrizzleDumpDatabase*> database_store;
DrizzleDumpConnection* db_connection;
DrizzleDumpConnection* destination_connection;

enum destinations {
  DESTINATION_DB,
  DESTINATION_FILES,
  DESTINATION_STDOUT
};

int opt_destination= DESTINATION_STDOUT;
std::string opt_destination_host;
uint16_t opt_destination_port;
std::string opt_destination_user;
std::string opt_destination_password;
std::string opt_destination_database;

const string progname= "drizzledump";

string password,
  enclosed,
  escaped,
  current_host,
  path,
  current_user,
  opt_password,
  opt_protocol,
  where;

boost::unordered_set<string> ignore_table;

void maybe_exit(int error);
static void die(int error, const char* reason, ...);
static void write_header(char *db_name);
static int dump_selected_tables(const string &db, const vector<string> &table_names);
static int dump_databases(const vector<string> &db_names);
static int dump_all_databases(void);
int get_server_type();
void dump_all_tables(void);
void generate_dump(void);
void generate_dump_db(void);

void dump_all_tables(void)
{
  std::vector<DrizzleDumpDatabase*>::iterator i;
  for (i= database_store.begin(); i != database_store.end(); ++i)
  {
    if ((not (*i)->populateTables()) && (not ignore_errors))
      maybe_exit(EX_DRIZZLEERR);
  }
}

void generate_dump(void)
{
  std::vector<DrizzleDumpDatabase*>::iterator i;

  if (path.empty())
  {
    cout << endl << "SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;"
      << endl << "SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;" << endl;
  }

  if (opt_autocommit)
    cout << "SET AUTOCOMMIT=0;" << endl;

  for (i= database_store.begin(); i != database_store.end(); ++i)
  {
    DrizzleDumpDatabase *database= *i;
    cout << *database;
  }

  if (path.empty())
  {
    cout << "SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;"
      << endl << "SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;" << endl;
  }
}

void generate_dump_db(void)
{
  std::vector<DrizzleDumpDatabase*>::iterator i;
  DrizzleStringBuf sbuf(1024);
  try
  {
    destination_connection= new DrizzleDumpConnection(opt_destination_host,
      opt_destination_port, opt_destination_user, opt_destination_password,
      false);
  }
  catch (std::exception&)
  {
    cerr << "Could not connect to destination database server" << endl;
    maybe_exit(EX_DRIZZLEERR);
  }
  sbuf.setConnection(destination_connection);
  std::ostream sout(&sbuf);
  sout.exceptions(ios_base::badbit);

  if (path.empty())
  {
    sout << "SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;" << endl;
    sout << "SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;" << endl;
  }

  if (opt_autocommit)
    cout << "SET AUTOCOMMIT=0;" << endl;

  for (i= database_store.begin(); i != database_store.end(); ++i)
  {
    try
    {
      DrizzleDumpDatabase *database= *i;
      sout << *database;
    }
    catch (std::exception&)
    {
      std::cout << _("Error inserting into destination database") << std::endl;
      if (not ignore_errors)
        maybe_exit(EX_DRIZZLEERR);
    }
  }

  if (path.empty())
  {
    sout << "SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;" << endl;
    sout << "SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;" << endl;
  }
}

/*
  exit with message if ferror(file)

  SYNOPSIS
  check_io()
  file        - checked file
*/

static void check_io(FILE *file)
{
  if (ferror(file))
    die(EX_EOF, _("Got errno %d on write"), errno);
}

static void write_header(char *db_name)
{
  if ((not opt_compact) and (opt_comments))
  {
    cout << "-- drizzledump " << VERSION << " libdrizzle "
      << drizzle_version() << ", for " << HOST_VENDOR << "-" << HOST_OS
      << " (" << HOST_CPU << ")" << endl << "--" << endl;
    cout << "-- Host: " << current_host << "    Database: " << db_name << endl;
    cout << "-- ------------------------------------------------------" << endl;
    cout << "-- Server version\t" << db_connection->getServerVersion();
    if (db_connection->getServerType() == ServerDetect::SERVER_MYSQL_FOUND)
      cout << " (MySQL server)";
    else if (db_connection->getServerType() == ServerDetect::SERVER_DRIZZLE_FOUND)
      cout << " (Drizzle server)";
    cout << endl << endl;
  }

} /* write_header */


static void write_footer(FILE *sql_file)
{
  if (! opt_compact)
  {
    if (opt_comments)
    {
      if (opt_dump_date)
      {
        boost::posix_time::ptime time(boost::posix_time::second_clock::local_time());
        fprintf(sql_file, "-- Dump completed on %s\n",
          boost::posix_time::to_simple_string(time).c_str());
      }
      else
        fprintf(sql_file, "-- Dump completed\n");
    }
    check_io(sql_file);
  }
} /* write_footer */

static int get_options(void)
{
  if (opt_single_transaction && opt_lock_all_tables)
  {
    fprintf(stderr, _("%s: You can't use --single-transaction and "
                      "--lock-all-tables at the same time.\n"), progname.c_str());
    return(EX_USAGE);
  }
  if ((opt_databases || opt_alldbs) && ! path.empty())
  {
    fprintf(stderr,
            _("%s: --databases or --all-databases can't be used with --tab.\n"),
            progname.c_str());
    return(EX_USAGE);
  }

  if (tty_password)
    opt_password=client_get_tty_password(NULL);
  return(0);
} /* get_options */


/*
  Prints out an error message and kills the process.

  SYNOPSIS
  die()
  error_num   - process return value
  fmt_reason  - a format string for use by vsnprintf.
  ...         - variable arguments for above fmt_reason string

  DESCRIPTION
  This call prints out the formatted error message to stderr and then
  terminates the process.
*/
static void die(int error_num, const char* fmt_reason, ...)
{
  char buffer[1000];
  va_list args;
  va_start(args,fmt_reason);
  vsnprintf(buffer, sizeof(buffer), fmt_reason, args);
  va_end(args);

  fprintf(stderr, "%s: %s\n", progname.c_str(), buffer);
  fflush(stderr);

  ignore_errors= 0; /* force the exit */
  maybe_exit(error_num);
}

static void free_resources(void)
{
  if (md_result_file && md_result_file != stdout)
    fclose(md_result_file);
  opt_password.erase();
}


void maybe_exit(int error)
{
  if (!first_error)
    first_error= error;
  if (ignore_errors)
    return;
  delete db_connection;
  delete destination_connection;
  free_resources();
  exit(error);
}

static int dump_all_databases()
{
  drizzle_row_t row;
  drizzle_result_st *tableres;
  int result=0;
  std::string query;
  DrizzleDumpDatabase *database;

  if (verbose)
    std::cerr << _("-- Retrieving database structures...") << std::endl;

  /* Blocking the MySQL privilege tables too because we can't import them due to bug#646187 */
  if (db_connection->getServerType() == ServerDetect::SERVER_MYSQL_FOUND)
    query= "SELECT SCHEMA_NAME, DEFAULT_COLLATION_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME NOT IN ('information_schema', 'performance_schema', 'mysql')";
  else
    query= "SELECT SCHEMA_NAME, DEFAULT_COLLATION_NAME FROM DATA_DICTIONARY.SCHEMAS WHERE SCHEMA_NAME NOT IN ('information_schema','data_dictionary')";

  tableres= db_connection->query(query);
  while ((row= drizzle_row_next(tableres)))
  {
    std::string database_name(row[0]);
    if (db_connection->getServerType() == ServerDetect::SERVER_MYSQL_FOUND)
      database= new DrizzleDumpDatabaseMySQL(database_name, db_connection);
    else
      database= new DrizzleDumpDatabaseDrizzle(database_name, db_connection);

    database->setCollate(row[1]);
    database_store.push_back(database);
  }
  db_connection->freeResult(tableres);
  return result;
}
/* dump_all_databases */


static int dump_databases(const vector<string> &db_names)
{
  int result=0;
  string temp;
  DrizzleDumpDatabase *database;

  for (vector<string>::const_iterator it= db_names.begin(); it != db_names.end(); ++it)
  {
    temp= *it;
    if (db_connection->getServerType() == ServerDetect::SERVER_MYSQL_FOUND)
      database= new DrizzleDumpDatabaseMySQL(temp, db_connection);
    else
      database= new DrizzleDumpDatabaseDrizzle(temp, db_connection);
    database_store.push_back(database);
  }
  return(result);
} /* dump_databases */

static int dump_selected_tables(const string &db, const vector<string> &table_names)
{
  DrizzleDumpDatabase *database;

  if (db_connection->getServerType() == ServerDetect::SERVER_MYSQL_FOUND)
    database= new DrizzleDumpDatabaseMySQL(db, db_connection);
  else
    database= new DrizzleDumpDatabaseDrizzle(db, db_connection);

  if (not database->populateTables(table_names))
  {
    delete database;
    if (not ignore_errors)
      maybe_exit(EX_DRIZZLEERR);
  }

  database_store.push_back(database); 

  return 0;
} /* dump_selected_tables */

static int do_flush_tables_read_lock()
{
  /*
    We do first a FLUSH TABLES. If a long update is running, the FLUSH TABLES
    will wait but will not stall the whole mysqld, and when the long update is
    done the FLUSH TABLES WITH READ LOCK will start and succeed quickly. So,
    FLUSH TABLES is to lower the probability of a stage where both drizzled
    and most client connections are stalled. Of course, if a second long
    update starts between the two FLUSHes, we have that bad stall.
  */

   db_connection->queryNoResult("FLUSH TABLES");
   db_connection->queryNoResult("FLUSH TABLES WITH READ LOCK");

  return 0;
}

static int start_transaction()
{
  db_connection->queryNoResult("SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ");
  db_connection->queryNoResult("START TRANSACTION WITH CONSISTENT SNAPSHOT");

  if (db_connection->getServerType() == ServerDetect::SERVER_DRIZZLE_FOUND)
  {
    drizzle_result_st *result;
    drizzle_row_t row;
    std::string query("SELECT COMMIT_ID, ID FROM DATA_DICTIONARY.SYS_REPLICATION_LOG WHERE COMMIT_ID=(SELECT MAX(COMMIT_ID) FROM DATA_DICTIONARY.SYS_REPLICATION_LOG)");
    result= db_connection->query(query);
    if ((row= drizzle_row_next(result)))
    {
      cout << "-- SYS_REPLICATION_LOG: COMMIT_ID = " << row[0] << ", ID = " << row[1] << endl << endl;
    }
    db_connection->freeResult(result);
  }

  return 0;
}

int main(int argc, char **argv)
{
try
{
  int exit_code;

#if defined(ENABLE_NLS)
# if defined(HAVE_LOCALE_H)
  setlocale(LC_ALL, "");
# endif
  bindtextdomain("drizzle7", LOCALEDIR);
  textdomain("drizzle7");
#endif

  po::options_description commandline_options(_("Options used only in command line"));
  commandline_options.add_options()
  ("all-databases,A", po::value<bool>(&opt_alldbs)->default_value(false)->zero_tokens(),
  _("Dump all the databases. This will be same as --databases with all databases selected."))
  ("flush-logs,F", po::value<bool>(&flush_logs)->default_value(false)->zero_tokens(),
  _("Flush logs file in server before starting dump. Note that if you dump many databases at once (using the option --databases= or --all-databases), the logs will be flushed for each database dumped. The exception is when using --lock-all-tables in this case the logs will be flushed only once, corresponding to the moment all tables are locked. So if you want your dump and the log flush to happen at the same exact moment you should use --lock-all-tables or --flush-logs"))
  ("force,f", po::value<bool>(&ignore_errors)->default_value(false)->zero_tokens(),
  _("Continue even if we get an sql-error."))
  ("help,?", _("Display this help message and exit."))
  ("lock-all-tables,x", po::value<bool>(&opt_lock_all_tables)->default_value(false)->zero_tokens(),
  _("Locks all tables across all databases. This is achieved by taking a global read lock for the duration of the whole dump. Automatically turns --single-transaction off."))
  ("single-transaction", po::value<bool>(&opt_single_transaction)->default_value(false)->zero_tokens(),
  _("Creates a consistent snapshot by dumping all tables in a single transaction. Works ONLY for tables stored in storage engines which support multiversioning (currently only InnoDB does); the dump is NOT guaranteed to be consistent for other storage engines. While a --single-transaction dump is in process, to ensure a valid dump file (correct table contents), no other connection should use the following statements: ALTER TABLE, DROP TABLE, RENAME TABLE, TRUNCATE TABLE, as consistent snapshot is not isolated from them."))
  ("skip-opt", 
  _("Disable --opt. Disables --add-drop-table, --add-locks, --create-options, ---extended-insert and --disable-keys."))    
  ("tables", _("Overrides option --databases (-B)."))
  ("show-progress-size", po::value<uint32_t>(&show_progress_size)->default_value(10000),
  _("Number of rows before each output progress report (requires --verbose)."))
  ("verbose,v", po::value<bool>(&verbose)->default_value(false)->zero_tokens(),
  _("Print info about the various stages."))
  ("version,V", _("Output version information and exit."))
  ("skip-comments", _("Turn off Comments"))
  ("skip-create", _("Turn off create-options"))
  ("skip-extended-insert", _("Turn off extended-insert"))
  ("skip-dump-date", _( "Turn off dump date at the end of the output"))
  ("no-defaults", _("Do not read from the configuration files"))
  ;

  po::options_description dump_options(_("Options specific to the drizzle client"));
  dump_options.add_options()
  ("add-drop-database", po::value<bool>(&opt_drop_database)->default_value(false)->zero_tokens(),
  _("Add a 'DROP DATABASE' before each create."))
  ("skip-drop-table", _("Do not add a 'drop table' before each create."))
  ("compact", po::value<bool>(&opt_compact)->default_value(false)->zero_tokens(),
  _("Give less verbose output (useful for debugging). Disables structure comments and header/footer constructs.  Enables options --skip-add-drop-table --no-set-names --skip-disable-keys"))
  ("databases,B", po::value<bool>(&opt_databases)->default_value(false)->zero_tokens(),
  _("To dump several databases. Note the difference in usage; In this case no tables are given. All name arguments are regarded as databasenames. 'USE db_name;' will be included in the output."))
  ("skip-disable-keys,K",
  _("'ALTER TABLE tb_name DISABLE KEYS;' and 'ALTER TABLE tb_name ENABLE KEYS;' will not be put in the output."))
  ("ignore-table", po::value<string>(),
  _("Do not dump the specified table. To specify more than one table to ignore, use the directive multiple times, once for each table.  Each table must be specified with both database and table names, e.g. --ignore-table=database.table"))
  ("insert-ignore", po::value<bool>(&opt_ignore)->default_value(false)->zero_tokens(),
  _("Insert rows with INSERT IGNORE."))
  ("no-autocommit", po::value<bool>(&opt_autocommit)->default_value(false)->zero_tokens(),
  _("Wrap a table's data in START TRANSACTION/COMMIT statements."))
  ("no-create-db,n", po::value<bool>(&opt_create_db)->default_value(false)->zero_tokens(),
  _("'CREATE DATABASE IF NOT EXISTS db_name;' will not be put in the output. The above line will be added otherwise, if --databases or --all-databases option was given."))
  ("no-data,d", po::value<bool>(&opt_no_data)->default_value(false)->zero_tokens(),
  _("No row information."))
  ("replace", po::value<bool>(&opt_replace_into)->default_value(false)->zero_tokens(),
  _("Use REPLACE INTO instead of INSERT INTO."))
  ("destination-type", po::value<string>()->default_value("stdout"),
  _("Where to send output to (stdout|database"))
  ("destination-host", po::value<string>(&opt_destination_host)->default_value("localhost"),
  _("Hostname for destination db server (requires --destination-type=database)"))
  ("destination-port", po::value<uint16_t>(&opt_destination_port)->default_value(4427),
  _("Port number for destination db server (requires --destination-type=database)"))
  ("destination-user", po::value<string>(&opt_destination_user),
  _("User name for destination db server (resquires --destination-type=database)"))
  ("destination-password", po::value<string>(&opt_destination_password),
  _("Password for destination db server (requires --destination-type=database)"))
  ("destination-database", po::value<string>(&opt_destination_database),
  _("The database in the destination db server (requires --destination-type=database, not for use with --all-databases)"))
  ("my-data-is-mangled", po::value<bool>(&opt_data_is_mangled)->default_value(false)->zero_tokens(),
  _("Do not make a UTF8 connection to MySQL, use if you have UTF8 data in a non-UTF8 table"))
  ;

  po::options_description client_options(_("Options specific to the client"));
  client_options.add_options()
  ("host,h", po::value<string>(&current_host)->default_value("localhost"),
  _("Connect to host."))
  ("password,P", po::value<string>(&password)->default_value(PASSWORD_SENTINEL),
  _("Password to use when connecting to server. If password is not given it's solicited on the tty."))
  ("port,p", po::value<uint32_t>(&opt_drizzle_port)->default_value(0),
  _("Port number to use for connection."))
  ("user,u", po::value<string>(&current_user)->default_value(UserDetect().getUser()),
  _("User for login if not current user."))
  ("protocol",po::value<string>(&opt_protocol)->default_value("mysql"),
  _("The protocol of connection (mysql or drizzle)."))
  ;

  po::options_description hidden_options(_("Hidden Options"));
  hidden_options.add_options()
  ("database-used", po::value<vector<string> >(), _("Used to select the database"))
  ("Table-used", po::value<vector<string> >(), _("Used to select the tables"))
  ;

  po::options_description all_options(_("Allowed Options + Hidden Options"));
  all_options.add(commandline_options).add(dump_options).add(client_options).add(hidden_options);

  po::options_description long_options(_("Allowed Options"));
  long_options.add(commandline_options).add(dump_options).add(client_options);

  std::string system_config_dir_dump(SYSCONFDIR); 
  system_config_dir_dump.append("/drizzle/drizzledump.cnf");

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

  po::positional_options_description p;
  p.add("database-used", 1);
  p.add("Table-used",-1);

  md_result_file= stdout;

  po::variables_map vm;

  // Disable allow_guessing
  int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;

  po::store(po::command_line_parser(argc, argv).style(style).
            options(all_options).positional(p).
            extra_parser(parse_password_arg).run(), vm);

  if (! vm.count("no-defaults"))
  {
    std::string user_config_dir_dump(user_config_dir);
    user_config_dir_dump.append("/drizzle/drizzledump.cnf"); 

    std::string user_config_dir_client(user_config_dir);
    user_config_dir_client.append("/drizzle/client.cnf");

    ifstream user_dump_ifs(user_config_dir_dump.c_str());
    po::store(parse_config_file(user_dump_ifs, dump_options), vm);

    ifstream user_client_ifs(user_config_dir_client.c_str());
    po::store(parse_config_file(user_client_ifs, client_options), vm);

    ifstream system_dump_ifs(system_config_dir_dump.c_str());
    po::store(parse_config_file(system_dump_ifs, dump_options), vm);

    ifstream system_client_ifs(system_config_dir_client.c_str());
    po::store(parse_config_file(system_client_ifs, client_options), vm);
  }

  po::notify(vm);  
  
  if ((not vm.count("database-used") && not vm.count("Table-used") 
    && not opt_alldbs && path.empty())
    || (vm.count("help")) || vm.count("version"))
  {
    printf(_("Drizzledump %s build %s, for %s-%s (%s)\n"),
      drizzle_version(), VERSION, HOST_VENDOR, HOST_OS, HOST_CPU);
    if (vm.count("version"))
      exit(0);
    puts("");
    puts(_("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n"));
    puts(_("Dumps definitions and data from a Drizzle database server"));
    printf(_("Usage: %s [OPTIONS] database [tables]\n"), progname.c_str());
    printf(_("OR     %s [OPTIONS] --databases [OPTIONS] DB1 [DB2 DB3...]\n"),
          progname.c_str());
    printf(_("OR     %s [OPTIONS] --all-databases [OPTIONS]\n"), progname.c_str());
    cout << long_options;
    if (vm.count("help"))
      exit(0);
    else
      exit(1);
  }

  /* Inverted Booleans */

  opt_drop= not vm.count("skip-drop-table");
  opt_comments= not vm.count("skip-comments");
  extended_insert= not vm.count("skip-extended-insert");
  opt_dump_date= not vm.count("skip-dump-date");
  opt_disable_keys= not vm.count("skip-disable-keys");
  opt_quoted= not vm.count("skip-quote-names");

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
     *        This also helps with potential data loss casting unsigned long to a
     *               uint32_t. 
     */
    if (opt_drizzle_port > 65535)
    {
      fprintf(stderr, _("Value supplied for port is not valid.\n"));
      exit(-1);
    }
  }

  if (vm.count("password"))
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

  if (! path.empty())
  { 
    opt_disable_keys= 0;
  }

  if (vm.count("skip-opt"))
  {
    extended_insert= opt_drop= create_options= 0;
    opt_disable_keys= 0;
  }

  if (opt_compact)
  { 
    opt_comments= opt_drop= opt_disable_keys= 0;
  }

  if (vm.count("opt"))
  {
    extended_insert= opt_drop= create_options= 1;
    opt_disable_keys= 1;
  }

  if (vm.count("tables"))
  { 
    opt_databases= false;
  }

  if (vm.count("ignore-table"))
  {
    if (!strchr(vm["ignore-table"].as<string>().c_str(), '.'))
    {
      fprintf(stderr, _("Illegal use of option --ignore-table=<database>.<table>\n"));
      exit(EXIT_ARGUMENT_INVALID);
    }
    string tmpptr(vm["ignore-table"].as<string>());
    ignore_table.insert(tmpptr); 
  }

  if (vm.count("skip-create"))
  {
    opt_create_db= opt_no_create_info= create_options= false;
  }
 
  exit_code= get_options();
  if (exit_code)
  {
    free_resources();
    exit(exit_code);
  }
  try
  {
    db_connection = new DrizzleDumpConnection(current_host, opt_drizzle_port,
      current_user, opt_password, use_drizzle_protocol);
  }
  catch (std::exception&)
  {
    maybe_exit(EX_DRIZZLEERR);
  }

  if ((db_connection->getServerType() == ServerDetect::SERVER_MYSQL_FOUND) and (not opt_data_is_mangled))
    db_connection->queryNoResult("SET NAMES 'utf8'");

  if (vm.count("destination-type"))
  {
    string tmp_destination(vm["destination-type"].as<string>());
    if (tmp_destination.compare("database") == 0)
      opt_destination= DESTINATION_DB;
    else if (tmp_destination.compare("stdout") == 0)
      opt_destination= DESTINATION_STDOUT;
    else
      exit(EXIT_ARGUMENT_INVALID);
  }


  if (path.empty() && vm.count("database-used"))
  {
    string database_used= *vm["database-used"].as< vector<string> >().begin();
    write_header((char *)database_used.c_str());
  }

  if ((opt_lock_all_tables) && do_flush_tables_read_lock())
    goto err;
  if (opt_single_transaction && start_transaction())
    goto err;
  if (opt_lock_all_tables)
    db_connection->queryNoResult("FLUSH LOGS");

  if (opt_alldbs)
  {
    dump_all_databases();
    dump_all_tables();
  }
  if (vm.count("database-used") && vm.count("Table-used") && not opt_databases)
  {
    string database_used= *vm["database-used"].as< vector<string> >().begin();
    /* Only one database and selected table(s) */
    dump_selected_tables(database_used, vm["Table-used"].as< vector<string> >());
  }

  if (vm.count("Table-used") && opt_databases)
  {
    vector<string> database_used= vm["database-used"].as< vector<string> >();
    vector<string> table_used= vm["Table-used"].as< vector<string> >();

    for (vector<string>::iterator it= table_used.begin();
       it != table_used.end();
       ++it)
    {
      database_used.insert(database_used.end(), *it);
    }

    dump_databases(database_used);
    dump_all_tables();
  }

  if (vm.count("database-used") && not vm.count("Table-used"))
  {
    dump_databases(vm["database-used"].as< vector<string> >());
    dump_all_tables();
  }

  if (opt_destination == DESTINATION_STDOUT)
    generate_dump();
  else
    generate_dump_db();

  /* ensure dumped data flushed */
  if (md_result_file && fflush(md_result_file))
  {
    if (!first_error)
      first_error= EX_DRIZZLEERR;
    goto err;
  }

  /*
    No reason to explicitely COMMIT the transaction, neither to explicitely
    UNLOCK TABLES: these will be automatically be done by the server when we
    disconnect now. Saves some code here, some network trips, adds nothing to
    server.
  */
err:
  delete db_connection;
  delete destination_connection;
  if (path.empty())
    write_footer(md_result_file);
  free_resources();

  if (stderror_file)
    fclose(stderror_file);
}

  catch(exception &err)
  {
    cerr << err.what() << endl;
  }
  
  return(first_error);
} /* main */
