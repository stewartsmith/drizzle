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
#include <drizzled/module/library.h>

using namespace std;
using namespace drizzled;

/* 
 * Suffix _STRING was added because sys/param.h on OSX defines a BSD symbol
 * to the version of BSD being run. FAIL
 */
static const string GPL_STRING("GPL");
static const string LGPL_STRING("LGPL");
static const string BSD_STRING("BSD");
static const string PROPRIETARY_STRING("PROPRIETARY");

ModulesTool::ModulesTool() :
  plugin::TableFunction("DATA_DICTIONARY", "MODULES")
{
  add_field("MODULE_NAME");
  add_field("MODULE_VERSION", 20);
  add_field("MODULE_AUTHOR");
  add_field("IS_BUILTIN", plugin::TableFunction::BOOLEAN, 0, false);
  add_field("MODULE_LIBRARY", 254);
  add_field("MODULE_DESCRIPTION", 254);
  add_field("MODULE_LICENSE", 80);
}

ModulesTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  module::Registry &registry= module::Registry::singleton();
  modules= registry.getList();
  it= modules.begin();
}

bool ModulesTool::Generator::populate()
{
  if (it == modules.end())
    return false;

  {
    module::Module *module= *it;
    const module::Manifest &manifest= module->getManifest();

    /* MODULE_NAME */
    push(module->getName());

    /* MODULE_VERSION */
    push(manifest.version ? manifest.version : 0);

    /* MODULE_AUTHOR */
    manifest.author ? push(manifest.author) : push();

    /* IS_BUILTIN */
    push((module->plugin_dl == NULL));

    /* MODULE_LIBRARY */
    push((module->plugin_dl == NULL) ? "builtin" : module->plugin_dl->getName());

    /* MODULE_DESCRIPTION */
    manifest.descr ? push(manifest.descr) : push();

    /* MODULE_LICENSE */
    switch (manifest.license) {
    case PLUGIN_LICENSE_GPL:
      push(GPL_STRING);
      break;
    case PLUGIN_LICENSE_BSD:
      push(BSD_STRING);
      break;
    case PLUGIN_LICENSE_LGPL:
      push(LGPL_STRING);
      break;
    default:
      push(PROPRIETARY_STRING);
      break;
    }
  }

  it++;

  return true;
}
