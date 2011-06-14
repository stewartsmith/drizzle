/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Jay Pipes
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
 * Implementation of various routines that can be used to convert
 * Statement messages to other formats, including SQL strings.
 */

#include <config.h>

#include <boost/lexical_cast.hpp>

#include <drizzled/charset.h>
#include <drizzled/message.h>
#include <drizzled/message/statement_transform.h>
#include <drizzled/message/transaction.pb.h>
#include <drizzled/message/access.h>

#include <string>
#include <vector>
#include <sstream>
#include <cstdio>

using namespace std;

namespace drizzled
{

namespace message
{

static void escapeEmbeddedQuotes(string &s, const char quote='\'')
{
  string::iterator it;

  for (it= s.begin(); it != s.end(); ++it)
  {
    if (*it == quote)
    {
      it= s.insert(it, quote);
      ++it;  // advance back to the quote
    }
  }
}

/* Incredibly similar to append_unescaped() in table.cc, but for std::string */
static void append_escaped_string(std::string *res, const std::string &input, const char quote='\'')
{
  const char *pos= input.c_str();
  const char *end= input.c_str()+input.length();
  res->push_back(quote);

  for (; pos != end ; pos++)
  {
    uint32_t mblen;
    if (use_mb(default_charset_info) &&
        (mblen= my_ismbchar(default_charset_info, pos, end)))
    {
      res->append(pos, mblen);
      pos+= mblen - 1;
      if (pos >= end)
        break;
      continue;
    }

    switch (*pos) {
    case 0:				/* Must be escaped for 'mysql' */
      res->push_back('\\');
      res->push_back('0');
      break;
    case '\n':				/* Must be escaped for logs */
      res->push_back('\\');
      res->push_back('n');
      break;
    case '\r':
      res->push_back('\\');		/* This gives better readability */
      res->push_back('r');
      break;
    case '\\':
      res->push_back('\\');		/* Because of the sql syntax */
      res->push_back('\\');
      break;
    default:
      if (*pos == quote) /* SQL syntax for quoting a quote */
      {
        res->push_back(quote);
        res->push_back(quote);
      }
      else
        res->push_back(*pos);
      break;
    }
  }
  res->push_back(quote);
}

enum TransformSqlError
transformStatementToSql(const Statement &source,
                        vector<string> &sql_strings,
                        enum TransformSqlVariant sql_variant,
                        bool already_in_transaction)
{
  TransformSqlError error= NONE;

  switch (source.type())
  {
  case Statement::ROLLBACK_STATEMENT:
    {
      break;
    }
  case Statement::ROLLBACK:
    {
      sql_strings.push_back("ROLLBACK");
      break;
    }
  case Statement::INSERT:
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

      const InsertHeader &insert_header= source.insert_header();
      const InsertData &insert_data= source.insert_data();
      size_t num_keys= insert_data.record_size();

      if (num_keys > 1 && ! already_in_transaction)
        sql_strings.push_back("START TRANSACTION");

      for (size_t x= 0; x < num_keys; ++x)
      {
        string destination;

        error= transformInsertRecordToSql(insert_header,
                                          insert_data.record(x),
                                          destination,
                                          sql_variant);
        if (error != NONE)
          break;

        sql_strings.push_back(destination);
      }

      if (num_keys > 1 && ! already_in_transaction)
      {
        if (error == NONE)
          sql_strings.push_back("COMMIT");
        else
          sql_strings.push_back("ROLLBACK");
      }
    }
    break;
  case Statement::UPDATE:
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

      const UpdateHeader &update_header= source.update_header();
      const UpdateData &update_data= source.update_data();
      size_t num_keys= update_data.record_size();
      size_t x;

      if (num_keys > 1 && ! already_in_transaction)
        sql_strings.push_back("START TRANSACTION");

      for (x= 0; x < num_keys; ++x)
      {
        string destination;

        error= transformUpdateRecordToSql(update_header,
                                          update_data.record(x),
                                          destination,
                                          sql_variant);
        if (error != NONE)
          break;

        sql_strings.push_back(destination);
      }

      if (num_keys > 1 && ! already_in_transaction)
      {
        if (error == NONE)
          sql_strings.push_back("COMMIT");
        else
          sql_strings.push_back("ROLLBACK");
      }
    }
    break;
  case Statement::DELETE:
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

      const DeleteHeader &delete_header= source.delete_header();
      const DeleteData &delete_data= source.delete_data();
      size_t num_keys= delete_data.record_size();
      size_t x;

      if (num_keys > 1 && ! already_in_transaction)
        sql_strings.push_back("START TRANSACTION");

      for (x= 0; x < num_keys; ++x)
      {
        string destination;

        error= transformDeleteRecordToSql(delete_header,
                                          delete_data.record(x),
                                          destination,
                                          sql_variant);
        if (error != NONE)
          break;

        sql_strings.push_back(destination);
      }

