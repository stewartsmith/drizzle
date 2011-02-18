/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

/* Functions to map drizzle errno to sql_state */
#include <config.h>

#include <algorithm>

#include <drizzled/error.h>
#include <drizzled/error/sql_state.h>

using namespace std;

namespace drizzled
{

namespace error
{

struct sql_state_t
{
  drizzled::error_t drizzle_errno;
  const char *odbc_state;
  const char *jdbc_state;
};

sql_state_t sqlstate_map[]=
{
  { ER_DUP_KEY                              ,"23000", "" },
  { ER_OUTOFMEMORY                          ,"HY001", "S1001" },
  { ER_OUT_OF_SORTMEMORY                    ,"HY001", "S1001" },
  { ER_CON_COUNT_ERROR                      ,"08004", "" },
  { ER_BAD_HOST_ERROR                       ,"08S01", "" },
  { ER_HANDSHAKE_ERROR                      ,"08S01", "" },
  { ER_DBACCESS_DENIED_ERROR                ,"42000", "" },
  { ER_ACCESS_DENIED_ERROR                  ,"28000", "" },
  { ER_NO_DB_ERROR                          ,"3D000", "" },
  { ER_UNKNOWN_COM_ERROR                    ,"08S01", "" },
  { ER_BAD_NULL_ERROR                       ,"23000", "" },
  { ER_BAD_DB_ERROR                         ,"42000", "" },
  { ER_TABLE_EXISTS_ERROR                   ,"42S01", "" },
  { ER_BAD_TABLE_ERROR                      ,"42S02", "" },
  { ER_NON_UNIQ_ERROR                       ,"23000", "" },
  { ER_SERVER_SHUTDOWN                      ,"08S01", "" },
  { ER_BAD_FIELD_ERROR                      ,"42S22", "S0022" },
  { ER_WRONG_FIELD_WITH_GROUP               ,"42000", "S1009" },
  { ER_WRONG_GROUP_FIELD                    ,"42000", "S1009" },
  { ER_WRONG_SUM_SELECT                     ,"42000", "S1009" },
  { ER_WRONG_VALUE_COUNT                    ,"21S01", "" },
  { ER_TOO_LONG_IDENT                       ,"42000", "S1009" },
  { ER_DUP_FIELDNAME                        ,"42S21", "S1009" },
  { ER_DUP_KEYNAME                          ,"42000", "S1009" },
  { ER_DUP_ENTRY                            ,"23000", "S1009" },
  { ER_WRONG_FIELD_SPEC                     ,"42000", "S1009" },
  { ER_PARSE_ERROR                          ,"42000", "s1009" },
  { ER_EMPTY_QUERY                          ,"42000", "" },
  { ER_NONUNIQ_TABLE                        ,"42000", "S1009" },
  { ER_INVALID_DEFAULT                      ,"42000", "S1009" },
  { ER_MULTIPLE_PRI_KEY                     ,"42000", "S1009" },
  { ER_TOO_MANY_KEYS                        ,"42000", "S1009" },
  { ER_TOO_MANY_KEY_PARTS                   ,"42000", "S1009" },
  { ER_TOO_LONG_KEY                         ,"42000", "S1009" },
  { ER_KEY_COLUMN_DOES_NOT_EXITS            ,"42000", "S1009" },
  { ER_BLOB_USED_AS_KEY                     ,"42000", "S1009" },
  { ER_TOO_BIG_FIELDLENGTH                  ,"42000", "S1009" },
  { ER_WRONG_AUTO_KEY                       ,"42000", "S1009" },
  { ER_FORCING_CLOSE                        ,"08S01", "" },
  { ER_IPSOCK_ERROR                         ,"08S01", "" },
  { ER_NO_SUCH_INDEX                        ,"42S12", "S1009" },
  { ER_WRONG_FIELD_TERMINATORS              ,"42000", "S1009" },
  { ER_BLOBS_AND_NO_TERMINATED              ,"42000", "S1009" },
  { ER_CANT_REMOVE_ALL_FIELDS               ,"42000", "" },
  { ER_CANT_DROP_FIELD_OR_KEY               ,"42000", "" },
  { ER_BLOB_CANT_HAVE_DEFAULT               ,"42000", "" },
  { ER_WRONG_DB_NAME                        ,"42000", "" },
  { ER_WRONG_TABLE_NAME                     ,"42000", "" },
  { ER_TOO_BIG_SELECT                       ,"42000", "" },
  { ER_UNKNOWN_PROCEDURE                    ,"42000", "" },
  { ER_WRONG_PARAMCOUNT_TO_PROCEDURE        ,"42000", "" },
  { ER_UNKNOWN_TABLE                        ,"42S02", "" },
  { ER_FIELD_SPECIFIED_TWICE                ,"42000", "" },
  { ER_UNSUPPORTED_EXTENSION                ,"42000", "" },
  { ER_TABLE_MUST_HAVE_COLUMNS              ,"42000", "" },
  { ER_TOO_BIG_ROWSIZE                      ,"42000", "" },
  { ER_WRONG_OUTER_JOIN                     ,"42000", "" },
  { ER_NULL_COLUMN_IN_INDEX                 ,"42000", "" },
  { ER_WRONG_VALUE_COUNT_ON_ROW             ,"21S01", "" },
  { ER_MIX_OF_GROUP_FUNC_AND_FIELDS         ,"42000", "" },
  { ER_TABLE_UNKNOWN                        ,"42S02", "" },
  { ER_SYNTAX_ERROR                         ,"42000", "" },
  { ER_NET_PACKET_TOO_LARGE                 ,"08S01", "" },
  { ER_NET_PACKETS_OUT_OF_ORDER             ,"08S01", "" },
  { ER_TABLE_CANT_HANDLE_BLOB               ,"42000", "" },
  { ER_TABLE_CANT_HANDLE_AUTO_INCREMENT     ,"42000", "" },
  { ER_WRONG_COLUMN_NAME                    ,"42000", "" },
  { ER_WRONG_KEY_COLUMN                     ,"42000", "" },
  { ER_DUP_UNIQUE                           ,"23000", "" },
  { ER_BLOB_KEY_WITHOUT_LENGTH              ,"42000", "" },
  { ER_PRIMARY_CANT_HAVE_NULL               ,"42000", "" },
  { ER_TOO_MANY_ROWS                        ,"42000", "" },
  { ER_REQUIRES_PRIMARY_KEY                 ,"42000", "" },
  { ER_KEY_DOES_NOT_EXITS                   ,"42000", "S1009" },
  { ER_CHECK_NO_SUCH_TABLE                  ,"42000", "" },
  { ER_CHECK_NOT_IMPLEMENTED                ,"42000", "" },
  { ER_NEW_ABORTING_CONNECTION              ,"08S01", "" },
  { ER_READ_ONLY_TRANSACTION                ,"25000", "" },
  { ER_LOCK_DEADLOCK                        ,"40001", "" },
  { ER_NO_REFERENCED_ROW                    ,"23000", "" },
  { ER_ROW_IS_REFERENCED                    ,"23000", "" },
  { ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT    ,"21000", "" },
  { ER_NO_DEFAULT                           ,"42000", "" },
  { ER_WRONG_VALUE_FOR_VAR                  ,"42000", "" },
  { ER_WRONG_TYPE_FOR_VAR                   ,"42000", "" },
  { ER_CANT_USE_OPTION_HERE                 ,"42000", "" },
  { ER_NOT_SUPPORTED_YET                    ,"42000", "" },
  { ER_WRONG_FK_DEF                         ,"42000", "" },
  { ER_OPERAND_COLUMNS                      ,"21000", "" },
  { ER_SUBQUERY_NO_1_ROW                    ,"21000", "" },
  { ER_ILLEGAL_REFERENCE                    ,"42S22", "" },
  { ER_DERIVED_MUST_HAVE_ALIAS              ,"42000", "" },
  { ER_SELECT_REDUCED                       ,"01000", "" },
  { ER_TABLENAME_NOT_ALLOWED_HERE           ,"42000", "" },
  { ER_SPATIAL_CANT_HAVE_NULL               ,"42000", "" },
  { ER_COLLATION_CHARSET_MISMATCH           ,"42000", "" },
  { ER_WARN_TOO_FEW_RECORDS                 ,"01000", "" },
  { ER_WARN_TOO_MANY_RECORDS                ,"01000", "" },
  { ER_WARN_NULL_TO_NOTNULL                 ,"22004", "" },
  { ER_WARN_DATA_OUT_OF_RANGE               ,"22003", "" },
  { ER_WARN_DATA_TRUNCATED                  ,"01000", "" },
  { ER_WRONG_NAME_FOR_INDEX                 ,"42000", "" },
  { ER_WRONG_NAME_FOR_CATALOG               ,"42000", "" },
  { ER_UNKNOWN_STORAGE_ENGINE               ,"42000", "" },
  { ER_TRUNCATED_WRONG_VALUE                ,"22007", "" },
  { ER_SP_DOES_NOT_EXIST                    ,"42000", "" },
  { ER_QUERY_INTERRUPTED                    ,"70100", "" },
  { ER_DIVISION_BY_ZERO                     ,"22012", "" },
  { ER_ILLEGAL_VALUE_FOR_TYPE               ,"22007", "" },
  { ER_XAER_RMFAIL                          ,"XAE07", "" },
  { ER_DATA_TOO_LONG                        ,"22001", "" },
  { ER_SP_NO_RETSET                         ,"0A000", "" },
  { ER_CANT_CREATE_GEOMETRY_OBJECT          ,"22003", "" },
  { ER_TOO_BIG_SCALE                        ,"42000", "S1009" },
  { ER_TOO_BIG_PRECISION                    ,"42000", "S1009" },
  { ER_M_BIGGER_THAN_D                      ,"42000", "S1009" },
  { ER_TOO_BIG_DISPLAYWIDTH                 ,"42000", "S1009" },
  { ER_DATETIME_FUNCTION_OVERFLOW           ,"22008", "" },
  { ER_ROW_IS_REFERENCED_2                  ,"23000", "" },
  { ER_NO_REFERENCED_ROW_2                  ,"23000", "" },
  { ER_NON_GROUPING_FIELD_USED              ,"42000", "" },
  { ER_FOREIGN_DUPLICATE_KEY                ,"23000", "S1009" },
  { ER_CANT_CHANGE_TX_ISOLATION             ,"25001", "" },
  { ER_WRONG_PARAMCOUNT_TO_FUNCTION         ,"42000", "" },
  { ER_WRONG_PARAMETERS_TO_NATIVE_FCT       ,"42000", "" },
  { ER_DUP_ENTRY_WITH_KEY_NAME              ,"23000", "S1009" },
};

static bool compare_errno_map(sql_state_t a,
                              sql_state_t b)
{
  return (a.drizzle_errno < b.drizzle_errno);
}

const char *convert_to_sqlstate(drizzled::error_t drizzle_errno)
{

  sql_state_t drizzle_err_state= {drizzle_errno, NULL, NULL};
  sql_state_t* result=
    lower_bound(&sqlstate_map[0],
                &sqlstate_map[sizeof(sqlstate_map)/sizeof(*sqlstate_map)],
                drizzle_err_state, compare_errno_map);

  if ((*result).drizzle_errno == drizzle_errno)
    return (*result).odbc_state;

  /* General error */
  return "HY000";
}

} /* namespace error */
} /* namespace drizzled */
