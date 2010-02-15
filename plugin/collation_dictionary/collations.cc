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

#include "config.h"
#include "plugin/collation_dictionary/dictionary.h"

using namespace std;
using namespace drizzled;


CollationsTool::CollationsTool() :
  plugin::TableFunction("DATA_DICTIONARY", "COLLATIONS")
{
  add_field("CHARACTER_SET_NAME");
  add_field("COLLATION_NAME");
  add_field("DESCRIPTION");
  add_field("ID", plugin::TableFunction::NUMBER);
  add_field("IS_DEFAULT", plugin::TableFunction::BOOLEAN);
  add_field("IS_COMPILED", plugin::TableFunction::BOOLEAN);
  add_field("SORTLEN", plugin::TableFunction::NUMBER);
}

CollationsTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  cs= all_charsets;
  cl= all_charsets;
}

bool CollationsTool::Generator::populate()
{
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
        /* CHARACTER_SET_NAME */
        push(tmp_cs->name);

        /* "COLLATION_NAME" */
        push(tmp_cl->name);

        /* "DESCRIPTION" */
        push(tmp_cl->csname);

        /* COLLATION_ID */
        push((int64_t) tmp_cl->number);
         
        /* IS_DEFAULT */
        push((bool)(tmp_cl->state & MY_CS_PRIMARY));

        /* IS_COMPILED */
        push((bool)(tmp_cl->state & MY_CS_COMPILED));

        /* SORTLEN */
        push((int64_t) tmp_cl->strxfrm_multiply);

        cl++;

        return true;
      }
      cs++;
    }

    cl= all_charsets;
  }

  return false;
}
