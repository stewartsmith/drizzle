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

#include <config.h>
#include <drizzled/plugin/function.h>

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

static int gearman_udf_plugin_init(drizzled::module::Context &context)
{
  gman_servers_set= new plugin::Create_function<Item_func_gman_servers_set>("gman_servers_set");
  gman_do= new plugin::Create_function<Item_func_gman_do>("gman_do");
  gman_do_high= new plugin::Create_function<Item_func_gman_do_high>("gman_do_high");
  gman_do_low= new plugin::Create_function<Item_func_gman_do_low>("gman_do_low");
  gman_do_background= new plugin::Create_function<Item_func_gman_do_background>("gman_do_background");
  gman_do_high_background= new plugin::Create_function<Item_func_gman_do_high_background>("gman_do_high_background");
  gman_do_low_background= new plugin::Create_function<Item_func_gman_do_low_background>("gman_do_low_background");
  context.add(gman_servers_set);
  context.add(gman_do);
  context.add(gman_do_high);
  context.add(gman_do_low);
  context.add(gman_do_background);
  context.add(gman_do_high_background);
  context.add(gman_do_low_background);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "gearman_udf",
  "0.1",
  "Eric Day",
  "Gearman Client UDFs",
  PLUGIN_LICENSE_BSD,
  gearman_udf_plugin_init, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