      if (num_keys > 1 && ! already_in_transaction)
      {
        if (error == NONE)
          sql_strings.push_back("COMMIT");
        else
          sql_strings.push_back("ROLLBACK");
      }
    }
    break;
  case Statement::CREATE_TABLE:
    {
      assert(source.has_create_table_statement());
      string destination;
      error= transformCreateTableStatementToSql(source.create_table_statement(),
                                                destination,
                                                sql_variant);
      sql_strings.push_back(destination);
    }
    break;
  case Statement::TRUNCATE_TABLE:
    {
      assert(source.has_truncate_table_statement());
      string destination;
      error= transformTruncateTableStatementToSql(source.truncate_table_statement(),
                                                  destination,
                                                  sql_variant);
      sql_strings.push_back(destination);
    }
    break;
  case Statement::DROP_TABLE:
    {
      assert(source.has_drop_table_statement());
      string destination;
      error= transformDropTableStatementToSql(source.drop_table_statement(),
                                              destination,
                                              sql_variant);
      sql_strings.push_back(destination);
    }
    break;
  case Statement::CREATE_SCHEMA:
    {
      assert(source.has_create_schema_statement());
      string destination;
      error= transformCreateSchemaStatementToSql(source.create_schema_statement(),
                                                 destination,
                                                 sql_variant);
      sql_strings.push_back(destination);
    }
    break;
  case Statement::DROP_SCHEMA:
    {
      assert(source.has_drop_schema_statement());
      string destination;
      error= transformDropSchemaStatementToSql(source.drop_schema_statement(),
                                               destination,
                                               sql_variant);
      sql_strings.push_back(destination);
    }
    break;
  case Statement::ALTER_SCHEMA:
    {
      assert(source.has_alter_schema_statement());
      string destination;
      error= transformAlterSchemaStatementToSql(source.alter_schema_statement(),
                                                destination,
                                                sql_variant);
      sql_strings.push_back(destination);
    }
    break;
  case Statement::SET_VARIABLE:
    {
      assert(source.has_set_variable_statement());
      string destination;
      error= transformSetVariableStatementToSql(source.set_variable_statement(),
                                                destination,
                                                sql_variant);
      sql_strings.push_back(destination);
    }
    break;
  case Statement::RAW_SQL:
    {
      if (source.has_raw_sql_schema())
      {
        string destination("USE ");
        destination.append(source.raw_sql_schema());
        sql_strings.push_back(destination);
      }
      sql_strings.push_back(source.sql());
    }
    break;
  default:
    sql_strings.push_back(source.sql());
    break;
  }
  return error;
}

enum TransformSqlError
transformInsertHeaderToSql(const InsertHeader &header,
                           string &destination,
                           enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination.assign("INSERT INTO ", 12);
  destination.push_back(quoted_identifier);
  destination.append(header.table_metadata().schema_name());
  destination.push_back(quoted_identifier);
  destination.push_back('.');
  destination.push_back(quoted_identifier);
  destination.append(header.table_metadata().table_name());
  destination.push_back(quoted_identifier);
  destination.append(" (", 2);

  /* Add field list to SQL string... */
  size_t num_fields= header.field_metadata_size();
  size_t x;

  for (x= 0; x < num_fields; ++x)
  {
    const FieldMetadata &field_metadata= header.field_metadata(x);
    if (x != 0)
      destination.push_back(',');
    
    destination.push_back(quoted_identifier);
    destination.append(field_metadata.name());
    destination.push_back(quoted_identifier);
  }

  return NONE;
}

enum TransformSqlError
transformInsertRecordToSql(const InsertHeader &header,
                           const InsertRecord &record,
                           string &destination,
                           enum TransformSqlVariant sql_variant)
{
  enum TransformSqlError error= transformInsertHeaderToSql(header,
                                                           destination,
                                                           sql_variant);

  destination.append(") VALUES (");

  /* Add insert values */
  size_t num_fields= header.field_metadata_size();
  size_t x;
  bool should_quote_field_value= false;
  
  for (x= 0; x < num_fields; ++x)
  {
    if (x != 0)
      destination.push_back(',');

    const FieldMetadata &field_metadata= header.field_metadata(x);

    if (record.is_null(x))
    {
      should_quote_field_value= false;
    }
    else 
    {
      should_quote_field_value= shouldQuoteFieldValue(field_metadata.type());
    }

    if (should_quote_field_value)
      destination.push_back('\'');

    if (record.is_null(x))
    {
      destination.append("NULL");
    }
    else
    {
      if (field_metadata.type() == Table::Field::BLOB)
      {
        /*
         * We do this here because BLOB data is returned
         * in a string correctly, but calling append()
         * without a length will result in only the string
         * up to a \0 being output here.
         */
        string raw_data(record.insert_value(x));
        destination.append(raw_data.c_str(), raw_data.size());
      }
      else
      {
        string tmp(record.insert_value(x));
        escapeEmbeddedQuotes(tmp);
        destination.append(tmp);
      }
    }

    if (should_quote_field_value)
      destination.push_back('\'');
  }
  destination.push_back(')');

  return error;
}

