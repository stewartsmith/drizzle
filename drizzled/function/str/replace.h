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

#include <drizzled/function/str/strfunc.h>

namespace drizzled
{

class Item_func_replace :public Item_str_func
{
  String tmp_value, tmp_value2;
  Session &session;
public:
  Item_func_replace(Session &session_arg,
                    Item *org,
                    Item *find,
                    Item *replace) :
    Item_str_func(org, find, replace),
    session(session_arg)
  {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "replace"; }
};

} /* namespace drizzled */

