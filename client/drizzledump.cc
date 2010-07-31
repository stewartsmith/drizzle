/* Copyright 2000-2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.

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
#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/m_string.h"
#include "drizzled/charset_info.h"
#include <stdarg.h>
#include <drizzled/unordered_set.h>
#include <algorithm>
#include <fstream>
#include <drizzled/gettext.h>
#include <drizzled/configmake.h>
#include <drizzled/error.h>
#include <boost/program_options.hpp>

using namespace std;
using namespace drizzled;
namespace po= boost::program_options;

/* Exit codes */

#define EX_USAGE 1
#define EX_DRIZZLEERR 2
#define EX_CONSCHECK 3
#define EX_EOM 4
#define EX_EOF 5 /* ferror for output file was got */
#define EX_ILLEGAL_TABLE 6
#define EX_TABLE_STATUS 7

/* index into 'show fields from table' */

#define SHOW_FIELDNAME  0
#define SHOW_TYPE  1
#define SHOW_NULL  2
#define SHOW_DEFAULT  4
#define SHOW_EXTRA  5

/* Size of buffer for dump's select query */
#define QUERY_LENGTH 1536
#define DRIZZLE_MAX_LINE_LENGTH 1024*1024L-1025

/* ignore table flags */
#define IGNORE_NONE 0x00 /* no ignore */
#define IGNORE_DATA 0x01 /* don't dump data for this table */
#define IGNORE_INSERT_DELAYED 0x02 /* table doesn't support INSERT DELAYED */

static void add_load_option(string &str, const char *option,
                            const string &option_value);
static uint32_t find_set(TYPELIB *lib, const char *x, uint32_t length,
                         char **err_pos, uint32_t *err_len);

static void field_escape(string &in, const string &from);
static bool  verbose= false;
static bool opt_no_create_info;
static bool opt_no_data= false;
static bool opt_mysql= false;
static bool quick= true;
static bool extended_insert= true;
static bool ignore_errors= false;
static bool flush_logs= false;
static bool opt_drop= true; 
static bool opt_keywords= false;
static bool opt_compress= false;
static bool opt_delayed= false; 
static bool create_options= true; 
static bool opt_quoted= false;
static bool opt_databases= false; 
static bool opt_alldbs= false; 
static bool opt_create_db= false;
static bool opt_lock_all_tables= false;
static bool opt_set_charset= false; 
static bool opt_dump_date= true;
static bool opt_autocommit= false; 
static bool opt_disable_keys= true;
static bool opt_xml;
static bool opt_single_transaction= false; 
static bool opt_comments;
static bool opt_compact;
static bool opt_hex_blob= false;
static bool opt_order_by_primary=false; 
static bool opt_ignore= false;
static bool opt_complete_insert= false;
static bool opt_drop_database;
static bool opt_replace_into= false;
static bool opt_routines= false;
static bool opt_alltspcs= false;
static uint32_t show_progress_size= 0;
static uint64_t total_rows= 0;
static drizzle_st drizzle;
static drizzle_con_st dcon;
static string insert_pat;
static char *order_by= NULL;
static char *err_ptr= NULL;
static char compatible_mode_normal_str[255];
static uint32_t opt_compatible_mode= 0;
static uint32_t opt_drizzle_port= 0;
static int first_error= 0;
static string extended_row;
FILE *md_result_file= 0;
FILE *stderror_file= 0;

string password,
  opt_compatible_mode_str,
  enclosed,
  escaped,
  current_host,
  opt_enclosed,
  fields_terminated,
  path,
  lines_terminated,
  current_user,
  opt_password,
  where;

static const CHARSET_INFO *charset_info= &my_charset_utf8_general_ci;

static const char *compatible_mode_names[]=
{
  "MYSQL323", "MYSQL40", "POSTGRESQL", "ORACLE", "MSSQL", "DB2",
  "MAXDB", "NO_KEY_OPTIONS", "NO_TABLE_OPTIONS", "NO_FIELD_OPTIONS",
  "ANSI",
  NULL
};
static TYPELIB compatible_mode_typelib= {array_elements(compatible_mode_names) - 1,
  "", compatible_mode_names, NULL};

unordered_set<string> ignore_table;

static void maybe_exit(int error);
static void die(int error, const char* reason, ...);
static void maybe_die(int error, const char* reason, ...);
static void write_header(FILE *sql_file, char *db_name);
static void print_value(FILE *file, drizzle_result_st *result,
                        drizzle_row_t row, const char *prefix, const char *name,
                        int string_value);
static const char* fetch_named_row(drizzle_result_st *result, drizzle_row_t row,
                                   const char* name);
static int dump_selected_tables(const string &db, const vector<string> &table_names);
static int dump_all_tables_in_db(char *db);
static int init_dumping_tables(char *);
static int init_dumping(char *, int init_func(char*));
static int dump_databases(const vector<string> &db_names);
static int dump_all_databases(void);
static char *quote_name(const char *name, char *buff, bool force);
char check_if_ignore_table(const char *table_name, char *table_type);
static char *primary_key_fields(const char *table_name);

/*
  Print the supplied message if in verbose mode

  SYNOPSIS
  verbose_msg()
  fmt   format specifier
  ...   variable number of parameters
*/

static void verbose_msg(const char *fmt, ...)
{
  va_list args;


  if (!verbose)
    return;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
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

static void write_header(FILE *sql_file, char *db_name)
{
  if (opt_xml)
  {
    fputs("<?xml version=\"1.0\"?>\n", sql_file);
    /*
      Schema reference.  Allows use of xsi:nil for NULL values and
xsi:type to define an element's data type.
    */
    fputs("<drizzledump ", sql_file);
    fputs("xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"",
          sql_file);
    fputs(">\n", sql_file);
    check_io(sql_file);
  }
  else if (! opt_compact)
  { 
    if (opt_comments)
    {
      fprintf(sql_file,
              "-- drizzledump %s libdrizzle %s, for %s-%s (%s)\n--\n",
              VERSION, drizzle_version(), HOST_VENDOR, HOST_OS, HOST_CPU);
      fprintf(sql_file, "-- Host: %s    Database: %s\n",
              ! current_host.empty() ? current_host.c_str() : "localhost", db_name ? db_name :
              "");
      fputs("-- ------------------------------------------------------\n",
            sql_file);
      fprintf(sql_file, "-- Server version\t%s\n",
              drizzle_con_server_version(&dcon));
    }
    if (opt_set_charset)
      fprintf(sql_file,
              "\nSET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION;\n");

    if (path.empty())
    {
      fprintf(md_result_file,"SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;\nSET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;\n");
    }
    check_io(sql_file);
  }
} /* write_header */


static void write_footer(FILE *sql_file)
{
  if (opt_xml)
  {
    fputs("</drizzledump>\n", sql_file);
    check_io(sql_file);
  }
  else if (! opt_compact)
  {
    if (path.empty())
    {
      fprintf(md_result_file,"SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;\nSET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;\n");
    }
    if (opt_set_charset)
      fprintf(sql_file, "SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION;\n");
    if (opt_comments)
    {
      if (opt_dump_date)
      {
        char time_str[20];
        internal::get_date(time_str, GETDATE_DATE_TIME, 0);
        fprintf(sql_file, "-- Dump completed on %s\n",
                time_str);
      }
      else
        fprintf(sql_file, "-- Dump completed\n");
    }
    check_io(sql_file);
  }
} /* write_footer */

static int get_options(void)
{

  if (path.empty() && (! enclosed.empty() || ! opt_enclosed.empty() || ! escaped.empty() || ! lines_terminated.empty() ||
                ! fields_terminated.empty()))
  {
    fprintf(stderr,
            _("%s: You must use option --tab with --fields-...\n"), internal::my_progname);
    return(EX_USAGE);
  }

  if (opt_single_transaction && opt_lock_all_tables)
  {
    fprintf(stderr, _("%s: You can't use --single-transaction and "
                      "--lock-all-tables at the same time.\n"), internal::my_progname);
    return(EX_USAGE);
  }
  if (! enclosed.empty() && ! opt_enclosed.empty())
  {
    fprintf(stderr, _("%s: You can't use ..enclosed.. and ..optionally-enclosed.. at the same time.\n"), internal::my_progname);
    return(EX_USAGE);
  }
  if ((opt_databases || opt_alldbs) && ! path.empty())
  {
    fprintf(stderr,
            _("%s: --databases or --all-databases can't be used with --tab.\n"),
            internal::my_progname);
    return(EX_USAGE);
  }

  if (tty_password)
    opt_password=client_get_tty_password(NULL);
  return(0);
} /* get_options */


/*
 ** DB_error -- prints DRIZZLE error message and exits the program.
*/
static void DB_error(drizzle_result_st *res, drizzle_return_t ret,
                     const char *when)
{
  if (ret == DRIZZLE_RETURN_ERROR_CODE)
  {
    maybe_die(EX_DRIZZLEERR, _("Got error: %s (%d) %s"),
              drizzle_result_error(res),
              drizzle_result_error_code(res),
              when);
    drizzle_result_free(res);
  }
  else
    maybe_die(EX_DRIZZLEERR, _("Got error: %d %s"), ret, when);

  return;
}



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

  fprintf(stderr, "%s: %s\n", internal::my_progname, buffer);
  fflush(stderr);

  ignore_errors= 0; /* force the exit */
  maybe_exit(error_num);
}


/*
  Prints out an error message and maybe kills the process.

  SYNOPSIS
  maybe_die()
  error_num   - process return value
  fmt_reason  - a format string for use by vsnprintf.
  ...         - variable arguments for above fmt_reason string

  DESCRIPTION
  This call prints out the formatted error message to stderr and then
  terminates the process, unless the --force command line option is used.

  This call should be used for non-fatal errors (such as database
  errors) that the code may still be able to continue to the next unit
  of work.

*/
static void maybe_die(int error_num, const char* fmt_reason, ...)
{
  char buffer[1000];
  va_list args;
  va_start(args,fmt_reason);
  vsnprintf(buffer, sizeof(buffer), fmt_reason, args);
  va_end(args);

  fprintf(stderr, "%s: %s\n", internal::my_progname, buffer);
  fflush(stderr);

  maybe_exit(error_num);
}



/*
  Sends a query to server, optionally reads result, prints error message if
  some.

  SYNOPSIS
  drizzleclient_query_with_error_report()
  drizzle_con       connection to use
  res             if non zero, result will be put there with
  drizzleclient_store_result()
  query           query to send to server

  RETURN VALUES
  0               query sending and (if res!=0) result reading went ok
  1               error
*/

