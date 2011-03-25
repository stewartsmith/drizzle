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

#include <uuid/uuid.h>

#include <boost/shared_ptr.hpp>
#include <drizzled/message/table.pb.h>
#include <drizzled/identifier.h>

namespace drizzled {
namespace message {
namespace table {

typedef boost::shared_ptr <message::Table> shared_ptr;
typedef const message::Table& const_reference;

shared_ptr make_shared(const identifier::Table& identifier, const std::string &engine_arg);
shared_ptr make_shared(const std::string &name_arg, const std::string &schema_arg, const std::string &engine_arg);
void init(drizzled::message::Table &arg, const std::string &name_arg, const std::string &schema_arg, const std::string &engine_arg);
void update(drizzled::message::Table &arg);

} // namespace table
} // namespace message
} // namespace drizzled

