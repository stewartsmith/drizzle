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


using namespace std;
using namespace drizzled;

ShowIndexes::ShowIndexes() :
  show_dictionary::Show("SHOW_INDEXES")
{
  add_field("Table");
  add_field("Unique", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("Key_name");
  add_field("Seq_in_index", plugin::TableFunction::NUMBER, 0, false);
  add_field("Column_name");
}

ShowIndexes::Generator::Generator(Field **arg) :
  show_dictionary::Show::Generator(arg),
  is_tables_primed(false),
  is_index_primed(false),
  is_index_part_primed(false),
  index_iterator(0),
  index_part_iterator(0)
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

bool ShowIndexes::Generator::nextIndexCore()
{
  if (isIndexesPrimed())
  {
    index_iterator++;
  }
  else
  {
    if (not isTablesPrimed())
      return false;

    index_iterator= 0;
    is_index_primed= true;
  }

  if (index_iterator >= getTableProto().indexes_size())
    return false;

  index= getTableProto().indexes(index_iterator);

  return true;
}

bool ShowIndexes::Generator::nextIndex()
{
  while (not nextIndexCore())
  {
    return false;
  }

  return true;
}

bool ShowIndexes::Generator::nextIndexPartsCore()
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


bool ShowIndexes::Generator::nextIndexParts()
{
  while (not nextIndexPartsCore())
  {
    if (not nextIndex())
      return false;
    is_index_part_primed= false;
  }

  return true;
}



bool ShowIndexes::Generator::populate()
{
  if (not nextIndexParts())
    return false;

  fill();

  return true;
}

void ShowIndexes::Generator::fill()
{
  /* Table */
  push(getTableName());

  /* Unique */
  push(getIndex().is_unique());

  /* Key_name */
  push(getIndex().name());

  /* Seq_in_index */
  push(static_cast<int64_t>(index_part_iterator + 1));

  /* Column_name */
  push(getTableProto().field(getIndexPart().fieldnr()).name());
}
