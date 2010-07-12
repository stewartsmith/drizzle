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

#include "config.h"

#include "plugin/information_schema_dictionary/dictionary.h"

static const std::string VARCHAR("VARCHAR");
static const std::string DOUBLE("DOUBLE");
static const std::string BLOB("BLOB");
static const std::string ENUM("ENUM");
static const std::string INTEGER("INTEGER");
static const std::string BIGINT("BIGINT");
static const std::string DECIMAL("DECIMAL");
static const std::string DATE("DATE");
static const std::string TIMESTAMP("TIMESTAMP");
static const std::string DATETIME("DATETIME");

void InformationSchema::Generator::pushType(drizzled::message::Table::Field::FieldType type)
{
  switch (type)
  {
  default:
  case drizzled::message::Table::Field::VARCHAR:
    push(VARCHAR);
    break;
  case drizzled::message::Table::Field::DOUBLE:
    push(DOUBLE);
    break;
  case drizzled::message::Table::Field::BLOB:
    push(BLOB);
    break;
  case drizzled::message::Table::Field::ENUM:
    push(ENUM);
    break;
  case drizzled::message::Table::Field::INTEGER:
    push(INTEGER);
    break;
  case drizzled::message::Table::Field::BIGINT:
    push(BIGINT);
    break;
  case drizzled::message::Table::Field::DECIMAL:
    push(DECIMAL);
    break;
  case drizzled::message::Table::Field::DATE:
    push(DATE);
    break;
  case drizzled::message::Table::Field::TIMESTAMP:
    push(TIMESTAMP);
    break;
  case drizzled::message::Table::Field::DATETIME:
    push(DATETIME);
    break;
  }
}
