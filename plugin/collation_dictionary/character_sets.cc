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
#include <plugin/collation_dictionary/dictionary.h>

using namespace std;
using namespace drizzled;

CharacterSetsTool::CharacterSetsTool() :
  plugin::TableFunction("DATA_DICTIONARY", "CHARACTER_SETS")
{
  add_field("CHARACTER_SET_NAME");
  add_field("DEFAULT_COLLATE_NAME");
  add_field("DESCRIPTION");
  add_field("MAXLEN", plugin::TableFunction::NUMBER, 0, false);
}

CharacterSetsTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  is_char_primed(false)
{
}

bool CharacterSetsTool::Generator::checkCharacterSet()
{
  if (character_set() && (character_set()->state & MY_CS_PRIMARY) &&
      (character_set()->state & MY_CS_AVAILABLE) && not (character_set()->state & MY_CS_HIDDEN))
  {
    return false;
  }

  return true;
}

bool CharacterSetsTool::Generator::nextCharacterSetCore()
{
  if (is_char_primed)
  {
    character_set_iter++;
  }
  else
  {
    character_set_iter= all_charsets;
    is_char_primed= true;
  }

  if (character_set_iter == all_charsets+255)
    return false;

  if (checkCharacterSet())
      return false;

  return true;
}

bool CharacterSetsTool::Generator::nextCharacterSet()
{
  while (not nextCharacterSetCore())
  {
    if (character_set_iter == all_charsets+255)
      return false;
  }

  return true;
}

bool CharacterSetsTool::Generator::populate()
{
  if (nextCharacterSet())
  {
    fill();
    return true;
  }

  return false;
}

void CharacterSetsTool::Generator::fill()
{
  const charset_info_st * const tmp_cs= character_set_iter[0];

  /* CHARACTER_SET_NAME */
  push(tmp_cs->csname);

  /* DEFAULT_COLLATE_NAME */
  push(tmp_cs->name);

  /* DESCRIPTION */
  push(tmp_cs->comment);

  /* MAXLEN */
  push(static_cast<int64_t>(tmp_cs->mbmaxlen));
}