static int drizzleclient_query_with_error_report(drizzle_con_st *con,
                                                 drizzle_result_st *result,
                                                 const char *query_str,
                                                 bool no_buffer)
{
  drizzle_return_t ret;

  if (drizzle_query_str(con, result, query_str, &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      maybe_die(EX_DRIZZLEERR, _("Couldn't execute '%s': %s (%d)"),
                query_str, drizzle_result_error(result),
                drizzle_result_error_code(result));
      drizzle_result_free(result);
    }
    else
    {
      maybe_die(EX_DRIZZLEERR, _("Couldn't execute '%s': %s (%d)"),
                query_str, drizzle_con_error(con), ret);
    }
    return 1;
  }

  if (no_buffer)
    ret= drizzle_column_buffer(result);
  else
    ret= drizzle_result_buffer(result);
  if (ret != DRIZZLE_RETURN_OK)
  {
    drizzle_result_free(result);
    maybe_die(EX_DRIZZLEERR, _("Couldn't execute '%s': %s (%d)"),
              query_str, drizzle_con_error(con), ret);
    return 1;
  }

  return 0;
}

/*
  Open a new .sql file to dump the table or view into

  SYNOPSIS
  open_sql_file_for_table
  name      name of the table or view

  RETURN VALUES
  0        Failed to open file
  > 0      Handle of the open file
*/
static FILE* open_sql_file_for_table(const char* table)
{
  FILE* res;
  char filename[FN_REFLEN], tmp_path[FN_REFLEN];
  internal::convert_dirname(tmp_path,(char *)path.c_str(),NULL);
  res= fopen(internal::fn_format(filename, table, tmp_path, ".sql", 4), "w");

  return res;
}


static void free_resources(void)
{
  if (md_result_file && md_result_file != stdout)
    fclose(md_result_file);
  opt_password.erase();
  internal::my_end();
}


static void maybe_exit(int error)
{
  if (!first_error)
    first_error= error;
  if (ignore_errors)
    return;
  drizzle_con_free(&dcon);
  drizzle_free(&drizzle);
  free_resources();
  exit(error);
}


/*
  db_connect -- connects to the host and selects DB.
*/

static int connect_to_db(string host, string user,string passwd)
{
  drizzle_return_t ret;

  verbose_msg(_("-- Connecting to %s...\n"), ! host.empty() ? (char *)host.c_str() : "localhost");
  drizzle_create(&drizzle);
  drizzle_con_create(&drizzle, &dcon);
  drizzle_con_set_tcp(&dcon, (char *)host.c_str(), opt_drizzle_port);
  drizzle_con_set_auth(&dcon, (char *)user.c_str(), (char *)passwd.c_str());
  if (opt_mysql)
    drizzle_con_add_options(&dcon, DRIZZLE_CON_MYSQL);
  ret= drizzle_con_connect(&dcon);
  if (ret != DRIZZLE_RETURN_OK)
  {
    DB_error(NULL, ret, "when trying to connect");
    return(1);
  }

  return(0);
} /* connect_to_db */


/*
 ** dbDisconnect -- disconnects from the host.
*/
static void dbDisconnect(string &host)
{
  verbose_msg(_("-- Disconnecting from %s...\n"), ! host.empty() ? host.c_str() : "localhost");
  drizzle_con_free(&dcon);
  drizzle_free(&drizzle);
} /* dbDisconnect */


static void unescape(FILE *file,char *pos,uint32_t length)
{
  char *tmp;

  if (!(tmp=(char*) malloc(length*2+1)))
    die(EX_DRIZZLEERR, _("Couldn't allocate memory"));

  drizzle_escape_string(tmp, pos, length);
  fputc('\'', file);
  fputs(tmp, file);
  fputc('\'', file);
  check_io(file);
  free(tmp);
  return;
} /* unescape */


static bool test_if_special_chars(const char *str)
{
  for ( ; *str ; str++)
    if (!my_isvar(charset_info,*str) && *str != '$')
      return 1;
  return 0;
} /* test_if_special_chars */



/*
  quote_name(name, buff, force)

  Quotes char string, taking into account compatible mode

  Args

  name                 Unquoted string containing that which will be quoted
  buff                 The buffer that contains the quoted value, also returned
  force                Flag to make it ignore 'test_if_special_chars'

  Returns

  buff                 quoted string

*/
static char *quote_name(const char *name, char *buff, bool force)
{
  char *to= buff;
  char qtype= '`';

  if (!force && !opt_quoted && !test_if_special_chars(name))
    return (char*) name;
  *to++= qtype;
  while (*name)
  {
    if (*name == qtype)
      *to++= qtype;
    *to++= *name++;
  }
  to[0]= qtype;
  to[1]= 0;
  return buff;
} /* quote_name */


/*
  Quote a table name so it can be used in "SHOW TABLES LIKE <tabname>"

  SYNOPSIS
  quote_for_like()
  name     name of the table
  buff     quoted name of the table

  DESCRIPTION
  Quote \, _, ' and % characters

Note: Because DRIZZLE uses the C escape syntax in strings
(for example, '\n' to represent newline), you must double
any '\' that you use in your LIKE  strings. For example, to
search for '\n', specify it as '\\n'. To search for '\', specify
it as '\\\\' (the backslashes are stripped once by the parser
and another time when the pattern match is done, leaving a
single backslash to be matched).

Example: "t\1" => "t\\\\1"

*/
static char *quote_for_like(const char *name, char *buff)
{
  char *to= buff;
  *to++= '\'';
  while (*name)
  {
    if (*name == '\\')
    {
      *to++='\\';
      *to++='\\';
      *to++='\\';
    }
    else if (*name == '\'' || *name == '_'  || *name == '%')
      *to++= '\\';
    *to++= *name++;
  }
  to[0]= '\'';
  to[1]= 0;
  return buff;
}


/*
  Quote and print a string.

  SYNOPSIS
  print_quoted_xml()
  xml_file    - output file
  str         - string to print
  len         - its length

  DESCRIPTION
  Quote '<' '>' '&' '\"' chars and print a string to the xml_file.
*/

static void print_quoted_xml(FILE *xml_file, const char *str, uint32_t len)
{
  const char *end;

  for (end= str + len; str != end; str++)
  {
    switch (*str) {
    case '<':
      fputs("&lt;", xml_file);
      break;
    case '>':
      fputs("&gt;", xml_file);
      break;
    case '&':
      fputs("&amp;", xml_file);
      break;
      case '\"':
        fputs("&quot;", xml_file);
      break;
    default:
      fputc(*str, xml_file);
      break;
    }
  }
  check_io(xml_file);
}


/*
  Print xml tag. Optionally add attribute(s).

  SYNOPSIS
  print_xml_tag(xml_file, sbeg, send, tag_name, first_attribute_name,
  ..., attribute_name_n, attribute_value_n, NULL)
  xml_file              - output file
  sbeg                  - line beginning
  line_end              - line ending
  tag_name              - XML tag name.
  first_attribute_name  - tag and first attribute
  first_attribute_value - (Implied) value of first attribute
  attribute_name_n      - attribute n
  attribute_value_n     - value of attribute n

  DESCRIPTION
  Print XML tag with any number of attribute="value" pairs to the xml_file.

  Format is:
  sbeg<tag_name first_attribute_name="first_attribute_value" ...
  attribute_name_n="attribute_value_n">send
  NOTE
  Additional arguments must be present in attribute/value pairs.
  The last argument should be the null character pointer.
  All attribute_value arguments MUST be NULL terminated strings.
  All attribute_value arguments will be quoted before output.
*/

static void print_xml_tag(FILE * xml_file, const char* sbeg,
                          const char* line_end,
                          const char* tag_name,
                          const char* first_attribute_name, ...)
{
  va_list arg_list;
  const char *attribute_name, *attribute_value;

  fputs(sbeg, xml_file);
  fputc('<', xml_file);
  fputs(tag_name, xml_file);

  va_start(arg_list, first_attribute_name);
  attribute_name= first_attribute_name;
  while (attribute_name != NULL)
  {
    attribute_value= va_arg(arg_list, char *);
    assert(attribute_value != NULL);

    fputc(' ', xml_file);
    fputs(attribute_name, xml_file);
    fputc('\"', xml_file);

    print_quoted_xml(xml_file, attribute_value, strlen(attribute_value));
    fputc('\"', xml_file);

    attribute_name= va_arg(arg_list, char *);
  }
  va_end(arg_list);

  fputc('>', xml_file);
  fputs(line_end, xml_file);
  check_io(xml_file);
}


/*
  Print xml tag with for a field that is null

  SYNOPSIS
  print_xml_null_tag()
  xml_file    - output file
  sbeg        - line beginning
  stag_atr    - tag and attribute
  sval        - value of attribute
  line_end        - line ending

  DESCRIPTION
  Print tag with one attribute to the xml_file. Format is:
  <stag_atr="sval" xsi:nil="true"/>
  NOTE
  sval MUST be a NULL terminated string.
  sval string will be qouted before output.
*/

static void print_xml_null_tag(FILE * xml_file, const char* sbeg,
                               const char* stag_atr, const char* sval,
                               const char* line_end)
{
  fputs(sbeg, xml_file);
  fputs("<", xml_file);
  fputs(stag_atr, xml_file);
  fputs("\"", xml_file);
  print_quoted_xml(xml_file, sval, strlen(sval));
  fputs("\" xsi:nil=\"true\" />", xml_file);
  fputs(line_end, xml_file);
  check_io(xml_file);
}


/*
  Print xml tag with many attributes.

  SYNOPSIS
  print_xml_row()
  xml_file    - output file
  row_name    - xml tag name
  tableRes    - query result
  row         - result row

  DESCRIPTION
  Print tag with many attribute to the xml_file. Format is:
  \t\t<row_name Atr1="Val1" Atr2="Val2"... />
  NOTE
  All atributes and values will be quoted before output.
*/

static void print_xml_row(FILE *xml_file, const char *row_name,
                          drizzle_result_st *tableRes, drizzle_row_t *row)
{
  uint32_t i;
  drizzle_column_st *column;
  size_t *lengths= drizzle_row_field_sizes(tableRes);

  fprintf(xml_file, "\t\t<%s", row_name);
  check_io(xml_file);
  drizzle_column_seek(tableRes, 0);
  for (i= 0; (column= drizzle_column_next(tableRes)); i++)
  {
    if ((*row)[i])
    {
      fputc(' ', xml_file);
      print_quoted_xml(xml_file, drizzle_column_name(column),
                       strlen(drizzle_column_name(column)));
      fputs("=\"", xml_file);
      print_quoted_xml(xml_file, (*row)[i], lengths[i]);
      fputc('"', xml_file);
      check_io(xml_file);
    }
  }
  fputs(" />\n", xml_file);
  check_io(xml_file);
}


