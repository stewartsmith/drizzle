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


CollationsTool::CollationsTool() :
  CharacterSetsTool("COLLATIONS")
{
  add_field("CHARACTER_SET_NAME");
  add_field("COLLATION_NAME");
  add_field("DESCRIPTION");
  add_field("ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("IS_DEFAULT", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("IS_COMPILED", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("SORTLEN", plugin::TableFunction::NUMBER, 0, false);
}

CollationsTool::Generator::Generator(Field **arg) :
  CharacterSetsTool::Generator(arg),
  is_collation_primed(false)
{
}


bool CollationsTool::Generator::end()
{
  return collation_iter == all_charsets+255;
}


bool CollationsTool::Generator::check()
{
  const charset_info_st *tmp_cs= character_set();
  const charset_info_st *tmp_cl= collation();

  if (not tmp_cl || 
      not (tmp_cl->state & MY_CS_AVAILABLE) ||
      not my_charset_same(tmp_cs, tmp_cl))
    return true;

  return false;
}

bool CollationsTool::Generator::nextCollationCore()
{
  if (isPrimed())
  {
    collation_iter++;
  }
  else
  {
    if (not isCharacterSetPrimed())
     return false;

    collation_iter= all_charsets;
    prime();
  }

  if (end())
    return false;

  if (check())
      return false;

  return true;
}


bool CollationsTool::Generator::next()
{
  while (not nextCollationCore())
  {

    if (isPrimed() && not end())
      continue;

    if (not nextCharacterSet())
      return false;

    prime(false);
  }

  return true;
}

bool CollationsTool::Generator::populate()
{
  if (not next())
    return false;

  fill();

  return true;
}

void CollationsTool::Generator::fill()
{
  const charset_info_st *tmp_cs= character_set();
  const charset_info_st *tmp_cl= collation_iter[0];

  assert(tmp_cs);
  assert(tmp_cl);

  assert(tmp_cs->name);
  /* CHARACTER_SET_NAME */
  push(tmp_cs->name);

  /* "COLLATION_NAME" */
  assert(tmp_cl->name);
  push(tmp_cl->name);

  /* "DESCRIPTION" */
  push(tmp_cl->csname);

  /* COLLATION_ID */
  push(static_cast<int64_t>(tmp_cl->number));

  /* IS_DEFAULT */
  push((bool)(tmp_cl->state & MY_CS_PRIMARY));

  /* IS_COMPILED */
  push((bool)(tmp_cl->state & MY_CS_COMPILED));

  /* SORTLEN */
  push(static_cast<int64_t>(tmp_cl->strxfrm_multiply));
}
