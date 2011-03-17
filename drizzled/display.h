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

#include <drizzled/common.h>
#include <string>

#include <drizzled/item/func.h>

namespace drizzled {
namespace display {

const std::string &type(drizzled::Cast_target type);
const std::string &type(Item_result type);
const std::string &type(drizzled::enum_field_types type);
const std::string &type(drizzled::enum_server_command type);
const std::string &type(bool type);

std::string hexdump(const unsigned char *str, size_t length);

} /* namespace display */
} /* namespace drizzled */

