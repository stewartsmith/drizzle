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

#include <plugin/data_engine/function.h>
#include <drizzled/plugin/library.h>

using namespace std;
using namespace drizzled;

ModulesTool::ModulesTool() :
  plugin::TableFunction("DATA_DICTIONARY", "MODULES")
{
  add_field("MODULE_NAME");
  add_field("MODULE_VERSION", 20);
  add_field("MODULE_AUTHOR");
  add_field("IS_BUILTIN", plugin::TableFunction::BOOLEAN);
  add_field("MODULE_LIBRARY", 254);
  add_field("MODULE_DESCRIPTION", 254);
  add_field("MODULE_LICENSE", 80);
}

ModulesTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  plugin::Registry &registry= plugin::Registry::singleton();
  modules= registry.getList(true);
  it= modules.begin();
}

bool ModulesTool::Generator::populate()
{
  if (it == modules.end())
    return false;

  {
    plugin::Module *module= *it;
    const plugin::Manifest &manifest= module->getManifest();

    push(module->getName());

    push(manifest.version ? manifest.version : 0);

    push(manifest.author ? manifest.author : "<unknown>");

    push((module->plugin_dl == NULL));

    push ((module->plugin_dl == NULL) ? "builtin" : module->plugin_dl->getName());

    push(manifest.descr ? manifest.descr : "none");

    switch (manifest.license) {
    case PLUGIN_LICENSE_GPL:
      push("GPL");
      break;
    case PLUGIN_LICENSE_BSD:
      push("BSD");
      break;
    case PLUGIN_LICENSE_LGPL:
      push("LGPL");
      break;
    default:
      push("PROPRIETARY");
      break;
    }
  }

  it++;

  return true;
}
