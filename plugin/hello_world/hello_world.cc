/* 
   -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
   *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

   Copyright (C) 2006 MySQL AB

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

#include <drizzled/common_includes.h>
#include <drizzled/item_func.h>
#include <drizzled/item_strfunc.h>

class Item_func_hello_world : public Item_str_func
{
public:
  const char *func_name() const { return "hello_world"; }
  String *val_str(String* s) {
    s->set(STRING_WITH_LEN("Hello World!"),system_charset_info);
    return s;
  };
  void fix_length_and_dec() {
    max_length=strlen("Hello World!");
  }
};

Item_func* create_hello_world_udf_item(MEM_ROOT* m)
{
  return  new (m) Item_func_hello_world();
}

static struct udf_func hello_world_udf = {
  { C_STRING_WITH_LEN("hello_world") },
  create_hello_world_udf_item
};

static int hello_world_plugin_init(void *p)
{
  udf_func **f = (udf_func**) p;

  *f= &hello_world_udf;

  return 0;
}

static int hello_world_plugin_deinit(void *p)
{
  udf_func *udff = (udf_func *) p;
  (void)udff;
  return 0;
}


mysql_declare_plugin(hello_world)
{
  DRIZZLE_UDF_PLUGIN,
  "hello_world",
  "1.0",
  "Mark Atwood",
  "Hello, world!",
  PLUGIN_LICENSE_GPL,
  hello_world_plugin_init, /* Plugin Init */
  hello_world_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
