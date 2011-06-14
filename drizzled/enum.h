/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#pragma once

namespace drizzled
{

/**
 * @TODO Move to a separate header?
 *
 * It's needed by item.h and field.h, which are both inter-dependent
 * and contain forward declarations of many structs/classes in the
 * other header file.
 *
 * What is needed is a separate header file that is included
 * by *both* item.h and field.h to resolve inter-dependencies
 *
 * But, probably want to hold off on this until Stew finished the UDF cleanup
 */
enum Derivation
{
  DERIVATION_IGNORABLE= 5,
  DERIVATION_COERCIBLE= 4,
  DERIVATION_SYSCONST= 3,
  DERIVATION_IMPLICIT= 2,
  DERIVATION_NONE= 1,
  DERIVATION_EXPLICIT= 0
};

enum enum_parsing_place
{
  NO_MATTER,
  IN_HAVING,
  SELECT_LIST,
  IN_WHERE,
  IN_ON
};

enum enum_mysql_completiontype
{
  ROLLBACK_RELEASE= -2,
  ROLLBACK= 1,
  ROLLBACK_AND_CHAIN= 7,
  COMMIT_RELEASE= -1,
  COMMIT= 0,
  COMMIT_AND_CHAIN= 6
};

enum enum_check_fields
{
  CHECK_FIELD_IGNORE,
  CHECK_FIELD_WARN,
  CHECK_FIELD_ERROR_FOR_NULL
};

enum sql_var_t
{
  OPT_DEFAULT= 0,
  OPT_SESSION,
  OPT_GLOBAL
};

enum column_format_type
{
  COLUMN_FORMAT_TYPE_NOT_USED= -1,
  COLUMN_FORMAT_TYPE_DEFAULT= 0,
  COLUMN_FORMAT_TYPE_FIXED= 1,
  COLUMN_FORMAT_TYPE_DYNAMIC= 2
};


/**
  Category of table found in the table share.
*/
enum enum_table_category
{
  /**
    Unknown value.
  */
  TABLE_UNKNOWN_CATEGORY=0,

  /**
    Temporary table.
    The table is visible only in the session.
    Therefore,
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    do not apply to this table.
    Note that LOCK Table t FOR READ/WRITE
    can be used on temporary tables.
    Temporary tables are not part of the table cache.
  */
  TABLE_CATEGORY_TEMPORARY=1,

  /**
    User table.
    These tables do honor:
    - LOCK Table t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    User tables are cached in the table cache.
  */
  TABLE_CATEGORY_USER=2,

