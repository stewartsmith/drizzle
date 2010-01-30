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
 *   Errors a drizzled can give you
 *   */

#include "config.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/definitions.h"
#include "drizzled/error.h"
#include "drizzled/gettext.h"

namespace {

// Use unsigned long rather than the drizzled_error_code as we plan
// on having other plugins add to the error codes.
typedef drizzled::hash_map<uint32_t, std::string> ErrorMessageMap;
ErrorMessageMap errorMessages;

/**
 * DESIGN NOTES:
 *
 * 1) Have a central error store.
 * 2) Have a single function that can take an error number, and return the
 *    translated error string, or something that can output a translated string.
 * 2a) ideally we'd want to be able to have a function that inserts directly
 *    into the the translated error.
 * 3) Plugins should be able to register/unregister their own errors
 * 3a) error ids should not overlap, which does bring with it the idea of
 *    why do we care if the errors are not unregistered?  If plugins are not
 *    changed during execution, and are loaded at startup, the only time they
 *    are being unregistered is when the application is shutting down.  In which
 *    case we can leave it up to the standard library implementation to free up
 *    the memory which has been allocated.
 */

void add_error(uint32_t errno, std::string const& message)
{
  // Do we care about overriding the error messages?  Probably.
  if (errorMessages.find(errno) == errorMessages.end())
  {
    // DIE, DIE, DIE
  }
  errorMessages[errno]= message;
}

void init_drizzle_errors()
{
  add_error(ER_NO, N_("NO"));
  add_error(ER_YES, N_("YES"));

  add_error(ER_CANT_CREATE_FILE,
            N_("Can't create file '%-.200s' (errno: %d)"));

  add_error(ER_CANT_CREATE_TABLE,
            N_("Can't create table '%-.200s' (errno: %d)"));

  add_error(ER_CANT_CREATE_DB,
            N_("Can't create database '%-.192s' (errno: %d)"));

  add_error(ER_DB_CREATE_EXISTS,
            N_("Can't create database '%-.192s'; database exists"));

  add_error(ER_DB_DROP_EXISTS,
            N_("Can't drop database '%-.192s'; database doesn't exist"));

  add_error(ER_DB_DROP_DELETE,
            N_("Error dropping database (can't delete '%-.192s', errno: %d)"));

  add_error(ER_DB_DROP_RMDIR,
            N_("Error dropping database (can't rmdir '%-.192s', errno: %d)"));

  add_error(ER_CANT_DELETE_FILE,
            N_("Error on delete of '%-.192s' (errno: %d)"));

  add_error(ER_CANT_FIND_SYSTEM_REC,
            N_("Can't read record in system table"));

  add_error(ER_CANT_GET_STAT,
            N_("Can't get status of '%-.200s' (errno: %d)"));

  add_error(ER_CANT_GET_WD,
            N_("Can't get working directory (errno: %d)"));

  add_error(ER_CANT_LOCK,
            N_("Can't lock file (errno: %d)"));

  add_error(ER_CANT_OPEN_FILE,
            N_("Can't open file: '%-.200s' (errno: %d)"));

  add_error(ER_FILE_NOT_FOUND,
            N_("Can't find file: '%-.200s' (errno: %d)"));

  add_error(ER_CANT_READ_DIR,
            N_("Can't read dir of '%-.192s' (errno: %d)"));

  add_error(ER_CANT_SET_WD,
            N_("Can't change dir to '%-.192s' (errno: %d)"));

  add_error(ER_CHECKREAD,
            N_("Record has changed since last read in table '%-.192s'"));

  add_error(ER_DISK_FULL,
            N_("Disk full (%s); waiting for someone to free some space..."));

  add_error(ER_DUP_KEY, // 23000,
            N_("Can't write; duplicate key in table '%-.192s'"));

  add_error(ER_ERROR_ON_CLOSE,
            N_("Error on close of '%-.192s' (errno: %d)"));

  add_error(ER_ERROR_ON_READ,
            N_("Error reading file '%-.200s' (errno: %d)"));

  add_error(ER_ERROR_ON_RENAME,
            N_("Error on rename of '%-.150s' to '%-.150s' (errno: %d)"));

  add_error(ER_ERROR_ON_WRITE,
            N_("Error writing file '%-.200s' (errno: %d)"));

  add_error(ER_FILE_USED,
            N_("'%-.192s' is locked against change"));

  add_error(ER_FILSORT_ABORT,
            N_("Sort aborted"));

  add_error(ER_FORM_NOT_FOUND,
            N_("View '%-.192s' doesn't exist for '%-.192s'"));

  add_error(ER_GET_ERRNO,
            N_("Got error %d from storage engine"));

  add_error(ER_ILLEGAL_HA,
            N_("Table storage engine for '%-.192s' doesn't have this option"));

  add_error(ER_KEY_NOT_FOUND,
            N_("Can't find record in '%-.192s'"));

  add_error(ER_NOT_FORM_FILE,
            N_("Incorrect information in file: '%-.200s'"));

  add_error(ER_NOT_KEYFILE,
            N_("Incorrect key file for table '%-.200s'; try to repair it"));

  add_error(ER_OLD_KEYFILE,
            N_("Old key file for table '%-.192s'; repair it!"));

  add_error(ER_OPEN_AS_READONLY,
            N_("Table '%-.192s' is read only"));

  add_error(ER_OUTOFMEMORY, // HY001 S1001,
            N_("Out of memory; restart server and try again (needed %lu bytes)"));

  add_error(ER_OUT_OF_SORTMEMORY, // HY001 S1001,
            N_("Out of sort memory; increase server sort buffer size"));

  add_error(ER_UNEXPECTED_EOF,
            N_("Unexpected EOF found when reading file '%-.192s' (errno: %d)"));

  add_error(ER_CON_COUNT_ERROR, // 08004,
            N_("Too many connections"));

  add_error(ER_OUT_OF_RESOURCES,
            N_("Out of memory; check if drizzled or some other process uses all available memory; if not, you may have to use 'ulimit' to allow drizzled to use more memory or you can add more swap space"));

  add_error(ER_BAD_HOST_ERROR, // 08S01,
            N_("Can't get hostname for your address"));

  add_error(ER_HANDSHAKE_ERROR, // 08S01,
            N_("Bad handshake"));

  add_error(ER_DBACCESS_DENIED_ERROR, // 42000,
            N_("Access denied for user '%-.48s'@'%-.64s' to database '%-.192s'"));

  add_error(ER_ACCESS_DENIED_ERROR, // 28000,
            N_("Access denied for user '%-.48s'@'%-.64s' (using password: %s)"));

  add_error(ER_NO_DB_ERROR, // 3D000,
            N_("No database selected"));

  add_error(ER_UNKNOWN_COM_ERROR, // 08S01,
            N_("Unknown command"));

  add_error(ER_BAD_NULL_ERROR, // 23000,
            N_("Column '%-.192s' cannot be null"));

  add_error(ER_BAD_DB_ERROR, // 42000,
            N_("Unknown database '%-.192s'"));

  add_error(ER_TABLE_EXISTS_ERROR, // 42S01,
            N_("Table '%-.192s' already exists"));

  add_error(ER_BAD_TABLE_ERROR, // 42S02,
            N_("Unknown table '%-.100s'"));

  add_error(ER_NON_UNIQ_ERROR, // 23000,
            N_("Column '%-.192s' in %-.192s is ambiguous"));

  add_error(ER_SERVER_SHUTDOWN, // 08S01,
            N_("Server shutdown in progress"));

  add_error(ER_BAD_FIELD_ERROR, // 42S22 S0022,
            N_("Unknown column '%-.192s' in '%-.192s'"));

  add_error(ER_WRONG_FIELD_WITH_GROUP, // 42000 S1009,
            N_("'%-.192s' isn't in GROUP BY"));

  add_error(ER_WRONG_GROUP_FIELD, // 42000 S1009,
            N_("Can't group on '%-.192s'"));

  add_error(ER_WRONG_SUM_SELECT, // 42000 S1009,
            N_("Statement has sum functions and columns in same statement"));

  add_error(ER_WRONG_VALUE_COUNT, // 21S01,
            N_("Column count doesn't match value count"));

  add_error(ER_TOO_LONG_IDENT, // 42000 S1009,
            N_("Identifier name '%-.100s' is too long"));

  add_error(ER_DUP_FIELDNAME, // 42S21 S1009,
            N_("Duplicate column name '%-.192s'"));

  add_error(ER_DUP_KEYNAME, // 42000 S1009,
            N_("Duplicate key name '%-.192s'"));

  add_error(ER_DUP_ENTRY, // 23000 S1009,
            N_("Duplicate entry '%-.192s' for key %d"));

  add_error(ER_WRONG_FIELD_SPEC, // 42000 S1009,
            N_("Incorrect column specifier for column '%-.192s'"));

  add_error(ER_PARSE_ERROR, // 42000 s1009,
            N_("%s near '%-.80s' at line %d"));

  add_error(ER_EMPTY_QUERY, // 42000,
            N_("Query was empty"));

  add_error(ER_NONUNIQ_TABLE, // 42000 S1009,
            N_("Not unique table/alias: '%-.192s'"));

  add_error(ER_INVALID_DEFAULT, // 42000 S1009,
            N_("Invalid default value for '%-.192s'"));

  add_error(ER_MULTIPLE_PRI_KEY, // 42000 S1009,
            N_("Multiple primary key defined"));

  add_error(ER_TOO_MANY_KEYS, // 42000 S1009,
            N_("Too many keys specified; max %d keys allowed"));

  add_error(ER_TOO_MANY_KEY_PARTS, // 42000 S1009,
            N_("Too many key parts specified; max %d parts allowed"));

  add_error(ER_TOO_LONG_KEY, // 42000 S1009,
            N_("Specified key was too long; max key length is %d bytes"));

  add_error(ER_KEY_COLUMN_DOES_NOT_EXITS, // 42000 S1009,
            N_("Key column '%-.192s' doesn't exist in table"));

  add_error(ER_BLOB_USED_AS_KEY, // 42000 S1009,
            N_("BLOB column '%-.192s' can't be used in key specification with the used table type"));

  add_error(ER_TOO_BIG_FIELDLENGTH, // 42000 S1009,
            N_("Column length too big for column '%-.192s' (max = %d); use BLOB or TEXT instead"));

  add_error(ER_WRONG_AUTO_KEY, // 42000 S1009,
            N_("Incorrect table definition; there can be only one auto column and it must be defined as a key"));

  add_error(ER_READY,
            N_("%s: ready for connections.\nVersion: '%s'  socket: '%s'  port: %u\n"));

  add_error(ER_NORMAL_SHUTDOWN,
            N_("%s: Normal shutdown\n"));

  add_error(ER_GOT_SIGNAL,
            N_("%s: Got signal %d. Aborting!\n"));

  add_error(ER_SHUTDOWN_COMPLETE,
            N_("%s: Shutdown complete\n"));

  add_error(ER_FORCING_CLOSE, // 08S01,
            N_("%s: Forcing close of thread %" PRIu64 " user: '%-.48s'\n"));

  add_error(ER_IPSOCK_ERROR, // 08S01,
            N_("Can't create IP socket"));

  add_error(ER_NO_SUCH_INDEX, // 42S12 S1009,
            N_("Table '%-.192s' has no index like the one used in CREATE INDEX; recreate the table"));

  add_error(ER_WRONG_FIELD_TERMINATORS, // 42000 S1009,
            N_("Field separator argument '%-.32s' with length '%d' is not what is expected; check the manual"));

  add_error(ER_BLOBS_AND_NO_TERMINATED, // 42000 S1009,
            N_("You can't use fixed rowlength with BLOBs; please use 'fields terminated by'"));

  add_error(ER_TEXTFILE_NOT_READABLE,
            N_("The file '%-.128s' must be in the database directory or be readable by all"));

  add_error(ER_FILE_EXISTS_ERROR,
            N_("File '%-.200s' already exists"));

  add_error(ER_LOAD_INFO,
            N_("Records: %ld  Deleted: %ld  Skipped: %ld  Warnings: %ld"));

  add_error(ER_ALTER_INFO,
            N_("Records: %ld  Duplicates: %ld"));

  add_error(ER_WRONG_SUB_KEY,
            N_("Incorrect prefix key; the used key part isn't a string, the used length is longer than the key part, or the storage engine doesn't support unique prefix keys"));

  add_error(ER_CANT_REMOVE_ALL_FIELDS, // 42000,
            N_("You can't delete all columns with ALTER TABLE; use DROP TABLE instead"));

  add_error(ER_CANT_DROP_FIELD_OR_KEY, // 42000,
            N_("Can't DROP '%-.192s'; check that column/key exists"));

  add_error(ER_INSERT_INFO,
            N_("Records: %ld  Duplicates: %ld  Warnings: %ld"));

  add_error(ER_UPDATE_TABLE_USED,
            N_("You can't specify target table '%-.192s' for update in FROM clause"));

  add_error(ER_NO_SUCH_THREAD,
            N_("Unknown thread id: %lu"));

  add_error(ER_KILL_DENIED_ERROR,
            N_("You are not owner of thread %lu"));

  add_error(ER_NO_TABLES_USED,
            N_("No tables used"));

  add_error(ER_TOO_BIG_SET,
            N_("Too many strings for column %-.192s and SET"));

  add_error(ER_NO_UNIQUE_LOGFILE,
            N_("Can't generate a unique log-filename %-.200s.(1-999)\n"));

  add_error(ER_TABLE_NOT_LOCKED_FOR_WRITE,
            N_("Table '%-.192s' was locked with a READ lock and can't be updated"));

  add_error(ER_TABLE_NOT_LOCKED,
            N_("Table '%-.192s' was not locked with LOCK TABLES"));

  add_error(ER_BLOB_CANT_HAVE_DEFAULT, // 42000,
            N_("BLOB/TEXT column '%-.192s' can't have a default value"));

  add_error(ER_WRONG_DB_NAME, // 42000,
            N_("Incorrect database name '%-.100s'"));

  add_error(ER_WRONG_TABLE_NAME, // 42000,
            N_("Incorrect table name '%-.100s'"));

  add_error(ER_TOO_BIG_SELECT, // 42000,
            N_("The SELECT would examine more than MAX_JOIN_SIZE rows; check your WHERE and use SET SQL_BIG_SELECTS=1 or SET MAX_JOIN_SIZE=# if the SELECT is okay"));

  add_error(ER_UNKNOWN_ERROR,
            N_("Unknown error"));

  add_error(ER_UNKNOWN_PROCEDURE, // 42000,
            N_("Unknown procedure '%-.192s'"));

  add_error(ER_WRONG_PARAMCOUNT_TO_PROCEDURE, // 42000,
            N_("Incorrect parameter count to procedure '%-.192s'"));

  add_error(ER_WRONG_PARAMETERS_TO_PROCEDURE,
            N_("Incorrect parameters to procedure '%-.192s'"));

  add_error(ER_UNKNOWN_TABLE, // 42S02,
            N_("Unknown table '%-.192s' in %-.32s"));

  add_error(ER_FIELD_SPECIFIED_TWICE, // 42000,
            N_("Column '%-.192s' specified twice"));

  add_error(ER_INVALID_GROUP_FUNC_USE,
            N_("Invalid use of group function"));

  add_error(ER_UNSUPPORTED_EXTENSION, // 42000,
            N_("Table '%-.192s' uses an extension that doesn't exist in this Drizzle version"));

  add_error(ER_TABLE_MUST_HAVE_COLUMNS, // 42000,
            N_("A table must have at least 1 column"));

  add_error(ER_RECORD_FILE_FULL,
            N_("The table '%-.192s' is full"));

  add_error(ER_UNKNOWN_CHARACTER_SET, // 42000,
            N_("Unknown character set: '%-.64s'"));

  add_error(ER_TOO_MANY_TABLES,
            N_("Too many tables; Drizzle can only use %d tables in a join"));

  add_error(ER_TOO_MANY_FIELDS,
            N_("Too many columns"));

  add_error(ER_TOO_BIG_ROWSIZE, // 42000,
            N_("Row size too large. The maximum row size for the used table type, not counting BLOBs, is %ld. You have to change some columns to TEXT or BLOBs"));

  add_error(ER_STACK_OVERRUN,
            N_("Thread stack overrun:  Used: %ld of a %ld stack.  Use 'drizzled -O thread_stack=#' to specify a bigger stack if needed"));

  add_error(ER_WRONG_OUTER_JOIN, // 42000,
            N_("Cross dependency found in OUTER JOIN; examine your ON conditions"));

  add_error(ER_NULL_COLUMN_IN_INDEX, // 42000,
            N_("Table handler doesn't support NULL in given index. Please change column '%-.192s' to be NOT NULL or use another handler"));

  add_error(ER_CANT_FIND_UDF,
            N_("Can't load function '%-.192s'"));

  add_error(ER_CANT_INITIALIZE_UDF,
            N_("Can't initialize function '%-.192s'; %-.80s"));

  add_error(ER_PLUGIN_NO_PATHS,
            N_("No paths allowed for plugin library"));

  add_error(ER_UDF_EXISTS,
            N_("Plugin '%-.192s' already exists"));

  add_error(ER_CANT_OPEN_LIBRARY,
            N_("Can't open shared library '%-.192s' (errno: %d %-.128s)"));

  add_error(ER_CANT_FIND_DL_ENTRY,
            N_("Can't find symbol '%-.128s' in library '%-.128s'"));

  add_error(ER_FUNCTION_NOT_DEFINED,
            N_("Function '%-.192s' is not defined"));

  add_error(ER_HOST_IS_BLOCKED,
            N_("Host '%-.64s' is blocked because of many connection errors; unblock with 'drizzleadmin flush-hosts'"));

  add_error(ER_HOST_NOT_PRIVILEGED,
            N_("Host '%-.64s' is not allowed to connect to this Drizzle server"));

  add_error(ER_PASSWORD_ANONYMOUS_USER, // 42000,
            N_("You are using Drizzle as an anonymous user and anonymous users are not allowed to change passwords"));

  add_error(ER_PASSWORD_NOT_ALLOWED, // 42000,
            N_("You must have privileges to update tables in the drizzle database to be able to change passwords for others"));

  add_error(ER_PASSWORD_NO_MATCH, // 42000,
            N_("Can't find any matching row in the user table"));

  add_error(ER_UPDATE_INFO,
            N_("Rows matched: %ld  Changed: %ld  Warnings: %ld"));

  add_error(ER_CANT_CREATE_THREAD,
            N_("Can't create a new thread (errno %d); if you are not out of available memory, you can consult the manual for a possible OS-dependent bug"));

  add_error(ER_WRONG_VALUE_COUNT_ON_ROW, // 21S01,
            N_("Column count doesn't match value count at row %ld"));

  add_error(ER_CANT_REOPEN_TABLE,
            N_("Can't reopen table: '%-.192s'"));

  add_error(ER_INVALID_USE_OF_NULL, // 22004,
            N_("Invalid use of NULL value"));

  add_error(ER_REGEXP_ERROR, // 42000,
            N_("Got error '%-.64s' from regexp"));

  add_error(ER_MIX_OF_GROUP_FUNC_AND_FIELDS, // 42000,
            N_("Mixing of GROUP columns (MIN(),MAX(),COUNT(),...) with no GROUP columns is illegal if there is no GROUP BY clause"));

  add_error(ER_NONEXISTING_GRANT, // 42000,
            N_("There is no such grant defined for user '%-.48s' on host '%-.64s'"));

  add_error(ER_TABLEACCESS_DENIED_ERROR, // 42000,
            N_("%-.16s command denied to user '%-.48s'@'%-.64s' for table '%-.192s'"));

  add_error(ER_COLUMNACCESS_DENIED_ERROR, // 42000,
            N_("%-.16s command denied to user '%-.48s'@'%-.64s' for column '%-.192s' in table '%-.192s'"));

  add_error(ER_ILLEGAL_GRANT_FOR_TABLE, // 42000,
            N_("Illegal GRANT/REVOKE command; please consult the manual to see which privileges can be used"));

  add_error(ER_GRANT_WRONG_HOST_OR_USER, // 42000,
            N_("The host or user argument to GRANT is too long"));

  add_error(ER_NO_SUCH_TABLE, // 42S02,
            N_("Table '%-.192s.%-.192s' doesn't exist"));

  add_error(ER_NONEXISTING_TABLE_GRANT, // 42000,
            N_("There is no such grant defined for user '%-.48s' on host '%-.64s' on table '%-.192s'"));

  add_error(ER_NOT_ALLOWED_COMMAND, // 42000,
            N_("The used command is not allowed with this Drizzle version"));

  add_error(ER_SYNTAX_ERROR, // 42000,
            N_("You have an error in your SQL syntax; check the manual that corresponds to your Drizzle server version for the right syntax to use"));

  add_error(ER_DELAYED_CANT_CHANGE_LOCK,
            N_("Delayed insert thread couldn't get requested lock for table %-.192s"));

  add_error(ER_TOO_MANY_DELAYED_THREADS,
            N_("Too many delayed threads in use"));

  add_error(ER_ABORTING_CONNECTION, // 08S01,
            N_("Aborted connection %ld to db: '%-.192s' user: '%-.48s' (%-.64s)"));

  add_error(ER_NET_PACKET_TOO_LARGE, // 08S01,
            N_("Got a packet bigger than 'max_allowed_packet' bytes"));

  add_error(ER_NET_READ_ERROR_FROM_PIPE, // 08S01,
            N_("Got a read error from the connection pipe"));

  add_error(ER_NET_FCNTL_ERROR, // 08S01,
            N_("Got an error from fcntl()"));

  add_error(ER_NET_PACKETS_OUT_OF_ORDER, // 08S01,
            N_("Got packets out of order"));

  add_error(ER_NET_UNCOMPRESS_ERROR, // 08S01,
            N_("Couldn't uncompress communication packet"));

  add_error(ER_NET_READ_ERROR, // 08S01,
            N_("Got an error reading communication packets"));

  add_error(ER_NET_READ_INTERRUPTED, // 08S01,
            N_("Got timeout reading communication packets"));

  add_error(ER_NET_ERROR_ON_WRITE, // 08S01,
            N_("Got an error writing communication packets"));

  add_error(ER_NET_WRITE_INTERRUPTED, // 08S01,
            N_("Got timeout writing communication packets"));

  add_error(ER_TOO_LONG_STRING, // 42000,
            N_("Result string is longer than 'max_allowed_packet' bytes"));

  add_error(ER_TABLE_CANT_HANDLE_BLOB, // 42000,
            N_("The used table type doesn't support BLOB/TEXT columns"));

  add_error(ER_TABLE_CANT_HANDLE_AUTO_INCREMENT, // 42000,
            N_("The used table type doesn't support AUTO_INCREMENT columns"));

  add_error(ER_DELAYED_INSERT_TABLE_LOCKED,
            N_("INSERT DELAYED can't be used with table '%-.192s' because it is locked with LOCK TABLES"));

  add_error(ER_WRONG_COLUMN_NAME, // 42000,
            N_("Incorrect column name '%-.100s'"));

  add_error(ER_WRONG_KEY_COLUMN, // 42000,
            N_("The used storage engine can't index column '%-.192s'"));

  add_error(ER_WRONG_MRG_TABLE,
            N_("Unable to open underlying table which is differently defined or of non-MyISAM type or doesn't exist"));

  add_error(ER_DUP_UNIQUE, // 23000,
            N_("Can't write, because of unique constraint, to table '%-.192s'"));

  add_error(ER_BLOB_KEY_WITHOUT_LENGTH, // 42000,
            N_("BLOB/TEXT column '%-.192s' used in key specification without a key length"));

  add_error(ER_PRIMARY_CANT_HAVE_NULL, // 42000,
            N_("All parts of a PRIMARY KEY must be NOT NULL; if you need NULL in a key, use UNIQUE instead"));

  add_error(ER_TOO_MANY_ROWS, // 42000,
            N_("Result consisted of more than one row"));

  add_error(ER_REQUIRES_PRIMARY_KEY, // 42000,
            N_("This table type requires a primary key"));

  add_error(ER_NO_RAID_COMPILED,
            N_("This version of Drizzle is not compiled with RAID support"));

  add_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
            N_("You are using safe update mode and you tried to update a table without a WHERE that uses a KEY column"));