/*
  Print hex value for blob data.

  SYNOPSIS
  print_blob_as_hex()
  output_file         - output file
  str                 - string to print
  len                 - its length

  DESCRIPTION
  Print hex value for blob data.
*/

static void print_blob_as_hex(FILE *output_file, const char *str, uint32_t len)
{
  /* sakaik got the idea to to provide blob's in hex notation. */
  const char *ptr= str, *end= ptr + len;
  for (; ptr < end ; ptr++)
    fprintf(output_file, "%02X", *((unsigned char *)ptr));
  check_io(output_file);
}

/*
  get_table_structure -- retrievs database structure, prints out corresponding
  CREATE statement and fills out insert_pat if the table is the type we will
  be dumping.

  ARGS
  table       - table name
  db          - db name
  table_type  - table type, e.g. "MyISAM" or "InnoDB", but also "VIEW"
  ignore_flag - what we must particularly ignore - see IGNORE_ defines above
  num_fields  - number of fields in the table

  RETURN
  true if success, false if error
*/

static bool get_table_structure(char *table, char *db, char *table_type,
                                char *ignore_flag, uint64_t *num_fields)
{
  bool    init=0, delayed, write_data, complete_insert;
  char       *result_table, *opt_quoted_table;
  const char *insert_option;
  char	     name_buff[DRIZZLE_MAX_COLUMN_NAME_SIZE+3];
  char       table_buff[DRIZZLE_MAX_COLUMN_NAME_SIZE*2+3];
  char       table_buff2[DRIZZLE_MAX_TABLE_SIZE*2+3];
  char       query_buff[QUERY_LENGTH];
  FILE       *sql_file= md_result_file;
  drizzle_result_st result;
  drizzle_row_t  row;

  *ignore_flag= check_if_ignore_table(table, table_type);

  delayed= opt_delayed;
  if (delayed && (*ignore_flag & IGNORE_INSERT_DELAYED))
  {
    delayed= 0;
    verbose_msg(_("-- Warning: Unable to use delayed inserts for table '%s' "
                  "because it's of type %s\n"), table, table_type);
  }

  complete_insert= 0;
  if ((write_data= !(*ignore_flag & IGNORE_DATA)))
  {
    complete_insert= opt_complete_insert;
    insert_pat.clear();
  }

  insert_option= ((delayed && opt_ignore) ? " DELAYED IGNORE " :
                  delayed ? " DELAYED " : opt_ignore ? " IGNORE " : "");

  verbose_msg(_("-- Retrieving table structure for table %s...\n"), table);

  result_table=     quote_name(table, table_buff, 1);
  opt_quoted_table= quote_name(table, table_buff2, 0);

  if (opt_order_by_primary)
  {
    free(order_by);
    order_by= primary_key_fields(result_table);
  }

  if (! opt_xml)
  { 
    /* using SHOW CREATE statement */
    if (! opt_no_create_info)
    { 
      /* Make an sql-file, if path was given iow. option -T was given */
      char buff[20+FN_REFLEN];
      const drizzle_column_st *column;

      snprintf(buff, sizeof(buff), "show create table %s", result_table);

      if (drizzleclient_query_with_error_report(&dcon, &result, buff, false))
        return false;

      if (! path.empty())
      {
        if (!(sql_file= open_sql_file_for_table(table)))
        {
          drizzle_result_free(&result);
          return false;
        }

        write_header(sql_file, db);
      }
      if (!opt_xml && opt_comments)
      {
        fprintf(sql_file, "\n--\n-- Table structure for table %s\n--\n\n",
                result_table);
        check_io(sql_file);
      }
      if (opt_drop)
      { 
        /*
          Even if the "table" is a view, we do a DROP TABLE here.
        */
        fprintf(sql_file, "DROP TABLE IF EXISTS %s;\n", opt_quoted_table);
        check_io(sql_file);
      }

      column= drizzle_column_index(&result, 0);

      row= drizzle_row_next(&result);

      fprintf(sql_file, "%s;\n", row[1]);

      check_io(sql_file);
      drizzle_result_free(&result);
    }

    snprintf(query_buff, sizeof(query_buff), "show fields from %s",
             result_table);

    if (drizzleclient_query_with_error_report(&dcon, &result, query_buff, false))
    {
      if (! path.empty())
        fclose(sql_file);
      return false;
    }

    /*
      If write_data is true, then we build up insert statements for
      the table's data. Note: in subsequent lines of code, this test
      will have to be performed each time we are appending to
      insert_pat.
    */
    if (write_data)
    {
      if (opt_replace_into)
        insert_pat.append("REPLACE ");
      else
        insert_pat.append("INSERT ");
      insert_pat.append(insert_option);
      insert_pat.append("INTO ");
      insert_pat.append(opt_quoted_table);
      if (complete_insert)
      {
        insert_pat.append(" (");
      }
      else
      {
        insert_pat.append(" VALUES ");
        if (!extended_insert)
          insert_pat.append("(");
      }
    }

    while ((row= drizzle_row_next(&result)))
    {
      if (complete_insert)
      {
        if (init)
        {
          insert_pat.append(", ");
        }
        init=1;
        insert_pat.append(quote_name(row[SHOW_FIELDNAME], name_buff, 0));
      }
    }
    *num_fields= drizzle_result_row_count(&result);
    drizzle_result_free(&result);
  }
  else
  {
    verbose_msg(_("%s: Warning: Can't set SQL_QUOTE_SHOW_CREATE option (%s)\n"),
                internal::my_progname, drizzle_con_error(&dcon));

    snprintf(query_buff, sizeof(query_buff), "show fields from %s",
             result_table);
    if (drizzleclient_query_with_error_report(&dcon, &result, query_buff, false))
      return false;

    /* Make an sql-file, if path was given iow. option -T was given */
    if (! opt_no_create_info)
    {
      if (! path.empty())
      {
        if (!(sql_file= open_sql_file_for_table(table)))
        {
          drizzle_result_free(&result);
          return false;
        }
        write_header(sql_file, db);
      }
      if (!opt_xml && opt_comments)
        fprintf(sql_file, "\n--\n-- Table structure for table %s\n--\n\n",
                result_table);
      if (opt_drop)
        fprintf(sql_file, "DROP TABLE IF EXISTS %s;\n", result_table);
      if (!opt_xml)
        fprintf(sql_file, "CREATE TABLE %s (\n", result_table);
      else
        print_xml_tag(sql_file, "\t", "\n", "table_structure", "name=", table,
                      NULL);
      check_io(sql_file);
    }

    if (write_data)
    {
      if (opt_replace_into)
        insert_pat.append("REPLACE ");
      else
        insert_pat.append("INSERT ");
      insert_pat.append(insert_option);
      insert_pat.append("INTO ");
      insert_pat.append(result_table);
      if (complete_insert)
        insert_pat.append(" (");
      else
      {
        insert_pat.append(" VALUES ");
        if (!extended_insert)
          insert_pat.append("(");
      }
    }

    while ((row= drizzle_row_next(&result)))
    {
      size_t *lengths= drizzle_row_field_sizes(&result);
      if (init)
      {
        if (!opt_xml && !opt_no_create_info)
        {
          fputs(",\n",sql_file);
          check_io(sql_file);
        }
        if (complete_insert)
          insert_pat.append(", ");
      }
      init=1;
      if (complete_insert)
        insert_pat.append(quote_name(row[SHOW_FIELDNAME], name_buff, 0));
      if (!opt_no_create_info)
      {
        if (opt_xml)
        {
          print_xml_row(sql_file, "field", &result, &row);
          continue;
        }

        if (opt_keywords)
          fprintf(sql_file, "  %s.%s %s", result_table,
                  quote_name(row[SHOW_FIELDNAME],name_buff, 0),
                  row[SHOW_TYPE]);
        else
          fprintf(sql_file, "  %s %s", quote_name(row[SHOW_FIELDNAME],
                                                  name_buff, 0),
                  row[SHOW_TYPE]);
        if (row[SHOW_DEFAULT])
        {
          fputs(" DEFAULT ", sql_file);
          unescape(sql_file, row[SHOW_DEFAULT], lengths[SHOW_DEFAULT]);
        }
        if (!row[SHOW_NULL][0])
          fputs(" NOT NULL", sql_file);
        if (row[SHOW_EXTRA][0])
          fprintf(sql_file, " %s",row[SHOW_EXTRA]);
        check_io(sql_file);
      }
    }
    *num_fields= drizzle_result_row_count(&result);
    drizzle_result_free(&result);

    if (!opt_no_create_info)
    {
      /* Make an sql-file, if path was given iow. option -T was given */
      char buff[20+FN_REFLEN];
      uint32_t keynr,primary_key;
      snprintf(buff, sizeof(buff), "show keys from %s", result_table);
      if (drizzleclient_query_with_error_report(&dcon, &result, buff, false))
      {
        fprintf(stderr, _("%s: Can't get keys for table %s\n"),
                internal::my_progname, result_table);
        if (! path.empty())
          fclose(sql_file);
        return false;
      }

      /* Find first which key is primary key */
      keynr=0;
      primary_key=INT_MAX;
      while ((row= drizzle_row_next(&result)))
      {
        if (atoi(row[3]) == 1)
        {
          keynr++;
#ifdef FORCE_PRIMARY_KEY
          if (atoi(row[1]) == 0 && primary_key == INT_MAX)
            primary_key=keynr;
#endif
          if (!strcmp(row[2],"PRIMARY"))
          {
            primary_key=keynr;
            break;
          }
        }
      }
      drizzle_row_seek(&result,0);
      keynr=0;
      while ((row= drizzle_row_next(&result)))
      {
        if (opt_xml)
        {
          print_xml_row(sql_file, "key", &result, &row);
          continue;
        }

        if (atoi(row[3]) == 1)
        {
          if (keynr++)
            putc(')', sql_file);
          if (atoi(row[1]))       /* Test if duplicate key */
            /* Duplicate allowed */
            fprintf(sql_file, ",\n  KEY %s (",quote_name(row[2],name_buff,0));
          else if (keynr == primary_key)
            fputs(",\n  PRIMARY KEY (",sql_file); /* First UNIQUE is primary */
          else
            fprintf(sql_file, ",\n  UNIQUE %s (",quote_name(row[2],name_buff,
                                                            0));
        }
        else
          putc(',', sql_file);
        fputs(quote_name(row[4], name_buff, 0), sql_file);
        if (row[7])
          fprintf(sql_file, " (%s)",row[7]);      /* Sub key */
        check_io(sql_file);
      }
      drizzle_result_free(&result);
      if (!opt_xml)
      {
        if (keynr)
          putc(')', sql_file);
        fputs("\n)",sql_file);
        check_io(sql_file);
      }
      /* Get DRIZZLE specific create options */
      if (create_options)
      {
        char show_name_buff[DRIZZLE_MAX_COLUMN_NAME_SIZE*2+2+24];

        /* Check memory for quote_for_like() */
        snprintf(buff, sizeof(buff), "show table status like %s",
                 quote_for_like(table, show_name_buff));

        if (!drizzleclient_query_with_error_report(&dcon, &result, buff, false))
        {
          if (!(row= drizzle_row_next(&result)))
          {
            fprintf(stderr,
                    _("Error: Couldn't read status information for table %s\n"),
                    result_table);
          }
          else
          {
            if (opt_xml)
              print_xml_row(sql_file, "options", &result, &row);
            else
            {
              fputs("/*!",sql_file);
              print_value(sql_file,&result,row,"engine=","Engine",0);
              print_value(sql_file,&result,row,"","Create_options",0);
              print_value(sql_file,&result,row,"comment=","Comment",1);

              fputs(" */",sql_file);
              check_io(sql_file);
            }
          }
          drizzle_result_free(&result);
        }
      }
      if (!opt_xml)
        fputs(";\n", sql_file);
      else
        fputs("\t</table_structure>\n", sql_file);
      check_io(sql_file);
    }
  }
  if (complete_insert) {
    insert_pat.append(") VALUES ");
    if (!extended_insert)
      insert_pat.append("(");
  }
  if (sql_file != md_result_file)
  {
    fputs("\n", sql_file);
    write_footer(sql_file);
    fclose(sql_file);
  }
  return true;
} /* get_table_structure */

