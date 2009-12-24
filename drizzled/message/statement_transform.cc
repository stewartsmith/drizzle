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

#include "config.h"

#include "drizzled/message/statement_transform.h"
#include "drizzled/message/transaction.pb.h"
#include "drizzled/message/table.pb.h"

#include <string>
#include <vector>

using namespace std;
using namespace drizzled;

enum message::TransformSqlError
message::transformStatementToSql(const message::Statement &source,
                                 vector<string> &sql_strings,
                                 enum message::TransformSqlVariant sql_variant)
{
  message::TransformSqlError error= NONE;

  switch (source.type())
  {
  case message::Statement::INSERT:
    {
      if (! source.has_insert_header())
      {
        error= MISSING_HEADER;
        return error;
      }
      if (! source.has_insert_data())
      {
        error= MISSING_DATA;
        return error;
      }

      const message::InsertHeader &insert_header= source.insert_header();
      const message::InsertData &insert_data= source.insert_data();
      size_t num_keys= insert_data.record_size();
      size_t x;

      if (num_keys > 1)
        sql_strings.push_back("START TRANSACTION");

      for (x= 0; x < num_keys; ++x)
      {
        string destination;

        error= transformInsertRecordToSql(insert_header,
                                          insert_data.record(x),
                                          &destination,
                                          sql_variant);
        if (error != NONE)
          break;

        sql_strings.push_back(destination);
      }

      if (num_keys > 1)
      {
        if (error == NONE)
          sql_strings.push_back("COMMIT");
        else
          sql_strings.push_back("ROLLBACK");
      }
    }
    break;
  case message::Statement::UPDATE:
    {
      if (! source.has_update_header())
      {
        error= MISSING_HEADER;
        return error;
      }
      if (! source.has_update_data())
      {
        error= MISSING_DATA;
        return error;
      }

      const message::UpdateHeader &update_header= source.update_header();
      const message::UpdateData &update_data= source.update_data();
      size_t num_keys= update_data.record_size();
      size_t x;

      if (num_keys > 1)
        sql_strings.push_back("START TRANSACTION");

      for (x= 0; x < num_keys; ++x)
      {
        string destination;

        error= transformUpdateRecordToSql(update_header,
                                          update_data.record(x),
                                          &destination,
                                          sql_variant);
        if (error != NONE)
          break;

        sql_strings.push_back(destination);
      }

      if (num_keys > 1)
      {
        if (error == NONE)
          sql_strings.push_back("COMMIT");
        else
          sql_strings.push_back("ROLLBACK");
      }
    }
    break;
  case message::Statement::DELETE:
    {
      if (! source.has_delete_header())
      {
        error= MISSING_HEADER;
        return error;
      }
      if (! source.has_delete_data())
      {
        error= MISSING_DATA;
        return error;
      }

      const message::DeleteHeader &delete_header= source.delete_header();
      const message::DeleteData &delete_data= source.delete_data();
      size_t num_keys= delete_data.record_size();
      size_t x;

      if (num_keys > 1)
        sql_strings.push_back("START TRANSACTION");

      for (x= 0; x < num_keys; ++x)
      {
        string destination;

        error= transformDeleteRecordToSql(delete_header,
                                          delete_data.record(x),
                                          &destination,
                                          sql_variant);
        if (error != NONE)
          break;

        sql_strings.push_back(destination);
      }

      if (num_keys > 1)
      {
        if (error == NONE)
          sql_strings.push_back("COMMIT");
        else
          sql_strings.push_back("ROLLBACK");
      }
    }
    break;
  case message::Statement::TRUNCATE_TABLE:
    {
      assert(source.has_truncate_table_statement());
      string destination;
      error= message::transformTruncateTableStatementToSql(source.truncate_table_statement(),
                                                           &destination,
                                                           sql_variant);
      sql_strings.push_back(destination);
    }
    break;
  case message::Statement::SET_VARIABLE:
    {
      assert(source.has_set_variable_statement());
      string destination;
      error= message::transformSetVariableStatementToSql(source.set_variable_statement(),
                                                       &destination,
                                                       sql_variant);
      sql_strings.push_back(destination);
    }
    break;
  case message::Statement::RAW_SQL:
  default:
    sql_strings.push_back(source.sql());
    break;
  }
  return error;
}