  add_error(ER_KEY_DOES_NOT_EXITS, // 42000 S1009,
            N_("Key '%-.192s' doesn't exist in table '%-.192s'"));

  add_error(ER_CHECK_NO_SUCH_TABLE, // 42000,
            N_("Can't open table"));

  add_error(ER_CHECK_NOT_IMPLEMENTED, // 42000,
            N_("The storage engine for the table doesn't support %s"));

  add_error(ER_CANT_DO_THIS_DURING_AN_TRANSACTION, // 25000,
            N_("You are not allowed to execute this command in a transaction"));

  add_error(ER_ERROR_DURING_COMMIT,
            N_("Got error %d during COMMIT"));

  add_error(ER_ERROR_DURING_ROLLBACK,
            N_("Got error %d during ROLLBACK"));

  add_error(ER_ERROR_DURING_FLUSH_LOGS,
            N_("Got error %d during FLUSH_LOGS"));

  add_error(ER_ERROR_DURING_CHECKPOINT,
            N_("Got error %d during CHECKPOINT"));

  // WTF is this?
  add_error(ER_NEW_ABORTING_CONNECTION, // 08S01,
            N_("Aborted connection %"PRIi64" to db: '%-.192s' user: '%-.48s' host: '%-.64s' (%-.64s)"));

  add_error(ER_DUMP_NOT_IMPLEMENTED,
            N_("The storage engine for the table does not support binary table dump"));

  add_error(ER_FLUSH_MASTER_BINLOG_CLOSED,
            N_("Binlog closed, cannot RESET MASTER"));

  add_error(ER_INDEX_REBUILD,
            N_("Failed rebuilding the index of  dumped table '%-.192s'"));

