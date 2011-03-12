/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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
#include <plugin/catalog/module.h>

static int init(drizzled::module::Context &context)
{
  context.add(new drizzled::plugin::Create_function<plugin::catalog::functions::Create>("create_catalog"));
  context.add(new drizzled::plugin::Create_function<plugin::catalog::functions::Drop>("drop_catalog"));
  context.add(new drizzled::plugin::Create_function<plugin::catalog::functions::Drop>("lock_catalog"));
  context.add(new drizzled::plugin::Create_function<plugin::catalog::functions::Drop>("unlock_catalog"));
  context.add(new plugin::catalog::Filesystem());
  context.add(new plugin::catalog::tables::Cache());
  context.add(new plugin::catalog::tables::Catalogs());

  return 0;
}


DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "Catalog System",
  "0.1",
  "Brian Aker",
  "Basic Catalog functions, data dictionary, and system.",
  drizzled::PLUGIN_LICENSE_GPL,
  init,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