enum message::TransformSqlError
message::transformInsertHeaderToSql(const message::InsertHeader &header,
                                    std::string *destination,
                                    enum message::TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination->assign("INSERT INTO ", 12);
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().schema_name());
  destination->push_back(quoted_identifier);
  destination->push_back('.');
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().table_name());
  destination->push_back(quoted_identifier);
  destination->append(" (", 2);

  /* Add field list to SQL string... */
  size_t num_fields= header.field_metadata_size();
  size_t x;

  for (x= 0; x < num_fields; ++x)
  {
    const message::FieldMetadata &field_metadata= header.field_metadata(x);
    if (x != 0)
      destination->push_back(',');
    
    destination->push_back(quoted_identifier);
    destination->append(field_metadata.name());
    destination->push_back(quoted_identifier);
  }

  return NONE;
}

enum message::TransformSqlError
message::transformInsertRecordToSql(const message::InsertHeader &header,
                                    const message::InsertRecord &record,
                                    std::string *destination,
                                    enum message::TransformSqlVariant sql_variant)
{
  enum message::TransformSqlError error= transformInsertHeaderToSql(header,
                                                                    destination,
                                                                    sql_variant);

  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination->append(") VALUES (");

  /* Add insert values */
  size_t num_fields= header.field_metadata_size();
  size_t x;
  bool should_quote_field_value= false;
  
  for (x= 0; x < num_fields; ++x)
  {
    if (x != 0)
      destination->push_back(',');

    const message::FieldMetadata &field_metadata= header.field_metadata(x);

    should_quote_field_value= message::shouldQuoteFieldValue(field_metadata.type());

    if (should_quote_field_value)
      destination->push_back('\'');

    if (field_metadata.type() == message::Table::Field::BLOB)
    {
      /* 
        * We do this here because BLOB data is returned
        * in a string correctly, but calling append()
        * without a length will result in only the string
        * up to a \0 being output here.
        */
      string raw_data(record.insert_value(x));
      destination->append(raw_data.c_str(), raw_data.size());
    }
    else
    {
      destination->append(record.insert_value(x));
    }

    if (should_quote_field_value)
      destination->push_back('\'');
  }
  destination->push_back(')');

  return error;
}

enum message::TransformSqlError
message::transformInsertStatementToSql(const message::InsertHeader &header,
                                       const message::InsertData &data,
                                       std::string *destination,
                                       enum message::TransformSqlVariant sql_variant)
{
  enum message::TransformSqlError error= transformInsertHeaderToSql(header,
                                                                    destination,
                                                                    sql_variant);

  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination->append(") VALUES (", 10);

  /* Add insert values */
  size_t num_records= data.record_size();
  size_t num_fields= header.field_metadata_size();
  size_t x, y;
  bool should_quote_field_value= false;
  
  for (x= 0; x < num_records; ++x)
  {
    if (x != 0)
      destination->append("),(", 3);

    for (y= 0; y < num_fields; ++y)
    {
      if (y != 0)
        destination->push_back(',');

      const message::FieldMetadata &field_metadata= header.field_metadata(y);
      
      should_quote_field_value= message::shouldQuoteFieldValue(field_metadata.type());

      if (should_quote_field_value)
        destination->push_back('\'');

      if (field_metadata.type() == message::Table::Field::BLOB)
      {
        /* 
         * We do this here because BLOB data is returned
         * in a string correctly, but calling append()
         * without a length will result in only the string
         * up to a \0 being output here.
         */
        string raw_data(data.record(x).insert_value(y));
        destination->append(raw_data.c_str(), raw_data.size());
      }
      else
      {
        destination->append(data.record(x).insert_value(y));
      }

      if (should_quote_field_value)
        destination->push_back('\'');
    }
  }
  destination->push_back(')');

