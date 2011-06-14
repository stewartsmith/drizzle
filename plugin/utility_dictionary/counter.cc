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

#include <plugin/utility_dictionary/dictionary.h>

#include <drizzled/atomics.h>
#include <drizzled/session.h>
#include <drizzled/sql_lex.h>

using namespace drizzled;
using namespace std;

drizzled::atomic<uint64_t> counter;

utility_dictionary::Counter::Counter() :
  plugin::TableFunction("DATA_DICTIONARY", "COUNTER")
{
  add_field("VALUE", plugin::TableFunction::NUMBER, 0, false);
}

bool utility_dictionary::Counter::Generator::populate()
{
  if (lex().isSumExprUsed() && count > 0)
    return false;

  if (lex().current_select->group_list.elements && count > 0)
    return false;

  if (lex().current_select->explicit_limit or count == 0)
  {
    push(counter.increment());
  }
  else
  {
    return false;
  }

  count++;

  return true;
}
