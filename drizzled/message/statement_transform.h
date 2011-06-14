/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Jay Pipes <jaypipes@gmail.com>
 *
 *  Authors:
 *
 *    Jay Pipes <jaypipes@gmail.com>
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
 * @file
 *
 * Declarations of various routines that can be used to convert
 * Transaction messages to other formats, including SQL statements.
 */

#pragma once

#include <drizzled/common_fwd.h>
#include <drizzled/common.h>
#include <drizzled/message/table.pb.h>
#include <string>
#include <vector>

namespace drizzled {
namespace message {

/** A Variation of SQL to be output during transformation */
enum TransformSqlVariant
{
  ANSI,
  MYSQL_4,
  MYSQL_5,
  DRIZZLE
};

/** Error codes which can happen during tranformations */
enum TransformSqlError
{
  NONE= 0,
  MISSING_HEADER= 1, /* A data segment without a header segment was found */
  MISSING_DATA= 2,   /* A header segment without a data segment was found */
  UUID_MISMATCH= 3
};

/**
 * This function looks at the Statement
 * message and appends one or more correctly-formatted SQL
 * strings to the supplied vector of strings.
 *
 * @param Statement message to transform
 * @param Vector of strings to append SQL statements to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformStatementToSql(const Statement &source,
                        std::vector<std::string> &sql_strings,
                        enum TransformSqlVariant sql_variant= DRIZZLE,
                        bool already_in_transaction= false);

/**
 * This function looks at a supplied InsertHeader
 * and InsertData message and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @note
 *
 * This function is used when you want to construct a <strong>
 * single SQL statement</strong> from an entire InsertHeader and
 * InsertData message.  If there are many records in the InsertData
 * message, the SQL statement will be a multi-value INSERT statement.
 *
 * @param InsertHeader message to transform
 * @param InsertData message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformInsertStatementToSql(const InsertHeader &header,
                              const InsertData &data,
                              std::string &destination,
                              enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied InsertHeader
 * and a single InsertRecord message and constructs a correctly-formatted
 * SQL statement to the supplied destination string.
 *
 * @param InsertHeader message to transform
 * @param InsertRecord message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformInsertRecordToSql(const InsertHeader &header,
                           const InsertRecord &record,
                           std::string &destination,
                           enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * Helper function to construct the header portion of an INSERT
 * SQL statement from an InsertHeader message.
 *
 * @param InsertHeader message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformInsertHeaderToSql(const InsertHeader &header,
                           std::string &destination,
                           enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * Helper function to construct the header portion of an UPDATE
 * SQL statement from an UpdateHeader message.
 *
 * @param UpdateHeader message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformUpdateHeaderToSql(const UpdateHeader &header,
                           std::string &destination,
                           enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied UpdateHeader
 * and a single UpdateRecord message and constructs a correctly-formatted
 * SQL statement to the supplied destination string.
 *
 * @param UpdateHeader message to transform
 * @param UpdateRecord message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformUpdateRecordToSql(const UpdateHeader &header,
                           const UpdateRecord &record,
                           std::string &destination,
                           enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied DeleteHeader
 * and DeleteData message and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @note
 *
 * This function constructs a <strong>single SQL statement</strong>
 * for all keys in the DeleteData message.
 *
 * @param DeleteHeader message to transform
 * @param DeleteData message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformDeleteStatementToSql(const DeleteHeader &header,
                              const DeleteData &data,
                              std::string &destination,
                              enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied DeleteHeader
 * and a single DeleteRecord message and constructs a correctly-formatted
 * SQL statement to the supplied destination string.
 *
 * @param DeleteHeader message to transform
 * @param DeleteRecord message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformDeleteRecordToSql(const DeleteHeader &header,
                           const DeleteRecord &record,
                           std::string &destination,
                           enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * Helper function to construct the header portion of a DELETE
 * SQL statement from an DeleteHeader message.
 *
 * @param DeleteHeader message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformDeleteHeaderToSql(const DeleteHeader &header,
                           std::string &destination,
                           enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied DropTableStatement
 * and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @param DropTableStatement message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformDropTableStatementToSql(const DropTableStatement &statement,
                                  std::string &destination,
                                  enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied TruncateTableStatement
 * and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @param TruncateTableStatement message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformTruncateTableStatementToSql(const TruncateTableStatement &statement,
                                     std::string &destination,
                                     enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied CreateSchemaStatement
 * and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @param CreateSchemaStatement message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformCreateSchemaStatementToSql(const CreateSchemaStatement &statement,
                                    std::string &destination,
                                    enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied DropSchemaStatement
 * and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @param DropSchemaStatement message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformDropSchemaStatementToSql(const DropSchemaStatement &statement,
                                  std::string &destination,
                                  enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied AlterSchemaStatement
 * and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @param AlterSchemaStatement message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformAlterSchemaStatementToSql(const AlterSchemaStatement &statement,
                                   std::string &destination,
                                   enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied SetVariableStatement
 * and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @param SetVariableStatement message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformSetVariableStatementToSql(const SetVariableStatement &statement,
                                   std::string &destination,
                                   enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * Appends to supplied string an SQL expression
 * containing the supplied CreateTableStatement's
 * appropriate CREATE TABLE SQL statement.
 */
enum TransformSqlError
transformCreateTableStatementToSql(const CreateTableStatement &statement,
                                   std::string &destination,
                                   enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * Appends to the supplied string an SQL expression
 * representing the table for a CREATE TABLE expression.
 *
 * @param[in]   Table message
 * @param[out]  String to append to
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformTableDefinitionToSql(const Table &table,
                              std::string &destination,
                              enum TransformSqlVariant sql_variant= DRIZZLE,
                              bool with_schema= true);

/**
 * Appends to the supplied string an SQL expression
 * representing the table's optional attributes.
 *
 * @note
 *
 * This function will eventually be a much simpler
 * listing of key/value pairs.
 *
 * @param[in]   TableOptions message
 * @param[out]  String to append to
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformTableOptionsToSql(const Table::TableOptions &table_options,
                           std::string &destination,
                           enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * Appends to the supplied string an SQL expression
 * representing the index's attributes.  The built string
 * corresponds to the SQL in a CREATE INDEX statement.
 *
 * @param[in]   Index message
 * @param[in]   Table containing this index (used to get field names...)
 * @param[out]  String to append to
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformIndexDefinitionToSql(const Table::Index &index,
                              const Table &table,
                              std::string &destination,
                              enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * Appends to the supplied string an SQL expression
 * representing the foreign key attributes.  The built string
 * corresponds to the SQL in a CREATE TABLE statement.
 *
 * @param[in]   Foreign Key Constraint message
 * @param[in]   Table containing this foregin key (used to get field names...)
 * @param[out]  String to append to
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformForeignKeyConstraintDefinitionToSql(const Table::ForeignKeyConstraint &fkey,
                                             const Table &table,
                                             std::string &destination,
                                             enum TransformSqlVariant sql_variant = DRIZZLE);

/**
 * Appends to the supplied string an SQL expression
 * representing the field's attributes.  The built string
 * corresponds to the SQL in a CREATE TABLE statement.
 *
 * @param[in]   Field message
 * @param[out]  String to append to
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformFieldDefinitionToSql(const Table::Field &field,
                              std::string &destination,
                              enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * Returns true if the supplied message::Table::Field::FieldType
 * should have its values quoted when modifying values.
 *
 * @param[in] type of field
 */
bool shouldQuoteFieldValue(Table::Field::FieldType in_type);

drizzled::message::Table::Field::FieldType internalFieldTypeToFieldProtoType(enum enum_field_types type);

/**
 * Returns true if the transaction contains any Statement
 * messages which are not end segments (i.e. a bulk statement has
 * previously been sent to replicators).
 *
 * @param The transaction to check
 */
bool transactionContainsBulkSegment(const Transaction &transaction);

} /* namespace message */
} /* namespace drizzled */

