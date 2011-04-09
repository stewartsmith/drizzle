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

#include <string>

#include <drizzled/visibility.h>

namespace drizzled {

class DRIZZLED_API Identifier 
{
public:
  virtual ~Identifier()
  { 
	}

	virtual std::string getSQLPath() const 
	{ 
		return "";
	}
};

} // namespace drizzled

#include <drizzled/identifier/catalog.h>
#include <drizzled/identifier/schema.h>
#include <drizzled/identifier/table.h>
#include <drizzled/identifier/user.h>

// Constant identifiers used internally
#include <drizzled/identifier/constants/schema.h>
#include <drizzled/identifier/constants/table.h>
#include <drizzled/identifier/constants/user.h>
