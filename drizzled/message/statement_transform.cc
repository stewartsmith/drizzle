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
 * Implementation of various routines that can be used to convert
 * Statement messages to other formats, including SQL strings.
 */

#include "drizzled/global.h"

#include "drizzled/message/statement_transform.h"
#include "drizzled/message/transaction.pb.h"
#include "drizzled/message/table.pb.h"

#include <string>

using namespace std;
using namespace drizzled;

enum message::TransformSqlError
message::transformStatementToSql(const message::Statement &source,
                                 string *destination,
                                 enum message::TransformSqlVariant sql_variant)
{
  message::TransformSqlError error= NONE;

  switch (source.type())
  {
  case message::Statement::INSERT:
    {
      /*
       * We can currently only handle non-bulk operations
       * here, because only a single copy of the header information
       * is constructed.
       */
      assert(source.has_insert_data());

      if (! source.has_insert_header())
      {
        error= BULK_OPERATION_NO_HEADER;
        return error;
      }

      const message::InsertHeader &insert_header= source.insert_header();
      const message::InsertData &insert_data= source.insert_data();
      error= transformInsertStatementToSql(insert_header,
                                           insert_data,
                                           destination,
                                           sql_variant);
    }
    break;
  case message::Statement::UPDATE:
    {
      /*
       * We can currently only handle non-bulk operations
       * here, because only a single copy of the header information
       * is constructed.
       */
      assert(source.has_update_data());

      if (! source.has_update_header())
      {
        error= BULK_OPERATION_NO_HEADER;
        return error;
      }

      const message::UpdateHeader &update_header= source.update_header();
      const message::UpdateData &update_data= source.update_data();
      error= transformUpdateStatementToSql(update_header,
                                           update_data,
                                           destination,
                                           sql_variant);
    }
    break;
  case message::Statement::DELETE:
    {
      /*
       * We can currently only handle non-bulk operations
       * here, because only a single copy of the header information
       * is constructed.
       */
      assert(source.has_delete_data());

      if (! source.has_delete_header())
      {
        error= BULK_OPERATION_NO_HEADER;
        return error;
      }

      const message::DeleteHeader &delete_header= source.delete_header();
      const message::DeleteData &delete_data= source.delete_data();
      error= message::transformDeleteStatementToSql(delete_header,
                                                    delete_data,
                                                    destination,
                                                    sql_variant);
    }
    break;
  case message::Statement::SET_VARIABLE:
    assert(source.has_set_variable_statement());
    error= message::transformSetVariableStatementToSql(source.set_variable_statement(),
                                                       destination,
                                                       sql_variant);
    break;
  case message::Statement::RAW_SQL:
  default:
    destination->assign(source.sql());
    break;
  }
  return error;
}

enum message::TransformSqlError
message::transformInsertStatementToSql(const message::InsertHeader &header,
                                       const message::InsertData &data,
                                       std::string *destination,
                                       enum message::TransformSqlVariant sql_variant)
{
  
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination->assign("INSERT INTO ");
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().schema_name());
  destination->push_back(quoted_identifier);
  destination->push_back('.');
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().table_name());
  destination->push_back(quoted_identifier);
  destination->append(" (");

  /* Add field list to SQL string... */
  uint32_t num_fields= header.field_metadata_size();
  uint32_t x, y;

  for (x= 0; x < num_fields; ++x)
  {
    const message::FieldMetadata &field_metadata= header.field_metadata(x);
    if (x != 0)
      destination->push_back(',');
    
    destination->push_back(quoted_identifier);
    destination->append(field_metadata.name());
    destination->push_back(quoted_identifier);
  }

  destination->append(") VALUES (");

  /* Add insert values */
  uint32_t num_records= data.record_size();
  bool should_quote_field_value= false;
  
  for (x= 0; x < num_records; ++x)
  {
    if (x != 0)
      destination->append("),(");

    for (y= 0; y < num_fields; ++y)
    {
      if (y != 0)
        destination->push_back(',');

      should_quote_field_value= message::shouldQuoteFieldValue(header.field_metadata(y).type());

      if (should_quote_field_value)
        destination->push_back('\'');

      destination->append(data.record(x).value(y));

      if (should_quote_field_value)
        destination->push_back('\'');
    }
  }
  destination->push_back(')');

  return NONE;
}

