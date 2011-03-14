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

#include <drizzled/table.h>

namespace drizzled {
namespace table {

class Shell : public drizzled::Table
{
  TableShare &_share; /**< Pointer to the shared metadata about the table */

public:
  Shell(TableShare &arg) :
    _share(arg)
  {
  }

  const TableShare *getShare() const { return &_share; } /* Get rid of this long term */
  TableShare *getMutableShare() { return &_share; } /* Get rid of this long term */
  bool hasShare() const { return true; } /* Get rid of this long term */
  void setShare(TableShare *) { ; } /* Get rid of this long term */

  void release() { assert(false); }; // This should never happen

};

} /* namespace table */
} /* namespace drizzled */

