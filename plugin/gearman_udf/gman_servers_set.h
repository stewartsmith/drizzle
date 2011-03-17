/* Copyright (C) 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#pragma once

#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

class Item_func_gman_servers_set :public drizzled::Item_str_func
{
  drizzled::String buffer;
public:
  Item_func_gman_servers_set():Item_str_func() {}
  Item_func_gman_servers_set(Item *a):Item_str_func(a) {}
  void fix_length_and_dec() { max_length= args[0]->max_length; }
  const char *func_name() const{ return "gman_servers_set"; }
  drizzled::String *val_str(drizzled::String *);
};

