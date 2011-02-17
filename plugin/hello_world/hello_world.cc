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

#include <config.h>

#include <drizzled/function/str/strfunc.h>
#include <drizzled/item/func.h>
#include <drizzled/plugin.h>
#include <drizzled/plugin/function.h>

#include <string>

using namespace std;
using namespace drizzled;

class Item_func_hello_world : public Item_str_func
{
public:
  Item_func_hello_world() : Item_str_func() {}
  const char *func_name() const { return "hello_world"; }
  String *val_str(String* s) {
    s->set(STRING_WITH_LEN("Hello World!"),system_charset_info);
    return s;
  }
  void fix_length_and_dec() {
    max_length=strlen("Hello World!");
  }
};

plugin::Create_function<Item_func_hello_world> *hello_world_udf= NULL;

static int hello_world_plugin_init(drizzled::module::Context &context)
{
  hello_world_udf=
    new plugin::Create_function<Item_func_hello_world>("hello_world");
  context.add(hello_world_udf);

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "hello_world",
  "1.0",
  "Mark Atwood",
  "Hello, world!",
  PLUGIN_LICENSE_GPL,
  hello_world_plugin_init, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
