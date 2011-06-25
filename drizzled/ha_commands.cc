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

/**
  @file Cursor.cc

  Handler-calling-functions
*/

#include <config.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/probes.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/replication_services.h>
#include <drizzled/lock.h>
#include <drizzled/item/int.h>
#include <drizzled/item/empty_string.h>
#include <drizzled/field/epoch.h>
#include <drizzled/plugin/client.h>
#include <drizzled/internal/my_sys.h>

using namespace std;

namespace drizzled {

KEY_CREATE_INFO default_key_create_info= { HA_KEY_ALG_UNDEF, 0, {NULL,0} };

const char *ha_row_type[] = {
  "", "FIXED", "DYNAMIC", "COMPRESSED", "REDUNDANT", "COMPACT", "PAGE", "?","?","?"
};



/**
  Register Cursor error messages for use with my_error().
*/

void ha_init_errors(void)
{
  // Set the dedicated error messages.
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_KEY_NOT_FOUND,          ER(ER_KEY_NOT_FOUND));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_FOUND_DUPP_KEY,         ER(ER_DUP_KEY));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_RECORD_CHANGED,         "Update wich is recoverable");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_WRONG_INDEX,            "Wrong index given to function");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_CRASHED,                ER(ER_NOT_KEYFILE));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_WRONG_IN_RECORD,        ER(ER_CRASHED_ON_USAGE));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_OUT_OF_MEM,             "Table Cursor out of memory");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NOT_A_TABLE,            "Incorrect file format '%.64s'");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_WRONG_COMMAND,          "Command not supported");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_OLD_FILE,               ER(ER_OLD_KEYFILE));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_ACTIVE_RECORD,       "No record read in update");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_RECORD_DELETED,         "Intern record deleted");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_RECORD_FILE_FULL,       ER(ER_RECORD_FILE_FULL));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_INDEX_FILE_FULL,        "No more room in index file '%.64s'");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_END_OF_FILE,            "End in next/prev/first/last");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_UNSUPPORTED,            ER(ER_ILLEGAL_HA));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TO_BIG_ROW,             "Too big row");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_WRONG_CREATE_OPTION,        "Wrong create option");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_FOUND_DUPP_UNIQUE,      ER(ER_DUP_UNIQUE));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_UNKNOWN_CHARSET,        "Can't open charset");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_WRONG_MRG_TABLE_DEF,    ER(ER_WRONG_MRG_TABLE));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_CRASHED_ON_REPAIR,      ER(ER_CRASHED_ON_REPAIR));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_CRASHED_ON_USAGE,       ER(ER_CRASHED_ON_USAGE));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_LOCK_WAIT_TIMEOUT,      ER(ER_LOCK_WAIT_TIMEOUT));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_LOCK_TABLE_FULL,        ER(ER_LOCK_TABLE_FULL));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_READ_ONLY_TRANSACTION,  ER(ER_READ_ONLY_TRANSACTION));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_LOCK_DEADLOCK,          ER(ER_LOCK_DEADLOCK));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_CANNOT_ADD_FOREIGN,     ER(ER_CANNOT_ADD_FOREIGN));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_REFERENCED_ROW,      ER(ER_NO_REFERENCED_ROW_2));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_ROW_IS_REFERENCED,      ER(ER_ROW_IS_REFERENCED_2));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_SAVEPOINT,           "No savepoint with that name");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NON_UNIQUE_BLOCK_SIZE,  "Non unique key block size");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_SUCH_TABLE,          "No such table: '%.64s'");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLE_EXIST,            ER(ER_TABLE_EXISTS_ERROR));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_NO_CONNECTION,          "Could not connect to storage engine");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLE_DEF_CHANGED,      ER(ER_TABLE_DEF_CHANGED));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_FOREIGN_DUPLICATE_KEY,  "FK constraint would lead to duplicate key");
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLE_NEEDS_UPGRADE,    ER(ER_TABLE_NEEDS_UPGRADE));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_TABLE_READONLY,         ER(ER_OPEN_AS_READONLY));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_AUTOINC_READ_FAILED,    ER(ER_AUTOINC_READ_FAILED));
  DRIZZLE_ADD_ERROR_MESSAGE(HA_ERR_AUTOINC_ERANGE,         ER(ER_WARN_DATA_OUT_OF_RANGE));
}

} /* namespace drizzled */
