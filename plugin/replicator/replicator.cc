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

#define DRIZZLE_SERVER 1 /* for session variable max_allowed_packet */
#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/item/strfunc.h>

mysql_declare_plugin(replicator)
{
  DRIZZLE_REPLICATOR_PLUGIN,
  "replicator",
  "0.1",
  "Brian Aker",
  "Basic replication module",
  PLUGIN_LICENSE_GPL,
  NULL, /* Plugin Init */
  NULL, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