  return error;
}

enum message::TransformSqlError
message::transformUpdateHeaderToSql(const message::UpdateHeader &header,
                                    std::string *destination,
                                    enum message::TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination->assign("UPDATE ", 7);
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().schema_name());
  destination->push_back(quoted_identifier);
  destination->push_back('.');
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().table_name());
  destination->push_back(quoted_identifier);
  destination->append(" SET ", 5);

  return NONE;
}

enum message::TransformSqlError
message::transformUpdateRecordToSql(const message::UpdateHeader &header,
                                    const message::UpdateRecord &record,
                                    std::string *destination,
                                    enum message::TransformSqlVariant sql_variant)
{
  enum message::TransformSqlError error= transformUpdateHeaderToSql(header,
                                                                    destination,
                                                                    sql_variant);

  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  /* Add field SET list to SQL string... */
  size_t num_set_fields= header.set_field_metadata_size();
  size_t x;
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

    if (field_metadata.type() == message::Table::Field::BLOB)
    {
      /* 
       * We do this here because BLOB data is returned
       * in a string correctly, but calling append()
       * without a length will result in only the string
       * up to a \0 being output here.
       */
      string raw_data(record.after_value(x));
      destination->append(raw_data.c_str(), raw_data.size());
    }
    else
    {
      destination->append(record.after_value(x));
    }

    if (should_quote_field_value)
      destination->push_back('\'');
  }

  size_t num_key_fields= header.key_field_metadata_size();

  destination->append(" WHERE ", 7);
  for (x= 0; x < num_key_fields; ++x) 
  {
    const message::FieldMetadata &field_metadata= header.key_field_metadata(x);
    
    if (x != 0)
      destination->append(" AND ", 5); /* Always AND condition with a multi-column PK */

    destination->push_back(quoted_identifier);
    destination->append(field_metadata.name());
    destination->push_back(quoted_identifier);

    destination->push_back('=');

    should_quote_field_value= message::shouldQuoteFieldValue(field_metadata.type());

    if (should_quote_field_value)
      destination->push_back('\'');

    if (field_metadata.type() == message::Table::Field::BLOB)
    {
      /* 
       * We do this here because BLOB data is returned
       * in a string correctly, but calling append()
       * without a length will result in only the string
       * up to a \0 being output here.
       */
      string raw_data(record.key_value(x));
      destination->append(raw_data.c_str(), raw_data.size());
    }
    else
    {
      destination->append(record.key_value(x));
    }

    if (should_quote_field_value)
      destination->push_back('\'');
  }
  if (num_key_fields > 1)
    destination->push_back(')');

  return error;
}

enum message::TransformSqlError
message::transformDeleteHeaderToSql(const message::DeleteHeader &header,
                                    std::string *destination,
                                    enum message::TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination->assign("DELETE FROM ", 12);
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().schema_name());
  destination->push_back(quoted_identifier);
  destination->push_back('.');
  destination->push_back(quoted_identifier);
  destination->append(header.table_metadata().table_name());
  destination->push_back(quoted_identifier);

  return NONE;
}

enum message::TransformSqlError
message::transformDeleteRecordToSql(const message::DeleteHeader &header,
                                    const message::DeleteRecord &record,
                                    std::string *destination,
                                    enum message::TransformSqlVariant sql_variant)
{
  enum message::TransformSqlError error= transformDeleteHeaderToSql(header,
                                                                    destination,
                                                                    sql_variant);
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  /* Add WHERE clause to SQL string... */
  uint32_t num_key_fields= header.key_field_metadata_size();
  uint32_t x;
  bool should_quote_field_value= false;

