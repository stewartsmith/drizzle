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

#include "config.h"
#include <plugin/single_thread/single_thread.h>

using namespace std;
using namespace drizzled;


/* Global's (TBR) */
static SingleThreadScheduler *scheduler= NULL;


static int init(plugin::Context &context)
{
  scheduler= new SingleThreadScheduler("single_thread");
  context.add(scheduler);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "single_thread",
  "0.1",
  "Brian Aker",
  "Single Thread Scheduler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  NULL,   /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
