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
#include <drizzled/statement/select.h>

using namespace std;
using namespace drizzled;

IndexPartsTool::IndexPartsTool() :
  IndexesTool("INDEX_PARTS")
{
  add_field("TABLE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("INDEX_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("COLUMN_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("COLUMN_NUMBER", plugin::TableFunction::NUMBER, 0, false);
  add_field("SEQUENCE_IN_INDEX", plugin::TableFunction::NUMBER, 0, false);
  add_field("COMPARE_LENGTH", plugin::TableFunction::NUMBER, 0, true);
  add_field("IS_ORDER_REVERSE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_USED_IN_PRIMARY", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_UNIQUE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_NULLABLE", plugin::TableFunction::BOOLEAN, 0, false);
}

IndexPartsTool::Generator::Generator(Field **arg) :
  IndexesTool::Generator(arg),
  index_part_iterator(0),
  is_index_part_primed(false)
{
}


bool IndexPartsTool::Generator::nextIndexPartsCore()
{
  if (is_index_part_primed)
  {
    index_part_iterator++;
  }
  else
  {
    if (not isIndexesPrimed())
      return false;

    index_part_iterator= 0;
    is_index_part_primed= true;
  }

  if (index_part_iterator >= getIndex().index_part_size())
    return false;

  index_part= getIndex().index_part(index_part_iterator);

  return true;
}

bool IndexPartsTool::Generator::nextIndexParts()
{
  while (not nextIndexPartsCore())
  {
    if (not nextIndex())
      return false;
    is_index_part_primed= false;
  }

  return true;
}

bool IndexPartsTool::Generator::populate()
{
  if (not nextIndexParts())
    return false;

  fill();

  return true;
}

void IndexPartsTool::Generator::fill()
{
  const message::Table::Field &field= getTableProto().field(index_part.fieldnr());

  /* TABLE_SCHEMA */
  push(getTableProto().schema());

  /* TABLE_NAME */
  push(getTableProto().name());

  /* INDEX_NAME */
  push(getIndex().name());

  /* COLUMN_NAME */
  push(field.name());

  /* COLUMN_NUMBER */
  push(static_cast<int64_t>(index_part.fieldnr()));

  /* SEQUENCE_IN_INDEX  */
  push(static_cast<int64_t>(index_part_iterator));

  /* COMPARE_LENGTH */
  if ((field.type() == message::Table::Field::VARCHAR or
    field.type() == message::Table::Field::BLOB) and
    (index_part.has_compare_length()) and
    (index_part.compare_length() != field.string_options().length()))
  {
    push(static_cast<int64_t>(index_part.compare_length()));
  }
  else
    push();

  /* IS_ORDER_REVERSE */
  push(index_part.in_reverse_order());

  /* IS_USED_IN_PRIMARY */
  push(getIndex().is_primary());

  /* IS_UNIQUE */
  push(getIndex().is_unique());

  /* IS_NULLABLE */
  push(getIndex().options().null_part_key());
}
