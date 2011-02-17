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

#include <plugin/catalog/module.h>

#include <drizzled/atomics.h>
#include <drizzled/session.h>

using namespace std;

namespace plugin {
namespace catalog {
namespace tables {

Cache::Cache() :
  drizzled::plugin::TableFunction("DATA_DICTIONARY", "CATALOG_CACHE")
{
  add_field("CATALOG_NAME", drizzled::plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
}

Cache::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg)
{
}

bool Cache::Generator::populate()
{

  drizzled::catalog::Instance::shared_ptr tmp;

  while ((tmp= catalog_generator))
  {
    // CATALOG_NAME
    push(tmp->getName());

    return true;
  }

  return false;
}

} /* namespace tables */
} /* namespace catalog */
} /* namespace plugin */
