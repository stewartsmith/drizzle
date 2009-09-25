/* Copyright (C) 2009 Sun Microsystems

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

#include "drizzled/server_includes.h"
#include "drizzled/slot/function.h"

#include "gman_servers_set.h"
#include "gman_do.h"

using namespace std;
using namespace drizzled;

plugin::Create_function<Item_func_gman_servers_set> *gman_servers_set= NULL;
plugin::Create_function<Item_func_gman_do> *gman_do= NULL;
plugin::Create_function<Item_func_gman_do_high> *gman_do_high= NULL;
plugin::Create_function<Item_func_gman_do_low> *gman_do_low= NULL;
plugin::Create_function<Item_func_gman_do_background> *gman_do_background= NULL;
plugin::Create_function<Item_func_gman_do_high_background>
  *gman_do_high_background= NULL;
plugin::Create_function<Item_func_gman_do_low_background>
  *gman_do_low_background= NULL;

static int gearman_udf_plugin_init(drizzled::plugin::Registry &registry)
{
  gman_servers_set= new plugin::Create_function<Item_func_gman_servers_set>("gman_servers_set");
  gman_do= new plugin::Create_function<Item_func_gman_do>("gman_do");
  gman_do_high= new plugin::Create_function<Item_func_gman_do_high>("gman_do_high");
  gman_do_low= new plugin::Create_function<Item_func_gman_do_low>("gman_do_low");
  gman_do_background= new plugin::Create_function<Item_func_gman_do_background>("gman_do_background");
  gman_do_high_background= new plugin::Create_function<Item_func_gman_do_high_background>("gman_do_high_background");
  gman_do_low_background= new plugin::Create_function<Item_func_gman_do_low_background>("gman_do_low_background");
  registry.add(gman_servers_set);
  registry.add(gman_do);
  registry.add(gman_do_high);
  registry.add(gman_do_low);
  registry.add(gman_do_background);
  registry.add(gman_do_high_background);
  registry.add(gman_do_low_background);
  return 0;
}

static int gearman_udf_plugin_deinit(drizzled::plugin::Registry &registry)
{
  registry.remove(gman_do_low_background);
  registry.remove(gman_do_high_background);
  registry.remove(gman_do_background);
  registry.remove(gman_do_low);
  registry.remove(gman_do_high);
  registry.remove(gman_do);
  registry.remove(gman_servers_set);
  delete gman_do_low_background;
  delete gman_do_high_background;
  delete gman_do_background;
  delete gman_do_low;
  delete gman_do_high;
  delete gman_do;
  delete gman_servers_set;
  return 0;
}

drizzle_declare_plugin(gearman_udf)
{
  "gearman_udf",
  "0.1",
  "Eric Day",
  "Gearman Client UDFs",
  PLUGIN_LICENSE_BSD,
  gearman_udf_plugin_init, /* Plugin Init */
  gearman_udf_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