  /**
    Information schema tables.
    These tables are an interface provided by the system
    to inspect the system metadata.
    These tables do *not* honor:
    - LOCK Table t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    as there is no point in locking explicitely
    an INFORMATION_SCHEMA table.
    Nothing is directly written to information schema tables.
    Note that this value is not used currently,
    since information schema tables are not shared,
    but implemented as session specific temporary tables.
  */
  /*
    TODO: Fixing the performance issues of I_S will lead
    to I_S tables in the table cache, which should use
    this table type.
  */
  TABLE_CATEGORY_INFORMATION
};

enum enum_mark_columns
{
  MARK_COLUMNS_NONE,
  MARK_COLUMNS_READ,
  MARK_COLUMNS_WRITE
};

enum enum_filetype
{
  FILETYPE_CSV,
  FILETYPE_XML
};

enum find_item_error_report_type
{
  REPORT_ALL_ERRORS,
  REPORT_EXCEPT_NOT_FOUND,
  IGNORE_ERRORS,
  REPORT_EXCEPT_NON_UNIQUE,
  IGNORE_EXCEPT_NON_UNIQUE
};

/*
  Values in this enum are used to indicate how a tables TIMESTAMP field
  should be treated. It can be set to the current timestamp on insert or
  update or both.
  WARNING: The values are used for bit operations. If you change the
  enum, you must keep the bitwise relation of the values. For example:
  (int) TIMESTAMP_AUTO_SET_ON_BOTH must be equal to
  (int) TIMESTAMP_AUTO_SET_ON_INSERT | (int) TIMESTAMP_AUTO_SET_ON_UPDATE.
  We use an enum here so that the debugger can display the value names.
*/
enum timestamp_auto_set_type
{
  TIMESTAMP_NO_AUTO_SET= 0,
  TIMESTAMP_AUTO_SET_ON_INSERT= 1,
  TIMESTAMP_AUTO_SET_ON_UPDATE= 2,
  TIMESTAMP_AUTO_SET_ON_BOTH= 3
};

enum enum_ha_read_modes
{
  RFIRST,
  RNEXT,
  RPREV,
  RLAST,
  RKEY,
  RNEXT_SAME
};

enum enum_tx_isolation
{
  ISO_READ_UNCOMMITTED,
  ISO_READ_COMMITTED,
  ISO_REPEATABLE_READ,
  ISO_SERIALIZABLE
};


enum SHOW_COMP_OPTION
{
  SHOW_OPTION_YES,
  SHOW_OPTION_NO,
  SHOW_OPTION_DISABLED
};

/*
  When a command is added here, be sure it's also added in mysqld.cc
  in "struct show_var_st status_vars[]= {" ...

  If the command returns a result set or is not allowed in stored
  functions or triggers, please also make sure that
  sp_get_flags_for_command (sp_head.cc) returns proper flags for the
  added SQLCOM_.
*/

enum enum_sql_command {
  SQLCOM_SELECT,
  SQLCOM_CREATE_TABLE,
  SQLCOM_CREATE_INDEX,
  SQLCOM_ALTER_TABLE,
  SQLCOM_UPDATE,
  SQLCOM_INSERT,
  SQLCOM_INSERT_SELECT,
  SQLCOM_DELETE,
  SQLCOM_TRUNCATE,
  SQLCOM_DROP_TABLE,
  SQLCOM_DROP_INDEX,
  SQLCOM_SHOW_CREATE,
  SQLCOM_SHOW_CREATE_DB,
  SQLCOM_LOAD,
  SQLCOM_SET_OPTION,
  SQLCOM_UNLOCK_TABLES,
  SQLCOM_CHANGE_DB,
  SQLCOM_CREATE_DB,
  SQLCOM_DROP_DB,
  SQLCOM_ALTER_DB,
  SQLCOM_REPLACE,
  SQLCOM_REPLACE_SELECT,
  SQLCOM_CHECK,
  SQLCOM_FLUSH,
  SQLCOM_KILL,
  SQLCOM_ANALYZE,
  SQLCOM_ROLLBACK,
  SQLCOM_ROLLBACK_TO_SAVEPOINT,
  SQLCOM_COMMIT,
  SQLCOM_SAVEPOINT,
  SQLCOM_RELEASE_SAVEPOINT,
  SQLCOM_BEGIN,
  SQLCOM_RENAME_TABLE,
  SQLCOM_SHOW_WARNS,
  SQLCOM_EMPTY_QUERY,
  SQLCOM_SHOW_ERRORS,
  SQLCOM_CHECKSUM,
  /*
    When a command is added here, be sure it's also added in mysqld.cc
    in "struct show_var_st status_vars[]= {" ...
  */
  /* This should be the last !!! */
  SQLCOM_END
};

enum enum_duplicates
{
  DUP_ERROR,
  DUP_REPLACE,
  DUP_UPDATE
};

enum drizzle_exit_codes
{
  EXIT_UNSPECIFIED_ERROR = 1,
  EXIT_UNKNOWN_OPTION,
  EXIT_AMBIGUOUS_OPTION,
  EXIT_NO_ARGUMENT_ALLOWED,
  EXIT_ARGUMENT_REQUIRED,
  EXIT_VAR_PREFIX_NOT_UNIQUE,
  EXIT_UNKNOWN_VARIABLE,
  EXIT_OUT_OF_MEMORY,
  EXIT_UNKNOWN_SUFFIX,
  EXIT_NO_PTR_TO_VARIABLE,
  EXIT_CANNOT_CONNECT_TO_SERVICE,
  EXIT_OPTION_DISABLED,
  EXIT_ARGUMENT_INVALID
};


} /* namespace drizzled */

