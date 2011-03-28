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
#include <drizzled/session/transactions.h>
#include <plugin/session_dictionary/dictionary.h>

namespace session_dictionary {

#define LARGEST_USER_SAVEPOINT_NAME 128

Savepoints::Savepoints() :
  drizzled::plugin::TableFunction("DATA_DICTIONARY", "USER_DEFINED_SAVEPOINTS")
{
  add_field("SAVEPOINT_NAME", drizzled::plugin::TableFunction::STRING, LARGEST_USER_SAVEPOINT_NAME, false);
}

Savepoints::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg),
  savepoints(getSession().transaction.savepoints),
  iter(savepoints.begin())
{
}

Savepoints::Generator::~Generator()
{
}

bool Savepoints::Generator::populate()
{
  while (iter != savepoints.end())
  {
    // SAVEPOINT_NAME
    push(iter->getName());

    iter++;

    return true;
  }

  return false;
}

} /* namespace session_dictionary */
