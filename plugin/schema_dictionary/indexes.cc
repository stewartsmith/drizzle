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

using namespace std;
using namespace drizzled;

IndexesTool::IndexesTool() :
  TablesTool("INDEXES")
{
  add_field("TABLE_SCHEMA", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("INDEX_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("IS_USED_IN_PRIMARY", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_UNIQUE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_NULLABLE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("KEY_LENGTH", plugin::TableFunction::NUMBER, 0, false);
  add_field("INDEX_TYPE");
  add_field("INDEX_COMMENT", plugin::TableFunction::STRING, 1024, true);
}

IndexesTool::Generator::Generator(Field **arg) :
  TablesTool::Generator(arg),
  index_iterator(0),
  is_index_primed(false)
{
}

bool IndexesTool::Generator::nextIndexCore()
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

bool IndexesTool::Generator::nextIndex()
{
  while (not nextIndexCore())
  {
    if (not nextTable())
      return false;
    is_index_primed= false;
  }

  return true;
}

bool IndexesTool::Generator::populate()
{
  if (not nextIndex())
    return false;

  fill();

  return true;
}

void IndexesTool::Generator::fill()
{
  /* TABLE_SCHEMA */
  push(getTableProto().schema());

  /* TABLE_NAME */
  push(getTableProto().name());

  /* INDEX_NAME */
  push(index.name());

  /* IS_USED_IN_PRIMARY */
  push(index.is_primary());

  /* IS_UNIQUE */
  push(index.is_unique());

  /* IS_NULLABLE */
  push(index.options().null_part_key());

  /* KEY_LENGTH */
  push(static_cast<uint64_t>(index.key_length()));

  /* INDEX_TYPE */
  {
    const char *str;
    uint32_t length;

    switch (index.type())
    {
    default:
    case message::Table::Index::UNKNOWN_INDEX:
      str= "UNKNOWN";
      length= sizeof("UNKNOWN");
      break;
    case message::Table::Index::BTREE:
      str= "BTREE";
      length= sizeof("BTREE");
      break;
    case message::Table::Index::RTREE:
      str= "RTREE";
      length= sizeof("RTREE");
      break;
    case message::Table::Index::HASH:
      str= "HASH";
      length= sizeof("HASH");
      break;
    case message::Table::Index::FULLTEXT:
      str= "FULLTEXT";
      length= sizeof("FULLTEXT");
      break;
    }
    /* Subtract 1 here, because sizeof gives us the wrong amount */
    push(str, length - 1);
  }

 /* "INDEX_COMMENT" */
  if (index.has_comment())
  {
    push(index.comment());
  }
  else
  {
    push();
  }
}
