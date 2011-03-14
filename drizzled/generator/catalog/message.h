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

#pragma once

#include <drizzled/message/catalog.h>
#include <drizzled/plugin/catalog.h>

namespace drizzled {
namespace generator {
namespace catalog {

class Message
{
  message::catalog::vector local_vector;
  message::catalog::vector::iterator iter;

public:

  Message()
  {
    plugin::Catalog::getMessages(local_vector);
    iter= local_vector.begin();
  }

  operator message::catalog::shared_ptr()
  {
    while (iter != local_vector.end())
    {
      message::catalog::shared_ptr ret(*iter);
      iter++;

      return ret;
    }

    return message::catalog::shared_ptr();
  }
};

} /* namespace catalog */
} /* namespace generator */
} /* namespace drizzled */

