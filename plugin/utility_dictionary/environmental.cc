/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
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

extern char **environ;

using namespace drizzled;
using namespace std;

#define LARGEST_ENV_STRING 512

utility_dictionary::Environmental::Environmental() :
  plugin::TableFunction("DATA_DICTIONARY", "ENVIRONMENTAL")
{
  add_field("VARIABLE_NAME", plugin::TableFunction::STRING, LARGEST_ENV_STRING, true);
  add_field("VARIABLE_VALUE", plugin::TableFunction::STRING, LARGEST_ENV_STRING, false);
}

utility_dictionary::Environmental::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg),
  position(0)
{
  position= environ;
}

bool utility_dictionary::Environmental::Generator::populate()
{
  if (not *position)
    return false;

  char *value= NULL;
  if ((value= strchr(*position, '=')))
  {
    value++;
  }

  if (value)
  {
    std::string substring(*position, 0, (size_t)(value - *position -1)); // The additional -1 is for the = 
    push(substring);

    substring.assign(value, 0, LARGEST_ENV_STRING);
    push(substring);
  }
  else
  {
    std::string substring(*position, 0, LARGEST_ENV_STRING);
    push(false);
  }
  
  position++;

  return true;
}
