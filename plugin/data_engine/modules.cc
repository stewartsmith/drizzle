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
#include <drizzled/plugin/library.h>

using namespace std;
using namespace drizzled;

ModulesTool::ModulesTool() :
  Tool("MODULES")
{
  add_field("MODULE_NAME", message::Table::Field::VARCHAR, 64);
  add_field("MODULE_VERSION", message::Table::Field::VARCHAR, 20);
  add_field("MODULE_AUTHOR", message::Table::Field::VARCHAR, 64);
  add_field("IS_BUILTIN", message::Table::Field::VARCHAR, 3);
  add_field("MODULE_LIBRARY", message::Table::Field::VARCHAR, 254);
  add_field("MODULE_DESCRIPTION", message::Table::Field::VARCHAR, 254);
  add_field("MODULE_LICENSE", message::Table::Field::VARCHAR, 80);
}

ModulesTool::Generator::Generator()
{
  drizzled::plugin::Registry &registry= drizzled::plugin::Registry::singleton();
  modules= registry.getList(true);
  it= modules.begin();
}

bool ModulesTool::Generator::populate(Field ** fields)
{
  Field **field= fields;

  if (it == modules.end())
    return false;

  {
    drizzled::plugin::Module *module= *it;
    const drizzled::plugin::Manifest &manifest= module->getManifest();
    const CHARSET_INFO * const cs= system_charset_info;

    (*field)->store(module->getName().c_str(),
                    module->getName().size(), cs);
    field++;

    if (manifest.version)
    {
      (*field)->store(manifest.version, strlen(manifest.version), cs);
    }
    else
    {
      (*field)->store(0.0);
    }
    field++;

    if (manifest.author)
    {
      (*field)->store(manifest.author, strlen(manifest.author), cs);
    }
    else
    {
      (*field)->store("<unknown>", sizeof("<unknown>"), cs);
    }
    field++;

    if (module->plugin_dl == NULL)
    {
      (*field)->store("YES", sizeof("YES"), cs);
      field++;
      (*field)->store("<builtin>", sizeof("<builtin>"), cs);
    }
    else
    {
      (*field)->store("NO", sizeof("NO"), cs);
      field++;
      (*field)->store(module->plugin_dl->getName().c_str(),
                             module->plugin_dl->getName().size(), cs);
    }
    field++;

    if (manifest.descr)
    {
      (*field)->store(manifest.descr, strlen(manifest.descr), cs);
    }
    else
    {
      (*field)->store("<none>", sizeof("<none>"), cs);
    }
    field++;

    switch (manifest.license) {
    case PLUGIN_LICENSE_GPL:
      (*field)->store("GPL", sizeof("GPL"), cs);
      break;
    case PLUGIN_LICENSE_BSD:
      (*field)->store("BSD", sizeof("BSD"), cs);
      break;
    case PLUGIN_LICENSE_LGPL:
      (*field)->store("LGPL", sizeof("LGPL"), cs);
      break;
    default:
      (*field)->store("LGPL", sizeof("PROPRIETARY"), cs);
      break;
    }
  }

  it++;

  return true;
}