  add_error(ER_MASTER,
            N_("Error from master: '%-.64s'"));

  add_error(ER_MASTER_NET_READ, // 08S01,
            N_("Net error reading from master"));

  add_error(ER_MASTER_NET_WRITE, // 08S01,
            N_("Net error writing to master"));

  add_error(ER_FT_MATCHING_KEY_NOT_FOUND,
            N_("Can't find FULLTEXT index matching the column list"));

  add_error(ER_LOCK_OR_ACTIVE_TRANSACTION,
            N_("Can't execute the given command because you have active locked tables or an active transaction"));

  add_error(ER_UNKNOWN_SYSTEM_VARIABLE,
            N_("Unknown system variable '%-.64s'"));

  add_error(ER_CRASHED_ON_USAGE,
            N_("Table '%-.192s' is marked as crashed and should be repaired"));

  add_error(ER_CRASHED_ON_REPAIR,
            N_("Table '%-.192s' is marked as crashed and last (automatic?) repair failed"));

  add_error(ER_WARNING_NOT_COMPLETE_ROLLBACK,
            N_("Some non-transactional changed tables couldn't be rolled back"));

  add_error(ER_TRANS_CACHE_FULL,
            N_("Multi-statement transaction required more than 'max_binlog_cache_size' bytes of storage; increase this drizzled variable and try again"));

  add_error(ER_SLAVE_MUST_STOP,
            N_("This operation cannot be performed with a running slave; run STOP SLAVE first"));

  add_error(ER_SLAVE_NOT_RUNNING,
            N_("This operation requires a running slave; configure slave and do START SLAVE"));

  add_error(ER_BAD_SLAVE,
            N_("The server is not configured as slave; fix with CHANGE MASTER TO"));

  add_error(ER_MASTER_INFO,
            N_("Could not initialize master info structure; more error messages can be found in the Drizzle error log"));

  add_error(ER_SLAVE_THREAD,
            N_("Could not create slave thread; check system resources"));

  add_error(ER_TOO_MANY_USER_CONNECTIONS, // 42000,
            N_("User %-.64s already has more than 'max_user_connections' active connections"));

  add_error(ER_SET_CONSTANTS_ONLY,
            N_("You may only use constant expressions with SET"));

  add_error(ER_LOCK_WAIT_TIMEOUT,
            N_("Lock wait timeout exceeded; try restarting transaction"));

  add_error(ER_LOCK_TABLE_FULL,
            N_("The total number of locks exceeds the lock table size"));

  add_error(ER_READ_ONLY_TRANSACTION, // 25000,
            N_("Update locks cannot be acquired during a READ UNCOMMITTED transaction"));

  add_error(ER_DROP_DB_WITH_READ_LOCK,
            N_("DROP DATABASE not allowed while thread is holding global read lock"));

  add_error(ER_CREATE_DB_WITH_READ_LOCK,
            N_("CREATE DATABASE not allowed while thread is holding global read lock"));

  add_error(ER_WRONG_ARGUMENTS,
            N_("Incorrect arguments to %s"));

  add_error(ER_NO_PERMISSION_TO_CREATE_USER, // 42000,
            N_("'%-.48s'@'%-.64s' is not allowed to create new users"));

  add_error(ER_UNION_TABLES_IN_DIFFERENT_DIR,
            N_("Incorrect table definition; all MERGE tables must be in the same database"));

  add_error(ER_LOCK_DEADLOCK, // 40001,
            N_("Deadlock found when trying to get lock; try restarting transaction"));

  add_error(ER_TABLE_CANT_HANDLE_FT,
            N_("The used table type doesn't support FULLTEXT indexes"));

  add_error(ER_CANNOT_ADD_FOREIGN,
            N_("Cannot add foreign key constraint"));

  add_error(ER_NO_REFERENCED_ROW, // 23000,
            N_("Cannot add or update a child row: a foreign key constraint fails"));

  add_error(ER_ROW_IS_REFERENCED, // 23000,
            N_("Cannot delete or update a parent row: a foreign key constraint fails"));

  add_error(ER_CONNECT_TO_MASTER, // 08S01,
            N_("Error connecting to master: %-.128s"));

  add_error(ER_QUERY_ON_MASTER,
            N_("Error running query on master: %-.128s"));

  add_error(ER_ERROR_WHEN_EXECUTING_COMMAND,
            N_("Error when executing command %s: %-.128s"));

  add_error(ER_WRONG_USAGE,
            N_("Incorrect usage of %s and %s"));

  add_error(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT, // 21000,
            N_("The used SELECT statements have a different number of columns"));

  add_error(ER_CANT_UPDATE_WITH_READLOCK,
            N_("Can't execute the query because you have a conflicting read lock"));

  add_error(ER_MIXING_NOT_ALLOWED,
            N_("Mixing of transactional and non-transactional tables is disabled"));

  add_error(ER_DUP_ARGUMENT,
            N_("Option '%s' used twice in statement"));

  add_error(ER_USER_LIMIT_REACHED, // 42000,
            N_("User '%-.64s' has exceeded the '%s' resource (current value: %ld)"));

