/* 
  Copyright (C) 2010 Vijay Samuel
  Copyright (C) 2010 Brian Aker
  Copyright (C) 2000-2006 MySQL AB
  Copyright (C) 2008-2009 Sun Microsystems, Inc.

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

#define IMPORT_VERSION "4.0"

#include "client_priv.h"
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include <pthread.h>

#include <drizzled/definitions.h>
#include <drizzled/internal/my_sys.h>
/* Added this for string translation. */
#include <drizzled/gettext.h>
#include <drizzled/configmake.h>

#include "user_detect.h"

namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

extern "C" void * worker_thread(void *arg);

int exitcode= 0;

const char *program_name= "drizzleimport";

/* Global Thread counter */
uint32_t counter;
pthread_mutex_t counter_mutex;
pthread_cond_t count_threshhold;

static void db_error(drizzle_con_st *con, drizzle_result_st *result,
                     drizzle_return_t ret, char *table);
static char *field_escape(char *to,const char *from,uint32_t length);
static char *add_load_option(char *ptr,const char *object,
           const char *statement);

static bool verbose= false, ignore_errors= false,
            opt_delete= false, opt_replace= false, silent= false,
            ignore_unique= false, opt_low_priority= false,
            use_drizzle_protocol= false, opt_local_file;

static uint32_t opt_use_threads;
static uint32_t opt_drizzle_port= 0;
static int64_t opt_ignore_lines= -1;

std::string opt_columns,
  opt_enclosed,
  escaped,
  password,
  current_db,
  lines_terminated,
  current_user,
  opt_password,
  enclosed,  
  current_host,
  fields_terminated,
  opt_protocol;


static int get_options(void)
{

  if (! enclosed.empty() && ! opt_enclosed.empty())
  {
    fprintf(stderr, "You can't use ..enclosed.. and ..optionally-enclosed.. at the same time.\n");
    return(1);
  }
  if (opt_replace && ignore_unique)
  {
    fprintf(stderr, "You can't use --ignore_unique (-i) and --replace (-r) at the same time.\n");
    return(1);
  }

  if (tty_password)
    opt_password=client_get_tty_password(NULL);
  return(0);
}



