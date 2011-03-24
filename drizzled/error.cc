/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2000 MySQL AB
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/definitions.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/identifier.h>
#include <drizzled/util/find_ptr.h>

#include <boost/unordered_map.hpp>

namespace drizzled {
namespace {

ErrorMap& get_error_map()
{
  static ErrorMap errors;
  return errors;
}

} // anonymous namespace

const ErrorMap::ErrorMessageMap& ErrorMap::get_error_message_map()
{
  return get_error_map().mapping_;
}

void add_error_message(error_t error_code,
                       const std::string &error_name,
                       const std::string &message)
{
  get_error_map().add(error_code, error_name, message);
}

const char* error_message(error_t code)
{
	const std::string* ptr = get_error_map().find(code);
	return ptr ? ptr->c_str() : get_error_map().find(ER_UNKNOWN_ERROR)->c_str();
}

error_handler_func error_handler_hook= NULL;

namespace error {

void access(const drizzled::identifier::User& user)
{
  my_error(ER_ACCESS_DENIED_ERROR, MYF(0), user.getSQLPath().c_str(), ER(user.hasPassword() ? ER_YES : ER_NO));
} 

void access(const drizzled::identifier::User& user, const drizzled::identifier::Schema& schema)
{
  my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), user.getSQLPath().c_str(), schema.getSQLPath().c_str());
} 

void access(const drizzled::identifier::User& user, const drizzled::identifier::Table& table)
{
  my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0), user.getSQLPath().c_str(), table.getSQLPath().c_str());
} 

static error::level_t _verbosity= error::ERROR;
static std::string _verbosity_strint;

const std::string &verbose_string()
{
  switch (_verbosity)
  {
  case error::INSPECT:
    {
      static std::string _arg= "INSPECT";
      return _arg;
    }
  case error::INFO:
    {
      static std::string _arg= "INFO";
      return _arg;
    }
  case error::WARN:
    {
      static std::string _arg= "WARN";
      return _arg;
    }
  case error::ERROR:
    {
      static std::string _arg= "ERROR";
      return _arg;
    }
  }

  abort();
}

error::level_t &verbosity()
{
  return _verbosity;
}

void check_verbosity(const std::string &arg)
{
  if (not arg.compare("INSPECT"))
  {
    _verbosity= error::INSPECT;
  }
  else if (not arg.compare("INFO"))
  {
    _verbosity= error::INFO;
  }
  else if (not arg.compare("WARN"))
  {
    _verbosity= error::WARN;
  }
  else if (not arg.compare("ERROR"))
  {
    _verbosity= error::ERROR;
  }
}

} // namespace error

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

void my_error(const std::string &ref, error_t nr, myf MyFlags)
{
  my_error(nr, MyFlags, ref.c_str());
} 

void my_error(error_t nr, const drizzled::Identifier& id, myf MyFlags)
{
  my_error(nr, MyFlags, id.getSQLPath().c_str());
} 

void my_error(error_t nr)
{
  my_error(nr, MYF(0));
}