static void add_load_option(string &str, const char *option,
                            const string &option_value)
{
  if (option_value.empty())
  {
    /* Null value means we don't add this option. */
    return;
  }

  str.append(option);

  if (option_value.compare(0, 2, "0x") == 0)
  {
    /* It's a hex constant, don't escape */
    str.append(option_value);
  }
  else
  {
    /* char constant; escape */
    field_escape(str, option_value);
  }
}


/*
  Allow the user to specify field terminator strings like:
  "'", "\", "\\" (escaped backslash), "\t" (tab), "\n" (newline)
  This is done by doubling ' and add a end -\ if needed to avoid
  syntax errors from the SQL parser.
*/

static void field_escape(string &in, const string &from)
{
  uint32_t end_backslashes= 0;

  in.append("'");

  string::const_iterator it= from.begin();
  while (it != from.end())
  {
    in.push_back(*it);

    if (*it == '\\')
      end_backslashes^= 1;    /* find odd number of backslashes */
    else
    {
      if (*it == '\'' && !end_backslashes)
      {
        /* We want a duplicate of "'" for DRIZZLE */
        in.push_back('\'');
      }
      end_backslashes=0;
    }
    ++it;
  }
  /* Add missing backslashes if user has specified odd number of backs.*/
  if (end_backslashes)
    in.append("\\");

  in.append("'");
}



/*

  SYNOPSIS
  dump_table()

  dump_table saves database contents as a series of INSERT statements.

  ARGS
  table - table name
  db    - db name

  RETURNS
  void
*/


static void dump_table(char *table, char *db)
{
  char ignore_flag;
  char table_buff[DRIZZLE_MAX_TABLE_SIZE+3];
  string query_string;
  char table_type[DRIZZLE_MAX_TABLE_SIZE];
  char *result_table, table_buff2[DRIZZLE_MAX_TABLE_SIZE*2+3], *opt_quoted_table;
  int error= 0;
  uint32_t rownr, row_break, total_length, init_length;
  uint64_t num_fields= 0;
  drizzle_return_t ret;
  drizzle_result_st result;
  drizzle_column_st *column;
  drizzle_row_t row;


  /*
    Make sure you get the create table info before the following check for
    --no-data flag below. Otherwise, the create table info won't be printed.
  */
  if (!get_table_structure(table, db, table_type, &ignore_flag, &num_fields))
  {
    maybe_die(EX_TABLE_STATUS, _("Error retrieving table structure for table: \"%s\""), table);
    return;
  }

  /* Check --no-data flag */
  if (opt_no_data)
  {
    verbose_msg(_("-- Skipping dump data for table '%s', --no-data was used\n"),
                table);
    return;
  }

  /*
    If the table type is a merge table or any type that has to be
    _completely_ ignored and no data dumped
  */
  if (ignore_flag & IGNORE_DATA)
  {
    verbose_msg(_("-- Warning: Skipping data for table '%s' because " \
                  "it's of type %s\n"), table, table_type);
    return;
  }
  /* Check that there are any fields in the table */
  if (num_fields == 0)
  {
    verbose_msg(_("-- Skipping dump data for table '%s', it has no fields\n"),
                table);
    return;
  }

  result_table= quote_name(table,table_buff, 1);
  opt_quoted_table= quote_name(table, table_buff2, 0);

  verbose_msg(_("-- Sending SELECT query...\n"));

  query_string.clear();
  query_string.reserve(1024);

  if (! path.empty())
  {
    char filename[FN_REFLEN], tmp_path[FN_REFLEN];

    /*
      Convert the path to native os format
      and resolve to the full filepath.
    */
    internal::convert_dirname(tmp_path,(char *)path.c_str(),NULL);
    internal::my_load_path(tmp_path, tmp_path, NULL);
    internal::fn_format(filename, table, tmp_path, ".txt", MYF(MY_UNPACK_FILENAME));

    /* Must delete the file that 'INTO OUTFILE' will write to */
    internal::my_delete(filename, MYF(0));

    /* now build the query string */

    query_string.append( "SELECT * INTO OUTFILE '");
    query_string.append( filename);
    query_string.append( "'");

    if (! fields_terminated.empty() || ! enclosed.empty() || ! opt_enclosed.empty() || ! escaped.empty())
      query_string.append( " FIELDS");

    add_load_option(query_string, " TERMINATED BY ", fields_terminated);
    add_load_option(query_string, " ENCLOSED BY ", enclosed);
    add_load_option(query_string, " OPTIONALLY ENCLOSED BY ", opt_enclosed);
    add_load_option(query_string, " ESCAPED BY ", escaped);
    add_load_option(query_string, " LINES TERMINATED BY ", lines_terminated);

    query_string.append(" FROM ");
    query_string.append(result_table);

    if (! where.empty())
    {
      query_string.append(" WHERE ");
      query_string.append(where);
    }

    if (order_by)
    {
      query_string.append(" ORDER BY ");
      query_string.append(order_by);
    }

    if (drizzle_query(&dcon, &result, query_string.c_str(),
                      query_string.length(), &ret) == NULL ||
        ret != DRIZZLE_RETURN_OK)
    {
      DB_error(&result, ret, _("when executing 'SELECT INTO OUTFILE'"));

      return;
    }
    drizzle_result_free(&result);
  }
 
  else
  {
    if (!opt_xml && opt_comments)
    {
      fprintf(md_result_file,_("\n--\n-- Dumping data for table %s\n--\n"),
              result_table);
      check_io(md_result_file);
    }

    query_string.append( "SELECT * FROM ");
    query_string.append( result_table);

    if (! where.empty())
    {
      if (!opt_xml && opt_comments)
      {
        fprintf(md_result_file, "-- WHERE:  %s\n", where.c_str());
        check_io(md_result_file);
      }

      query_string.append( " WHERE ");
      query_string.append( (char *)where.c_str());
    }
    if (order_by)
    {
      if (!opt_xml && opt_comments)
      {
        fprintf(md_result_file, "-- ORDER BY:  %s\n", order_by);
        check_io(md_result_file);
      }
      query_string.append( " ORDER BY ");
      query_string.append( order_by);
    }

    if (!opt_xml && !opt_compact)
    {
      fputs("\n", md_result_file);
      check_io(md_result_file);
    }
    if (drizzleclient_query_with_error_report(&dcon, &result,
                                              query_string.c_str(), quick))
    {
      goto err;
    }

    verbose_msg(_("-- Retrieving rows...\n"));
    if (drizzle_result_column_count(&result) != num_fields)
    {
      fprintf(stderr,_("%s: Error in field count for table: %s !  Aborting.\n"),
              internal::my_progname, result_table);
      error= EX_CONSCHECK;
      drizzle_result_free(&result);
      goto err;
    }

    /* Moved disable keys to after lock per bug 15977 */
    if (opt_disable_keys)
    {
      fprintf(md_result_file, "ALTER TABLE %s DISABLE KEYS;\n",
              opt_quoted_table);
      check_io(md_result_file);
    }
    
    total_length= DRIZZLE_MAX_LINE_LENGTH;                /* Force row break */
    row_break=0;
    rownr=0;
    init_length=(uint32_t) insert_pat.length()+4;
    if (opt_xml)
      print_xml_tag(md_result_file, "\t", "\n", "table_data", "name=", table,
                    NULL);
    if (opt_autocommit)
    {
      fprintf(md_result_file, "set autocommit=0;\n");
      check_io(md_result_file);
    }

    row= NULL;

    while (1)
    {
      uint32_t i;
      size_t *lengths;

      if (quick)
      {
        if (row)
          drizzle_row_free(&result, row);

        row= drizzle_row_buffer(&result, &ret);
        if (ret != DRIZZLE_RETURN_OK)
        {
          fprintf(stderr,
                  _("%s: Error reading rows for table: %s (%d:%s) ! Aborting.\n"),
                  internal::my_progname, result_table, ret, drizzle_con_error(&dcon));
          drizzle_result_free(&result);
          goto err;
        }
      }
      else
        row= drizzle_row_next(&result);

      if (row == NULL)
        break;

      lengths= drizzle_row_field_sizes(&result);

      rownr++;
      if ((rownr % show_progress_size) == 0)
      {
        verbose_msg(_("-- %"PRIu32" of ~%"PRIu64" rows dumped for table %s\n"), rownr, total_rows, opt_quoted_table);
      }
      if (!extended_insert && !opt_xml)
      {
        fputs(insert_pat.c_str(),md_result_file);
        check_io(md_result_file);
      }
      drizzle_column_seek(&result,0);

      if (opt_xml)
      {
        fputs("\t<row>\n", md_result_file);
        check_io(md_result_file);
      }

      for (i= 0; i < drizzle_result_column_count(&result); i++)
      {
        int is_blob;
        uint32_t length= lengths[i];

        if (!(column= drizzle_column_next(&result)))
          die(EX_CONSCHECK,
              _("Not enough fields from table %s! Aborting.\n"),
              result_table);

        /*
          63 is my_charset_bin. If charsetnr is not 63,
          we have not a BLOB but a TEXT column.
          we'll dump in hex only BLOB columns.
        */
        is_blob= (opt_hex_blob && drizzle_column_charset(column) == 63 &&
                  (drizzle_column_type(column) == DRIZZLE_COLUMN_TYPE_VARCHAR ||
                   drizzle_column_type(column) == DRIZZLE_COLUMN_TYPE_BLOB)) ? 1 : 0;
        if (extended_insert && !opt_xml)
        {
          if (i == 0)
          {
            extended_row.clear();
            extended_row.append("(");
          }
          else
            extended_row.append(",");

          if (row[i])
          {
            if (length)
            {
              if (!(drizzle_column_flags(column) & DRIZZLE_COLUMN_FLAGS_NUM))
              {
                /*
                  "length * 2 + 2" is OK for both HEX and non-HEX modes:
                  - In HEX mode we need exactly 2 bytes per character
                  plus 2 bytes for '0x' prefix.
                  - In non-HEX mode we need up to 2 bytes per character,
                  plus 2 bytes for leading and trailing '\'' characters.
                  Also we need to reserve 1 byte for terminating '\0'.
                */
                char * tmp_str= (char *)malloc(length * 2 + 2 + 1);
                memset(tmp_str, '\0', length * 2 + 2 + 1);
                if (opt_hex_blob && is_blob)
                {
                  extended_row.append("0x");
                  drizzle_hex_string(tmp_str, row[i], length);
                  extended_row.append(tmp_str);
                }
                else
                {
                  extended_row.append("'");
                  drizzle_escape_string(tmp_str, row[i],length);
                  extended_row.append(tmp_str);
                  extended_row.append("'");
                }
                free(tmp_str);
              }
              else
              {
                /* change any strings ("inf", "-inf", "nan") into NULL */
                char *ptr= row[i];
                if (my_isalpha(charset_info, *ptr) || (*ptr == '-' &&
                                                       my_isalpha(charset_info, ptr[1])))
                  extended_row.append( "NULL");
                else
                {
                  extended_row.append( ptr);
                }
              }
            }
            else
              extended_row.append("''");
          }
          else
            extended_row.append("NULL");
        }
        else
        {
          if (i && !opt_xml)
          {
            fputc(',', md_result_file);
            check_io(md_result_file);
          }
          if (row[i])
          {
            if (!(drizzle_column_flags(column) & DRIZZLE_COLUMN_FLAGS_NUM))
            {
              if (opt_xml)
              {
                if (opt_hex_blob && is_blob && length)
                {
                  /* Define xsi:type="xs:hexBinary" for hex encoded data */
                  print_xml_tag(md_result_file, "\t\t", "", "field", "name=",
                                drizzle_column_name(column), "xsi:type=", "xs:hexBinary", NULL);
                  print_blob_as_hex(md_result_file, row[i], length);
                }
                else
                {
                  print_xml_tag(md_result_file, "\t\t", "", "field", "name=",
                                drizzle_column_name(column), NULL);
                  print_quoted_xml(md_result_file, row[i], length);
                }
                fputs("</field>\n", md_result_file);
              }
              else if (opt_hex_blob && is_blob && length)
              {
                fputs("0x", md_result_file);
                print_blob_as_hex(md_result_file, row[i], length);
              }
              else
                unescape(md_result_file, row[i], length);
            }
            else
            {
              /* change any strings ("inf", "-inf", "nan") into NULL */
              char *ptr= row[i];
              if (opt_xml)
              {
                print_xml_tag(md_result_file, "\t\t", "", "field", "name=",
                              drizzle_column_name(column), NULL);
                fputs(!my_isalpha(charset_info, *ptr) ? ptr: "NULL",
                      md_result_file);
                fputs("</field>\n", md_result_file);
              }
              else if (my_isalpha(charset_info, *ptr) ||
                       (*ptr == '-' && my_isalpha(charset_info, ptr[1])))
                fputs("NULL", md_result_file);
              else
                fputs(ptr, md_result_file);
            }
          }
          else
          {
            /* The field value is NULL */
            if (!opt_xml)
              fputs("NULL", md_result_file);
            else
              print_xml_null_tag(md_result_file, "\t\t", "field name=",
                                 drizzle_column_name(column), "\n");
          }
          check_io(md_result_file);
        }
      }

      if (opt_xml)
      {
        fputs("\t</row>\n", md_result_file);
        check_io(md_result_file);
      }

      if (extended_insert)
      {
        uint32_t row_length;
        extended_row.append(")");
        row_length= 2 + extended_row.length();
        if (total_length + row_length < DRIZZLE_MAX_LINE_LENGTH)
        {
          total_length+= row_length;
          fputc(',',md_result_file);            /* Always row break */
          fputs(extended_row.c_str(),md_result_file);
        }
        else
        {
          if (row_break)
            fputs(";\n", md_result_file);
          row_break=1;                          /* This is first row */

          fputs(insert_pat.c_str(),md_result_file);
          fputs(extended_row.c_str(),md_result_file);
          total_length= row_length+init_length;
        }
        check_io(md_result_file);
      }
      else if (!opt_xml)
      {
        fputs(");\n", md_result_file);
        check_io(md_result_file);
      }
    }

    /* XML - close table tag and supress regular output */
    if (opt_xml)
      fputs("\t</table_data>\n", md_result_file);
    else if (extended_insert && row_break)
      fputs(";\n", md_result_file);             /* If not empty table */
    fflush(md_result_file);
    check_io(md_result_file);

    /* Moved enable keys to before unlock per bug 15977 */
    if (opt_disable_keys)
    {
      fprintf(md_result_file,"ALTER TABLE %s ENABLE KEYS;\n",
              opt_quoted_table);
      check_io(md_result_file);
    }
    if (opt_autocommit)
    {
      fprintf(md_result_file, "commit;\n");
      check_io(md_result_file);
    }
    drizzle_result_free(&result);
  }
  return;

err:
  maybe_exit(error);
  return;
} /* dump_table */


