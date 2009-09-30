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

#ifndef DRIZZLED_PLUGIN_REGISTRY_H
#define DRIZZLED_PLUGIN_REGISTRY_H

#include "drizzled/registry.h"
#include <string>
#include <vector>
#include <map>

#include "drizzled/gettext.h"
#include "drizzled/unireg.h"

namespace drizzled
{
namespace plugin
{
class Handle;
class Plugin;

class Registry
{
private:
  std::map<std::string, Handle *> handle_map;
  ::drizzled::Registry<const Plugin *> plugin_registry;

  Handle *current_handle;

  Registry()
   : handle_map(), plugin_registry(), current_handle(NULL)
  { }

  Registry(const Registry&);
  void addPlugin(Plugin *plugin);
  void removePlugin(const Plugin *plugin);
public:

  static plugin::Registry& singleton()
  {
    static plugin::Registry registry;
    return registry;
  }

  Handle *find(const LEX_STRING *name);

  void add(Handle *handle);

  void setCurrentHandle(Handle *plugin)
  {
    current_handle= plugin;
  }

  void clearCurrentHandle()
  {
    current_handle= NULL;
  }

  std::vector<Handle *> getList(bool active);

  template<class T>
  void add(T *plugin)
  {
    plugin->setHandle(current_handle);
    bool failed= false;
    if (plugin_registry.add(plugin))
      failed= true;
    if (T::addPlugin(plugin))
      failed= true;
    if (failed)
    {
      /* Can't use errmsg_printf here because we might be initializing the
       * error_message plugin.
       */
      fprintf(stderr,
              _("Fatal error: Failed initializing %s plugin."),
              plugin->getName().c_str());
      unireg_abort(1);
    }
  }

  template<class T>
  void remove(T *plugin)
  {
    T::removePlugin(plugin);
    plugin_registry.remove(plugin);
  }

};

} /* end namespace plugin */
} /* end namespace drizzled */
#endif /* DRIZZLED_PLUGIN_REGISTRY_H */
