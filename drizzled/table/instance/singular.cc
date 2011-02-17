/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

#include <drizzled/table/instance.h>

namespace drizzled
{

namespace table
{

namespace instance
{

Singular::Singular(const identifier::Table::Type type_arg,
                   const identifier::Table &identifier,
                   char *path_arg, uint32_t path_length_arg) :
  TableShare(type_arg, identifier, path_arg, path_length_arg)
{
}

Singular::~Singular()
{
  assert(getTableCount() == 0);
}

} /* namespace instance */
} /* namespace table */
} /* namespace drizzled */
