/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifndef DRIZZLED_GENERATOR_CATALOG_H
#define DRIZZLED_GENERATOR_CATALOG_H

#include "drizzled/catalog/cache.h"

namespace drizzled {
namespace generator {

class Catalog
{
  drizzled::catalog::Instance::vector local_vector;
  drizzled::catalog::Instance::vector::iterator iter;

public:

  Catalog()
  {
    catalog::Cache::singleton().CopyFrom(local_vector);
    iter= local_vector.begin();
  }

  ~Catalog()
  {
  }

  operator drizzled::catalog::Instance::shared_ptr()
  {
    while (iter != local_vector.end())
    {
      drizzled::catalog::Instance::shared_ptr ret(*iter);
      iter++;
      return ret;
    }

    return drizzled::catalog::Instance::shared_ptr();
  }
};

} /* namespace generator */
} /* namespace drizzled */

#endif /* DRIZZLED_GENERATOR_CATALOG_H */