static char *getTableName(int reset)
{
  static drizzle_result_st result;
  static bool have_result= false;
  drizzle_row_t row;

  if (!have_result)
  {
    if (drizzleclient_query_with_error_report(&dcon, &result, "SHOW TABLES", false))
      return NULL;
    have_result= true;
  }

  if ((row= drizzle_row_next(&result)))
    return row[0];

  if (reset)
    drizzle_row_seek(&result, 0);
  else
  {
    drizzle_result_free(&result);
    have_result= false;
  }
  return NULL;
} /* getTableName */


static int dump_all_databases()
{
  drizzle_row_t row;
  drizzle_result_st tableres;
  int result=0;

  if (drizzleclient_query_with_error_report(&dcon, &tableres, "SHOW DATABASES", false))
    return 1;
  while ((row= drizzle_row_next(&tableres)))
  {
    if (dump_all_tables_in_db(row[0]))
      result=1;
  }
  drizzle_result_free(&tableres);
  return result;
}
/* dump_all_databases */


static int dump_databases(const vector<string> &db_names)
{
  int result=0;
  string temp;
  for (vector<string>::const_iterator it= db_names.begin(); it != db_names.end(); ++it)
  {
    temp= *it;
    if (dump_all_tables_in_db((char *)temp.c_str()))
      result=1;
  }
  return(result);
} /* dump_databases */


/*
  Table Specific database initalization.

  SYNOPSIS
  init_dumping_tables
  qdatabase      quoted name of the database

  RETURN VALUES
  0        Success.
  1        Failure.
*/

int init_dumping_tables(char *qdatabase)
{
  if (!opt_create_db)
  {
    char qbuf[256];
    drizzle_row_t row;
    drizzle_result_st result;
    drizzle_return_t ret;

    snprintf(qbuf, sizeof(qbuf),
             "SHOW CREATE DATABASE IF NOT EXISTS %s",
             qdatabase);

    if (drizzle_query_str(&dcon, &result, qbuf, &ret) == NULL ||
        ret != DRIZZLE_RETURN_OK)
    {
      if (ret == DRIZZLE_RETURN_ERROR_CODE)
        drizzle_result_free(&result);

      /* Old server version, dump generic CREATE DATABASE */
      if (opt_drop_database)
        fprintf(md_result_file,
                "\nDROP DATABASE IF EXISTS %s;\n",
                qdatabase);
      fprintf(md_result_file,
              "\nCREATE DATABASE IF NOT EXISTS %s;\n",
              qdatabase);
    }
    else
    {
      if (drizzle_result_buffer(&result) == DRIZZLE_RETURN_OK)
      {
        if (opt_drop_database)
          fprintf(md_result_file,
                  "\nDROP DATABASE IF EXISTS %s;\n",
                  qdatabase);
        row = drizzle_row_next(&result);
        if (row != NULL && row[1])
        {
          fprintf(md_result_file,"\n%s;\n",row[1]);
        }
      }
      drizzle_result_free(&result);
    }
  }
  return(0);
} /* init_dumping_tables */


