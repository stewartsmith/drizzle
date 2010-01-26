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

CharacterSetsTool::CharacterSetsTool() :
  Tool("CHARACTER_SETS")
{
  add_field("CHARACTER_SET_NAME", message::Table::Field::VARCHAR, 64);
  add_field("DEFAULT_COLLATE_NAME", message::Table::Field::VARCHAR, 64);
  add_field("DESCRIPTION", message::Table::Field::VARCHAR, 64);
  add_field("MAXLEN", message::Table::Field::BIGINT);
}

CharacterSetsTool::Generator::Generator()
{
  cs= all_charsets;
}

bool CharacterSetsTool::Generator::populate(Field ** fields)
{
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;

  for (; cs < all_charsets+255 ; cs++)
  {
    const CHARSET_INFO * const tmp_cs= cs[0];

    if (tmp_cs && (tmp_cs->state & MY_CS_PRIMARY) &&
        (tmp_cs->state & MY_CS_AVAILABLE) &&
        ! (tmp_cs->state & MY_CS_HIDDEN))
    {
      const char *comment;

      (*field)->store(tmp_cs->csname, strlen(tmp_cs->csname), scs);
      field++;

      (*field)->store(tmp_cs->name, strlen(tmp_cs->name), scs);
      field++;

      comment= tmp_cs->comment ? tmp_cs->comment : "";
      (*field)->store(comment, strlen(comment), scs);
      field++;

      (*field)->store((int64_t) tmp_cs->mbmaxlen, true);

      cs++;

      return true;
    }
  }

  return false;
}