static int write_to_table(char *filename, drizzle_con_st *con)
{
  char tablename[FN_REFLEN], hard_path[FN_REFLEN],
       sql_statement[FN_REFLEN*16+256], *end;
  drizzle_result_st result;
  drizzle_return_t ret;

  internal::fn_format(tablename, filename, "", "", 1 | 2); /* removes path & ext. */
  if (not opt_local_file)
    strcpy(hard_path,filename);
  else
    internal::my_load_path(hard_path, filename, NULL); /* filename includes the path */

  if (opt_delete)
  {
    if (verbose)
      fprintf(stdout, "Deleting the old data from table %s\n", tablename);
#ifdef HAVE_SNPRINTF
    snprintf(sql_statement, sizeof(sql_statement), "DELETE FROM %s", tablename);
#else
    snprintf(sql_statement, sizeof(sql_statement), "DELETE FROM %s", tablename);
#endif
    if (drizzle_query_str(con, &result, sql_statement, &ret) == NULL ||
        ret != DRIZZLE_RETURN_OK)
    {
      db_error(con, &result, ret, tablename);
      return(1);
    }
    drizzle_result_free(&result);
  }
  if (verbose)
  {
    if (opt_local_file)
      fprintf(stdout, "Loading data from LOCAL file: %s into %s\n",
        hard_path, tablename);
    else
      fprintf(stdout, "Loading data from SERVER file: %s into %s\n",
        hard_path, tablename);
  }
  snprintf(sql_statement, sizeof(sql_statement), "LOAD DATA %s %s INFILE '%s'",
    opt_low_priority ? "LOW_PRIORITY" : "",
    opt_local_file ? "LOCAL" : "", hard_path);
  end= strchr(sql_statement, '\0');
  if (opt_replace)
    end= strcpy(end, " REPLACE")+8;
  if (ignore_unique)
    end= strcpy(end, " IGNORE")+7;

  end+= sprintf(end, " INTO TABLE %s", tablename);

  if (! fields_terminated.empty() || ! enclosed.empty() || ! opt_enclosed.empty() || ! escaped.empty())
      end= strcpy(end, " FIELDS")+7;
  end= add_load_option(end, (char *)fields_terminated.c_str(), " TERMINATED BY");
  end= add_load_option(end, (char *)enclosed.c_str(), " ENCLOSED BY");
  end= add_load_option(end, (char *)opt_enclosed.c_str(),
           " OPTIONALLY ENCLOSED BY");
  end= add_load_option(end, (char *)escaped.c_str(), " ESCAPED BY");
  end= add_load_option(end, (char *)lines_terminated.c_str(), " LINES TERMINATED BY");
  if (opt_ignore_lines >= 0)
  {
    end= strcpy(end, " IGNORE ")+8;
    ostringstream buffer;
    buffer << opt_ignore_lines;
    end= strcpy(end, buffer.str().c_str())+ buffer.str().size();
    end= strcpy(end, " LINES")+6;
  }
  if (! opt_columns.empty())
  {
    end= strcpy(end, " (")+2;
    end= strcpy(end, (char *)opt_columns.c_str()+opt_columns.length());
    end= strcpy(end, ")")+1;
  }
  *end= '\0';

  if (drizzle_query_str(con, &result, sql_statement, &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    db_error(con, &result, ret, tablename);
    return(1);
  }
  if (!silent)
  {
    if (strcmp(drizzle_result_info(&result), ""))
    {
      fprintf(stdout, "%s.%s: %s\n", current_db.c_str(), tablename,
        drizzle_result_info(&result));
    }
  }
  drizzle_result_free(&result);
  return(0);
}


static drizzle_con_st *db_connect(const string host, const string database,
                                  const string user, const string passwd)
{
  drizzle_st *drizzle;
  drizzle_con_st *con;
  drizzle_return_t ret;

  if (verbose)
    fprintf(stdout, "Connecting to %s, using protocol %s...\n", ! host.empty() ? host.c_str() : "localhost", opt_protocol.c_str());
  if (!(drizzle= drizzle_create(NULL)))
    return 0;
  if (!(con= drizzle_con_add_tcp(drizzle,NULL,(char *)host.c_str(),opt_drizzle_port,(char *)user.c_str(),(char *)passwd.c_str(),
                                 (char *)database.c_str(), use_drizzle_protocol ? DRIZZLE_CON_EXPERIMENTAL : DRIZZLE_CON_MYSQL)))
  {
    return 0;
  }

  if ((ret= drizzle_con_connect(con)) != DRIZZLE_RETURN_OK)
  {
    ignore_errors=0;    /* NO RETURN FROM db_error */
    db_error(con, NULL, ret, NULL);
  }

  if (verbose)
    fprintf(stdout, "Selecting database %s\n", database.c_str());

  return con;
}



static void db_disconnect(const string host, drizzle_con_st *con)
{
  if (verbose)
    fprintf(stdout, "Disconnecting from %s\n", ! host.empty() ? host.c_str() : "localhost");
  drizzle_free(drizzle_con_drizzle(con));
}



static void safe_exit(int error, drizzle_con_st *con)
{
  if (ignore_errors)
    return;
  if (con)
    drizzle_free(drizzle_con_drizzle(con));
  exit(error);
}



static void db_error(drizzle_con_st *con, drizzle_result_st *result,
                     drizzle_return_t ret, char *table)
{
  if (ret == DRIZZLE_RETURN_ERROR_CODE)
  {
    fprintf(stdout, "Error: %d, %s%s%s",
            drizzle_result_error_code(result),
            drizzle_result_error(result),
            table ? ", when using table: " : "", table ? table : "");
    drizzle_result_free(result);
  }
  else
  {
    fprintf(stdout, "Error: %d, %s%s%s", ret, drizzle_con_error(con),
            table ? ", when using table: " : "", table ? table : "");
  }

  safe_exit(1, con);
}


static char *add_load_option(char *ptr, const char *object,
           const char *statement)
{
  if (object)
  {
    /* Don't escape hex constants */
    if (object[0] == '0' && (object[1] == 'x' || object[1] == 'X'))
      ptr+= sprintf(ptr, " %s %s", statement, object);
    else
    {
      /* char constant; escape */
      ptr+= sprintf(ptr, " %s '", statement); 
      ptr= field_escape(ptr,object,(uint32_t) strlen(object));
      *ptr++= '\'';
    }
  }
  return ptr;
}

/*
** Allow the user to specify field terminator strings like:
** "'", "\", "\\" (escaped backslash), "\t" (tab), "\n" (newline)
** This is done by doubleing ' and add a end -\ if needed to avoid
** syntax errors from the SQL parser.
*/

static char *field_escape(char *to,const char *from,uint32_t length)
{
  const char *end;
  uint32_t end_backslashes=0;

  for (end= from+length; from != end; from++)
  {
    *to++= *from;
    if (*from == '\\')
      end_backslashes^=1;    /* find odd number of backslashes */
    else
    {
      if (*from == '\'' && !end_backslashes)
  *to++= *from;      /* We want a dublicate of "'" for DRIZZLE */
      end_backslashes=0;
    }
  }
  /* Add missing backslashes if user has specified odd number of backs.*/
  if (end_backslashes)
    *to++= '\\';
  return to;
}

void * worker_thread(void *arg)
{
  int error;
  char *raw_table_name= (char *)arg;
  drizzle_con_st *con;

  if (!(con= db_connect(current_host,current_db,current_user,opt_password)))
  {
    return 0;
  }

  /*
    We are not currently catching the error here.
  */
  if ((error= write_to_table(raw_table_name, con)))
  {
    if (exitcode == 0)
    {
      exitcode= error;
    }
  }

  if (con)
  {
    db_disconnect(current_host, con);
  }

  pthread_mutex_lock(&counter_mutex);
  counter--;
  pthread_cond_signal(&count_threshhold);
  pthread_mutex_unlock(&counter_mutex);

  return 0;
}


int main(int argc, char **argv)
{
try
{
  int error=0;

  po::options_description commandline_options("Options used only in command line");
  commandline_options.add_options()

  ("debug,#", po::value<string>(),
  "Output debug log. Often this is 'd:t:o,filename'.")
  ("delete,d", po::value<bool>(&opt_delete)->default_value(false)->zero_tokens(),
  "First delete all rows from table.")
  ("help,?", "Displays this help and exits.")
  ("ignore,i", po::value<bool>(&ignore_unique)->default_value(false)->zero_tokens(),
  "If duplicate unique key was found, keep old row.")
  ("low-priority", po::value<bool>(&opt_low_priority)->default_value(false)->zero_tokens(),
  "Use LOW_PRIORITY when updating the table.")
  ("replace,r", po::value<bool>(&opt_replace)->default_value(false)->zero_tokens(),
  "If duplicate unique key was found, replace old row.")
  ("verbose,v", po::value<bool>(&verbose)->default_value(false)->zero_tokens(),
  "Print info about the various stages.")
  ("version,V", "Output version information and exit.")
  ;

  po::options_description import_options("Options specific to the drizzleimport");
  import_options.add_options()
  ("columns,C", po::value<string>(&opt_columns)->default_value(""),
  "Use only these columns to import the data to. Give the column names in a comma separated list. This is same as giving columns to LOAD DATA INFILE.")
  ("fields-terminated-by", po::value<string>(&fields_terminated)->default_value(""),
  "Fields in the textfile are terminated by ...")
  ("fields-enclosed-by", po::value<string>(&enclosed)->default_value(""),
  "Fields in the importfile are enclosed by ...")
  ("fields-optionally-enclosed-by", po::value<string>(&opt_enclosed)->default_value(""),
  "Fields in the i.file are opt. enclosed by ...")
  ("fields-escaped-by", po::value<string>(&escaped)->default_value(""),
  "Fields in the i.file are escaped by ...")
  ("force,f", po::value<bool>(&ignore_errors)->default_value(false)->zero_tokens(),
  "Continue even if we get an sql-error.")
  ("ignore-lines", po::value<int64_t>(&opt_ignore_lines)->default_value(0),
  "Ignore first n lines of data infile.")
  ("lines-terminated-by", po::value<string>(&lines_terminated)->default_value(""),
  "Lines in the i.file are terminated by ...")
  ("local,L", po::value<bool>(&opt_local_file)->default_value(false)->zero_tokens(),
  "Read all files through the client.")
  ("silent,s", po::value<bool>(&silent)->default_value(false)->zero_tokens(),
  "Be more silent.")
  ("use-threads", po::value<uint32_t>(&opt_use_threads)->default_value(4),
  "Load files in parallel. The argument is the number of threads to use for loading data (default is 4.")
  ;

  po::options_description client_options("Options specific to the client");
  client_options.add_options()
  ("host,h", po::value<string>(&current_host)->default_value("localhost"),
  "Connect to host.")
  ("password,P", po::value<string>(&password),
  "Password to use when connecting to server. If password is not given it's asked from the tty." )
  ("port,p", po::value<uint32_t>(&opt_drizzle_port)->default_value(0),
  "Port number to use for connection") 
  ("protocol", po::value<string>(&opt_protocol)->default_value("mysql"),
  "The protocol of connection (mysql or drizzle).")
  ("user,u", po::value<string>(&current_user)->default_value(UserDetect().getUser()),
  "User for login if not current user.")
  ;

  po::options_description long_options("Allowed Options");
  long_options.add(commandline_options).add(import_options).add(client_options);

  std::string system_config_dir_import(SYSCONFDIR); 
  system_config_dir_import.append("/drizzle/drizzleimport.cnf");

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

  // Disable allow_guessing
  int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;

  po::store(po::command_line_parser(argc, argv).options(long_options).
            style(style).extra_parser(parse_password_arg).run(), vm);

  std::string user_config_dir_import(user_config_dir);
  user_config_dir_import.append("/drizzle/drizzleimport.cnf"); 

  std::string user_config_dir_client(user_config_dir);
  user_config_dir_client.append("/drizzle/client.cnf");

  ifstream user_import_ifs(user_config_dir_import.c_str());
  po::store(parse_config_file(user_import_ifs, import_options), vm);

  ifstream user_client_ifs(user_config_dir_client.c_str());
  po::store(parse_config_file(user_client_ifs, client_options), vm);

  ifstream system_import_ifs(system_config_dir_import.c_str());
  store(parse_config_file(system_import_ifs, import_options), vm);
 
  ifstream system_client_ifs(system_config_dir_client.c_str());
  po::store(parse_config_file(system_client_ifs, client_options), vm);

  po::notify(vm);
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
    if (opt_drizzle_port > 65535)
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


  if (vm.count("version"))
  {
    printf("%s  Ver %s Distrib %s, for %s-%s (%s)\n", program_name,
    IMPORT_VERSION, drizzle_version(),HOST_VENDOR,HOST_OS,HOST_CPU);
  }
  
  if (vm.count("help") || argc < 2)
  {
    printf("%s  Ver %s Distrib %s, for %s-%s (%s)\n", program_name,
    IMPORT_VERSION, drizzle_version(),HOST_VENDOR,HOST_OS,HOST_CPU);
    puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
    printf("\
    Loads tables from text files in various formats.  The base name of the\n\
    text file must be the name of the table that should be used.\n\
    If one uses sockets to connect to the Drizzle server, the server will open and\n\
    read the text file directly. In other cases the client will open the text\n\
    file. The SQL command 'LOAD DATA INFILE' is used to import the rows.\n");

    printf("\nUsage: %s [OPTIONS] database textfile...", program_name);
    cout<<long_options;
    exit(0);
  }


  if (get_options())
  {
    return(1);
  }
  
  current_db= (*argv)++;
  argc--;

  if (opt_use_threads)
  {
    pthread_t mainthread;            /* Thread descriptor */
    pthread_attr_t attr;          /* Thread attributes */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,
                                PTHREAD_CREATE_DETACHED);

    pthread_mutex_init(&counter_mutex, NULL);
    pthread_cond_init(&count_threshhold, NULL);

    for (counter= 0; *argv != NULL; argv++) /* Loop through tables */
    {
      pthread_mutex_lock(&counter_mutex);
      while (counter == opt_use_threads)
      {
        struct timespec abstime;

        set_timespec(abstime, 3);
        pthread_cond_timedwait(&count_threshhold, &counter_mutex, &abstime);
      }
      /* Before exiting the lock we set ourselves up for the next thread */
      counter++;
      pthread_mutex_unlock(&counter_mutex);
      /* now create the thread */
      if (pthread_create(&mainthread, &attr, worker_thread,
                         (void *)*argv) != 0)
      {
        pthread_mutex_lock(&counter_mutex);
        counter--;
        pthread_mutex_unlock(&counter_mutex);
        fprintf(stderr,"%s: Could not create thread\n", program_name);
      }
    }

    /*
      We loop until we know that all children have cleaned up.
    */
    pthread_mutex_lock(&counter_mutex);
    while (counter)
    {
      struct timespec abstime;

      set_timespec(abstime, 3);
      pthread_cond_timedwait(&count_threshhold, &counter_mutex, &abstime);
    }
    pthread_mutex_unlock(&counter_mutex);
    pthread_mutex_destroy(&counter_mutex);
    pthread_cond_destroy(&count_threshhold);
    pthread_attr_destroy(&attr);
  }
  else
  {
    drizzle_con_st *con;

    if (!(con= db_connect(current_host,current_db,current_user,opt_password)))
    {
      return(1);
    }

    for (; *argv != NULL; argv++)
      if ((error= write_to_table(*argv, con)))
        if (exitcode == 0)
          exitcode= error;
    db_disconnect(current_host, con);
  }
  opt_password.empty();
}
  catch(exception &err)
  {
    cerr<<err.what()<<endl;
  }
  return(exitcode);
}