enum TransformSqlError
transformInsertStatementToSql(const InsertHeader &header,
                              const InsertData &data,
                              string &destination,
                              enum TransformSqlVariant sql_variant)
{
  enum TransformSqlError error= transformInsertHeaderToSql(header,
                                                           destination,
                                                           sql_variant);

  destination.append(") VALUES (", 10);

  /* Add insert values */
  size_t num_records= data.record_size();
  size_t num_fields= header.field_metadata_size();
  size_t x, y;
  bool should_quote_field_value= false;
  
  for (x= 0; x < num_records; ++x)
  {
    if (x != 0)
      destination.append("),(", 3);

    for (y= 0; y < num_fields; ++y)
    {
      if (y != 0)
        destination.push_back(',');

      const FieldMetadata &field_metadata= header.field_metadata(y);
      
      should_quote_field_value= shouldQuoteFieldValue(field_metadata.type());

      if (should_quote_field_value)
        destination.push_back('\'');

      if (field_metadata.type() == Table::Field::BLOB)
      {
        /* 
         * We do this here because BLOB data is returned
         * in a string correctly, but calling append()
         * without a length will result in only the string
         * up to a \0 being output here.
         */
        string raw_data(data.record(x).insert_value(y));
        destination.append(raw_data.c_str(), raw_data.size());
      }
      else
      {
        string tmp(data.record(x).insert_value(y));
        escapeEmbeddedQuotes(tmp);
        destination.append(tmp);
      }

      if (should_quote_field_value)
        destination.push_back('\'');
    }
  }
  destination.push_back(')');

  return error;
}

enum TransformSqlError
transformUpdateHeaderToSql(const UpdateHeader &header,
                           string &destination,
                           enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination.assign("UPDATE ", 7);
  destination.push_back(quoted_identifier);
  destination.append(header.table_metadata().schema_name());
  destination.push_back(quoted_identifier);
  destination.push_back('.');
  destination.push_back(quoted_identifier);
  destination.append(header.table_metadata().table_name());
  destination.push_back(quoted_identifier);
  destination.append(" SET ", 5);

  return NONE;
}

enum TransformSqlError
transformUpdateRecordToSql(const UpdateHeader &header,
                           const UpdateRecord &record,
                           string &destination,
                           enum TransformSqlVariant sql_variant)
{
  enum TransformSqlError error= transformUpdateHeaderToSql(header,
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
    const FieldMetadata &field_metadata= header.set_field_metadata(x);
    if (x != 0)
      destination.push_back(',');
    
    destination.push_back(quoted_identifier);
    destination.append(field_metadata.name());
    destination.push_back(quoted_identifier);
    destination.push_back('=');

    if (record.is_null(x))
    {
      should_quote_field_value= false;
    }
    else 
    {
      should_quote_field_value= shouldQuoteFieldValue(field_metadata.type());
    }    

    if (should_quote_field_value)
      destination.push_back('\'');

    if (record.is_null(x))
    {
      destination.append("NULL");
    }
    else 
    {
      if (field_metadata.type() == Table::Field::BLOB)
      {
        /*
         * We do this here because BLOB data is returned
         * in a string correctly, but calling append()
         * without a length will result in only the string
         * up to a \0 being output here.
         */
        string raw_data(record.after_value(x));
        destination.append(raw_data.c_str(), raw_data.size());
      }
      else 
      {
        string tmp(record.after_value(x));
        escapeEmbeddedQuotes(tmp);
        destination.append(tmp);
      }
    }

    if (should_quote_field_value)
      destination.push_back('\'');
  }

  size_t num_key_fields= header.key_field_metadata_size();

  destination.append(" WHERE ", 7);
  for (x= 0; x < num_key_fields; ++x) 
  {
    const FieldMetadata &field_metadata= header.key_field_metadata(x);
    
    if (x != 0)
      destination.append(" AND ", 5); /* Always AND condition with a multi-column PK */

    destination.push_back(quoted_identifier);
    destination.append(field_metadata.name());
    destination.push_back(quoted_identifier);

    destination.push_back('=');

    should_quote_field_value= shouldQuoteFieldValue(field_metadata.type());

    if (should_quote_field_value)
      destination.push_back('\'');

    if (field_metadata.type() == Table::Field::BLOB)
    {
      /* 
       * We do this here because BLOB data is returned
       * in a string correctly, but calling append()
       * without a length will result in only the string
       * up to a \0 being output here.
       */
      string raw_data(record.key_value(x));
      destination.append(raw_data.c_str(), raw_data.size());
    }
    else
    {
      destination.append(record.key_value(x));
    }

    if (should_quote_field_value)
      destination.push_back('\'');
  }

  return error;
}

enum TransformSqlError
transformDeleteHeaderToSql(const DeleteHeader &header,
                           string &destination,
                           enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination.assign("DELETE FROM ", 12);
  destination.push_back(quoted_identifier);
  destination.append(header.table_metadata().schema_name());
  destination.push_back(quoted_identifier);
  destination.push_back('.');
  destination.push_back(quoted_identifier);
  destination.append(header.table_metadata().table_name());
  destination.push_back(quoted_identifier);

  return NONE;
}