  add_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, // 42000,
            N_("Access denied; you need the %-.128s privilege for this operation"));

  add_error(ER_LOCAL_VARIABLE,
            N_("Variable '%-.64s' is a SESSION variable and can't be used with SET GLOBAL"));

  add_error(ER_GLOBAL_VARIABLE,
            N_("Variable '%-.64s' is a GLOBAL variable and should be set with SET GLOBAL"));

  add_error(ER_NO_DEFAULT, // 42000,
            N_("Variable '%-.64s' doesn't have a default value"));

  add_error(ER_WRONG_VALUE_FOR_VAR, // 42000,
            N_("Variable '%-.64s' can't be set to the value of '%-.200s'"));

  add_error(ER_WRONG_TYPE_FOR_VAR, // 42000,
            N_("Incorrect argument type to variable '%-.64s'"));

  add_error(ER_VAR_CANT_BE_READ,
            N_("Variable '%-.64s' can only be set, not read"));

  add_error(ER_CANT_USE_OPTION_HERE, // 42000,
            N_("Incorrect usage/placement of '%s'"));

  add_error(ER_NOT_SUPPORTED_YET, // 42000,
            N_("This version of Drizzle doesn't yet support '%s'"));

  add_error(ER_MASTER_FATAL_ERROR_READING_BINLOG,
            N_("Got fatal error %d: '%-.128s' from master when reading data from binary log"));

  add_error(ER_SLAVE_IGNORED_TABLE,
            N_("Slave SQL thread ignored the query because of replicate-*-table rules"));

  add_error(ER_INCORRECT_GLOBAL_LOCAL_VAR,
            N_("Variable '%-.192s' is a %s variable"));

  add_error(ER_WRONG_FK_DEF, // 42000,
            N_("Incorrect foreign key definition for '%-.192s': %s"));

  add_error(ER_KEY_REF_DO_NOT_MATCH_TABLE_REF,
            N_("Key reference and table reference don't match"));

  add_error(ER_OPERAND_COLUMNS, // 21000,
            N_("Operand should contain %d column(s)"));

  add_error(ER_SUBQUERY_NO_1_ROW, // 21000,
            N_("Subquery returns more than 1 row"));

  add_error(ER_UNKNOWN_STMT_HANDLER,
            N_("Unknown prepared statement handler (%.*s) given to %s"));

  add_error(ER_CORRUPT_HELP_DB,
            N_("Help database is corrupt or does not exist"));

  add_error(ER_CYCLIC_REFERENCE,
            N_("Cyclic reference on subqueries"));

  add_error(ER_AUTO_CONVERT,
            N_("Converting column '%s' from %s to %s"));

  add_error(ER_ILLEGAL_REFERENCE, // 42S22,
            N_("Reference '%-.64s' not supported (%s)"));

  add_error(ER_DERIVED_MUST_HAVE_ALIAS, // 42000,
            N_("Every derived table must have its own alias"));

  add_error(ER_SELECT_REDUCED, // 01000,
            N_("Select %u was reduced during optimization"));

  add_error(ER_TABLENAME_NOT_ALLOWED_HERE, // 42000,
            N_("Table '%-.192s' from one of the SELECTs cannot be used in %-.32s"));

  add_error(ER_NOT_SUPPORTED_AUTH_MODE, // 08004,
            N_("Client does not support authentication protocol requested by server; consider upgrading Drizzle client"));

  add_error(ER_SPATIAL_CANT_HAVE_NULL, // 42000,
            N_("All parts of a SPATIAL index must be NOT NULL"));

  add_error(ER_COLLATION_CHARSET_MISMATCH, // 42000,
            N_("COLLATION '%s' is not valid for CHARACTER SET '%s'"));

  add_error(ER_SLAVE_WAS_RUNNING,
            N_("Slave is already running"));

  add_error(ER_SLAVE_WAS_NOT_RUNNING,
            N_("Slave already has been stopped"));

  add_error(ER_TOO_BIG_FOR_UNCOMPRESS,
            N_("Uncompressed data size too large; the maximum size is %d (probably, length of uncompressed data was corrupted)"));

  add_error(ER_ZLIB_Z_MEM_ERROR,
            N_("ZLIB: Not enough memory"));

  add_error(ER_ZLIB_Z_BUF_ERROR,
            N_("ZLIB: Not enough room in the output buffer (probably, length of uncompressed data was corrupted)"));

  add_error(ER_ZLIB_Z_DATA_ERROR,
            N_("ZLIB: Input data corrupted"));

  add_error(ER_CUT_VALUE_GROUP_CONCAT,
            N_("%d line(s) were cut by GROUP_CONCAT()"));

  add_error(ER_WARN_TOO_FEW_RECORDS, // 01000,
            N_("Row %ld doesn't contain data for all columns"));

  add_error(ER_WARN_TOO_MANY_RECORDS, // 01000,
            N_("Row %ld was truncated; it contained more data than there were input columns"));

  add_error(ER_WARN_NULL_TO_NOTNULL, // 22004,
            N_("Column set to default value; NULL supplied to NOT NULL column '%s' at row %ld"));

  add_error(ER_WARN_DATA_OUT_OF_RANGE, // 22003,
            N_("Out of range value for column '%s' at row %ld"));

  add_error(WARN_DATA_TRUNCATED, // 01000,
            N_("Data truncated for column '%s' at row %ld"));

  add_error(ER_WARN_USING_OTHER_HANDLER,
            N_("Using storage engine %s for table '%s'"));

  add_error(ER_CANT_AGGREGATE_2COLLATIONS,
            N_("Illegal mix of collations (%s,%s) and (%s,%s) for operation '%s'"));

  add_error(ER_DROP_USER,
            N_("Cannot drop one or more of the requested users"));

  add_error(ER_REVOKE_GRANTS,
            N_("Can't revoke all privileges for one or more of the requested users"));

  add_error(ER_CANT_AGGREGATE_3COLLATIONS,
            N_("Illegal mix of collations (%s,%s), (%s,%s), (%s,%s) for operation '%s'"));

  add_error(ER_CANT_AGGREGATE_NCOLLATIONS,
            N_("Illegal mix of collations for operation '%s'"));

  add_error(ER_VARIABLE_IS_NOT_STRUCT,
            N_("Variable '%-.64s' is not a variable component (can't be used as XXXX.variable_name)"));

  add_error(ER_UNKNOWN_COLLATION,
            N_("Unknown collation: '%-.64s'"));

  add_error(ER_SLAVE_IGNORED_SSL_PARAMS,
            N_("SSL parameters in CHANGE MASTER are ignored because this Drizzle slave was compiled without SSL support; they can be used later if Drizzle slave with SSL is started"));

  add_error(ER_SERVER_IS_IN_SECURE_AUTH_MODE,
            N_("Server is running in --secure-auth mode, but '%s'@'%s' has a password in the old format; please change the password to the new format"));

  add_error(ER_WARN_FIELD_RESOLVED,
            N_("Field or reference '%-.192s%s%-.192s%s%-.192s' of SELECT #%d was resolved in SELECT #%d"));

  add_error(ER_BAD_SLAVE_UNTIL_COND,
            N_("Incorrect parameter or combination of parameters for START SLAVE UNTIL"));

  add_error(ER_MISSING_SKIP_SLAVE,
            N_("It is recommended to use --skip-slave-start when doing step-by-step replication with START SLAVE UNTIL; otherwise, you will get problems if you get an unexpected slave's drizzled restart"));

  add_error(ER_UNTIL_COND_IGNORED,
            N_("SQL thread is not to be started so UNTIL options are ignored"));

  add_error(ER_WRONG_NAME_FOR_INDEX, // 42000,
            N_("Incorrect index name '%-.100s'"));

  add_error(ER_WRONG_NAME_FOR_CATALOG, // 42000,
            N_("Incorrect catalog name '%-.100s'"));

  add_error(ER_WARN_QC_RESIZE,
            N_("Query cache failed to set size %lu; new query cache size is %lu"));

  add_error(ER_BAD_FT_COLUMN,
            N_("Column '%-.192s' cannot be part of FULLTEXT index"));

  add_error(ER_UNKNOWN_KEY_CACHE,
            N_("Unknown key cache '%-.100s'"));

  add_error(ER_WARN_HOSTNAME_WONT_WORK,
            N_("Drizzle is started in --skip-name-resolve mode; you must restart it without this switch for this grant to work"));

  add_error(ER_UNKNOWN_STORAGE_ENGINE, // 42000,
            N_("Unknown table engine '%s'"));

  add_error(ER_WARN_DEPRECATED_SYNTAX,
            N_("'%s' is deprecated; use '%s' instead"));

  add_error(ER_NON_UPDATABLE_TABLE,
            N_("The target table %-.100s of the %s is not updatable"));

  add_error(ER_FEATURE_DISABLED,
            N_("The '%s' feature is disabled; you need Drizzle built with '%s' to have it working"));

  add_error(ER_OPTION_PREVENTS_STATEMENT,
            N_("The Drizzle server is running with the %s option so it cannot execute this statement"));

  add_error(ER_DUPLICATED_VALUE_IN_TYPE,
            N_("Column '%-.100s' has duplicated value '%-.64s' in %s"));

  add_error(ER_TRUNCATED_WRONG_VALUE, // 22007,
            N_("Truncated incorrect %-.32s value: '%-.128s'"));

  add_error(ER_TOO_MUCH_AUTO_TIMESTAMP_COLS,
            N_("Incorrect table definition; there can be only one TIMESTAMP column with CURRENT_TIMESTAMP in DEFAULT or ON UPDATE clause"));

  add_error(ER_INVALID_ON_UPDATE,
            N_("Invalid ON UPDATE clause for '%-.192s' column"));

  add_error(ER_UNSUPPORTED_PS,
            N_("This command is not supported in the prepared statement protocol yet"));

  add_error(ER_GET_ERRMSG,
            N_("Got error %d '%-.100s' from %s"));

  add_error(ER_GET_TEMPORARY_ERRMSG,
            N_("Got temporary error %d '%-.100s' from %s"));

  add_error(ER_UNKNOWN_TIME_ZONE,
            N_("Unknown or incorrect time zone: '%-.64s'"));

  add_error(ER_WARN_INVALID_TIMESTAMP,
            N_("Invalid TIMESTAMP value in column '%s' at row %ld"));

  add_error(ER_INVALID_CHARACTER_STRING,
            N_("Invalid %s character string: '%.64s'"));

  add_error(ER_WARN_ALLOWED_PACKET_OVERFLOWED,
            N_("Result of %s() was larger than max_allowed_packet (%ld) - truncated"));

  add_error(ER_CONFLICTING_DECLARATIONS,
            N_("Conflicting declarations: '%s%s' and '%s%s'"));

  add_error(ER_SP_NO_RECURSIVE_CREATE, // 2F003,
            N_("Can't create a %s from within another stored routine"));

  add_error(ER_SP_ALREADY_EXISTS, // 42000,
            N_("%s %s already exists"));

  add_error(ER_SP_DOES_NOT_EXIST, // 42000,
            N_("%s %s does not exist"));

  add_error(ER_SP_DROP_FAILED,
            N_("Failed to DROP %s %s"));

  add_error(ER_SP_STORE_FAILED,
            N_("Failed to CREATE %s %s"));

  add_error(ER_SP_LILABEL_MISMATCH, // 42000,
            N_("%s with no matching label: %s"));

  add_error(ER_SP_LABEL_REDEFINE, // 42000,
            N_("Redefining label %s"));

  add_error(ER_SP_LABEL_MISMATCH, // 42000,
            N_("End-label %s without match"));

  add_error(ER_SP_UNINIT_VAR, // 01000,
            N_("Referring to uninitialized variable %s"));

  add_error(ER_SP_BADSELECT, // 0A000,
            N_("PROCEDURE %s can't return a result set in the given context"));

  add_error(ER_SP_BADRETURN, // 42000,
            N_("RETURN is only allowed in a FUNCTION"));

  add_error(ER_SP_BADSTATEMENT, // 0A000,
            N_("%s is not allowed in stored procedures"));

  add_error(ER_UPDATE_LOG_DEPRECATED_IGNORED, // 42000,
            N_("The update log is deprecated and replaced by the binary log; SET SQL_LOG_UPDATE has been ignored"));

  add_error(ER_UPDATE_LOG_DEPRECATED_TRANSLATED, // 42000,
            N_("The update log is deprecated and replaced by the binary log; SET SQL_LOG_UPDATE has been translated to SET SQL_LOG_BIN"));

  add_error(ER_QUERY_INTERRUPTED, // 70100,
            N_("Query execution was interrupted"));

  add_error(ER_SP_WRONG_NO_OF_ARGS, // 42000,
            N_("Incorrect number of arguments for %s %s; expected %u, got %u"));

  add_error(ER_SP_COND_MISMATCH, // 42000,
            N_("Undefined CONDITION: %s"));

  add_error(ER_SP_NORETURN, // 42000,
            N_("No RETURN found in FUNCTION %s"));

  add_error(ER_SP_NORETURNEND, // 2F005,
            N_("FUNCTION %s ended without RETURN"));

  add_error(ER_SP_BAD_CURSOR_QUERY, // 42000,
            N_("Cursor statement must be a SELECT"));

  add_error(ER_SP_BAD_CURSOR_SELECT, // 42000,
            N_("Cursor SELECT must not have INTO"));

  add_error(ER_SP_CURSOR_MISMATCH, // 42000,
            N_("Undefined CURSOR: %s"));

  add_error(ER_SP_CURSOR_ALREADY_OPEN, // 24000,
            N_("Cursor is already open"));

  add_error(ER_SP_CURSOR_NOT_OPEN, // 24000,
            N_("Cursor is not open"));

  add_error(ER_SP_UNDECLARED_VAR, // 42000,
            N_("Undeclared variable: %s"));

  add_error(ER_SP_WRONG_NO_OF_FETCH_ARGS,
            N_("Incorrect number of FETCH variables"));

  add_error(ER_SP_FETCH_NO_DATA, // 02000,
            N_("No data - zero rows fetched, selected, or processed"));

  add_error(ER_SP_DUP_PARAM, // 42000,
            N_("Duplicate parameter: %s"));

  add_error(ER_SP_DUP_VAR, // 42000,
            N_("Duplicate variable: %s"));

  add_error(ER_SP_DUP_COND, // 42000,
            N_("Duplicate condition: %s"));

  add_error(ER_SP_DUP_CURS, // 42000,
            N_("Duplicate cursor: %s"));

  add_error(ER_SP_CANT_ALTER,
            N_("Failed to ALTER %s %s"));

  add_error(ER_SP_SUBSELECT_NYI, // 0A000,
            N_("Subquery value not supported"));

  add_error(ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG, // 0A000,
            N_("%s is not allowed in stored function or trigger"));

  add_error(ER_SP_VARCOND_AFTER_CURSHNDLR, // 42000,
            N_("Variable or condition declaration after cursor or handler declaration"));

  add_error(ER_SP_CURSOR_AFTER_HANDLER, // 42000,
            N_("Cursor declaration after handler declaration"));

  add_error(ER_SP_CASE_NOT_FOUND, // 20000,
            N_("Case not found for CASE statement"));

  add_error(ER_FPARSER_TOO_BIG_FILE,
            N_("Configuration file '%-.192s' is too big"));

  add_error(ER_FPARSER_BAD_HEADER,
            N_("Malformed file type header in file '%-.192s'"));

  add_error(ER_FPARSER_EOF_IN_COMMENT,
            N_("Unexpected end of file while parsing comment '%-.200s'"));

  add_error(ER_FPARSER_ERROR_IN_PARAMETER,
            N_("Error while parsing parameter '%-.192s' (line: '%-.192s')"));

  add_error(ER_FPARSER_EOF_IN_UNKNOWN_PARAMETER,
            N_("Unexpected end of file while skipping unknown parameter '%-.192s'"));

  add_error(ER_VIEW_NO_EXPLAIN,
            N_("EXPLAIN/SHOW can not be issued; lacking privileges for underlying table"));

  add_error(ER_WRONG_OBJECT,
            N_("'%-.192s.%-.192s' is not %s"));

  add_error(ER_NONUPDATEABLE_COLUMN,
            N_("Column '%-.192s' is not updatable"));

  add_error(ER_VIEW_SELECT_DERIVED,
            N_("View's SELECT contains a subquery in the FROM clause"));

  add_error(ER_VIEW_SELECT_CLAUSE,
            N_("View's SELECT contains a '%s' clause"));

  add_error(ER_VIEW_SELECT_VARIABLE,
            N_("View's SELECT contains a variable or parameter"));

  add_error(ER_VIEW_SELECT_TMPTABLE,
            N_("View's SELECT refers to a temporary table '%-.192s'"));

  add_error(ER_VIEW_WRONG_LIST,
            N_("View's SELECT and view's field list have different column counts"));

  add_error(ER_WARN_VIEW_MERGE,
            N_("View merge algorithm can't be used here for now (assumed undefined algorithm)"));

  add_error(ER_WARN_VIEW_WITHOUT_KEY,
            N_("View being updated does not have complete key of underlying table in it"));

  add_error(ER_VIEW_INVALID,
            N_("View '%-.192s.%-.192s' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them"));

  add_error(ER_SP_NO_DROP_SP,
            N_("Can't drop or alter a %s from within another stored routine"));

  add_error(ER_SP_GOTO_IN_HNDLR,
            N_("GOTO is not allowed in a stored procedure handler"));

  add_error(ER_TRG_ALREADY_EXISTS,
            N_("Trigger already exists"));

  add_error(ER_TRG_DOES_NOT_EXIST,
            N_("Trigger does not exist"));

  add_error(ER_TRG_ON_VIEW_OR_TEMP_TABLE,
            N_("Trigger's '%-.192s' is view or temporary table"));

  add_error(ER_TRG_CANT_CHANGE_ROW,
            N_("Updating of %s row is not allowed in %strigger"));

  add_error(ER_TRG_NO_SUCH_ROW_IN_TRG,
            N_("There is no %s row in %s trigger"));

  add_error(ER_NO_DEFAULT_FOR_FIELD,
            N_("Field '%-.192s' doesn't have a default value"));

  add_error(ER_DIVISION_BY_ZERO, // 22012,
            N_("Division by 0"));

  add_error(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
            N_("Incorrect %-.32s value: '%-.128s' for column '%.192s' at row %u"));

  add_error(ER_ILLEGAL_VALUE_FOR_TYPE, // 22007,
            N_("Illegal %s '%-.192s' value found during parsing"));

  add_error(ER_VIEW_NONUPD_CHECK,
            N_("CHECK OPTION on non-updatable view '%-.192s.%-.192s'"));

  add_error(ER_VIEW_CHECK_FAILED,
            N_("CHECK OPTION failed '%-.192s.%-.192s'"));

  add_error(ER_PROCACCESS_DENIED_ERROR, // 42000,
            N_("%-.16s command denied to user '%-.48s'@'%-.64s' for routine '%-.192s'"));

  add_error(ER_RELAY_LOG_FAIL,
            N_("Failed purging old relay logs: %s"));

  add_error(ER_PASSWD_LENGTH,
            N_("Password hash should be a %d-digit hexadecimal number"));

  add_error(ER_UNKNOWN_TARGET_BINLOG,
            N_("Target log not found in binlog index"));

  add_error(ER_IO_ERR_LOG_INDEX_READ,
            N_("I/O error reading log index file"));

  add_error(ER_BINLOG_PURGE_PROHIBITED,
            N_("Server configuration does not permit binlog purge"));

  add_error(ER_FSEEK_FAIL,
            N_("Failed on fseek()"));

  add_error(ER_BINLOG_PURGE_FATAL_ERR,
            N_("Fatal error during log purge"));

  add_error(ER_LOG_IN_USE,
            N_("A purgeable log is in use, will not purge"));

  add_error(ER_LOG_PURGE_UNKNOWN_ERR,
            N_("Unknown error during log purge"));

  add_error(ER_RELAY_LOG_INIT,
            N_("Failed initializing relay log position: %s"));

  add_error(ER_NO_BINARY_LOGGING,
            N_("You are not using binary logging"));

  add_error(ER_RESERVED_SYNTAX,
            N_("The '%-.64s' syntax is reserved for purposes internal to the Drizzle server"));

  add_error(ER_WSAS_FAILED,
            N_("WSAStartup Failed"));

  add_error(ER_DIFF_GROUPS_PROC,
            N_("Can't handle procedures with different groups yet"));

  add_error(ER_NO_GROUP_FOR_PROC,
            N_("Select must have a group with this procedure"));

  add_error(ER_ORDER_WITH_PROC,
            N_("Can't use ORDER clause with this procedure"));

  add_error(ER_LOGGING_PROHIBIT_CHANGING_OF,
            N_("Binary logging and replication forbid changing the global server %s"));

  add_error(ER_NO_FILE_MAPPING,
            N_("Can't map file: %-.200s, errno: %d"));

  add_error(ER_WRONG_MAGIC,
            N_("Wrong magic in %-.64s"));

  add_error(ER_PS_MANY_PARAM,
            N_("Prepared statement contains too many placeholders"));

  add_error(ER_KEY_PART_0,
            N_("Key part '%-.192s' length cannot be 0"));

  add_error(ER_VIEW_CHECKSUM,
            N_("View text checksum failed"));

  add_error(ER_VIEW_MULTIUPDATE,
            N_("Can not modify more than one base table through a join view '%-.192s.%-.192s'"));

  add_error(ER_VIEW_NO_INSERT_FIELD_LIST,
            N_("Can not insert into join view '%-.192s.%-.192s' without fields list"));

  add_error(ER_VIEW_DELETE_MERGE_VIEW,
            N_("Can not delete from join view '%-.192s.%-.192s'"));

  add_error(ER_CANNOT_USER,
            N_("Operation %s failed for %.256s"));

  add_error(ER_XAER_NOTA, // XAE04,
            N_("XAER_NOTA: Unknown XID"));

  add_error(ER_XAER_INVAL, // XAE05,
            N_("XAER_INVAL: Invalid arguments (or unsupported command)"));

  add_error(ER_XAER_RMFAIL, // XAE07,
            N_("XAER_RMFAIL: The command cannot be executed when global transaction is in the  %.64s state"));

  add_error(ER_XAER_OUTSIDE, // XAE09,
            N_("XAER_OUTSIDE: Some work is done outside global transaction"));

  add_error(ER_XAER_RMERR, // XAE03,
            N_("XAER_RMERR: Fatal error occurred in the transaction branch - check your data for consistency"));

  add_error(ER_XA_RBROLLBACK, // XA100,
            N_("XA_RBROLLBACK: Transaction branch was rolled back"));

  add_error(ER_NONEXISTING_PROC_GRANT, // 42000,
            N_("There is no such grant defined for user '%-.48s' on host '%-.64s' on routine '%-.192s'"));

  add_error(ER_PROC_AUTO_GRANT_FAIL,
            N_("Failed to grant EXECUTE and ALTER ROUTINE privileges"));

  add_error(ER_PROC_AUTO_REVOKE_FAIL,
            N_("Failed to revoke all privileges to dropped routine"));

  add_error(ER_DATA_TOO_LONG, // 22001,
            N_("Data too long for column '%s' at row %ld"));

  add_error(ER_SP_BAD_SQLSTATE, // 42000,
            N_("Bad SQLSTATE: '%s'"));

  add_error(ER_STARTUP,
            N_("%s: ready for connections.\nVersion: '%s' %s\n"));

  add_error(ER_LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR,
            N_("Can't load value from file with fixed size rows to variable"));

  add_error(ER_CANT_CREATE_USER_WITH_GRANT, // 42000,
            N_("You are not allowed to create a user with GRANT"));

  add_error(ER_WRONG_VALUE_FOR_TYPE,
            N_("Incorrect %-.32s value: '%-.128s' for function %-.32s"));

  add_error(ER_TABLE_DEF_CHANGED,
            N_("Table definition has changed, please retry transaction"));

  add_error(ER_SP_DUP_HANDLER, // 42000,
            N_("Duplicate handler declared in the same block"));

  add_error(ER_SP_NOT_VAR_ARG, // 42000,
            N_("OUT or INOUT argument %d for routine %s is not a variable or NEW pseudo-variable in BEFORE trigger"));

  add_error(ER_SP_NO_RETSET, // 0A000,
            N_("Not allowed to return a result set from a %s"));

  add_error(ER_CANT_CREATE_GEOMETRY_OBJECT, // 22003,
            N_("Cannot get geometry object from data you send to the GEOMETRY field"));

  add_error(ER_FAILED_ROUTINE_BREAK_BINLOG,
            N_("A routine failed and has neither NO SQL nor READS SQL DATA in its declaration and binary logging is enabled; if non-transactional tables were updated, the binary log will miss their changes"));

  add_error(ER_BINLOG_UNSAFE_ROUTINE,
            N_("This function has none of DETERMINISTIC, NO SQL, or READS SQL DATA in its declaration and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)"));

  add_error(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER,
            N_("You do not have the SUPER privilege and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)"));

  add_error(ER_EXEC_STMT_WITH_OPEN_CURSOR,
            N_("You can't execute a prepared statement which has an open cursor associated with it. Reset the statement to re-execute it."));

  add_error(ER_STMT_HAS_NO_OPEN_CURSOR,
            N_("The statement (%lu) has no open cursor."));

  add_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG,
            N_("Explicit or implicit commit is not allowed in stored function or trigger."));

  add_error(ER_NO_DEFAULT_FOR_VIEW_FIELD,
            N_("Field of view '%-.192s.%-.192s' underlying table doesn't have a default value"));

  add_error(ER_SP_NO_RECURSION,
            N_("Recursive stored functions and triggers are not allowed."));

  add_error(ER_TOO_BIG_SCALE, // 42000 S1009,
            N_("Too big scale %d specified for column '%-.192s'. Maximum is %d."));

  add_error(ER_TOO_BIG_PRECISION, // 42000 S1009,
            N_("Too big precision %d specified for column '%-.192s'. Maximum is %d."));

  add_error(ER_M_BIGGER_THAN_D, // 42000 S1009,
            N_("For float(M,D), double(M,D) or decimal(M,D), M must be >= D (column '%-.192s')."));

  add_error(ER_WRONG_LOCK_OF_SYSTEM_TABLE,
            N_("You can't combine write-locking of system tables with other tables or lock types"));

  add_error(ER_CONNECT_TO_FOREIGN_DATA_SOURCE,
            N_("Unable to connect to foreign data source: %.64s"));

  add_error(ER_QUERY_ON_FOREIGN_DATA_SOURCE,
            N_("There was a problem processing the query on the foreign data source. Data source error: %-.64s"));

  add_error(ER_FOREIGN_DATA_SOURCE_DOESNT_EXIST,
            N_("The foreign data source you are trying to reference does not exist. Data source error:  %-.64s"));

  add_error(ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE,
            N_("Can't create federated table. The data source connection string '%-.64s' is not in the correct format"));

  add_error(ER_FOREIGN_DATA_STRING_INVALID,
            N_("The data source connection string '%-.64s' is not in the correct format"));

  add_error(ER_CANT_CREATE_FEDERATED_TABLE,
            N_("Can't create federated table. Foreign data src error:  %-.64s"));

  add_error(ER_TRG_IN_WRONG_SCHEMA,
            N_("Trigger in wrong schema"));

  add_error(ER_STACK_OVERRUN_NEED_MORE,
            N_("Thread stack overrun:  %ld bytes used of a %ld byte stack, and %ld bytes needed.  Use 'drizzled -O thread_stack=#' to specify a bigger stack."));

  add_error(ER_TOO_LONG_BODY, // 42000 S1009,
            N_("Routine body for '%-.100s' is too long"));

  add_error(ER_WARN_CANT_DROP_DEFAULT_KEYCACHE,
            N_("Cannot drop default keycache"));

  add_error(ER_TOO_BIG_DISPLAYWIDTH, // 42000 S1009,
            N_("Display width out of range for column '%-.192s' (max = %d)"));

  add_error(ER_XAER_DUPID, // XAE08,
            N_("XAER_DUPID: The XID already exists"));

  add_error(ER_DATETIME_FUNCTION_OVERFLOW, // 22008,
            N_("Datetime function: %-.32s field overflow"));

  add_error(ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG,
            N_("Can't update table '%-.192s' in stored function/trigger because it is already used by statement which invoked this stored function/trigger."));

  add_error(ER_VIEW_PREVENT_UPDATE,
            N_("The definition of table '%-.192s' prevents operation %.192s on table '%-.192s'."));

  add_error(ER_PS_NO_RECURSION,
            N_("The prepared statement contains a stored routine call that refers to that same statement. It's not allowed to execute a prepared statement in such a recursive manner"));

  add_error(ER_SP_CANT_SET_AUTOCOMMIT,
            N_("Not allowed to set autocommit from a stored function or trigger"));

  add_error(ER_MALFORMED_DEFINER,
            N_("Definer is not fully qualified"));

  add_error(ER_VIEW_FRM_NO_USER,
            N_("View '%-.192s'.'%-.192s' has no definer information (old table format). Current user is used as definer. Please recreate the view!"));

  add_error(ER_VIEW_OTHER_USER,
            N_("You need the SUPER privilege for creation view with '%-.192s'@'%-.192s' definer"));

  add_error(ER_NO_SUCH_USER,
            N_("The user specified as a definer ('%-.64s'@'%-.64s') does not exist"));

  add_error(ER_FORBID_SCHEMA_CHANGE,
            N_("Changing schema from '%-.192s' to '%-.192s' is not allowed."));

  add_error(ER_ROW_IS_REFERENCED_2, // 23000,
            N_("Cannot delete or update a parent row: a foreign key constraint fails (%.192s)"));

  add_error(ER_NO_REFERENCED_ROW_2, // 23000,
            N_("Cannot add or update a child row: a foreign key constraint fails (%.192s)"));

  add_error(ER_SP_BAD_VAR_SHADOW, // 42000,
            N_("Variable '%-.64s' must be quoted with `...`, or renamed"));

  add_error(ER_TRG_NO_DEFINER,
            N_("No definer attribute for trigger '%-.192s'.'%-.192s'. The trigger will be activated under the authorization of the invoker, which may have insufficient privileges. Please recreate the trigger."));

  add_error(ER_OLD_FILE_FORMAT,
            N_("'%-.192s' has an old format, you should re-create the '%s' object(s)"));

  add_error(ER_SP_RECURSION_LIMIT,
            N_("Recursive limit %d (as set by the max_sp_recursion_depth variable) was exceeded for routine %.192s"));

  add_error(ER_SP_PROC_TABLE_CORRUPT,
            N_("Failed to load routine %-.192s. The table drizzle.proc is missing, corrupt, or contains bad data (internal code %d)"));

  add_error(ER_SP_WRONG_NAME, // 42000,
            N_("Incorrect routine name '%-.192s'"));

  add_error(ER_TABLE_NEEDS_UPGRADE,
            N_("Table upgrade required. Please do \"REPAIR TABLE `%-.32s`\" to fix it!"));

  add_error(ER_SP_NO_AGGREGATE, // 42000,
            N_("AGGREGATE is not supported for stored functions"));

  add_error(ER_MAX_PREPARED_STMT_COUNT_REACHED, // 42000,
            N_("Can't create more than max_prepared_stmt_count statements (current value: %lu)"));

  add_error(ER_VIEW_RECURSIVE,
            N_("`%-.192s`.`%-.192s` contains view recursion"));

  add_error(ER_NON_GROUPING_FIELD_USED, // 42000,
            N_("non-grouping field '%-.192s' is used in %-.64s clause"));

  add_error(ER_TABLE_CANT_HANDLE_SPKEYS,
            N_("The used table type doesn't support SPATIAL indexes"));

  add_error(ER_NO_TRIGGERS_ON_SYSTEM_SCHEMA,
            N_("Triggers can not be created on system tables"));

  add_error(ER_REMOVED_SPACES,
            N_("Leading spaces are removed from name '%s'"));

  add_error(ER_AUTOINC_READ_FAILED,
            N_("Failed to read auto-increment value from storage engine"));

  add_error(ER_USERNAME,
            N_("user name"));

  add_error(ER_HOSTNAME,
            N_("host name"));

  add_error(ER_WRONG_STRING_LENGTH,
            N_("String '%-.70s' is too long for %s (should be no longer than %d)"));

  add_error(ER_NON_INSERTABLE_TABLE,
            N_("The target table %-.100s of the %s is not insertable-into"));

  add_error(ER_ADMIN_WRONG_MRG_TABLE,
            N_("Table '%-.64s' is differently defined or of non-MyISAM type or doesn't exist"));

  add_error(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT,
            N_("Too high level of nesting for select"));

  add_error(ER_NAME_BECOMES_EMPTY,
            N_("Name '%-.64s' has become ''"));

  add_error(ER_AMBIGUOUS_FIELD_TERM,
            N_("First character of the FIELDS TERMINATED string is ambiguous; please use non-optional and non-empty FIELDS ENCLOSED BY"));

  add_error(ER_FOREIGN_SERVER_EXISTS,
            N_("The foreign server, %s, you are trying to create already exists."));

  add_error(ER_FOREIGN_SERVER_DOESNT_EXIST,
            N_("The foreign server name you are trying to reference does not exist. Data source error:  %-.64s"));

  add_error(ER_ILLEGAL_HA_CREATE_OPTION,
            N_("Table storage engine '%-.64s' does not support the create option '%.64s'"));

  add_error(ER_PARTITION_REQUIRES_VALUES_ERROR,
            N_("Syntax error: %-.64s PARTITIONING requires definition of VALUES %-.64s for each partition"));

  add_error(ER_PARTITION_WRONG_VALUES_ERROR,
            N_("Only %-.64s PARTITIONING can use VALUES %-.64s in partition definition"));

  add_error(ER_PARTITION_MAXVALUE_ERROR,
            N_("MAXVALUE can only be used in last partition definition"));

  add_error(ER_PARTITION_SUBPARTITION_ERROR,
            N_("Subpartitions can only be hash partitions and by key"));

  add_error(ER_PARTITION_SUBPART_MIX_ERROR,
            N_("Must define subpartitions on all partitions if on one partition"));

  add_error(ER_PARTITION_WRONG_NO_PART_ERROR,
            N_("Wrong number of partitions defined, mismatch with previous setting"));

  add_error(ER_PARTITION_WRONG_NO_SUBPART_ERROR,
            N_("Wrong number of subpartitions defined, mismatch with previous setting"));

  add_error(ER_CONST_EXPR_IN_PARTITION_FUNC_ERROR,
            N_("Constant/Random expression in (sub)partitioning function is not allowed"));

  add_error(ER_NO_CONST_EXPR_IN_RANGE_OR_LIST_ERROR,
            N_("Expression in RANGE/LIST VALUES must be constant"));

  add_error(ER_FIELD_NOT_FOUND_PART_ERROR,
            N_("Field in list of fields for partition function not found in table"));

  add_error(ER_LIST_OF_FIELDS_ONLY_IN_HASH_ERROR,
            N_("List of fields is only allowed in KEY partitions"));

  add_error(ER_INCONSISTENT_PARTITION_INFO_ERROR,
            N_("The partition info in the frm file is not consistent with what can be written into the frm file"));

  add_error(ER_PARTITION_FUNC_NOT_ALLOWED_ERROR,
            N_("The %-.192s function returns the wrong type"));

  add_error(ER_PARTITIONS_MUST_BE_DEFINED_ERROR,
            N_("For %-.64s partitions each partition must be defined"));

  add_error(ER_RANGE_NOT_INCREASING_ERROR,
            N_("VALUES LESS THAN value must be strictly increasing for each partition"));

  add_error(ER_INCONSISTENT_TYPE_OF_FUNCTIONS_ERROR,
            N_("VALUES value must be of same type as partition function"));

  add_error(ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR,
            N_("Multiple definition of same constant in list partitioning"));

  add_error(ER_PARTITION_ENTRY_ERROR,
            N_("Partitioning can not be used stand-alone in query"));

  add_error(ER_MIX_HANDLER_ERROR,
            N_("The mix of handlers in the partitions is not allowed in this version of Drizzle"));

  add_error(ER_PARTITION_NOT_DEFINED_ERROR,
            N_("For the partitioned engine it is necessary to define all %-.64s"));

  add_error(ER_TOO_MANY_PARTITIONS_ERROR,
            N_("Too many partitions (including subpartitions) were defined"));

  add_error(ER_SUBPARTITION_ERROR,
            N_("It is only possible to mix RANGE/LIST partitioning with HASH/KEY partitioning for subpartitioning"));

  add_error(ER_CANT_CREATE_HANDLER_FILE,
            N_("Failed to create specific handler file"));

  add_error(ER_BLOB_FIELD_IN_PART_FUNC_ERROR,
            N_("A BLOB field is not allowed in partition function"));

  add_error(ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF,
            N_("A %-.192s must include all columns in the table's partitioning function"));

  add_error(ER_NO_PARTS_ERROR,
            N_("Number of %-.64s = 0 is not an allowed value"));

  add_error(ER_PARTITION_MGMT_ON_NONPARTITIONED,
            N_("Partition management on a not partitioned table is not possible"));

  add_error(ER_FOREIGN_KEY_ON_PARTITIONED,
            N_("Foreign key condition is not yet supported in conjunction with partitioning"));

  add_error(ER_DROP_PARTITION_NON_EXISTENT,
            N_("Error in list of partitions to %-.64s"));

  add_error(ER_DROP_LAST_PARTITION,
            N_("Cannot remove all partitions, use DROP TABLE instead"));

  add_error(ER_COALESCE_ONLY_ON_HASH_PARTITION,
            N_("COALESCE PARTITION can only be used on HASH/KEY partitions"));

  add_error(ER_REORG_HASH_ONLY_ON_SAME_NO,
            N_("REORGANIZE PARTITION can only be used to reorganize partitions not to change their numbers"));

  add_error(ER_REORG_NO_PARAM_ERROR,
            N_("REORGANIZE PARTITION without parameters can only be used on auto-partitioned tables using HASH PARTITIONs"));

  add_error(ER_ONLY_ON_RANGE_LIST_PARTITION,
            N_("%-.64s PARTITION can only be used on RANGE/LIST partitions"));

  add_error(ER_ADD_PARTITION_SUBPART_ERROR,
            N_("Trying to Add partition(s) with wrong number of subpartitions"));

  add_error(ER_ADD_PARTITION_NO_NEW_PARTITION,
            N_("At least one partition must be added"));

  add_error(ER_COALESCE_PARTITION_NO_PARTITION,
            N_("At least one partition must be coalesced"));

  add_error(ER_REORG_PARTITION_NOT_EXIST,
            N_("More partitions to reorganize than there are partitions"));

  add_error(ER_SAME_NAME_PARTITION,
            N_("Duplicate partition name %-.192s"));

  add_error(ER_NO_BINLOG_ERROR,
            N_("It is not allowed to shut off binlog on this command"));

  add_error(ER_CONSECUTIVE_REORG_PARTITIONS,
            N_("When reorganizing a set of partitions they must be in consecutive order"));

  add_error(ER_REORG_OUTSIDE_RANGE,
            N_("Reorganize of range partitions cannot change total ranges except for last partition where it can extend the range"));

  add_error(ER_PARTITION_FUNCTION_FAILURE,
            N_("Partition function not supported in this version for this handler"));

  add_error(ER_PART_STATE_ERROR,
            N_("Partition state cannot be defined from CREATE/ALTER TABLE"));

  add_error(ER_LIMITED_PART_RANGE,
            N_("The %-.64s handler only supports 32 bit integers in VALUES"));

  add_error(ER_PLUGIN_IS_NOT_LOADED,
            N_("Plugin '%-.192s' is not loaded"));

  add_error(ER_WRONG_VALUE,
            N_("Incorrect %-.32s value: '%-.128s'"));

  add_error(ER_NO_PARTITION_FOR_GIVEN_VALUE,
            N_("Table has no partition for value %-.64s"));

  add_error(ER_FILEGROUP_OPTION_ONLY_ONCE,
            N_("It is not allowed to specify %s more than once"));

  add_error(ER_CREATE_FILEGROUP_FAILED,
            N_("Failed to create %s"));

  add_error(ER_DROP_FILEGROUP_FAILED,
            N_("Failed to drop %s"));

  add_error(ER_TABLESPACE_AUTO_EXTEND_ERROR,
            N_("The handler doesn't support autoextend of tablespaces"));

  add_error(ER_WRONG_SIZE_NUMBER,
            N_("A size parameter was incorrectly specified, either number or on the form 10M"));

  add_error(ER_SIZE_OVERFLOW_ERROR,
            N_("The size number was correct but we don't allow the digit part to be more than 2 billion"));

  add_error(ER_ALTER_FILEGROUP_FAILED,
            N_("Failed to alter: %s"));

  add_error(ER_BINLOG_ROW_LOGGING_FAILED,
            N_("Writing one row to the row-based binary log failed"));

  add_error(ER_BINLOG_ROW_WRONG_TABLE_DEF,
            N_("Table definition on master and slave does not match: %s"));

  add_error(ER_BINLOG_ROW_RBR_TO_SBR,
            N_("Slave running with --log-slave-updates must use row-based binary logging to be able to replicate row-based binary log events"));

  add_error(ER_EVENT_ALREADY_EXISTS,
            N_("Event '%-.192s' already exists"));

  add_error(ER_EVENT_STORE_FAILED,
            N_("Failed to store event %s. Error code %d from storage engine."));

  add_error(ER_EVENT_DOES_NOT_EXIST,
            N_("Unknown event '%-.192s'"));

  add_error(ER_EVENT_CANT_ALTER,
            N_("Failed to alter event '%-.192s'"));

  add_error(ER_EVENT_DROP_FAILED,
            N_("Failed to drop %s"));

  add_error(ER_EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG,
            N_("INTERVAL is either not positive or too big"));

  add_error(ER_EVENT_ENDS_BEFORE_STARTS,
            N_("ENDS is either invalid or before STARTS"));

  add_error(ER_EVENT_EXEC_TIME_IN_THE_PAST,
            N_("Event execution time is in the past. Event has been disabled"));

  add_error(ER_EVENT_OPEN_TABLE_FAILED,
            N_("Failed to open drizzle.event"));

  add_error(ER_EVENT_NEITHER_M_EXPR_NOR_M_AT,
            N_("No datetime expression provided"));

  add_error(ER_COL_COUNT_DOESNT_MATCH_CORRUPTED,
            N_("Column count of drizzle.%s is wrong. Expected %d, found %d. The table is probably corrupted"));

  add_error(ER_CANNOT_LOAD_FROM_TABLE,
            N_("Cannot load from drizzle.%s. The table is probably corrupted"));

  add_error(ER_EVENT_CANNOT_DELETE,
            N_("Failed to delete the event from drizzle.event"));

  add_error(ER_EVENT_COMPILE_ERROR,
            N_("Error during compilation of event's body"));

  add_error(ER_EVENT_SAME_NAME,
            N_("Same old and new event name"));

  add_error(ER_EVENT_DATA_TOO_LONG,
            N_("Data for column '%s' too long"));

  add_error(ER_DROP_INDEX_FK,
            N_("Cannot drop index '%-.192s': needed in a foreign key constraint"));

  add_error(ER_WARN_DEPRECATED_SYNTAX_WITH_VER,
            N_("The syntax '%s' is deprecated and will be removed in Drizzle %s. Please use %s instead"));

  add_error(ER_CANT_WRITE_LOCK_LOG_TABLE,
            N_("You can't write-lock a log table. Only read access is possible"));

  add_error(ER_CANT_LOCK_LOG_TABLE,
            N_("You can't use locks with log tables."));

  add_error(ER_FOREIGN_DUPLICATE_KEY, // 23000 S1009,
            N_("Upholding foreign key constraints for table '%.192s', entry '%-.192s', key %d would lead to a duplicate entry"));

  add_error(ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE,
            N_("Column count of drizzle.%s is wrong. Expected %d, found %d. Created with Drizzle %d, now running %d. Please use drizzle_upgrade to fix this error."));

  add_error(ER_TEMP_TABLE_PREVENTS_SWITCH_OUT_OF_RBR,
            N_("Cannot switch out of the row-based binary log format when the session has open temporary tables"));

  add_error(ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_FORMAT,
            N_("Cannot change the binary logging format inside a stored function or trigger"));

  add_error(ER_NDB_CANT_SWITCH_BINLOG_FORMAT,
            N_("The NDB cluster engine does not support changing the binlog format on the fly yet"));

  add_error(ER_PARTITION_NO_TEMPORARY,
            N_("Cannot create temporary table with partitions"));

  add_error(ER_PARTITION_CONST_DOMAIN_ERROR,
            N_("Partition constant is out of partition function domain"));

  add_error(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED,
            N_("This partition function is not allowed"));

  add_error(ER_DDL_LOG_ERROR,
            N_("Error in DDL log"));

  add_error(ER_NULL_IN_VALUES_LESS_THAN,
            N_("Not allowed to use NULL value in VALUES LESS THAN"));

  add_error(ER_WRONG_PARTITION_NAME,
            N_("Incorrect partition name"));

  add_error(ER_CANT_CHANGE_TX_ISOLATION, // 25001,
            N_("Transaction isolation level can't be changed while a transaction is in progress"));

  add_error(ER_DUP_ENTRY_AUTOINCREMENT_CASE,
            N_("ALTER TABLE causes auto_increment resequencing, resulting in duplicate entry '%-.192s' for key '%-.192s'"));

  add_error(ER_EVENT_MODIFY_QUEUE_ERROR,
            N_("Internal scheduler error %d"));

  add_error(ER_EVENT_SET_VAR_ERROR,
            N_("Error during starting/stopping of the scheduler. Error code %u"));

  add_error(ER_PARTITION_MERGE_ERROR,
            N_("Engine cannot be used in partitioned tables"));

  add_error(ER_CANT_ACTIVATE_LOG,
            N_("Cannot activate '%-.64s' log"));

  add_error(ER_RBR_NOT_AVAILABLE,
            N_("The server was not built with row-based replication"));

  add_error(ER_BASE64_DECODE_ERROR,
            N_("Decoding of base64 string failed"));

  add_error(ER_EVENT_RECURSION_FORBIDDEN,
            N_("Recursion of EVENT DDL statements is forbidden when body is present"));

  add_error(ER_EVENTS_DB_ERROR,
            N_("Cannot proceed because system tables used by Event Scheduler were found damaged at server start"));

  add_error(ER_ONLY_INTEGERS_ALLOWED,
            N_("Only integers allowed as number here"));

  add_error(ER_UNSUPORTED_LOG_ENGINE,
            N_("This storage engine cannot be used for log tables"));

  add_error(ER_BAD_LOG_STATEMENT,
            N_("You cannot '%s' a log table if logging is enabled"));

  add_error(ER_CANT_RENAME_LOG_TABLE,
            N_("Cannot rename '%s'. When logging enabled, rename to/from log table must rename two tables: the log table to an archive table and another table back to '%s'"));

  add_error(ER_WRONG_PARAMCOUNT_TO_FUNCTION, // 42000,
            N_("Incorrect parameter count in the call to native function '%-.192s'"));

  add_error(ER_WRONG_PARAMETERS_TO_NATIVE_FCT, // 42000,
            N_("Incorrect parameters in the call to native function '%-.192s'"));

  add_error(ER_WRONG_PARAMETERS_TO_STORED_FCT, // 42000,
            N_("Incorrect parameters in the call to stored function '%-.192s'"));

  add_error(ER_NATIVE_FCT_NAME_COLLISION,
            N_("This function '%-.192s' has the same name as a native function"));

  add_error(ER_DUP_ENTRY_WITH_KEY_NAME, // 23000 S1009,
            N_("Duplicate entry '%-.64s' for key '%-.192s'"));

  add_error(ER_BINLOG_PURGE_EMFILE,
            N_("Too many files opened, please execute the command again"));

  add_error(ER_EVENT_CANNOT_CREATE_IN_THE_PAST,
            N_("Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation."));

  add_error(ER_EVENT_CANNOT_ALTER_IN_THE_PAST,
            N_("Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation."));

  add_error(ER_SLAVE_INCIDENT,
            N_("The incident %s occurred on the master. Message: %-.64s"));

  add_error(ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT,
            N_("Table has no partition for some existing values"));

  add_error(ER_BINLOG_UNSAFE_STATEMENT,
            N_("Statement is not safe to log in statement format."));

  add_error(ER_SLAVE_FATAL_ERROR,
            N_("Fatal error: %s"));

  add_error(ER_SLAVE_RELAY_LOG_READ_FAILURE,
            N_("Relay log read failure: %s"));

  add_error(ER_SLAVE_RELAY_LOG_WRITE_FAILURE,
            N_("Relay log write failure: %s"));

  add_error(ER_SLAVE_CREATE_EVENT_FAILURE,
            N_("Failed to create %s"));

  add_error(ER_SLAVE_MASTER_COM_FAILURE,
            N_("Master command %s failed: %s"));

  add_error(ER_BINLOG_LOGGING_IMPOSSIBLE,
            N_("Binary logging not possible. Message: %s"));

  add_error(ER_VIEW_NO_CREATION_CTX,
            N_("View `%-.64s`.`%-.64s` has no creation context"));

  add_error(ER_VIEW_INVALID_CREATION_CTX,
            N_("Creation context of view `%-.64s`.`%-.64s' is invalid"));

  add_error(ER_SR_INVALID_CREATION_CTX,
            N_("Creation context of stored routine `%-.64s`.`%-.64s` is invalid"));

  add_error(ER_TRG_CORRUPTED_FILE,
            N_("Corrupted TRG file for table `%-.64s`.`%-.64s`"));

  add_error(ER_TRG_NO_CREATION_CTX,
            N_("Triggers for table `%-.64s`.`%-.64s` have no creation context"));

  add_error(ER_TRG_INVALID_CREATION_CTX,
            N_("Trigger creation context of table `%-.64s`.`%-.64s` is invalid"));

  add_error(ER_EVENT_INVALID_CREATION_CTX,
            N_("Creation context of event `%-.64s`.`%-.64s` is invalid"));

  add_error(ER_TRG_CANT_OPEN_TABLE,
            N_("Cannot open table for trigger `%-.64s`.`%-.64s`"));

  add_error(ER_CANT_CREATE_SROUTINE,
            N_("Cannot create stored routine `%-.64s`. Check warnings"));

  add_error(ER_SLAVE_AMBIGOUS_EXEC_MODE,
            N_("Ambiguous slave modes combination. %s"));

  add_error(ER_NO_FORMAT_DESCRIPTION_EVENT_BEFORE_BINLOG_STATEMENT,
            N_("The BINLOG statement of type `%s` was not preceded by a format description BINLOG statement."));

  add_error(ER_SLAVE_CORRUPT_EVENT,
            N_("Corrupted replication event was detected"));

  add_error(ER_LOAD_DATA_INVALID_COLUMN,
            N_("Invalid column reference (%-.64s) in LOAD DATA"));

  add_error(ER_LOG_PURGE_NO_FILE,
            N_("Being purged log %s was not found"));

  add_error(ER_WARN_AUTO_CONVERT_LOCK,
            N_("Converted to non-transactional lock on '%-.64s'"));

  add_error(ER_NO_AUTO_CONVERT_LOCK_STRICT,
            N_("Cannot convert to non-transactional lock in strict mode on '%-.64s'"));

  add_error(ER_NO_AUTO_CONVERT_LOCK_TRANSACTION,
            N_("Cannot convert to non-transactional lock in an active transaction on '%-.64s'"));

  add_error(ER_NO_STORAGE_ENGINE,
            N_("Can't access storage engine of table %-.64s"));

  add_error(ER_BACKUP_BACKUP_START,
            N_("Starting backup process"));

  add_error(ER_BACKUP_BACKUP_DONE,
            N_("Backup completed"));

  add_error(ER_BACKUP_RESTORE_START,
            N_("Starting restore process"));

  add_error(ER_BACKUP_RESTORE_DONE,
            N_("Restore completed"));

  add_error(ER_BACKUP_NOTHING_TO_BACKUP,
            N_("Nothing to backup"));

  add_error(ER_BACKUP_CANNOT_INCLUDE_DB,
            N_("Database '%-.64s' cannot be included in a backup"));

  add_error(ER_BACKUP_BACKUP,
            N_("Error during backup operation - server's error log contains more information about the error"));

  add_error(ER_BACKUP_RESTORE,
            N_("Error during restore operation - server's error log contains more information about the error"));

  add_error(ER_BACKUP_RUNNING,
            N_("Can't execute this command because another BACKUP/RESTORE operation is in progress"));

  add_error(ER_BACKUP_BACKUP_PREPARE,
            N_("Error when preparing for backup operation"));

  add_error(ER_BACKUP_RESTORE_PREPARE,
            N_("Error when preparing for restore operation"));

  add_error(ER_BACKUP_INVALID_LOC,
            N_("Invalid backup location '%-.64s'"));

  add_error(ER_BACKUP_READ_LOC,
            N_("Can't read backup location '%-.64s'"));

  add_error(ER_BACKUP_WRITE_LOC,
            N_("Can't write to backup location '%-.64s' (file already exists?)"));

  add_error(ER_BACKUP_LIST_DBS,
            N_("Can't enumerate server databases"));

  add_error(ER_BACKUP_LIST_TABLES,
            N_("Can't enumerate server tables"));

  add_error(ER_BACKUP_LIST_DB_TABLES,
            N_("Can't enumerate tables in database %-.64s"));

  add_error(ER_BACKUP_SKIP_VIEW,
            N_("Skipping view %-.64s in database %-.64s"));

  add_error(ER_BACKUP_NO_ENGINE,
            N_("Skipping table %-.64s since it has no valid storage engine"));

  add_error(ER_BACKUP_TABLE_OPEN,
            N_("Can't open table %-.64s"));

  add_error(ER_BACKUP_READ_HEADER,
            N_("Can't read backup archive preamble"));

  add_error(ER_BACKUP_WRITE_HEADER,
            N_("Can't write backup archive preamble"));

  add_error(ER_BACKUP_NO_BACKUP_DRIVER,
            N_("Can't find backup driver for table %-.64s"));

  add_error(ER_BACKUP_NOT_ACCEPTED,
            N_("%-.64s backup driver was selected for table %-.64s but it rejects to handle this table"));

  add_error(ER_BACKUP_CREATE_BACKUP_DRIVER,
            N_("Can't create %-.64s backup driver"));

  add_error(ER_BACKUP_CREATE_RESTORE_DRIVER,
            N_("Can't create %-.64s restore driver"));

  add_error(ER_BACKUP_TOO_MANY_IMAGES,
            N_("Found %d images in backup archive but maximum %d are supported"));

  add_error(ER_BACKUP_WRITE_META,
            N_("Error when saving meta-data of %-.64s"));

  add_error(ER_BACKUP_READ_META,
            N_("Error when reading meta-data list"));

  add_error(ER_BACKUP_CREATE_META,
            N_("Can't create %-.64s"));

  add_error(ER_BACKUP_GET_BUF,
            N_("Can't allocate buffer for image data transfer"));

  add_error(ER_BACKUP_WRITE_DATA,
            N_("Error when writing %-.64s backup image data (for table #%d)"));

  add_error(ER_BACKUP_READ_DATA,
            N_("Error when reading data from backup stream"));

  add_error(ER_BACKUP_NEXT_CHUNK,
            N_("Can't go to the next chunk in backup stream"));

  add_error(ER_BACKUP_INIT_BACKUP_DRIVER,
            N_("Can't initialize %-.64s backup driver"));

  add_error(ER_BACKUP_INIT_RESTORE_DRIVER,
            N_("Can't initialize %-.64s restore driver"));

  add_error(ER_BACKUP_STOP_BACKUP_DRIVER,
            N_("Can't shut down %-.64s backup driver"));

  add_error(ER_BACKUP_STOP_RESTORE_DRIVERS,
            N_("Can't shut down %-.64s backup driver(s)"));

  add_error(ER_BACKUP_PREPARE_DRIVER,
            N_("%-.64s backup driver can't prepare for synchronization"));

  add_error(ER_BACKUP_CREATE_VP,
            N_("%-.64s backup driver can't create its image validity point"));

  add_error(ER_BACKUP_UNLOCK_DRIVER,
            N_("Can't unlock %-.64s backup driver after creating the validity point"));

  add_error(ER_BACKUP_CANCEL_BACKUP,
            N_("%-.64s backup driver can't cancel its backup operation"));

  add_error(ER_BACKUP_CANCEL_RESTORE,
            N_("%-.64s restore driver can't cancel its restore operation"));

  add_error(ER_BACKUP_GET_DATA,
            N_("Error when polling %-.64s backup driver for its image data"));

  add_error(ER_BACKUP_SEND_DATA,
            N_("Error when sending image data (for table #%d) to %-.64s restore driver"));

  add_error(ER_BACKUP_SEND_DATA_RETRY,
            N_("After %d attempts %-.64s restore driver still can't accept next block of data"));

  add_error(ER_BACKUP_OPEN_TABLES,
            N_("Open and lock tables failed in %-.64s"));

  add_error(ER_BACKUP_THREAD_INIT,
            N_("Backup driver's table locking thread can not be initialized."));

  add_error(ER_BACKUP_PROGRESS_TABLES,
            N_("Can't open the online backup progress tables. Check 'drizzle.online_backup' and 'drizzle.online_backup_progress'."));

  add_error(ER_TABLESPACE_EXIST,
            N_("Tablespace '%-.192s' already exists"));

  add_error(ER_NO_SUCH_TABLESPACE,
            N_("Tablespace '%-.192s' doesn't exist"));

  add_error(ER_SLAVE_HEARTBEAT_FAILURE,
            N_("Unexpected master's heartbeat data: %s"));

  add_error(ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE,
            N_("The requested value for the heartbeat period %s %s"));

  add_error(ER_BACKUP_LOG_WRITE_ERROR,
            N_("Can't write to the online backup progress log %-.64s."));

  add_error(ER_TABLESPACE_NOT_EMPTY,
            N_("Tablespace '%-.192s' not empty"));

  add_error(ER_BACKUP_TS_CHANGE,
            N_("Tablespace `%-.64s` needed by tables being restored has changed on the server. The original definition of the required tablespace is '%-.256s' while the same tablespace is defined on the server as '%-.256s'"));

  add_error(ER_VCOL_BASED_ON_VCOL,
            N_("A virtual column cannot be based on a virtual column"));

  add_error(ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED,
            N_("Non-deterministic expression for virtual column '%s'."));

  add_error(ER_DATA_CONVERSION_ERROR_FOR_VIRTUAL_COLUMN,
            N_("Generated value for virtual column '%s' cannot be converted to type '%s'."));

  add_error(ER_PRIMARY_KEY_BASED_ON_VIRTUAL_COLUMN,
            N_("Primary key cannot be defined upon a virtual column."));

  add_error(ER_KEY_BASED_ON_GENERATED_VIRTUAL_COLUMN,
            N_("Key/Index cannot be defined on a non-stored virtual column."));

  add_error(ER_WRONG_FK_OPTION_FOR_VIRTUAL_COLUMN,
            N_("Cannot define foreign key with %s clause on a virtual column."));

  add_error(ER_WARNING_NON_DEFAULT_VALUE_FOR_VIRTUAL_COLUMN,
            N_("The value specified for virtual column '%s' in table '%s' ignored."));

  add_error(ER_UNSUPPORTED_ACTION_ON_VIRTUAL_COLUMN,
            N_("'%s' is not yet supported for virtual columns."));

  add_error(ER_CONST_EXPR_IN_VCOL,
            N_("Constant expression in virtual column function is not allowed."));

  add_error(ER_UNKNOWN_TEMPORAL_TYPE,
            N_("Encountered an unknown temporal type."));

  add_error(ER_INVALID_STRING_FORMAT_FOR_DATE,
            N_("Received an invalid string format '%s' for a date value."));

  add_error(ER_INVALID_STRING_FORMAT_FOR_TIME,
            N_("Received an invalid string format '%s' for a time value."));

  add_error(ER_INVALID_UNIX_TIMESTAMP_VALUE,
            N_("Received an invalid value '%s' for a UNIX timestamp."));

  add_error(ER_INVALID_DATETIME_VALUE,
            N_("Received an invalid datetime value '%s'."));

  add_error(ER_INVALID_NULL_ARGUMENT,
            N_("Received a NULL argument for function '%s'."));

  add_error(ER_INVALID_NEGATIVE_ARGUMENT,
            N_("Received an invalid negative argument '%s' for function '%s'."));

  add_error(ER_ARGUMENT_OUT_OF_RANGE,
            N_("Received an out-of-range argument '%s' for function '%s'."));

  add_error(ER_INVALID_TIME_VALUE,
            N_("Received an invalid time value '%s'."));

  add_error(ER_INVALID_ENUM_VALUE,
            N_("Received an invalid enum value '%s'."));

  add_error(ER_NO_PRIMARY_KEY_ON_REPLICATED_TABLE,
            N_("Tables which are replicated require a primary key."));

}

}

}


