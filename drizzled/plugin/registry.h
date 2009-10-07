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

class Registry
{
private:
  std::map<std::string, Handle *>
    plugin_map;

  Registry() {}
  Registry(const Registry&);
public:

  static plugin::Registry& singleton()
  {
    static plugin::Registry *registry= new plugin::Registry();
    return *registry;
  }

  static void shutdown()
  {
    plugin::Registry& registry= singleton();
    delete &registry;
  }

  Handle *find(const LEX_STRING *name);

  void add(Handle *plugin);

  std::vector<Handle *> getList(bool active);

  template<class T>
  void add(T *plugin)
  {
    bool failed= T::addPlugin(plugin);
    if (failed)
    {
      /* Can't use errmsg_printf here because we might be initializing the
       * error_message plugin.
       */
      /**
       * @TODO
       * Once plugin-base-class is merged, we'll add in this statment
       * fprintf(stderr,
       *       _("Fatal error: Failed initializing %s plugin."),
       *       plugin->getName().c_str());
       */
      unireg_abort(1);
    }
  }

  template<class T>
  void remove(T *plugin)
  {
    T::removePlugin(plugin);
  }

};

} /* end namespace plugin */
} /* end namespace drizzled */
#endif /* DRIZZLED_PLUGIN_REGISTRY_H */