enum TransformSqlError
transformDeleteRecordToSql(const DeleteHeader &header,
                           const DeleteRecord &record,
                           string &destination,
                           enum TransformSqlVariant sql_variant)
{
  enum TransformSqlError error= transformDeleteHeaderToSql(header,
                                                           destination,
                                                           sql_variant);
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  /* Add WHERE clause to SQL string... */
  uint32_t num_key_fields= header.key_field_metadata_size();
  uint32_t x;
  bool should_quote_field_value= false;

  destination.append(" WHERE ", 7);
  for (x= 0; x < num_key_fields; ++x) 
  {
    const FieldMetadata &field_metadata= header.key_field_metadata(x);
    
    if (x != 0)
      destination.append(" AND ", 5); /* Always AND condition with a multi-column PK */

    destination.push_back(quoted_identifier);
    destination.append(field_metadata.name());
    destination.push_back(quoted_identifier);

    destination.push_back('=');

    should_quote_field_value= shouldQuoteFieldValue(field_metadata.type());

    if (should_quote_field_value)
      destination.push_back('\'');

    if (field_metadata.type() == Table::Field::BLOB)
    {
      /* 
       * We do this here because BLOB data is returned
       * in a string correctly, but calling append()
       * without a length will result in only the string
       * up to a \0 being output here.
       */
      string raw_data(record.key_value(x));
      destination.append(raw_data.c_str(), raw_data.size());
    }
    else
    {
      string tmp(record.key_value(x));
      escapeEmbeddedQuotes(tmp);
      destination.append(tmp);
    }

    if (should_quote_field_value)
      destination.push_back('\'');
  }

  return error;
}

enum TransformSqlError
transformDeleteStatementToSql(const DeleteHeader &header,
                              const DeleteData &data,
                              string &destination,
                              enum TransformSqlVariant sql_variant)
{
  enum TransformSqlError error= transformDeleteHeaderToSql(header,
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

  destination.append(" WHERE ", 7);
  for (x= 0; x < num_key_records; ++x)
  {
    if (x != 0)
      destination.append(" OR ", 4); /* Always OR condition for multiple key records */

    if (num_key_fields > 1)
      destination.push_back('(');

    for (y= 0; y < num_key_fields; ++y) 
    {
      const FieldMetadata &field_metadata= header.key_field_metadata(y);
      
      if (y != 0)
        destination.append(" AND ", 5); /* Always AND condition with a multi-column PK */

      destination.push_back(quoted_identifier);
      destination.append(field_metadata.name());
      destination.push_back(quoted_identifier);

      destination.push_back('=');

      should_quote_field_value= shouldQuoteFieldValue(field_metadata.type());

      if (should_quote_field_value)
        destination.push_back('\'');

      if (field_metadata.type() == Table::Field::BLOB)
      {
        /* 
         * We do this here because BLOB data is returned
         * in a string correctly, but calling append()
         * without a length will result in only the string
         * up to a \0 being output here.
         */
        string raw_data(data.record(x).key_value(y));
        destination.append(raw_data.c_str(), raw_data.size());
      }
      else
      {
        string tmp(data.record(x).key_value(y));
        escapeEmbeddedQuotes(tmp);
        destination.append(tmp);
      }

      if (should_quote_field_value)
        destination.push_back('\'');
    }
    if (num_key_fields > 1)
      destination.push_back(')');
  }
  return error;
}

enum TransformSqlError
transformAlterSchemaStatementToSql(const AlterSchemaStatement &statement,
                                   string &destination,
                                   enum TransformSqlVariant sql_variant)
{
  const Schema &before= statement.before();
  const Schema &after= statement.after();

  /* Make sure we are given the before and after for the same object */
  if (before.uuid() != after.uuid())
    return UUID_MISMATCH;

  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination.append("ALTER SCHEMA ");
  destination.push_back(quoted_identifier);
  destination.append(before.name());
  destination.push_back(quoted_identifier);

  /*
   * Diff our schemas. Currently, only collation can change so a
   * diff of the two structures is not really necessary.
   */
  destination.append(" COLLATE = ");
  destination.append(after.collation());