const char * error_message(unsigned int code)
{
  /**
   if the connection is killed, code is 2
   See lp bug# 435619
   */
  if ((code > ER_ERROR_FIRST) )
   {
     return drizzled_error_messages[code-ER_ERROR_FIRST];
   }
  else
    return drizzled_error_messages[ER_UNKNOWN_ERROR - ER_ERROR_FIRST];
}



/**
  Read messages from errorfile.

  This function can be called multiple times to reload the messages.
  If it fails to load the messages, it will fail softly by initializing
  the errmesg pointer to an array of empty strings or by keeping the
  old array if it exists.

  @retval
    FALSE       OK
  @retval
    TRUE        Error
*/


static void init_myfunc_errs()
{
  init_glob_errors();                     /* Initiate english errors */
  init_drizzle_errors();

  {
    EE(EE_FILENOTFOUND)   = ER(ER_FILE_NOT_FOUND);
    EE(EE_CANTCREATEFILE) = ER(ER_CANT_CREATE_FILE);
    EE(EE_READ)           = ER(ER_ERROR_ON_READ);
    EE(EE_WRITE)          = ER(ER_ERROR_ON_WRITE);
    EE(EE_BADCLOSE)       = ER(ER_ERROR_ON_CLOSE);
    EE(EE_OUTOFMEMORY)    = ER(ER_OUTOFMEMORY);
    EE(EE_DELETE)         = ER(ER_CANT_DELETE_FILE);
    EE(EE_LINK)           = ER(ER_ERROR_ON_RENAME);
    EE(EE_EOFERR)         = ER(ER_UNEXPECTED_EOF);
    EE(EE_CANTLOCK)       = ER(ER_CANT_LOCK);
    EE(EE_DIR)            = ER(ER_CANT_READ_DIR);
    EE(EE_STAT)           = ER(ER_CANT_GET_STAT);
    EE(EE_GETWD)          = ER(ER_CANT_GET_WD);
    EE(EE_SETWD)          = ER(ER_CANT_SET_WD);
    EE(EE_DISK_FULL)      = ER(ER_DISK_FULL);
  }
}

