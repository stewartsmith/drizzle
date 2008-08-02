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

#ifndef _drizzled_drizzled_error_messages
#define _drizzled_drizzled_error_messages


/*
 *   Errors a drizzled can give you
 *   */

static const char *drizzled_error_messages[]=
{
/* ER_HASHCHK   */
gettext_noop("hashchk"),
/* ER_NISAMCHK   */
gettext_noop("isamchk"),
/* ER_NO   */
gettext_noop("NO"),
/* ER_YES   */
gettext_noop("YES"),
/* ER_CANT_CREATE_FILE   */
gettext_noop("Can't create file '%-.200s' (errno: %d)"),
/* ER_CANT_CREATE_TABLE   */
gettext_noop("Can't create table '%-.200s' (errno: %d)"),
/* ER_CANT_CREATE_DB   */
gettext_noop("Can't create database '%-.192s' (errno: %d)"),
/* ER_DB_CREATE_EXISTS   */
gettext_noop("Can't create database '%-.192s'; database exists"),
/* ER_DB_DROP_EXISTS   */
gettext_noop("Can't drop database '%-.192s'; database doesn't exist"),
/* ER_DB_DROP_DELETE   */
gettext_noop("Error dropping database (can't delete '%-.192s', errno: %d)"),
/* ER_DB_DROP_RMDIR   */
gettext_noop("Error dropping database (can't rmdir '%-.192s', errno: %d)"),
/* ER_CANT_DELETE_FILE   */
gettext_noop("Error on delete of '%-.192s' (errno: %d)"),
/* ER_CANT_FIND_SYSTEM_REC   */
gettext_noop("Can't read record in system table"),
/* ER_CANT_GET_STAT   */
gettext_noop("Can't get status of '%-.200s' (errno: %d)"),
/* ER_CANT_GET_WD   */
gettext_noop("Can't get working directory (errno: %d)"),
/* ER_CANT_LOCK   */
gettext_noop("Can't lock file (errno: %d)"),
/* ER_CANT_OPEN_FILE   */
gettext_noop("Can't open file: '%-.200s' (errno: %d)"),
/* ER_FILE_NOT_FOUND   */
gettext_noop("Can't find file: '%-.200s' (errno: %d)"),
/* ER_CANT_READ_DIR   */
gettext_noop("Can't read dir of '%-.192s' (errno: %d)"),
/* ER_CANT_SET_WD   */
gettext_noop("Can't change dir to '%-.192s' (errno: %d)"),
/* ER_CHECKREAD   */
gettext_noop("Record has changed since last read in table '%-.192s'"),
/* ER_DISK_FULL   */
gettext_noop("Disk full (%s); waiting for someone to free some space..."),
/* ER_DUP_KEY 23000  */
gettext_noop("Can't write; duplicate key in table '%-.192s'"),
/* ER_ERROR_ON_CLOSE   */
gettext_noop("Error on close of '%-.192s' (errno: %d)"),
/* ER_ERROR_ON_READ   */
gettext_noop("Error reading file '%-.200s' (errno: %d)"),
/* ER_ERROR_ON_RENAME   */
gettext_noop("Error on rename of '%-.150s' to '%-.150s' (errno: %d)"),
/* ER_ERROR_ON_WRITE   */
gettext_noop("Error writing file '%-.200s' (errno: %d)"),
/* ER_FILE_USED   */
gettext_noop("'%-.192s' is locked against change"),
/* ER_FILSORT_ABORT   */
gettext_noop("Sort aborted"),
/* ER_FORM_NOT_FOUND   */
gettext_noop("View '%-.192s' doesn't exist for '%-.192s'"),
/* ER_GET_ERRNO   */
gettext_noop("Got error %d from storage engine"),
/* ER_ILLEGAL_HA   */
gettext_noop("Table storage engine for '%-.192s' doesn't have this option"),
/* ER_KEY_NOT_FOUND   */
gettext_noop("Can't find record in '%-.192s'"),
/* ER_NOT_FORM_FILE   */
gettext_noop("Incorrect information in file: '%-.200s'"),
/* ER_NOT_KEYFILE   */
gettext_noop("Incorrect key file for table '%-.200s'; try to repair it"),
/* ER_OLD_KEYFILE   */
gettext_noop("Old key file for table '%-.192s'; repair it!"),
/* ER_OPEN_AS_READONLY   */
gettext_noop("Table '%-.192s' is read only"),
/* ER_OUTOFMEMORY HY001 S1001 */
gettext_noop("Out of memory; restart server and try again (needed %d bytes)"),
/* ER_OUT_OF_SORTMEMORY HY001 S1001 */
gettext_noop("Out of sort memory; increase server sort buffer size"),
/* ER_UNEXPECTED_EOF   */
gettext_noop("Unexpected EOF found when reading file '%-.192s' (errno: %d)"),
/* ER_CON_COUNT_ERROR 08004  */
gettext_noop("Too many connections"),
/* ER_OUT_OF_RESOURCES   */
gettext_noop("Out of memory; check if mysqld or some other process uses all available memory; if not, you may have to use 'ulimit' to allow mysqld to use more memory or you can add more swap space"),
/* ER_BAD_HOST_ERROR 08S01  */
gettext_noop("Can't get hostname for your address"),
/* ER_HANDSHAKE_ERROR 08S01  */
gettext_noop("Bad handshake"),
/* ER_DBACCESS_DENIED_ERROR 42000  */
gettext_noop("Access denied for user '%-.48s'@'%-.64s' to database '%-.192s'"),
/* ER_ACCESS_DENIED_ERROR 28000  */
gettext_noop("Access denied for user '%-.48s'@'%-.64s' (using password: %s)"),
/* ER_NO_DB_ERROR 3D000  */
gettext_noop("No database selected"),
/* ER_UNKNOWN_COM_ERROR 08S01  */
gettext_noop("Unknown command"),
/* ER_BAD_NULL_ERROR 23000  */
gettext_noop("Column '%-.192s' cannot be null"),
/* ER_BAD_DB_ERROR 42000  */
gettext_noop("Unknown database '%-.192s'"),
/* ER_TABLE_EXISTS_ERROR 42S01  */
gettext_noop("Table '%-.192s' already exists"),
/* ER_BAD_TABLE_ERROR 42S02  */
gettext_noop("Unknown table '%-.100s'"),
/* ER_NON_UNIQ_ERROR 23000  */
gettext_noop("Column '%-.192s' in %-.192s is ambiguous"),
/* ER_SERVER_SHUTDOWN 08S01  */
gettext_noop("Server shutdown in progress"),
/* ER_BAD_FIELD_ERROR 42S22 S0022 */
gettext_noop("Unknown column '%-.192s' in '%-.192s'"),
/* ER_WRONG_FIELD_WITH_GROUP 42000 S1009 */
gettext_noop("'%-.192s' isn't in GROUP BY"),
/* ER_WRONG_GROUP_FIELD 42000 S1009 */
gettext_noop("Can't group on '%-.192s'"),
/* ER_WRONG_SUM_SELECT 42000 S1009 */
gettext_noop("Statement has sum functions and columns in same statement"),
/* ER_WRONG_VALUE_COUNT 21S01  */
gettext_noop("Column count doesn't match value count"),
/* ER_TOO_LONG_IDENT 42000 S1009 */
gettext_noop("Identifier name '%-.100s' is too long"),
/* ER_DUP_FIELDNAME 42S21 S1009 */
gettext_noop("Duplicate column name '%-.192s'"),
/* ER_DUP_KEYNAME 42000 S1009 */
gettext_noop("Duplicate key name '%-.192s'"),
/* ER_DUP_ENTRY 23000 S1009 */
gettext_noop("Duplicate entry '%-.192s' for key %d"),
/* ER_WRONG_FIELD_SPEC 42000 S1009 */
gettext_noop("Incorrect column specifier for column '%-.192s'"),
/* ER_PARSE_ERROR 42000 s1009 */
gettext_noop("%s near '%-.80s' at line %d"),
/* ER_EMPTY_QUERY 42000   */
gettext_noop("Query was empty"),
/* ER_NONUNIQ_TABLE 42000 S1009 */
gettext_noop("Not unique table/alias: '%-.192s'"),
/* ER_INVALID_DEFAULT 42000 S1009 */
gettext_noop("Invalid default value for '%-.192s'"),
/* ER_MULTIPLE_PRI_KEY 42000 S1009 */
gettext_noop("Multiple primary key defined"),
/* ER_TOO_MANY_KEYS 42000 S1009 */
gettext_noop("Too many keys specified; max %d keys allowed"),
/* ER_TOO_MANY_KEY_PARTS 42000 S1009 */
gettext_noop("Too many key parts specified; max %d parts allowed"),
/* ER_TOO_LONG_KEY 42000 S1009 */
gettext_noop("Specified key was too long; max key length is %d bytes"),
/* ER_KEY_COLUMN_DOES_NOT_EXITS 42000 S1009 */
gettext_noop("Key column '%-.192s' doesn't exist in table"),
/* ER_BLOB_USED_AS_KEY 42000 S1009 */
gettext_noop("BLOB column '%-.192s' can't be used in key specification with the used table type"),
/* ER_TOO_BIG_FIELDLENGTH 42000 S1009 */
gettext_noop("Column length too big for column '%-.192s' (max = %d); use BLOB or TEXT instead"),
/* ER_WRONG_AUTO_KEY 42000 S1009 */
gettext_noop("Incorrect table definition; there can be only one auto column and it must be defined as a key"),
/* ER_READY   */
gettext_noop("%s: ready for connections.\nVersion: '%s'  socket: '%s'  port: %d"),
/* ER_NORMAL_SHUTDOWN   */
gettext_noop("%s: Normal shutdown\n"),
/* ER_GOT_SIGNAL   */
gettext_noop("%s: Got signal %d. Aborting!\n"),
/* ER_SHUTDOWN_COMPLETE   */
gettext_noop("%s: Shutdown complete\n"),
/* ER_FORCING_CLOSE 08S01  */
gettext_noop("%s: Forcing close of thread %ld  user: '%-.48s'\n"),
/* ER_IPSOCK_ERROR 08S01  */
gettext_noop("Can't create IP socket"),
/* ER_NO_SUCH_INDEX 42S12 S1009 */
gettext_noop("Table '%-.192s' has no index like the one used in CREATE INDEX; recreate the table"),
/* ER_WRONG_FIELD_TERMINATORS 42000 S1009 */
gettext_noop("Field separator argument is not what is expected; check the manual"),
/* ER_BLOBS_AND_NO_TERMINATED 42000 S1009 */
gettext_noop("You can't use fixed rowlength with BLOBs; please use 'fields terminated by'"),
/* ER_TEXTFILE_NOT_READABLE   */
gettext_noop("The file '%-.128s' must be in the database directory or be readable by all"),
/* ER_FILE_EXISTS_ERROR   */
gettext_noop("File '%-.200s' already exists"),
/* ER_LOAD_INFO   */
gettext_noop("Records: %ld  Deleted: %ld  Skipped: %ld  Warnings: %ld"),
/* ER_ALTER_INFO   */
gettext_noop("Records: %ld  Duplicates: %ld"),
/* ER_WRONG_SUB_KEY   */
gettext_noop("Incorrect prefix key; the used key part isn't a string, the used length is longer than the key part, or the storage engine doesn't support unique prefix keys"),
/* ER_CANT_REMOVE_ALL_FIELDS 42000  */
gettext_noop("You can't delete all columns with ALTER TABLE; use DROP TABLE instead"),
/* ER_CANT_DROP_FIELD_OR_KEY 42000  */
gettext_noop("Can't DROP '%-.192s'; check that column/key exists"),
/* ER_INSERT_INFO   */
gettext_noop("Records: %ld  Duplicates: %ld  Warnings: %ld"),
/* ER_UPDATE_TABLE_USED   */
gettext_noop("You can't specify target table '%-.192s' for update in FROM clause"),
/* ER_NO_SUCH_THREAD   */
gettext_noop("Unknown thread id: %lu"),
/* ER_KILL_DENIED_ERROR   */
gettext_noop("You are not owner of thread %lu"),
/* ER_NO_TABLES_USED   */
gettext_noop("No tables used"),
/* ER_TOO_BIG_SET   */
gettext_noop("Too many strings for column %-.192s and SET"),
/* ER_NO_UNIQUE_LOGFILE   */
gettext_noop("Can't generate a unique log-filename %-.200s.(1-999)\n"),
/* ER_TABLE_NOT_LOCKED_FOR_WRITE   */
gettext_noop("Table '%-.192s' was locked with a READ lock and can't be updated"),
/* ER_TABLE_NOT_LOCKED   */
gettext_noop("Table '%-.192s' was not locked with LOCK TABLES"),
/* ER_BLOB_CANT_HAVE_DEFAULT 42000  */
gettext_noop("BLOB/TEXT column '%-.192s' can't have a default value"),
/* ER_WRONG_DB_NAME 42000  */
gettext_noop("Incorrect database name '%-.100s'"),
/* ER_WRONG_TABLE_NAME 42000  */
gettext_noop("Incorrect table name '%-.100s'"),
/* ER_TOO_BIG_SELECT 42000  */
gettext_noop("The SELECT would examine more than MAX_JOIN_SIZE rows; check your WHERE and use SET SQL_BIG_SELECTS=1 or SET SQL_MAX_JOIN_SIZE=# if the SELECT is okay"),
/* ER_UNKNOWN_ERROR   */
gettext_noop("Unknown error"),
/* ER_UNKNOWN_PROCEDURE 42000  */
gettext_noop("Unknown procedure '%-.192s'"),
/* ER_WRONG_PARAMCOUNT_TO_PROCEDURE 42000  */
gettext_noop("Incorrect parameter count to procedure '%-.192s'"),
/* ER_WRONG_PARAMETERS_TO_PROCEDURE   */
gettext_noop("Incorrect parameters to procedure '%-.192s'"),
/* ER_UNKNOWN_TABLE 42S02  */
gettext_noop("Unknown table '%-.192s' in %-.32s"),
/* ER_FIELD_SPECIFIED_TWICE 42000  */
gettext_noop("Column '%-.192s' specified twice"),
/* ER_INVALID_GROUP_FUNC_USE   */
gettext_noop("Invalid use of group function"),
/* ER_UNSUPPORTED_EXTENSION 42000  */
gettext_noop("Table '%-.192s' uses an extension that doesn't exist in this MySQL version"),
/* ER_TABLE_MUST_HAVE_COLUMNS 42000  */
gettext_noop("A table must have at least 1 column"),
/* ER_RECORD_FILE_FULL   */
gettext_noop("The table '%-.192s' is full"),
/* ER_UNKNOWN_CHARACTER_SET 42000  */
gettext_noop("Unknown character set: '%-.64s'"),
/* ER_TOO_MANY_TABLES   */
gettext_noop("Too many tables; MySQL can only use %d tables in a join"),
/* ER_TOO_MANY_FIELDS   */
gettext_noop("Too many columns"),
/* ER_TOO_BIG_ROWSIZE 42000  */
gettext_noop("Row size too large. The maximum row size for the used table type, not counting BLOBs, is %ld. You have to change some columns to TEXT or BLOBs"),
/* ER_STACK_OVERRUN   */
gettext_noop("Thread stack overrun:  Used: %ld of a %ld stack.  Use 'mysqld -O thread_stack=#' to specify a bigger stack if needed"),
/* ER_WRONG_OUTER_JOIN 42000  */
gettext_noop("Cross dependency found in OUTER JOIN; examine your ON conditions"),
/* ER_NULL_COLUMN_IN_INDEX 42000  */
gettext_noop("Table handler doesn't support NULL in given index. Please change column '%-.192s' to be NOT NULL or use another handler"),
/* ER_CANT_FIND_UDF   */
gettext_noop("Can't load function '%-.192s'"),
/* ER_CANT_INITIALIZE_UDF   */
gettext_noop("Can't initialize function '%-.192s'; %-.80s"),
/* ER_UDF_NO_PATHS   */
gettext_noop("No paths allowed for shared library"),
/* ER_UDF_EXISTS   */
gettext_noop("Function '%-.192s' already exists"),
/* ER_CANT_OPEN_LIBRARY   */
gettext_noop("Can't open shared library '%-.192s' (errno: %d %-.128s)"),
/* ER_CANT_FIND_DL_ENTRY */
gettext_noop("Can't find symbol '%-.128s' in library"),
/* ER_FUNCTION_NOT_DEFINED   */
gettext_noop("Function '%-.192s' is not defined"),
/* ER_HOST_IS_BLOCKED   */
gettext_noop("Host '%-.64s' is blocked because of many connection errors; unblock with 'mysqladmin flush-hosts'"),
/* ER_HOST_NOT_PRIVILEGED   */
gettext_noop("Host '%-.64s' is not allowed to connect to this MySQL server"),
/* ER_PASSWORD_ANONYMOUS_USER 42000  */
gettext_noop("You are using MySQL as an anonymous user and anonymous users are not allowed to change passwords"),
/* ER_PASSWORD_NOT_ALLOWED 42000  */
gettext_noop("You must have privileges to update tables in the mysql database to be able to change passwords for others"),
/* ER_PASSWORD_NO_MATCH 42000  */
gettext_noop("Can't find any matching row in the user table"),
/* ER_UPDATE_INFO   */
gettext_noop("Rows matched: %ld  Changed: %ld  Warnings: %ld"),
/* ER_CANT_CREATE_THREAD   */
gettext_noop("Can't create a new thread (errno %d); if you are not out of available memory, you can consult the manual for a possible OS-dependent bug"),
/* ER_WRONG_VALUE_COUNT_ON_ROW 21S01  */
gettext_noop("Column count doesn't match value count at row %ld"),
/* ER_CANT_REOPEN_TABLE   */
gettext_noop("Can't reopen table: '%-.192s'"),
/* ER_INVALID_USE_OF_NULL 22004  */
gettext_noop("Invalid use of NULL value"),
/* ER_REGEXP_ERROR 42000  */
gettext_noop("Got error '%-.64s' from regexp"),
/* ER_MIX_OF_GROUP_FUNC_AND_FIELDS 42000  */
gettext_noop("Mixing of GROUP columns (MIN(),MAX(),COUNT(),...) with no GROUP columns is illegal if there is no GROUP BY clause"),
/* ER_NONEXISTING_GRANT 42000  */
gettext_noop("There is no such grant defined for user '%-.48s' on host '%-.64s'"),
/* ER_TABLEACCESS_DENIED_ERROR 42000  */
gettext_noop("%-.16s command denied to user '%-.48s'@'%-.64s' for table '%-.192s'"),
/* ER_COLUMNACCESS_DENIED_ERROR 42000  */
gettext_noop("%-.16s command denied to user '%-.48s'@'%-.64s' for column '%-.192s' in table '%-.192s'"),
/* ER_ILLEGAL_GRANT_FOR_TABLE 42000  */
gettext_noop("Illegal GRANT/REVOKE command; please consult the manual to see which privileges can be used"),
/* ER_GRANT_WRONG_HOST_OR_USER 42000  */
gettext_noop("The host or user argument to GRANT is too long"),
/* ER_NO_SUCH_TABLE 42S02  */
gettext_noop("Table '%-.192s.%-.192s' doesn't exist"),
/* ER_NONEXISTING_TABLE_GRANT 42000  */
gettext_noop("There is no such grant defined for user '%-.48s' on host '%-.64s' on table '%-.192s'"),
/* ER_NOT_ALLOWED_COMMAND 42000  */
gettext_noop("The used command is not allowed with this MySQL version"),
/* ER_SYNTAX_ERROR 42000  */
gettext_noop("You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use"),
/* ER_DELAYED_CANT_CHANGE_LOCK   */
gettext_noop("Delayed insert thread couldn't get requested lock for table %-.192s"),
/* ER_TOO_MANY_DELAYED_THREADS   */
gettext_noop("Too many delayed threads in use"),
/* ER_ABORTING_CONNECTION 08S01  */
gettext_noop("Aborted connection %ld to db: '%-.192s' user: '%-.48s' (%-.64s)"),
/* ER_NET_PACKET_TOO_LARGE 08S01  */
gettext_noop("Got a packet bigger than 'max_allowed_packet' bytes"),
/* ER_NET_READ_ERROR_FROM_PIPE 08S01  */
gettext_noop("Got a read error from the connection pipe"),
/* ER_NET_FCNTL_ERROR 08S01  */
gettext_noop("Got an error from fcntl()"),
/* ER_NET_PACKETS_OUT_OF_ORDER 08S01  */
gettext_noop("Got packets out of order"),
/* ER_NET_UNCOMPRESS_ERROR 08S01  */
gettext_noop("Couldn't uncompress communication packet"),
/* ER_NET_READ_ERROR 08S01  */
gettext_noop("Got an error reading communication packets"),
/* ER_NET_READ_INTERRUPTED 08S01  */
gettext_noop("Got timeout reading communication packets"),
/* ER_NET_ERROR_ON_WRITE 08S01  */
gettext_noop("Got an error writing communication packets"),
/* ER_NET_WRITE_INTERRUPTED 08S01  */
gettext_noop("Got timeout writing communication packets"),
/* ER_TOO_LONG_STRING 42000  */
gettext_noop("Result string is longer than 'max_allowed_packet' bytes"),
/* ER_TABLE_CANT_HANDLE_BLOB 42000  */
gettext_noop("The used table type doesn't support BLOB/TEXT columns"),
/* ER_TABLE_CANT_HANDLE_AUTO_INCREMENT 42000  */
gettext_noop("The used table type doesn't support AUTO_INCREMENT columns"),
/* ER_DELAYED_INSERT_TABLE_LOCKED   */
gettext_noop("INSERT DELAYED can't be used with table '%-.192s' because it is locked with LOCK TABLES"),
/* ER_WRONG_COLUMN_NAME 42000  */
gettext_noop("Incorrect column name '%-.100s'"),
/* ER_WRONG_KEY_COLUMN 42000  */
gettext_noop("The used storage engine can't index column '%-.192s'"),
/* ER_WRONG_MRG_TABLE   */
gettext_noop("Unable to open underlying table which is differently defined or of non-MyISAM type or doesn't exist"),
/* ER_DUP_UNIQUE 23000  */
gettext_noop("Can't write, because of unique constraint, to table '%-.192s'"),
/* ER_BLOB_KEY_WITHOUT_LENGTH 42000  */
gettext_noop("BLOB/TEXT column '%-.192s' used in key specification without a key length"),
/* ER_PRIMARY_CANT_HAVE_NULL 42000  */
gettext_noop("All parts of a PRIMARY KEY must be NOT NULL; if you need NULL in a key, use UNIQUE instead"),
/* ER_TOO_MANY_ROWS 42000  */
gettext_noop("Result consisted of more than one row"),
/* ER_REQUIRES_PRIMARY_KEY 42000  */
gettext_noop("This table type requires a primary key"),
/* ER_NO_RAID_COMPILED   */
gettext_noop("This version of MySQL is not compiled with RAID support"),
/* ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE   */
gettext_noop("You are using safe update mode and you tried to update a table without a WHERE that uses a KEY column"),
/* ER_KEY_DOES_NOT_EXITS 42000 S1009 */
gettext_noop("Key '%-.192s' doesn't exist in table '%-.192s'"),
/* ER_CHECK_NO_SUCH_TABLE 42000  */
gettext_noop("Can't open table"),
/* ER_CHECK_NOT_IMPLEMENTED 42000  */
gettext_noop("The storage engine for the table doesn't support %s"),
/* ER_CANT_DO_THIS_DURING_AN_TRANSACTION 25000  */
gettext_noop("You are not allowed to execute this command in a transaction"),
/* ER_ERROR_DURING_COMMIT   */
gettext_noop("Got error %d during COMMIT"),
/* ER_ERROR_DURING_ROLLBACK   */
gettext_noop("Got error %d during ROLLBACK"),
/* ER_ERROR_DURING_FLUSH_LOGS   */
gettext_noop("Got error %d during FLUSH_LOGS"),
/* ER_ERROR_DURING_CHECKPOINT   */
gettext_noop("Got error %d during CHECKPOINT"),
/* ER_NEW_ABORTING_CONNECTION 08S01  */
gettext_noop("Aborted connection %ld to db: '%-.192s' user: '%-.48s' host: '%-.64s' (%-.64s)"),
/* ER_DUMP_NOT_IMPLEMENTED   */
gettext_noop("The storage engine for the table does not support binary table dump"),
/* ER_FLUSH_MASTER_BINLOG_CLOSED   */
gettext_noop("Binlog closed, cannot RESET MASTER"),
/* ER_INDEX_REBUILD   */
gettext_noop("Failed rebuilding the index of  dumped table '%-.192s'"),
/* ER_MASTER   */
gettext_noop("Error from master: '%-.64s'"),
/* ER_MASTER_NET_READ 08S01  */
gettext_noop("Net error reading from master"),
/* ER_MASTER_NET_WRITE 08S01  */
gettext_noop("Net error writing to master"),
/* ER_FT_MATCHING_KEY_NOT_FOUND   */
gettext_noop("Can't find FULLTEXT index matching the column list"),
/* ER_LOCK_OR_ACTIVE_TRANSACTION   */
gettext_noop("Can't execute the given command because you have active locked tables or an active transaction"),
/* ER_UNKNOWN_SYSTEM_VARIABLE   */
gettext_noop("Unknown system variable '%-.64s'"),
/* ER_CRASHED_ON_USAGE   */
gettext_noop("Table '%-.192s' is marked as crashed and should be repaired"),
/* ER_CRASHED_ON_REPAIR   */
gettext_noop("Table '%-.192s' is marked as crashed and last (automatic?) repair failed"),
/* ER_WARNING_NOT_COMPLETE_ROLLBACK   */
gettext_noop("Some non-transactional changed tables couldn't be rolled back"),
/* ER_TRANS_CACHE_FULL   */
gettext_noop("Multi-statement transaction required more than 'max_binlog_cache_size' bytes of storage; increase this mysqld variable and try again"),
/* ER_SLAVE_MUST_STOP   */
gettext_noop("This operation cannot be performed with a running slave; run STOP SLAVE first"),
/* ER_SLAVE_NOT_RUNNING   */
gettext_noop("This operation requires a running slave; configure slave and do START SLAVE"),
/* ER_BAD_SLAVE   */
gettext_noop("The server is not configured as slave; fix with CHANGE MASTER TO"),
/* ER_MASTER_INFO   */
gettext_noop("Could not initialize master info structure; more error messages can be found in the MySQL error log"),
/* ER_SLAVE_THREAD   */
gettext_noop("Could not create slave thread; check system resources"),
/* ER_TOO_MANY_USER_CONNECTIONS 42000  */
gettext_noop("User %-.64s already has more than 'max_user_connections' active connections"),
/* ER_SET_CONSTANTS_ONLY   */
gettext_noop("You may only use constant expressions with SET"),
/* ER_LOCK_WAIT_TIMEOUT   */
gettext_noop("Lock wait timeout exceeded; try restarting transaction"),
/* ER_LOCK_TABLE_FULL   */
gettext_noop("The total number of locks exceeds the lock table size"),
/* ER_READ_ONLY_TRANSACTION 25000  */
gettext_noop("Update locks cannot be acquired during a READ UNCOMMITTED transaction"),
/* ER_DROP_DB_WITH_READ_LOCK   */
gettext_noop("DROP DATABASE not allowed while thread is holding global read lock"),
/* ER_CREATE_DB_WITH_READ_LOCK   */
gettext_noop("CREATE DATABASE not allowed while thread is holding global read lock"),
/* ER_WRONG_ARGUMENTS   */
gettext_noop("Incorrect arguments to %s"),
/* ER_NO_PERMISSION_TO_CREATE_USER 42000  */
gettext_noop("'%-.48s'@'%-.64s' is not allowed to create new users"),
/* ER_UNION_TABLES_IN_DIFFERENT_DIR   */
gettext_noop("Incorrect table definition; all MERGE tables must be in the same database"),
/* ER_LOCK_DEADLOCK 40001  */
gettext_noop("Deadlock found when trying to get lock; try restarting transaction"),
/* ER_TABLE_CANT_HANDLE_FT   */
gettext_noop("The used table type doesn't support FULLTEXT indexes"),
/* ER_CANNOT_ADD_FOREIGN   */
gettext_noop("Cannot add foreign key constraint"),
/* ER_NO_REFERENCED_ROW 23000  */
gettext_noop("Cannot add or update a child row: a foreign key constraint fails"),
/* ER_ROW_IS_REFERENCED 23000  */
gettext_noop("Cannot delete or update a parent row: a foreign key constraint fails"),
/* ER_CONNECT_TO_MASTER 08S01  */
gettext_noop("Error connecting to master: %-.128s"),
/* ER_QUERY_ON_MASTER   */
gettext_noop("Error running query on master: %-.128s"),
/* ER_ERROR_WHEN_EXECUTING_COMMAND   */
gettext_noop("Error when executing command %s: %-.128s"),
/* ER_WRONG_USAGE   */
gettext_noop("Incorrect usage of %s and %s"),
/* ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT 21000  */
gettext_noop("The used SELECT statements have a different number of columns"),
/* ER_CANT_UPDATE_WITH_READLOCK   */
gettext_noop("Can't execute the query because you have a conflicting read lock"),
/* ER_MIXING_NOT_ALLOWED   */
gettext_noop("Mixing of transactional and non-transactional tables is disabled"),
/* ER_DUP_ARGUMENT   */
gettext_noop("Option '%s' used twice in statement"),
/* ER_USER_LIMIT_REACHED 42000  */
gettext_noop("User '%-.64s' has exceeded the '%s' resource (current value: %ld)"),
/* ER_SPECIFIC_ACCESS_DENIED_ERROR 42000  */
gettext_noop("Access denied; you need the %-.128s privilege for this operation"),
/* ER_LOCAL_VARIABLE   */
gettext_noop("Variable '%-.64s' is a SESSION variable and can't be used with SET GLOBAL"),
/* ER_GLOBAL_VARIABLE   */
gettext_noop("Variable '%-.64s' is a GLOBAL variable and should be set with SET GLOBAL"),
/* ER_NO_DEFAULT 42000  */
gettext_noop("Variable '%-.64s' doesn't have a default value"),
/* ER_WRONG_VALUE_FOR_VAR 42000  */
gettext_noop("Variable '%-.64s' can't be set to the value of '%-.200s'"),
/* ER_WRONG_TYPE_FOR_VAR 42000  */
gettext_noop("Incorrect argument type to variable '%-.64s'"),
/* ER_VAR_CANT_BE_READ   */
gettext_noop("Variable '%-.64s' can only be set, not read"),
/* ER_CANT_USE_OPTION_HERE 42000  */
gettext_noop("Incorrect usage/placement of '%s'"),
/* ER_NOT_SUPPORTED_YET 42000  */
gettext_noop("This version of MySQL doesn't yet support '%s'"),
/* ER_MASTER_FATAL_ERROR_READING_BINLOG   */
gettext_noop("Got fatal error %d: '%-.128s' from master when reading data from binary log"),
/* ER_SLAVE_IGNORED_TABLE   */
gettext_noop("Slave SQL thread ignored the query because of replicate-*-table rules"),
/* ER_INCORRECT_GLOBAL_LOCAL_VAR   */
gettext_noop("Variable '%-.192s' is a %s variable"),
/* ER_WRONG_FK_DEF 42000  */
gettext_noop("Incorrect foreign key definition for '%-.192s': %s"),
/* ER_KEY_REF_DO_NOT_MATCH_TABLE_REF   */
gettext_noop("Key reference and table reference don't match"),
/* ER_OPERAND_COLUMNS 21000  */
gettext_noop("Operand should contain %d column(s)"),
/* ER_SUBQUERY_NO_1_ROW 21000  */
gettext_noop("Subquery returns more than 1 row"),
/* ER_UNKNOWN_STMT_HANDLER   */
gettext_noop("Unknown prepared statement handler (%.*s) given to %s"),
/* ER_CORRUPT_HELP_DB   */
gettext_noop("Help database is corrupt or does not exist"),
/* ER_CYCLIC_REFERENCE   */
gettext_noop("Cyclic reference on subqueries"),
/* ER_AUTO_CONVERT   */
gettext_noop("Converting column '%s' from %s to %s"),
/* ER_ILLEGAL_REFERENCE 42S22  */
gettext_noop("Reference '%-.64s' not supported (%s)"),
/* ER_DERIVED_MUST_HAVE_ALIAS 42000  */
gettext_noop("Every derived table must have its own alias"),
/* ER_SELECT_REDUCED 01000  */
gettext_noop("Select %u was reduced during optimization"),
/* ER_TABLENAME_NOT_ALLOWED_HERE 42000  */
gettext_noop("Table '%-.192s' from one of the SELECTs cannot be used in %-.32s"),
/* ER_NOT_SUPPORTED_AUTH_MODE 08004  */
gettext_noop("Client does not support authentication protocol requested by server; consider upgrading MySQL client"),
/* ER_SPATIAL_CANT_HAVE_NULL 42000  */
gettext_noop("All parts of a SPATIAL index must be NOT NULL"),
/* ER_COLLATION_CHARSET_MISMATCH 42000  */
gettext_noop("COLLATION '%s' is not valid for CHARACTER SET '%s'"),
/* ER_SLAVE_WAS_RUNNING   */
gettext_noop("Slave is already running"),
/* ER_SLAVE_WAS_NOT_RUNNING   */
gettext_noop("Slave already has been stopped"),
/* ER_TOO_BIG_FOR_UNCOMPRESS   */
gettext_noop("Uncompressed data size too large; the maximum size is %d (probably, length of uncompressed data was corrupted)"),
/* ER_ZLIB_Z_MEM_ERROR   */
gettext_noop("ZLIB: Not enough memory"),
/* ER_ZLIB_Z_BUF_ERROR   */
gettext_noop("ZLIB: Not enough room in the output buffer (probably, length of uncompressed data was corrupted)"),
/* ER_ZLIB_Z_DATA_ERROR   */
gettext_noop("ZLIB: Input data corrupted"),
/* ER_CUT_VALUE_GROUP_CONCAT   */
gettext_noop("%d line(s) were cut by GROUP_CONCAT()"),
/* ER_WARN_TOO_FEW_RECORDS 01000  */
gettext_noop("Row %ld doesn't contain data for all columns"),
/* ER_WARN_TOO_MANY_RECORDS 01000  */
gettext_noop("Row %ld was truncated; it contained more data than there were input columns"),
/* ER_WARN_NULL_TO_NOTNULL 22004  */
gettext_noop("Column set to default value; NULL supplied to NOT NULL column '%s' at row %ld"),
/* ER_WARN_DATA_OUT_OF_RANGE 22003  */
gettext_noop("Out of range value for column '%s' at row %ld"),
/* WARN_DATA_TRUNCATED 01000  */
gettext_noop("Data truncated for column '%s' at row %ld"),
/* ER_WARN_USING_OTHER_HANDLER   */
gettext_noop("Using storage engine %s for table '%s'"),
/* ER_CANT_AGGREGATE_2COLLATIONS   */
gettext_noop("Illegal mix of collations (%s,%s) and (%s,%s) for operation '%s'"),
/* ER_DROP_USER   */
gettext_noop("Cannot drop one or more of the requested users"),
/* ER_REVOKE_GRANTS   */
gettext_noop("Can't revoke all privileges for one or more of the requested users"),
/* ER_CANT_AGGREGATE_3COLLATIONS   */
gettext_noop("Illegal mix of collations (%s,%s), (%s,%s), (%s,%s) for operation '%s'"),
/* ER_CANT_AGGREGATE_NCOLLATIONS   */
gettext_noop("Illegal mix of collations for operation '%s'"),
/* ER_VARIABLE_IS_NOT_STRUCT   */
gettext_noop("Variable '%-.64s' is not a variable component (can't be used as XXXX.variable_name)"),
/* ER_UNKNOWN_COLLATION   */
gettext_noop("Unknown collation: '%-.64s'"),
/* ER_SLAVE_IGNORED_SSL_PARAMS   */
gettext_noop("SSL parameters in CHANGE MASTER are ignored because this MySQL slave was compiled without SSL support; they can be used later if MySQL slave with SSL is started"),
/* ER_SERVER_IS_IN_SECURE_AUTH_MODE   */
gettext_noop("Server is running in --secure-auth mode, but '%s'@'%s' has a password in the old format; please change the password to the new format"),
/* ER_WARN_FIELD_RESOLVED   */
gettext_noop("Field or reference '%-.192s%s%-.192s%s%-.192s' of SELECT #%d was resolved in SELECT #%d"),
/* ER_BAD_SLAVE_UNTIL_COND   */
gettext_noop("Incorrect parameter or combination of parameters for START SLAVE UNTIL"),
/* ER_MISSING_SKIP_SLAVE   */
gettext_noop("It is recommended to use --skip-slave-start when doing step-by-step replication with START SLAVE UNTIL; otherwise, you will get problems if you get an unexpected slave's mysqld restart"),
/* ER_UNTIL_COND_IGNORED   */
gettext_noop("SQL thread is not to be started so UNTIL options are ignored"),
/* ER_WRONG_NAME_FOR_INDEX 42000  */
gettext_noop("Incorrect index name '%-.100s'"),
/* ER_WRONG_NAME_FOR_CATALOG 42000  */
gettext_noop("Incorrect catalog name '%-.100s'"),
/* ER_WARN_QC_RESIZE   */
gettext_noop("Query cache failed to set size %lu; new query cache size is %lu"),
/* ER_BAD_FT_COLUMN   */
gettext_noop("Column '%-.192s' cannot be part of FULLTEXT index"),
/* ER_UNKNOWN_KEY_CACHE   */
gettext_noop("Unknown key cache '%-.100s'"),
/* ER_WARN_HOSTNAME_WONT_WORK   */
gettext_noop("MySQL is started in --skip-name-resolve mode; you must restart it without this switch for this grant to work"),
/* ER_UNKNOWN_STORAGE_ENGINE 42000  */
gettext_noop("Unknown table engine '%s'"),
/* ER_WARN_DEPRECATED_SYNTAX   */
gettext_noop("'%s' is deprecated; use '%s' instead"),
/* ER_NON_UPDATABLE_TABLE   */
gettext_noop("The target table %-.100s of the %s is not updatable"),
/* ER_FEATURE_DISABLED   */
gettext_noop("The '%s' feature is disabled; you need MySQL built with '%s' to have it working"),
/* ER_OPTION_PREVENTS_STATEMENT   */
gettext_noop("The MySQL server is running with the %s option so it cannot execute this statement"),
/* ER_DUPLICATED_VALUE_IN_TYPE   */
gettext_noop("Column '%-.100s' has duplicated value '%-.64s' in %s"),
/* ER_TRUNCATED_WRONG_VALUE 22007  */
gettext_noop("Truncated incorrect %-.32s value: '%-.128s'"),
/* ER_TOO_MUCH_AUTO_TIMESTAMP_COLS   */
gettext_noop("Incorrect table definition; there can be only one TIMESTAMP column with CURRENT_TIMESTAMP in DEFAULT or ON UPDATE clause"),
/* ER_INVALID_ON_UPDATE   */
gettext_noop("Invalid ON UPDATE clause for '%-.192s' column"),
/* ER_UNSUPPORTED_PS   */
gettext_noop("This command is not supported in the prepared statement protocol yet"),
/* ER_GET_ERRMSG   */
gettext_noop("Got error %d '%-.100s' from %s"),
/* ER_GET_TEMPORARY_ERRMSG   */
gettext_noop("Got temporary error %d '%-.100s' from %s"),
/* ER_UNKNOWN_TIME_ZONE   */
gettext_noop("Unknown or incorrect time zone: '%-.64s'"),
/* ER_WARN_INVALID_TIMESTAMP   */
gettext_noop("Invalid TIMESTAMP value in column '%s' at row %ld"),
/* ER_INVALID_CHARACTER_STRING   */
gettext_noop("Invalid %s character string: '%.64s'"),
/* ER_WARN_ALLOWED_PACKET_OVERFLOWED   */
gettext_noop("Result of %s() was larger than max_allowed_packet (%ld) - truncated"),
/* ER_CONFLICTING_DECLARATIONS   */
gettext_noop("Conflicting declarations: '%s%s' and '%s%s'"),
/* ER_SP_NO_RECURSIVE_CREATE 2F003  */
gettext_noop("Can't create a %s from within another stored routine"),
/* ER_SP_ALREADY_EXISTS 42000  */
gettext_noop("%s %s already exists"),
/* ER_SP_DOES_NOT_EXIST 42000  */
gettext_noop("%s %s does not exist"),
/* ER_SP_DROP_FAILED   */
gettext_noop("Failed to DROP %s %s"),
/* ER_SP_STORE_FAILED   */
gettext_noop("Failed to CREATE %s %s"),
/* ER_SP_LILABEL_MISMATCH 42000  */
gettext_noop("%s with no matching label: %s"),
/* ER_SP_LABEL_REDEFINE 42000  */
gettext_noop("Redefining label %s"),
/* ER_SP_LABEL_MISMATCH 42000  */
gettext_noop("End-label %s without match"),
/* ER_SP_UNINIT_VAR 01000  */
gettext_noop("Referring to uninitialized variable %s"),
/* ER_SP_BADSELECT 0A000  */
gettext_noop("PROCEDURE %s can't return a result set in the given context"),
/* ER_SP_BADRETURN 42000  */
gettext_noop("RETURN is only allowed in a FUNCTION"),
/* ER_SP_BADSTATEMENT 0A000  */
gettext_noop("%s is not allowed in stored procedures"),
/* ER_UPDATE_LOG_DEPRECATED_IGNORED 42000  */
gettext_noop("The update log is deprecated and replaced by the binary log; SET SQL_LOG_UPDATE has been ignored"),
/* ER_UPDATE_LOG_DEPRECATED_TRANSLATED 42000  */
gettext_noop("The update log is deprecated and replaced by the binary log; SET SQL_LOG_UPDATE has been translated to SET SQL_LOG_BIN"),
/* ER_QUERY_INTERRUPTED 70100  */
gettext_noop("Query execution was interrupted"),
/* ER_SP_WRONG_NO_OF_ARGS 42000  */
gettext_noop("Incorrect number of arguments for %s %s; expected %u, got %u"),
/* ER_SP_COND_MISMATCH 42000  */
gettext_noop("Undefined CONDITION: %s"),
/* ER_SP_NORETURN 42000  */
gettext_noop("No RETURN found in FUNCTION %s"),
/* ER_SP_NORETURNEND 2F005  */
gettext_noop("FUNCTION %s ended without RETURN"),
/* ER_SP_BAD_CURSOR_QUERY 42000  */
gettext_noop("Cursor statement must be a SELECT"),
/* ER_SP_BAD_CURSOR_SELECT 42000  */
gettext_noop("Cursor SELECT must not have INTO"),
/* ER_SP_CURSOR_MISMATCH 42000  */
gettext_noop("Undefined CURSOR: %s"),
/* ER_SP_CURSOR_ALREADY_OPEN 24000  */
gettext_noop("Cursor is already open"),
/* ER_SP_CURSOR_NOT_OPEN 24000  */
gettext_noop("Cursor is not open"),
/* ER_SP_UNDECLARED_VAR 42000  */
gettext_noop("Undeclared variable: %s"),
/* ER_SP_WRONG_NO_OF_FETCH_ARGS   */
gettext_noop("Incorrect number of FETCH variables"),
/* ER_SP_FETCH_NO_DATA 02000  */
gettext_noop("No data - zero rows fetched, selected, or processed"),
/* ER_SP_DUP_PARAM 42000  */
gettext_noop("Duplicate parameter: %s"),
/* ER_SP_DUP_VAR 42000  */
gettext_noop("Duplicate variable: %s"),
/* ER_SP_DUP_COND 42000  */
gettext_noop("Duplicate condition: %s"),
/* ER_SP_DUP_CURS 42000  */
gettext_noop("Duplicate cursor: %s"),
/* ER_SP_CANT_ALTER   */
gettext_noop("Failed to ALTER %s %s"),
/* ER_SP_SUBSELECT_NYI 0A000  */
gettext_noop("Subquery value not supported"),
/* ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG 0A000 */
gettext_noop("%s is not allowed in stored function or trigger"),
/* ER_SP_VARCOND_AFTER_CURSHNDLR 42000  */
gettext_noop("Variable or condition declaration after cursor or handler declaration"),
/* ER_SP_CURSOR_AFTER_HANDLER 42000  */
gettext_noop("Cursor declaration after handler declaration"),
/* ER_SP_CASE_NOT_FOUND 20000  */
gettext_noop("Case not found for CASE statement"),
/* ER_FPARSER_TOO_BIG_FILE   */
gettext_noop("Configuration file '%-.192s' is too big"),
/* ER_FPARSER_BAD_HEADER   */
gettext_noop("Malformed file type header in file '%-.192s'"),
/* ER_FPARSER_EOF_IN_COMMENT   */
gettext_noop("Unexpected end of file while parsing comment '%-.200s'"),
/* ER_FPARSER_ERROR_IN_PARAMETER   */
gettext_noop("Error while parsing parameter '%-.192s' (line: '%-.192s')"),
/* ER_FPARSER_EOF_IN_UNKNOWN_PARAMETER   */
gettext_noop("Unexpected end of file while skipping unknown parameter '%-.192s'"),
/* ER_VIEW_NO_EXPLAIN   */
gettext_noop("EXPLAIN/SHOW can not be issued; lacking privileges for underlying table"),
/* ER_FRM_UNKNOWN_TYPE   */
gettext_noop("File '%-.192s' has unknown type '%-.64s' in its header"),
/* ER_WRONG_OBJECT   */
gettext_noop("'%-.192s.%-.192s' is not %s"),
/* ER_NONUPDATEABLE_COLUMN   */
gettext_noop("Column '%-.192s' is not updatable"),
/* ER_VIEW_SELECT_DERIVED   */
gettext_noop("View's SELECT contains a subquery in the FROM clause"),
/* ER_VIEW_SELECT_CLAUSE   */
gettext_noop("View's SELECT contains a '%s' clause"),
/* ER_VIEW_SELECT_VARIABLE   */
gettext_noop("View's SELECT contains a variable or parameter"),
/* ER_VIEW_SELECT_TMPTABLE   */
gettext_noop("View's SELECT refers to a temporary table '%-.192s'"),
/* ER_VIEW_WRONG_LIST   */
gettext_noop("View's SELECT and view's field list have different column counts"),
/* ER_WARN_VIEW_MERGE   */
gettext_noop("View merge algorithm can't be used here for now (assumed undefined algorithm)"),
/* ER_WARN_VIEW_WITHOUT_KEY   */
gettext_noop("View being updated does not have complete key of underlying table in it"),
/* ER_VIEW_INVALID   */
gettext_noop("View '%-.192s.%-.192s' references invalid table(s) or column(s) or function(s) or definer/invoker of view lack rights to use them"),
/* ER_SP_NO_DROP_SP   */
gettext_noop("Can't drop or alter a %s from within another stored routine"),
/* ER_SP_GOTO_IN_HNDLR   */
gettext_noop("GOTO is not allowed in a stored procedure handler"),
/* ER_TRG_ALREADY_EXISTS   */
gettext_noop("Trigger already exists"),
/* ER_TRG_DOES_NOT_EXIST   */
gettext_noop("Trigger does not exist"),
/* ER_TRG_ON_VIEW_OR_TEMP_TABLE   */
gettext_noop("Trigger's '%-.192s' is view or temporary table"),
/* ER_TRG_CANT_CHANGE_ROW   */
gettext_noop("Updating of %s row is not allowed in %strigger"),
/* ER_TRG_NO_SUCH_ROW_IN_TRG   */
gettext_noop("There is no %s row in %s trigger"),
/* ER_NO_DEFAULT_FOR_FIELD   */
gettext_noop("Field '%-.192s' doesn't have a default value"),
/* ER_DIVISION_BY_ZERO 22012  */
gettext_noop("Division by 0"),
/* ER_TRUNCATED_WRONG_VALUE_FOR_FIELD   */
gettext_noop("Incorrect %-.32s value: '%-.128s' for column '%.192s' at row %ld"),
/* ER_ILLEGAL_VALUE_FOR_TYPE 22007  */
gettext_noop("Illegal %s '%-.192s' value found during parsing"),
/* ER_VIEW_NONUPD_CHECK   */
gettext_noop("CHECK OPTION on non-updatable view '%-.192s.%-.192s'"),
/* ER_VIEW_CHECK_FAILED   */
gettext_noop("CHECK OPTION failed '%-.192s.%-.192s'"),
/* ER_PROCACCESS_DENIED_ERROR 42000  */
gettext_noop("%-.16s command denied to user '%-.48s'@'%-.64s' for routine '%-.192s'"),
/* ER_RELAY_LOG_FAIL   */
gettext_noop("Failed purging old relay logs: %s"),
/* ER_PASSWD_LENGTH   */
gettext_noop("Password hash should be a %d-digit hexadecimal number"),
/* ER_UNKNOWN_TARGET_BINLOG   */
gettext_noop("Target log not found in binlog index"),
/* ER_IO_ERR_LOG_INDEX_READ   */
gettext_noop("I/O error reading log index file"),
/* ER_BINLOG_PURGE_PROHIBITED   */
gettext_noop("Server configuration does not permit binlog purge"),
/* ER_FSEEK_FAIL   */
gettext_noop("Failed on fseek()"),
/* ER_BINLOG_PURGE_FATAL_ERR   */
gettext_noop("Fatal error during log purge"),
/* ER_LOG_IN_USE   */
gettext_noop("A purgeable log is in use, will not purge"),
/* ER_LOG_PURGE_UNKNOWN_ERR   */
gettext_noop("Unknown error during log purge"),
/* ER_RELAY_LOG_INIT   */
gettext_noop("Failed initializing relay log position: %s"),
/* ER_NO_BINARY_LOGGING   */
gettext_noop("You are not using binary logging"),
/* ER_RESERVED_SYNTAX   */
gettext_noop("The '%-.64s' syntax is reserved for purposes internal to the MySQL server"),
/* ER_WSAS_FAILED   */
gettext_noop("WSAStartup Failed"),
/* ER_DIFF_GROUPS_PROC   */
gettext_noop("Can't handle procedures with different groups yet"),
/* ER_NO_GROUP_FOR_PROC   */
gettext_noop("Select must have a group with this procedure"),
/* ER_ORDER_WITH_PROC   */
gettext_noop("Can't use ORDER clause with this procedure"),
/* ER_LOGGING_PROHIBIT_CHANGING_OF   */
gettext_noop("Binary logging and replication forbid changing the global server %s"),
/* ER_NO_FILE_MAPPING   */
gettext_noop("Can't map file: %-.200s, errno: %d"),
/* ER_WRONG_MAGIC   */
gettext_noop("Wrong magic in %-.64s"),
/* ER_PS_MANY_PARAM   */
gettext_noop("Prepared statement contains too many placeholders"),
/* ER_KEY_PART_0   */
gettext_noop("Key part '%-.192s' length cannot be 0"),
/* ER_VIEW_CHECKSUM   */
gettext_noop("View text checksum failed"),
/* ER_VIEW_MULTIUPDATE   */
gettext_noop("Can not modify more than one base table through a join view '%-.192s.%-.192s'"),
/* ER_VIEW_NO_INSERT_FIELD_LIST   */
gettext_noop("Can not insert into join view '%-.192s.%-.192s' without fields list"),
/* ER_VIEW_DELETE_MERGE_VIEW   */
gettext_noop("Can not delete from join view '%-.192s.%-.192s'"),
/* ER_CANNOT_USER   */
gettext_noop("Operation %s failed for %.256s"),
/* ER_XAER_NOTA XAE04 */
gettext_noop("XAER_NOTA: Unknown XID"),
/* ER_XAER_INVAL XAE05 */
gettext_noop("XAER_INVAL: Invalid arguments (or unsupported command)"),
/* ER_XAER_RMFAIL XAE07 */
gettext_noop("XAER_RMFAIL: The command cannot be executed when global transaction is in the  %.64s state"),
/* ER_XAER_OUTSIDE XAE09 */
gettext_noop("XAER_OUTSIDE: Some work is done outside global transaction"),
/* ER_XAER_RMERR XAE03 */
gettext_noop("XAER_RMERR: Fatal error occurred in the transaction branch - check your data for consistency"),
/* ER_XA_RBROLLBACK XA100 */
gettext_noop("XA_RBROLLBACK: Transaction branch was rolled back"),
/* ER_NONEXISTING_PROC_GRANT 42000  */
gettext_noop("There is no such grant defined for user '%-.48s' on host '%-.64s' on routine '%-.192s'"),
/* ER_PROC_AUTO_GRANT_FAIL */
gettext_noop("Failed to grant EXECUTE and ALTER ROUTINE privileges"),
/* ER_PROC_AUTO_REVOKE_FAIL */
gettext_noop("Failed to revoke all privileges to dropped routine"),
/* ER_DATA_TOO_LONG 22001 */
gettext_noop("Data too long for column '%s' at row %ld"),
/* ER_SP_BAD_SQLSTATE 42000 */
gettext_noop("Bad SQLSTATE: '%s'"),
/* ER_STARTUP */
gettext_noop("%s: ready for connections.\nVersion: '%s'  socket: '%s'  port: %d  %s"),
/* ER_LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR */
gettext_noop("Can't load value from file with fixed size rows to variable"),
/* ER_CANT_CREATE_USER_WITH_GRANT 42000 */
gettext_noop("You are not allowed to create a user with GRANT"),
/* ER_WRONG_VALUE_FOR_TYPE   */
gettext_noop("Incorrect %-.32s value: '%-.128s' for function %-.32s"),
/* ER_TABLE_DEF_CHANGED */
gettext_noop("Table definition has changed, please retry transaction"),
/* ER_SP_DUP_HANDLER 42000 */
gettext_noop("Duplicate handler declared in the same block"),
/* ER_SP_NOT_VAR_ARG 42000 */
gettext_noop("OUT or INOUT argument %d for routine %s is not a variable or NEW pseudo-variable in BEFORE trigger"),
/* ER_SP_NO_RETSET 0A000 */
gettext_noop("Not allowed to return a result set from a %s"),
/* ER_CANT_CREATE_GEOMETRY_OBJECT 22003  */
gettext_noop("Cannot get geometry object from data you send to the GEOMETRY field"),
/* ER_FAILED_ROUTINE_BREAK_BINLOG */
gettext_noop("A routine failed and has neither NO SQL nor READS SQL DATA in its declaration and binary logging is enabled; if non-transactional tables were updated, the binary log will miss their changes"),
/* ER_BINLOG_UNSAFE_ROUTINE */
gettext_noop("This function has none of DETERMINISTIC, NO SQL, or READS SQL DATA in its declaration and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)"),
/* ER_BINLOG_CREATE_ROUTINE_NEED_SUPER */
gettext_noop("You do not have the SUPER privilege and binary logging is enabled (you *might* want to use the less safe log_bin_trust_function_creators variable)"),
/* ER_EXEC_STMT_WITH_OPEN_CURSOR */
gettext_noop("You can't execute a prepared statement which has an open cursor associated with it. Reset the statement to re-execute it."),
/* ER_STMT_HAS_NO_OPEN_CURSOR */
gettext_noop("The statement (%lu) has no open cursor."),
/* ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG */
gettext_noop("Explicit or implicit commit is not allowed in stored function or trigger."),
/* ER_NO_DEFAULT_FOR_VIEW_FIELD */
gettext_noop("Field of view '%-.192s.%-.192s' underlying table doesn't have a default value"),
/* ER_SP_NO_RECURSION */
gettext_noop("Recursive stored functions and triggers are not allowed."),
/* ER_TOO_BIG_SCALE 42000 S1009 */
gettext_noop("Too big scale %d specified for column '%-.192s'. Maximum is %d."),
/* ER_TOO_BIG_PRECISION 42000 S1009 */
gettext_noop("Too big precision %d specified for column '%-.192s'. Maximum is %d."),
/* ER_M_BIGGER_THAN_D 42000 S1009 */
gettext_noop("For float(M,D), double(M,D) or decimal(M,D), M must be >= D (column '%-.192s')."),
/* ER_WRONG_LOCK_OF_SYSTEM_TABLE */
gettext_noop("You can't combine write-locking of system tables with other tables or lock types"),
/* ER_CONNECT_TO_FOREIGN_DATA_SOURCE */
gettext_noop("Unable to connect to foreign data source: %.64s"),
/* ER_QUERY_ON_FOREIGN_DATA_SOURCE */
gettext_noop("There was a problem processing the query on the foreign data source. Data source error: %-.64s"),
/* ER_FOREIGN_DATA_SOURCE_DOESNT_EXIST */
gettext_noop("The foreign data source you are trying to reference does not exist. Data source error:  %-.64s"),
/* ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE */
gettext_noop("Can't create federated table. The data source connection string '%-.64s' is not in the correct format"),
/* ER_FOREIGN_DATA_STRING_INVALID */
gettext_noop("The data source connection string '%-.64s' is not in the correct format"),
/* ER_CANT_CREATE_FEDERATED_TABLE   */
gettext_noop("Can't create federated table. Foreign data src error:  %-.64s"),
/* ER_TRG_IN_WRONG_SCHEMA   */
gettext_noop("Trigger in wrong schema"),
/* ER_STACK_OVERRUN_NEED_MORE */
gettext_noop("Thread stack overrun:  %ld bytes used of a %ld byte stack, and %ld bytes needed.  Use 'mysqld -O thread_stack=#' to specify a bigger stack."),
/* ER_TOO_LONG_BODY 42000 S1009 */
gettext_noop("Routine body for '%-.100s' is too long"),
/* ER_WARN_CANT_DROP_DEFAULT_KEYCACHE */
gettext_noop("Cannot drop default keycache"),
/* ER_TOO_BIG_DISPLAYWIDTH 42000 S1009 */
gettext_noop("Display width out of range for column '%-.192s' (max = %d)"),
/* ER_XAER_DUPID XAE08 */
gettext_noop("XAER_DUPID: The XID already exists"),
/* ER_DATETIME_FUNCTION_OVERFLOW 22008 */
gettext_noop("Datetime function: %-.32s field overflow"),
/* ER_CANT_UPDATE_USED_TABLE_IN_SF_OR_TRG */
gettext_noop("Can't update table '%-.192s' in stored function/trigger because it is already used by statement which invoked this stored function/trigger."),
/* ER_VIEW_PREVENT_UPDATE */
gettext_noop("The definition of table '%-.192s' prevents operation %.192s on table '%-.192s'."),
/* ER_PS_NO_RECURSION */
gettext_noop("The prepared statement contains a stored routine call that refers to that same statement. It's not allowed to execute a prepared statement in such a recursive manner"),
/* ER_SP_CANT_SET_AUTOCOMMIT */
gettext_noop("Not allowed to set autocommit from a stored function or trigger"),
/* ER_MALFORMED_DEFINER */
gettext_noop("Definer is not fully qualified"),
/* ER_VIEW_FRM_NO_USER */
gettext_noop("View '%-.192s'.'%-.192s' has no definer information (old table format). Current user is used as definer. Please recreate the view!"),
/* ER_VIEW_OTHER_USER */
gettext_noop("You need the SUPER privilege for creation view with '%-.192s'@'%-.192s' definer"),
/* ER_NO_SUCH_USER */
gettext_noop("The user specified as a definer ('%-.64s'@'%-.64s') does not exist"),
/* ER_FORBID_SCHEMA_CHANGE */
gettext_noop("Changing schema from '%-.192s' to '%-.192s' is not allowed."),
/* ER_ROW_IS_REFERENCED_2 23000 */
gettext_noop("Cannot delete or update a parent row: a foreign key constraint fails (%.192s)"),
/* ER_NO_REFERENCED_ROW_2 23000 */
gettext_noop("Cannot add or update a child row: a foreign key constraint fails (%.192s)"),
/* ER_SP_BAD_VAR_SHADOW 42000 */
gettext_noop("Variable '%-.64s' must be quoted with `...`, or renamed"),
/* ER_TRG_NO_DEFINER */
gettext_noop("No definer attribute for trigger '%-.192s'.'%-.192s'. The trigger will be activated under the authorization of the invoker, which may have insufficient privileges. Please recreate the trigger."),
/* ER_OLD_FILE_FORMAT */
gettext_noop("'%-.192s' has an old format, you should re-create the '%s' object(s)"),
/* ER_SP_RECURSION_LIMIT */
gettext_noop("Recursive limit %d (as set by the max_sp_recursion_depth variable) was exceeded for routine %.192s"),
/* ER_SP_PROC_TABLE_CORRUPT */
gettext_noop("Failed to load routine %-.192s. The table mysql.proc is missing, corrupt, or contains bad data (internal code %d)"),
/* ER_SP_WRONG_NAME 42000 */
gettext_noop("Incorrect routine name '%-.192s'"),
/* ER_TABLE_NEEDS_UPGRADE */
gettext_noop("Table upgrade required. Please do \"REPAIR TABLE `%-.32s`\" to fix it!"),
/* ER_SP_NO_AGGREGATE 42000 */
gettext_noop("AGGREGATE is not supported for stored functions"),
/* ER_MAX_PREPARED_STMT_COUNT_REACHED 42000 */
gettext_noop("Can't create more than max_prepared_stmt_count statements (current value: %lu)"),
/* ER_VIEW_RECURSIVE */
gettext_noop("`%-.192s`.`%-.192s` contains view recursion"),
/* ER_NON_GROUPING_FIELD_USED 42000 */
gettext_noop("non-grouping field '%-.192s' is used in %-.64s clause"),
/* ER_TABLE_CANT_HANDLE_SPKEYS */
gettext_noop("The used table type doesn't support SPATIAL indexes"),
/* ER_NO_TRIGGERS_ON_SYSTEM_SCHEMA */
gettext_noop("Triggers can not be created on system tables"),
/* ER_REMOVED_SPACES */
gettext_noop("Leading spaces are removed from name '%s'"),
/* ER_AUTOINC_READ_FAILED */
gettext_noop("Failed to read auto-increment value from storage engine"),
/* ER_USERNAME */
gettext_noop("user name"),
/* ER_HOSTNAME */
gettext_noop("host name"),
/* ER_WRONG_STRING_LENGTH */
gettext_noop("String '%-.70s' is too long for %s (should be no longer than %d)"),
/* ER_NON_INSERTABLE_TABLE   */
gettext_noop("The target table %-.100s of the %s is not insertable-into"),
/* ER_ADMIN_WRONG_MRG_TABLE */
gettext_noop("Table '%-.64s' is differently defined or of non-MyISAM type or doesn't exist"),
/* ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT */
gettext_noop("Too high level of nesting for select"),
/* ER_NAME_BECOMES_EMPTY */
gettext_noop("Name '%-.64s' has become ''"),
/* ER_AMBIGUOUS_FIELD_TERM */
gettext_noop("First character of the FIELDS TERMINATED string is ambiguous; please use non-optional and non-empty FIELDS ENCLOSED BY"),
/* ER_FOREIGN_SERVER_EXISTS */
gettext_noop("The foreign server, %s, you are trying to create already exists."),
/* ER_FOREIGN_SERVER_DOESNT_EXIST */
gettext_noop("The foreign server name you are trying to reference does not exist. Data source error:  %-.64s"),
/* ER_ILLEGAL_HA_CREATE_OPTION */
gettext_noop("Table storage engine '%-.64s' does not support the create option '%.64s'"),
/* ER_PARTITION_REQUIRES_VALUES_ERROR */
gettext_noop("Syntax error: %-.64s PARTITIONING requires definition of VALUES %-.64s for each partition"),
/* ER_PARTITION_WRONG_VALUES_ERROR */
gettext_noop("Only %-.64s PARTITIONING can use VALUES %-.64s in partition definition"),
/* ER_PARTITION_MAXVALUE_ERROR */
gettext_noop("MAXVALUE can only be used in last partition definition"),
/* ER_PARTITION_SUBPARTITION_ERROR */
gettext_noop("Subpartitions can only be hash partitions and by key"),
/* ER_PARTITION_SUBPART_MIX_ERROR */
gettext_noop("Must define subpartitions on all partitions if on one partition"),
/* ER_PARTITION_WRONG_NO_PART_ERROR */
gettext_noop("Wrong number of partitions defined, mismatch with previous setting"),
/* ER_PARTITION_WRONG_NO_SUBPART_ERROR */
gettext_noop("Wrong number of subpartitions defined, mismatch with previous setting"),
/* ER_CONST_EXPR_IN_PARTITION_FUNC_ERROR */
gettext_noop("Constant/Random expression in (sub)partitioning function is not allowed"),
/* ER_NO_CONST_EXPR_IN_RANGE_OR_LIST_ERROR */
gettext_noop("Expression in RANGE/LIST VALUES must be constant"),
/* ER_FIELD_NOT_FOUND_PART_ERROR */
gettext_noop("Field in list of fields for partition function not found in table"),
/* ER_LIST_OF_FIELDS_ONLY_IN_HASH_ERROR */
gettext_noop("List of fields is only allowed in KEY partitions"),
/* ER_INCONSISTENT_PARTITION_INFO_ERROR */
gettext_noop("The partition info in the frm file is not consistent with what can be written into the frm file"),
/* ER_PARTITION_FUNC_NOT_ALLOWED_ERROR */
gettext_noop("The %-.192s function returns the wrong type"),
/* ER_PARTITIONS_MUST_BE_DEFINED_ERROR */
gettext_noop("For %-.64s partitions each partition must be defined"),
/* ER_RANGE_NOT_INCREASING_ERROR */
gettext_noop("VALUES LESS THAN value must be strictly increasing for each partition"),
/* ER_INCONSISTENT_TYPE_OF_FUNCTIONS_ERROR */
gettext_noop("VALUES value must be of same type as partition function"),
/* ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR */
gettext_noop("Multiple definition of same constant in list partitioning"),
/* ER_PARTITION_ENTRY_ERROR */
gettext_noop("Partitioning can not be used stand-alone in query"),
/* ER_MIX_HANDLER_ERROR */
gettext_noop("The mix of handlers in the partitions is not allowed in this version of MySQL"),
/* ER_PARTITION_NOT_DEFINED_ERROR */
gettext_noop("For the partitioned engine it is necessary to define all %-.64s"),
/* ER_TOO_MANY_PARTITIONS_ERROR */
gettext_noop("Too many partitions (including subpartitions) were defined"),
/* ER_SUBPARTITION_ERROR */
gettext_noop("It is only possible to mix RANGE/LIST partitioning with HASH/KEY partitioning for subpartitioning"),
/* ER_CANT_CREATE_HANDLER_FILE */
gettext_noop("Failed to create specific handler file"),
/* ER_BLOB_FIELD_IN_PART_FUNC_ERROR */
gettext_noop("A BLOB field is not allowed in partition function"),
/* ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF */
gettext_noop("A %-.192s must include all columns in the table's partitioning function"),
/* ER_NO_PARTS_ERROR */
gettext_noop("Number of %-.64s = 0 is not an allowed value"),
/* ER_PARTITION_MGMT_ON_NONPARTITIONED */
gettext_noop("Partition management on a not partitioned table is not possible"),
/* ER_FOREIGN_KEY_ON_PARTITIONED */
gettext_noop("Foreign key condition is not yet supported in conjunction with partitioning"),
/* ER_DROP_PARTITION_NON_EXISTENT */
gettext_noop("Error in list of partitions to %-.64s"),
/* ER_DROP_LAST_PARTITION */
gettext_noop("Cannot remove all partitions, use DROP TABLE instead"),
/* ER_COALESCE_ONLY_ON_HASH_PARTITION */
gettext_noop("COALESCE PARTITION can only be used on HASH/KEY partitions"),
/* ER_REORG_HASH_ONLY_ON_SAME_NO */
gettext_noop("REORGANISE PARTITION can only be used to reorganise partitions not to change their numbers"),
/* ER_REORG_NO_PARAM_ERROR */
gettext_noop("REORGANISE PARTITION without parameters can only be used on auto-partitioned tables using HASH PARTITIONs"),
/* ER_ONLY_ON_RANGE_LIST_PARTITION */
gettext_noop("%-.64s PARTITION can only be used on RANGE/LIST partitions"),
/* ER_ADD_PARTITION_SUBPART_ERROR */
gettext_noop("Trying to Add partition(s) with wrong number of subpartitions"),
/* ER_ADD_PARTITION_NO_NEW_PARTITION */
gettext_noop("At least one partition must be added"),
/* ER_COALESCE_PARTITION_NO_PARTITION */
gettext_noop("At least one partition must be coalesced"),
/* ER_REORG_PARTITION_NOT_EXIST */
gettext_noop("More partitions to reorganise than there are partitions"),
/* ER_SAME_NAME_PARTITION */
gettext_noop("Duplicate partition name %-.192s"),
/* ER_NO_BINLOG_ERROR */
gettext_noop("It is not allowed to shut off binlog on this command"),
/* ER_CONSECUTIVE_REORG_PARTITIONS */
gettext_noop("When reorganising a set of partitions they must be in consecutive order"),
/* ER_REORG_OUTSIDE_RANGE */
gettext_noop("Reorganize of range partitions cannot change total ranges except for last partition where it can extend the range"),
/* ER_PARTITION_FUNCTION_FAILURE */
gettext_noop("Partition function not supported in this version for this handler"),
/* ER_PART_STATE_ERROR */
gettext_noop("Partition state cannot be defined from CREATE/ALTER TABLE"),
/* ER_LIMITED_PART_RANGE */
gettext_noop("The %-.64s handler only supports 32 bit integers in VALUES"),
/* ER_PLUGIN_IS_NOT_LOADED */
gettext_noop("Plugin '%-.192s' is not loaded"),
/* ER_WRONG_VALUE */
gettext_noop("Incorrect %-.32s value: '%-.128s'"),
/* ER_NO_PARTITION_FOR_GIVEN_VALUE */
gettext_noop("Table has no partition for value %-.64s"),
/* ER_FILEGROUP_OPTION_ONLY_ONCE */
gettext_noop("It is not allowed to specify %s more than once"),
/* ER_CREATE_FILEGROUP_FAILED */
gettext_noop("Failed to create %s"),
/* ER_DROP_FILEGROUP_FAILED */
gettext_noop("Failed to drop %s"),
/* ER_TABLESPACE_AUTO_EXTEND_ERROR */
gettext_noop("The handler doesn't support autoextend of tablespaces"),
/* ER_WRONG_SIZE_NUMBER */
gettext_noop("A size parameter was incorrectly specified, either number or on the form 10M"),
/* ER_SIZE_OVERFLOW_ERROR */
gettext_noop("The size number was correct but we don't allow the digit part to be more than 2 billion"),
/* ER_ALTER_FILEGROUP_FAILED */
gettext_noop("Failed to alter: %s"),
/* ER_BINLOG_ROW_LOGGING_FAILED */
gettext_noop("Writing one row to the row-based binary log failed"),
/* ER_BINLOG_ROW_WRONG_TABLE_DEF */
gettext_noop("Table definition on master and slave does not match: %s"),
/* ER_BINLOG_ROW_RBR_TO_SBR */
gettext_noop("Slave running with --log-slave-updates must use row-based binary logging to be able to replicate row-based binary log events"),
/* ER_EVENT_ALREADY_EXISTS */
gettext_noop("Event '%-.192s' already exists"),
/* ER_EVENT_STORE_FAILED */
gettext_noop("Failed to store event %s. Error code %d from storage engine."),
/* ER_EVENT_DOES_NOT_EXIST */
gettext_noop("Unknown event '%-.192s'"),
/* ER_EVENT_CANT_ALTER */
gettext_noop("Failed to alter event '%-.192s'"),
/* ER_EVENT_DROP_FAILED */
gettext_noop("Failed to drop %s"),
/* ER_EVENT_INTERVAL_NOT_POSITIVE_OR_TOO_BIG */
gettext_noop("INTERVAL is either not positive or too big"),
/* ER_EVENT_ENDS_BEFORE_STARTS */
gettext_noop("ENDS is either invalid or before STARTS"),
/* ER_EVENT_EXEC_TIME_IN_THE_PAST */
gettext_noop("Event execution time is in the past. Event has been disabled"),
/* ER_EVENT_OPEN_TABLE_FAILED */
gettext_noop("Failed to open mysql.event"),
/* ER_EVENT_NEITHER_M_EXPR_NOR_M_AT */
gettext_noop("No datetime expression provided"),
/* ER_COL_COUNT_DOESNT_MATCH_CORRUPTED */
gettext_noop("Column count of mysql.%s is wrong. Expected %d, found %d. The table is probably corrupted"),
/* ER_CANNOT_LOAD_FROM_TABLE */
gettext_noop("Cannot load from mysql.%s. The table is probably corrupted"),
/* ER_EVENT_CANNOT_DELETE */
gettext_noop("Failed to delete the event from mysql.event"),
/* ER_EVENT_COMPILE_ERROR */
gettext_noop("Error during compilation of event's body"),
/* ER_EVENT_SAME_NAME */
gettext_noop("Same old and new event name"),
/* ER_EVENT_DATA_TOO_LONG */
gettext_noop("Data for column '%s' too long"),
/* ER_DROP_INDEX_FK */
gettext_noop("Cannot drop index '%-.192s': needed in a foreign key constraint"),
/* ER_WARN_DEPRECATED_SYNTAX_WITH_VER   */
gettext_noop("The syntax '%s' is deprecated and will be removed in MySQL %s. Please use %s instead"),
/* ER_CANT_WRITE_LOCK_LOG_TABLE */
gettext_noop("You can't write-lock a log table. Only read access is possible"),
/* ER_CANT_LOCK_LOG_TABLE */
gettext_noop("You can't use locks with log tables."),
/* ER_FOREIGN_DUPLICATE_KEY 23000 S1009 */
gettext_noop("Upholding foreign key constraints for table '%.192s', entry '%-.192s', key %d would lead to a duplicate entry"),
/* ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE */
gettext_noop("Column count of mysql.%s is wrong. Expected %d, found %d. Created with MySQL %d, now running %d. Please use mysql_upgrade to fix this error."),
/* ER_TEMP_TABLE_PREVENTS_SWITCH_OUT_OF_RBR */
gettext_noop("Cannot switch out of the row-based binary log format when the session has open temporary tables"),
/* ER_STORED_FUNCTION_PREVENTS_SWITCH_BINLOG_FORMAT */
gettext_noop("Cannot change the binary logging format inside a stored function or trigger"),
/* ER_NDB_CANT_SWITCH_BINLOG_FORMAT */
gettext_noop("The NDB cluster engine does not support changing the binlog format on the fly yet"),
/* ER_PARTITION_NO_TEMPORARY */
gettext_noop("Cannot create temporary table with partitions"),
/* ER_PARTITION_CONST_DOMAIN_ERROR */
gettext_noop("Partition constant is out of partition function domain"),
/* ER_PARTITION_FUNCTION_IS_NOT_ALLOWED */
gettext_noop("This partition function is not allowed"),
/* ER_DDL_LOG_ERROR */
gettext_noop("Error in DDL log"),
/* ER_NULL_IN_VALUES_LESS_THAN */
gettext_noop("Not allowed to use NULL value in VALUES LESS THAN"),
/* ER_WRONG_PARTITION_NAME */
gettext_noop("Incorrect partition name"),
/* ER_CANT_CHANGE_TX_ISOLATION 25001 */
gettext_noop("Transaction isolation level can't be changed while a transaction is in progress"),
/* ER_DUP_ENTRY_AUTOINCREMENT_CASE */
gettext_noop("ALTER TABLE causes auto_increment resequencing, resulting in duplicate entry '%-.192s' for key '%-.192s'"),
/* ER_EVENT_MODIFY_QUEUE_ERROR */
gettext_noop("Internal scheduler error %d"),
/* ER_EVENT_SET_VAR_ERROR */
gettext_noop("Error during starting/stopping of the scheduler. Error code %u"),
/* ER_PARTITION_MERGE_ERROR */
gettext_noop("Engine cannot be used in partitioned tables"),
/* ER_CANT_ACTIVATE_LOG */
gettext_noop("Cannot activate '%-.64s' log"),
/* ER_RBR_NOT_AVAILABLE */
gettext_noop("The server was not built with row-based replication"),
/* ER_BASE64_DECODE_ERROR */
gettext_noop("Decoding of base64 string failed"),
/* ER_EVENT_RECURSION_FORBIDDEN */
gettext_noop("Recursion of EVENT DDL statements is forbidden when body is present"),
/* ER_EVENTS_DB_ERROR */
gettext_noop("Cannot proceed because system tables used by Event Scheduler were found damaged at server start"),
/* ER_ONLY_INTEGERS_ALLOWED */
gettext_noop("Only integers allowed as number here"),
/* ER_UNSUPORTED_LOG_ENGINE */
gettext_noop("This storage engine cannot be used for log tables"),
/* ER_BAD_LOG_STATEMENT */
gettext_noop("You cannot '%s' a log table if logging is enabled"),
/* ER_CANT_RENAME_LOG_TABLE */
gettext_noop("Cannot rename '%s'. When logging enabled, rename to/from log table must rename two tables: the log table to an archive table and another table back to '%s'"),
/* ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT 42000 */
gettext_noop("Incorrect parameter count in the call to native function '%-.192s'"),
/* ER_WRONG_PARAMETERS_TO_NATIVE_FCT 42000 */
gettext_noop("Incorrect parameters in the call to native function '%-.192s'"),
/* ER_WRONG_PARAMETERS_TO_STORED_FCT 42000   */
gettext_noop("Incorrect parameters in the call to stored function '%-.192s'"),
/* ER_NATIVE_FCT_NAME_COLLISION */
gettext_noop("This function '%-.192s' has the same name as a native function"),
/* ER_DUP_ENTRY_WITH_KEY_NAME 23000 S1009 */
gettext_noop("Duplicate entry '%-.64s' for key '%-.192s'"),
/* ER_BINLOG_PURGE_EMFILE */
gettext_noop("Too many files opened, please execute the command again"),
/* ER_EVENT_CANNOT_CREATE_IN_THE_PAST */
gettext_noop("Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation."),
/* ER_EVENT_CANNOT_ALTER_IN_THE_PAST */
gettext_noop("Event execution time is in the past and ON COMPLETION NOT PRESERVE is set. The event was dropped immediately after creation."),
/* ER_SLAVE_INCIDENT */
gettext_noop("The incident %s occured on the master. Message: %-.64s"),
/* ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT */
gettext_noop("Table has no partition for some existing values"),
/* ER_BINLOG_UNSAFE_STATEMENT */
gettext_noop("Statement is not safe to log in statement format."),
/* ER_SLAVE_FATAL_ERROR */
gettext_noop("Fatal error: %s"),
/* ER_SLAVE_RELAY_LOG_READ_FAILURE */
gettext_noop("Relay log read failure: %s"),
/* ER_SLAVE_RELAY_LOG_WRITE_FAILURE */
gettext_noop("Relay log write failure: %s"),
/* ER_SLAVE_CREATE_EVENT_FAILURE */
gettext_noop("Failed to create %s"),
/* ER_SLAVE_MASTER_COM_FAILURE */
gettext_noop("Master command %s failed: %s"),
/* ER_BINLOG_LOGGING_IMPOSSIBLE */
gettext_noop("Binary logging not possible. Message: %s"),
/* ER_VIEW_NO_CREATION_CTX */
gettext_noop("View `%-.64s`.`%-.64s` has no creation context"),
/* ER_VIEW_INVALID_CREATION_CTX */
gettext_noop("Creation context of view `%-.64s`.`%-.64s' is invalid"),
/* ER_SR_INVALID_CREATION_CTX */
gettext_noop("Creation context of stored routine `%-.64s`.`%-.64s` is invalid"),
/* ER_TRG_CORRUPTED_FILE */
gettext_noop("Corrupted TRG file for table `%-.64s`.`%-.64s`"),
/* ER_TRG_NO_CREATION_CTX */
gettext_noop("Triggers for table `%-.64s`.`%-.64s` have no creation context"),
/* ER_TRG_INVALID_CREATION_CTX */
gettext_noop("Trigger creation context of table `%-.64s`.`%-.64s` is invalid"),
/* ER_EVENT_INVALID_CREATION_CTX */
gettext_noop("Creation context of event `%-.64s`.`%-.64s` is invalid"),
/* ER_TRG_CANT_OPEN_TABLE */
gettext_noop("Cannot open table for trigger `%-.64s`.`%-.64s`"),
/* ER_CANT_CREATE_SROUTINE */
gettext_noop("Cannot create stored routine `%-.64s`. Check warnings"),
/* ER_SLAVE_AMBIGOUS_EXEC_MODE */
gettext_noop("Ambiguous slave modes combination. %s"),
/* ER_NO_FORMAT_DESCRIPTION_EVENT_BEFORE_BINLOG_STATEMENT */
gettext_noop("The BINLOG statement of type `%s` was not preceded by a format description BINLOG statement."),
/* ER_SLAVE_CORRUPT_EVENT */
gettext_noop("Corrupted replication event was detected"),
/* ER_LOAD_DATA_INVALID_COLUMN */
gettext_noop("Invalid column reference (%-.64s) in LOAD DATA"),
/* ER_LOG_PURGE_NO_FILE   */
gettext_noop("Being purged log %s was not found"),
/* ER_WARN_AUTO_CONVERT_LOCK */
gettext_noop("Converted to non-transactional lock on '%-.64s'"),
/* ER_NO_AUTO_CONVERT_LOCK_STRICT */
gettext_noop("Cannot convert to non-transactional lock in strict mode on '%-.64s'"),
/* ER_NO_AUTO_CONVERT_LOCK_TRANSACTION */
gettext_noop("Cannot convert to non-transactional lock in an active transaction on '%-.64s'"),
/* ER_NO_STORAGE_ENGINE */
gettext_noop("Can't access storage engine of table %-.64s"),
/* ER_BACKUP_BACKUP_START */
gettext_noop("Starting backup process"),
/* ER_BACKUP_BACKUP_DONE */
gettext_noop("Backup completed"),
/* ER_BACKUP_RESTORE_START */
gettext_noop("Starting restore process"),
/* ER_BACKUP_RESTORE_DONE */
gettext_noop("Restore completed"),
/* ER_BACKUP_NOTHING_TO_BACKUP */
gettext_noop("Nothing to backup"),
/* ER_BACKUP_CANNOT_INCLUDE_DB */
gettext_noop("Database '%-.64s' cannot be included in a backup"),
/* ER_BACKUP_BACKUP */
gettext_noop("Error during backup operation - server's error log contains more information about the error"),
/* ER_BACKUP_RESTORE */
gettext_noop("Error during restore operation - server's error log contains more information about the error"),
/* ER_BACKUP_RUNNING */
gettext_noop("Can't execute this command because another BACKUP/RESTORE operation is in progress"),
/* ER_BACKUP_BACKUP_PREPARE */
gettext_noop("Error when preparing for backup operation"),
/* ER_BACKUP_RESTORE_PREPARE */
gettext_noop("Error when preparing for restore operation"),
/* ER_BACKUP_INVALID_LOC */
gettext_noop("Invalid backup location '%-.64s'"),
/* ER_BACKUP_READ_LOC */
gettext_noop("Can't read backup location '%-.64s'"),
/* ER_BACKUP_WRITE_LOC */
gettext_noop("Can't write to backup location '%-.64s' (file already exists?)"),
/* ER_BACKUP_LIST_DBS */
gettext_noop("Can't enumerate server databases"),
/* ER_BACKUP_LIST_TABLES */
gettext_noop("Can't enumerate server tables"),
/* ER_BACKUP_LIST_DB_TABLES */
gettext_noop("Can't enumerate tables in database %-.64s"),
/* ER_BACKUP_SKIP_VIEW */
gettext_noop("Skipping view %-.64s in database %-.64s"),
/* ER_BACKUP_NO_ENGINE */
gettext_noop("Skipping table %-.64s since it has no valid storage engine"),
/* ER_BACKUP_TABLE_OPEN */
gettext_noop("Can't open table %-.64s"),
/* ER_BACKUP_READ_HEADER */
gettext_noop("Can't read backup archive preamble"),
/* ER_BACKUP_WRITE_HEADER */
gettext_noop("Can't write backup archive preamble"),
/* ER_BACKUP_NO_BACKUP_DRIVER */
gettext_noop("Can't find backup driver for table %-.64s"),
/* ER_BACKUP_NOT_ACCEPTED */
gettext_noop("%-.64s backup driver was selected for table %-.64s but it rejects to handle this table"),
/* ER_BACKUP_CREATE_BACKUP_DRIVER */
gettext_noop("Can't create %-.64s backup driver"),
/* ER_BACKUP_CREATE_RESTORE_DRIVER */
gettext_noop("Can't create %-.64s restore driver"),
/* ER_BACKUP_TOO_MANY_IMAGES */
gettext_noop("Found %d images in backup archive but maximum %d are supported"),
/* ER_BACKUP_WRITE_META */
gettext_noop("Error when saving meta-data of %-.64s"),
/* ER_BACKUP_READ_META */
gettext_noop("Error when reading meta-data list"),
/* ER_BACKUP_CREATE_META */
gettext_noop("Can't create %-.64s"),
/* ER_BACKUP_GET_BUF */
gettext_noop("Can't allocate buffer for image data transfer"),
/* ER_BACKUP_WRITE_DATA */
gettext_noop("Error when writing %-.64s backup image data (for table #%d)"),
/* ER_BACKUP_READ_DATA */
gettext_noop("Error when reading data from backup stream"),
/* ER_BACKUP_NEXT_CHUNK */
gettext_noop("Can't go to the next chunk in backup stream"),
/* ER_BACKUP_INIT_BACKUP_DRIVER */
gettext_noop("Can't initialize %-.64s backup driver"),
/* ER_BACKUP_INIT_RESTORE_DRIVER */
gettext_noop("Can't initialize %-.64s restore driver"),
/* ER_BACKUP_STOP_BACKUP_DRIVER */
gettext_noop("Can't shut down %-.64s backup driver"),
/* ER_BACKUP_STOP_RESTORE_DRIVERS */
gettext_noop("Can't shut down %-.64s backup driver(s)"),
/* ER_BACKUP_PREPARE_DRIVER */
gettext_noop("%-.64s backup driver can't prepare for synchronization"),
/* ER_BACKUP_CREATE_VP */
gettext_noop("%-.64s backup driver can't create its image validity point"),
/* ER_BACKUP_UNLOCK_DRIVER */
gettext_noop("Can't unlock %-.64s backup driver after creating the validity point"),
/* ER_BACKUP_CANCEL_BACKUP */
gettext_noop("%-.64s backup driver can't cancel its backup operation"),
/* ER_BACKUP_CANCEL_RESTORE */
gettext_noop("%-.64s restore driver can't cancel its restore operation"),
/* ER_BACKUP_GET_DATA */
gettext_noop("Error when polling %-.64s backup driver for its image data"),
/* ER_BACKUP_SEND_DATA */
gettext_noop("Error when sending image data (for table #%d) to %-.64s restore driver"),
/* ER_BACKUP_SEND_DATA_RETRY */
gettext_noop("After %d attempts %-.64s restore driver still can't accept next block of data"),
/* ER_BACKUP_OPEN_TABLES */
gettext_noop("Open and lock tables failed in %-.64s"),
/* ER_BACKUP_THREAD_INIT */
gettext_noop("Backup driver's table locking thread can not be initialized."),
/* ER_BACKUP_PROGRESS_TABLES */
gettext_noop("Can't open the online backup progress tables. Check 'mysql.online_backup' and 'mysql.online_backup_progress'."),
/* ER_TABLESPACE_EXIST */
gettext_noop("Tablespace '%-.192s' already exists"),
/* ER_NO_SUCH_TABLESPACE */
gettext_noop("Tablespace '%-.192s' doesn't exist"),
/* ER_SLAVE_HEARTBEAT_FAILURE */
gettext_noop("Unexpected master's heartbeat data: %s"),
/* ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE */
gettext_noop("The requested value for the heartbeat period %s %s"),
/* ER_BACKUP_LOG_WRITE_ERROR */
gettext_noop("Can't write to the online backup progress log %-.64s."),
/* ER_TABLESPACE_NOT_EMPTY */
gettext_noop("Tablespace '%-.192s' not empty"),
/* ER_BACKUP_TS_CHANGE */
gettext_noop("Tablespace `%-.64s` needed by tables being restored has changed on the server. The original definition of the required tablespace is '%-.256s' while the same tablespace is defined on the server as '%-.256s'")
};

#endif