  return NONE;
}

enum TransformSqlError
transformDropSchemaStatementToSql(const DropSchemaStatement &statement,
                                  string &destination,
                                  enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination.append("DROP SCHEMA ", 12);
  destination.push_back(quoted_identifier);
  destination.append(statement.schema_name());
  destination.push_back(quoted_identifier);

  return NONE;
}

enum TransformSqlError
transformCreateSchemaStatementToSql(const CreateSchemaStatement &statement,
                                    string &destination,
                                    enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  const Schema &schema= statement.schema();

  destination.append("CREATE SCHEMA ");
  destination.push_back(quoted_identifier);
  destination.append(schema.name());
  destination.push_back(quoted_identifier);

  if (schema.has_collation())
  {
    destination.append(" COLLATE ");
    destination.append(schema.collation());
  }

  if (not message::is_replicated(schema))
  {
    destination.append(" REPLICATE = FALSE");
  }

  if (message::has_definer(schema))
  {
    destination.append(" DEFINER ");
    destination.push_back('\'');
    destination.append(message::definer(schema));
    destination.push_back('\'');
  }

  return NONE;
}

enum TransformSqlError
transformDropTableStatementToSql(const DropTableStatement &statement,
                                 string &destination,
                                 enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  const TableMetadata &table_metadata= statement.table_metadata();

  destination.append("DROP TABLE ");

  /* Add the IF EXISTS clause if necessary */
  if (statement.has_if_exists_clause() &&
      statement.if_exists_clause() == true)
  {
    destination.append("IF EXISTS ");
  }

  destination.push_back(quoted_identifier);
  destination.append(table_metadata.schema_name());
  destination.push_back(quoted_identifier);
  destination.push_back('.');
  destination.push_back(quoted_identifier);
  destination.append(table_metadata.table_name());
  destination.push_back(quoted_identifier);

  return NONE;
}

enum TransformSqlError
transformTruncateTableStatementToSql(const TruncateTableStatement &statement,
                                     string &destination,
                                     enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  const TableMetadata &table_metadata= statement.table_metadata();

  destination.append("TRUNCATE TABLE ");
  destination.push_back(quoted_identifier);
  destination.append(table_metadata.schema_name());
  destination.push_back(quoted_identifier);
  destination.push_back('.');
  destination.push_back(quoted_identifier);
  destination.append(table_metadata.table_name());
  destination.push_back(quoted_identifier);

  return NONE;
}

enum TransformSqlError
transformSetVariableStatementToSql(const SetVariableStatement &statement,
                                   string &destination,
                                   enum TransformSqlVariant sql_variant)
{
  (void) sql_variant;
  const FieldMetadata &variable_metadata= statement.variable_metadata();
  bool should_quote_field_value= shouldQuoteFieldValue(variable_metadata.type());

  destination.append("SET GLOBAL "); /* Only global variables are replicated */
  destination.append(variable_metadata.name());
  destination.push_back('=');

  if (should_quote_field_value)
    destination.push_back('\'');
  
  destination.append(statement.variable_value());

  if (should_quote_field_value)
    destination.push_back('\'');

  return NONE;
}

enum TransformSqlError
transformCreateTableStatementToSql(const CreateTableStatement &statement,
                                   string &destination,
                                   enum TransformSqlVariant sql_variant)
{
  return transformTableDefinitionToSql(statement.table(), destination, sql_variant);
}

enum TransformSqlError
transformTableDefinitionToSql(const Table &table,
                              string &destination,
                              enum TransformSqlVariant sql_variant, bool with_schema)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination.append("CREATE ");

  if (table.type() == Table::TEMPORARY)
    destination.append("TEMPORARY ");
  
  destination.append("TABLE ");
  if (with_schema)
  {
    append_escaped_string(&destination, table.schema(), quoted_identifier);
    destination.push_back('.');
  }
  append_escaped_string(&destination, table.name(), quoted_identifier);
  destination.append(" (\n");

  enum TransformSqlError result= NONE;
  size_t num_fields= table.field_size();
  for (size_t x= 0; x < num_fields; ++x)
  {
    const Table::Field &field= table.field(x);

    if (x != 0)
      destination.append(",\n");

    destination.append("  ");

    result= transformFieldDefinitionToSql(field, destination, sql_variant);

    if (result != NONE)
      return result;
  }

  size_t num_indexes= table.indexes_size();
  
  if (num_indexes > 0)
    destination.append(",\n");

  for (size_t x= 0; x < num_indexes; ++x)
  {
    const message::Table::Index &index= table.indexes(x);

    if (x != 0)
      destination.append(",\n");

    result= transformIndexDefinitionToSql(index, table, destination, sql_variant);
    
    if (result != NONE)
      return result;
  }

  size_t num_foreign_keys= table.fk_constraint_size();

  if (num_foreign_keys > 0)
    destination.append(",\n");

  for (size_t x= 0; x < num_foreign_keys; ++x)
  {
    const message::Table::ForeignKeyConstraint &fkey= table.fk_constraint(x);

    if (x != 0)
      destination.append(",\n");

    result= transformForeignKeyConstraintDefinitionToSql(fkey, table, destination, sql_variant);

    if (result != NONE)
      return result;
  }

  destination.append("\n)");

  /* Add ENGINE = " clause */
  if (table.has_engine())
  {
    destination.append(" ENGINE=");
    destination.append(table.engine().name());

    size_t num_engine_options= table.engine().options_size();
    if (num_engine_options > 0)
      destination.append(" ", 1);
    for (size_t x= 0; x < num_engine_options; ++x)
    {
      const Engine::Option &option= table.engine().options(x);
      destination.append(option.name());
      destination.append("='");
      destination.append(option.state());
      destination.append("'");
      if (x != num_engine_options-1)
      {
        destination.append(", ");
      }
    }
  }

  if (table.has_options())
    (void) transformTableOptionsToSql(table.options(), destination, sql_variant);