bool init_errmessage(void)
{

  /* Register messages for use with my_error(). */
  if (my_error_register(drizzled_error_messages,
                        ER_ERROR_FIRST, ER_ERROR_LAST))
  {
    return(true);
  }

  init_myfunc_errs();                   /* Init myfunc messages */
  return(false);
}

// -- From here has just been moved from my_error.cc

/* Error message numbers in global map */
const char * globerrs[GLOBERRS];

error_handler_func error_handler_hook= NULL;


void init_glob_errors()
{
  add_error(EE_CANTCREATEFILE,
            N_("Can't create/write to file '%s' (Errcode: %d)"));

  add_error(EE_READ,
            N_("Error reading file '%s' (Errcode: %d)"));

  add_error(EE_WRITE,
            N_("Error writing file '%s' (Errcode: %d)"));

  add_error(EE_BADCLOSE,
            N_("Error on close of '%s' (Errcode: %d)"));

  add_error(EE_OUTOFMEMORY,
            N_("Out of memory (Needed %u bytes)"));

  add_error(EE_DELETE,
            N_("Error on delete of '%s' (Errcode: %d)"));

  add_error(EE_LINK,
            N_("Error on rename of '%s' to '%s' (Errcode: %d)"));

  add_error(EE_EOFERR,
            N_("Unexpected eof found when reading file '%s' (Errcode: %d)"));

  add_error(EE_CANTLOCK,
            N_("Can't lock file (Errcode: %d)"));

  add_error(EE_CANTUNLOCK,
            N_("Can't unlock file (Errcode: %d)"));

  add_error(EE_DIR,
            N_("Can't read dir of '%s' (Errcode: %d)"));

  add_error(EE_STAT,
            N_("Can't get stat of '%s' (Errcode: %d)"));

  add_error(EE_CANT_CHSIZE,
            N_("Can't change size of file (Errcode: %d)"));

  add_error(EE_CANT_OPEN_STREAM,
            N_("Can't open stream from handle (Errcode: %d)"));

  add_error(EE_GETWD,
            N_("Can't get working dirctory (Errcode: %d)"));

  add_error(EE_SETWD,
            N_("Can't change dir to '%s' (Errcode: %d)"));

  add_error(EE_LINK_WARNING,
            N_("Warning: '%s' had %d links"));

  add_error(EE_OPEN_WARNING,
            N_("Warning: %d files and %d streams is left open\n"));

  add_error(EE_DISK_FULL,
            N_("Disk is full writing '%s'. "
               "Waiting for someone to free space..."));

  add_error(EE_CANT_MKDIR,
            N_("Can't create directory '%s' (Errcode: %d)"));

  add_error(EE_UNKNOWN_CHARSET,
            N_("Character set '%s' is not a compiled character set "
               "and is not specified in the %s file"));

  add_error(EE_OUT_OF_FILERESOURCES,
            N_("Out of resources when opening file '%s' (Errcode: %d)"));

  add_error(EE_CANT_READLINK,
            N_("Can't read value for symlink '%s' (Error %d)"));

  add_error(EE_CANT_SYMLINK,
            N_("Can't create symlink '%s' pointing at '%s' (Error %d)"));

  add_error(EE_REALPATH,
            N_("Error on realpath() on '%s' (Error %d)"));

  add_error(EE_SYNC,
            N_("Can't sync file '%s' to disk (Errcode: %d)"));

  add_error(EE_UNKNOWN_COLLATION,
            N_("Collation '%s' is not a compiled collation "
               "and is not specified in the %s file"));

  add_error(EE_FILENOTFOUND,
            N_("File '%s' not found (Errcode: %d)"));

  add_error(EE_FILE_NOT_CLOSED,
            N_("File '%s' (fileno: %d) was not closed"));
}

