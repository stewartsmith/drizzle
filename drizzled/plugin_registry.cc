/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include "drizzled/server_includes.h"

#include "drizzled/plugin_registry.h"
#include "drizzled/plugin_registry_impl.h"

#include <vector>

using namespace std;


static Plugin_registry the_registry;

Plugin_registry::Plugin_registry()
{
  pImpl= new Plugin_registry_impl();
}

Plugin_registry& Plugin_registry::get_plugin_registry()
{
  return the_registry;
}

st_plugin_int *Plugin_registry::find(const LEX_STRING *name, int type)
{
  return pImpl->find(name, type);
}

void Plugin_registry::add(st_mysql_plugin *handle, st_plugin_int *plugin)
{
  pImpl->add(handle, plugin);
}

void Plugin_registry::get_list(uint32_t type, vector<st_plugin_int *> &plugins)
{
  pImpl->get_list(type, plugins, false);
}

void Plugin_registry::get_list(uint32_t type, vector<st_plugin_int *> &plugins, bool all)
{
  pImpl->get_list(type, plugins, all);
}
