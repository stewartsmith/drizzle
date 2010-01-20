/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Tim Penhey <tim@penhey.net>
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

#include "config.h"
#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

#include <string>

using namespace std;
using namespace drizzled;

namespace {
  char const* rot13 = "rot13";
}

class Item_func_rot13 : public Item_str_func
{
public:
  Item_func_rot13() : Item_str_func() {}
  const char *func_name() const { return rot13; }

  String *val_str(String* s) {
    return args[0]->val_str(s);
  };

  void fix_length_and_dec() {
    max_length = args[0]->max_length;
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }
};

plugin::Create_function<Item_func_rot13> *rot13_func = NULL;

static int rot13_plugin_init(drizzled::plugin::Registry &registry)
{
  rot13_func =
    new plugin::Create_function<Item_func_rot13>(rot13);
  registry.add(rot13_func);
  return 0;
}

static int rot13_plugin_deinit(drizzled::plugin::Registry &registry)
{
  registry.remove(rot13_func);
  delete rot13_func;
  return 0;
}

DRIZZLE_PLUGIN(rot13_plugin_init, rot13_plugin_deinit, NULL, NULL);