/*
  WARNING!
  my_error family functions have to be used according following rules:
  - if message have not parameters use my_message(ER_CODE, ER(ER_CODE), MYF(N))
  - if message registered use my_error(ER_CODE, MYF(N), ...).
  - With some special text of errror message use:
  my_printf_error(ER_CODE, format, MYF(N), ...)
*/

/*
  Message texts are registered into a linked list of 'my_err_head' structs.
  Each struct contains (1.) an array of pointers to C character strings with
  '\0' termination, (2.) the error number for the first message in the array
  (array index 0) and (3.) the error number for the last message in the array
  (array index (last - first)).
  The array may contain gaps with NULL pointers and pointers to empty strings.
  Both kinds of gaps will be translated to "Unknown error %d.", if my_error()
  is called with a respective error number.
  The list of header structs is sorted in increasing order of error numbers.
  Negative error numbers are allowed. Overlap of error numbers is not allowed.
  Not registered error numbers will be translated to "Unknown error %d.".
*/
static struct my_err_head
{
  struct my_err_head    *meh_next;      /* chain link */
  const char            **meh_errmsgs;  /* error messages array */
  int                   meh_first;      /* error number matching array slot 0 */
  int                   meh_last;       /* error number matching last slot */
  bool			is_globerrs;
} my_errmsgs_globerrs = {NULL, globerrs, EE_ERROR_FIRST, EE_ERROR_LAST, true};

