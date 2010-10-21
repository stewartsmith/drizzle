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

#ifndef DRIZZLED_MESSAGE_H
#define DRIZZLED_MESSAGE_H

#include "drizzled/message/table.pb.h"
#include "drizzled/message/schema.pb.h"

#include <boost/shared_ptr.hpp>

namespace drizzled {
namespace message {

void init(drizzled::message::Schema &arg, const std::string &name_arg);
void init(drizzled::message::Table &arg, const std::string &name_arg, const std::string &schema_arg, const std::string &engine_arg);

void update(drizzled::message::Schema &arg);
void update(drizzled::message::Table &arg);

const std::string &type(drizzled::message::Table::Field::FieldType type);
const std::string &type(drizzled::message::Table::ForeignKeyConstraint::ForeignKeyOption type);
const std::string &type(bool type);
const std::string &type(drizzled::message::Table::Index::IndexType type);

typedef boost::shared_ptr<drizzled::message::Schema> SchemaPtr;

} /* namespace message */
} /* namespace drizzled */

#endif /* DRIZZLED_MESSAGE_H */
