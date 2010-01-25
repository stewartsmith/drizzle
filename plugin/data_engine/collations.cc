/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems
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

#include <plugin/data_engine/dictionary.h>
#include <drizzled/charset.h>

using namespace std;
using namespace drizzled;

CollationsTool::CollationsTool()
{
  message::Table::StorageEngine *engine;
  message::Table::TableOptions *table_options;

  schema.set_name("character_sets");
  schema.set_type(message::Table::STANDARD);

  table_options= schema.mutable_options();
  table_options->set_collation_id(default_charset_info->number);
  table_options->set_collation(default_charset_info->name);

  engine= schema.mutable_engine();
  engine->set_name(engine_name);

  add_field(schema, "COLLATION_NAME", message::Table::Field::VARCHAR, 64);
  add_field(schema, "CHARACTER_SET_NAME", message::Table::Field::VARCHAR, 64);
  add_field(schema, "DESCRIPTION", message::Table::Field::VARCHAR, 64);
  add_field(schema, "ID", message::Table::Field::BIGINT);
  add_field(schema, "IS_DEFAULT", message::Table::Field::BIGINT);
  add_field(schema, "IS_COMPILED", message::Table::Field::BIGINT);
  add_field(schema, "SORTLEN", message::Table::Field::BIGINT);
}

CollationsTool::Generator::Generator()
{
  cs= all_charsets;
  cl= all_charsets;
}

bool CollationsTool::Generator::populate(Field ** fields)
{
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;

  for (; cs < all_charsets+255 ; cs++)
  {
    const CHARSET_INFO *tmp_cs= cs[0];

    if (! tmp_cs || ! (tmp_cs->state & MY_CS_AVAILABLE) ||
        (tmp_cs->state & MY_CS_HIDDEN) ||
        !(tmp_cs->state & MY_CS_PRIMARY))
      continue;

    for (; cl < all_charsets+255 ;cl ++)
    {
      const CHARSET_INFO *tmp_cl= cl[0];

      if (! tmp_cl || ! (tmp_cl->state & MY_CS_AVAILABLE) ||
          !my_charset_same(tmp_cs, tmp_cl))
        continue;

      {
        const char *tmp_buff;

        (*field)->store(tmp_cl->name, strlen(tmp_cl->name), scs);
        field++;

        (*field)->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
        field++;

        (*field)->store((int64_t) tmp_cl->number, true);
        field++;

        tmp_buff= (tmp_cl->state & MY_CS_PRIMARY) ? "Yes" : "";
        (*field)->store(tmp_buff, strlen(tmp_buff), scs);
        field++;

        tmp_buff= (tmp_cl->state & MY_CS_COMPILED)? "Yes" : "";
        (*field)->store(tmp_buff, strlen(tmp_buff), scs);
        field++;

        (*field)->store((int64_t) tmp_cl->strxfrm_multiply, true);

        cs++;

        return true;
      }
    }

    cl= all_charsets;
  }

  return false;
}