static struct my_err_head *my_errmsgs_list= &my_errmsgs_globerrs;


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
  const char *format;
  struct my_err_head *meh_p;
  va_list args;
  char ebuff[ERRMSGSIZE + 20];

  /* Search for the error messages array, which could contain the message. */
  for (meh_p= my_errmsgs_list; meh_p; meh_p= meh_p->meh_next)
    if (nr <= meh_p->meh_last)
      break;

  /* get the error message string. Default, if NULL or empty string (""). */
  if (! (format= (meh_p && (nr >= meh_p->meh_first)) ?
         _(meh_p->meh_errmsgs[nr - meh_p->meh_first]) : NULL) || ! *format)
    (void) snprintf (ebuff, sizeof(ebuff), _("Unknown error %d"), nr);
  else
  {
    va_start(args,MyFlags);
    (void) vsnprintf (ebuff, sizeof(ebuff), format, args);
    va_end(args);
  }
  (*error_handler_hook)(nr, ebuff, MyFlags);
  return;
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


/*
  Register error messages for use with my_error().

  SYNOPSIS
    my_error_register()
    errmsgs                     array of pointers to error messages
    first                       error number of first message in the array
    last                        error number of last message in the array

  DESCRIPTION
    The pointer array is expected to contain addresses to NUL-terminated
    C character strings. The array contains (last - first + 1) pointers.
    NULL pointers and empty strings ("") are allowed. These will be mapped to
    "Unknown error" when my_error() is called with a matching error number.
    This function registers the error numbers 'first' to 'last'.
    No overlapping with previously registered error numbers is allowed.

  RETURN
    0           OK
    != 0        Error
*/

int my_error_register(const char **errmsgs, int first, int last)
{
  struct my_err_head *meh_p;
  struct my_err_head **search_meh_pp;

  /* Allocate a new header structure. */
  if (! (meh_p= (struct my_err_head*) malloc(sizeof(struct my_err_head))))
    return 1;
  meh_p->meh_errmsgs= errmsgs;
  meh_p->meh_first= first;
  meh_p->meh_last= last;
  meh_p->is_globerrs= false;

  /* Search for the right position in the list. */
  for (search_meh_pp= &my_errmsgs_list;
       *search_meh_pp;
       search_meh_pp= &(*search_meh_pp)->meh_next)
  {
    if ((*search_meh_pp)->meh_last > first)
      break;
  }

  /* Error numbers must be unique. No overlapping is allowed. */
  if (*search_meh_pp && ((*search_meh_pp)->meh_first <= last))
  {
    free((unsigned char*)meh_p);
    return 1;
  }

  /* Insert header into the chain. */
  meh_p->meh_next= *search_meh_pp;
  *search_meh_pp= meh_p;
  return 0;
}


/*
  Unregister formerly registered error messages.

  SYNOPSIS
    my_error_unregister()
    first                       error number of first message
    last                        error number of last message

  DESCRIPTION
    This function unregisters the error numbers 'first' to 'last'.
    These must have been previously registered by my_error_register().
    'first' and 'last' must exactly match the registration.
    If a matching registration is present, the header is removed from the
    list and the pointer to the error messages pointers array is returned.
    Otherwise, NULL is returned.

  RETURN
    non-NULL    OK, returns address of error messages pointers array.
    NULL        Error, no such number range registered.
*/

const char **my_error_unregister(int first, int last)
{
  struct my_err_head    *meh_p;
  struct my_err_head    **search_meh_pp;
  const char            **errmsgs;

  /* Search for the registration in the list. */
  for (search_meh_pp= &my_errmsgs_list;
       *search_meh_pp;
       search_meh_pp= &(*search_meh_pp)->meh_next)
  {
    if (((*search_meh_pp)->meh_first == first) &&
        ((*search_meh_pp)->meh_last == last))
      break;
  }
  if (! *search_meh_pp)
    return NULL;

  /* Remove header from the chain. */
  meh_p= *search_meh_pp;
  *search_meh_pp= meh_p->meh_next;

  /* Save the return value and free the header. */
  errmsgs= meh_p->meh_errmsgs;
  bool is_globerrs= meh_p->is_globerrs;

  free((unsigned char*) meh_p);

  if (is_globerrs)
    return NULL;

  return errmsgs;
}


void my_error_unregister_all(void)
{
  struct my_err_head    *list, *next;
  for (list= my_errmsgs_globerrs.meh_next; list; list= next)
  {
    next= list->meh_next;
    free((unsigned char*) list);
  }
  my_errmsgs_list= &my_errmsgs_globerrs;
}
