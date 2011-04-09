/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <config.h>

#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/schema.h>
#include <drizzled/plugin/event_observer.h>
#include <drizzled/message.h>

#include <string>

namespace drizzled {
namespace message {

static const std::string PROGRAM_ERROR("PROGRAM_ERROR");

// These are used to generate strings for types
static const std::string VARCHAR("VARCHAR");
static const std::string VARBINARY("VARBINARY");
static const std::string DOUBLE("DOUBLE");
static const std::string TEXT("TEXT");
static const std::string BLOB("BLOB");
static const std::string ENUM("ENUM");
static const std::string INTEGER("INTEGER");
static const std::string BIGINT("BIGINT");
static const std::string DECIMAL("DECIMAL");
static const std::string DATE("DATE");
static const std::string EPOCH("EPOCH");
static const std::string TIMESTAMP("TIMESTAMP");
static const std::string MICROTIME("MICROTIME");
static const std::string DATETIME("DATETIME");
static const std::string TIME("TIME");
static const std::string UUID("UUID");
static const std::string BOOLEAN("BOOLEAN");

static const std::string UNDEFINED("UNDEFINED");
static const std::string RESTRICT("RESTRICT");
static const std::string CASCADE("CASCADE");
static const std::string SET_NULL("SET NULL");
static const std::string NO_ACTION("NO ACTION");
static const std::string SET_DEFAULT("SET DEFAULT");

static const std::string YES("YES");
static const std::string NO("NO");

static const std::string UNKNOWN_INDEX("UNKNOWN_INDEX");
static const std::string BTREE("BTREE");
static const std::string RTREE("RTREE");
static const std::string HASH("HASH");
static const std::string FULLTEXT("FULLTEXT");

static const std::string MATCH_FULL("FULL");
static const std::string MATCH_PARTIAL("PARTIAL");
static const std::string MATCH_SIMPLE("SIMPLE");

const static std::string STANDARD_STRING("STANDARD");
const static std::string TEMPORARY_STRING("TEMPORARY");
const static std::string INTERNAL_STRING("INTERNAL");
const static std::string FUNCTION_STRING("FUNCTION");

void update(drizzled::message::Schema &arg)
{
  arg.set_version(arg.version() + 1);
  arg.set_update_timestamp(time(NULL));
}

void update(drizzled::message::Table &arg)
{
  arg.set_version(arg.version() + 1);
  arg.set_update_timestamp(time(NULL));
}

bool is_numeric(const message::Table::Field &field)
{
  message::Table::Field::FieldType type= field.type();

  switch (type)
  {
  case message::Table::Field::DOUBLE:
  case message::Table::Field::INTEGER:
  case message::Table::Field::BIGINT:
  case message::Table::Field::DECIMAL:
    return true;
  case message::Table::Field::BLOB:
  case message::Table::Field::VARCHAR:
  case message::Table::Field::ENUM:
  case message::Table::Field::DATE:
  case message::Table::Field::EPOCH:
  case message::Table::Field::DATETIME:
  case message::Table::Field::TIME:
  case message::Table::Field::UUID:
  case message::Table::Field::BOOLEAN:
    break;
  }

  return false;
}

const std::string &type(const message::Table::Field &field)
{
  message::Table::Field::FieldType type= field.type();

  switch (type)
  {
  case message::Table::Field::VARCHAR:
    return field.string_options().collation().compare("binary") ? VARCHAR : VARBINARY;
  case message::Table::Field::DOUBLE:
    return DOUBLE;
  case message::Table::Field::BLOB:
    return field.string_options().collation().compare("binary") ? TEXT : BLOB;
  case message::Table::Field::ENUM:
    return ENUM;
  case message::Table::Field::INTEGER:
    return INTEGER;
  case message::Table::Field::BIGINT:
    return BIGINT;
  case message::Table::Field::DECIMAL:
    return DECIMAL;
  case message::Table::Field::DATE:
    return DATE;
  case message::Table::Field::EPOCH:
    return TIMESTAMP;
  case message::Table::Field::DATETIME:
    return DATETIME;
  case message::Table::Field::TIME:
    return TIME;
  case message::Table::Field::UUID:
    return UUID;
  case message::Table::Field::BOOLEAN:
    return BOOLEAN;
  }

  abort();
}

const std::string &type(drizzled::message::Table::Field::FieldType type)
{
  switch (type)
  {
  case message::Table::Field::VARCHAR:
    return VARCHAR;
  case message::Table::Field::DOUBLE:
    return DOUBLE;
  case message::Table::Field::BLOB:
    return BLOB;
  case message::Table::Field::ENUM:
    return ENUM;
  case message::Table::Field::INTEGER:
    return INTEGER;
  case message::Table::Field::BIGINT:
    return BIGINT;
  case message::Table::Field::DECIMAL:
    return DECIMAL;
  case message::Table::Field::DATE:
    return DATE;
  case message::Table::Field::EPOCH:
    return EPOCH;
  case message::Table::Field::DATETIME:
    return DATETIME;
  case message::Table::Field::TIME:
    return TIME;
  case message::Table::Field::UUID:
    return UUID;
  case message::Table::Field::BOOLEAN:
    return BOOLEAN;
  }

  abort();
}

const std::string &type(drizzled::message::Table::ForeignKeyConstraint::ForeignKeyOption type)
{
  switch (type)
  {
  case message::Table::ForeignKeyConstraint::OPTION_RESTRICT:
    return RESTRICT;
  case message::Table::ForeignKeyConstraint::OPTION_CASCADE:
    return CASCADE;
  case message::Table::ForeignKeyConstraint::OPTION_SET_NULL:
    return SET_NULL;
  case message::Table::ForeignKeyConstraint::OPTION_UNDEF:
  case message::Table::ForeignKeyConstraint::OPTION_NO_ACTION:
    return NO_ACTION;
  case message::Table::ForeignKeyConstraint::OPTION_SET_DEFAULT:
    return SET_DEFAULT;
  }

  return NO_ACTION;
}

// This matches SQL standard of using YES/NO not the normal TRUE/FALSE
const std::string &type(bool type)
{
  return type ? YES : NO;
}

const std::string &type(drizzled::message::Table::Index::IndexType type)
{
  switch (type)
  {
  case message::Table::Index::UNKNOWN_INDEX:
    return UNKNOWN_INDEX;
  case message::Table::Index::BTREE:
    return BTREE;
  case message::Table::Index::RTREE:
    return RTREE;
  case message::Table::Index::HASH:
    return HASH;
  case message::Table::Index::FULLTEXT:
    return FULLTEXT;
  }

  assert(0);
  return PROGRAM_ERROR;
}

const std::string &type(drizzled::message::Table::ForeignKeyConstraint::ForeignKeyMatchOption type)
{
  switch (type)
  {
  case message::Table::ForeignKeyConstraint::MATCH_FULL:
    return MATCH_FULL;
  case message::Table::ForeignKeyConstraint::MATCH_PARTIAL:
    return MATCH_PARTIAL;
  case message::Table::ForeignKeyConstraint::MATCH_UNDEFINED:
  case message::Table::ForeignKeyConstraint::MATCH_SIMPLE:
    return MATCH_SIMPLE;
  }

  return MATCH_SIMPLE;
}

const std::string &type(drizzled::message::Table::TableType type)
{
  switch (type)
  {
  case message::Table::STANDARD:
    return STANDARD_STRING;
  case message::Table::TEMPORARY:
    return TEMPORARY_STRING;
  case message::Table::INTERNAL:
    return INTERNAL_STRING;
  case message::Table::FUNCTION:
    return FUNCTION_STRING;
  }

  assert(0);
  return PROGRAM_ERROR;
}

#if 0
std::ostream& operator<<(std::ostream& output, const message::Transaction &message)
{ 
    std::string buffer;

    google::protobuf::TextFormat::PrintToString(message, &buffer);
    output << buffer;

    return output;
}

std::ostream& operator<<(std::ostream& output, const message::Table &message)
{ 
  std::string buffer;

  google::protobuf::TextFormat::PrintToString(message, &buffer);
  output << buffer;

  return output;
}
#endif


} /* namespace message */
} /* namespace drizzled */
