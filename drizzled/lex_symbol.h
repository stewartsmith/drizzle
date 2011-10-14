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


/* This struct includes all reserved words and functions */

#pragma once

namespace drizzled {

struct SYM_GROUP 
{
  const char* name;
  const char* needed_define;
};

struct SYMBOL
{
  const char* name;
  uint tok;
};

struct LEX_SYMBOL
{
  const char* begin() const
  {
    return data();
  }

  const char* end() const
  {
    return data() + size();
  }

  const char* data() const
  {
    return str;
  }

  size_t size() const
  {
    return length;
  }

  const SYMBOL* symbol;
  char* str;
  uint32_t length;
};

extern SYM_GROUP sym_group_common;
extern SYM_GROUP sym_group_geom;
extern SYM_GROUP sym_group_rtree;

} /* namespace drizzled */

