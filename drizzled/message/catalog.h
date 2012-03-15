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

#pragma once

#include <boost/shared_ptr.hpp>
#include <drizzled/common_fwd.h>
#include <drizzled/message/catalog.pb.h>

namespace drizzled {

namespace message {
namespace catalog {

typedef boost::shared_ptr<message::Catalog> shared_ptr;
typedef std::vector< shared_ptr > vector;
typedef message::Catalog value_type;

shared_ptr make_shared(const drizzled::identifier::Catalog &identifier);

} /* namespace catalog */
} /* namespace message */
} /* namespace drizzled */

