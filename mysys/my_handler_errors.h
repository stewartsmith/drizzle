#include <libdrizzle/gettext.h>

/*
  Errors a handler can give you
*/

static const char *handler_error_messages[]=
{
  /* HA_ERR_KEY_NOT_FOUND */
  gettext_noop("Didn't find key on read or update"),
  /* HA_ERR_FOUND_DUPP_KEY */
  gettext_noop("Duplicate key on write or update"),
  /* HA_ERR_INTERNAL_ERROR */
  gettext_noop("Internal (unspecified) error in handler"),
  /* HA_ERR_RECORD_CHANGED */
  gettext_noop("Someone has changed the row since it was read (while the table was locked to prevent it)"),
  /* HA_ERR_WRONG_INDEX */
  gettext_noop("Wrong index given to function"),
  /* empty */
  gettext_noop("Undefined handler error 125"),
  /* HA_ERR_CRASHED */
  gettext_noop("Index file is crashed"),
  /* HA_ERR_WRONG_IN_RECORD */
  gettext_noop("Record file is crashed"),
  /* HA_ERR_OUT_OF_MEM */
  gettext_noop("Out of memory in engine"),
  /* empty */
  gettext_noop("Undefined handler error 129"),
  /* HA_ERR_NOT_A_TABLE */
  gettext_noop("Incorrect file format"),
  /* HA_ERR_WRONG_COMMAND */
  gettext_noop("Command not supported by database"),
  /* HA_ERR_OLD_FILE */
  gettext_noop("Old database file"),
  /* HA_ERR_NO_ACTIVE_RECORD */
  gettext_noop("No record read before update"),
  /* HA_ERR_RECORD_DELETED */
  gettext_noop("Record was already deleted (or record file crashed)"),
  /* HA_ERR_RECORD_FILE_FULL */
  gettext_noop("No more room in record file"),
  /* HA_ERR_INDEX_FILE_FULL */
  gettext_noop("No more room in index file"),
  /* HA_ERR_END_OF_FILE */
  gettext_noop("No more records (read after end of file)"),
  /* HA_ERR_UNSUPPORTED */
  gettext_noop("Unsupported extension used for table"),
  /* HA_ERR_TO_BIG_ROW */
  gettext_noop("Too big row"),
  /* HA_WRONG_CREATE_OPTION */
  gettext_noop("Wrong create options"),
  /* HA_ERR_FOUND_DUPP_UNIQUE */
  gettext_noop("Duplicate unique key or constraint on write or update"),
  /* HA_ERR_UNKNOWN_CHARSET */
  gettext_noop("Unknown character set used in table"),
  /* HA_ERR_WRONG_MRG_TABLE_DEF */
  gettext_noop("Conflicting table definitions in sub-tables of MERGE table"),
  /* HA_ERR_CRASHED_ON_REPAIR */
  gettext_noop("Table is crashed and last repair failed"),
  /* HA_ERR_CRASHED_ON_USAGE */
  gettext_noop("Table was marked as crashed and should be repaired"),
  /* HA_ERR_LOCK_WAIT_TIMEOUT */
  gettext_noop("Lock timed out; Retry transaction"),
  /* HA_ERR_LOCK_TABLE_FULL */
  gettext_noop("Lock table is full;  Restart program with a larger locktable"),
  /* HA_ERR_READ_ONLY_TRANSACTION */
  gettext_noop("Updates are not allowed under a read only transactions"),
  /* HA_ERR_LOCK_DEADLOCK */
  gettext_noop("Lock deadlock; Retry transaction"),
  /* HA_ERR_CANNOT_ADD_FOREIGN */
  gettext_noop("Foreign key constraint is incorrectly formed"),
  /* HA_ERR_NO_REFERENCED_ROW */
  gettext_noop("Cannot add a child row"),
  /* HA_ERR_ROW_IS_REFERENCED */
  gettext_noop("Cannot delete a parent row"),
  /* HA_ERR_NO_SAVEPOINT */
  gettext_noop("No savepoint with that name"),
  /* HA_ERR_NON_UNIQUE_BLOCK_SIZE */
  gettext_noop("Non unique key block size"),
  /* HA_ERR_NO_SUCH_TABLE */
  gettext_noop("The table does not exist in engine"),
  /* HA_ERR_TABLE_EXIST */
  gettext_noop("The table already existed in storage engine"),
  /* HA_ERR_NO_CONNECTION */
  gettext_noop("Could not connect to storage engine"),
  /* HA_ERR_NULL_IN_SPATIAL */
  gettext_noop("Unexpected null pointer found when using spatial index"),
  /* HA_ERR_TABLE_DEF_CHANGED */
  gettext_noop("The table changed in storage engine"),
  /* HA_ERR_NO_PARTITION_FOUND */
  gettext_noop("There's no partition in table for the given value"),
  /* HA_ERR_RBR_LOGGING_FAILED */
  gettext_noop("Row-based binlogging of row failed"),
  /* HA_ERR_DROP_INDEX_FK */
  gettext_noop("Index needed in foreign key constraint"),
  /* HA_ERR_FOREIGN_DUPLICATE_KEY */
  gettext_noop("Upholding foreign key constraints would lead to a duplicate key error"),
  /* HA_ERR_TABLE_NEEDS_UPGRADE */
  gettext_noop("Table needs to be upgraded before it can be used"),
  /* HA_ERR_TABLE_READONLY */
  gettext_noop("Table is read only"),
  /* HA_ERR_AUTOINC_READ_FAILED */
  gettext_noop("Failed to get next auto increment value"),
  /* HA_ERR_AUTOINC_ERANGE */
  gettext_noop("Failed to set row auto increment value"),
  /* HA_ERR_GENERIC */
  gettext_noop("Unknown (generic) error from engine"),
  /* HA_ERR_RECORD_IS_THE_SAME */
  gettext_noop("Record is the same"),
  /* HA_ERR_LOGGING_IMPOSSIBLE */
  gettext_noop("It is not possible to log this statement"),
  /* HA_ERR_TABLESPACE_EXIST */
  gettext_noop("Tablespace exists"),
  /* HA_ERR_CORRUPT_EVENT */
  gettext_noop("The event was corrupt, leading to illegal data being read"),
  /* HA_ERR_NEW_FILE */
  gettext_noop("The table is of a new format not supported by this version"),
  /* HA_ERR_ROWS_EVENT_APPLY */
  gettext_noop("The event could not be processed no other hanlder error happened"),
  /* HA_ERR_INITIALIZATION */
  gettext_noop("Got a fatal error during initialzaction of handler"),
  /* HA_ERR_FILE_TOO_SHORT */
  gettext_noop("File to short; Expected more data in file"),
  /* HA_ERR_WRONG_CRC */
  gettext_noop("Read page with wrong checksum"),
  /* HA_ERR_LOCK_OR_ACTIVE_TRANSACTION */
  gettext_noop("Lock or active transaction"), /* TODO: get a better message */
  /* HA_ERR_NO_SUCH_TABLESPACE */
  gettext_noop("No such table space"), /* TODO: get a better message */
  /* HA_ERR_TABLESPACE_NOT_EMPTY */
  gettext_noop("Tablespace not empty") /* TODO: get a better message */
};

