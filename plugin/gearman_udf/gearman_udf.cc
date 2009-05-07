/* Copyright (C) 2009 Eric Day

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

#include <drizzled/server_includes.h>
#include <drizzled/sql_udf.h>

#include "gman_servers_set.h"
#include "gman_do.h"

using namespace std;

Create_function<Item_func_gman_servers_set> gman_servers_set(string("gman_servers_set"));
Create_function<Item_func_gman_do> gman_do(string("gman_do"));
Create_function<Item_func_gman_do_high> gman_do_high(string("gman_do_high"));
Create_function<Item_func_gman_do_low> gman_do_low(string("gman_do_low"));
Create_function<Item_func_gman_do_background> gman_do_background(string("gman_do_background"));
Create_function<Item_func_gman_do_high_background> gman_do_high_background(string("gman_do_high_background"));
Create_function<Item_func_gman_do_low_background> gman_do_low_background(string("gman_do_low_background"));

static int gearman_udf_plugin_init(PluginRegistry &registry)
{
  registry.add(&gman_servers_set);
  registry.add(&gman_do);
  registry.add(&gman_do_high);
  registry.add(&gman_do_low);
  registry.add(&gman_do_background);
  registry.add(&gman_do_high_background);
  registry.add(&gman_do_low_background);
  return 0;
}

static int gearman_udf_plugin_deinit(PluginRegistry &registry)
{
  registry.remove(&gman_do_low_background);
  registry.remove(&gman_do_high_background);
  registry.remove(&gman_do_background);
  registry.remove(&gman_do_low);
  registry.remove(&gman_do_high);
  registry.remove(&gman_do);
  registry.remove(&gman_servers_set);
  return 0;
}

drizzle_declare_plugin(gearman)
{
  "gearman",
  "0.1",
  "Eric Day",
  "UDFs for Gearman Client",
  PLUGIN_LICENSE_BSD,
  gearman_udf_plugin_init, /* Plugin Init */
  gearman_udf_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
