/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#include <drizzled/memory/sql_alloc.h>

namespace drizzled
{

namespace memory { class Root; }

class Item;

class AlterDrop :public memory::SqlAlloc {
public:
  enum drop_type
  {
    KEY,
    COLUMN,
    FOREIGN_KEY
  };
  const char *name;
  enum drop_type type;
  AlterDrop(enum drop_type par_type,
            const char *par_name) :
    name(par_name),
    type(par_type)
  {}
  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  AlterDrop *clone(memory::Root *mem_root) const
  {
    return new (mem_root) AlterDrop(*this);
  }
};

} /* namespace drizzled */

