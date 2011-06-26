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
#include <drizzled/lex_string.h>

namespace drizzled {

class Key_part_spec :public memory::SqlAlloc {
public:
  LEX_STRING field_name;
  uint32_t length;
  Key_part_spec(const LEX_STRING &name, uint32_t len)
    : field_name(name), length(len)
  {}
  Key_part_spec(const char *name, const size_t name_len, uint32_t len)
    : length(len)
  { field_name.str= const_cast<char *>(name); field_name.length= name_len; }
  bool operator==(const Key_part_spec& other) const;
  /**
    Construct a copy of this Key_part_spec. field_name is copied
    by-pointer as it is known to never change. At the same time
    'length' may be reset in mysql_prepare_create_table, and this
    is why we supply it with a copy.

    @return If out of memory, 0 is returned and an error is set in
    Session.
  */
  Key_part_spec *clone(memory::Root *mem_root) const
  {
    return new (*mem_root) Key_part_spec(*this);
  }
};

} /* namespace drizzled */