  destination->append(" WHERE ", 7);
  for (x= 0; x < num_key_fields; ++x) 
  {
    const message::FieldMetadata &field_metadata= header.key_field_metadata(x);
    
    if (x != 0)
      destination->append(" AND ", 5); /* Always AND condition with a multi-column PK */

    destination->push_back(quoted_identifier);
    destination->append(field_metadata.name());
    destination->push_back(quoted_identifier);

    destination->push_back('=');

    should_quote_field_value= message::shouldQuoteFieldValue(field_metadata.type());

    if (should_quote_field_value)
      destination->push_back('\'');

    if (field_metadata.type() == message::Table::Field::BLOB)
    {
      /* 
       * We do this here because BLOB data is returned
       * in a string correctly, but calling append()
       * without a length will result in only the string
       * up to a \0 being output here.
       */
      string raw_data(record.key_value(x));
      destination->append(raw_data.c_str(), raw_data.size());
    }
    else
    {
      destination->append(record.key_value(x));
    }

    if (should_quote_field_value)
      destination->push_back('\'');
  }

  return error;
}

enum message::TransformSqlError
message::transformDeleteStatementToSql(const message::DeleteHeader &header,
                                       const message::DeleteData &data,
                                       std::string *destination,
                                       enum message::TransformSqlVariant sql_variant)
{
  enum message::TransformSqlError error= transformDeleteHeaderToSql(header,
                                                                    destination,
                                                                    sql_variant);
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  /* Add WHERE clause to SQL string... */
  uint32_t num_key_fields= header.key_field_metadata_size();
  uint32_t num_key_records= data.record_size();
  uint32_t x, y;
  bool should_quote_field_value= false;

  destination->append(" WHERE ", 7);
  for (x= 0; x < num_key_records; ++x)
  {
    if (x != 0)
      destination->append(" OR ", 4); /* Always OR condition for multiple key records */

    if (num_key_fields > 1)
      destination->push_back('(');

    for (y= 0; y < num_key_fields; ++y) 
    {
      const message::FieldMetadata &field_metadata= header.key_field_metadata(y);
      
      if (y != 0)
        destination->append(" AND ", 5); /* Always AND condition with a multi-column PK */

      destination->push_back(quoted_identifier);
      destination->append(field_metadata.name());
      destination->push_back(quoted_identifier);

      destination->push_back('=');

      should_quote_field_value= message::shouldQuoteFieldValue(field_metadata.type());

      if (should_quote_field_value)
        destination->push_back('\'');

      if (field_metadata.type() == message::Table::Field::BLOB)
      {
        /* 
         * We do this here because BLOB data is returned
         * in a string correctly, but calling append()
         * without a length will result in only the string
         * up to a \0 being output here.
         */
        string raw_data(data.record(x).key_value(y));
        destination->append(raw_data.c_str(), raw_data.size());
      }
      else
      {
        destination->append(data.record(x).key_value(y));
      }

      if (should_quote_field_value)
        destination->push_back('\'');
    }
    if (num_key_fields > 1)
      destination->push_back(')');
  }
  return error;
}

enum message::TransformSqlError
message::transformTruncateTableStatementToSql(const message::TruncateTableStatement &statement,
                                              std::string *destination,
                                              enum message::TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  const message::TableMetadata &table_metadata= statement.table_metadata();

  destination->append("TRUNCATE TABLE ", 15);
  destination->push_back(quoted_identifier);
  destination->append(table_metadata.schema_name());
  destination->push_back(quoted_identifier);
  destination->push_back('.');
  destination->push_back(quoted_identifier);
  destination->append(table_metadata.table_name());
  destination->push_back(quoted_identifier);

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

  destination->append("SET GLOBAL ", 11); /* Only global variables are replicated */
  destination->append(variable_metadata.name());
  destination->push_back('=');

  if (should_quote_field_value)
    destination->push_back('\'');
  
  destination->append(statement.variable_value());

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
