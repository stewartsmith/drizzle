/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Patrick Galbraith
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

#include <unistd.h>
#include <drizzled/server_includes.h>
#include <drizzled/sql_udf.h>
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

#include <string>

using namespace std;

class Item_func_sleep : public Item_int_func
{
  String value;

public:
  int64_t val_int();
  Item_func_sleep() : Item_int_func()
  {
    unsigned_flag= true;
  }

  const char *func_name() const
  {
    return "sleep";
  }

  void fix_length_and_dec() {
    max_length=1;
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }

};

int64_t Item_func_sleep::val_int()
{
  /* int time in seconds */
  uint dtime;
  /* string time */
  String *stime;

  if ((arg_count != 1) || ! (stime= args[0]->val_str(&value)))
  {
    null_value= true;
    return 0;
  }

  /* obtain int time */
  dtime= atoi(stime->c_ptr());

  /* sleep dtime seconds */
  sleep(dtime);

  null_value= false;
  return 0;
}

Create_function<Item_func_sleep>
  sleep_udf(string("sleep"));

static int sleep_plugin_init(PluginRegistry &registry)
{
  registry.add(&sleep_udf);
  return 0;
}

static int sleep_plugin_deinit(PluginRegistry &registry)
{
  registry.remove(&sleep_udf);
  return 0;
}


drizzle_declare_plugin(sleep)
{
  "sleep",
  "1.0",
  "Patrick Galbraith",
  "sleep()",
  PLUGIN_LICENSE_GPL,
  sleep_plugin_init, /* Plugin Init */
  sleep_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