void my_error(error_t nr, myf MyFlags, ...)
{
  va_list args;
  char ebuff[ERRMSGSIZE + 20];

  if (const std::string* format= get_error_map().find(nr))
	{	
    va_start(args,MyFlags);
    (void) vsnprintf (ebuff, sizeof(ebuff), _(format->c_str()), args);
    va_end(args);
  }
	else
    (void) snprintf (ebuff, sizeof(ebuff), _("Unknown error %d"), nr);
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

void my_printf_error(drizzled::error_t error, const char *format, myf MyFlags, ...)
{
  va_list args;
  char ebuff[ERRMSGSIZE+20];

  va_start(args,MyFlags);
  (void) vsnprintf (ebuff, sizeof(ebuff), format, args);
  va_end(args);
  (*error_handler_hook)(error, ebuff, MyFlags);
}

/*
  Give message using error_handler_hook

  SYNOPSIS
    my_message()
      error	Errno
      str	Error message
      MyFlags	Flags
*/

void my_message(drizzled::error_t error, const char *str, myf MyFlags)
{
  (*error_handler_hook)(error, str, MyFlags);
}


// Insert the message for the error.  If the error already has an existing
// mapping, an error is logged, but the function continues.
void ErrorMap::add(drizzled::error_t error_num,
                   const std::string &error_name,
                   const std::string &message)
{
  if (not mapping_.count(error_num))
  {
    // Log the error.
    mapping_[error_num]= ErrorMap::value_type(error_name, message);
  }
  else
  {
    mapping_.insert(ErrorMessageMap::value_type(error_num, ErrorMap::value_type(error_name, message)));
  }
}

const std::string* ErrorMap::find(drizzled::error_t error_num) const
{
  const ErrorMessageMap::mapped_type* pos= find_ptr(mapping_, error_num);
	return pos ? &pos->second : NULL;
}

#define ADD_ERROR_MESSAGE(code, msg) add(code, STRINGIFY_ARG(code), msg)
// Constructor sets the default mappings.
ErrorMap::ErrorMap()
{
  ADD_ERROR_MESSAGE(EE_OK, N_("SUCCESS"));
  ADD_ERROR_MESSAGE(EE_ERROR_FIRST, N_("Error on first"));
  ADD_ERROR_MESSAGE(ER_NO, N_("NO"));
  ADD_ERROR_MESSAGE(ER_YES, N_("YES"));
  ADD_ERROR_MESSAGE(ER_CANT_CREATE_FILE, N_("Can't create file '%-.200s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_CANT_CREATE_TABLE, N_("Can't create table '%-.200s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_CANT_CREATE_DB, N_("Can't create schema '%-.192s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_DB_CREATE_EXISTS, N_("Can't create schema '%-.192s'; schema exists"));
  ADD_ERROR_MESSAGE(ER_DB_DROP_EXISTS, N_("Can't drop schema '%-.192s'; schema doesn't exist"));
  ADD_ERROR_MESSAGE(ER_CANT_DELETE_FILE, N_("Error on delete of '%-.192s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_CANT_GET_STAT, N_("Can't get status of '%-.200s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_CANT_LOCK, N_("Can't lock file (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_CANT_OPEN_FILE, N_("Can't open file: '%-.200s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_FILE_NOT_FOUND, N_("Can't find file: '%-.200s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_CANT_READ_DIR, N_("Can't read dir of '%-.192s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_CHECKREAD, N_("Record has changed since last read in table '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_DISK_FULL, N_("Disk full (%s); waiting for someone to free some space..."));
  ADD_ERROR_MESSAGE(ER_DUP_KEY, N_("Can't write; duplicate key in table '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_ERROR_ON_CLOSE, N_("Error on close of '%-.192s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_ERROR_ON_READ, N_("Error reading file '%-.200s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_ERROR_ON_RENAME, N_("Error on rename of '%-.150s' to '%-.150s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_ERROR_ON_WRITE, N_("Error writing file '%-.200s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_FILE_USED, N_("'%-.192s' is locked against change"));
  ADD_ERROR_MESSAGE(ER_FILSORT_ABORT, N_("Sort aborted"));
  ADD_ERROR_MESSAGE(ER_GET_ERRNO, N_("Got error %d from storage engine"));
  ADD_ERROR_MESSAGE(ER_ILLEGAL_HA, N_("Table storage engine for '%-.192s' doesn't have this option"));
  ADD_ERROR_MESSAGE(ER_KEY_NOT_FOUND, N_("Can't find record in '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_NOT_FORM_FILE, N_("Incorrect information in file: '%-.200s'"));
  ADD_ERROR_MESSAGE(ER_NOT_KEYFILE, N_("Incorrect key file for table '%-.200s'; try to repair it"));
  ADD_ERROR_MESSAGE(ER_OLD_KEYFILE, N_("Old key file for table '%-.192s'; repair it!"));
  ADD_ERROR_MESSAGE(ER_OPEN_AS_READONLY, N_("Table '%-.192s' is read only"));
  ADD_ERROR_MESSAGE(ER_OUTOFMEMORY, N_("Out of memory; restart server and try again (needed %lu bytes)"));
  ADD_ERROR_MESSAGE(ER_OUT_OF_SORTMEMORY, N_("Out of sort memory; increase server sort buffer size"));
  ADD_ERROR_MESSAGE(ER_OUT_OF_GLOBAL_SORTMEMORY, N_("Global sort memory constraint hit; increase sort-heap-threshold"));
  ADD_ERROR_MESSAGE(ER_OUT_OF_GLOBAL_JOINMEMORY, N_("Global join memory constraint hit; increase join-heap-threshold"));
  ADD_ERROR_MESSAGE(ER_OUT_OF_GLOBAL_READRNDMEMORY, N_("Global read_rnd memory constraint hit; increase read-rnd-heap-threshold"));
  ADD_ERROR_MESSAGE(ER_OUT_OF_GLOBAL_READMEMORY, N_("Global read memory constraint hit; increase read-buffer-threshold"));
  ADD_ERROR_MESSAGE(ER_UNEXPECTED_EOF, N_("Unexpected EOF found when reading file '%-.192s' (errno: %d)"));
  ADD_ERROR_MESSAGE(ER_CON_COUNT_ERROR, N_("Too many connections"));
  ADD_ERROR_MESSAGE(ER_OUT_OF_RESOURCES, N_("Out of memory; check if drizzled or some other process uses all available memory; if not, you may have to use 'ulimit' to allow drizzled to use more memory or you can add more swap space"));
  ADD_ERROR_MESSAGE(ER_BAD_HOST_ERROR, N_("Can't get hostname for your address"));
  ADD_ERROR_MESSAGE(ER_HANDSHAKE_ERROR, N_("Bad handshake"));

  // Access error messages
  ADD_ERROR_MESSAGE(ER_DBACCESS_DENIED_ERROR, N_("Access denied for user '%s' to schema '%s'"));
  ADD_ERROR_MESSAGE(ER_TABLEACCESS_DENIED_ERROR, N_("Access denied for user '%s' to table '%s'"));
  ADD_ERROR_MESSAGE(ER_ACCESS_DENIED_ERROR, N_("Access denied for user '%s' (using password: %s)"));

  ADD_ERROR_MESSAGE(ER_NO_DB_ERROR, N_("No schema selected"));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_COM_ERROR, N_("Unknown command"));
  ADD_ERROR_MESSAGE(ER_BAD_NULL_ERROR, N_("Column '%-.192s' cannot be null"));
  ADD_ERROR_MESSAGE(ER_BAD_DB_ERROR, N_("Unknown schema '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_TABLE_EXISTS_ERROR, N_("Table '%-.192s' already exists"));
  ADD_ERROR_MESSAGE(ER_BAD_TABLE_ERROR, N_("Unknown table '%-.100s'"));
  ADD_ERROR_MESSAGE(ER_NON_UNIQ_ERROR, N_("Column '%-.192s' in %-.192s is ambiguous"));
  ADD_ERROR_MESSAGE(ER_SERVER_SHUTDOWN, N_("Server shutdown in progress"));
  ADD_ERROR_MESSAGE(ER_BAD_FIELD_ERROR, N_("Unknown column '%-.192s' in '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_WRONG_FIELD_WITH_GROUP, N_("'%-.192s' isn't in GROUP BY"));
  ADD_ERROR_MESSAGE(ER_WRONG_GROUP_FIELD, N_("Can't group on '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_WRONG_SUM_SELECT, N_("Statement has sum functions and columns in same statement"));
  ADD_ERROR_MESSAGE(ER_WRONG_VALUE_COUNT, N_("Column count doesn't match value count"));
  ADD_ERROR_MESSAGE(ER_TOO_LONG_IDENT, N_("Identifier name '%-.100s' is too long"));
  ADD_ERROR_MESSAGE(ER_DUP_FIELDNAME, N_("Duplicate column name '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_DUP_KEYNAME, N_("Duplicate key name '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_DUP_ENTRY, N_("Duplicate entry '%-.192s' for key %d"));
  ADD_ERROR_MESSAGE(ER_WRONG_FIELD_SPEC, N_("Incorrect column specifier for column '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_PARSE_ERROR, N_("%s near '%-.80s' at line %d"));
  ADD_ERROR_MESSAGE(ER_PARSE_ERROR_UNKNOWN, N_("Parsing error near '%s'"));
  ADD_ERROR_MESSAGE(ER_EMPTY_QUERY, N_("Query was empty"));
  ADD_ERROR_MESSAGE(ER_NONUNIQ_TABLE, N_("Not unique table/alias: '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_INVALID_DEFAULT, N_("Invalid default value for '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_MULTIPLE_PRI_KEY, N_("Multiple primary key defined"));
  ADD_ERROR_MESSAGE(ER_TOO_MANY_KEYS, N_("Too many keys specified; max %d keys allowed"));
  ADD_ERROR_MESSAGE(ER_TOO_MANY_KEY_PARTS, N_("Too many key parts specified; max %d parts allowed"));
  ADD_ERROR_MESSAGE(ER_TOO_LONG_KEY, N_("Specified key was too long; max key length is %d bytes"));
  ADD_ERROR_MESSAGE(ER_KEY_COLUMN_DOES_NOT_EXITS, N_("Key column '%-.192s' doesn't exist in table"));
  ADD_ERROR_MESSAGE(ER_BLOB_USED_AS_KEY, N_("BLOB column '%-.192s' can't be used in key specification with the used table type"));
  ADD_ERROR_MESSAGE(ER_TOO_BIG_FIELDLENGTH, N_("Column length too big for column '%-.192s' (max = %d); use BLOB or TEXT instead"));
  ADD_ERROR_MESSAGE(ER_WRONG_AUTO_KEY, N_("Incorrect table definition; there can be only one auto column and it must be defined as a key"));
  ADD_ERROR_MESSAGE(ER_NORMAL_SHUTDOWN, N_("%s: Normal shutdown\n"));
  ADD_ERROR_MESSAGE(ER_GOT_SIGNAL, N_("%s: Got signal %d. Aborting!\n"));
  ADD_ERROR_MESSAGE(ER_SHUTDOWN_COMPLETE, N_("%s: Shutdown complete\n"));
  ADD_ERROR_MESSAGE(ER_FORCING_CLOSE, N_("%s: Forcing close of thread %" PRIu64 " user: '%-.48s'\n"));
  ADD_ERROR_MESSAGE(ER_IPSOCK_ERROR, N_("Can't create IP socket"));
  ADD_ERROR_MESSAGE(ER_NO_SUCH_INDEX, N_("Table '%-.192s' has no index like the one used in CREATE INDEX; recreate the table"));
  ADD_ERROR_MESSAGE(ER_WRONG_FIELD_TERMINATORS, N_("Field separator argument '%-.32s' with length '%d' is not what is expected; check the manual"));
  ADD_ERROR_MESSAGE(ER_BLOBS_AND_NO_TERMINATED, N_("You can't use fixed rowlength with BLOBs; please use 'fields terminated by'"));
  ADD_ERROR_MESSAGE(ER_TEXTFILE_NOT_READABLE, N_("The file '%-.128s' must be in the schema directory or be readable by all"));
  ADD_ERROR_MESSAGE(ER_FILE_EXISTS_ERROR, N_("File '%-.200s' already exists"));
  ADD_ERROR_MESSAGE(ER_LOAD_INFO, N_("Records: %ld  Deleted: %ld  Skipped: %ld  Warnings: %ld"));
  ADD_ERROR_MESSAGE(ER_WRONG_SUB_KEY, N_("Incorrect prefix key; the used key part isn't a string, the used length is longer than the key part, or the storage engine doesn't support unique prefix keys"));
  ADD_ERROR_MESSAGE(ER_CANT_REMOVE_ALL_FIELDS, N_("You can't delete all columns with ALTER TABLE; use DROP TABLE instead"));
  ADD_ERROR_MESSAGE(ER_CANT_DROP_FIELD_OR_KEY, N_("Can't DROP '%-.192s'; check that column/key exists"));
  ADD_ERROR_MESSAGE(ER_INSERT_INFO, N_("Records: %ld  Duplicates: %ld  Warnings: %ld"));
  ADD_ERROR_MESSAGE(ER_UPDATE_TABLE_USED, N_("You can't specify target table '%-.192s' for update in FROM clause"));

  // KILL session errors
  ADD_ERROR_MESSAGE(ER_NO_SUCH_THREAD, N_("Unknown session id: %lu"));
  ADD_ERROR_MESSAGE(ER_KILL_DENIED_ERROR, N_("You are not the owner of session %lu"));
  ADD_ERROR_MESSAGE(ER_KILL_DENY_SELF_ERROR, N_("You cannot kill the session you are connected from."));


  ADD_ERROR_MESSAGE(ER_NO_TABLES_USED, N_("No tables used"));
  ADD_ERROR_MESSAGE(ER_BLOB_CANT_HAVE_DEFAULT, N_("BLOB/TEXT column '%-.192s' can't have a default value"));
  ADD_ERROR_MESSAGE(ER_WRONG_DB_NAME, N_("Incorrect schema name '%-.100s'"));
  ADD_ERROR_MESSAGE(ER_WRONG_TABLE_NAME, N_("Incorrect table name '%-.100s'"));
  ADD_ERROR_MESSAGE(ER_TOO_BIG_SELECT, N_("The SELECT would examine more than MAX_JOIN_SIZE rows; check your WHERE and use SET SQL_BIG_SELECTS=1 or SET MAX_JOIN_SIZE=# if the SELECT is okay"));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_ERROR, N_("Unknown error"));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_PROCEDURE, N_("Unknown procedure '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_WRONG_PARAMCOUNT_TO_PROCEDURE, N_("Incorrect parameter count to procedure '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_TABLE, N_("Unknown table '%-.192s' in %-.32s"));
  ADD_ERROR_MESSAGE(ER_FIELD_SPECIFIED_TWICE, N_("Column '%-.192s' specified twice"));
  ADD_ERROR_MESSAGE(ER_INVALID_GROUP_FUNC_USE, N_("Invalid use of group function"));
  ADD_ERROR_MESSAGE(ER_UNSUPPORTED_EXTENSION, N_("Table '%-.192s' uses an extension that doesn't exist in this Drizzle version"));
  ADD_ERROR_MESSAGE(ER_TABLE_MUST_HAVE_COLUMNS, N_("A table must have at least 1 column"));
  ADD_ERROR_MESSAGE(ER_RECORD_FILE_FULL, N_("The table '%-.192s' is full"));
  ADD_ERROR_MESSAGE(ER_TOO_MANY_TABLES, N_("Too many tables; Drizzle can only use %d tables in a join"));
  ADD_ERROR_MESSAGE(ER_TOO_MANY_FIELDS, N_("Too many columns"));
  ADD_ERROR_MESSAGE(ER_TOO_BIG_ROWSIZE, N_("Row size too large. The maximum row size for the used table type, not counting BLOBs, is %ld. You have to change some columns to TEXT or BLOBs"));
  ADD_ERROR_MESSAGE(ER_WRONG_OUTER_JOIN, N_("Cross dependency found in OUTER JOIN; examine your ON conditions"));
  ADD_ERROR_MESSAGE(ER_NULL_COLUMN_IN_INDEX, N_("Table handler doesn't support NULL in given index. Please change column '%-.192s' to be NOT NULL or use another handler"));
  ADD_ERROR_MESSAGE(ER_PLUGIN_NO_PATHS, N_("No paths allowed for plugin library"));
  ADD_ERROR_MESSAGE(ER_PLUGIN_EXISTS, N_("Plugin '%-.192s' already exists"));
  ADD_ERROR_MESSAGE(ER_CANT_OPEN_LIBRARY, N_("Can't open shared library '%-.192s' (errno: %d %s)"));
  ADD_ERROR_MESSAGE(ER_CANT_FIND_DL_ENTRY, N_("Can't find symbol '%-.128s' in library '%s'"));
  ADD_ERROR_MESSAGE(ER_UPDATE_INFO, N_("Rows matched: %ld  Changed: %ld  Warnings: %ld"));
  ADD_ERROR_MESSAGE(ER_CANT_CREATE_THREAD, N_("Can't create a new thread (errno %d); if you are not out of available memory, you can consult the manual for a possible OS-dependent bug"));
  ADD_ERROR_MESSAGE(ER_WRONG_VALUE_COUNT_ON_ROW, N_("Column count doesn't match value count at row %ld"));
  ADD_ERROR_MESSAGE(ER_CANT_REOPEN_TABLE, N_("Can't reopen table: '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_MIX_OF_GROUP_FUNC_AND_FIELDS, N_("Mixing of GROUP columns (MIN(),MAX(),COUNT(),...) with no GROUP columns is illegal if there is no GROUP BY clause"));
  ADD_ERROR_MESSAGE(ER_SYNTAX_ERROR, N_("You have an error in your SQL syntax; check the manual that corresponds to your Drizzle server version for the right syntax to use"));
  ADD_ERROR_MESSAGE(ER_NET_PACKET_TOO_LARGE, N_("Got a packet bigger than 'max_allowed_packet' bytes"));
  ADD_ERROR_MESSAGE(ER_NET_PACKETS_OUT_OF_ORDER, N_("Got packets out of order"));
  ADD_ERROR_MESSAGE(ER_TABLE_CANT_HANDLE_BLOB, N_("The used table type doesn't support BLOB/TEXT columns"));
  ADD_ERROR_MESSAGE(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT, N_("The used table type doesn't support AUTO_INCREMENT columns"));
  ADD_ERROR_MESSAGE(ER_WRONG_COLUMN_NAME, N_("Incorrect column name '%-.100s'"));
  ADD_ERROR_MESSAGE(ER_WRONG_KEY_COLUMN, N_("The used storage engine can't index column '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_WRONG_MRG_TABLE, N_("Unable to open underlying table which is differently defined or of non-MyISAM type or doesn't exist"));
  ADD_ERROR_MESSAGE(ER_DUP_UNIQUE, N_("Can't write, because of unique constraint, to table '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_BLOB_KEY_WITHOUT_LENGTH, N_("BLOB/TEXT column '%-.192s' used in key specification without a key length"));
  ADD_ERROR_MESSAGE(ER_PRIMARY_CANT_HAVE_NULL, N_("All parts of a PRIMARY KEY must be NOT NULL; if you need NULL in a key, use UNIQUE instead"));
  ADD_ERROR_MESSAGE(ER_TOO_MANY_ROWS, N_("Result consisted of more than one row"));
  ADD_ERROR_MESSAGE(ER_REQUIRES_PRIMARY_KEY, N_("This table type requires a primary key"));
  ADD_ERROR_MESSAGE(ER_KEY_DOES_NOT_EXITS, N_("Key '%-.192s' doesn't exist in table '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_CHECK_NO_SUCH_TABLE, N_("Can't open table"));
  ADD_ERROR_MESSAGE(ER_CHECK_NOT_IMPLEMENTED, N_("The storage engine for the table doesn't support %s"));
  ADD_ERROR_MESSAGE(ER_ERROR_DURING_COMMIT, N_("Got error %d during COMMIT"));
  ADD_ERROR_MESSAGE(ER_ERROR_DURING_ROLLBACK, N_("Got error %d during ROLLBACK"));
  // This is a very incorrect place to use the PRIi64 macro as the
  // program that looks over the source for the N_() macros does not
  // (obviously) do macro expansion, so the string is entirely wrong for
  // what it is trying to output for every language except english.
  ADD_ERROR_MESSAGE(ER_NEW_ABORTING_CONNECTION, N_("Aborted connection %"PRIi64" to db: '%-.192s' user: '%-.48s' host: '%-.64s' (%-.64s)"));
  ADD_ERROR_MESSAGE(ER_LOCK_OR_ACTIVE_TRANSACTION, N_("Can't execute the given command because you have active locked tables or an active transaction"));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_SYSTEM_VARIABLE, N_("Unknown system variable '%-.64s'"));
  ADD_ERROR_MESSAGE(ER_CRASHED_ON_USAGE, N_("Table '%-.192s' is marked as crashed and should be repaired"));
  ADD_ERROR_MESSAGE(ER_CRASHED_ON_REPAIR, N_("Table '%-.192s' is marked as crashed and last (automatic?) repair failed"));
  ADD_ERROR_MESSAGE(ER_WARNING_NOT_COMPLETE_ROLLBACK, N_("Some non-transactional changed tables couldn't be rolled back"));
  ADD_ERROR_MESSAGE(ER_SET_CONSTANTS_ONLY, N_("You may only use constant expressions with SET"));
  ADD_ERROR_MESSAGE(ER_LOCK_WAIT_TIMEOUT, N_("Lock wait timeout exceeded; try restarting transaction"));
  ADD_ERROR_MESSAGE(ER_LOCK_TABLE_FULL, N_("The total number of locks exceeds the lock table size"));
  ADD_ERROR_MESSAGE(ER_READ_ONLY_TRANSACTION, N_("Update locks cannot be acquired during a READ UNCOMMITTED transaction"));
  ADD_ERROR_MESSAGE(ER_DROP_DB_WITH_READ_LOCK, N_("DROP DATABASE not allowed while thread is holding global read lock"));
  ADD_ERROR_MESSAGE(ER_WRONG_ARGUMENTS, N_("Incorrect arguments to %s"));
  ADD_ERROR_MESSAGE(ER_LOCK_DEADLOCK, N_("Deadlock found when trying to get lock; try restarting transaction"));
  ADD_ERROR_MESSAGE(ER_TABLE_CANT_HANDLE_FT, N_("The used table type doesn't support FULLTEXT indexes"));
  ADD_ERROR_MESSAGE(ER_CANNOT_ADD_FOREIGN, N_("Cannot add foreign key constraint"));
  ADD_ERROR_MESSAGE(ER_NO_REFERENCED_ROW, N_("Cannot add or update a child row: a foreign key constraint fails"));
  ADD_ERROR_MESSAGE(ER_ROW_IS_REFERENCED, N_("Cannot delete or update a parent row: a foreign key constraint fails"));
  ADD_ERROR_MESSAGE(ER_WRONG_USAGE, N_("Incorrect usage of %s and %s"));
  ADD_ERROR_MESSAGE(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT, N_("The used SELECT statements have a different number of columns"));
  ADD_ERROR_MESSAGE(ER_CANT_UPDATE_WITH_READLOCK, N_("Can't execute the query because you have a conflicting read lock"));
  ADD_ERROR_MESSAGE(ER_LOCAL_VARIABLE, N_("Variable '%-.64s' is a SESSION variable and can't be used with SET GLOBAL"));
  ADD_ERROR_MESSAGE(ER_GLOBAL_VARIABLE, N_("Variable '%-.64s' is a GLOBAL variable and should be set with SET GLOBAL"));
  ADD_ERROR_MESSAGE(ER_NO_DEFAULT, N_("Variable '%-.64s' doesn't have a default value"));
  ADD_ERROR_MESSAGE(ER_WRONG_VALUE_FOR_VAR, N_("Variable '%-.64s' can't be set to the value of '%-.200s'"));
  ADD_ERROR_MESSAGE(ER_WRONG_TYPE_FOR_VAR, N_("Incorrect argument type to variable '%-.64s'"));
  ADD_ERROR_MESSAGE(ER_VAR_CANT_BE_READ, N_("Variable '%-.64s' can only be set, not read"));
  ADD_ERROR_MESSAGE(ER_CANT_USE_OPTION_HERE, N_("Incorrect usage/placement of '%s'"));
  ADD_ERROR_MESSAGE(ER_NOT_SUPPORTED_YET, N_("This version of Drizzle doesn't yet support '%s'"));
  ADD_ERROR_MESSAGE(ER_INCORRECT_GLOBAL_LOCAL_VAR, N_("Variable '%-.192s' is a %s variable"));
  ADD_ERROR_MESSAGE(ER_WRONG_FK_DEF, N_("Incorrect foreign key definition for '%-.192s': %s"));
  ADD_ERROR_MESSAGE(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF, N_("Key reference and table reference don't match"));
  ADD_ERROR_MESSAGE(ER_OPERAND_COLUMNS, N_("Operand should contain %d column(s)"));
  ADD_ERROR_MESSAGE(ER_SUBQUERY_NO_1_ROW, N_("Subquery returns more than 1 row"));
  ADD_ERROR_MESSAGE(ER_AUTO_CONVERT, N_("Converting column '%s' from %s to %s"));
  ADD_ERROR_MESSAGE(ER_ILLEGAL_REFERENCE, N_("Reference '%-.64s' not supported (%s)"));
  ADD_ERROR_MESSAGE(ER_DERIVED_MUST_HAVE_ALIAS, N_("Every derived table must have its own alias"));
  ADD_ERROR_MESSAGE(ER_SELECT_REDUCED, N_("Select %u was reduced during optimization"));
  ADD_ERROR_MESSAGE(ER_TABLENAME_NOT_ALLOWED_HERE, N_("Table '%-.192s' from one of the SELECTs cannot be used in %-.32s"));
  ADD_ERROR_MESSAGE(ER_SPATIAL_CANT_HAVE_NULL, N_("All parts of a SPATIAL index must be NOT NULL"));
  ADD_ERROR_MESSAGE(ER_COLLATION_CHARSET_MISMATCH, N_("COLLATION '%s' is not valid for CHARACTER SET '%s'"));
  ADD_ERROR_MESSAGE(ER_TOO_BIG_FOR_UNCOMPRESS, N_("Uncompressed data size too large; the maximum size is %d (based on max_allowed_packet). The length of uncompressed data may also be corrupted."));
  ADD_ERROR_MESSAGE(ER_ZLIB_Z_MEM_ERROR, N_("ZLIB: Not enough memory"));
  ADD_ERROR_MESSAGE(ER_ZLIB_Z_BUF_ERROR, N_("ZLIB: Not enough room in the output buffer (probably, length of uncompressed data was corrupted)"));
  ADD_ERROR_MESSAGE(ER_ZLIB_Z_DATA_ERROR, N_("ZLIB: Input data corrupted"));
  ADD_ERROR_MESSAGE(ER_CUT_VALUE_GROUP_CONCAT, N_("%d line(s) were cut by GROUP_CONCAT()"));
  ADD_ERROR_MESSAGE(ER_WARN_TOO_FEW_RECORDS, N_("Row %ld doesn't contain data for all columns"));
  ADD_ERROR_MESSAGE(ER_WARN_TOO_MANY_RECORDS, N_("Row %ld was truncated; it contained more data than there were input columns"));
  ADD_ERROR_MESSAGE(ER_WARN_NULL_TO_NOTNULL, N_("Column set to default value; NULL supplied to NOT NULL column '%s' at row %ld"));
  ADD_ERROR_MESSAGE(ER_WARN_DATA_OUT_OF_RANGE, N_("Out of range value for column '%s' at row %ld"));
  ADD_ERROR_MESSAGE(ER_WARN_DATA_TRUNCATED, N_("Data truncated for column '%s' at row %ld"));
  ADD_ERROR_MESSAGE(ER_CANT_AGGREGATE_2COLLATIONS, N_("Illegal mix of collations (%s,%s) and (%s,%s) for operation '%s'"));
  ADD_ERROR_MESSAGE(ER_CANT_AGGREGATE_3COLLATIONS, N_("Illegal mix of collations (%s,%s), (%s,%s), (%s,%s) for operation '%s'"));
  ADD_ERROR_MESSAGE(ER_CANT_AGGREGATE_NCOLLATIONS, N_("Illegal mix of collations for operation '%s'"));
  ADD_ERROR_MESSAGE(ER_VARIABLE_IS_NOT_STRUCT, N_("Variable '%-.64s' is not a variable component (can't be used as XXXX.variable_name)"));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_COLLATION, N_("Unknown collation: '%-.64s'"));
  ADD_ERROR_MESSAGE(ER_WARN_FIELD_RESOLVED, N_("Field or reference '%-.192s%s%-.192s%s%-.192s' of SELECT #%d was resolved in SELECT #%d"));
  ADD_ERROR_MESSAGE(ER_WRONG_NAME_FOR_INDEX, N_("Incorrect index name '%-.100s'"));
  ADD_ERROR_MESSAGE(ER_WRONG_NAME_FOR_CATALOG, N_("Incorrect catalog name '%-.100s'"));
  ADD_ERROR_MESSAGE(ER_BAD_FT_COLUMN, N_("Column '%-.192s' cannot be part of FULLTEXT index"));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_STORAGE_ENGINE, N_("Unknown table engine '%s'"));
  ADD_ERROR_MESSAGE(ER_NON_UPDATABLE_TABLE, N_("The target table %-.100s of the %s is not updatable"));
  ADD_ERROR_MESSAGE(ER_FEATURE_DISABLED, N_("The '%s' feature is disabled; you need Drizzle built with '%s' to have it working"));
  ADD_ERROR_MESSAGE(ER_OPTION_PREVENTS_STATEMENT, N_("The Drizzle server is running with the %s option so it cannot execute this statement"));
  ADD_ERROR_MESSAGE(ER_DUPLICATED_VALUE_IN_TYPE, N_("Column '%-.100s' has duplicated value '%-.64s' in %s"));
  ADD_ERROR_MESSAGE(ER_TRUNCATED_WRONG_VALUE, N_("Truncated incorrect %-.32s value: '%-.128s'"));
  ADD_ERROR_MESSAGE(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS, N_("Incorrect table definition; there can be only one TIMESTAMP column with CURRENT_TIMESTAMP in DEFAULT or ON UPDATE clause"));
  ADD_ERROR_MESSAGE(ER_INVALID_ON_UPDATE, N_("Invalid ON UPDATE clause for '%-.192s' column"));
  ADD_ERROR_MESSAGE(ER_GET_ERRMSG, N_("Got error %d '%-.100s' from %s"));
  ADD_ERROR_MESSAGE(ER_GET_TEMPORARY_ERRMSG, N_("Got temporary error %d '%-.100s' from %s"));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_TIME_ZONE, N_("Unknown or incorrect time zone: '%-.64s'"));
  ADD_ERROR_MESSAGE(ER_INVALID_CHARACTER_STRING, N_("Invalid %s character string: '%.64s'"));
  ADD_ERROR_MESSAGE(ER_WARN_ALLOWED_PACKET_OVERFLOWED, N_("Result of %s() was larger than max_allowed_packet (%ld) - truncated"));
  ADD_ERROR_MESSAGE(ER_SP_DOES_NOT_EXIST, N_("%s %s does not exist"));
  ADD_ERROR_MESSAGE(ER_QUERY_INTERRUPTED, N_("Query execution was interrupted"));
  ADD_ERROR_MESSAGE(ER_VIEW_INVALID, N_("View '%-.192s.%-.192s' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them"));
  ADD_ERROR_MESSAGE(ER_NO_DEFAULT_FOR_FIELD, N_("Field '%-.192s' doesn't have a default value"));
  ADD_ERROR_MESSAGE(ER_DIVISION_BY_ZERO, N_("Division by 0"));
  ADD_ERROR_MESSAGE(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD, N_("Incorrect %-.32s value: '%-.128s' for column '%.192s' at row %u"));
  ADD_ERROR_MESSAGE(ER_ILLEGAL_VALUE_FOR_TYPE, N_("Illegal %s '%-.192s' value found during parsing"));
  ADD_ERROR_MESSAGE(ER_KEY_PART_0, N_("Key part '%-.192s' length cannot be 0"));
  ADD_ERROR_MESSAGE(ER_XAER_RMFAIL, N_("XAER_RMFAIL: The command cannot be executed when global transaction is in the  %.64s state"));
  ADD_ERROR_MESSAGE(ER_DATA_TOO_LONG, N_("Data too long for column '%s' at row %ld"));
  ADD_ERROR_MESSAGE(ER_STARTUP, N_("%s: ready for connections.\nVersion: '%s' %s\n"));
  ADD_ERROR_MESSAGE(ER_LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR, N_("Can't load value from file with fixed size rows to variable"));
  ADD_ERROR_MESSAGE(ER_WRONG_VALUE_FOR_TYPE, N_("Incorrect %-.32s value: '%-.128s' for function %-.32s"));
  ADD_ERROR_MESSAGE(ER_TABLE_DEF_CHANGED, N_("Table definition has changed, please retry transaction"));
  ADD_ERROR_MESSAGE(ER_SP_NO_RETSET, N_("Not allowed to return a result set from a %s"));
  ADD_ERROR_MESSAGE(ER_CANT_CREATE_GEOMETRY_OBJECT, N_("Cannot get geometry object from data you send to the GEOMETRY field"));
  ADD_ERROR_MESSAGE(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, N_("Explicit or implicit commit is not allowed in stored function or trigger."));
  ADD_ERROR_MESSAGE(ER_TOO_BIG_SCALE, N_("Too big scale %d specified for column '%-.192s'. Maximum is %d."));
  ADD_ERROR_MESSAGE(ER_TOO_BIG_PRECISION, N_("Too big precision %d specified for column '%-.192s'. Maximum is %d."));
  ADD_ERROR_MESSAGE(ER_M_BIGGER_THAN_D, N_("For float(M,D), double(M,D) or decimal(M,D), M must be >= D (column '%-.192s')."));
  ADD_ERROR_MESSAGE(ER_TRG_IN_WRONG_SCHEMA, N_("Trigger in wrong schema"));
  ADD_ERROR_MESSAGE(ER_STACK_OVERRUN_NEED_MORE, N_("Thread stack overrun:  %ld bytes used of a %ld byte stack, and %ld bytes needed.  Use 'drizzled -O thread_stack=#' to specify a bigger stack."));
  ADD_ERROR_MESSAGE(ER_TOO_BIG_DISPLAYWIDTH, N_("Display width out of range for column '%-.192s' (max = %d)"));
  ADD_ERROR_MESSAGE(ER_DATETIME_FUNCTION_OVERFLOW, N_("Datetime function: %-.32s field overflow"));
  ADD_ERROR_MESSAGE(ER_ROW_IS_REFERENCED_2, N_("Cannot delete or update a parent row: a foreign key constraint fails (%.192s)"));
  ADD_ERROR_MESSAGE(ER_NO_REFERENCED_ROW_2, N_("Cannot add or update a child row: a foreign key constraint fails (%.192s)"));
  ADD_ERROR_MESSAGE(ER_SP_FETCH_NO_DATA, N_("No data - zero rows fetched, selected, or processed"));
  ADD_ERROR_MESSAGE(ER_TABLE_NEEDS_UPGRADE, N_("Table upgrade required. Please do \"REPAIR TABLE `%-.32s`\" to fix it!"));
  ADD_ERROR_MESSAGE(ER_NON_GROUPING_FIELD_USED, N_("non-grouping field '%-.192s' is used in %-.64s clause"));
  ADD_ERROR_MESSAGE(ER_TABLE_CANT_HANDLE_SPKEYS, N_("The used table type doesn't support SPATIAL indexes"));
  ADD_ERROR_MESSAGE(ER_REMOVED_SPACES, N_("Leading spaces are removed from name '%s'"));
  ADD_ERROR_MESSAGE(ER_AUTOINC_READ_FAILED, N_("Failed to read auto-increment value from storage engine"));
  ADD_ERROR_MESSAGE(ER_WRONG_STRING_LENGTH, N_("String '%-.70s' is too long for %s (should be no longer than %d)"));
  ADD_ERROR_MESSAGE(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT, N_("Too high level of nesting for select"));
  ADD_ERROR_MESSAGE(ER_NAME_BECOMES_EMPTY, N_("Name '%-.64s' has become ''"));
  ADD_ERROR_MESSAGE(ER_AMBIGUOUS_FIELD_TERM, N_("First character of the FIELDS TERMINATED string is ambiguous; please use non-optional and non-empty FIELDS ENCLOSED BY"));
  ADD_ERROR_MESSAGE(ER_ILLEGAL_HA_CREATE_OPTION, N_("Table storage engine '%-.64s' does not support the create option '%.64s'"));
  ADD_ERROR_MESSAGE(ER_INVALID_OPTION_VALUE, N_("Error setting %-.32s. Given value %-.128s %-.128s"));
  ADD_ERROR_MESSAGE(ER_WRONG_VALUE, N_("Incorrect %-.32s value: '%-.128s'"));
  ADD_ERROR_MESSAGE(ER_NO_PARTITION_FOR_GIVEN_VALUE, N_("Table has no partition for value %-.64s"));
  ADD_ERROR_MESSAGE(ER_BINLOG_ROW_LOGGING_FAILED, N_("Writing one row to the row-based binary log failed"));
  ADD_ERROR_MESSAGE(ER_DROP_INDEX_FK, N_("Cannot drop index '%-.192s': needed in a foreign key constraint"));
  ADD_ERROR_MESSAGE(ER_FOREIGN_DUPLICATE_KEY, N_("Upholding foreign key constraints for table '%.192s', entry '%-.192s', key %d would lead to a duplicate entry"));
  ADD_ERROR_MESSAGE(ER_CANT_CHANGE_TX_ISOLATION, N_("Transaction isolation level can't be changed while a transaction is in progress"));
  ADD_ERROR_MESSAGE(ER_WRONG_PARAMCOUNT_TO_FUNCTION, N_("Incorrect parameter count in the call to native function '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_WRONG_PARAMETERS_TO_NATIVE_FCT, N_("Incorrect parameters in the call to native function '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_DUP_ENTRY_WITH_KEY_NAME, N_("Duplicate entry '%-.64s' for key '%-.192s'"));
  ADD_ERROR_MESSAGE(ER_LOAD_DATA_INVALID_COLUMN, N_("Invalid column reference (%-.64s) in LOAD DATA"));

  ADD_ERROR_MESSAGE(ER_INVALID_DATETIME_VALUE, N_("Received an invalid datetime value '%s'."));
  ADD_ERROR_MESSAGE(ER_INVALID_DATE_VALUE, N_("Received an invalid DATE value '%s'."));
  ADD_ERROR_MESSAGE(ER_INVALID_NULL_ARGUMENT, N_("Received a NULL argument for function '%s'."));
  ADD_ERROR_MESSAGE(ER_INVALID_TIMESTAMP_VALUE, N_("Received an invalid timestamp value '%s'."));
  ADD_ERROR_MESSAGE(ER_INVALID_TIME_VALUE, N_("Received an invalid TIME value '%s'."));
  ADD_ERROR_MESSAGE(ER_INVALID_UNIX_TIMESTAMP_VALUE, N_("Received an invalid value '%s' for a UNIX timestamp."));

  ADD_ERROR_MESSAGE(ER_ARGUMENT_OUT_OF_RANGE, N_("Received an out-of-range argument '%s' for function '%s'."));
  ADD_ERROR_MESSAGE(ER_INVALID_ENUM_VALUE, N_("Received an invalid enum value '%s'."));
  ADD_ERROR_MESSAGE(ER_NO_PRIMARY_KEY_ON_REPLICATED_TABLE, N_("Tables which are replicated require a primary key."));

  ADD_ERROR_MESSAGE(ER_CORRUPT_SCHEMA_DEFINITION, N_("Corrupt or invalid schema definition for '%s' : %s"));
  ADD_ERROR_MESSAGE(ER_CORRUPT_TABLE_DEFINITION, N_("Corrupt or invalid table definition for '%s': %s"));
  ADD_ERROR_MESSAGE(ER_CORRUPT_TABLE_DEFINITION_ENUM, N_("The number of enum that were required was too high for table '%s'"));
  ADD_ERROR_MESSAGE(ER_CORRUPT_TABLE_DEFINITION_UNKNOWN, N_("Corrupt or invalid table definition for '%s'"));
  ADD_ERROR_MESSAGE(ER_CORRUPT_TABLE_DEFINITION_UNKNOWN_COLLATION, N_("Collation '%s' for table %s is invalid/unknown"));

  ADD_ERROR_MESSAGE(ER_TABLE_DROP, N_("Cannot drop table '%s'"));
  ADD_ERROR_MESSAGE(ER_TABLE_DROP_ERROR_OCCURRED, N_("Error occurred while dropping table '%s'"));
  ADD_ERROR_MESSAGE(ER_TABLE_PERMISSION_DENIED, N_("Permission denied to create '%s'"));
  ADD_ERROR_MESSAGE(ER_TABLE_UNKNOWN, N_("Unknown table '%s'"));

  ADD_ERROR_MESSAGE(ER_SCHEMA_CANNOT_CREATE, N_("Cannot create schema '%s'"));
  ADD_ERROR_MESSAGE(ER_SCHEMA_DOES_NOT_EXIST, N_("Schema does not exist: %s"));
  ADD_ERROR_MESSAGE(ER_ALTER_SCHEMA, N_("Error altering schema: %s"));
  ADD_ERROR_MESSAGE(ER_DROP_SCHEMA, +N_("Error droppping Schema : %s"));

  ADD_ERROR_MESSAGE(ER_USE_SQL_BIG_RESULT, N_("Temporary table too large, rerun with SQL_BIG_RESULT."));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_ENGINE_OPTION, N_("Unknown table engine option key/pair %s = %s."));
  ADD_ERROR_MESSAGE(ER_UNKNOWN_SCHEMA_OPTION, N_("Unknown schema engine option key/pair %s = %s."));
  ADD_ERROR_MESSAGE(ER_CARTESIAN_JOIN_ATTEMPTED, N_("Implicit cartesian join attempted."));
  ADD_ERROR_MESSAGE(ER_ADMIN_ACCESS, N_("Admin access not allowed from this username/IP address."));

  // User lock/barrier error messages
  ADD_ERROR_MESSAGE(ER_USER_LOCKS_CANT_WAIT_ON_OWN_BARRIER, N_("wait() can not be called on session owning user defined barrier."));
  ADD_ERROR_MESSAGE(ER_USER_LOCKS_UNKNOWN_BARRIER, N_("Unknown user defined barrier requested."));
  ADD_ERROR_MESSAGE(ER_USER_LOCKS_NOT_OWNER_OF_BARRIER, N_("Session does not own user defined barrier."));
  ADD_ERROR_MESSAGE(ER_USER_LOCKS_CANT_WAIT_ON_OWN_LOCK, N_("Session can not wait on a user defined lock owned by the session."));
  ADD_ERROR_MESSAGE(ER_USER_LOCKS_NOT_OWNER_OF_LOCK, N_("Session does not own user defined lock."));

  ADD_ERROR_MESSAGE(ER_USER_LOCKS_INVALID_NAME_BARRIER, N_("Invalid name for user defined barrier."));
  ADD_ERROR_MESSAGE(ER_USER_LOCKS_INVALID_NAME_LOCK, N_("Invalid name for user defined lock."));

  ADD_ERROR_MESSAGE(ER_INVALID_ALTER_TABLE_FOR_NOT_NULL, N_("Either a DEFAULt value or NULL NULL description is required for a new column if table is not empty"));

  // Cast errors
  ADD_ERROR_MESSAGE(ER_INVALID_CAST_TO_UNSIGNED, N_("Cast to unsigned converted negative integer to it's positive complement: %s"));
  ADD_ERROR_MESSAGE(ER_INVALID_CAST_TO_SIGNED, N_("Invalid cast to signed integer: %s"));

  ADD_ERROR_MESSAGE(ER_SQL_KEYWORD, N_("Identifier '%.*s' is a SQL keyword."));


  ADD_ERROR_MESSAGE(EE_CANTUNLOCK, N_("Can't unlock file (Errcode: %d)"));
  ADD_ERROR_MESSAGE(EE_CANT_CHSIZE, N_("Can't change size of file (Errcode: %d)"));
  ADD_ERROR_MESSAGE(EE_CANT_OPEN_STREAM, N_("Can't open stream from handle (Errcode: %d)"));
  ADD_ERROR_MESSAGE(EE_LINK_WARNING, N_("Warning: '%s' had %d links"));
  ADD_ERROR_MESSAGE(EE_OPEN_WARNING, N_("Warning: %d files and %d streams is left open\n"));
  ADD_ERROR_MESSAGE(EE_CANT_MKDIR, N_("Can't create directory '%s' (Errcode: %d)"));
  ADD_ERROR_MESSAGE(EE_UNKNOWN_CHARSET, N_("Character set '%s' is not a compiled character set and is not specified in the %s file"));
  ADD_ERROR_MESSAGE(EE_OUT_OF_FILERESOURCES, N_("Out of resources when opening file '%s' (Errcode: %d)"));
  ADD_ERROR_MESSAGE(EE_CANT_READLINK, N_("Can't read value for symlink '%s' (Error %d)"));
  ADD_ERROR_MESSAGE(EE_CANT_SYMLINK, N_("Can't create symlink '%s' pointing at '%s' (Error %d)"));
  ADD_ERROR_MESSAGE(EE_REALPATH, N_("Error on realpath() on '%s' (Error %d)"));
  ADD_ERROR_MESSAGE(EE_SYNC, N_("Can't sync file '%s' to disk (Errcode: %d)"));
  ADD_ERROR_MESSAGE(EE_UNKNOWN_COLLATION, N_("Collation '%s' is not a compiled collation and is not specified in the %s file"));
  ADD_ERROR_MESSAGE(EE_FILE_NOT_CLOSED, N_("File '%s' (fileno: %d) was not closed"));

  // For UUID type
  ADD_ERROR_MESSAGE(ER_INVALID_UUID_VALUE, N_("Received an invalid UUID value"));
  ADD_ERROR_MESSAGE(ER_INVALID_UUID_TIME, N_("The UUID was not created with a valid time"));

  // For BOOL type
  ADD_ERROR_MESSAGE(ER_INVALID_BOOLEAN_VALUE, N_("Received an invalid BOOLEAN value '%s'."));
  ADD_ERROR_MESSAGE(ER_INVALID_CAST_TO_BOOLEAN, N_("Invalid cast to BOOLEAN: '%s'."));

  // Transactional DDL
  ADD_ERROR_MESSAGE(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, N_("Transactional DDL not supported"));
  // ASSERT Message
  ADD_ERROR_MESSAGE(ER_ASSERT, N_("Assertion '%s' failed."));
  ADD_ERROR_MESSAGE(ER_ASSERT_NULL, N_("Assertion '%s' failed, the result was NULL."));

  // Some old error values use the same strings as some new error values.
  ADD_ERROR_MESSAGE(EE_FILENOTFOUND, *find(ER_FILE_NOT_FOUND));
  ADD_ERROR_MESSAGE(EE_CANTCREATEFILE, *find(ER_CANT_CREATE_FILE));
  ADD_ERROR_MESSAGE(EE_READ, *find(ER_ERROR_ON_READ));
  ADD_ERROR_MESSAGE(EE_WRITE, *find(ER_ERROR_ON_WRITE));
  ADD_ERROR_MESSAGE(EE_BADCLOSE, *find(ER_ERROR_ON_CLOSE));
  ADD_ERROR_MESSAGE(EE_OUTOFMEMORY, *find(ER_OUTOFMEMORY));
  ADD_ERROR_MESSAGE(EE_DELETE, *find(ER_CANT_DELETE_FILE));
  ADD_ERROR_MESSAGE(EE_LINK, *find(ER_ERROR_ON_RENAME));
  ADD_ERROR_MESSAGE(EE_EOFERR, *find(ER_UNEXPECTED_EOF));
  ADD_ERROR_MESSAGE(EE_CANTLOCK, *find(ER_CANT_LOCK));
  ADD_ERROR_MESSAGE(EE_DIR, *find(ER_CANT_READ_DIR));
  ADD_ERROR_MESSAGE(EE_STAT, *find(ER_CANT_GET_STAT));
  ADD_ERROR_MESSAGE(EE_DISK_FULL, *find(ER_DISK_FULL));
  
  // Catalog related errors
  ADD_ERROR_MESSAGE(ER_CATALOG_CANNOT_CREATE, N_("Cannot create catalog '%s'."));
  ADD_ERROR_MESSAGE(ER_CATALOG_CANNOT_CREATE_PERMISSION, N_("Permission is denied to create '%s' catalog."));
  ADD_ERROR_MESSAGE(ER_CATALOG_CANNOT_DROP, N_("Cannot drop catalog '%s'."));
  ADD_ERROR_MESSAGE(ER_CATALOG_CANNOT_DROP_PERMISSION, N_("Permission is denied to drop '%s' catalog."));
  ADD_ERROR_MESSAGE(ER_CATALOG_DOES_NOT_EXIST, N_("Catalog '%s' does not exist."));
  ADD_ERROR_MESSAGE(ER_CATALOG_NO_DROP_LOCAL, N_("You cannot drop the 'local' catalog."));
  ADD_ERROR_MESSAGE(ER_CATALOG_NO_LOCK, N_("Could not gain lock on '%s'."));
  ADD_ERROR_MESSAGE(ER_CORRUPT_CATALOG_DEFINITION, N_("Corrupt or invalid catalog definition for '%s' : '%s'."));
  ADD_ERROR_MESSAGE(ER_WRONG_NAME_FOR_CATALOG, N_("Invalid catalog name."));
  ADD_ERROR_MESSAGE(ER_USE_DATA_DICTIONARY, N_("Engine status is now stored in the data_dictionary tables, please use these instead."));
  ADD_ERROR_MESSAGE(ER_TRANSACTION_ALREADY_STARTED, N_("There is already a transaction in progress"));
  ADD_ERROR_MESSAGE(ER_NO_LOCK_HELD, N_("No lock is held by this connection."));
}

} /* namespace drizzled */
