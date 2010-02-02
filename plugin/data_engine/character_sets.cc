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

#include <plugin/data_engine/function.h>
#include <drizzled/charset.h>

using namespace std;
using namespace drizzled;

CharacterSetsTool::CharacterSetsTool() :
  Tool("DATA_DICTIONARY", "CHARACTER_SETS")
{
  add_field("CHARACTER_SET_NAME");
  add_field("DEFAULT_COLLATE_NAME");
  add_field("DESCRIPTION");
  add_field("MAXLEN", Tool::NUMBER);
}

CharacterSetsTool::Generator::Generator(Field **arg) :
  Tool::Generator(arg)
{
  cs= all_charsets;
}

bool CharacterSetsTool::Generator::populate()
{
  for (; cs < all_charsets+255 ; cs++)
  {
    const CHARSET_INFO * const tmp_cs= cs[0];

    if (tmp_cs && (tmp_cs->state & MY_CS_PRIMARY) &&
        (tmp_cs->state & MY_CS_AVAILABLE) &&
        ! (tmp_cs->state & MY_CS_HIDDEN))
    {
      /* CHARACTER_SET_NAME */
      push(tmp_cs->csname);

      /* DEFAULT_COLLATE_NAME */
      push(tmp_cs->name);

      /* DESCRIPTION */
      push(tmp_cs->comment);

      /* MAXLEN */
      push((int64_t) tmp_cs->mbmaxlen);

      cs++;

      return true;
    }
  }

  return false;
}
