/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
 *
 *  Authors:
 *
 *    Jay Pipes <joinfu@sun.com>
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

#ifndef DRIZZLED_MESSAGE_STATEMENT_TRANSFORM_H
#define DRIZZLED_MESSAGE_STATEMENT_TRANSFORM_H

#include <drizzled/message/table.pb.h>
#include <string>

namespace drizzled
{
namespace message
{
/* some forward declarations */
class Statement;
class InsertHeader;
class InsertData;
class UpdateHeader;
class UpdateData;
class DeleteHeader;
class DeleteData;
class SetVariableStatement;

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
  NONE = 0,
  BULK_OPERATION_NO_HEADER = 1 /* A data segment without a header segment was found */
};

/**
 * This function looks at the Statement
 * message and appends a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @param Statement message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformStatementToSql(const Statement &source,
                        std::string *destination,
                        enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied InsertHeader
 * and InsertData message and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @note
 *
 * This function is typically used when a bulk insert operation
 * has been recognized and a saved InsertHeader is matched to
 * a new InsertData segment.
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
                              std::string *destination,
                              enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied UpdateHeader
 * and UpdateData message and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @note
 *
 * This function is typically used when a bulk update operation
 * has been recognized and a saved UpdateHeader is matched to
 * a new UpdateData segment.
 *
 * @param UpdateHeader message to transform
 * @param UpdateData message to transform
 * @param Destination string to append SQL to
 * @param Variation of SQL to generate
 *
 * @retval
 *  NONE if successful transformation
 * @retval
 *  Error code (see enum TransformSqlError definition) if failure
 */
enum TransformSqlError
transformUpdateStatementToSql(const UpdateHeader &header,
                              const UpdateData &data,
                              std::string *destination,
                              enum TransformSqlVariant sql_variant= DRIZZLE);

/**
 * This function looks at a supplied DeleteHeader
 * and DeleteData message and constructs a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @note
 *
 * This function is typically used when a bulk delete operation
 * has been recognized and a saved DeleteHeader is matched to
 * a new DeleteData segment.
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
                              std::string *destination,
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
                                   std::string *destination,
                                   enum TransformSqlVariant sql_variant= DRIZZLE);


/**
 * Returns true if the supplied message::Table::Field::FieldType
 * should have its values quoted when modifying values.
 *
 * @param[in] type of field
 */
bool shouldQuoteFieldValue(Table::Field::FieldType in_type);

} /* end namespace drizzled::message */
} /* end namespace drizzled */

#endif /* DRIZZLED_MESSAGE_STATEMENT_TRANSFORM_H */
