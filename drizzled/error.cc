/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2000 MySQL AB
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
 * Errors a drizzled can give you
 */

#include "config.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/definitions.h"
#include "drizzled/error.h"
#include "drizzled/gettext.h"

#include <boost/unordered_map.hpp>
#include <exception>

namespace drizzled
{
namespace
{

class ErrorStringNotFound: public std::exception
{
public:
  ErrorStringNotFound(uint32_t code)
    : error_num_(code)
  {}
  uint32_t error_num() const
  {
    return error_num_;
  }
private:
  uint32_t error_num_;
};

/*
 * Provides a mapping from the error enum values to std::strings.
 */
class ErrorMap
{
public:
  ErrorMap();

  // Insert the message for the error.  If the error already has an existing
  // mapping, an error is logged, but the function continues.
  void add(uint32_t error_num, const std::string &message);

  // If there is no error mapping for the error_num, ErrorStringNotFound is raised.
  const std::string &find(uint32_t error_num) const;

private:
  // Disable copy and assignment.
  ErrorMap(const ErrorMap &e);
  ErrorMap& operator=(const ErrorMap &e);

  typedef boost::unordered_map<uint32_t, std::string> ErrorMessageMap;
  ErrorMessageMap mapping_;
};

ErrorMap& get_error_map()
{
  static ErrorMap errors;
  return errors;
}

} // anonymous namespace

void add_error_message(uint32_t error_code, const std::string &message)
{
  get_error_map().add(error_code, message);
}

const char * error_message(unsigned int code)
{
  try
  {
    return get_error_map().find(code).c_str();
  }
  catch (ErrorStringNotFound const& e)
  {
    return get_error_map().find(ER_UNKNOWN_ERROR).c_str();
  }
}

error_handler_func error_handler_hook= NULL;

/*
  WARNING!
  my_error family functions have to be used according following rules:
  - if message have not parameters use my_message(ER_CODE, ER(ER_CODE), MYF(N))
  - if message registered use my_error(ER_CODE, MYF(N), ...).
  - With some special text of errror message use:
  my_printf_error(ER_CODE, format, MYF(N), ...)
*/

/*
   Error message to user

   SYNOPSIS
     my_error()
       nr	Errno
       MyFlags	Flags
       ...	variable list
*/

void my_error(int nr, myf MyFlags, ...)
{
  std::string format;
  va_list args;
  char ebuff[ERRMSGSIZE + 20];

  try
  {
    format= get_error_map().find(nr);
    va_start(args,MyFlags);
    (void) vsnprintf (ebuff, sizeof(ebuff), _(format.c_str()), args);
    va_end(args);
  }
  catch (ErrorStringNotFound const& e)
  {
    (void) snprintf (ebuff, sizeof(ebuff), _("Unknown error %d"), nr);
  }
  (*error_handler_hook)(nr, ebuff, MyFlags);
}

/*
  Error as printf

         SYNOPSIS
    my_printf_error()
      error	Errno
      format	Format string
      MyFlags	Flags
      ...	variable list
*/

void my_printf_error(uint32_t error, const char *format, myf MyFlags, ...)
{
  va_list args;
  char ebuff[ERRMSGSIZE+20];

  va_start(args,MyFlags);
  (void) vsnprintf (ebuff, sizeof(ebuff), format, args);
  va_end(args);
  (*error_handler_hook)(error, ebuff, MyFlags);
  return;
}

/*
  Give message using error_handler_hook

  SYNOPSIS
    my_message()
      error	Errno
      str	Error message
      MyFlags	Flags
*/

void my_message(uint32_t error, const char *str, register myf MyFlags)
{
  (*error_handler_hook)(error, str, MyFlags);
}


namespace
{

// Insert the message for the error.  If the error already has an existing
// mapping, an error is logged, but the function continues.
void ErrorMap::add(uint32_t error_num, const std::string &message)
{
  if (mapping_.find(error_num) == mapping_.end())
  {
    // Log the error.
    mapping_[error_num]= message;
  }
  else
  {
    mapping_.insert(ErrorMessageMap::value_type(error_num, message));
  }
}

const std::string &ErrorMap::find(uint32_t error_num) const
{
  ErrorMessageMap::const_iterator pos= mapping_.find(error_num);
  if (pos == mapping_.end())
  {
    throw ErrorStringNotFound(error_num);
  }
  return pos->second;
}


// Constructor sets the default mappings.
ErrorMap::ErrorMap()
{
  add(ER_NO, N_("NO"));
  add(ER_YES, N_("YES"));
  add(ER_CANT_CREATE_FILE, N_("Can't create file '%-.200s' (errno: %d)"));
  add(ER_CANT_CREATE_TABLE, N_("Can't create table '%-.200s' (errno: %d)"));
  add(ER_CANT_CREATE_DB, N_("Can't create database '%-.192s' (errno: %d)"));
  add(ER_DB_CREATE_EXISTS, N_("Can't create database '%-.192s'; database exists"));
  add(ER_DB_DROP_EXISTS, N_("Can't drop database '%-.192s'; database doesn't exist"));
  add(ER_CANT_DELETE_FILE, N_("Error on delete of '%-.192s' (errno: %d)"));
  add(ER_CANT_GET_STAT, N_("Can't get status of '%-.200s' (errno: %d)"));
  add(ER_CANT_LOCK, N_("Can't lock file (errno: %d)"));
  add(ER_CANT_OPEN_FILE, N_("Can't open file: '%-.200s' (errno: %d)"));
  add(ER_FILE_NOT_FOUND, N_("Can't find file: '%-.200s' (errno: %d)"));
  add(ER_CANT_READ_DIR, N_("Can't read dir of '%-.192s' (errno: %d)"));
  add(ER_CHECKREAD, N_("Record has changed since last read in table '%-.192s'"));
  add(ER_DISK_FULL, N_("Disk full (%s); waiting for someone to free some space..."));
  add(ER_DUP_KEY, N_("Can't write; duplicate key in table '%-.192s'"));
  add(ER_ERROR_ON_CLOSE, N_("Error on close of '%-.192s' (errno: %d)"));
  add(ER_ERROR_ON_READ, N_("Error reading file '%-.200s' (errno: %d)"));
  add(ER_ERROR_ON_RENAME, N_("Error on rename of '%-.150s' to '%-.150s' (errno: %d)"));
  add(ER_ERROR_ON_WRITE, N_("Error writing file '%-.200s' (errno: %d)"));
  add(ER_FILE_USED, N_("'%-.192s' is locked against change"));
  add(ER_FILSORT_ABORT, N_("Sort aborted"));
  add(ER_GET_ERRNO, N_("Got error %d from storage engine"));
  add(ER_ILLEGAL_HA, N_("Table storage engine for '%-.192s' doesn't have this option"));
  add(ER_KEY_NOT_FOUND, N_("Can't find record in '%-.192s'"));
  add(ER_NOT_FORM_FILE, N_("Incorrect information in file: '%-.200s'"));
  add(ER_NOT_KEYFILE, N_("Incorrect key file for table '%-.200s'; try to repair it"));
  add(ER_OLD_KEYFILE, N_("Old key file for table '%-.192s'; repair it!"));
  add(ER_OPEN_AS_READONLY, N_("Table '%-.192s' is read only"));
  add(ER_OUTOFMEMORY, N_("Out of memory; restart server and try again (needed %lu bytes)"));
  add(ER_OUT_OF_SORTMEMORY, N_("Out of sort memory; increase server sort buffer size"));
  add(ER_UNEXPECTED_EOF, N_("Unexpected EOF found when reading file '%-.192s' (errno: %d)"));
  add(ER_CON_COUNT_ERROR, N_("Too many connections"));
  add(ER_OUT_OF_RESOURCES, N_("Out of memory; check if drizzled or some other process uses all available memory; if not, you may have to use 'ulimit' to allow drizzled to use more memory or you can add more swap space"));
  add(ER_BAD_HOST_ERROR, N_("Can't get hostname for your address"));
  add(ER_HANDSHAKE_ERROR, N_("Bad handshake"));
  add(ER_DBACCESS_DENIED_ERROR, N_("Access denied for user '%-.48s'@'%-.64s' to database '%-.192s'"));
  add(ER_ACCESS_DENIED_ERROR, N_("Access denied for user '%-.48s'@'%-.64s' (using password: %s)"));
  add(ER_NO_DB_ERROR, N_("No database selected"));
  add(ER_UNKNOWN_COM_ERROR, N_("Unknown command"));
  add(ER_BAD_NULL_ERROR, N_("Column '%-.192s' cannot be null"));
  add(ER_BAD_DB_ERROR, N_("Unknown database '%-.192s'"));
  add(ER_TABLE_EXISTS_ERROR, N_("Table '%-.192s' already exists"));
  add(ER_BAD_TABLE_ERROR, N_("Unknown table '%-.100s'"));
  add(ER_NON_UNIQ_ERROR, N_("Column '%-.192s' in %-.192s is ambiguous"));
  add(ER_SERVER_SHUTDOWN, N_("Server shutdown in progress"));
  add(ER_BAD_FIELD_ERROR, N_("Unknown column '%-.192s' in '%-.192s'"));
  add(ER_WRONG_FIELD_WITH_GROUP, N_("'%-.192s' isn't in GROUP BY"));
  add(ER_WRONG_GROUP_FIELD, N_("Can't group on '%-.192s'"));
  add(ER_WRONG_SUM_SELECT, N_("Statement has sum functions and columns in same statement"));
  add(ER_WRONG_VALUE_COUNT, N_("Column count doesn't match value count"));
  add(ER_TOO_LONG_IDENT, N_("Identifier name '%-.100s' is too long"));
  add(ER_DUP_FIELDNAME, N_("Duplicate column name '%-.192s'"));
  add(ER_DUP_KEYNAME, N_("Duplicate key name '%-.192s'"));
  add(ER_DUP_ENTRY, N_("Duplicate entry '%-.192s' for key %d"));
  add(ER_WRONG_FIELD_SPEC, N_("Incorrect column specifier for column '%-.192s'"));
  add(ER_PARSE_ERROR, N_("%s near '%-.80s' at line %d"));
  add(ER_EMPTY_QUERY, N_("Query was empty"));
  add(ER_NONUNIQ_TABLE, N_("Not unique table/alias: '%-.192s'"));
  add(ER_INVALID_DEFAULT, N_("Invalid default value for '%-.192s'"));
  add(ER_MULTIPLE_PRI_KEY, N_("Multiple primary key defined"));
  add(ER_TOO_MANY_KEYS, N_("Too many keys specified; max %d keys allowed"));
  add(ER_TOO_MANY_KEY_PARTS, N_("Too many key parts specified; max %d parts allowed"));
  add(ER_TOO_LONG_KEY, N_("Specified key was too long; max key length is %d bytes"));
  add(ER_KEY_COLUMN_DOES_NOT_EXITS, N_("Key column '%-.192s' doesn't exist in table"));
  add(ER_BLOB_USED_AS_KEY, N_("BLOB column '%-.192s' can't be used in key specification with the used table type"));
  add(ER_TOO_BIG_FIELDLENGTH, N_("Column length too big for column '%-.192s' (max = %d); use BLOB or TEXT instead"));
  add(ER_WRONG_AUTO_KEY, N_("Incorrect table definition; there can be only one auto column and it must be defined as a key"));
  add(ER_NORMAL_SHUTDOWN, N_("%s: Normal shutdown\n"));
  add(ER_GOT_SIGNAL, N_("%s: Got signal %d. Aborting!\n"));
  add(ER_SHUTDOWN_COMPLETE, N_("%s: Shutdown complete\n"));
  add(ER_FORCING_CLOSE, N_("%s: Forcing close of thread %" PRIu64 " user: '%-.48s'\n"));
  add(ER_IPSOCK_ERROR, N_("Can't create IP socket"));
  add(ER_NO_SUCH_INDEX, N_("Table '%-.192s' has no index like the one used in CREATE INDEX; recreate the table"));
  add(ER_WRONG_FIELD_TERMINATORS, N_("Field separator argument '%-.32s' with length '%d' is not what is expected; check the manual"));
  add(ER_BLOBS_AND_NO_TERMINATED, N_("You can't use fixed rowlength with BLOBs; please use 'fields terminated by'"));
  add(ER_TEXTFILE_NOT_READABLE, N_("The file '%-.128s' must be in the database directory or be readable by all"));
  add(ER_FILE_EXISTS_ERROR, N_("File '%-.200s' already exists"));
  add(ER_LOAD_INFO, N_("Records: %ld  Deleted: %ld  Skipped: %ld  Warnings: %ld"));
  add(ER_WRONG_SUB_KEY, N_("Incorrect prefix key; the used key part isn't a string, the used length is longer than the key part, or the storage engine doesn't support unique prefix keys"));
  add(ER_CANT_REMOVE_ALL_FIELDS, N_("You can't delete all columns with ALTER TABLE; use DROP TABLE instead"));
  add(ER_CANT_DROP_FIELD_OR_KEY, N_("Can't DROP '%-.192s'; check that column/key exists"));
  add(ER_INSERT_INFO, N_("Records: %ld  Duplicates: %ld  Warnings: %ld"));
  add(ER_UPDATE_TABLE_USED, N_("You can't specify target table '%-.192s' for update in FROM clause"));
  add(ER_NO_SUCH_THREAD, N_("Unknown thread id: %lu"));
  add(ER_KILL_DENIED_ERROR, N_("You are not owner of thread %lu"));
  add(ER_NO_TABLES_USED, N_("No tables used"));
  add(ER_BLOB_CANT_HAVE_DEFAULT, N_("BLOB/TEXT column '%-.192s' can't have a default value"));
  add(ER_WRONG_DB_NAME, N_("Incorrect database name '%-.100s'"));
  add(ER_WRONG_TABLE_NAME, N_("Incorrect table name '%-.100s'"));
  add(ER_TOO_BIG_SELECT, N_("The SELECT would examine more than MAX_JOIN_SIZE rows; check your WHERE and use SET SQL_BIG_SELECTS=1 or SET MAX_JOIN_SIZE=# if the SELECT is okay"));
  add(ER_UNKNOWN_ERROR, N_("Unknown error"));
  add(ER_UNKNOWN_PROCEDURE, N_("Unknown procedure '%-.192s'"));
  add(ER_WRONG_PARAMCOUNT_TO_PROCEDURE, N_("Incorrect parameter count to procedure '%-.192s'"));
  add(ER_UNKNOWN_TABLE, N_("Unknown table '%-.192s' in %-.32s"));
  add(ER_FIELD_SPECIFIED_TWICE, N_("Column '%-.192s' specified twice"));
  add(ER_INVALID_GROUP_FUNC_USE, N_("Invalid use of group function"));
  add(ER_UNSUPPORTED_EXTENSION, N_("Table '%-.192s' uses an extension that doesn't exist in this Drizzle version"));
  add(ER_TABLE_MUST_HAVE_COLUMNS, N_("A table must have at least 1 column"));
  add(ER_RECORD_FILE_FULL, N_("The table '%-.192s' is full"));
  add(ER_TOO_MANY_TABLES, N_("Too many tables; Drizzle can only use %d tables in a join"));
  add(ER_TOO_MANY_FIELDS, N_("Too many columns"));
  add(ER_TOO_BIG_ROWSIZE, N_("Row size too large. The maximum row size for the used table type, not counting BLOBs, is %ld. You have to change some columns to TEXT or BLOBs"));
  add(ER_WRONG_OUTER_JOIN, N_("Cross dependency found in OUTER JOIN; examine your ON conditions"));
  add(ER_NULL_COLUMN_IN_INDEX, N_("Table handler doesn't support NULL in given index. Please change column '%-.192s' to be NOT NULL or use another handler"));
  add(ER_PLUGIN_NO_PATHS, N_("No paths allowed for plugin library"));
  add(ER_PLUGIN_EXISTS, N_("Plugin '%-.192s' already exists"));
  add(ER_CANT_OPEN_LIBRARY, N_("Can't open shared library '%-.192s' (errno: %d %-.128s)"));
  add(ER_CANT_FIND_DL_ENTRY, N_("Can't find symbol '%-.128s' in library '%-.128s'"));
  add(ER_UPDATE_INFO, N_("Rows matched: %ld  Changed: %ld  Warnings: %ld"));
  add(ER_CANT_CREATE_THREAD, N_("Can't create a new thread (errno %d); if you are not out of available memory, you can consult the manual for a possible OS-dependent bug"));
  add(ER_WRONG_VALUE_COUNT_ON_ROW, N_("Column count doesn't match value count at row %ld"));
  add(ER_CANT_REOPEN_TABLE, N_("Can't reopen table: '%-.192s'"));
  add(ER_MIX_OF_GROUP_FUNC_AND_FIELDS, N_("Mixing of GROUP columns (MIN(),MAX(),COUNT(),...) with no GROUP columns is illegal if there is no GROUP BY clause"));
  add(ER_NO_SUCH_TABLE, N_("Table '%-.192s.%-.192s' doesn't exist"));
  add(ER_SYNTAX_ERROR, N_("You have an error in your SQL syntax; check the manual that corresponds to your Drizzle server version for the right syntax to use"));
  add(ER_NET_PACKET_TOO_LARGE, N_("Got a packet bigger than 'max_allowed_packet' bytes"));
  add(ER_NET_READ_ERROR_FROM_PIPE, N_("Got a read error from the connection pipe"));
  add(ER_NET_FCNTL_ERROR, N_("Got an error from fcntl()"));
  add(ER_NET_PACKETS_OUT_OF_ORDER, N_("Got packets out of order"));
  add(ER_NET_UNCOMPRESS_ERROR, N_("Couldn't uncompress communication packet"));
  add(ER_NET_READ_ERROR, N_("Got an error reading communication packets"));
  add(ER_NET_READ_INTERRUPTED, N_("Got timeout reading communication packets"));
  add(ER_NET_ERROR_ON_WRITE, N_("Got an error writing communication packets"));
  add(ER_NET_WRITE_INTERRUPTED, N_("Got timeout writing communication packets"));
  add(ER_TOO_LONG_STRING, N_("Result string is longer than 'max_allowed_packet' bytes"));
  add(ER_TABLE_CANT_HANDLE_BLOB, N_("The used table type doesn't support BLOB/TEXT columns"));
  add(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT, N_("The used table type doesn't support AUTO_INCREMENT columns"));
  add(ER_WRONG_COLUMN_NAME, N_("Incorrect column name '%-.100s'"));
  add(ER_WRONG_KEY_COLUMN, N_("The used storage engine can't index column '%-.192s'"));
  add(ER_WRONG_MRG_TABLE, N_("Unable to open underlying table which is differently defined or of non-MyISAM type or doesn't exist"));
  add(ER_DUP_UNIQUE, N_("Can't write, because of unique constraint, to table '%-.192s'"));
  add(ER_BLOB_KEY_WITHOUT_LENGTH, N_("BLOB/TEXT column '%-.192s' used in key specification without a key length"));
  add(ER_PRIMARY_CANT_HAVE_NULL, N_("All parts of a PRIMARY KEY must be NOT NULL; if you need NULL in a key, use UNIQUE instead"));
  add(ER_TOO_MANY_ROWS, N_("Result consisted of more than one row"));
  add(ER_REQUIRES_PRIMARY_KEY, N_("This table type requires a primary key"));
  add(ER_KEY_DOES_NOT_EXITS, N_("Key '%-.192s' doesn't exist in table '%-.192s'"));
  add(ER_CHECK_NO_SUCH_TABLE, N_("Can't open table"));
  add(ER_CHECK_NOT_IMPLEMENTED, N_("The storage engine for the table doesn't support %s"));
  add(ER_CANT_DO_THIS_DURING_AN_TRANSACTION, N_("You are not allowed to execute this command in a transaction"));
  add(ER_ERROR_DURING_COMMIT, N_("Got error %d during COMMIT"));
  add(ER_ERROR_DURING_ROLLBACK, N_("Got error %d during ROLLBACK"));
  add(ER_ERROR_DURING_CHECKPOINT, N_("Got error %d during CHECKPOINT"));
  // This is a very incorrect place to use the PRIi64 macro as the
  // program that looks over the source for the N_() macros does not
  // (obviously) do macro expansion, so the string is entirely wrong for
  // what it is trying to output for every language except english.
  add(ER_NEW_ABORTING_CONNECTION, N_("Aborted connection %"PRIi64" to db: '%-.192s' user: '%-.48s' host: '%-.64s' (%-.64s)"));
  add(ER_MASTER_NET_READ, N_("Net error reading from master"));
  add(ER_MASTER_NET_WRITE, N_("Net error writing to master"));
  add(ER_LOCK_OR_ACTIVE_TRANSACTION, N_("Can't execute the given command because you have active locked tables or an active transaction"));
  add(ER_UNKNOWN_SYSTEM_VARIABLE, N_("Unknown system variable '%-.64s'"));
  add(ER_CRASHED_ON_USAGE, N_("Table '%-.192s' is marked as crashed and should be repaired"));
  add(ER_CRASHED_ON_REPAIR, N_("Table '%-.192s' is marked as crashed and last (automatic?) repair failed"));
  add(ER_WARNING_NOT_COMPLETE_ROLLBACK, N_("Some non-transactional changed tables couldn't be rolled back"));
  add(ER_TOO_MANY_USER_CONNECTIONS, N_("User %-.64s already has more than 'max_user_connections' active connections"));
  add(ER_SET_CONSTANTS_ONLY, N_("You may only use constant expressions with SET"));
  add(ER_LOCK_WAIT_TIMEOUT, N_("Lock wait timeout exceeded; try restarting transaction"));
  add(ER_LOCK_TABLE_FULL, N_("The total number of locks exceeds the lock table size"));
  add(ER_READ_ONLY_TRANSACTION, N_("Update locks cannot be acquired during a READ UNCOMMITTED transaction"));
  add(ER_DROP_DB_WITH_READ_LOCK, N_("DROP DATABASE not allowed while thread is holding global read lock"));
  add(ER_WRONG_ARGUMENTS, N_("Incorrect arguments to %s"));
  add(ER_NO_PERMISSION_TO_CREATE_USER, N_("'%-.48s'@'%-.64s' is not allowed to create new users"));
  add(ER_LOCK_DEADLOCK, N_("Deadlock found when trying to get lock; try restarting transaction"));
  add(ER_TABLE_CANT_HANDLE_FT, N_("The used table type doesn't support FULLTEXT indexes"));
  add(ER_CANNOT_ADD_FOREIGN, N_("Cannot add foreign key constraint"));
  add(ER_NO_REFERENCED_ROW, N_("Cannot add or update a child row: a foreign key constraint fails"));
  add(ER_ROW_IS_REFERENCED, N_("Cannot delete or update a parent row: a foreign key constraint fails"));
  add(ER_CONNECT_TO_MASTER, N_("Error connecting to master: %-.128s"));
  add(ER_WRONG_USAGE, N_("Incorrect usage of %s and %s"));
  add(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT, N_("The used SELECT statements have a different number of columns"));
  add(ER_CANT_UPDATE_WITH_READLOCK, N_("Can't execute the query because you have a conflicting read lock"));
  add(ER_USER_LIMIT_REACHED, N_("User '%-.64s' has exceeded the '%s' resource (current value: %ld)"));
  add(ER_SPECIFIC_ACCESS_DENIED_ERROR, N_("Access denied; you need the %-.128s privilege for this operation"));
  add(ER_LOCAL_VARIABLE, N_("Variable '%-.64s' is a SESSION variable and can't be used with SET GLOBAL"));
  add(ER_GLOBAL_VARIABLE, N_("Variable '%-.64s' is a GLOBAL variable and should be set with SET GLOBAL"));
  add(ER_NO_DEFAULT, N_("Variable '%-.64s' doesn't have a default value"));
  add(ER_WRONG_VALUE_FOR_VAR, N_("Variable '%-.64s' can't be set to the value of '%-.200s'"));
  add(ER_WRONG_TYPE_FOR_VAR, N_("Incorrect argument type to variable '%-.64s'"));
  add(ER_VAR_CANT_BE_READ, N_("Variable '%-.64s' can only be set, not read"));
  add(ER_CANT_USE_OPTION_HERE, N_("Incorrect usage/placement of '%s'"));
  add(ER_NOT_SUPPORTED_YET, N_("This version of Drizzle doesn't yet support '%s'"));
  add(ER_INCORRECT_GLOBAL_LOCAL_VAR, N_("Variable '%-.192s' is a %s variable"));
  add(ER_WRONG_FK_DEF, N_("Incorrect foreign key definition for '%-.192s': %s"));
  add(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF, N_("Key reference and table reference don't match"));
  add(ER_OPERAND_COLUMNS, N_("Operand should contain %d column(s)"));
  add(ER_SUBQUERY_NO_1_ROW, N_("Subquery returns more than 1 row"));
  add(ER_AUTO_CONVERT, N_("Converting column '%s' from %s to %s"));
  add(ER_ILLEGAL_REFERENCE, N_("Reference '%-.64s' not supported (%s)"));
  add(ER_DERIVED_MUST_HAVE_ALIAS, N_("Every derived table must have its own alias"));
  add(ER_SELECT_REDUCED, N_("Select %u was reduced during optimization"));
  add(ER_TABLENAME_NOT_ALLOWED_HERE, N_("Table '%-.192s' from one of the SELECTs cannot be used in %-.32s"));
  add(ER_NOT_SUPPORTED_AUTH_MODE, N_("Client does not support authentication protocol requested by server; consider upgrading Drizzle client"));
  add(ER_SPATIAL_CANT_HAVE_NULL, N_("All parts of a SPATIAL index must be NOT NULL"));
  add(ER_COLLATION_CHARSET_MISMATCH, N_("COLLATION '%s' is not valid for CHARACTER SET '%s'"));
  add(ER_TOO_BIG_FOR_UNCOMPRESS, N_("Uncompressed data size too large; the maximum size is %d (based on max_allowed_packet). The length of uncompressed data may also be corrupted."));
  add(ER_ZLIB_Z_MEM_ERROR, N_("ZLIB: Not enough memory"));
  add(ER_ZLIB_Z_BUF_ERROR, N_("ZLIB: Not enough room in the output buffer (probably, length of uncompressed data was corrupted)"));
  add(ER_ZLIB_Z_DATA_ERROR, N_("ZLIB: Input data corrupted"));
  add(ER_CUT_VALUE_GROUP_CONCAT, N_("%d line(s) were cut by GROUP_CONCAT()"));
  add(ER_WARN_TOO_FEW_RECORDS, N_("Row %ld doesn't contain data for all columns"));
  add(ER_WARN_TOO_MANY_RECORDS, N_("Row %ld was truncated; it contained more data than there were input columns"));
  add(ER_WARN_NULL_TO_NOTNULL, N_("Column set to default value; NULL supplied to NOT NULL column '%s' at row %ld"));
  add(ER_WARN_DATA_OUT_OF_RANGE, N_("Out of range value for column '%s' at row %ld"));
  add(ER_WARN_DATA_TRUNCATED, N_("Data truncated for column '%s' at row %ld"));
  add(ER_CANT_AGGREGATE_2COLLATIONS, N_("Illegal mix of collations (%s,%s) and (%s,%s) for operation '%s'"));
  add(ER_CANT_AGGREGATE_3COLLATIONS, N_("Illegal mix of collations (%s,%s), (%s,%s), (%s,%s) for operation '%s'"));
  add(ER_CANT_AGGREGATE_NCOLLATIONS, N_("Illegal mix of collations for operation '%s'"));
  add(ER_VARIABLE_IS_NOT_STRUCT, N_("Variable '%-.64s' is not a variable component (can't be used as XXXX.variable_name)"));
  add(ER_UNKNOWN_COLLATION, N_("Unknown collation: '%-.64s'"));
  add(ER_WARN_FIELD_RESOLVED, N_("Field or reference '%-.192s%s%-.192s%s%-.192s' of SELECT #%d was resolved in SELECT #%d"));
  add(ER_WRONG_NAME_FOR_INDEX, N_("Incorrect index name '%-.100s'"));
  add(ER_WRONG_NAME_FOR_CATALOG, N_("Incorrect catalog name '%-.100s'"));
  add(ER_BAD_FT_COLUMN, N_("Column '%-.192s' cannot be part of FULLTEXT index"));
  add(ER_UNKNOWN_STORAGE_ENGINE, N_("Unknown table engine '%s'"));
  add(ER_NON_UPDATABLE_TABLE, N_("The target table %-.100s of the %s is not updatable"));
  add(ER_FEATURE_DISABLED, N_("The '%s' feature is disabled; you need Drizzle built with '%s' to have it working"));
  add(ER_OPTION_PREVENTS_STATEMENT, N_("The Drizzle server is running with the %s option so it cannot execute this statement"));
  add(ER_DUPLICATED_VALUE_IN_TYPE, N_("Column '%-.100s' has duplicated value '%-.64s' in %s"));
  add(ER_TRUNCATED_WRONG_VALUE, N_("Truncated incorrect %-.32s value: '%-.128s'"));
  add(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS, N_("Incorrect table definition; there can be only one TIMESTAMP column with CURRENT_TIMESTAMP in DEFAULT or ON UPDATE clause"));
  add(ER_INVALID_ON_UPDATE, N_("Invalid ON UPDATE clause for '%-.192s' column"));
  add(ER_GET_ERRMSG, N_("Got error %d '%-.100s' from %s"));
  add(ER_GET_TEMPORARY_ERRMSG, N_("Got temporary error %d '%-.100s' from %s"));
  add(ER_UNKNOWN_TIME_ZONE, N_("Unknown or incorrect time zone: '%-.64s'"));
  add(ER_INVALID_CHARACTER_STRING, N_("Invalid %s character string: '%.64s'"));
  add(ER_WARN_ALLOWED_PACKET_OVERFLOWED, N_("Result of %s() was larger than max_allowed_packet (%ld) - truncated"));
  add(ER_SP_DOES_NOT_EXIST, N_("%s %s does not exist"));
  add(ER_SP_LILABEL_MISMATCH, N_("%s with no matching label: %s"));
  add(ER_SP_LABEL_REDEFINE, N_("Redefining label %s"));
  add(ER_SP_LABEL_MISMATCH, N_("End-label %s without match"));
  add(ER_SP_UNINIT_VAR, N_("Referring to uninitialized variable %s"));
  add(ER_SP_BADSELECT, N_("PROCEDURE %s can't return a result set in the given context"));
  add(ER_SP_BADRETURN, N_("RETURN is only allowed in a FUNCTION"));
  add(ER_SP_BADSTATEMENT, N_("%s is not allowed in stored procedures"));
  add(ER_UPDATE_LOG_DEPRECATED_IGNORED, N_("The update log is deprecated and replaced by the binary log; SET SQL_LOG_UPDATE has been ignored"));
  add(ER_UPDATE_LOG_DEPRECATED_TRANSLATED, N_("The update log is deprecated and replaced by the binary log; SET SQL_LOG_UPDATE has been translated to SET SQL_LOG_BIN"));
  add(ER_QUERY_INTERRUPTED, N_("Query execution was interrupted"));
  add(ER_SP_WRONG_NO_OF_ARGS, N_("Incorrect number of arguments for %s %s; expected %u, got %u"));
  add(ER_SP_COND_MISMATCH, N_("Undefined CONDITION: %s"));
  add(ER_SP_NORETURN, N_("No RETURN found in FUNCTION %s"));
  add(ER_SP_NORETURNEND, N_("FUNCTION %s ended without RETURN"));
  add(ER_SP_BAD_CURSOR_QUERY, N_("Cursor statement must be a SELECT"));
  add(ER_SP_BAD_CURSOR_SELECT, N_("Cursor SELECT must not have INTO"));
  add(ER_SP_CURSOR_MISMATCH, N_("Undefined CURSOR: %s"));
  add(ER_SP_CURSOR_ALREADY_OPEN, N_("Cursor is already open"));
  add(ER_SP_CURSOR_NOT_OPEN, N_("Cursor is not open"));
  add(ER_SP_FETCH_NO_DATA, N_("No data - zero rows fetched, selected, or processed"));
  add(ER_SP_DUP_PARAM, N_("Duplicate parameter: %s"));
  add(ER_SP_DUP_VAR, N_("Duplicate variable: %s"));
  add(ER_SP_DUP_COND, N_("Duplicate condition: %s"));
  add(ER_SP_DUP_CURS, N_("Duplicate cursor: %s"));
  add(ER_SP_SUBSELECT_NYI, N_("Subquery value not supported"));
  add(ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG, N_("%s is not allowed in stored function or trigger"));
  add(ER_SP_VARCOND_AFTER_CURSHNDLR, N_("Variable or condition declaration after cursor or handler declaration"));
  add(ER_SP_CURSOR_AFTER_HANDLER, N_("Cursor declaration after handler declaration"));
  add(ER_FPARSER_BAD_HEADER, N_("Malformed file type header in file '%-.192s'"));
  add(ER_FPARSER_EOF_IN_COMMENT, N_("Unexpected end of file while parsing comment '%-.200s'"));
  add(ER_FPARSER_ERROR_IN_PARAMETER, N_("Error while parsing parameter '%-.192s' (line: '%-.192s')"));
  add(ER_VIEW_SELECT_DERIVED, N_("View's SELECT contains a subquery in the FROM clause"));
  add(ER_VIEW_SELECT_CLAUSE, N_("View's SELECT contains a '%s' clause"));
  add(ER_VIEW_SELECT_VARIABLE, N_("View's SELECT contains a variable or parameter"));
  add(ER_VIEW_SELECT_TMPTABLE, N_("View's SELECT refers to a temporary table '%-.192s'"));
  add(ER_VIEW_WRONG_LIST, N_("View's SELECT and view's field list have different column counts"));
  add(ER_WARN_VIEW_MERGE, N_("View merge algorithm can't be used here for now (assumed undefined algorithm)"));
  add(ER_WARN_VIEW_WITHOUT_KEY, N_("View being updated does not have complete key of underlying table in it"));
  add(ER_VIEW_INVALID, N_("View '%-.192s.%-.192s' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them"));
  add(ER_SP_NO_DROP_SP, N_("Can't drop or alter a %s from within another stored routine"));
  add(ER_SP_GOTO_IN_HNDLR, N_("GOTO is not allowed in a stored procedure handler"));
  add(ER_TRG_ALREADY_EXISTS, N_("Trigger already exists"));
  add(ER_TRG_DOES_NOT_EXIST, N_("Trigger does not exist"));
  add(ER_TRG_ON_VIEW_OR_TEMP_TABLE, N_("Trigger's '%-.192s' is view or temporary table"));
  add(ER_TRG_CANT_CHANGE_ROW, N_("Updating of %s row is not allowed in %strigger"));
  add(ER_TRG_NO_SUCH_ROW_IN_TRG, N_("There is no %s row in %s trigger"));
  add(ER_NO_DEFAULT_FOR_FIELD, N_("Field '%-.192s' doesn't have a default value"));
  add(ER_DIVISION_BY_ZERO, N_("Division by 0"));
  add(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD, N_("Incorrect %-.32s value: '%-.128s' for column '%.192s' at row %u"));
  add(ER_ILLEGAL_VALUE_FOR_TYPE, N_("Illegal %s '%-.192s' value found during parsing"));
  add(ER_VIEW_NONUPD_CHECK, N_("CHECK OPTION on non-updatable view '%-.192s.%-.192s'"));
  add(ER_VIEW_CHECK_FAILED, N_("CHECK OPTION failed '%-.192s.%-.192s'"));
  add(ER_PROCACCESS_DENIED_ERROR, N_("%-.16s command denied to user '%-.48s'@'%-.64s' for routine '%-.192s'"));
  add(ER_RELAY_LOG_FAIL, N_("Failed purging old relay logs: %s"));
  add(ER_PASSWD_LENGTH, N_("Password hash should be a %d-digit hexadecimal number"));
  add(ER_UNKNOWN_TARGET_BINLOG, N_("Target log not found in binlog index"));
  add(ER_IO_ERR_LOG_INDEX_READ, N_("I/O error reading log index file"));
  add(ER_BINLOG_PURGE_PROHIBITED, N_("Server configuration does not permit binlog purge"));
  add(ER_BINLOG_PURGE_FATAL_ERR, N_("Fatal error during log purge"));
  add(ER_LOG_IN_USE, N_("A purgeable log is in use, will not purge"));
  add(ER_LOG_PURGE_UNKNOWN_ERR, N_("Unknown error during log purge"));
  add(ER_RELAY_LOG_INIT, N_("Failed initializing relay log position: %s"));
  add(ER_NO_BINARY_LOGGING, N_("You are not using binary logging"));
  add(ER_RESERVED_SYNTAX, N_("The '%-.64s' syntax is reserved for purposes internal to the Drizzle server"));
  add(ER_WSAS_FAILED, N_("WSAStartup Failed"));
  add(ER_DIFF_GROUPS_PROC, N_("Can't handle procedures with different groups yet"));
  add(ER_NO_GROUP_FOR_PROC, N_("Select must have a group with this procedure"));
  add(ER_ORDER_WITH_PROC, N_("Can't use ORDER clause with this procedure"));
  add(ER_LOGGING_PROHIBIT_CHANGING_OF, N_("Binary logging and replication forbid changing the global server %s"));
  add(ER_NO_FILE_MAPPING, N_("Can't map file: %-.200s, errno: %d"));
  add(ER_WRONG_MAGIC, N_("Wrong magic in %-.64s"));
  add(ER_PS_MANY_PARAM, N_("Prepared statement contains too many placeholders"));
  add(ER_KEY_PART_0, N_("Key part '%-.192s' length cannot be 0"));
  add(ER_VIEW_CHECKSUM, N_("View text checksum failed"));
  add(ER_VIEW_MULTIUPDATE, N_("Can not modify more than one base table through a join view '%-.192s.%-.192s'"));
  add(ER_VIEW_NO_INSERT_FIELD_LIST, N_("Can not insert into join view '%-.192s.%-.192s' without fields list"));
  add(ER_VIEW_DELETE_MERGE_VIEW, N_("Can not delete from join view '%-.192s.%-.192s'"));
  add(ER_CANNOT_USER, N_("Operation %s failed for %.256s"));
  add(ER_XAER_NOTA, N_("XAER_NOTA: Unknown XID"));
  add(ER_XAER_INVAL, N_("XAER_INVAL: Invalid arguments (or unsupported command)"));
  add(ER_XAER_RMFAIL, N_("XAER_RMFAIL: The command cannot be executed when global transaction is in the  %.64s state"));
  add(ER_XAER_OUTSIDE, N_("XAER_OUTSIDE: Some work is done outside global transaction"));
  add(ER_XAER_RMERR, N_("XAER_RMERR: Fatal error occurred in the transaction branch - check your data for consistency"));
  add(ER_XA_RBROLLBACK, N_("XA_RBROLLBACK: Transaction branch was rolled back"));
  add(ER_NONEXISTING_PROC_GRANT, N_("There is no such grant defined for user '%-.48s' on host '%-.64s' on routine '%-.192s'"));
  add(ER_PROC_AUTO_GRANT_FAIL, N_("Failed to grant EXECUTE and ALTER ROUTINE privileges"));
  add(ER_PROC_AUTO_REVOKE_FAIL, N_("Failed to revoke all privileges to dropped routine"));
  add(ER_DATA_TOO_LONG, N_("Data too long for column '%s' at row %ld"));
  add(ER_SP_BAD_SQLSTATE, N_("Bad SQLSTATE: '%s'"));
  add(ER_STARTUP, N_("%s: ready for connections.\nVersion: '%s' %s\n"));
  add(ER_LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR, N_("Can't load value from file with fixed size rows to variable"));
  add(ER_CANT_CREATE_USER_WITH_GRANT, N_("You are not allowed to create a user with GRANT"));
  add(ER_WRONG_VALUE_FOR_TYPE, N_("Incorrect %-.32s value: '%-.128s' for function %-.32s"));
  add(ER_TABLE_DEF_CHANGED, N_("Table definition has changed, please retry transaction"));
  add(ER_SP_DUP_HANDLER, N_("Duplicate handler declared in the same block"));
  add(ER_SP_NOT_VAR_ARG, N_("OUT or INOUT argument %d for routine %s is not a variable or NEW pseudo-variable in BEFORE trigger"));
  add(ER_SP_NO_RETSET, N_("Not allowed to return a result set from a %s"));
  add(ER_CANT_CREATE_GEOMETRY_OBJECT, N_("Cannot get geometry object from data you send to the GEOMETRY field"));
  add(ER_FAILED_ROUTINE_BREAK_BINLOG, N_("A routine failed and has neither NO SQL nor READS SQL DATA in its declaration and binary logging is enabled; if non-transactional tables were updated, the binary log will miss their changes"));
  add(ER_BINLOG_UNSAFE_ROUTINE, N_("This function has none of DETERMINISTIC, NO SQL, or READS SQL DATA in its declaration and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)"));
  add(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER, N_("You do not have the SUPER privilege and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)"));
  add(ER_EXEC_STMT_WITH_OPEN_CURSOR, N_("You can't execute a prepared statement which has an open cursor associated with it. Reset the statement to re-execute it."));
  add(ER_STMT_HAS_NO_OPEN_CURSOR, N_("The statement (%lu) has no open cursor."));
  add(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, N_("Explicit or implicit commit is not allowed in stored function or trigger."));
  add(ER_NO_DEFAULT_FOR_VIEW_FIELD, N_("Field of view '%-.192s.%-.192s' underlying table doesn't have a default value"));
  add(ER_SP_NO_RECURSION, N_("Recursive stored functions and triggers are not allowed."));
  add(ER_TOO_BIG_SCALE, N_("Too big scale %d specified for column '%-.192s'. Maximum is %d."));
  add(ER_TOO_BIG_PRECISION, N_("Too big precision %d specified for column '%-.192s'. Maximum is %d."));
  add(ER_M_BIGGER_THAN_D, N_("For float(M,D), double(M,D) or decimal(M,D), M must be >= D (column '%-.192s')."));
  add(ER_WRONG_LOCK_OF_SYSTEM_TABLE, N_("You can't combine write-locking of system tables with other tables or lock types"));
  add(ER_CONNECT_TO_FOREIGN_DATA_SOURCE, N_("Unable to connect to foreign data source: %.64s"));
  add(ER_QUERY_ON_FOREIGN_DATA_SOURCE, N_("There was a problem processing the query on the foreign data source. Data source error: %-.64s"));
  add(ER_FOREIGN_DATA_SOURCE_DOESNT_EXIST, N_("The foreign data source you are trying to reference does not exist. Data source error:  %-.64s"));
  add(ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE, N_("Can't create federated table. The data source connection string '%-.64s' is not in the correct format"));
  add(ER_FOREIGN_DATA_STRING_INVALID, N_("The data source connection string '%-.64s' is not in the correct format"));
  add(ER_CANT_CREATE_FEDERATED_TABLE, N_("Can't create federated table. Foreign data src error:  %-.64s"));
  add(ER_TRG_IN_WRONG_SCHEMA, N_("Trigger in wrong schema"));
  add(ER_STACK_OVERRUN_NEED_MORE, N_("Thread stack overrun:  %ld bytes used of a %ld byte stack, and %ld bytes needed.  Use 'drizzled -O thread_stack=#' to specify a bigger stack."));
  add(ER_TOO_LONG_BODY, N_("Routine body for '%-.100s' is too long"));
  add(ER_WARN_CANT_DROP_DEFAULT_KEYCACHE, N_("Cannot drop default keycache"));
  add(ER_TOO_BIG_DISPLAYWIDTH, N_("Display width out of range for column '%-.192s' (max = %d)"));
  add(ER_XAER_DUPID, N_("XAER_DUPID: The XID already exists"));
  add(ER_DATETIME_FUNCTION_OVERFLOW, N_("Datetime function: %-.32s field overflow"));
  add(ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG, N_("Can't update table '%-.192s' in stored function/trigger because it is already used by statement which invoked this stored function/trigger."));
  add(ER_VIEW_PREVENT_UPDATE, N_("The definition of table '%-.192s' prevents operation %.192s on table '%-.192s'."));
  add(ER_PS_NO_RECURSION, N_("The prepared statement contains a stored routine call that refers to that same statement. It's not allowed to execute a prepared statement in such a recursive manner"));
  add(ER_SP_CANT_SET_AUTOCOMMIT, N_("Not allowed to set autocommit from a stored function or trigger"));
  add(ER_MALFORMED_DEFINER, N_("Definer is not fully qualified"));
  add(ER_VIEW_FRM_NO_USER, N_("View '%-.192s'.'%-.192s' has no definer information (old table format). Current user is used as definer. Please recreate the view!"));
  add(ER_VIEW_OTHER_USER, N_("You need the SUPER privilege for creation view with '%-.192s'@'%-.192s' definer"));
  add(ER_NO_SUCH_USER, N_("The user specified as a definer ('%-.64s'@'%-.64s') does not exist"));
  add(ER_FORBID_SCHEMA_CHANGE, N_("Changing schema from '%-.192s' to '%-.192s' is not allowed."));
  add(ER_ROW_IS_REFERENCED_2, N_("Cannot delete or update a parent row: a foreign key constraint fails (%.192s)"));
  add(ER_NO_REFERENCED_ROW_2, N_("Cannot add or update a child row: a foreign key constraint fails (%.192s)"));
  add(ER_SP_BAD_VAR_SHADOW, N_("Variable '%-.64s' must be quoted with `...`, or renamed"));
  add(ER_TRG_NO_DEFINER, N_("No definer attribute for trigger '%-.192s'.'%-.192s'. The trigger will be activated under the authorization of the invoker, which may have insufficient privileges. Please recreate the trigger."));
  add(ER_OLD_FILE_FORMAT, N_("'%-.192s' has an old format, you should re-create the '%s' object(s)"));
  add(ER_SP_RECURSION_LIMIT, N_("Recursive limit %d (as set by the max_sp_recursion_depth variable) was exceeded for routine %.192s"));
  add(ER_SP_PROC_TABLE_CORRUPT, N_("Failed to load routine %-.192s. The table drizzle.proc is missing, corrupt, or contains bad data (internal code %d)"));
  add(ER_SP_WRONG_NAME, N_("Incorrect routine name '%-.192s'"));
  add(ER_TABLE_NEEDS_UPGRADE, N_("Table upgrade required. Please do \"REPAIR TABLE `%-.32s`\" to fix it!"));
  add(ER_SP_NO_AGGREGATE, N_("AGGREGATE is not supported for stored functions"));
  add(ER_MAX_PREPARED_STMT_COUNT_REACHED, N_("Can't create more than max_prepared_stmt_count statements (current value: %lu)"));
  add(ER_VIEW_RECURSIVE, N_("`%-.192s`.`%-.192s` contains view recursion"));
  add(ER_NON_GROUPING_FIELD_USED, N_("non-grouping field '%-.192s' is used in %-.64s clause"));
  add(ER_TABLE_CANT_HANDLE_SPKEYS, N_("The used table type doesn't support SPATIAL indexes"));
  add(ER_NO_TRIGGERS_ON_SYSTEM_SCHEMA, N_("Triggers can not be created on system tables"));
  add(ER_REMOVED_SPACES, N_("Leading spaces are removed from name '%s'"));
  add(ER_AUTOINC_READ_FAILED, N_("Failed to read auto-increment value from storage engine"));
  add(ER_USERNAME, N_("user name"));
  add(ER_HOSTNAME, N_("host name"));
  add(ER_WRONG_STRING_LENGTH, N_("String '%-.70s' is too long for %s (should be no longer than %d)"));
  add(ER_NON_INSERTABLE_TABLE, N_("The target table %-.100s of the %s is not insertable-into"));
  add(ER_ADMIN_WRONG_MRG_TABLE, N_("Table '%-.64s' is differently defined or of non-MyISAM type or doesn't exist"));
  add(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT, N_("Too high level of nesting for select"));
  add(ER_NAME_BECOMES_EMPTY, N_("Name '%-.64s' has become ''"));
  add(ER_AMBIGUOUS_FIELD_TERM, N_("First character of the FIELDS TERMINATED string is ambiguous; please use non-optional and non-empty FIELDS ENCLOSED BY"));
  add(ER_FOREIGN_SERVER_EXISTS, N_("The foreign server, %s, you are trying to create already exists."));
  add(ER_FOREIGN_SERVER_DOESNT_EXIST, N_("The foreign server name you are trying to reference does not exist. Data source error:  %-.64s"));
  add(ER_ILLEGAL_HA_CREATE_OPTION, N_("Table storage engine '%-.64s' does not support the create option '%.64s'"));
  add(ER_PARTITION_REQUIRES_VALUES_ERROR, N_("Syntax error: %-.64s PARTITIONING requires definition of VALUES %-.64s for each partition"));
  add(ER_PARTITION_WRONG_VALUES_ERROR, N_("Only %-.64s PARTITIONING can use VALUES %-.64s in partition definition"));
  add(ER_PARTITION_MAXVALUE_ERROR, N_("MAXVALUE can only be used in last partition definition"));
  add(ER_PARTITION_SUBPARTITION_ERROR, N_("Subpartitions can only be hash partitions and by key"));
  add(ER_PARTITION_SUBPART_MIX_ERROR, N_("Must define subpartitions on all partitions if on one partition"));
  add(ER_PARTITION_WRONG_NO_PART_ERROR, N_("Wrong number of partitions defined, mismatch with previous setting"));
  add(ER_PARTITION_WRONG_NO_SUBPART_ERROR, N_("Wrong number of subpartitions defined, mismatch with previous setting"));
  add(ER_CONST_EXPR_IN_PARTITION_FUNC_ERROR, N_("Constant/Random expression in (sub)partitioning function is not allowed"));
  add(ER_NO_CONST_EXPR_IN_RANGE_OR_LIST_ERROR, N_("Expression in RANGE/LIST VALUES must be constant"));
  add(ER_FIELD_NOT_FOUND_PART_ERROR, N_("Field in list of fields for partition function not found in table"));
  add(ER_LIST_OF_FIELDS_ONLY_IN_HASH_ERROR, N_("List of fields is only allowed in KEY partitions"));
  add(ER_INCONSISTENT_PARTITION_INFO_ERROR, N_("The partition info in the frm file is not consistent with what can be written into the frm file"));
  add(ER_PARTITION_FUNC_NOT_ALLOWED_ERROR, N_("The %-.192s function returns the wrong type"));
  add(ER_PARTITIONS_MUST_BE_DEFINED_ERROR, N_("For %-.64s partitions each partition must be defined"));
  add(ER_RANGE_NOT_INCREASING_ERROR, N_("VALUES LESS THAN value must be strictly increasing for each partition"));
  add(ER_INCONSISTENT_TYPE_OF_FUNCTIONS_ERROR, N_("VALUES value must be of same type as partition function"));
  add(ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR, N_("Multiple definition of same constant in list partitioning"));
  add(ER_PARTITION_ENTRY_ERROR, N_("Partitioning can not be used stand-alone in query"));
  add(ER_MIX_HANDLER_ERROR, N_("The mix of handlers in the partitions is not allowed in this version of Drizzle"));
  add(ER_PARTITION_NOT_DEFINED_ERROR, N_("For the partitioned engine it is necessary to define all %-.64s"));
  add(ER_TOO_MANY_PARTITIONS_ERROR, N_("Too many partitions (including subpartitions) were defined"));
  add(ER_SUBPARTITION_ERROR, N_("It is only possible to mix RANGE/LIST partitioning with HASH/KEY partitioning for subpartitioning"));
  add(ER_CANT_CREATE_HANDLER_FILE, N_("Failed to create specific handler file"));
  add(ER_BLOB_FIELD_IN_PART_FUNC_ERROR, N_("A BLOB field is not allowed in partition function"));
  add(ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF, N_("A %-.192s must include all columns in the table's partitioning function"));
  add(ER_NO_PARTS_ERROR, N_("Number of %-.64s = 0 is not an allowed value"));
  add(ER_PARTITION_MGMT_ON_NONPARTITIONED, N_("Partition management on a not partitioned table is not possible"));
  add(ER_FOREIGN_KEY_ON_PARTITIONED, N_("Foreign key condition is not yet supported in conjunction with partitioning"));
  add(ER_DROP_PARTITION_NON_EXISTENT, N_("Error in list of partitions to %-.64s"));
  add(ER_DROP_LAST_PARTITION, N_("Cannot remove all partitions, use DROP TABLE instead"));
  add(ER_COALESCE_ONLY_ON_HASH_PARTITION, N_("COALESCE PARTITION can only be used on HASH/KEY partitions"));
  add(ER_REORG_HASH_ONLY_ON_SAME_NO, N_("REORGANIZE PARTITION can only be used to reorganize partitions not to change their numbers"));
  add(ER_REORG_NO_PARAM_ERROR, N_("REORGANIZE PARTITION without parameters can only be used on auto-partitioned tables using HASH PARTITIONs"));
  add(ER_ONLY_ON_RANGE_LIST_PARTITION, N_("%-.64s PARTITION can only be used on RANGE/LIST partitions"));
  add(ER_ADD_PARTITION_SUBPART_ERROR, N_("Trying to Add partition(s) with wrong number of subpartitions"));
  add(ER_ADD_PARTITION_NO_NEW_PARTITION, N_("At least one partition must be added"));
  add(ER_COALESCE_PARTITION_NO_PARTITION, N_("At least one partition must be coalesced"));
  add(ER_REORG_PARTITION_NOT_EXIST, N_("More partitions to reorganize than there are partitions"));
  add(ER_SAME_NAME_PARTITION, N_("Duplicate partition name %-.192s"));
  add(ER_NO_BINLOG_ERROR, N_("It is not allowed to shut off binlog on this command"));
  add(ER_CONSECUTIVE_REORG_PARTITIONS, N_("When reorganizing a set of partitions they must be in consecutive order"));
  add(ER_REORG_OUTSIDE_RANGE, N_("Reorganize of range partitions cannot change total ranges except for last partition where it can extend the range"));
  add(ER_PARTITION_FUNCTION_FAILURE, N_("Partition function not supported in this version for this handler"));
  add(ER_PART_STATE_ERROR, N_("Partition state cannot be defined from CREATE/ALTER TABLE"));
  add(ER_LIMITED_PART_RANGE, N_("The %-.64s handler only supports 32 bit integers in VALUES"));
  add(ER_PLUGIN_IS_NOT_LOADED, N_("Plugin '%-.192s' is not loaded"));
  add(ER_WRONG_VALUE, N_("Incorrect %-.32s value: '%-.128s'"));
  add(ER_NO_PARTITION_FOR_GIVEN_VALUE, N_("Table has no partition for value %-.64s"));
  add(ER_FILEGROUP_OPTION_ONLY_ONCE, N_("It is not allowed to specify %s more than once"));
  add(ER_CREATE_FILEGROUP_FAILED, N_("Failed to create %s"));
  add(ER_DROP_FILEGROUP_FAILED, N_("Failed to drop %s"));
  add(ER_TABLESPACE_AUTO_EXTEND_ERROR, N_("The handler doesn't support autoextend of tablespaces"));
  add(ER_WRONG_SIZE_NUMBER, N_("A size parameter was incorrectly specified, either number or on the form 10M"));
  add(ER_SIZE_OVERFLOW_ERROR, N_("The size number was correct but we don't allow the digit part to be more than 2 billion"));
  add(ER_ALTER_FILEGROUP_FAILED, N_("Failed to alter: %s"));
  add(ER_BINLOG_ROW_LOGGING_FAILED, N_("Writing one row to the row-based binary log failed"));
  add(ER_BINLOG_ROW_WRONG_TABLE_DEF, N_("Table definition on master and slave does not match: %s"));
  add(ER_BINLOG_ROW_RBR_TO_SBR, N_("Slave running with --log-slave-updates must use row-based binary logging to be able to replicate row-based binary log events"));
  add(ER_EVENT_ALREADY_EXISTS, N_("Event '%-.192s' already exists"));
  add(ER_EVENT_STORE_FAILED, N_("Failed to store event %s. Error code %d from storage engine."));
  add(ER_EVENT_DOES_NOT_EXIST, N_("Unknown event '%-.192s'"));
  add(ER_EVENT_CANT_ALTER, N_("Failed to alter event '%-.192s'"));
  add(ER_EVENT_DROP_FAILED, N_("Failed to drop %s"));
  add(ER_EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG, N_("INTERVAL is either not positive or too big"));
  add(ER_EVENT_ENDS_BEFORE_STARTS, N_("ENDS is either invalid or before STARTS"));
  add(ER_EVENT_EXEC_TIME_IN_THE_PAST, N_("Event execution time is in the past. Event has been disabled"));
  add(ER_EVENT_OPEN_TABLE_FAILED, N_("Failed to open drizzle.event"));
  add(ER_EVENT_NEITHER_M_EXPR_NOR_M_AT, N_("No datetime expression provided"));
  add(ER_COL_COUNT_DOESNT_MATCH_CORRUPTED, N_("Column count of drizzle.%s is wrong. Expected %d, found %d. The table is probably corrupted"));
  add(ER_CANNOT_LOAD_FROM_TABLE, N_("Cannot load from drizzle.%s. The table is probably corrupted"));
  add(ER_EVENT_CANNOT_DELETE, N_("Failed to delete the event from drizzle.event"));
  add(ER_EVENT_COMPILE_ERROR, N_("Error during compilation of event's body"));
  add(ER_EVENT_SAME_NAME, N_("Same old and new event name"));
  add(ER_EVENT_DATA_TOO_LONG, N_("Data for column '%s' too long"));
  add(ER_DROP_INDEX_FK, N_("Cannot drop index '%-.192s': needed in a foreign key constraint"));
  add(ER_WARN_DEPRECATED_SYNTAX_WITH_VER, N_("The syntax '%s' is deprecated and will be removed in Drizzle %s. Please use %s instead"));
  add(ER_CANT_WRITE_LOCK_LOG_TABLE, N_("You can't write-lock a log table. Only read access is possible"));
  add(ER_CANT_LOCK_LOG_TABLE, N_("You can't use locks with log tables."));
  add(ER_FOREIGN_DUPLICATE_KEY, N_("Upholding foreign key constraints for table '%.192s', entry '%-.192s', key %d would lead to a duplicate entry"));
  add(ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE, N_("Column count of drizzle.%s is wrong. Expected %d, found %d. Created with Drizzle %d, now running %d. Please use drizzle_upgrade to fix this error."));
  add(ER_TEMP_TABLE_PREVENTS_SWITCH_OUT_OF_RBR, N_("Cannot switch out of the row-based binary log format when the session has open temporary tables"));
  add(ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_FORMAT, N_("Cannot change the binary logging format inside a stored function or trigger"));
  add(ER_NDB_CANT_SWITCH_BINLOG_FORMAT, N_("The NDB cluster engine does not support changing the binlog format on the fly yet"));
  add(ER_PARTITION_NO_TEMPORARY, N_("Cannot create temporary table with partitions"));
  add(ER_PARTITION_CONST_DOMAIN_ERROR, N_("Partition constant is out of partition function domain"));
  add(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED, N_("This partition function is not allowed"));
  add(ER_DDL_LOG_ERROR, N_("Error in DDL log"));
  add(ER_NULL_IN_VALUES_LESS_THAN, N_("Not allowed to use NULL value in VALUES LESS THAN"));
  add(ER_WRONG_PARTITION_NAME, N_("Incorrect partition name"));
  add(ER_CANT_CHANGE_TX_ISOLATION, N_("Transaction isolation level can't be changed while a transaction is in progress"));
  add(ER_DUP_ENTRY_AUTOINCREMENT_CASE, N_("ALTER TABLE causes auto_increment resequencing, resulting in duplicate entry '%-.192s' for key '%-.192s'"));
  add(ER_EVENT_MODIFY_QUEUE_ERROR, N_("Internal scheduler error %d"));
  add(ER_EVENT_SET_VAR_ERROR, N_("Error during starting/stopping of the scheduler. Error code %u"));
  add(ER_PARTITION_MERGE_ERROR, N_("Engine cannot be used in partitioned tables"));
  add(ER_CANT_ACTIVATE_LOG, N_("Cannot activate '%-.64s' log"));
  add(ER_RBR_NOT_AVAILABLE, N_("The server was not built with row-based replication"));
  add(ER_BASE64_DECODE_ERROR, N_("Decoding of base64 string failed"));
  add(ER_EVENT_RECURSION_FORBIDDEN, N_("Recursion of EVENT DDL statements is forbidden when body is present"));
  add(ER_EVENTS_DB_ERROR, N_("Cannot proceed because system tables used by Event Scheduler were found damaged at server start"));
  add(ER_ONLY_INTEGERS_ALLOWED, N_("Only integers allowed as number here"));
  add(ER_UNSUPORTED_LOG_ENGINE, N_("This storage engine cannot be used for log tables"));
  add(ER_BAD_LOG_STATEMENT, N_("You cannot '%s' a log table if logging is enabled"));
  add(ER_CANT_RENAME_LOG_TABLE, N_("Cannot rename '%s'. When logging enabled, rename to/from log table must rename two tables: the log table to an archive table and another table back to '%s'"));
  add(ER_WRONG_PARAMCOUNT_TO_FUNCTION, N_("Incorrect parameter count in the call to native function '%-.192s'"));
  add(ER_WRONG_PARAMETERS_TO_NATIVE_FCT, N_("Incorrect parameters in the call to native function '%-.192s'"));
  add(ER_WRONG_PARAMETERS_TO_STORED_FCT, N_("Incorrect parameters in the call to stored function '%-.192s'"));
  add(ER_NATIVE_FCT_NAME_COLLISION, N_("This function '%-.192s' has the same name as a native function"));
  add(ER_DUP_ENTRY_WITH_KEY_NAME, N_("Duplicate entry '%-.64s' for key '%-.192s'"));
  add(ER_BINLOG_PURGE_EMFILE, N_("Too many files opened, please execute the command again"));
  add(ER_EVENT_CANNOT_CREATE_IN_THE_PAST, N_("Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation."));
  add(ER_EVENT_CANNOT_ALTER_IN_THE_PAST, N_("Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation."));
  add(ER_SLAVE_INCIDENT, N_("The incident %s occurred on the master. Message: %-.64s"));
  add(ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT, N_("Table has no partition for some existing values"));
  add(ER_BINLOG_UNSAFE_STATEMENT, N_("Statement is not safe to log in statement format."));
  add(ER_SLAVE_FATAL_ERROR, N_("Fatal error: %s"));
  add(ER_SLAVE_RELAY_LOG_READ_FAILURE, N_("Relay log read failure: %s"));
  add(ER_SLAVE_RELAY_LOG_WRITE_FAILURE, N_("Relay log write failure: %s"));
  add(ER_SLAVE_CREATE_EVENT_FAILURE, N_("Failed to create %s"));
  add(ER_SLAVE_MASTER_COM_FAILURE, N_("Master command %s failed: %s"));
  add(ER_BINLOG_LOGGING_IMPOSSIBLE, N_("Binary logging not possible. Message: %s"));
  add(ER_VIEW_NO_CREATION_CTX, N_("View `%-.64s`.`%-.64s` has no creation context"));
  add(ER_VIEW_INVALID_CREATION_CTX, N_("Creation context of view `%-.64s`.`%-.64s' is invalid"));
  add(ER_SR_INVALID_CREATION_CTX, N_("Creation context of stored routine `%-.64s`.`%-.64s` is invalid"));
  add(ER_TRG_CORRUPTED_FILE, N_("Corrupted TRG file for table `%-.64s`.`%-.64s`"));
  add(ER_TRG_NO_CREATION_CTX, N_("Triggers for table `%-.64s`.`%-.64s` have no creation context"));
  add(ER_TRG_INVALID_CREATION_CTX, N_("Trigger creation context of table `%-.64s`.`%-.64s` is invalid"));
  add(ER_EVENT_INVALID_CREATION_CTX, N_("Creation context of event `%-.64s`.`%-.64s` is invalid"));
  add(ER_TRG_CANT_OPEN_TABLE, N_("Cannot open table for trigger `%-.64s`.`%-.64s`"));
  add(ER_CANT_CREATE_SROUTINE, N_("Cannot create stored routine `%-.64s`. Check warnings"));
  add(ER_SLAVE_AMBIGOUS_EXEC_MODE, N_("Ambiguous slave modes combination. %s"));
  add(ER_NO_FORMAT_DESCRIPTION_EVENT_BEFORE_BINLOG_STATEMENT, N_("The BINLOG statement of type `%s` was not preceded by a format description BINLOG statement."));
  add(ER_SLAVE_CORRUPT_EVENT, N_("Corrupted replication event was detected"));
  add(ER_LOAD_DATA_INVALID_COLUMN, N_("Invalid column reference (%-.64s) in LOAD DATA"));
  add(ER_LOG_PURGE_NO_FILE, N_("Being purged log %s was not found"));
  add(ER_WARN_AUTO_CONVERT_LOCK, N_("Converted to non-transactional lock on '%-.64s'"));
  add(ER_NO_AUTO_CONVERT_LOCK_STRICT, N_("Cannot convert to non-transactional lock in strict mode on '%-.64s'"));
  add(ER_NO_AUTO_CONVERT_LOCK_TRANSACTION, N_("Cannot convert to non-transactional lock in an active transaction on '%-.64s'"));
  add(ER_NO_STORAGE_ENGINE, N_("Can't access storage engine of table %-.64s"));
  add(ER_BACKUP_BACKUP_START, N_("Starting backup process"));
  add(ER_BACKUP_BACKUP_DONE, N_("Backup completed"));
  add(ER_BACKUP_RESTORE_START, N_("Starting restore process"));
  add(ER_BACKUP_RESTORE_DONE, N_("Restore completed"));
  add(ER_BACKUP_NOTHING_TO_BACKUP, N_("Nothing to backup"));
  add(ER_BACKUP_CANNOT_INCLUDE_DB, N_("Database '%-.64s' cannot be included in a backup"));
  add(ER_BACKUP_BACKUP, N_("Error during backup operation - server's error log contains more information about the error"));
  add(ER_BACKUP_RESTORE, N_("Error during restore operation - server's error log contains more information about the error"));
  add(ER_BACKUP_RUNNING, N_("Can't execute this command because another BACKUP/RESTORE operation is in progress"));
  add(ER_BACKUP_BACKUP_PREPARE, N_("Error when preparing for backup operation"));
  add(ER_BACKUP_RESTORE_PREPARE, N_("Error when preparing for restore operation"));
  add(ER_BACKUP_INVALID_LOC, N_("Invalid backup location '%-.64s'"));
  add(ER_BACKUP_READ_LOC, N_("Can't read backup location '%-.64s'"));
  add(ER_BACKUP_WRITE_LOC, N_("Can't write to backup location '%-.64s' (file already exists?)"));
  add(ER_BACKUP_LIST_DBS, N_("Can't enumerate server databases"));
  add(ER_BACKUP_LIST_TABLES, N_("Can't enumerate server tables"));
  add(ER_BACKUP_LIST_DB_TABLES, N_("Can't enumerate tables in database %-.64s"));
  add(ER_BACKUP_SKIP_VIEW, N_("Skipping view %-.64s in database %-.64s"));
  add(ER_BACKUP_NO_ENGINE, N_("Skipping table %-.64s since it has no valid storage engine"));
  add(ER_BACKUP_TABLE_OPEN, N_("Can't open table %-.64s"));
  add(ER_BACKUP_READ_HEADER, N_("Can't read backup archive preamble"));
  add(ER_BACKUP_WRITE_HEADER, N_("Can't write backup archive preamble"));
  add(ER_BACKUP_NO_BACKUP_DRIVER, N_("Can't find backup driver for table %-.64s"));
  add(ER_BACKUP_NOT_ACCEPTED, N_("%-.64s backup driver was selected for table %-.64s but it rejects to handle this table"));
  add(ER_BACKUP_CREATE_BACKUP_DRIVER, N_("Can't create %-.64s backup driver"));
  add(ER_BACKUP_CREATE_RESTORE_DRIVER, N_("Can't create %-.64s restore driver"));
  add(ER_BACKUP_TOO_MANY_IMAGES, N_("Found %d images in backup archive but maximum %d are supported"));
  add(ER_BACKUP_WRITE_META, N_("Error when saving meta-data of %-.64s"));
  add(ER_BACKUP_READ_META, N_("Error when reading meta-data list"));
  add(ER_BACKUP_CREATE_META, N_("Can't create %-.64s"));
  add(ER_BACKUP_GET_BUF, N_("Can't allocate buffer for image data transfer"));
  add(ER_BACKUP_WRITE_DATA, N_("Error when writing %-.64s backup image data (for table #%d)"));
  add(ER_BACKUP_READ_DATA, N_("Error when reading data from backup stream"));
  add(ER_BACKUP_NEXT_CHUNK, N_("Can't go to the next chunk in backup stream"));
  add(ER_BACKUP_INIT_BACKUP_DRIVER, N_("Can't initialize %-.64s backup driver"));
  add(ER_BACKUP_INIT_RESTORE_DRIVER, N_("Can't initialize %-.64s restore driver"));
  add(ER_BACKUP_STOP_BACKUP_DRIVER, N_("Can't shut down %-.64s backup driver"));
  add(ER_BACKUP_STOP_RESTORE_DRIVERS, N_("Can't shut down %-.64s backup driver(s)"));
  add(ER_BACKUP_PREPARE_DRIVER, N_("%-.64s backup driver can't prepare for synchronization"));
  add(ER_BACKUP_CREATE_VP, N_("%-.64s backup driver can't create its image validity point"));
  add(ER_BACKUP_UNLOCK_DRIVER, N_("Can't unlock %-.64s backup driver after creating the validity point"));
  add(ER_BACKUP_CANCEL_BACKUP, N_("%-.64s backup driver can't cancel its backup operation"));
  add(ER_BACKUP_CANCEL_RESTORE, N_("%-.64s restore driver can't cancel its restore operation"));
  add(ER_BACKUP_GET_DATA, N_("Error when polling %-.64s backup driver for its image data"));
  add(ER_BACKUP_SEND_DATA, N_("Error when sending image data (for table #%d) to %-.64s restore driver"));
  add(ER_BACKUP_SEND_DATA_RETRY, N_("After %d attempts %-.64s restore driver still can't accept next block of data"));
  add(ER_BACKUP_OPEN_TABLES, N_("Open and lock tables failed in %-.64s"));
  add(ER_BACKUP_THREAD_INIT, N_("Backup driver's table locking thread can not be initialized."));
  add(ER_BACKUP_PROGRESS_TABLES, N_("Can't open the online backup progress tables. Check 'drizzle.online_backup' and 'drizzle.online_backup_progress'."));
  add(ER_TABLESPACE_EXIST, N_("Tablespace '%-.192s' already exists"));
  add(ER_NO_SUCH_TABLESPACE, N_("Tablespace '%-.192s' doesn't exist"));
  add(ER_SLAVE_HEARTBEAT_FAILURE, N_("Unexpected master's heartbeat data: %s"));
  add(ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE, N_("The requested value for the heartbeat period %s %s"));
  add(ER_BACKUP_LOG_WRITE_ERROR, N_("Can't write to the online backup progress log %-.64s."));
  add(ER_TABLESPACE_NOT_EMPTY, N_("Tablespace '%-.192s' not empty"));
  add(ER_BACKUP_TS_CHANGE, N_("Tablespace `%-.64s` needed by tables being restored has changed on the server. The original definition of the required tablespace is '%-.256s' while the same tablespace is defined on the server as '%-.256s'"));
  add(ER_VCOL_BASED_ON_VCOL, N_("A virtual column cannot be based on a virtual column"));
  add(ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED, N_("Non-deterministic expression for virtual column '%s'."));
  add(ER_DATA_CONVERSION_ERROR_FOR_VIRTUAL_COLUMN, N_("Generated value for virtual column '%s' cannot be converted to type '%s'."));
  add(ER_PRIMARY_KEY_BASED_ON_VIRTUAL_COLUMN, N_("Primary key cannot be defined upon a virtual column."));
  add(ER_KEY_BASED_ON_GENERATED_VIRTUAL_COLUMN, N_("Key/Index cannot be defined on a non-stored virtual column."));
  add(ER_WRONG_FK_OPTION_FOR_VIRTUAL_COLUMN, N_("Cannot define foreign key with %s clause on a virtual column."));
  add(ER_WARNING_NON_DEFAULT_VALUE_FOR_VIRTUAL_COLUMN, N_("The value specified for virtual column '%s' in table '%s' ignored."));
  add(ER_UNSUPPORTED_ACTION_ON_VIRTUAL_COLUMN, N_("'%s' is not yet supported for virtual columns."));
  add(ER_CONST_EXPR_IN_VCOL, N_("Constant expression in virtual column function is not allowed."));
  add(ER_UNKNOWN_TEMPORAL_TYPE, N_("Encountered an unknown temporal type."));
  add(ER_INVALID_STRING_FORMAT_FOR_DATE, N_("Received an invalid string format '%s' for a date value."));
  add(ER_INVALID_STRING_FORMAT_FOR_TIME, N_("Received an invalid string format '%s' for a time value."));
  add(ER_INVALID_UNIX_TIMESTAMP_VALUE, N_("Received an invalid value '%s' for a UNIX timestamp."));
  add(ER_INVALID_DATETIME_VALUE, N_("Received an invalid datetime value '%s'."));
  add(ER_INVALID_NULL_ARGUMENT, N_("Received a NULL argument for function '%s'."));
  add(ER_INVALID_NEGATIVE_ARGUMENT, N_("Received an invalid negative argument '%s' for function '%s'."));
  add(ER_ARGUMENT_OUT_OF_RANGE, N_("Received an out-of-range argument '%s' for function '%s'."));
  add(ER_INVALID_ENUM_VALUE, N_("Received an invalid enum value '%s'."));
  add(ER_NO_PRIMARY_KEY_ON_REPLICATED_TABLE, N_("Tables which are replicated require a primary key."));
  add(ER_CORRUPT_TABLE_DEFINITION, N_("Corrupt or invalid table definition: %s"));
  add(ER_CORRUPT_SCHEMA_DEFINITION, N_("Corrupt or invalid schema definition for %s : %s"));
  add(ER_SCHEMA_DOES_NOT_EXIST, N_("Schema does not exist: %s"));
  add(ER_ALTER_SCHEMA, N_("Error altering schema: %s"));
  add(ER_DROP_SCHEMA, +N_("Error droppping Schema : %s"));
  add(ER_USE_SQL_BIG_RESULT, N_("Temporary table too large, rerun with SQL_BIG_RESULT."));
  add(ER_UNKNOWN_ENGINE_OPTION, N_("Unknown table engine option key/pair %s = %s."));
  add(ER_UNKNOWN_SCHEMA_OPTION, N_("Unknown schema engine option key/pair %s = %s."));

  add(EE_CANTUNLOCK, N_("Can't unlock file (Errcode: %d)"));
  add(EE_CANT_CHSIZE, N_("Can't change size of file (Errcode: %d)"));
  add(EE_CANT_OPEN_STREAM, N_("Can't open stream from handle (Errcode: %d)"));
  add(EE_LINK_WARNING, N_("Warning: '%s' had %d links"));
  add(EE_OPEN_WARNING, N_("Warning: %d files and %d streams is left open\n"));
  add(EE_CANT_MKDIR, N_("Can't create directory '%s' (Errcode: %d)"));
  add(EE_UNKNOWN_CHARSET, N_("Character set '%s' is not a compiled character set and is not specified in the %s file"));
  add(EE_OUT_OF_FILERESOURCES, N_("Out of resources when opening file '%s' (Errcode: %d)"));
  add(EE_CANT_READLINK, N_("Can't read value for symlink '%s' (Error %d)"));
  add(EE_CANT_SYMLINK, N_("Can't create symlink '%s' pointing at '%s' (Error %d)"));
  add(EE_REALPATH, N_("Error on realpath() on '%s' (Error %d)"));
  add(EE_SYNC, N_("Can't sync file '%s' to disk (Errcode: %d)"));
  add(EE_UNKNOWN_COLLATION, N_("Collation '%s' is not a compiled collation and is not specified in the %s file"));
  add(EE_FILE_NOT_CLOSED, N_("File '%s' (fileno: %d) was not closed"));

  // Some old error values use the same strings as some new error values.
  add(EE_FILENOTFOUND, find(ER_FILE_NOT_FOUND));
  add(EE_CANTCREATEFILE, find(ER_CANT_CREATE_FILE));
  add(EE_READ, find(ER_ERROR_ON_READ));
  add(EE_WRITE, find(ER_ERROR_ON_WRITE));
  add(EE_BADCLOSE, find(ER_ERROR_ON_CLOSE));
  add(EE_OUTOFMEMORY, find(ER_OUTOFMEMORY));
  add(EE_DELETE, find(ER_CANT_DELETE_FILE));
  add(EE_LINK, find(ER_ERROR_ON_RENAME));
  add(EE_EOFERR, find(ER_UNEXPECTED_EOF));
  add(EE_CANTLOCK, find(ER_CANT_LOCK));
  add(EE_DIR, find(ER_CANT_READ_DIR));
  add(EE_STAT, find(ER_CANT_GET_STAT));
  add(EE_DISK_FULL, find(ER_DISK_FULL));

}

}

} /* namespace drizzled */
