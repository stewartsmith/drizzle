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

#include <drizzled/global.h>
#include <drizzled/base.h>
#include <mysys/my_sys.h>
#include <storage/myisam/my_handler.h>

#include <drizzled/gettext.h>

/*
  Errors a handler can give you
*/

static const char *handler_error_messages[]=
{
  /* HA_ERR_KEY_NOT_FOUND */
  N_("Didn't find key on read or update"),
  /* HA_ERR_FOUND_DUPP_KEY */
  N_("Duplicate key on write or update"),
  /* HA_ERR_INTERNAL_ERROR */
  N_("Internal (unspecified) error in handler"),
  /* HA_ERR_RECORD_CHANGED */
  N_("Someone has changed the row since it was read (while the table "
     "was locked to prevent it)"),
  /* HA_ERR_WRONG_INDEX */
  N_("Wrong index given to function"),
  /* empty */
  N_("Undefined handler error 125"),
  /* HA_ERR_CRASHED */
  N_("Index file is crashed"),
  /* HA_ERR_WRONG_IN_RECORD */
  N_("Record file is crashed"),
  /* HA_ERR_OUT_OF_MEM */
  N_("Out of memory in engine"),
  /* empty */
  N_("Undefined handler error 129"),
  /* HA_ERR_NOT_A_TABLE */
  N_("Incorrect file format"),
  /* HA_ERR_WRONG_COMMAND */
  N_("Command not supported by database"),
  /* HA_ERR_OLD_FILE */
  N_("Old database file"),
  /* HA_ERR_NO_ACTIVE_RECORD */
  N_("No record read before update"),
  /* HA_ERR_RECORD_DELETED */
  N_("Record was already deleted (or record file crashed)"),
  /* HA_ERR_RECORD_FILE_FULL */
  N_("No more room in record file"),
  /* HA_ERR_INDEX_FILE_FULL */
  N_("No more room in index file"),
  /* HA_ERR_END_OF_FILE */
  N_("No more records (read after end of file)"),
  /* HA_ERR_UNSUPPORTED */
  N_("Unsupported extension used for table"),
  /* HA_ERR_TO_BIG_ROW */
  N_("Too big row"),
  /* HA_WRONG_CREATE_OPTION */
  N_("Wrong create options"),
  /* HA_ERR_FOUND_DUPP_UNIQUE */
  N_("Duplicate unique key or constraint on write or update"),
  /* HA_ERR_UNKNOWN_CHARSET */
  N_("Unknown character set used in table"),
  /* HA_ERR_WRONG_MRG_TABLE_DEF */
  N_("Conflicting table definitions in sub-tables of MERGE table"),
  /* HA_ERR_CRASHED_ON_REPAIR */
  N_("Table is crashed and last repair failed"),
  /* HA_ERR_CRASHED_ON_USAGE */
  N_("Table was marked as crashed and should be repaired"),
  /* HA_ERR_LOCK_WAIT_TIMEOUT */
  N_("Lock timed out; Retry transaction"),
  /* HA_ERR_LOCK_TABLE_FULL */
  N_("Lock table is full;  Restart program with a larger locktable"),
  /* HA_ERR_READ_ONLY_TRANSACTION */
  N_("Updates are not allowed under a read only transactions"),
  /* HA_ERR_LOCK_DEADLOCK */
  N_("Lock deadlock; Retry transaction"),
  /* HA_ERR_CANNOT_ADD_FOREIGN */
  N_("Foreign key constraint is incorrectly formed"),
  /* HA_ERR_NO_REFERENCED_ROW */
  N_("Cannot add a child row"),
  /* HA_ERR_ROW_IS_REFERENCED */
  N_("Cannot delete a parent row"),
  /* HA_ERR_NO_SAVEPOINT */
  N_("No savepoint with that name"),
  /* HA_ERR_NON_UNIQUE_BLOCK_SIZE */
  N_("Non unique key block size"),
  /* HA_ERR_NO_SUCH_TABLE */
  N_("The table does not exist in engine"),
  /* HA_ERR_TABLE_EXIST */
  N_("The table already existed in storage engine"),
  /* HA_ERR_NO_CONNECTION */
  N_("Could not connect to storage engine"),
  /* HA_ERR_NULL_IN_SPATIAL */
  N_("Unexpected null pointer found when using spatial index"),
  /* HA_ERR_TABLE_DEF_CHANGED */
  N_("The table changed in storage engine"),
  /* HA_ERR_NO_PARTITION_FOUND */
  N_("There's no partition in table for the given value"),
  /* HA_ERR_RBR_LOGGING_FAILED */
  N_("Row-based binlogging of row failed"),
  /* HA_ERR_DROP_INDEX_FK */
  N_("Index needed in foreign key constraint"),
  /* HA_ERR_FOREIGN_DUPLICATE_KEY */
  N_("Upholding foreign key constraints would lead to a duplicate key error"),
  /* HA_ERR_TABLE_NEEDS_UPGRADE */
  N_("Table needs to be upgraded before it can be used"),
  /* HA_ERR_TABLE_READONLY */
  N_("Table is read only"),
  /* HA_ERR_AUTOINC_READ_FAILED */
  N_("Failed to get next auto increment value"),
  /* HA_ERR_AUTOINC_ERANGE */
  N_("Failed to set row auto increment value"),
  /* HA_ERR_GENERIC */
  N_("Unknown (generic) error from engine"),
  /* HA_ERR_RECORD_IS_THE_SAME */
  N_("Record is the same"),
  /* HA_ERR_LOGGING_IMPOSSIBLE */
  N_("It is not possible to log this statement"),
  /* HA_ERR_TABLESPACE_EXIST */
  N_("Tablespace exists"),
  /* HA_ERR_CORRUPT_EVENT */
  N_("The event was corrupt, leading to illegal data being read"),
  /* HA_ERR_NEW_FILE */
  N_("The table is of a new format not supported by this version"),
  /* HA_ERR_ROWS_EVENT_APPLY */
  N_("The event could not be processed no other hanlder error happened"),
  /* HA_ERR_INITIALIZATION */
  N_("Got a fatal error during initialzaction of handler"),
  /* HA_ERR_FILE_TOO_SHORT */
  N_("File to short; Expected more data in file"),
  /* HA_ERR_WRONG_CRC */
  N_("Read page with wrong checksum"),
  /* HA_ERR_LOCK_OR_ACTIVE_TRANSACTION */
  N_("Lock or active transaction"), /* TODO: get a better message */
  /* HA_ERR_NO_SUCH_TABLESPACE */
  N_("No such table space"), /* TODO: get a better message */
  /* HA_ERR_TABLESPACE_NOT_EMPTY */
  N_("Tablespace not empty") /* TODO: get a better message */
};


/*
  Register handler error messages for usage with my_error()

  NOTES
    This is safe to call multiple times as my_error_register()
    will ignore calls to register already registered error numbers.
*/


void my_handler_error_register(void)
{
  /*
    If you got compilation error here about compile_time_assert array, check
    that every HA_ERR_xxx constant has a corresponding error message in
    handler_error_messages[] list (check mysys/ma_handler_errors.h and
    include/my_base.h).

    TODO: Remove fix the handler_error_messages so that this hack isn't
          necessary.
  */
#ifdef __GNUC__
  char compile_time_assert[(HA_ERR_FIRST +
                            array_elements(handler_error_messages) ==
                            HA_ERR_LAST + 1) ? 1 : -1]
    __attribute__ ((__unused__));
#endif
  my_error_register(handler_error_messages, HA_ERR_FIRST,
                    HA_ERR_FIRST+ array_elements(handler_error_messages)-1);
}


void my_handler_error_unregister(void)
{
  my_error_unregister(HA_ERR_FIRST,
                      HA_ERR_FIRST+ array_elements(handler_error_messages)-1);
}