  if (not message::is_replicated(table))
  {
    destination.append(" REPLICATE = FALSE");
  }

  if (message::has_definer(table))
  {
    destination.append(" DEFINER ");
    destination.push_back('\'');
    destination.append(message::definer(table));
    destination.push_back('\'');
  }

  return NONE;
}

enum TransformSqlError
transformTableOptionsToSql(const Table::TableOptions &options,
                           string &destination,
                           enum TransformSqlVariant sql_variant)
{
  if (sql_variant == ANSI)
    return NONE; /* ANSI does not support table options... */

  if (options.has_comment())
  {
    destination.append(" COMMENT=");
    append_escaped_string(&destination, options.comment());
  }

  if (options.has_collation())
  {
    destination.append(" COLLATE = ");
    destination.append(options.collation());
  }

  if (options.has_data_file_name())
  {
    destination.append("\nDATA_FILE_NAME = '");
    destination.append(options.data_file_name());
    destination.push_back('\'');
  }

  if (options.has_index_file_name())
  {
    destination.append("\nINDEX_FILE_NAME = '");
    destination.append(options.index_file_name());
    destination.push_back('\'');
  }

  if (options.has_max_rows())
  {
    destination.append("\nMAX_ROWS = ");
    destination.append(boost::lexical_cast<string>(options.max_rows()));
  }

  if (options.has_min_rows())
  {
    destination.append("\nMIN_ROWS = ");
    destination.append(boost::lexical_cast<string>(options.min_rows()));
  }

  if (options.has_user_set_auto_increment_value()
      && options.has_auto_increment_value())
  {
    destination.append(" AUTO_INCREMENT=");
    destination.append(boost::lexical_cast<string>(options.auto_increment_value()));
  }

  if (options.has_avg_row_length())
  {
    destination.append("\nAVG_ROW_LENGTH = ");
    destination.append(boost::lexical_cast<string>(options.avg_row_length()));
  }

  if (options.has_checksum() && options.checksum())
    destination.append("\nCHECKSUM = TRUE");

  if (options.has_page_checksum() && options.page_checksum())
    destination.append("\nPAGE_CHECKSUM = TRUE");

  return NONE;
}

enum TransformSqlError
transformIndexDefinitionToSql(const Table::Index &index,
                              const Table &table,
                              string &destination,
                              enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination.append("  ", 2);

  if (index.is_primary())
    destination.append("PRIMARY ");
  else if (index.is_unique())
    destination.append("UNIQUE ");

  destination.append("KEY ", 4);
  if (! (index.is_primary() && index.name().compare("PRIMARY")==0))
  {
    destination.push_back(quoted_identifier);
    destination.append(index.name());
    destination.push_back(quoted_identifier);
    destination.append(" (", 2);
  }
  else
    destination.append("(", 1);

  size_t num_parts= index.index_part_size();
  for (size_t x= 0; x < num_parts; ++x)
  {
    const Table::Index::IndexPart &part= index.index_part(x);
    const Table::Field &field= table.field(part.fieldnr());

    if (x != 0)
      destination.push_back(',');
    
    destination.push_back(quoted_identifier);
    destination.append(field.name());
    destination.push_back(quoted_identifier);

    /* 
     * If the index part's field type is VARCHAR or TEXT
     * then check for a prefix length then is different
     * from the field's full length...
     */
    if (field.type() == Table::Field::VARCHAR ||
        field.type() == Table::Field::BLOB)
    {
      if (part.has_compare_length())
      {
        if (part.compare_length() != field.string_options().length())
        {
          destination.push_back('(');
          destination.append(boost::lexical_cast<string>(part.compare_length()));
          destination.push_back(')');
        }
      }
    }
  }
  destination.push_back(')');

  switch (index.type())
  {
  case Table::Index::UNKNOWN_INDEX:
    break;
  case Table::Index::BTREE:
    destination.append(" USING BTREE");
    break;
  case Table::Index::RTREE:
    destination.append(" USING RTREE");
    break;
  case Table::Index::HASH:
    destination.append(" USING HASH");
    break;
  case Table::Index::FULLTEXT:
    destination.append(" USING FULLTEXT");
    break;
  }

  if (index.has_comment())
  {
    destination.append(" COMMENT ");
    append_escaped_string(&destination, index.comment());
  }

  return NONE;
}

static void transformForeignKeyOptionToSql(Table::ForeignKeyConstraint::ForeignKeyOption opt, string &destination)
{
  switch (opt)
  {
  case Table::ForeignKeyConstraint::OPTION_RESTRICT:
    destination.append("RESTRICT");
    break;
  case Table::ForeignKeyConstraint::OPTION_CASCADE:
    destination.append("CASCADE");
    break;
  case Table::ForeignKeyConstraint::OPTION_SET_NULL:
    destination.append("SET NULL");
    break;
  case Table::ForeignKeyConstraint::OPTION_UNDEF:
  case Table::ForeignKeyConstraint::OPTION_NO_ACTION:
    destination.append("NO ACTION");
    break;
  case Table::ForeignKeyConstraint::OPTION_SET_DEFAULT:
    destination.append("SET DEFAULT");
    break;
  }
}

