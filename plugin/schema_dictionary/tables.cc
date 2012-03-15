/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems, Inc.
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
#include <plugin/schema_dictionary/dictionary.h>
#include <drizzled/identifier.h>
#include <drizzled/table_proto.h>

using namespace std;
using namespace drizzled;

static const string STANDARD("STANDARD");
static const string TEMPORARY("TEMPORARY");
static const string INTERNAL("INTERNAL");
static const string FUNCTION("FUNCTION");


static const string VARCHAR("VARCHAR");
static const string DOUBLE("DOUBLE");
static const string BLOB("BLOB");
static const string ENUM("ENUM");
static const string INTEGER("INTEGER");
static const string BIGINT("BIGINT");
static const string DECIMAL("DECIMAL");
static const string DATE("DATE");
static const string TIMESTAMP("TIMESTAMP");
static const string DATETIME("DATETIME");

TablesTool::TablesTool() :
  plugin::TableFunction("DATA_DICTIONARY", "TABLES")
{
  add_field("TABLE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_TYPE");
  add_field("TABLE_ARCHETYPE");
  add_field("ENGINE");
  add_field("ROW_FORMAT", 10);
  add_field("TABLE_COLLATION");
  add_field("TABLE_CREATION_TIME");
  add_field("TABLE_UPDATE_TIME");
  add_field("TABLE_COMMENT", plugin::TableFunction::STRING,
            TABLE_COMMENT_MAXLEN, true);
  add_field("AUTO_INCREMENT", plugin::TableFunction::NUMBER, 0, false);
  add_field("TABLE_UUID", plugin::TableFunction::STRING, 36, true);
  add_field("TABLE_VERSION", plugin::TableFunction::NUMBER, 0, true);
  add_field("IS_REPLICATED", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("TABLE_DEFINER", plugin::TableFunction::STRING, 64, true);
}

TablesTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  all_tables_generator(getSession())
{
}

bool TablesTool::Generator::nextTable()
{
  drizzled::message::table::shared_ptr table_ptr;
  while ((table_ptr= all_tables_generator))
  {
    table_message.CopyFrom(*table_ptr);
    return true;
  }

  return false;
}

bool TablesTool::Generator::populate()
{
  if (nextTable())
  {
    fill();
    return true;
  }

  return false;
}

void TablesTool::Generator::fill()
{

  /**
    @note use --replace-column
  */

  /* TABLE_SCHEMA */
  push(getTableMessage().schema());

  /* TABLE_NAME */
  push(getTableMessage().name());

  /* TABLE_TYPE */
  if (drizzled::identifier::Table::isView(getTableMessage().type()))
  {
    push("VIEW");
  }
  else
  {
    push("BASE");
  }

  /* TABLE_ARCHETYPE */
  {
    switch (getTableMessage().type())
    {
    default:
    case message::Table::STANDARD:
      push(STANDARD);
      break;
    case message::Table::TEMPORARY:
      push(TEMPORARY);
      break;
    case message::Table::INTERNAL:
      push(INTERNAL);
      break;
    case message::Table::FUNCTION:
      push(FUNCTION);
      break;
    }
  }

  /* ENGINE */
  const drizzled::message::Engine &engine= getTableMessage().engine();
  push(engine.name());

  /* ROW_FORMAT */
  bool row_format_sent= false;
  for (ssize_t it= 0; it < engine.options_size(); it++)
  {
    const drizzled::message::Engine::Option &opt= engine.options(it);
    if (opt.name().compare("ROW_FORMAT") == 0)
    {
      row_format_sent= true;
      push(opt.state());
      break;
    }
  }

  if (not row_format_sent)
    push("DEFAULT");

  /* TABLE_COLLATION */
  push(getTableMessage().options().collation());

  /* TABLE_CREATION_TIME */
  time_t time_arg= getTableMessage().creation_timestamp();
  char buffer[40];
  struct tm tm_buffer;

  localtime_r(&time_arg, &tm_buffer);
  strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm_buffer);
  push(buffer);

  /* TABLE_UPDATE_TIME */
  time_arg= getTableMessage().update_timestamp();
  localtime_r(&time_arg, &tm_buffer);
  strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm_buffer);
  push(buffer);

  /* TABLE_COMMENT */
  if (getTableMessage().options().has_comment())
  {
    push(getTableMessage().options().comment());
  }
  else
  {
    push();
  }

  /* AUTO_INCREMENT */
  push(getTableMessage().options().auto_increment_value());

  /* TABLE_UUID */
  push(getTableMessage().uuid());

  /* TABLE_VERSION */
  push(getTableMessage().version());

  /* IS_REPLICATED */
  push(message::is_replicated(getTableMessage()));

  /* _DEFINER */
  if (message::has_definer(getTableMessage()))
  {
    push(message::definer(getTableMessage()));
  }
  else
  {
    push();
  }
}
