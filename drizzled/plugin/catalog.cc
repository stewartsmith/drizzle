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

#include "config.h"

#include "drizzled/plugin/catalog.h"


namespace drizzled
{
namespace plugin
{

class Instances {
  Catalog::vector _catalogs;

public:
  static Instances& singleton()
  {
    static Instances ptr;
    return ptr;
  }

  Catalog::vector &catalogs()
  {
    return _catalogs;
  }
};

Catalog::~Catalog()
{
}

bool Catalog::create(const identifier::Catalog &)
{
  return false;
}

bool Catalog::drop(const identifier::Catalog &)
{
  return false;
}

bool plugin::Catalog::addPlugin(plugin::Catalog *arg)
{
  Instances::singleton().catalogs().push_back(arg);

  return false;
}


void plugin::Catalog::removePlugin(plugin::Catalog *arg)
{
  Instances::singleton().catalogs().erase(find(Instances::singleton().catalogs().begin(),
                                               Instances::singleton().catalogs().end(),
                                               arg));
}

} /* namespace plugin */
} /* namespace drizzled */
