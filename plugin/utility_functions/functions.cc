/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *  Copyright (C) 2010 Stewart Smith
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <drizzled/plugin.h>
#include <plugin/utility_functions/functions.h>

using namespace drizzled;

static int init(drizzled::module::Context &context)
{
  context.add(new plugin::Create_function<utility_functions::Assert>("assert"));
  context.add(new plugin::Create_function<utility_functions::BitCount>("bit_count"));
  context.add(new plugin::Create_function<utility_functions::Catalog>("catalog"));
  context.add(new plugin::Create_function<utility_functions::Execute>("execute"));
  context.add(new plugin::Create_function<utility_functions::GlobalReadLock>("global_read_lock"));
  context.add(new plugin::Create_function<utility_functions::ResultType>("result_type"));
  context.add(new plugin::Create_function<utility_functions::Kill>("kill"));
  context.add(new plugin::Create_function<utility_functions::Schema>("database"));
  context.add(new plugin::Create_function<utility_functions::Typeof>("typeof"));
  context.add(new plugin::Create_function<utility_functions::User>("user"));

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "Utility Functions",
  "1.4",
  "Brian Aker, Stewart Smith",
  "Utility Functions.",
  PLUGIN_LICENSE_GPL,
  init,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
