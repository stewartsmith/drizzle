/* Copyright (C) 2006 MySQL AB

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

#include <plugin/single_thread/single_thread.h>

using namespace std;
using namespace drizzled;


/* Global's (TBR) */
static SingleThreadScheduler *scheduler= NULL;


static int init(drizzled::plugin::Registry &registry)
{
  scheduler= new SingleThreadScheduler("single_thread");
  registry.add(scheduler);
  return 0;
}

static int deinit(drizzled::plugin::Registry &registry)
{
  registry.remove(scheduler);
  delete scheduler;

  return 0;
}

static struct st_mysql_sys_var* system_variables[]= {
  NULL
};

drizzle_declare_plugin(single_thread)
{
  "single_thread",
  "0.1",
  "Brian Aker",
  "Single Thread Scheduler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  system_variables,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
