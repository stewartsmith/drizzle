/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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

#include <config.h>
#include <drizzled/base.h>
#include <drizzled/internal/my_sys.h>
#include <plugin/myisam/my_handler.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>

using namespace drizzled;

/*
  Errors a handler can give you
*/

/*
  Register handler error messages for usage with my_error()

  NOTES
    Calling this method multiple times will emit warnings as the
    message values are overridden.
*/
void my_handler_error_register(void)
{
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_KEY_NOT_FOUND, N_("Didn't find key on read or update"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_FOUND_DUPP_KEY, N_("Duplicate key on write or update"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_INTERNAL_ERROR, N_("Internal (unspecified) error in handler"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_RECORD_CHANGED,
                    N_("Someone has changed the row since it was read (while the table "
                       "was locked to prevent it)"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_WRONG_INDEX, N_("Wrong index given to function"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_CRASHED, N_("Index file is crashed"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_WRONG_IN_RECORD, N_("Record file is crashed"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_OUT_OF_MEM, N_("Out of memory in engine"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NOT_A_TABLE, N_("Incorrect file format"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_WRONG_COMMAND, N_("Command not supported by database"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_OLD_FILE, N_("Old database file"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_ACTIVE_RECORD, N_("No record read before update"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_RECORD_DELETED, N_("Record was already deleted (or record file crashed)"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_RECORD_FILE_FULL, N_("No more room in record file"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_INDEX_FILE_FULL, N_("No more room in index file"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_END_OF_FILE, N_("No more records (read after end of file)"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_UNSUPPORTED, N_("Unsupported extension used for table"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TO_BIG_ROW, N_("Too big row"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_WRONG_CREATE_OPTION, N_("Wrong create options"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_FOUND_DUPP_UNIQUE, N_("Duplicate unique key or constraint on write or update"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_UNKNOWN_CHARSET, N_("Unknown character set used in table"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_WRONG_MRG_TABLE_DEF, N_("Conflicting table definitions in sub-tables of MERGE table"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_CRASHED_ON_REPAIR, N_("Table is crashed and last repair failed"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_CRASHED_ON_USAGE, N_("Table was marked as crashed and should be repaired"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_LOCK_WAIT_TIMEOUT, N_("Lock timed out; Retry transaction"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_LOCK_TABLE_FULL, N_("Lock table is full;  Restart program with a larger locktable"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_READ_ONLY_TRANSACTION, N_("Updates are not allowed under a read only transactions"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_LOCK_DEADLOCK, N_("Lock deadlock; Retry transaction"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_CANNOT_ADD_FOREIGN, N_("Foreign key constraint is incorrectly formed"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_REFERENCED_ROW, N_("Cannot add a child row"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_ROW_IS_REFERENCED, N_("Cannot delete a parent row"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_SAVEPOINT, N_("No savepoint with that name"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NON_UNIQUE_BLOCK_SIZE, N_("Non unique key block size"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_SUCH_TABLE, N_("The table does not exist in engine"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLE_EXIST, N_("The table already existed in storage engine"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_CONNECTION, N_("Could not connect to storage engine"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NULL_IN_SPATIAL, N_("Unexpected null pointer found when using spatial index"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLE_DEF_CHANGED, N_("The table changed in storage engine"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_PARTITION_FOUND, N_("There's no partition in table for the given value"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_RBR_LOGGING_FAILED, N_("Row-based binlogging of row failed"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_DROP_INDEX_FK, N_("Index needed in foreign key constraint"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_FOREIGN_DUPLICATE_KEY, N_("Upholding foreign key constraints would lead to a duplicate key error"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLE_NEEDS_UPGRADE, N_("Table needs to be upgraded before it can be used"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLE_READONLY, N_("Table is read only"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_AUTOINC_READ_FAILED, N_("Failed to get next auto increment value"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_AUTOINC_ERANGE, N_("Failed to set row auto increment value"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_GENERIC, N_("Unknown (generic) error from engine"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_RECORD_IS_THE_SAME, N_("Record is the same"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_LOGGING_IMPOSSIBLE, N_("It is not possible to log this statement"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLESPACE_EXIST, N_("Tablespace exists"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_CORRUPT_EVENT, N_("The event was corrupt, leading to illegal data being read"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NEW_FILE, N_("The table is of a new format not supported by this version"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_ROWS_EVENT_APPLY, N_("The event could not be processed no other handler error happened"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_INITIALIZATION, N_("Got a fatal error during initialzation of handler"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_FILE_TOO_SHORT, N_("File to short; Expected more data in file"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_WRONG_CRC, N_("Read page with wrong checksum"));
  /* TODO: get a better message for these */
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_LOCK_OR_ACTIVE_TRANSACTION, N_("Lock or active transaction"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_SUCH_TABLESPACE, N_("No such table space"));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLESPACE_NOT_EMPTY, N_("Tablespace not empty"));
}
