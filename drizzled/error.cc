/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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

#include <drizzled/global.h>
#include <mysys/my_sys.h>
#include <mysys/mysys_err.h>
#include <drizzled/definitions.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>

static const char *drizzled_error_messages[]=
{
/* ER_HASHCHK   */
N_("hashchk"),
/* ER_NISAMCHK   */
N_("isamchk"),
/* ER_NO   */
N_("NO"),
/* ER_YES   */
N_("YES"),
/* ER_CANT_CREATE_FILE   */
N_("Can't create file '%-.200s' (errno: %d)"),
/* ER_CANT_CREATE_TABLE   */
N_("Can't create table '%-.200s' (errno: %d)"),
/* ER_CANT_CREATE_DB   */
N_("Can't create database '%-.192s' (errno: %d)"),
/* ER_DB_CREATE_EXISTS   */
N_("Can't create database '%-.192s'; database exists"),
/* ER_DB_DROP_EXISTS   */
N_("Can't drop database '%-.192s'; database doesn't exist"),
/* ER_DB_DROP_DELETE   */
N_("Error dropping database (can't delete '%-.192s', errno: %d)"),
/* ER_DB_DROP_RMDIR   */
N_("Error dropping database (can't rmdir '%-.192s', errno: %d)"),
/* ER_CANT_DELETE_FILE   */
N_("Error on delete of '%-.192s' (errno: %d)"),
/* ER_CANT_FIND_SYSTEM_REC   */
N_("Can't read record in system table"),
/* ER_CANT_GET_STAT   */
N_("Can't get status of '%-.200s' (errno: %d)"),
/* ER_CANT_GET_WD   */
N_("Can't get working directory (errno: %d)"),
/* ER_CANT_LOCK   */
N_("Can't lock file (errno: %d)"),
/* ER_CANT_OPEN_FILE   */
N_("Can't open file: '%-.200s' (errno: %d)"),
/* ER_FILE_NOT_FOUND   */
N_("Can't find file: '%-.200s' (errno: %d)"),
/* ER_CANT_READ_DIR   */
N_("Can't read dir of '%-.192s' (errno: %d)"),
/* ER_CANT_SET_WD   */
N_("Can't change dir to '%-.192s' (errno: %d)"),
/* ER_CHECKREAD   */
N_("Record has changed since last read in table '%-.192s'"),
/* ER_DISK_FULL   */
N_("Disk full (%s); waiting for someone to free some space..."),
/* ER_DUP_KEY 23000  */
N_("Can't write; duplicate key in table '%-.192s'"),
/* ER_ERROR_ON_CLOSE   */
N_("Error on close of '%-.192s' (errno: %d)"),
/* ER_ERROR_ON_READ   */
N_("Error reading file '%-.200s' (errno: %d)"),
/* ER_ERROR_ON_RENAME   */
N_("Error on rename of '%-.150s' to '%-.150s' (errno: %d)"),
/* ER_ERROR_ON_WRITE   */
N_("Error writing file '%-.200s' (errno: %d)"),
/* ER_FILE_USED   */
N_("'%-.192s' is locked against change"),
/* ER_FILSORT_ABORT   */
N_("Sort aborted"),
/* ER_FORM_NOT_FOUND   */
N_("View '%-.192s' doesn't exist for '%-.192s'"),
/* ER_GET_ERRNO   */
N_("Got error %d from storage engine"),
/* ER_ILLEGAL_HA   */
N_("Table storage engine for '%-.192s' doesn't have this option"),
/* ER_KEY_NOT_FOUND   */
N_("Can't find record in '%-.192s'"),
/* ER_NOT_FORM_FILE   */
N_("Incorrect information in file: '%-.200s'"),
/* ER_NOT_KEYFILE   */
N_("Incorrect key file for table '%-.200s'; try to repair it"),
/* ER_OLD_KEYFILE   */
N_("Old key file for table '%-.192s'; repair it!"),
/* ER_OPEN_AS_READONLY   */
N_("Table '%-.192s' is read only"),
/* ER_OUTOFMEMORY HY001 S1001 */
N_("Out of memory; restart server and try again (needed %lu bytes)"),
/* ER_OUT_OF_SORTMEMORY HY001 S1001 */
N_("Out of sort memory; increase server sort buffer size"),
/* ER_UNEXPECTED_EOF   */
N_("Unexpected EOF found when reading file '%-.192s' (errno: %d)"),
/* ER_CON_COUNT_ERROR 08004  */
N_("Too many connections"),
/* ER_OUT_OF_RESOURCES   */
N_("Out of memory; check if drizzled or some other process uses all available memory; if not, you may have to use 'ulimit' to allow drizzled to use more memory or you can add more swap space"),
/* ER_BAD_HOST_ERROR 08S01  */
N_("Can't get hostname for your address"),
/* ER_HANDSHAKE_ERROR 08S01  */
N_("Bad handshake"),
/* ER_DBACCESS_DENIED_ERROR 42000  */
N_("Access denied for user '%-.48s'@'%-.64s' to database '%-.192s'"),
/* ER_ACCESS_DENIED_ERROR 28000  */
N_("Access denied for user '%-.48s'@'%-.64s' (using password: %s)"),
/* ER_NO_DB_ERROR 3D000  */
N_("No database selected"),
/* ER_UNKNOWN_COM_ERROR 08S01  */
N_("Unknown command"),
/* ER_BAD_NULL_ERROR 23000  */
N_("Column '%-.192s' cannot be null"),
/* ER_BAD_DB_ERROR 42000  */
N_("Unknown database '%-.192s'"),
/* ER_TABLE_EXISTS_ERROR 42S01  */
N_("Table '%-.192s' already exists"),
/* ER_BAD_TABLE_ERROR 42S02  */
N_("Unknown table '%-.100s'"),
/* ER_NON_UNIQ_ERROR 23000  */
N_("Column '%-.192s' in %-.192s is ambiguous"),
/* ER_SERVER_SHUTDOWN 08S01  */
N_("Server shutdown in progress"),
/* ER_BAD_FIELD_ERROR 42S22 S0022 */
N_("Unknown column '%-.192s' in '%-.192s'"),
/* ER_WRONG_FIELD_WITH_GROUP 42000 S1009 */
N_("'%-.192s' isn't in GROUP BY"),
/* ER_WRONG_GROUP_FIELD 42000 S1009 */
N_("Can't group on '%-.192s'"),
/* ER_WRONG_SUM_SELECT 42000 S1009 */
N_("Statement has sum functions and columns in same statement"),
/* ER_WRONG_VALUE_COUNT 21S01  */
N_("Column count doesn't match value count"),
/* ER_TOO_LONG_IDENT 42000 S1009 */
N_("Identifier name '%-.100s' is too long"),
/* ER_DUP_FIELDNAME 42S21 S1009 */
N_("Duplicate column name '%-.192s'"),
/* ER_DUP_KEYNAME 42000 S1009 */
N_("Duplicate key name '%-.192s'"),
/* ER_DUP_ENTRY 23000 S1009 */
N_("Duplicate entry '%-.192s' for key %d"),
/* ER_WRONG_FIELD_SPEC 42000 S1009 */
N_("Incorrect column specifier for column '%-.192s'"),
/* ER_PARSE_ERROR 42000 s1009 */
N_("%s near '%-.80s' at line %d"),
/* ER_EMPTY_QUERY 42000   */
N_("Query was empty"),
/* ER_NONUNIQ_TABLE 42000 S1009 */
N_("Not unique table/alias: '%-.192s'"),
/* ER_INVALID_DEFAULT 42000 S1009 */
N_("Invalid default value for '%-.192s'"),
/* ER_MULTIPLE_PRI_KEY 42000 S1009 */
N_("Multiple primary key defined"),
/* ER_TOO_MANY_KEYS 42000 S1009 */
N_("Too many keys specified; max %d keys allowed"),
/* ER_TOO_MANY_KEY_PARTS 42000 S1009 */
N_("Too many key parts specified; max %d parts allowed"),
/* ER_TOO_LONG_KEY 42000 S1009 */
N_("Specified key was too long; max key length is %d bytes"),
/* ER_KEY_COLUMN_DOES_NOT_EXITS 42000 S1009 */
N_("Key column '%-.192s' doesn't exist in table"),
/* ER_BLOB_USED_AS_KEY 42000 S1009 */
N_("BLOB column '%-.192s' can't be used in key specification with the used table type"),
/* ER_TOO_BIG_FIELDLENGTH 42000 S1009 */
N_("Column length too big for column '%-.192s' (max = %d); use BLOB or TEXT instead"),
/* ER_WRONG_AUTO_KEY 42000 S1009 */
N_("Incorrect table definition; there can be only one auto column and it must be defined as a key"),
/* ER_READY   */
N_("%s: ready for connections.\nVersion: '%s'  socket: '%s'  port: %d"),
/* ER_NORMAL_SHUTDOWN   */
N_("%s: Normal shutdown\n"),
/* ER_GOT_SIGNAL   */
N_("%s: Got signal %d. Aborting!\n"),
/* ER_SHUTDOWN_COMPLETE   */
N_("%s: Shutdown complete\n"),
/* ER_FORCING_CLOSE 08S01  */
N_("%s: Forcing close of thread %ld  user: '%-.48s'\n"),
/* ER_IPSOCK_ERROR 08S01  */
N_("Can't create IP socket"),
/* ER_NO_SUCH_INDEX 42S12 S1009 */
N_("Table '%-.192s' has no index like the one used in CREATE INDEX; recreate the table"),
/* ER_WRONG_FIELD_TERMINATORS 42000 S1009 */
N_("Field separator argument '%-.32s' with length '%d' is not what is expected; check the manual"),
/* ER_BLOBS_AND_NO_TERMINATED 42000 S1009 */
N_("You can't use fixed rowlength with BLOBs; please use 'fields terminated by'"),
/* ER_TEXTFILE_NOT_READABLE   */
N_("The file '%-.128s' must be in the database directory or be readable by all"),
/* ER_FILE_EXISTS_ERROR   */
N_("File '%-.200s' already exists"),
/* ER_LOAD_INFO   */
N_("Records: %ld  Deleted: %ld  Skipped: %ld  Warnings: %ld"),
/* ER_ALTER_INFO   */
N_("Records: %ld  Duplicates: %ld"),
/* ER_WRONG_SUB_KEY   */
N_("Incorrect prefix key; the used key part isn't a string, the used length is longer than the key part, or the storage engine doesn't support unique prefix keys"),
/* ER_CANT_REMOVE_ALL_FIELDS 42000  */
N_("You can't delete all columns with ALTER TABLE; use DROP TABLE instead"),
/* ER_CANT_DROP_FIELD_OR_KEY 42000  */
N_("Can't DROP '%-.192s'; check that column/key exists"),
/* ER_INSERT_INFO   */
N_("Records: %ld  Duplicates: %ld  Warnings: %ld"),
/* ER_UPDATE_TABLE_USED   */
N_("You can't specify target table '%-.192s' for update in FROM clause"),
/* ER_NO_SUCH_THREAD   */
N_("Unknown thread id: %lu"),
/* ER_KILL_DENIED_ERROR   */
N_("You are not owner of thread %lu"),
/* ER_NO_TABLES_USED   */
N_("No tables used"),
/* ER_TOO_BIG_SET   */
N_("Too many strings for column %-.192s and SET"),
/* ER_NO_UNIQUE_LOGFILE   */
N_("Can't generate a unique log-filename %-.200s.(1-999)\n"),
/* ER_TABLE_NOT_LOCKED_FOR_WRITE   */
N_("Table '%-.192s' was locked with a READ lock and can't be updated"),
/* ER_TABLE_NOT_LOCKED   */
N_("Table '%-.192s' was not locked with LOCK TABLES"),
/* ER_BLOB_CANT_HAVE_DEFAULT 42000  */
N_("BLOB/TEXT column '%-.192s' can't have a default value"),
/* ER_WRONG_DB_NAME 42000  */
N_("Incorrect database name '%-.100s'"),
/* ER_WRONG_TABLE_NAME 42000  */
N_("Incorrect table name '%-.100s'"),
/* ER_TOO_BIG_SELECT 42000  */
N_("The SELECT would examine more than MAX_JOIN_SIZE rows; check your WHERE and use SET SQL_BIG_SELECTS=1 or SET MAX_JOIN_SIZE=# if the SELECT is okay"),
/* ER_UNKNOWN_ERROR   */
N_("Unknown error"),
/* ER_UNKNOWN_PROCEDURE 42000  */
N_("Unknown procedure '%-.192s'"),
/* ER_WRONG_PARAMCOUNT_TO_PROCEDURE 42000  */
N_("Incorrect parameter count to procedure '%-.192s'"),
/* ER_WRONG_PARAMETERS_TO_PROCEDURE   */
N_("Incorrect parameters to procedure '%-.192s'"),
/* ER_UNKNOWN_TABLE 42S02  */
N_("Unknown table '%-.192s' in %-.32s"),
/* ER_FIELD_SPECIFIED_TWICE 42000  */
N_("Column '%-.192s' specified twice"),
/* ER_INVALID_GROUP_FUNC_USE   */
N_("Invalid use of group function"),
/* ER_UNSUPPORTED_EXTENSION 42000  */
N_("Table '%-.192s' uses an extension that doesn't exist in this Drizzle version"),
/* ER_TABLE_MUST_HAVE_COLUMNS 42000  */
N_("A table must have at least 1 column"),
/* ER_RECORD_FILE_FULL   */
N_("The table '%-.192s' is full"),
/* ER_UNKNOWN_CHARACTER_SET 42000  */
N_("Unknown character set: '%-.64s'"),
/* ER_TOO_MANY_TABLES   */
N_("Too many tables; Drizzle can only use %d tables in a join"),
/* ER_TOO_MANY_FIELDS   */
N_("Too many columns"),
/* ER_TOO_BIG_ROWSIZE 42000  */
N_("Row size too large. The maximum row size for the used table type, not counting BLOBs, is %ld. You have to change some columns to TEXT or BLOBs"),
/* ER_STACK_OVERRUN   */
N_("Thread stack overrun:  Used: %ld of a %ld stack.  Use 'drizzled -O thread_stack=#' to specify a bigger stack if needed"),
/* ER_WRONG_OUTER_JOIN 42000  */
N_("Cross dependency found in OUTER JOIN; examine your ON conditions"),
/* ER_NULL_COLUMN_IN_INDEX 42000  */
N_("Table handler doesn't support NULL in given index. Please change column '%-.192s' to be NOT NULL or use another handler"),
/* ER_CANT_FIND_UDF   */
N_("Can't load function '%-.192s'"),
/* ER_CANT_INITIALIZE_UDF   */
N_("Can't initialize function '%-.192s'; %-.80s"),
/* ER_UDF_NO_PATHS   */
N_("No paths allowed for shared library"),
/* ER_UDF_EXISTS   */
N_("Function '%-.192s' already exists"),
/* ER_CANT_OPEN_LIBRARY   */
N_("Can't open shared library '%-.192s' (errno: %d %-.128s)"),
/* ER_CANT_FIND_DL_ENTRY */
N_("Can't find symbol '%-.128s' in library"),
/* ER_FUNCTION_NOT_DEFINED   */
N_("Function '%-.192s' is not defined"),
/* ER_HOST_IS_BLOCKED   */
N_("Host '%-.64s' is blocked because of many connection errors; unblock with 'drizzleadmin flush-hosts'"),
/* ER_HOST_NOT_PRIVILEGED   */
N_("Host '%-.64s' is not allowed to connect to this Drizzle server"),
/* ER_PASSWORD_ANONYMOUS_USER 42000  */
N_("You are using Drizzle as an anonymous user and anonymous users are not allowed to change passwords"),
/* ER_PASSWORD_NOT_ALLOWED 42000  */
N_("You must have privileges to update tables in the drizzle database to be able to change passwords for others"),
/* ER_PASSWORD_NO_MATCH 42000  */
N_("Can't find any matching row in the user table"),
/* ER_UPDATE_INFO   */
N_("Rows matched: %ld  Changed: %ld  Warnings: %ld"),
/* ER_CANT_CREATE_THREAD   */
N_("Can't create a new thread (errno %d); if you are not out of available memory, you can consult the manual for a possible OS-dependent bug"),
/* ER_WRONG_VALUE_COUNT_ON_ROW 21S01  */
N_("Column count doesn't match value count at row %ld"),
/* ER_CANT_REOPEN_TABLE   */
N_("Can't reopen table: '%-.192s'"),
/* ER_INVALID_USE_OF_NULL 22004  */
N_("Invalid use of NULL value"),
/* ER_REGEXP_ERROR 42000  */
N_("Got error '%-.64s' from regexp"),
/* ER_MIX_OF_GROUP_FUNC_AND_FIELDS 42000  */
N_("Mixing of GROUP columns (MIN(),MAX(),COUNT(),...) with no GROUP columns is illegal if there is no GROUP BY clause"),
/* ER_NONEXISTING_GRANT 42000  */
N_("There is no such grant defined for user '%-.48s' on host '%-.64s'"),
/* ER_TABLEACCESS_DENIED_ERROR 42000  */
N_("%-.16s command denied to user '%-.48s'@'%-.64s' for table '%-.192s'"),
/* ER_COLUMNACCESS_DENIED_ERROR 42000  */
N_("%-.16s command denied to user '%-.48s'@'%-.64s' for column '%-.192s' in table '%-.192s'"),
/* ER_ILLEGAL_GRANT_FOR_TABLE 42000  */
N_("Illegal GRANT/REVOKE command; please consult the manual to see which privileges can be used"),
/* ER_GRANT_WRONG_HOST_OR_USER 42000  */
N_("The host or user argument to GRANT is too long"),
/* ER_NO_SUCH_TABLE 42S02  */
N_("Table '%-.192s.%-.192s' doesn't exist"),
/* ER_NONEXISTING_TABLE_GRANT 42000  */
N_("There is no such grant defined for user '%-.48s' on host '%-.64s' on table '%-.192s'"),
/* ER_NOT_ALLOWED_COMMAND 42000  */
N_("The used command is not allowed with this Drizzle version"),
/* ER_SYNTAX_ERROR 42000  */
N_("You have an error in your SQL syntax; check the manual that corresponds to your Drizzle server version for the right syntax to use"),
/* ER_DELAYED_CANT_CHANGE_LOCK   */
N_("Delayed insert thread couldn't get requested lock for table %-.192s"),
/* ER_TOO_MANY_DELAYED_THREADS   */
N_("Too many delayed threads in use"),
/* ER_ABORTING_CONNECTION 08S01  */
N_("Aborted connection %ld to db: '%-.192s' user: '%-.48s' (%-.64s)"),
/* ER_NET_PACKET_TOO_LARGE 08S01  */
N_("Got a packet bigger than 'max_allowed_packet' bytes"),
/* ER_NET_READ_ERROR_FROM_PIPE 08S01  */
N_("Got a read error from the connection pipe"),
/* ER_NET_FCNTL_ERROR 08S01  */
N_("Got an error from fcntl()"),
/* ER_NET_PACKETS_OUT_OF_ORDER 08S01  */
N_("Got packets out of order"),
/* ER_NET_UNCOMPRESS_ERROR 08S01  */
N_("Couldn't uncompress communication packet"),
/* ER_NET_READ_ERROR 08S01  */
N_("Got an error reading communication packets"),
/* ER_NET_READ_INTERRUPTED 08S01  */
N_("Got timeout reading communication packets"),
/* ER_NET_ERROR_ON_WRITE 08S01  */
N_("Got an error writing communication packets"),
/* ER_NET_WRITE_INTERRUPTED 08S01  */
N_("Got timeout writing communication packets"),
/* ER_TOO_LONG_STRING 42000  */
N_("Result string is longer than 'max_allowed_packet' bytes"),
/* ER_TABLE_CANT_HANDLE_BLOB 42000  */
N_("The used table type doesn't support BLOB/TEXT columns"),
/* ER_TABLE_CANT_HANDLE_AUTO_INCREMENT 42000  */
N_("The used table type doesn't support AUTO_INCREMENT columns"),
/* ER_DELAYED_INSERT_TABLE_LOCKED   */
N_("INSERT DELAYED can't be used with table '%-.192s' because it is locked with LOCK TABLES"),
/* ER_WRONG_COLUMN_NAME 42000  */
N_("Incorrect column name '%-.100s'"),
/* ER_WRONG_KEY_COLUMN 42000  */
N_("The used storage engine can't index column '%-.192s'"),
/* ER_WRONG_MRG_TABLE   */
N_("Unable to open underlying table which is differently defined or of non-MyISAM type or doesn't exist"),
/* ER_DUP_UNIQUE 23000  */
N_("Can't write, because of unique constraint, to table '%-.192s'"),
/* ER_BLOB_KEY_WITHOUT_LENGTH 42000  */
N_("BLOB/TEXT column '%-.192s' used in key specification without a key length"),
/* ER_PRIMARY_CANT_HAVE_NULL 42000  */
N_("All parts of a PRIMARY KEY must be NOT NULL; if you need NULL in a key, use UNIQUE instead"),
/* ER_TOO_MANY_ROWS 42000  */
N_("Result consisted of more than one row"),
/* ER_REQUIRES_PRIMARY_KEY 42000  */
N_("This table type requires a primary key"),
/* ER_NO_RAID_COMPILED   */
N_("This version of Drizzle is not compiled with RAID support"),
/* ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE   */
N_("You are using safe update mode and you tried to update a table without a WHERE that uses a KEY column"),
/* ER_KEY_DOES_NOT_EXITS 42000 S1009 */
N_("Key '%-.192s' doesn't exist in table '%-.192s'"),
/* ER_CHECK_NO_SUCH_TABLE 42000  */
N_("Can't open table"),
/* ER_CHECK_NOT_IMPLEMENTED 42000  */
N_("The storage engine for the table doesn't support %s"),
/* ER_CANT_DO_THIS_DURING_AN_TRANSACTION 25000  */
N_("You are not allowed to execute this command in a transaction"),
/* ER_ERROR_DURING_COMMIT   */
N_("Got error %d during COMMIT"),
/* ER_ERROR_DURING_ROLLBACK   */
N_("Got error %d during ROLLBACK"),
/* ER_ERROR_DURING_FLUSH_LOGS   */
N_("Got error %d during FLUSH_LOGS"),
/* ER_ERROR_DURING_CHECKPOINT   */
N_("Got error %d during CHECKPOINT"),
/* ER_NEW_ABORTING_CONNECTION 08S01  */
N_("Aborted connection %"PRIi64" to db: '%-.192s' user: '%-.48s' host: '%-.64s' (%-.64s)"),
/* ER_DUMP_NOT_IMPLEMENTED   */
N_("The storage engine for the table does not support binary table dump"),
/* ER_FLUSH_MASTER_BINLOG_CLOSED   */
N_("Binlog closed, cannot RESET MASTER"),
/* ER_INDEX_REBUILD   */
N_("Failed rebuilding the index of  dumped table '%-.192s'"),
/* ER_MASTER   */
N_("Error from master: '%-.64s'"),
/* ER_MASTER_NET_READ 08S01  */
N_("Net error reading from master"),
/* ER_MASTER_NET_WRITE 08S01  */
N_("Net error writing to master"),
/* ER_FT_MATCHING_KEY_NOT_FOUND   */
N_("Can't find FULLTEXT index matching the column list"),
/* ER_LOCK_OR_ACTIVE_TRANSACTION   */
N_("Can't execute the given command because you have active locked tables or an active transaction"),
/* ER_UNKNOWN_SYSTEM_VARIABLE   */
N_("Unknown system variable '%-.64s'"),
/* ER_CRASHED_ON_USAGE   */
N_("Table '%-.192s' is marked as crashed and should be repaired"),
/* ER_CRASHED_ON_REPAIR   */
N_("Table '%-.192s' is marked as crashed and last (automatic?) repair failed"),
/* ER_WARNING_NOT_COMPLETE_ROLLBACK   */
N_("Some non-transactional changed tables couldn't be rolled back"),
/* ER_TRANS_CACHE_FULL   */
N_("Multi-statement transaction required more than 'max_binlog_cache_size' bytes of storage; increase this drizzled variable and try again"),
/* ER_SLAVE_MUST_STOP   */
N_("This operation cannot be performed with a running slave; run STOP SLAVE first"),
/* ER_SLAVE_NOT_RUNNING   */
N_("This operation requires a running slave; configure slave and do START SLAVE"),
/* ER_BAD_SLAVE   */
N_("The server is not configured as slave; fix with CHANGE MASTER TO"),
/* ER_MASTER_INFO   */
N_("Could not initialize master info structure; more error messages can be found in the Drizzle error log"),
/* ER_SLAVE_THREAD   */
N_("Could not create slave thread; check system resources"),
/* ER_TOO_MANY_USER_CONNECTIONS 42000  */
N_("User %-.64s already has more than 'max_user_connections' active connections"),
/* ER_SET_CONSTANTS_ONLY   */
N_("You may only use constant expressions with SET"),
/* ER_LOCK_WAIT_TIMEOUT   */
N_("Lock wait timeout exceeded; try restarting transaction"),
/* ER_LOCK_TABLE_FULL   */
N_("The total number of locks exceeds the lock table size"),
/* ER_READ_ONLY_TRANSACTION 25000  */
N_("Update locks cannot be acquired during a READ UNCOMMITTED transaction"),
/* ER_DROP_DB_WITH_READ_LOCK   */
N_("DROP DATABASE not allowed while thread is holding global read lock"),
/* ER_CREATE_DB_WITH_READ_LOCK   */
N_("CREATE DATABASE not allowed while thread is holding global read lock"),
/* ER_WRONG_ARGUMENTS   */
N_("Incorrect arguments to %s"),
/* ER_NO_PERMISSION_TO_CREATE_USER 42000  */
N_("'%-.48s'@'%-.64s' is not allowed to create new users"),
/* ER_UNION_TABLES_IN_DIFFERENT_DIR   */
N_("Incorrect table definition; all MERGE tables must be in the same database"),
/* ER_LOCK_DEADLOCK 40001  */
N_("Deadlock found when trying to get lock; try restarting transaction"),
/* ER_TABLE_CANT_HANDLE_FT   */
N_("The used table type doesn't support FULLTEXT indexes"),
/* ER_CANNOT_ADD_FOREIGN   */
N_("Cannot add foreign key constraint"),
/* ER_NO_REFERENCED_ROW 23000  */
N_("Cannot add or update a child row: a foreign key constraint fails"),
/* ER_ROW_IS_REFERENCED 23000  */
N_("Cannot delete or update a parent row: a foreign key constraint fails"),
/* ER_CONNECT_TO_MASTER 08S01  */
N_("Error connecting to master: %-.128s"),
/* ER_QUERY_ON_MASTER   */
N_("Error running query on master: %-.128s"),
/* ER_ERROR_WHEN_EXECUTING_COMMAND   */
N_("Error when executing command %s: %-.128s"),
/* ER_WRONG_USAGE   */
N_("Incorrect usage of %s and %s"),
/* ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT 21000  */
N_("The used SELECT statements have a different number of columns"),
/* ER_CANT_UPDATE_WITH_READLOCK   */
N_("Can't execute the query because you have a conflicting read lock"),
/* ER_MIXING_NOT_ALLOWED   */
N_("Mixing of transactional and non-transactional tables is disabled"),
/* ER_DUP_ARGUMENT   */
N_("Option '%s' used twice in statement"),
/* ER_USER_LIMIT_REACHED 42000  */
N_("User '%-.64s' has exceeded the '%s' resource (current value: %ld)"),
/* ER_SPECIFIC_ACCESS_DENIED_ERROR 42000  */
N_("Access denied; you need the %-.128s privilege for this operation"),
/* ER_LOCAL_VARIABLE   */
N_("Variable '%-.64s' is a SESSION variable and can't be used with SET GLOBAL"),
/* ER_GLOBAL_VARIABLE   */
N_("Variable '%-.64s' is a GLOBAL variable and should be set with SET GLOBAL"),
/* ER_NO_DEFAULT 42000  */
N_("Variable '%-.64s' doesn't have a default value"),
/* ER_WRONG_VALUE_FOR_VAR 42000  */
N_("Variable '%-.64s' can't be set to the value of '%-.200s'"),
/* ER_WRONG_TYPE_FOR_VAR 42000  */
N_("Incorrect argument type to variable '%-.64s'"),
/* ER_VAR_CANT_BE_READ   */
N_("Variable '%-.64s' can only be set, not read"),
/* ER_CANT_USE_OPTION_HERE 42000  */
N_("Incorrect usage/placement of '%s'"),
/* ER_NOT_SUPPORTED_YET 42000  */
N_("This version of Drizzle doesn't yet support '%s'"),
/* ER_MASTER_FATAL_ERROR_READING_BINLOG   */
N_("Got fatal error %d: '%-.128s' from master when reading data from binary log"),
/* ER_SLAVE_IGNORED_TABLE   */
N_("Slave SQL thread ignored the query because of replicate-*-table rules"),
/* ER_INCORRECT_GLOBAL_LOCAL_VAR   */
N_("Variable '%-.192s' is a %s variable"),
/* ER_WRONG_FK_DEF 42000  */
N_("Incorrect foreign key definition for '%-.192s': %s"),
/* ER_KEY_REF_DO_NOT_MATCH_TABLE_REF   */
N_("Key reference and table reference don't match"),
/* ER_OPERAND_COLUMNS 21000  */
N_("Operand should contain %d column(s)"),
/* ER_SUBQUERY_NO_1_ROW 21000  */
N_("Subquery returns more than 1 row"),
/* ER_UNKNOWN_STMT_HANDLER   */
N_("Unknown prepared statement handler (%.*s) given to %s"),
/* ER_CORRUPT_HELP_DB   */
N_("Help database is corrupt or does not exist"),
/* ER_CYCLIC_REFERENCE   */
N_("Cyclic reference on subqueries"),
/* ER_AUTO_CONVERT   */
N_("Converting column '%s' from %s to %s"),
/* ER_ILLEGAL_REFERENCE 42S22  */
N_("Reference '%-.64s' not supported (%s)"),
/* ER_DERIVED_MUST_HAVE_ALIAS 42000  */
N_("Every derived table must have its own alias"),
/* ER_SELECT_REDUCED 01000  */
N_("Select %u was reduced during optimization"),
/* ER_TABLENAME_NOT_ALLOWED_HERE 42000  */
N_("Table '%-.192s' from one of the SELECTs cannot be used in %-.32s"),
/* ER_NOT_SUPPORTED_AUTH_MODE 08004  */
N_("Client does not support authentication protocol requested by server; consider upgrading Drizzle client"),
/* ER_SPATIAL_CANT_HAVE_NULL 42000  */
N_("All parts of a SPATIAL index must be NOT NULL"),
/* ER_COLLATION_CHARSET_MISMATCH 42000  */
N_("COLLATION '%s' is not valid for CHARACTER SET '%s'"),
/* ER_SLAVE_WAS_RUNNING   */
N_("Slave is already running"),
/* ER_SLAVE_WAS_NOT_RUNNING   */
N_("Slave already has been stopped"),
/* ER_TOO_BIG_FOR_UNCOMPRESS   */
N_("Uncompressed data size too large; the maximum size is %d (probably, length of uncompressed data was corrupted)"),
/* ER_ZLIB_Z_MEM_ERROR   */
N_("ZLIB: Not enough memory"),
/* ER_ZLIB_Z_BUF_ERROR   */
N_("ZLIB: Not enough room in the output buffer (probably, length of uncompressed data was corrupted)"),
/* ER_ZLIB_Z_DATA_ERROR   */
N_("ZLIB: Input data corrupted"),
/* ER_CUT_VALUE_GROUP_CONCAT   */
N_("%d line(s) were cut by GROUP_CONCAT()"),
/* ER_WARN_TOO_FEW_RECORDS 01000  */
N_("Row %ld doesn't contain data for all columns"),
/* ER_WARN_TOO_MANY_RECORDS 01000  */
N_("Row %ld was truncated; it contained more data than there were input columns"),
/* ER_WARN_NULL_TO_NOTNULL 22004  */
N_("Column set to default value; NULL supplied to NOT NULL column '%s' at row %ld"),
/* ER_WARN_DATA_OUT_OF_RANGE 22003  */
N_("Out of range value for column '%s' at row %ld"),
/* WARN_DATA_TRUNCATED 01000  */
N_("Data truncated for column '%s' at row %ld"),
/* ER_WARN_USING_OTHER_HANDLER   */
N_("Using storage engine %s for table '%s'"),
/* ER_CANT_AGGREGATE_2COLLATIONS   */
N_("Illegal mix of collations (%s,%s) and (%s,%s) for operation '%s'"),
/* ER_DROP_USER   */
N_("Cannot drop one or more of the requested users"),
/* ER_REVOKE_GRANTS   */
N_("Can't revoke all privileges for one or more of the requested users"),
/* ER_CANT_AGGREGATE_3COLLATIONS   */
N_("Illegal mix of collations (%s,%s), (%s,%s), (%s,%s) for operation '%s'"),
/* ER_CANT_AGGREGATE_NCOLLATIONS   */
N_("Illegal mix of collations for operation '%s'"),
/* ER_VARIABLE_IS_NOT_STRUCT   */
N_("Variable '%-.64s' is not a variable component (can't be used as XXXX.variable_name)"),
/* ER_UNKNOWN_COLLATION   */
N_("Unknown collation: '%-.64s'"),
/* ER_SLAVE_IGNORED_SSL_PARAMS   */
N_("SSL parameters in CHANGE MASTER are ignored because this Drizzle slave was compiled without SSL support; they can be used later if Drizzle slave with SSL is started"),
/* ER_SERVER_IS_IN_SECURE_AUTH_MODE   */
N_("Server is running in --secure-auth mode, but '%s'@'%s' has a password in the old format; please change the password to the new format"),
/* ER_WARN_FIELD_RESOLVED   */
N_("Field or reference '%-.192s%s%-.192s%s%-.192s' of SELECT #%d was resolved in SELECT #%d"),
/* ER_BAD_SLAVE_UNTIL_COND   */
N_("Incorrect parameter or combination of parameters for START SLAVE UNTIL"),
/* ER_MISSING_SKIP_SLAVE   */
N_("It is recommended to use --skip-slave-start when doing step-by-step replication with START SLAVE UNTIL; otherwise, you will get problems if you get an unexpected slave's drizzled restart"),
/* ER_UNTIL_COND_IGNORED   */
N_("SQL thread is not to be started so UNTIL options are ignored"),
/* ER_WRONG_NAME_FOR_INDEX 42000  */
N_("Incorrect index name '%-.100s'"),
/* ER_WRONG_NAME_FOR_CATALOG 42000  */
N_("Incorrect catalog name '%-.100s'"),
/* ER_WARN_QC_RESIZE   */
N_("Query cache failed to set size %lu; new query cache size is %lu"),
/* ER_BAD_FT_COLUMN   */
N_("Column '%-.192s' cannot be part of FULLTEXT index"),
/* ER_UNKNOWN_KEY_CACHE   */
N_("Unknown key cache '%-.100s'"),
/* ER_WARN_HOSTNAME_WONT_WORK   */
N_("Drizzle is started in --skip-name-resolve mode; you must restart it without this switch for this grant to work"),
/* ER_UNKNOWN_STORAGE_ENGINE 42000  */
N_("Unknown table engine '%s'"),
/* ER_WARN_DEPRECATED_SYNTAX   */
N_("'%s' is deprecated; use '%s' instead"),
/* ER_NON_UPDATABLE_TABLE   */
N_("The target table %-.100s of the %s is not updatable"),
/* ER_FEATURE_DISABLED   */
N_("The '%s' feature is disabled; you need Drizzle built with '%s' to have it working"),
/* ER_OPTION_PREVENTS_STATEMENT   */
N_("The Drizzle server is running with the %s option so it cannot execute this statement"),
/* ER_DUPLICATED_VALUE_IN_TYPE   */
N_("Column '%-.100s' has duplicated value '%-.64s' in %s"),
/* ER_TRUNCATED_WRONG_VALUE 22007  */
N_("Truncated incorrect %-.32s value: '%-.128s'"),
/* ER_TOO_MUCH_AUTO_TIMESTAMP_COLS   */
N_("Incorrect table definition; there can be only one TIMESTAMP column with CURRENT_TIMESTAMP in DEFAULT or ON UPDATE clause"),
/* ER_INVALID_ON_UPDATE   */
N_("Invalid ON UPDATE clause for '%-.192s' column"),
/* ER_UNSUPPORTED_PS   */
N_("This command is not supported in the prepared statement protocol yet"),
/* ER_GET_ERRMSG   */
N_("Got error %d '%-.100s' from %s"),
/* ER_GET_TEMPORARY_ERRMSG   */
N_("Got temporary error %d '%-.100s' from %s"),
/* ER_UNKNOWN_TIME_ZONE   */
N_("Unknown or incorrect time zone: '%-.64s'"),
/* ER_WARN_INVALID_TIMESTAMP   */
N_("Invalid TIMESTAMP value in column '%s' at row %ld"),
/* ER_INVALID_CHARACTER_STRING   */
N_("Invalid %s character string: '%.64s'"),
/* ER_WARN_ALLOWED_PACKET_OVERFLOWED   */
N_("Result of %s() was larger than max_allowed_packet (%ld) - truncated"),
/* ER_CONFLICTING_DECLARATIONS   */
N_("Conflicting declarations: '%s%s' and '%s%s'"),
/* ER_SP_NO_RECURSIVE_CREATE 2F003  */
N_("Can't create a %s from within another stored routine"),
/* ER_SP_ALREADY_EXISTS 42000  */
N_("%s %s already exists"),
/* ER_SP_DOES_NOT_EXIST 42000  */
N_("%s %s does not exist"),
/* ER_SP_DROP_FAILED   */
N_("Failed to DROP %s %s"),
/* ER_SP_STORE_FAILED   */
N_("Failed to CREATE %s %s"),
/* ER_SP_LILABEL_MISMATCH 42000  */
N_("%s with no matching label: %s"),
/* ER_SP_LABEL_REDEFINE 42000  */
N_("Redefining label %s"),
/* ER_SP_LABEL_MISMATCH 42000  */
N_("End-label %s without match"),
/* ER_SP_UNINIT_VAR 01000  */
N_("Referring to uninitialized variable %s"),
/* ER_SP_BADSELECT 0A000  */
N_("PROCEDURE %s can't return a result set in the given context"),
/* ER_SP_BADRETURN 42000  */
N_("RETURN is only allowed in a FUNCTION"),
/* ER_SP_BADSTATEMENT 0A000  */
N_("%s is not allowed in stored procedures"),
/* ER_UPDATE_LOG_DEPRECATED_IGNORED 42000  */
N_("The update log is deprecated and replaced by the binary log; SET SQL_LOG_UPDATE has been ignored"),
/* ER_UPDATE_LOG_DEPRECATED_TRANSLATED 42000  */
N_("The update log is deprecated and replaced by the binary log; SET SQL_LOG_UPDATE has been translated to SET SQL_LOG_BIN"),
/* ER_QUERY_INTERRUPTED 70100  */
N_("Query execution was interrupted"),
/* ER_SP_WRONG_NO_OF_ARGS 42000  */
N_("Incorrect number of arguments for %s %s; expected %u, got %u"),
/* ER_SP_COND_MISMATCH 42000  */
N_("Undefined CONDITION: %s"),
/* ER_SP_NORETURN 42000  */
N_("No RETURN found in FUNCTION %s"),
/* ER_SP_NORETURNEND 2F005  */
N_("FUNCTION %s ended without RETURN"),
/* ER_SP_BAD_CURSOR_QUERY 42000  */
N_("Cursor statement must be a SELECT"),
/* ER_SP_BAD_CURSOR_SELECT 42000  */
N_("Cursor SELECT must not have INTO"),
/* ER_SP_CURSOR_MISMATCH 42000  */
N_("Undefined CURSOR: %s"),
/* ER_SP_CURSOR_ALREADY_OPEN 24000  */
N_("Cursor is already open"),
/* ER_SP_CURSOR_NOT_OPEN 24000  */
N_("Cursor is not open"),
/* ER_SP_UNDECLARED_VAR 42000  */
N_("Undeclared variable: %s"),
/* ER_SP_WRONG_NO_OF_FETCH_ARGS   */
N_("Incorrect number of FETCH variables"),
/* ER_SP_FETCH_NO_DATA 02000  */
N_("No data - zero rows fetched, selected, or processed"),
/* ER_SP_DUP_PARAM 42000  */
N_("Duplicate parameter: %s"),
/* ER_SP_DUP_VAR 42000  */
N_("Duplicate variable: %s"),
/* ER_SP_DUP_COND 42000  */
N_("Duplicate condition: %s"),
/* ER_SP_DUP_CURS 42000  */
N_("Duplicate cursor: %s"),
/* ER_SP_CANT_ALTER   */
N_("Failed to ALTER %s %s"),
/* ER_SP_SUBSELECT_NYI 0A000  */
N_("Subquery value not supported"),
/* ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG 0A000 */
N_("%s is not allowed in stored function or trigger"),
/* ER_SP_VARCOND_AFTER_CURSHNDLR 42000  */
N_("Variable or condition declaration after cursor or handler declaration"),
/* ER_SP_CURSOR_AFTER_HANDLER 42000  */
N_("Cursor declaration after handler declaration"),
/* ER_SP_CASE_NOT_FOUND 20000  */
N_("Case not found for CASE statement"),
/* ER_FPARSER_TOO_BIG_FILE   */
N_("Configuration file '%-.192s' is too big"),
/* ER_FPARSER_BAD_HEADER   */
N_("Malformed file type header in file '%-.192s'"),
/* ER_FPARSER_EOF_IN_COMMENT   */
N_("Unexpected end of file while parsing comment '%-.200s'"),
/* ER_FPARSER_ERROR_IN_PARAMETER   */
N_("Error while parsing parameter '%-.192s' (line: '%-.192s')"),
/* ER_FPARSER_EOF_IN_UNKNOWN_PARAMETER   */
N_("Unexpected end of file while skipping unknown parameter '%-.192s'"),
/* ER_VIEW_NO_EXPLAIN   */
N_("EXPLAIN/SHOW can not be issued; lacking privileges for underlying table"),
/* ER_FRM_UNKNOWN_TYPE   */
N_("File '%-.192s' has unknown type '%-.64s' in its header"),
/* ER_WRONG_OBJECT   */
N_("'%-.192s.%-.192s' is not %s"),
/* ER_NONUPDATEABLE_COLUMN   */
N_("Column '%-.192s' is not updatable"),
/* ER_VIEW_SELECT_DERIVED   */
N_("View's SELECT contains a subquery in the FROM clause"),
/* ER_VIEW_SELECT_CLAUSE   */
N_("View's SELECT contains a '%s' clause"),
/* ER_VIEW_SELECT_VARIABLE   */
N_("View's SELECT contains a variable or parameter"),
/* ER_VIEW_SELECT_TMPTABLE   */
N_("View's SELECT refers to a temporary table '%-.192s'"),
/* ER_VIEW_WRONG_LIST   */
N_("View's SELECT and view's field list have different column counts"),
/* ER_WARN_VIEW_MERGE   */
N_("View merge algorithm can't be used here for now (assumed undefined algorithm)"),
/* ER_WARN_VIEW_WITHOUT_KEY   */
N_("View being updated does not have complete key of underlying table in it"),
/* ER_VIEW_INVALID   */
N_("View '%-.192s.%-.192s' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them"),
/* ER_SP_NO_DROP_SP   */
N_("Can't drop or alter a %s from within another stored routine"),
/* ER_SP_GOTO_IN_HNDLR   */
N_("GOTO is not allowed in a stored procedure handler"),
/* ER_TRG_ALREADY_EXISTS   */
N_("Trigger already exists"),
/* ER_TRG_DOES_NOT_EXIST   */
N_("Trigger does not exist"),
/* ER_TRG_ON_VIEW_OR_TEMP_TABLE   */
N_("Trigger's '%-.192s' is view or temporary table"),
/* ER_TRG_CANT_CHANGE_ROW   */
N_("Updating of %s row is not allowed in %strigger"),
/* ER_TRG_NO_SUCH_ROW_IN_TRG   */
N_("There is no %s row in %s trigger"),
/* ER_NO_DEFAULT_FOR_FIELD   */
N_("Field '%-.192s' doesn't have a default value"),
/* ER_DIVISION_BY_ZERO 22012  */
N_("Division by 0"),
/* ER_TRUNCATED_WRONG_VALUE_FOR_FIELD   */
N_("Incorrect %-.32s value: '%-.128s' for column '%.192s' at row %u"),
/* ER_ILLEGAL_VALUE_FOR_TYPE 22007  */
N_("Illegal %s '%-.192s' value found during parsing"),
/* ER_VIEW_NONUPD_CHECK   */
N_("CHECK OPTION on non-updatable view '%-.192s.%-.192s'"),
/* ER_VIEW_CHECK_FAILED   */
N_("CHECK OPTION failed '%-.192s.%-.192s'"),
/* ER_PROCACCESS_DENIED_ERROR 42000  */
N_("%-.16s command denied to user '%-.48s'@'%-.64s' for routine '%-.192s'"),
/* ER_RELAY_LOG_FAIL   */
N_("Failed purging old relay logs: %s"),
/* ER_PASSWD_LENGTH   */
N_("Password hash should be a %d-digit hexadecimal number"),
/* ER_UNKNOWN_TARGET_BINLOG   */
N_("Target log not found in binlog index"),
/* ER_IO_ERR_LOG_INDEX_READ   */
N_("I/O error reading log index file"),
/* ER_BINLOG_PURGE_PROHIBITED   */
N_("Server configuration does not permit binlog purge"),
/* ER_FSEEK_FAIL   */
N_("Failed on fseek()"),
/* ER_BINLOG_PURGE_FATAL_ERR   */
N_("Fatal error during log purge"),
/* ER_LOG_IN_USE   */
N_("A purgeable log is in use, will not purge"),
/* ER_LOG_PURGE_UNKNOWN_ERR   */
N_("Unknown error during log purge"),
/* ER_RELAY_LOG_INIT   */
N_("Failed initializing relay log position: %s"),
/* ER_NO_BINARY_LOGGING   */
N_("You are not using binary logging"),
/* ER_RESERVED_SYNTAX   */
N_("The '%-.64s' syntax is reserved for purposes internal to the Drizzle server"),
/* ER_WSAS_FAILED   */
N_("WSAStartup Failed"),
/* ER_DIFF_GROUPS_PROC   */
N_("Can't handle procedures with different groups yet"),
/* ER_NO_GROUP_FOR_PROC   */
N_("Select must have a group with this procedure"),
/* ER_ORDER_WITH_PROC   */
N_("Can't use ORDER clause with this procedure"),
/* ER_LOGGING_PROHIBIT_CHANGING_OF   */
N_("Binary logging and replication forbid changing the global server %s"),
/* ER_NO_FILE_MAPPING   */
N_("Can't map file: %-.200s, errno: %d"),
/* ER_WRONG_MAGIC   */
N_("Wrong magic in %-.64s"),
/* ER_PS_MANY_PARAM   */
N_("Prepared statement contains too many placeholders"),
/* ER_KEY_PART_0   */
N_("Key part '%-.192s' length cannot be 0"),
/* ER_VIEW_CHECKSUM   */
N_("View text checksum failed"),
/* ER_VIEW_MULTIUPDATE   */
N_("Can not modify more than one base table through a join view '%-.192s.%-.192s'"),
/* ER_VIEW_NO_INSERT_FIELD_LIST   */
N_("Can not insert into join view '%-.192s.%-.192s' without fields list"),
/* ER_VIEW_DELETE_MERGE_VIEW   */
N_("Can not delete from join view '%-.192s.%-.192s'"),
/* ER_CANNOT_USER   */
N_("Operation %s failed for %.256s"),
/* ER_XAER_NOTA XAE04 */
N_("XAER_NOTA: Unknown XID"),
/* ER_XAER_INVAL XAE05 */
N_("XAER_INVAL: Invalid arguments (or unsupported command)"),
/* ER_XAER_RMFAIL XAE07 */
N_("XAER_RMFAIL: The command cannot be executed when global transaction is in the  %.64s state"),
/* ER_XAER_OUTSIDE XAE09 */
N_("XAER_OUTSIDE: Some work is done outside global transaction"),
/* ER_XAER_RMERR XAE03 */
N_("XAER_RMERR: Fatal error occurred in the transaction branch - check your data for consistency"),
/* ER_XA_RBROLLBACK XA100 */
N_("XA_RBROLLBACK: Transaction branch was rolled back"),
/* ER_NONEXISTING_PROC_GRANT 42000  */
N_("There is no such grant defined for user '%-.48s' on host '%-.64s' on routine '%-.192s'"),
/* ER_PROC_AUTO_GRANT_FAIL */
N_("Failed to grant EXECUTE and ALTER ROUTINE privileges"),
/* ER_PROC_AUTO_REVOKE_FAIL */
N_("Failed to revoke all privileges to dropped routine"),
/* ER_DATA_TOO_LONG 22001 */
N_("Data too long for column '%s' at row %ld"),
/* ER_SP_BAD_SQLSTATE 42000 */
N_("Bad SQLSTATE: '%s'"),
/* ER_STARTUP */
N_("%s: ready for connections.\nVersion: '%s'  socket: '%s'  port: %d  %s"),
/* ER_LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR */
N_("Can't load value from file with fixed size rows to variable"),
/* ER_CANT_CREATE_USER_WITH_GRANT 42000 */
N_("You are not allowed to create a user with GRANT"),
/* ER_WRONG_VALUE_FOR_TYPE   */
N_("Incorrect %-.32s value: '%-.128s' for function %-.32s"),
/* ER_TABLE_DEF_CHANGED */
N_("Table definition has changed, please retry transaction"),
/* ER_SP_DUP_HANDLER 42000 */
N_("Duplicate handler declared in the same block"),
/* ER_SP_NOT_VAR_ARG 42000 */
N_("OUT or INOUT argument %d for routine %s is not a variable or NEW pseudo-variable in BEFORE trigger"),
/* ER_SP_NO_RETSET 0A000 */
N_("Not allowed to return a result set from a %s"),
/* ER_CANT_CREATE_GEOMETRY_OBJECT 22003  */
N_("Cannot get geometry object from data you send to the GEOMETRY field"),
/* ER_FAILED_ROUTINE_BREAK_BINLOG */
N_("A routine failed and has neither NO SQL nor READS SQL DATA in its declaration and binary logging is enabled; if non-transactional tables were updated, the binary log will miss their changes"),
/* ER_BINLOG_UNSAFE_ROUTINE */
N_("This function has none of DETERMINISTIC, NO SQL, or READS SQL DATA in its declaration and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)"),
/* ER_BINLOG_CREATE_ROUTINE_NEED_SUPER */
N_("You do not have the SUPER privilege and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)"),
/* ER_EXEC_STMT_WITH_OPEN_CURSOR */
N_("You can't execute a prepared statement which has an open cursor associated with it. Reset the statement to re-execute it."),
/* ER_STMT_HAS_NO_OPEN_CURSOR */
N_("The statement (%lu) has no open cursor."),
/* ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG */
N_("Explicit or implicit commit is not allowed in stored function or trigger."),
/* ER_NO_DEFAULT_FOR_VIEW_FIELD */
N_("Field of view '%-.192s.%-.192s' underlying table doesn't have a default value"),
/* ER_SP_NO_RECURSION */
N_("Recursive stored functions and triggers are not allowed."),
/* ER_TOO_BIG_SCALE 42000 S1009 */
N_("Too big scale %d specified for column '%-.192s'. Maximum is %d."),
/* ER_TOO_BIG_PRECISION 42000 S1009 */
N_("Too big precision %d specified for column '%-.192s'. Maximum is %d."),
/* ER_M_BIGGER_THAN_D 42000 S1009 */
N_("For float(M,D), double(M,D) or decimal(M,D), M must be >= D (column '%-.192s')."),
/* ER_WRONG_LOCK_OF_SYSTEM_TABLE */
N_("You can't combine write-locking of system tables with other tables or lock types"),
/* ER_CONNECT_TO_FOREIGN_DATA_SOURCE */
N_("Unable to connect to foreign data source: %.64s"),
/* ER_QUERY_ON_FOREIGN_DATA_SOURCE */
N_("There was a problem processing the query on the foreign data source. Data source error: %-.64s"),
/* ER_FOREIGN_DATA_SOURCE_DOESNT_EXIST */
N_("The foreign data source you are trying to reference does not exist. Data source error:  %-.64s"),
/* ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE */
N_("Can't create federated table. The data source connection string '%-.64s' is not in the correct format"),
/* ER_FOREIGN_DATA_STRING_INVALID */
N_("The data source connection string '%-.64s' is not in the correct format"),
/* ER_CANT_CREATE_FEDERATED_TABLE   */
N_("Can't create federated table. Foreign data src error:  %-.64s"),
/* ER_TRG_IN_WRONG_SCHEMA   */
N_("Trigger in wrong schema"),
/* ER_STACK_OVERRUN_NEED_MORE */
N_("Thread stack overrun:  %ld bytes used of a %ld byte stack, and %ld bytes needed.  Use 'drizzled -O thread_stack=#' to specify a bigger stack."),
/* ER_TOO_LONG_BODY 42000 S1009 */
N_("Routine body for '%-.100s' is too long"),
/* ER_WARN_CANT_DROP_DEFAULT_KEYCACHE */
N_("Cannot drop default keycache"),
/* ER_TOO_BIG_DISPLAYWIDTH 42000 S1009 */
N_("Display width out of range for column '%-.192s' (max = %d)"),
/* ER_XAER_DUPID XAE08 */
N_("XAER_DUPID: The XID already exists"),
/* ER_DATETIME_FUNCTION_OVERFLOW 22008 */
N_("Datetime function: %-.32s field overflow"),
/* ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG */
N_("Can't update table '%-.192s' in stored function/trigger because it is already used by statement which invoked this stored function/trigger."),
/* ER_VIEW_PREVENT_UPDATE */
N_("The definition of table '%-.192s' prevents operation %.192s on table '%-.192s'."),
/* ER_PS_NO_RECURSION */
N_("The prepared statement contains a stored routine call that refers to that same statement. It's not allowed to execute a prepared statement in such a recursive manner"),
/* ER_SP_CANT_SET_AUTOCOMMIT */
N_("Not allowed to set autocommit from a stored function or trigger"),
/* ER_MALFORMED_DEFINER */
N_("Definer is not fully qualified"),
/* ER_VIEW_FRM_NO_USER */
N_("View '%-.192s'.'%-.192s' has no definer information (old table format). Current user is used as definer. Please recreate the view!"),
/* ER_VIEW_OTHER_USER */
N_("You need the SUPER privilege for creation view with '%-.192s'@'%-.192s' definer"),
/* ER_NO_SUCH_USER */
N_("The user specified as a definer ('%-.64s'@'%-.64s') does not exist"),
/* ER_FORBID_SCHEMA_CHANGE */
N_("Changing schema from '%-.192s' to '%-.192s' is not allowed."),
/* ER_ROW_IS_REFERENCED_2 23000 */
N_("Cannot delete or update a parent row: a foreign key constraint fails (%.192s)"),
/* ER_NO_REFERENCED_ROW_2 23000 */
N_("Cannot add or update a child row: a foreign key constraint fails (%.192s)"),
/* ER_SP_BAD_VAR_SHADOW 42000 */
N_("Variable '%-.64s' must be quoted with `...`, or renamed"),
/* ER_TRG_NO_DEFINER */
N_("No definer attribute for trigger '%-.192s'.'%-.192s'. The trigger will be activated under the authorization of the invoker, which may have insufficient privileges. Please recreate the trigger."),
/* ER_OLD_FILE_FORMAT */
N_("'%-.192s' has an old format, you should re-create the '%s' object(s)"),
/* ER_SP_RECURSION_LIMIT */
N_("Recursive limit %d (as set by the max_sp_recursion_depth variable) was exceeded for routine %.192s"),
/* ER_SP_PROC_TABLE_CORRUPT */
N_("Failed to load routine %-.192s. The table drizzle.proc is missing, corrupt, or contains bad data (internal code %d)"),
/* ER_SP_WRONG_NAME 42000 */
N_("Incorrect routine name '%-.192s'"),
/* ER_TABLE_NEEDS_UPGRADE */
N_("Table upgrade required. Please do \"REPAIR TABLE `%-.32s`\" to fix it!"),
/* ER_SP_NO_AGGREGATE 42000 */
N_("AGGREGATE is not supported for stored functions"),
/* ER_MAX_PREPARED_STMT_COUNT_REACHED 42000 */
N_("Can't create more than max_prepared_stmt_count statements (current value: %lu)"),
/* ER_VIEW_RECURSIVE */
N_("`%-.192s`.`%-.192s` contains view recursion"),
/* ER_NON_GROUPING_FIELD_USED 42000 */
N_("non-grouping field '%-.192s' is used in %-.64s clause"),
/* ER_TABLE_CANT_HANDLE_SPKEYS */
N_("The used table type doesn't support SPATIAL indexes"),
/* ER_NO_TRIGGERS_ON_SYSTEM_SCHEMA */
N_("Triggers can not be created on system tables"),
/* ER_REMOVED_SPACES */
N_("Leading spaces are removed from name '%s'"),
/* ER_AUTOINC_READ_FAILED */
N_("Failed to read auto-increment value from storage engine"),
/* ER_USERNAME */
N_("user name"),
/* ER_HOSTNAME */
N_("host name"),
/* ER_WRONG_STRING_LENGTH */
N_("String '%-.70s' is too long for %s (should be no longer than %d)"),
/* ER_NON_INSERTABLE_TABLE   */
N_("The target table %-.100s of the %s is not insertable-into"),
/* ER_ADMIN_WRONG_MRG_TABLE */
N_("Table '%-.64s' is differently defined or of non-MyISAM type or doesn't exist"),
/* ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT */
N_("Too high level of nesting for select"),
/* ER_NAME_BECOMES_EMPTY */
N_("Name '%-.64s' has become ''"),
/* ER_AMBIGUOUS_FIELD_TERM */
N_("First character of the FIELDS TERMINATED string is ambiguous; please use non-optional and non-empty FIELDS ENCLOSED BY"),
/* ER_FOREIGN_SERVER_EXISTS */
N_("The foreign server, %s, you are trying to create already exists."),
/* ER_FOREIGN_SERVER_DOESNT_EXIST */
N_("The foreign server name you are trying to reference does not exist. Data source error:  %-.64s"),
/* ER_ILLEGAL_HA_CREATE_OPTION */
N_("Table storage engine '%-.64s' does not support the create option '%.64s'"),
/* ER_PARTITION_REQUIRES_VALUES_ERROR */
N_("Syntax error: %-.64s PARTITIONING requires definition of VALUES %-.64s for each partition"),
/* ER_PARTITION_WRONG_VALUES_ERROR */
N_("Only %-.64s PARTITIONING can use VALUES %-.64s in partition definition"),
/* ER_PARTITION_MAXVALUE_ERROR */
N_("MAXVALUE can only be used in last partition definition"),
/* ER_PARTITION_SUBPARTITION_ERROR */
N_("Subpartitions can only be hash partitions and by key"),
/* ER_PARTITION_SUBPART_MIX_ERROR */
N_("Must define subpartitions on all partitions if on one partition"),
/* ER_PARTITION_WRONG_NO_PART_ERROR */
N_("Wrong number of partitions defined, mismatch with previous setting"),
/* ER_PARTITION_WRONG_NO_SUBPART_ERROR */
N_("Wrong number of subpartitions defined, mismatch with previous setting"),
/* ER_CONST_EXPR_IN_PARTITION_FUNC_ERROR */
N_("Constant/Random expression in (sub)partitioning function is not allowed"),
/* ER_NO_CONST_EXPR_IN_RANGE_OR_LIST_ERROR */
N_("Expression in RANGE/LIST VALUES must be constant"),
/* ER_FIELD_NOT_FOUND_PART_ERROR */
N_("Field in list of fields for partition function not found in table"),
/* ER_LIST_OF_FIELDS_ONLY_IN_HASH_ERROR */
N_("List of fields is only allowed in KEY partitions"),
/* ER_INCONSISTENT_PARTITION_INFO_ERROR */
N_("The partition info in the frm file is not consistent with what can be written into the frm file"),
/* ER_PARTITION_FUNC_NOT_ALLOWED_ERROR */
N_("The %-.192s function returns the wrong type"),
/* ER_PARTITIONS_MUST_BE_DEFINED_ERROR */
N_("For %-.64s partitions each partition must be defined"),
/* ER_RANGE_NOT_INCREASING_ERROR */
N_("VALUES LESS THAN value must be strictly increasing for each partition"),
/* ER_INCONSISTENT_TYPE_OF_FUNCTIONS_ERROR */
N_("VALUES value must be of same type as partition function"),
/* ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR */
N_("Multiple definition of same constant in list partitioning"),
/* ER_PARTITION_ENTRY_ERROR */
N_("Partitioning can not be used stand-alone in query"),
/* ER_MIX_HANDLER_ERROR */
N_("The mix of handlers in the partitions is not allowed in this version of Drizzle"),
/* ER_PARTITION_NOT_DEFINED_ERROR */
N_("For the partitioned engine it is necessary to define all %-.64s"),
/* ER_TOO_MANY_PARTITIONS_ERROR */
N_("Too many partitions (including subpartitions) were defined"),
/* ER_SUBPARTITION_ERROR */
N_("It is only possible to mix RANGE/LIST partitioning with HASH/KEY partitioning for subpartitioning"),
/* ER_CANT_CREATE_HANDLER_FILE */
N_("Failed to create specific handler file"),
/* ER_BLOB_FIELD_IN_PART_FUNC_ERROR */
N_("A BLOB field is not allowed in partition function"),
/* ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF */
N_("A %-.192s must include all columns in the table's partitioning function"),
/* ER_NO_PARTS_ERROR */
N_("Number of %-.64s = 0 is not an allowed value"),
/* ER_PARTITION_MGMT_ON_NONPARTITIONED */
N_("Partition management on a not partitioned table is not possible"),
/* ER_FOREIGN_KEY_ON_PARTITIONED */
N_("Foreign key condition is not yet supported in conjunction with partitioning"),
/* ER_DROP_PARTITION_NON_EXISTENT */
N_("Error in list of partitions to %-.64s"),
/* ER_DROP_LAST_PARTITION */
N_("Cannot remove all partitions, use DROP TABLE instead"),
/* ER_COALESCE_ONLY_ON_HASH_PARTITION */
N_("COALESCE PARTITION can only be used on HASH/KEY partitions"),
/* ER_REORG_HASH_ONLY_ON_SAME_NO */
N_("REORGANIZE PARTITION can only be used to reorganize partitions not to change their numbers"),
/* ER_REORG_NO_PARAM_ERROR */
N_("REORGANIZE PARTITION without parameters can only be used on auto-partitioned tables using HASH PARTITIONs"),
/* ER_ONLY_ON_RANGE_LIST_PARTITION */
N_("%-.64s PARTITION can only be used on RANGE/LIST partitions"),
/* ER_ADD_PARTITION_SUBPART_ERROR */
N_("Trying to Add partition(s) with wrong number of subpartitions"),
/* ER_ADD_PARTITION_NO_NEW_PARTITION */
N_("At least one partition must be added"),
/* ER_COALESCE_PARTITION_NO_PARTITION */
N_("At least one partition must be coalesced"),
/* ER_REORG_PARTITION_NOT_EXIST */
N_("More partitions to reorganize than there are partitions"),
/* ER_SAME_NAME_PARTITION */
N_("Duplicate partition name %-.192s"),
/* ER_NO_BINLOG_ERROR */
N_("It is not allowed to shut off binlog on this command"),
/* ER_CONSECUTIVE_REORG_PARTITIONS */
N_("When reorganizing a set of partitions they must be in consecutive order"),
/* ER_REORG_OUTSIDE_RANGE */
N_("Reorganize of range partitions cannot change total ranges except for last partition where it can extend the range"),
/* ER_PARTITION_FUNCTION_FAILURE */
N_("Partition function not supported in this version for this handler"),
/* ER_PART_STATE_ERROR */
N_("Partition state cannot be defined from CREATE/ALTER TABLE"),
/* ER_LIMITED_PART_RANGE */
N_("The %-.64s handler only supports 32 bit integers in VALUES"),
/* ER_PLUGIN_IS_NOT_LOADED */
N_("Plugin '%-.192s' is not loaded"),
/* ER_WRONG_VALUE */
N_("Incorrect %-.32s value: '%-.128s'"),
/* ER_NO_PARTITION_FOR_GIVEN_VALUE */
N_("Table has no partition for value %-.64s"),
/* ER_FILEGROUP_OPTION_ONLY_ONCE */
N_("It is not allowed to specify %s more than once"),
/* ER_CREATE_FILEGROUP_FAILED */
N_("Failed to create %s"),
/* ER_DROP_FILEGROUP_FAILED */
N_("Failed to drop %s"),
/* ER_TABLESPACE_AUTO_EXTEND_ERROR */
N_("The handler doesn't support autoextend of tablespaces"),
/* ER_WRONG_SIZE_NUMBER */
N_("A size parameter was incorrectly specified, either number or on the form 10M"),
/* ER_SIZE_OVERFLOW_ERROR */
N_("The size number was correct but we don't allow the digit part to be more than 2 billion"),
/* ER_ALTER_FILEGROUP_FAILED */
N_("Failed to alter: %s"),
/* ER_BINLOG_ROW_LOGGING_FAILED */
N_("Writing one row to the row-based binary log failed"),
/* ER_BINLOG_ROW_WRONG_TABLE_DEF */
N_("Table definition on master and slave does not match: %s"),
/* ER_BINLOG_ROW_RBR_TO_SBR */
N_("Slave running with --log-slave-updates must use row-based binary logging to be able to replicate row-based binary log events"),
/* ER_EVENT_ALREADY_EXISTS */
N_("Event '%-.192s' already exists"),
/* ER_EVENT_STORE_FAILED */
N_("Failed to store event %s. Error code %d from storage engine."),
/* ER_EVENT_DOES_NOT_EXIST */
N_("Unknown event '%-.192s'"),
/* ER_EVENT_CANT_ALTER */
N_("Failed to alter event '%-.192s'"),
/* ER_EVENT_DROP_FAILED */
N_("Failed to drop %s"),
/* ER_EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG */
N_("INTERVAL is either not positive or too big"),
/* ER_EVENT_ENDS_BEFORE_STARTS */
N_("ENDS is either invalid or before STARTS"),
/* ER_EVENT_EXEC_TIME_IN_THE_PAST */
N_("Event execution time is in the past. Event has been disabled"),
/* ER_EVENT_OPEN_TABLE_FAILED */
N_("Failed to open drizzle.event"),
/* ER_EVENT_NEITHER_M_EXPR_NOR_M_AT */
N_("No datetime expression provided"),
/* ER_COL_COUNT_DOESNT_MATCH_CORRUPTED */
N_("Column count of drizzle.%s is wrong. Expected %d, found %d. The table is probably corrupted"),
/* ER_CANNOT_LOAD_FROM_TABLE */
N_("Cannot load from drizzle.%s. The table is probably corrupted"),
/* ER_EVENT_CANNOT_DELETE */
N_("Failed to delete the event from drizzle.event"),
/* ER_EVENT_COMPILE_ERROR */
N_("Error during compilation of event's body"),
/* ER_EVENT_SAME_NAME */
N_("Same old and new event name"),
/* ER_EVENT_DATA_TOO_LONG */
N_("Data for column '%s' too long"),
/* ER_DROP_INDEX_FK */
N_("Cannot drop index '%-.192s': needed in a foreign key constraint"),
/* ER_WARN_DEPRECATED_SYNTAX_WITH_VER   */
N_("The syntax '%s' is deprecated and will be removed in Drizzle %s. Please use %s instead"),
/* ER_CANT_WRITE_LOCK_LOG_TABLE */
N_("You can't write-lock a log table. Only read access is possible"),
/* ER_CANT_LOCK_LOG_TABLE */
N_("You can't use locks with log tables."),
/* ER_FOREIGN_DUPLICATE_KEY 23000 S1009 */
N_("Upholding foreign key constraints for table '%.192s', entry '%-.192s', key %d would lead to a duplicate entry"),
/* ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE */
N_("Column count of drizzle.%s is wrong. Expected %d, found %d. Created with Drizzle %d, now running %d. Please use drizzle_upgrade to fix this error."),
/* ER_TEMP_TABLE_PREVENTS_SWITCH_OUT_OF_RBR */
N_("Cannot switch out of the row-based binary log format when the session has open temporary tables"),
/* ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_FORMAT */
N_("Cannot change the binary logging format inside a stored function or trigger"),
/* ER_NDB_CANT_SWITCH_BINLOG_FORMAT */
N_("The NDB cluster engine does not support changing the binlog format on the fly yet"),
/* ER_PARTITION_NO_TEMPORARY */
N_("Cannot create temporary table with partitions"),
/* ER_PARTITION_CONST_DOMAIN_ERROR */
N_("Partition constant is out of partition function domain"),
/* ER_PARTITION_FUNCTION_IS_NOT_ALLOWED */
N_("This partition function is not allowed"),
/* ER_DDL_LOG_ERROR */
N_("Error in DDL log"),
/* ER_NULL_IN_VALUES_LESS_THAN */
N_("Not allowed to use NULL value in VALUES LESS THAN"),
/* ER_WRONG_PARTITION_NAME */
N_("Incorrect partition name"),
/* ER_CANT_CHANGE_TX_ISOLATION 25001 */
N_("Transaction isolation level can't be changed while a transaction is in progress"),
/* ER_DUP_ENTRY_AUTOINCREMENT_CASE */
N_("ALTER TABLE causes auto_increment resequencing, resulting in duplicate entry '%-.192s' for key '%-.192s'"),
/* ER_EVENT_MODIFY_QUEUE_ERROR */
N_("Internal scheduler error %d"),
/* ER_EVENT_SET_VAR_ERROR */
N_("Error during starting/stopping of the scheduler. Error code %u"),
/* ER_PARTITION_MERGE_ERROR */
N_("Engine cannot be used in partitioned tables"),
/* ER_CANT_ACTIVATE_LOG */
N_("Cannot activate '%-.64s' log"),
/* ER_RBR_NOT_AVAILABLE */
N_("The server was not built with row-based replication"),
/* ER_BASE64_DECODE_ERROR */
N_("Decoding of base64 string failed"),
/* ER_EVENT_RECURSION_FORBIDDEN */
N_("Recursion of EVENT DDL statements is forbidden when body is present"),
/* ER_EVENTS_DB_ERROR */
N_("Cannot proceed because system tables used by Event Scheduler were found damaged at server start"),
/* ER_ONLY_INTEGERS_ALLOWED */
N_("Only integers allowed as number here"),
/* ER_UNSUPORTED_LOG_ENGINE */
N_("This storage engine cannot be used for log tables"),
/* ER_BAD_LOG_STATEMENT */
N_("You cannot '%s' a log table if logging is enabled"),
/* ER_CANT_RENAME_LOG_TABLE */
N_("Cannot rename '%s'. When logging enabled, rename to/from log table must rename two tables: the log table to an archive table and another table back to '%s'"),
/* ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT 42000 */
N_("Incorrect parameter count in the call to native function '%-.192s'"),
/* ER_WRONG_PARAMETERS_TO_NATIVE_FCT 42000 */
N_("Incorrect parameters in the call to native function '%-.192s'"),
/* ER_WRONG_PARAMETERS_TO_STORED_FCT 42000   */
N_("Incorrect parameters in the call to stored function '%-.192s'"),
/* ER_NATIVE_FCT_NAME_COLLISION */
N_("This function '%-.192s' has the same name as a native function"),
/* ER_DUP_ENTRY_WITH_KEY_NAME 23000 S1009 */
N_("Duplicate entry '%-.64s' for key '%-.192s'"),
/* ER_BINLOG_PURGE_EMFILE */
N_("Too many files opened, please execute the command again"),
/* ER_EVENT_CANNOT_CREATE_IN_THE_PAST */
N_("Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation."),
/* ER_EVENT_CANNOT_ALTER_IN_THE_PAST */
N_("Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation."),
/* ER_SLAVE_INCIDENT */
N_("The incident %s occurred on the master. Message: %-.64s"),
/* ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT */
N_("Table has no partition for some existing values"),
/* ER_BINLOG_UNSAFE_STATEMENT */
N_("Statement is not safe to log in statement format."),
/* ER_SLAVE_FATAL_ERROR */
N_("Fatal error: %s"),
/* ER_SLAVE_RELAY_LOG_READ_FAILURE */
N_("Relay log read failure: %s"),
/* ER_SLAVE_RELAY_LOG_WRITE_FAILURE */
N_("Relay log write failure: %s"),
/* ER_SLAVE_CREATE_EVENT_FAILURE */
N_("Failed to create %s"),
/* ER_SLAVE_MASTER_COM_FAILURE */
N_("Master command %s failed: %s"),
/* ER_BINLOG_LOGGING_IMPOSSIBLE */
N_("Binary logging not possible. Message: %s"),
/* ER_VIEW_NO_CREATION_CTX */
N_("View `%-.64s`.`%-.64s` has no creation context"),
/* ER_VIEW_INVALID_CREATION_CTX */
N_("Creation context of view `%-.64s`.`%-.64s' is invalid"),
/* ER_SR_INVALID_CREATION_CTX */
N_("Creation context of stored routine `%-.64s`.`%-.64s` is invalid"),
/* ER_TRG_CORRUPTED_FILE */
N_("Corrupted TRG file for table `%-.64s`.`%-.64s`"),
/* ER_TRG_NO_CREATION_CTX */
N_("Triggers for table `%-.64s`.`%-.64s` have no creation context"),
/* ER_TRG_INVALID_CREATION_CTX */
N_("Trigger creation context of table `%-.64s`.`%-.64s` is invalid"),
/* ER_EVENT_INVALID_CREATION_CTX */
N_("Creation context of event `%-.64s`.`%-.64s` is invalid"),
/* ER_TRG_CANT_OPEN_TABLE */
N_("Cannot open table for trigger `%-.64s`.`%-.64s`"),
/* ER_CANT_CREATE_SROUTINE */
N_("Cannot create stored routine `%-.64s`. Check warnings"),
/* ER_SLAVE_AMBIGOUS_EXEC_MODE */
N_("Ambiguous slave modes combination. %s"),
/* ER_NO_FORMAT_DESCRIPTION_EVENT_BEFORE_BINLOG_STATEMENT */
N_("The BINLOG statement of type `%s` was not preceded by a format description BINLOG statement."),
/* ER_SLAVE_CORRUPT_EVENT */
N_("Corrupted replication event was detected"),
/* ER_LOAD_DATA_INVALID_COLUMN */
N_("Invalid column reference (%-.64s) in LOAD DATA"),
/* ER_LOG_PURGE_NO_FILE   */
N_("Being purged log %s was not found"),
/* ER_WARN_AUTO_CONVERT_LOCK */
N_("Converted to non-transactional lock on '%-.64s'"),
/* ER_NO_AUTO_CONVERT_LOCK_STRICT */
N_("Cannot convert to non-transactional lock in strict mode on '%-.64s'"),
/* ER_NO_AUTO_CONVERT_LOCK_TRANSACTION */
N_("Cannot convert to non-transactional lock in an active transaction on '%-.64s'"),
/* ER_NO_STORAGE_ENGINE */
N_("Can't access storage engine of table %-.64s"),
/* ER_BACKUP_BACKUP_START */
N_("Starting backup process"),
/* ER_BACKUP_BACKUP_DONE */
N_("Backup completed"),
/* ER_BACKUP_RESTORE_START */
N_("Starting restore process"),
/* ER_BACKUP_RESTORE_DONE */
N_("Restore completed"),
/* ER_BACKUP_NOTHING_TO_BACKUP */
N_("Nothing to backup"),
/* ER_BACKUP_CANNOT_INCLUDE_DB */
N_("Database '%-.64s' cannot be included in a backup"),
/* ER_BACKUP_BACKUP */
N_("Error during backup operation - server's error log contains more information about the error"),
/* ER_BACKUP_RESTORE */
N_("Error during restore operation - server's error log contains more information about the error"),
/* ER_BACKUP_RUNNING */
N_("Can't execute this command because another BACKUP/RESTORE operation is in progress"),
/* ER_BACKUP_BACKUP_PREPARE */
N_("Error when preparing for backup operation"),
/* ER_BACKUP_RESTORE_PREPARE */
N_("Error when preparing for restore operation"),
/* ER_BACKUP_INVALID_LOC */
N_("Invalid backup location '%-.64s'"),
/* ER_BACKUP_READ_LOC */
N_("Can't read backup location '%-.64s'"),
/* ER_BACKUP_WRITE_LOC */
N_("Can't write to backup location '%-.64s' (file already exists?)"),
/* ER_BACKUP_LIST_DBS */
N_("Can't enumerate server databases"),
/* ER_BACKUP_LIST_TABLES */
N_("Can't enumerate server tables"),
/* ER_BACKUP_LIST_DB_TABLES */
N_("Can't enumerate tables in database %-.64s"),
/* ER_BACKUP_SKIP_VIEW */
N_("Skipping view %-.64s in database %-.64s"),
/* ER_BACKUP_NO_ENGINE */
N_("Skipping table %-.64s since it has no valid storage engine"),
/* ER_BACKUP_TABLE_OPEN */
N_("Can't open table %-.64s"),
/* ER_BACKUP_READ_HEADER */
N_("Can't read backup archive preamble"),
/* ER_BACKUP_WRITE_HEADER */
N_("Can't write backup archive preamble"),
/* ER_BACKUP_NO_BACKUP_DRIVER */
N_("Can't find backup driver for table %-.64s"),
/* ER_BACKUP_NOT_ACCEPTED */
N_("%-.64s backup driver was selected for table %-.64s but it rejects to handle this table"),
/* ER_BACKUP_CREATE_BACKUP_DRIVER */
N_("Can't create %-.64s backup driver"),
/* ER_BACKUP_CREATE_RESTORE_DRIVER */
N_("Can't create %-.64s restore driver"),
/* ER_BACKUP_TOO_MANY_IMAGES */
N_("Found %d images in backup archive but maximum %d are supported"),
/* ER_BACKUP_WRITE_META */
N_("Error when saving meta-data of %-.64s"),
/* ER_BACKUP_READ_META */
N_("Error when reading meta-data list"),
/* ER_BACKUP_CREATE_META */
N_("Can't create %-.64s"),
/* ER_BACKUP_GET_BUF */
N_("Can't allocate buffer for image data transfer"),
/* ER_BACKUP_WRITE_DATA */
N_("Error when writing %-.64s backup image data (for table #%d)"),
/* ER_BACKUP_READ_DATA */
N_("Error when reading data from backup stream"),
/* ER_BACKUP_NEXT_CHUNK */
N_("Can't go to the next chunk in backup stream"),
/* ER_BACKUP_INIT_BACKUP_DRIVER */
N_("Can't initialize %-.64s backup driver"),
/* ER_BACKUP_INIT_RESTORE_DRIVER */
N_("Can't initialize %-.64s restore driver"),
/* ER_BACKUP_STOP_BACKUP_DRIVER */
N_("Can't shut down %-.64s backup driver"),
/* ER_BACKUP_STOP_RESTORE_DRIVERS */
N_("Can't shut down %-.64s backup driver(s)"),
/* ER_BACKUP_PREPARE_DRIVER */
N_("%-.64s backup driver can't prepare for synchronization"),
/* ER_BACKUP_CREATE_VP */
N_("%-.64s backup driver can't create its image validity point"),
/* ER_BACKUP_UNLOCK_DRIVER */
N_("Can't unlock %-.64s backup driver after creating the validity point"),
/* ER_BACKUP_CANCEL_BACKUP */
N_("%-.64s backup driver can't cancel its backup operation"),
/* ER_BACKUP_CANCEL_RESTORE */
N_("%-.64s restore driver can't cancel its restore operation"),
/* ER_BACKUP_GET_DATA */
N_("Error when polling %-.64s backup driver for its image data"),
/* ER_BACKUP_SEND_DATA */
N_("Error when sending image data (for table #%d) to %-.64s restore driver"),
/* ER_BACKUP_SEND_DATA_RETRY */
N_("After %d attempts %-.64s restore driver still can't accept next block of data"),
/* ER_BACKUP_OPEN_TABLES */
N_("Open and lock tables failed in %-.64s"),
/* ER_BACKUP_THREAD_INIT */
N_("Backup driver's table locking thread can not be initialized."),
/* ER_BACKUP_PROGRESS_TABLES */
N_("Can't open the online backup progress tables. Check 'drizzle.online_backup' and 'drizzle.online_backup_progress'."),
/* ER_TABLESPACE_EXIST */
N_("Tablespace '%-.192s' already exists"),
/* ER_NO_SUCH_TABLESPACE */
N_("Tablespace '%-.192s' doesn't exist"),
/* ER_SLAVE_HEARTBEAT_FAILURE */
N_("Unexpected master's heartbeat data: %s"),
/* ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE */
N_("The requested value for the heartbeat period %s %s"),
/* ER_BACKUP_LOG_WRITE_ERROR */
N_("Can't write to the online backup progress log %-.64s."),
/* ER_TABLESPACE_NOT_EMPTY */
N_("Tablespace '%-.192s' not empty"),
/* ER_BACKUP_TS_CHANGE */
N_("Tablespace `%-.64s` needed by tables being restored has changed on the server. The original definition of the required tablespace is '%-.256s' while the same tablespace is defined on the server as '%-.256s'"),
/* ER_VCOL_BASED_ON_VCOL */
N_("A virtual column cannot be based on a virtual column"),
/* ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED */
N_("Non-deterministic expression for virtual column '%s'."),
/* ER_DATA_CONVERSION_ERROR_FOR_VIRTUAL_COLUMN */
N_("Generated value for virtual column '%s' cannot be converted to type '%s'."),
/* ER_PRIMARY_KEY_BASED_ON_VIRTUAL_COLUMN */
N_("Primary key cannot be defined upon a virtual column."),
/* ER_KEY_BASED_ON_GENERATED_VIRTUAL_COLUMN */
N_("Key/Index cannot be defined on a non-stored virtual column."),
/* ER_WRONG_FK_OPTION_FOR_VIRTUAL_COLUMN */
N_("Cannot define foreign key with %s clause on a virtual column."),
/* ER_WARNING_NON_DEFAULT_VALUE_FOR_VIRTUAL_COLUMN */
N_("The value specified for virtual column '%s' in table '%s' ignored."),
/* ER_UNSUPPORTED_ACTION_ON_VIRTUAL_COLUMN */
N_("'%s' is not yet supported for virtual columns."),
/* ER_CONST_EXPR_IN_VCOL */
N_("Constant expression in virtual column function is not allowed."),
/* ER_UNKNOWN_TEMPORAL_TYPE */
N_("Encountered an unknown temporal type."),
/* ER_INVALID_STRING_FORMAT_FOR_DATE */
N_("Received an invalid string format '%s' for a date value."),
/* ER_INVALID_STRING_FORMAT_FOR_TIME */
N_("Received an invalid string format '%s' for a time value."),
/* ER_INVALID_UNIX_TIMESTAMP_VALUE */
N_("Received an invalid value '%s' for a UNIX timestamp."),
/* ER_INVALID_DATETIME_VALUE */
N_("Received an invalid datetime value '%s'."),
/* ER_INVALID_NULL_ARGUMENT */
N_("Received a NULL argument for function '%s'."),
/* ER_INVALID_NEGATIVE_ARGUMENT */
N_("Received an invalid negative argument '%s' for function '%s'."),
/* ER_ARGUMENT_OUT_OF_RANGE */
N_("Received an out-of-range argument '%s' for function '%s'."),
/* ER_INVALID_TIME_VALUE */
N_("Received an invalid time value '%s'."),
};

const char * error_message(unsigned int code)
{
  return drizzled_error_messages[code-ER_ERROR_FIRST];
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
  init_glob_errs();                     /* Initiate english errors */

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

