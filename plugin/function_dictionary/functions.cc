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

#include <plugin/function_dictionary/dictionary.h>

#include <drizzled/atomics.h>
#include <drizzled/session.h>


using namespace drizzled;
using namespace std;

#define FUNCTION_NAME_LEN 64

function_dictionary::Functions::Functions() :
  plugin::TableFunction("DATA_DICTIONARY", "FUNCTIONS")
{
  add_field("FUNCTION_NAME", plugin::TableFunction::STRING, FUNCTION_NAME_LEN, false);
}

bool function_dictionary::Functions::Generator::populate()
{
  std::string *name_ptr;

  while ((name_ptr= functions))
  {
    push(*name_ptr);
    return true;
  }

  return false;
}
