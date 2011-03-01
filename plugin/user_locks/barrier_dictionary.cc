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

#include <plugin/user_locks/module.h>

#include <drizzled/atomics.h>
#include <drizzled/session.h>

using namespace drizzled;
using namespace std;

namespace user_locks {
namespace barriers {

UserBarriers::UserBarriers() :
  plugin::TableFunction("DATA_DICTIONARY", "USER_DEFINED_BARRIERS")
{
  add_field("USER_BARRIER_NAME", plugin::TableFunction::STRING, LARGEST_LOCK_NAME, false);
  add_field("SESSION_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("USER_NAME", plugin::TableFunction::STRING);
  add_field("WAITER_LIMIT", plugin::TableFunction::NUMBER, 0, false);
  add_field("GENERATION", plugin::TableFunction::NUMBER, 0, false);
  add_field("WAITERS", plugin::TableFunction::NUMBER, 0, false);
  add_field("OBSERVERS", plugin::TableFunction::NUMBER, 0, false);
}

UserBarriers::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg)
{
  Barriers::getInstance().Copy(barrier_map);
  iter= barrier_map.begin();
}

bool UserBarriers::Generator::populate()
{

  while (iter != barrier_map.end())
  {
    // USER_BARRIER_NAME
    push(iter->first.getLockName());

    // SESSION_ID
    push(iter->second->getOwner());
     
    // USER_NAME
    push(iter->first.getUser());
    
    // WAITER_LIMIT
    push(iter->second->getLimit());
    
    // GENERATION
    push(iter->second->getGeneration());
    
    // WAITERS
    push(iter->second->sizeWaiters());
    
    // OBSERVERS
    push(iter->second->sizeObservers());

    iter++;
    return true;
  }

  return false;
}

} /* namespace barriers */
} /* namespace user_locks */