enum TransformSqlError
transformForeignKeyConstraintDefinitionToSql(const Table::ForeignKeyConstraint &fkey,
                                             const Table &,
                                             string &destination,
                                             enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  if (sql_variant == ANSI)
    quoted_identifier= '"';

  destination.append("  ");

  if (fkey.has_name())
  {
    destination.append("CONSTRAINT ");
    append_escaped_string(&destination, fkey.name(), quoted_identifier);
    destination.append(" ", 1);
  }

  destination.append("FOREIGN KEY (");

  for (ssize_t x= 0; x < fkey.column_names_size(); ++x)
  {
    if (x != 0)
      destination.append(", ");

    append_escaped_string(&destination, fkey.column_names(x),
                          quoted_identifier);
  }

  destination.append(") REFERENCES ");

  append_escaped_string(&destination, fkey.references_table_name(),
                        quoted_identifier);
  destination.append(" (");

  for (ssize_t x= 0; x < fkey.references_columns_size(); ++x)
  {
    if (x != 0)
      destination.append(", ");

    append_escaped_string(&destination, fkey.references_columns(x),
                          quoted_identifier);
  }

  destination.push_back(')');

  if (fkey.has_update_option() and fkey.update_option() != Table::ForeignKeyConstraint::OPTION_UNDEF)
  {
    destination.append(" ON UPDATE ");
    transformForeignKeyOptionToSql(fkey.update_option(), destination);
  }

  if (fkey.has_delete_option() and fkey.delete_option() != Table::ForeignKeyConstraint::OPTION_UNDEF)
  {
    destination.append(" ON DELETE ");
    transformForeignKeyOptionToSql(fkey.delete_option(), destination);
  }

  return NONE;
}

enum TransformSqlError
transformFieldDefinitionToSql(const Table::Field &field,
                              string &destination,
                              enum TransformSqlVariant sql_variant)
{
  char quoted_identifier= '`';
  char quoted_default;

  if (sql_variant == ANSI)
    quoted_identifier= '"';

  if (sql_variant == DRIZZLE)
    quoted_default= '\'';
  else
    quoted_default= quoted_identifier;

  append_escaped_string(&destination, field.name(), quoted_identifier);

  Table::Field::FieldType field_type= field.type();

  switch (field_type)
  {
    case Table::Field::DOUBLE:
    destination.append(" DOUBLE");
    if (field.has_numeric_options()
        && field.numeric_options().has_precision())
    {
      stringstream ss;
      ss << "(" << field.numeric_options().precision() << ",";
      ss << field.numeric_options().scale() << ")";
      destination.append(ss.str());
    }
    break;
  case Table::Field::VARCHAR:
    {
      if (field.string_options().has_collation()
          && field.string_options().collation().compare("binary") == 0)
        destination.append(" VARBINARY(");
      else
        destination.append(" VARCHAR(");

      destination.append(boost::lexical_cast<string>(field.string_options().length()));
      destination.append(")");
    }
    break;
  case Table::Field::BLOB:
    {
      if (field.string_options().has_collation()
          && field.string_options().collation().compare("binary") == 0)
        destination.append(" BLOB");
      else
        destination.append(" TEXT");
    }
    break;
  case Table::Field::ENUM:
    {
      size_t num_field_values= field.enumeration_values().field_value_size();
      destination.append(" ENUM(");
      for (size_t x= 0; x < num_field_values; ++x)
      {
        const string &type= field.enumeration_values().field_value(x);

        if (x != 0)
          destination.push_back(',');

        destination.push_back('\'');
        destination.append(type);
        destination.push_back('\'');
      }
      destination.push_back(')');
      break;
    }
  case Table::Field::UUID:
    destination.append(" UUID");
    break;
  case Table::Field::BOOLEAN:
    destination.append(" BOOLEAN");
    break;
  case Table::Field::INTEGER:
    destination.append(" INT");
    break;
  case Table::Field::BIGINT:
    if (field.has_constraints() and
        field.constraints().is_unsigned())
    {
      destination.append(" BIGINT UNSIGNED");
    }
    else
    {
      destination.append(" BIGINT");
    }
    break;
  case Table::Field::DECIMAL:
    {
      destination.append(" DECIMAL(");
      stringstream ss;
      ss << field.numeric_options().precision() << ",";
      ss << field.numeric_options().scale() << ")";
      destination.append(ss.str());
    }
    break;
  case Table::Field::DATE:
    destination.append(" DATE");
    break;

  case Table::Field::EPOCH:
    if (field.time_options().microseconds())
    {
      destination.append(" TIMESTAMP(6)");
    }
    else
    {
      destination.append(" TIMESTAMP");
    }
    break;

  case Table::Field::DATETIME:
    destination.append(" DATETIME");
    break;
  case Table::Field::TIME:
    destination.append(" TIME");
    break;
  }