static int init_dumping(char *database, int init_func(char*))
{
  drizzle_result_st result;
  drizzle_return_t ret;
  char qbuf[512];
  
  /* If this DB contains non-standard tables we don't want it */

  snprintf(qbuf, sizeof(qbuf), "SELECT TABLE_NAME FROM DATA_DICTIONARY.TABLES WHERE TABLE_SCHEMA='%s' AND TABLE_TYPE != 'STANDARD'", database);
  
  if (drizzle_query_str(&dcon, &result, qbuf, &ret) != NULL)
  {
    drizzle_result_buffer(&result);
    if (drizzle_result_row_count(&result) > 0)
    {
      drizzle_result_free(&result);
      return 1;
    }
  }

  drizzle_result_free(&result);

  if (drizzle_select_db(&dcon, &result, database, &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    DB_error(&result, ret, _("when executing 'SELECT INTO OUTFILE'"));
    return 1;                   /* If --force */
  }
  drizzle_result_free(&result);

  if (path.empty() && !opt_xml)
  {
    if (opt_databases || opt_alldbs)
    {
      /*
        length of table name * 2 (if name contains quotes), 2 quotes and 0
      */
      char quoted_database_buf[DRIZZLE_MAX_DB_SIZE*2+3];
      char *qdatabase= quote_name(database,quoted_database_buf,opt_quoted);
      if (opt_comments)
      {
        fprintf(md_result_file,"\n--\n-- Current Database: %s\n--\n", qdatabase);
        check_io(md_result_file);
      }

      /* Call the view or table specific function */
      init_func(qdatabase);

      fprintf(md_result_file,"\nUSE %s;\n", qdatabase);
      check_io(md_result_file);
    }
  }
  if (extended_insert)
    extended_row.clear();
  return 0;
} /* init_dumping */


/* Return 1 if we should copy the table */

static bool include_table(const char *hash_key, size_t key_size)
{
  string match(hash_key, key_size);
  unordered_set<string>::iterator iter= ignore_table.find(match);
  return (iter == ignore_table.end());
}


static int dump_all_tables_in_db(char *database)
{
  char *table;
  char hash_key[DRIZZLE_MAX_DB_SIZE+DRIZZLE_MAX_TABLE_SIZE+2];  /* "db.tablename" */
  char *afterdot;
  drizzle_result_st result;
  drizzle_return_t ret;

  memset(hash_key, 0, DRIZZLE_MAX_DB_SIZE+DRIZZLE_MAX_TABLE_SIZE+2);
  afterdot= strcpy(hash_key, database) + strlen(database);
  *afterdot++= '.';

  if (init_dumping(database, init_dumping_tables))
    return(1);
  if (opt_xml)
    print_xml_tag(md_result_file, "", "\n", "database", "name=", database, NULL);
  if (flush_logs)
  {
    if (drizzle_query_str(&dcon, &result, "FLUSH LOGS", &ret) == NULL ||
        ret != DRIZZLE_RETURN_OK)
    {
      DB_error(&result, ret, _("when doing refresh"));
      /* We shall continue here, if --force was given */
    }
    else
      drizzle_result_free(&result);
  }
  while ((table= getTableName(0)))
  {
    char *end= strcpy(afterdot, table) + strlen(table);
    if (include_table(hash_key, end - hash_key))
    {
      dump_table(table,database);
      free(order_by);
      order_by= 0;
    }
  }
  if (opt_xml)
  {
    fputs("</database>\n", md_result_file);
    check_io(md_result_file);
  }

  return 0;
} /* dump_all_tables_in_db */


/*
  get_actual_table_name -- executes a SHOW TABLES LIKE '%s' to get the actual
  table name from the server for the table name given on the command line.
  we do this because the table name given on the command line may be a
  different case (e.g.  T1 vs t1)

  RETURN
  pointer to the table name
  0 if error
*/

static char *get_actual_table_name(const char *old_table_name,
                                   drizzled::memory::Root *root)
{
  char *name= 0;
  drizzle_result_st result;
  drizzle_row_t  row;
  char query[50 + 2*DRIZZLE_MAX_TABLE_SIZE];
  char show_name_buff[FN_REFLEN];
  uint64_t num_rows;


  /* Check memory for quote_for_like() */
  assert(2*sizeof(old_table_name) < sizeof(show_name_buff));
  snprintf(query, sizeof(query), "SHOW TABLES LIKE %s",
           quote_for_like(old_table_name, show_name_buff));

  if (drizzleclient_query_with_error_report(&dcon, &result, query, false))
    return NULL;

  num_rows= drizzle_result_row_count(&result);
  if (num_rows > 0)
  {
    size_t *lengths;
    /*
      Return first row
      TODO-> Return all matching rows
    */
    row= drizzle_row_next(&result);
    lengths= drizzle_row_field_sizes(&result);
    name= root->strmake_root(row[0], lengths[0]);
  }
  drizzle_result_free(&result);

  return(name);
}


static int dump_selected_tables(const string &db, const vector<string> &table_names)
{
  drizzled::memory::Root root;
  char **dump_tables, **pos, **end;
  drizzle_result_st result;
  drizzle_return_t ret;


  if (init_dumping((char *)db.c_str(), init_dumping_tables))
    return(1);

  root.init_alloc_root(8192);
  if (!(dump_tables= pos= (char**) root.alloc_root(table_names.size() * sizeof(char *))))
    die(EX_EOM, _("alloc_root failure."));

  for (vector<string>::const_iterator it= table_names.begin(); it != table_names.end(); ++it)
  {
    string temp= *it;
    /* the table name passed on commandline may be wrong case */
    if ((*pos= get_actual_table_name(temp.c_str(), &root)))
    {
      pos++;
    }
    else
    {
      if (!ignore_errors)
      {
        root.free_root(MYF(0));
      }
      maybe_die(EX_ILLEGAL_TABLE, _("Couldn't find table: \"%s\""),(char *) temp.c_str());
      /* We shall countinue here, if --force was given */
    }
  }
  end= pos;

  if (flush_logs)
  {
    if (drizzle_query_str(&dcon, &result, "FLUSH LOGS", &ret) == NULL ||
        ret != DRIZZLE_RETURN_OK)
    {
      if (!ignore_errors)
        root.free_root(MYF(0));
      DB_error(&result, ret, _("when doing refresh"));
      /* We shall countinue here, if --force was given */
    }
    else
      drizzle_result_free(&result);
  }
  if (opt_xml)
    print_xml_tag(md_result_file, "", "\n", "database", "name=", (char *)db.c_str(), NULL);

  /* Dump each selected table */
  for (pos= dump_tables; pos < end; pos++)
    dump_table(*pos, (char *)db.c_str());

  root.free_root(MYF(0));
  free(order_by);
  order_by= 0;
  if (opt_xml)
  {
    fputs("</database>\n", md_result_file);
    check_io(md_result_file);
  }
  return 0;
} /* dump_selected_tables */

static int do_flush_tables_read_lock(drizzle_con_st *drizzle_con)
{
  /*
    We do first a FLUSH TABLES. If a long update is running, the FLUSH TABLES
    will wait but will not stall the whole mysqld, and when the long update is
    done the FLUSH TABLES WITH READ LOCK will start and succeed quickly. So,
    FLUSH TABLES is to lower the probability of a stage where both drizzled
    and most client connections are stalled. Of course, if a second long
    update starts between the two FLUSHes, we have that bad stall.
  */
  return
    ( drizzleclient_query_with_error_report(drizzle_con, 0, "FLUSH TABLES", false) ||
      drizzleclient_query_with_error_report(drizzle_con, 0,
                                            "FLUSH TABLES WITH READ LOCK", false) );
}

static int do_unlock_tables(drizzle_con_st *drizzle_con)
{
  return drizzleclient_query_with_error_report(drizzle_con, 0, "UNLOCK TABLES", false);
}

static int start_transaction(drizzle_con_st *drizzle_con)
{
  return (drizzleclient_query_with_error_report(drizzle_con, 0,
                                                "SET SESSION TRANSACTION ISOLATION "
                                                "LEVEL REPEATABLE READ", false) ||
          drizzleclient_query_with_error_report(drizzle_con, 0,
                                                "START TRANSACTION "
                                                "WITH CONSISTENT SNAPSHOT", false));
}


static uint32_t find_set(TYPELIB *lib, const char *x, uint32_t length,
                         char **err_pos, uint32_t *err_len)
{
  const char *end= x + length;
  uint32_t found= 0;
  uint32_t find;
  char buff[255];

  *err_pos= 0;                  /* No error yet */
  while (end > x && my_isspace(charset_info, end[-1]))
    end--;

  *err_len= 0;
  if (x != end)
  {
    const char *start= x;
    for (;;)
    {
      const char *pos= start;
      uint32_t var_len;

      for (; pos != end && *pos != ','; pos++) ;
      var_len= (uint32_t) (pos - start);
      strncpy(buff, start, min((uint32_t)sizeof(buff), var_len+1));
      find= find_type(buff, lib, var_len);
      if (!find)
      {
        *err_pos= (char*) start;
        *err_len= var_len;
      }
      else
        found|= (uint32_t)((int64_t) 1 << (find - 1));
      if (pos == end)
        break;
      start= pos + 1;
    }
  }
  return found;
}


/* Print a value with a prefix on file */
static void print_value(FILE *file, drizzle_result_st  *result, drizzle_row_t row,
                        const char *prefix, const char *name,
                        int string_value)
{
  drizzle_column_st *column;
  drizzle_column_seek(result, 0);

  for ( ; (column= drizzle_column_next(result)) ; row++)
  {
    if (!strcmp(drizzle_column_name(column),name))
    {
      if (row[0] && row[0][0] && strcmp(row[0],"0")) /* Skip default */
      {
        fputc(' ',file);
        fputs(prefix, file);
        if (string_value)
          unescape(file,row[0],(uint32_t) strlen(row[0]));
        else
          fputs(row[0], file);
        check_io(file);
        return;
      }
    }
  }
  return;                                       /* This shouldn't happen */
} /* print_value */

/**
 * Fetches a row from a result based on a field name
 * Returns const char* of the data in that row or NULL if not found
 */

static const char* fetch_named_row(drizzle_result_st *result, drizzle_row_t row, const char *name)
{
  drizzle_column_st *column;
  drizzle_column_seek(result, 0);

  for ( ; (column= drizzle_column_next(result)) ; row++)
  {
    if (!strcmp(drizzle_column_name(column),name))
    {
      if (row[0] && row[0][0] && strcmp(row[0],"0")) /* Skip default */
      {
        drizzle_column_seek(result, 0);
        return row[0];
      }
    }
  }
  drizzle_column_seek(result, 0);
  return NULL;
}


/*
  SYNOPSIS

  Check if we the table is one of the table types that should be ignored:
  MRG_ISAM, MRG_MYISAM, if opt_delayed, if that table supports delayed inserts.
  If the table should be altogether ignored, it returns a true, false if it
  should not be ignored. If the user has selected to use INSERT DELAYED, it
  sets the value of the bool pointer supports_delayed_inserts to 0 if not
  supported, 1 if it is supported.

  ARGS

  check_if_ignore_table()
  table_name                  Table name to check
  table_type                  Type of table

  GLOBAL VARIABLES
  drizzle                       Drizzle connection
  verbose                     Write warning messages

  RETURN
  char (bit value)            See IGNORE_ values at top
*/

char check_if_ignore_table(const char *table_name, char *table_type)
{
  char result= IGNORE_NONE;
  char buff[FN_REFLEN+80], show_name_buff[FN_REFLEN];
  const char *number_of_rows= NULL;
  drizzle_result_st res;
  drizzle_row_t row;

  /* Check memory for quote_for_like() */
  assert(2*sizeof(table_name) < sizeof(show_name_buff));
  snprintf(buff, sizeof(buff), "show table status like %s",
           quote_for_like(table_name, show_name_buff));
  if (drizzleclient_query_with_error_report(&dcon, &res, buff, false))
  {
    return result;
  }
  if (!(row= drizzle_row_next(&res)))
  {
    fprintf(stderr,
            _("Error: Couldn't read status information for table %s\n"),
            table_name);
    drizzle_result_free(&res);
    return(result);                         /* assume table is ok */
  }
  else
  {
    if ((number_of_rows= fetch_named_row(&res, row, "Rows")) != NULL)
    {
      total_rows= strtoul(number_of_rows, NULL, 10);
    }
  }
  /*
    If the table type matches any of these, we do support delayed inserts.
    Note-> we do not want to skip dumping this table if if is not one of
    these types, but we do want to use delayed inserts in the dump if
    the table type is _NOT_ one of these types
  */
  {
    strncpy(table_type, row[1], DRIZZLE_MAX_TABLE_SIZE-1);
    if (opt_delayed)
    {
      if (strcmp(table_type,"MyISAM") &&
          strcmp(table_type,"ARCHIVE") &&
          strcmp(table_type,"HEAP") &&
          strcmp(table_type,"MEMORY"))
        result= IGNORE_INSERT_DELAYED;
    }
  }
  drizzle_result_free(&res);
  return(result);
}


/*
  Get string of comma-separated primary key field names

  SYNOPSIS
  char *primary_key_fields(const char *table_name)
  RETURNS     pointer to allocated buffer (must be freed by caller)
  table_name  quoted table name

  DESCRIPTION
  Use SHOW KEYS FROM table_name, allocate a buffer to hold the
  field names, and then build that string and return the pointer
  to that buffer.

  Returns NULL if there is no PRIMARY or UNIQUE key on the table,
  or if there is some failure.  It is better to continue to dump
  the table unsorted, rather than exit without dumping the data.
*/

static char *primary_key_fields(const char *table_name)
{
  drizzle_result_st res;
  drizzle_return_t ret;
  drizzle_row_t  row;
  /* SHOW KEYS FROM + table name * 2 (escaped) + 2 quotes + \0 */
  char show_keys_buff[15 + DRIZZLE_MAX_TABLE_SIZE * 2 + 3];
  uint32_t result_length= 0;
  char *result= 0;
  char buff[DRIZZLE_MAX_TABLE_SIZE * 2 + 3];
  char *quoted_field;

  snprintf(show_keys_buff, sizeof(show_keys_buff),
           "SHOW KEYS FROM %s", table_name);
  if (drizzle_query_str(&dcon, &res, show_keys_buff, &ret) == NULL ||
      ret != DRIZZLE_RETURN_OK)
  {
    if (ret == DRIZZLE_RETURN_ERROR_CODE)
    {
      fprintf(stderr, _("Warning: Couldn't read keys from table %s;"
                        " records are NOT sorted (%s)\n"),
              table_name, drizzle_result_error(&res));
      drizzle_result_free(&res);
    }
    else
    {
      fprintf(stderr, _("Warning: Couldn't read keys from table %s;"
                        " records are NOT sorted (%s)\n"),
              table_name, drizzle_con_error(&dcon));
    }

    return result;
  }

  if (drizzle_result_buffer(&res) != DRIZZLE_RETURN_OK)
  {
    fprintf(stderr, _("Warning: Couldn't read keys from table %s;"
                      " records are NOT sorted (%s)\n"),
            table_name, drizzle_con_error(&dcon));
    return result;
  }

  /*
   * Figure out the length of the ORDER BY clause result.
   * Note that SHOW KEYS is ordered:  a PRIMARY key is always the first
   * row, and UNIQUE keys come before others.  So we only need to check
   * the first key, not all keys.
 */
  if ((row= drizzle_row_next(&res)) && atoi(row[1]) == 0)
  {
    /* Key is unique */
    do
    {
      quoted_field= quote_name(row[4], buff, 0);
      result_length+= strlen(quoted_field) + 1; /* + 1 for ',' or \0 */
    } while ((row= drizzle_row_next(&res)) && atoi(row[3]) > 1);
  }

  /* Build the ORDER BY clause result */
  if (result_length)
  {
    char *end;
    /* result (terminating \0 is already in result_length) */

    size_t result_length_alloc= result_length + 10;
    result= (char *)malloc(result_length_alloc);
    if (!result)
    {
      fprintf(stderr, _("Error: Not enough memory to store ORDER BY clause\n"));
      drizzle_result_free(&res);
      return result;
    }
    drizzle_row_seek(&res, 0);
    row= drizzle_row_next(&res);
    quoted_field= quote_name(row[4], buff, 0);
    end= strcpy(result, quoted_field) + strlen(quoted_field);
    result_length_alloc -= strlen(quoted_field);
    while ((row= drizzle_row_next(&res)) && atoi(row[3]) > 1)
    {
      quoted_field= quote_name(row[4], buff, 0);
      end+= snprintf(end, result_length_alloc, ",%s",quoted_field);
      result_length_alloc -= strlen(quoted_field);
    }
  }

  drizzle_result_free(&res);
  return result;
}

int main(int argc, char **argv)
{
try
{
  int exit_code;
  MY_INIT("drizzledump");
  drizzle_result_st result;

  po::options_description commandline_options("Options used only in command line");
  commandline_options.add_options()
  ("all-databases,A", po::value<bool>(&opt_alldbs)->default_value(false)->zero_tokens(),
  "Dump all the databases. This will be same as --databases with all databases selected.")
  ("all-tablespaces,Y", po::value<bool>(&opt_alltspcs)->default_value(false)->zero_tokens(),
  "Dump all the tablespaces.")
  ("complete-insert,c", po::value<bool>(&opt_complete_insert)->default_value(false)->zero_tokens(),
  "Use complete insert statements.")
  ("compress,C", po::value<bool>(&opt_compress)->default_value(false)->zero_tokens(),
  "Use compression in server/client protocol.")
  ("flush-logs,F", po::value<bool>(&flush_logs)->default_value(false)->zero_tokens(),
  "Flush logs file in server before starting dump. Note that if you dump many databases at once (using the option --databases= or --all-databases), the logs will be flushed for each database dumped. The exception is when using --lock-all-tables in this case the logs will be flushed only once, corresponding to the moment all tables are locked. So if you want your dump and the log flush to happen at the same exact moment you should use --lock-all-tables or --flush-logs")
  ("force,f", po::value<bool>(&ignore_errors)->default_value(false)->zero_tokens(),
  "Continue even if we get an sql-error.")
  ("help,?", "Display this help message and exit.")
  ("lock-all-tables,x", po::value<bool>(&opt_lock_all_tables)->default_value(false)->zero_tokens(),
  "Locks all tables across all databases. This is achieved by taking a global read lock for the duration of the whole dump. Automatically turns --single-transaction and --lock-tables off.")
  ("order-by-primary", po::value<bool>(&opt_order_by_primary)->default_value(false)->zero_tokens(),
  "Sorts each table's rows by primary key, or first unique key, if such a key exists.  Useful when dumping a MyISAM table to be loaded into an InnoDB table, but will make the dump itself take considerably longer.")
  ("routines,R", po::value<bool>(&opt_routines)->default_value(false)->zero_tokens(),
  "Dump stored routines (functions and procedures).")
  ("single-transaction", po::value<bool>(&opt_single_transaction)->default_value(false)->zero_tokens(),
  "Creates a consistent snapshot by dumping all tables in a single transaction. Works ONLY for tables stored in storage engines which support multiversioning (currently only InnoDB does); the dump is NOT guaranteed to be consistent for other storage engines. While a --single-transaction dump is in process, to ensure a valid dump file (correct table contents), no other connection should use the following statements: ALTER TABLE, DROP TABLE, RENAME TABLE, TRUNCATE TABLE, as consistent snapshot is not isolated from them. Option automatically turns off --lock-tables.")
  ("opt", "Same as --add-drop-table, --add-locks, --create-options, --quick, --extended-insert, --lock-tables, --set-charset, and --disable-keys. Enabled by default, disable with --skip-opt.") 
  ("skip-opt", 
  "Disable --opt. Disables --add-drop-table, --add-locks, --create-options, --quick, --extended-insert, --lock-tables, --set-charset, and --disable-keys.")    
  ("tables", "Overrides option --databases (-B).")
  ("show-progress-size", po::value<uint32_t>(&show_progress_size)->default_value(10000),
  N_("Number of rows before each output progress report (requires --verbose)."))
  ("verbose,v", po::value<bool>(&verbose)->default_value(false)->zero_tokens(),
  "Print info about the various stages.")
  ("version,V", "Output version information and exit.")
  ("xml,X", "Dump a database as well formed XML.")
  ("skip-comments", "Turn off Comments")
  ("skip-create", "Turn off create-options")
  ("skip-extended-insert", "Turn off extended-insert") 
  ("skip-dump-date", "Turn off dump-date")
  ("no-defaults", "Do not read from the configuration files")
  ;

  po::options_description dump_options("Options specific to the drizzle client");
  dump_options.add_options()
  ("add-drop-database", po::value<bool>(&opt_drop_database)->default_value(false)->zero_tokens(),
  "Add a 'DROP DATABASE' before each create.")
  ("add-drop-table", po::value<bool>(&opt_drop)->default_value(true)->zero_tokens(),
  "Add a 'drop table' before each create.")
  ("allow-keywords", po::value<bool>(&opt_keywords)->default_value(false)->zero_tokens(),
  "Allow creation of column names that are keywords.")
  ("comments,i", po::value<bool>(&opt_comments)->default_value(true)->zero_tokens(),
  "Write additional information.")
  ("compatible", po::value<string>(&opt_compatible_mode_str)->default_value(""),
  "Change the dump to be compatible with a given mode. By default tables are dumped in a format optimized for MySQL. Legal modes are: ansi, mysql323, mysql40, postgresql, oracle, mssql, db2, maxdb, no_key_options, no_table_options, no_field_options. One can use several modes separated by commas. Note: Requires DRIZZLE server version 4.1.0 or higher. This option is ignored with earlier server versions.")
  ("compact", po::value<bool>(&opt_compact)->default_value(false)->zero_tokens(),
  "Give less verbose output (useful for debugging). Disables structure comments and header/footer constructs.  Enables options --skip-add-drop-table --no-set-names --skip-disable-keys --skip-add-locks")
  ("create-options", po::value<bool>(&create_options)->default_value(true)->zero_tokens(),
  "Include all DRIZZLE specific create options.")
  ("dump-date", po::value<bool>(&opt_dump_date)->default_value(true)->zero_tokens(),
  "Put a dump date to the end of the output.")
  ("databases,B", po::value<bool>(&opt_databases)->default_value(false)->zero_tokens(),
  "To dump several databases. Note the difference in usage; In this case no tables are given. All name arguments are regarded as databasenames. 'USE db_name;' will be included in the output.")
  ("delayed-insert", po::value<bool>(&opt_delayed)->default_value(false)->zero_tokens(),
  "Insert rows with INSERT DELAYED; ")
  ("disable-keys,K", po::value<bool>(&opt_disable_keys)->default_value(true)->zero_tokens(),
  "'ALTER TABLE tb_name DISABLE KEYS; and 'ALTER TABLE tb_name ENABLE KEYS; will be put in the output.")
  ("extended-insert,e", po::value<bool>(&extended_insert)->default_value(true)->zero_tokens(),
  "Allows utilization of the new, much faster INSERT syntax.")
  ("fields-terminated-by", po::value<string>(&fields_terminated)->default_value(""),
  "Fields in the textfile are terminated by ...")
  ("fields-enclosed-by", po::value<string>(&enclosed)->default_value(""),
  "Fields in the importfile are enclosed by ...")
  ("fields-optionally-enclosed-by", po::value<string>(&opt_enclosed)->default_value(""),
  "Fields in the i.file are opt. enclosed by ...")
  ("fields-escaped-by", po::value<string>(&escaped)->default_value(""),
  "Fields in the i.file are escaped by ...")
  ("hex-blob", po::value<bool>(&opt_hex_blob)->default_value(false)->zero_tokens(),
  "Dump binary strings (BINARY, VARBINARY, BLOB) in hexadecimal format.")
  ("ignore-table", po::value<string>(),
  "Do not dump the specified table. To specify more than one table to ignore, use the directive multiple times, once for each table.  Each table must be specified with both database and table names, e.g. --ignore-table=database.table")
  ("insert-ignore", po::value<bool>(&opt_ignore)->default_value(false)->zero_tokens(),
  "Insert rows with INSERT IGNORE.")
  ("lines-terminated-by", po::value<string>(&lines_terminated)->default_value(""),
  "Lines in the i.file are terminated by ...")
  ("no-autocommit", po::value<bool>(&opt_autocommit)->default_value(false)->zero_tokens(),
  "Wrap tables with autocommit/commit statements.")
  ("no-create-db,n", po::value<bool>(&opt_create_db)->default_value(false)->zero_tokens(),
  "'CREATE DATABASE IF NOT EXISTS db_name;' will not be put in the output. The above line will be added otherwise, if --databases or --all-databases option was given.}.")
  ("no-create-info,t", po::value<bool>(&opt_no_create_info)->default_value(false)->zero_tokens(),
  "Don't write table creation info.")
  ("no-data,d", po::value<bool>(&opt_no_data)->default_value(false)->zero_tokens(),
  "No row information.")
  ("no-set-names,N", "Deprecated. Use --skip-set-charset instead.")
  ("set-charset", po::value<bool>(&opt_set_charset)->default_value(false)->zero_tokens(),
  "Enable set-name")
  ("quick,q", po::value<bool>(&quick)->default_value(true)->zero_tokens(),
  "Don't buffer query, dump directly to stdout.")
  ("quote-names,Q", po::value<bool>(&opt_quoted)->default_value(true)->zero_tokens(),
  "Quote table and column names with backticks (`).")
  ("replace", po::value<bool>(&opt_replace_into)->default_value(false)->zero_tokens(),
  "Use REPLACE INTO instead of INSERT INTO.")
  ("result-file,r", po::value<string>(),
  "Direct output to a given file. This option should be used in MSDOS, because it prevents new line '\\n' from being converted to '\\r\\n' (carriage return + line feed).")  
  ("tab,T", po::value<string>(&path)->default_value(""),
  "Creates tab separated textfile for each table to given path. (creates .sql and .txt files). NOTE: This only works if drizzledump is run on the same machine as the drizzled daemon.")
  ("where,w", po::value<string>(&where)->default_value(""),
  "Dump only selected records; QUOTES mandatory!")
  ;

  po::options_description client_options("Options specific to the client");
  client_options.add_options()
  ("host,h", po::value<string>(&current_host)->default_value("localhost"),
  "Connect to host.")
  ("mysql,m", po::value<bool>(&opt_mysql)->default_value(true)->zero_tokens(),
  N_("Use MySQL Protocol."))
  ("password,P", po::value<string>(&password)->default_value(PASSWORD_SENTINEL),
  "Password to use when connecting to server. If password is not given it's solicited on the tty.")
  ("port,p", po::value<uint32_t>(&opt_drizzle_port)->default_value(0),
  "Port number to use for connection.")
  ("user,u", po::value<string>(&current_user)->default_value(""),
  "User for login if not current user.")
  ("protocol",po::value<string>(),
  "The protocol of connection (tcp,socket,pipe,memory).")
  ;

  po::options_description hidden_options("Hidden Options");
  hidden_options.add_options()
  ("database-used", po::value<vector<string> >(), "Used to select the database")
  ("Table-used", po::value<vector<string> >(), "Used to select the tables")
  ;

  po::options_description all_options("Allowed Options + Hidden Options");
  all_options.add(commandline_options).add(dump_options).add(client_options).add(hidden_options);

  po::options_description long_options("Allowed Options");
  long_options.add(commandline_options).add(dump_options).add(client_options);

  std::string system_config_dir_dump(SYSCONFDIR); 
  system_config_dir_dump.append("/drizzle/drizzledump.cnf");

  std::string system_config_dir_client(SYSCONFDIR); 
  system_config_dir_client.append("/drizzle/client.cnf");

  std::string user_config_dir((getenv("XDG_CONFIG_HOME")? getenv("XDG_CONFIG_HOME"):"~/.config"));

  po::positional_options_description p;
  p.add("database-used", 1);
  p.add("Table-used",-1);

  compatible_mode_normal_str[0]= 0;
  
  md_result_file= stdout;

  po::variables_map vm;

  po::store(po::command_line_parser(argc, argv).options(all_options).
            positional(p).extra_parser(parse_password_arg).run(), vm);

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
  
  if ( ! vm.count("database-used") && ! vm.count("Table-used") && ! opt_alldbs && path.empty())
  {
    printf(_("Usage: %s [OPTIONS] database [tables]\n"), internal::my_progname);
    printf(_("OR     %s [OPTIONS] --databases [OPTIONS] DB1 [DB2 DB3...]\n"),
          internal::my_progname);
    printf(_("OR     %s [OPTIONS] --all-databases [OPTIONS]\n"), internal::my_progname);
    exit(1);
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

  if(vm.count("password"))
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

  if (vm.count("result-file"))
  {
    if (!(md_result_file= fopen(vm["result-file"].as<string>().c_str(), "w")))
      exit(1);
  }

  if (vm.count("no-set-names"))
  {
    opt_set_charset= 0;
  }
 
  if (! path.empty())
  { 
    opt_disable_keys= 0;

    if (vm["tab"].as<string>().length() >= FN_REFLEN)
    {
      /*
        This check is made because the some the file functions below
        have FN_REFLEN sized stack allocated buffers and will cause
        a crash even if the input destination buffer is large enough
        to hold the output.
      */
      fprintf(stderr, _("Input filename too long: %s"), vm["tab"].as<string>().c_str());
      exit(EXIT_ARGUMENT_INVALID);
    }
  }

  if (vm.count("version"))
  {
     printf(_("%s  Drizzle %s libdrizzle %s, for %s-%s (%s)\n"), internal::my_progname,
       VERSION, drizzle_version(), HOST_VENDOR, HOST_OS, HOST_CPU);
  }
 
  if (vm.count("xml"))
  { 
    opt_xml= 1;
    extended_insert= opt_drop= opt_disable_keys= opt_autocommit= opt_create_db= 0;
  }

  if (vm.count("help"))
  {
    printf(_("%s  Drizzle %s libdrizzle %s, for %s-%s (%s)\n"), internal::my_progname,
      VERSION, drizzle_version(), HOST_VENDOR, HOST_OS, HOST_CPU);
    puts("");
    puts(_("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n"));
    puts(_("Dumps definitions and data from a Drizzle database server"));
    cout << long_options;
    printf(_("Usage: %s [OPTIONS] database [tables]\n"), internal::my_progname);
    printf(_("OR     %s [OPTIONS] --databases [OPTIONS] DB1 [DB2 DB3...]\n"),
          internal::my_progname);
    printf(_("OR     %s [OPTIONS] --all-databases [OPTIONS]\n"), internal::my_progname);
    exit(1);
  }
  
  if (vm.count("skip-opt"))
  {
    extended_insert= opt_drop= quick= create_options= 0;
    opt_disable_keys= opt_set_charset= 0;
  }

  if (opt_compact)
  { 
    opt_comments= opt_drop= opt_disable_keys= 0;
    opt_set_charset= 0;
  }

  if (vm.count("opt"))
  {
    extended_insert= opt_drop= quick= create_options= 1;
    opt_disable_keys= opt_set_charset= 1;
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

  if (vm.count("skip-comments"))
  {
    opt_comments= false; 
  }

  if (vm.count("skip-extended-insert"))
  {
    extended_insert= false; 
  }

  if (vm.count("skip-dump-date"))
  {
    opt_dump_date= false; 
  } 

  if (! opt_compatible_mode_str.empty())
  {
    char buff[255];
    char *end= compatible_mode_normal_str;
    uint32_t i;
    uint32_t mode;
    uint32_t error_len;

    opt_quoted= 1;
    opt_set_charset= 0;
    opt_compatible_mode= find_set(&compatible_mode_typelib,
                                            opt_compatible_mode_str.c_str(), opt_compatible_mode_str.length(),
                                            &err_ptr, &error_len);
    if (error_len)
    {
      strncpy(buff, err_ptr, min((uint32_t)sizeof(buff), error_len+1));
      fprintf(stderr, _("Invalid mode to --compatible: %s\n"), buff);
      exit(EXIT_ARGUMENT_INVALID);
    }
    mode= opt_compatible_mode;
    for (i= 0, mode= opt_compatible_mode; mode; mode>>= 1, i++)
    {
      if (mode & 1)
      {
        uint32_t len = strlen(compatible_mode_names[i]);
        end= strcpy(end, compatible_mode_names[i]) + len;
        end= strcpy(end, ",")+1;
      }
    }
    if (end!=compatible_mode_normal_str)
      end[-1]= 0;
  }
 

  exit_code= get_options();
  if (exit_code)
  {
    free_resources();
    exit(exit_code);
  }

  if (connect_to_db(current_host, current_user, opt_password))
  {
    free_resources();
    exit(EX_DRIZZLEERR);
  }
  if (path.empty() && vm.count("database-used"))
  {
    string database_used= *vm["database-used"].as< vector<string> >().begin();
    write_header(md_result_file, (char *)database_used.c_str());
  }

  if ((opt_lock_all_tables) && do_flush_tables_read_lock(&dcon))
    goto err;
  if (opt_single_transaction && start_transaction(&dcon))
    goto err;
  if (opt_lock_all_tables)
  {
    if (drizzleclient_query_with_error_report(&dcon, &result, "FLUSH LOGS", false))
      goto err;
    drizzle_result_free(&result);
    flush_logs= 0; /* not anymore; that would not be sensible */
  }
  if (opt_single_transaction && do_unlock_tables(&dcon)) /* unlock but no commit! */
    goto err;

  if (opt_alldbs)
  {
    dump_all_databases();
  }
  if (vm.count("database-used") && vm.count("Table-used") && ! opt_databases)
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
  }
  
  if (vm.count("database-used") && ! vm.count("Table-used"))
  {
    dump_databases(vm["database-used"].as< vector<string> >());
  }

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
  dbDisconnect(current_host);
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
