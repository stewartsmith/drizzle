/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include <plugin/data_engine/dictionary.h>

using namespace std;
using namespace drizzled;

PluginsTool::PluginsTool() :
  Tool("PLUGINS")
{
  add_field(schema, "PLUGIN_NAME", message::Table::Field::VARCHAR, 64);
  add_field(schema, "PLUGIN_TYPE", message::Table::Field::VARCHAR, 64);
  add_field(schema, "IS_ACTIVE", message::Table::Field::VARCHAR, 3);
  add_field(schema, "MODULE_NAME", message::Table::Field::VARCHAR, 64);
}

PluginsTool::Generator::Generator()
{
  drizzled::plugin::Registry &registry= drizzled::plugin::Registry::singleton();
  const map<string, const drizzled::plugin::Plugin *> &plugin_map=
    registry.getPluginsMap();

  it= plugin_map.begin();
  end= plugin_map.end();
}

bool PluginsTool::Generator::populate(Field ** fields)
{
  const drizzled::plugin::Plugin *plugin= (*it).second;
  const CHARSET_INFO * const cs= system_charset_info;
  Field **field= fields;

  if (it == end)
    return false;

  (*field)->store(plugin->getName().c_str(),
                  plugin->getName().size(), cs);
  field++;

  (*field)->store(plugin->getTypeName().c_str(),
                  plugin->getTypeName().size(), cs);
  field++;

  if (plugin->isActive())
  {
    (*field)->store("YES", sizeof("YES"), cs);
  }
  else
  {
    (*field)->store("NO", sizeof("NO"), cs);
  }
  field++;

  (*field)->store(plugin->getModuleName().c_str(),
                  plugin->getModuleName().size(), cs);

  it++;

  return true;
}