  if (field.type() == Table::Field::BLOB ||
      field.type() == Table::Field::VARCHAR)
  {
    if (field.string_options().has_collation()
        && field.string_options().collation().compare("binary"))
    {
      destination.append(" COLLATE ");
      destination.append(field.string_options().collation());
    }
  }

  if (field.has_constraints() and field.constraints().is_unique())
  {
    destination.append(" UNIQUE");
  }

  if (field.has_constraints() && field.constraints().is_notnull())
  {
    destination.append(" NOT NULL");
  }
  else if (field.type() == Table::Field::EPOCH)
  {
    destination.append(" NULL");
  }

  if (field.type() == Table::Field::INTEGER || 
      field.type() == Table::Field::BIGINT)
  {
    /* AUTO_INCREMENT must be after NOT NULL */
    if (field.has_numeric_options() &&
        field.numeric_options().is_autoincrement())
    {
      destination.append(" AUTO_INCREMENT");
    }
  }

  if (field.options().has_default_value())
  {
    destination.append(" DEFAULT ");
    append_escaped_string(&destination, field.options().default_value());
  }
  else if (field.options().has_default_expression())
  {
    destination.append(" DEFAULT ");
    destination.append(field.options().default_expression());
  }
  else if (field.options().has_default_bin_value())
  {
    const string &v= field.options().default_bin_value();
    if (v.length() == 0)
    {
      destination.append(" DEFAULT ''");
    }
    else
    {
      destination.append(" DEFAULT 0x");
      for (size_t x= 0; x < v.length(); x++)
      {
        char hex[3];
        snprintf(hex, sizeof(hex), "%.2X", *(v.c_str() + x));
        destination.append(hex, 2);
      }
    }
  }
  else if (field.options().has_default_null()
           && field.options().default_null()
           && field.type() != Table::Field::BLOB)
  {
    destination.append(" DEFAULT NULL");
  }

  if (field.has_options() && field.options().has_update_expression())
  {
    destination.append(" ON UPDATE ");
    destination.append(field.options().update_expression());
  }

  if (field.has_comment())
  {
    destination.append(" COMMENT ");
    append_escaped_string(&destination, field.comment(), quoted_default);
  }
  return NONE;
}

bool shouldQuoteFieldValue(Table::Field::FieldType in_type)
{
  switch (in_type)
  {
  case Table::Field::DOUBLE:
  case Table::Field::DECIMAL:
  case Table::Field::INTEGER:
  case Table::Field::BIGINT:
    return false;
  default:
    return true;
  } 
}

Table::Field::FieldType internalFieldTypeToFieldProtoType(enum enum_field_types type)
{
  switch (type) {
  case DRIZZLE_TYPE_LONG:
    return Table::Field::INTEGER;
  case DRIZZLE_TYPE_DOUBLE:
    return Table::Field::DOUBLE;
  case DRIZZLE_TYPE_NULL:
    assert(false); /* Not a user definable type */
    return Table::Field::INTEGER; /* unreachable */
  case DRIZZLE_TYPE_MICROTIME:
  case DRIZZLE_TYPE_TIMESTAMP:
    return Table::Field::EPOCH;
  case DRIZZLE_TYPE_LONGLONG:
    return Table::Field::BIGINT;
  case DRIZZLE_TYPE_DATETIME:
    return Table::Field::DATETIME;
  case DRIZZLE_TYPE_TIME:
    return Table::Field::TIME;
  case DRIZZLE_TYPE_DATE:
    return Table::Field::DATE;
  case DRIZZLE_TYPE_VARCHAR:
    return Table::Field::VARCHAR;
  case DRIZZLE_TYPE_DECIMAL:
    return Table::Field::DECIMAL;
  case DRIZZLE_TYPE_ENUM:
    return Table::Field::ENUM;
  case DRIZZLE_TYPE_BLOB:
    return Table::Field::BLOB;
  case DRIZZLE_TYPE_UUID:
    return Table::Field::UUID;
  case DRIZZLE_TYPE_BOOLEAN:
    return Table::Field::BOOLEAN;
  }

  assert(false);
  return Table::Field::INTEGER; /* unreachable */
}

bool transactionContainsBulkSegment(const Transaction &transaction)
{
  size_t num_statements= transaction.statement_size();
  if (num_statements == 0)
    return false;

  /*
   * Only INSERT, UPDATE, and DELETE statements can possibly
   * have bulk segments.  So, we loop through the statements
   * checking for segment_id > 1 in those specific submessages.
   */
  size_t x;
  for (x= 0; x < num_statements; ++x)
  {
    const Statement &statement= transaction.statement(x);
    Statement::Type type= statement.type();

    switch (type)
    {
      case Statement::INSERT:
        if (statement.insert_data().segment_id() > 1)
          return true;
        break;
      case Statement::UPDATE:
        if (statement.update_data().segment_id() > 1)
          return true;
        break;
      case Statement::DELETE:
        if (statement.delete_data().segment_id() > 1)
          return true;
        break;
      default:
        break;
    }
  }
  return false;
}

} /* namespace message */
} /* namespace drizzled */