enum message::TransformSqlError
message::transformUpdateStatementToSql(const message::UpdateHeader &header,
                                       const message::UpdateData &data,
                                       std::string *destination,
                                       enum message::TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination->assign("UPDATE ");
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().schema_name());
  destination->push_back(quoted_identifier);
  destination->push_back('.');
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().table_name());
  destination->push_back(quoted_identifier);
  destination->append(" SET ");

  /* Add field SET list to SQL string... */
  uint32_t num_set_fields= header.set_field_metadata_size();
  uint32_t x, y;
  bool should_quote_field_value= false;

  for (x= 0; x < num_set_fields; ++x)
  {
    const message::FieldMetadata &field_metadata= header.set_field_metadata(x);
    if (x != 0)
      destination->push_back(',');
    
    destination->push_back(quoted_identifier);
    destination->append(field_metadata.name());
    destination->push_back(quoted_identifier);
    destination->push_back('=');

    should_quote_field_value= message::shouldQuoteFieldValue(field_metadata.type());

    if (should_quote_field_value)
      destination->push_back('\'');

    destination->append(header.set_value(x));

    if (should_quote_field_value)
      destination->push_back('\'');
  }

  /* Add WHERE clause to SQL string... */
  uint32_t num_key_fields= header.key_field_metadata_size();
  uint32_t num_key_records= data.record_size();

  destination->append(" WHERE ");
  for (x= 0; x < num_key_records; ++x)
  {
    if (x != 0)
      destination->append(" OR "); /* Always OR condition for multiple key records */

    if (num_key_fields > 1)
      destination->push_back('(');

    for (y= 0; y < num_key_fields; ++y) 
    {
      const message::FieldMetadata &field_metadata= header.key_field_metadata(y);
      
      if (y != 0)
        destination->append(" AND "); /* Always AND condition with a multi-column PK */

      destination->push_back(quoted_identifier);
      destination->append(field_metadata.name());
      destination->push_back(quoted_identifier);

      destination->push_back('=');

      should_quote_field_value= message::shouldQuoteFieldValue(field_metadata.type());

      if (should_quote_field_value)
        destination->push_back('\'');

      destination->append(data.record(x).key_value(y));

      if (should_quote_field_value)
        destination->push_back('\'');
    }
    if (num_key_fields > 1)
      destination->push_back(')');
  }
  return NONE;
}

enum message::TransformSqlError
message::transformDeleteStatementToSql(const message::DeleteHeader &header,
                                       const message::DeleteData &data,
                                       std::string *destination,
                                       enum message::TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination->assign("DELETE FROM ");
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().schema_name());
  destination->push_back(quoted_identifier);
  destination->push_back('.');
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().table_name());
  destination->push_back(quoted_identifier);
  
  /* Add WHERE clause to SQL string... */
  uint32_t num_key_fields= header.key_field_metadata_size();
  uint32_t num_key_records= data.record_size();
  uint32_t x, y;
  bool should_quote_field_value= false;

  destination->append(" WHERE ");
  for (x= 0; x < num_key_records; ++x)
  {
    if (x != 0)
      destination->append(" OR "); /* Always OR condition for multiple key records */

    if (num_key_fields > 1)
      destination->push_back('(');

    for (y= 0; y < num_key_fields; ++y) 
    {
      const message::FieldMetadata &field_metadata= header.key_field_metadata(y);
      
      if (y != 0)
        destination->append(" AND "); /* Always AND condition with a multi-column PK */

      destination->push_back(quoted_identifier);
      destination->append(field_metadata.name());
      destination->push_back(quoted_identifier);

      destination->push_back('=');

      should_quote_field_value= message::shouldQuoteFieldValue(field_metadata.type());

      if (should_quote_field_value)
        destination->push_back('\'');

      destination->append(data.record(x).key_value(y));

      if (should_quote_field_value)
        destination->push_back('\'');
    }
    if (num_key_fields > 1)
      destination->push_back(')');
  }
  return NONE;
}

enum message::TransformSqlError
message::transformSetVariableStatementToSql(const message::SetVariableStatement &statement,
                                            std::string *destination,
                                            enum message::TransformSqlVariant sql_variant)
{
  (void) sql_variant;
  const message::FieldMetadata &variable_metadata= statement.variable_metadata();
  bool should_quote_field_value= message::shouldQuoteFieldValue(variable_metadata.type());

  destination->append("SET GLOBAL "); /* Only global variables are replicated */
  destination->append(variable_metadata.name());
  destination->push_back('=');

  if (should_quote_field_value)
    destination->push_back('\'');
  
  destination->append(statement.value());

  if (should_quote_field_value)
    destination->push_back('\'');

  return NONE;
}

bool message::shouldQuoteFieldValue(message::Table::Field::FieldType in_type)
{
  switch (in_type)
  {
  case message::Table::Field::DOUBLE:
  case message::Table::Field::DECIMAL:
  case message::Table::Field::INTEGER:
  case message::Table::Field::BIGINT:
  case message::Table::Field::ENUM:
    return false;
  default:
    return true;
  } 
}
