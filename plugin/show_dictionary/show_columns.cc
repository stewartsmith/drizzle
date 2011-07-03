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
#include <plugin/show_dictionary/dictionary.h>
#include <drizzled/identifier.h>
#include <string>

using namespace std;
using namespace drizzled;

static const string VARCHAR("VARCHAR");
/* VARBINARY already defined elsewhere */
static const string VARBIN("VARBINARY");
static const string DOUBLE("DOUBLE");
static const string BLOB("BLOB");
static const string TEXT("TEXT");
static const string ENUM("ENUM");
static const string INTEGER("INTEGER");
static const string BIGINT("BIGINT");
static const string DECIMAL("DECIMAL");
static const string DATE("DATE");
static const string TIMESTAMP("TIMESTAMP");
static const string DATETIME("DATETIME");

ShowColumns::ShowColumns() :
  show_dictionary::Show("SHOW_COLUMNS")
{
  add_field("Field");
  add_field("Type");
  add_field("Null", plugin::TableFunction::BOOLEAN, 0 , false);
  add_field("Default");
  add_field("Default_is_NULL", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("On_Update");
}

ShowColumns::Generator::Generator(Field **arg) :
  show_dictionary::Show::Generator(arg),
  is_tables_primed(false),
  is_columns_primed(false),
  column_iterator(0)
{
  if (not isShowQuery())
   return;

  statement::Show& select= static_cast<statement::Show&>(statement());

  if (not select.getShowTable().empty() && not select.getShowSchema().empty())
  {
    table_name.append(select.getShowTable().c_str());
    identifier::Table identifier(select.getShowSchema().c_str(), select.getShowTable().c_str());

    if (not plugin::Authorization::isAuthorized(*getSession().user(),
                                            identifier, false))
    {
      drizzled::error::access(*getSession().user(), identifier);
      return;
    }

    table_proto= plugin::StorageEngine::getTableMessage(getSession(), identifier);

    if (table_proto)
      is_tables_primed= true;
  }
}

bool ShowColumns::Generator::nextColumnCore()
{
  if (is_columns_primed)
  {
    column_iterator++;
  }
  else
  {
    if (not isTablesPrimed())
      return false;

    column_iterator= 0;
    is_columns_primed= true;
  }

  if (column_iterator >= getTableProto()->field_size())
    return false;

  column= getTableProto()->field(column_iterator);

  return true;
}


bool ShowColumns::Generator::nextColumn()
{
  while (not nextColumnCore())
  {
    return false;
  }

  return true;
}

bool ShowColumns::Generator::populate()
{

  if (not nextColumn())
    return false;

  fill();

  return true;
}

void ShowColumns::Generator::pushType(message::Table::Field::FieldType type, const string &collation)
{
  switch (type)
  {
  default:
  case message::Table::Field::VARCHAR:
    push(collation.compare("binary") ? VARCHAR : VARBIN);
    break;
  case message::Table::Field::DOUBLE:
    push(DOUBLE);
    break;
  case message::Table::Field::BLOB:
    push(collation.compare("binary") ? TEXT : BLOB);
    break;
  case message::Table::Field::ENUM:
    push(ENUM);
    break;
  case message::Table::Field::INTEGER:
    push(INTEGER);
    break;
  case message::Table::Field::BIGINT:
    push(BIGINT);
    break;
  case message::Table::Field::DECIMAL:
    push(DECIMAL);
    break;
  case message::Table::Field::DATE:
    push(DATE);
    break;
  case message::Table::Field::EPOCH:
    push(TIMESTAMP);
    break;
  case message::Table::Field::DATETIME:
    push(DATETIME);
    break;
  }
}


void ShowColumns::Generator::fill()
{
  /* Field */
  push(column.name());

  /* Type */
  pushType(column.type(), column.string_options().collation());

  /* Null */
  push(not column.constraints().is_notnull());

  /* Default */
  if (column.options().has_default_value())
    push(column.options().default_value());
  else if (column.options().has_default_expression())
    push(column.options().default_expression());
  else
    push(column.options().default_bin_value());

  /* Default_is_NULL */
  push(column.options().default_null());

  /* On_Update */
  push(column.options().update_expression());
}
