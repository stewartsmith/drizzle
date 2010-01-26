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

CollationCharacterSetApplicabilityTool::CollationCharacterSetApplicabilityTool() :
  Tool("COLLATION_CHARACTER_SET_APPLICABILITY")
{
  add_field("COLLATION_NAME", message::Table::Field::VARCHAR, 64);
  add_field("CHARACTER_SET_NAME", message::Table::Field::VARCHAR, 64);
}

CollationCharacterSetApplicabilityTool::Generator::Generator()
{
  cs= all_charsets;
  cl= all_charsets;
}

bool CollationCharacterSetApplicabilityTool::Generator::populate(Field ** fields)
{
  const CHARSET_INFO * const scs= system_charset_info;
  Field **field= fields;

  /*
    We find the next good character set.
  */
  for (; cs < all_charsets+255 ; cs++ )
  {
    const CHARSET_INFO *tmp_cs= cs[0];

    if (! tmp_cs || ! (tmp_cs->state & MY_CS_AVAILABLE) || ! (tmp_cs->state & MY_CS_PRIMARY))
      continue;

    for (; cl < all_charsets+255 ; cl++)
    {
      const CHARSET_INFO *tmp_cl= cl[0];

      if (! tmp_cl || ! (tmp_cl->state & MY_CS_AVAILABLE) || ! my_charset_same(tmp_cs, tmp_cl))
        continue;

      (*field)->store(tmp_cl->name, strlen(tmp_cl->name), scs);
      field++;

      (*field)->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);

      cl++;

      return true;
    }

    cl= all_charsets;
  }

  return false;
}
