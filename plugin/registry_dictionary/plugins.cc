/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <plugin/registry_dictionary/dictionary.h>

using namespace drizzled;

PluginsTool::PluginsTool() :
  plugin::TableFunction("DATA_DICTIONARY", "PLUGINS")
{
  add_field("PLUGIN_NAME");
  add_field("PLUGIN_TYPE");
  add_field("IS_ACTIVE", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("MODULE_NAME");
}

PluginsTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  module::Registry &registry= module::Registry::singleton();
  const plugin::Plugin::map &plugin_map=
    registry.getPluginsMap();

  it= plugin_map.begin();
  end= plugin_map.end();
}

bool PluginsTool::Generator::populate()
{
  if (it == end)
    return false;

  const plugin::Plugin *plugin= (*it).second;

  /* PLUGIN_NAME */
  push(plugin->getName());

  /* PLUGIN_TYPE */
  push(plugin->getTypeName());

  /* IS_ACTIVE */
  push(plugin->isActive());

  /* MODULE_NAME */
  push(plugin->getModuleName());

  it++;

  return true;
}
