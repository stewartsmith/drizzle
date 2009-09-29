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
    static plugin::Registry registry;
    return registry;
  }

  Handle *find(const LEX_STRING *name);

  void add(Handle *plugin);

  std::vector<Handle *> get_list(bool active);

  template<class T>
  void add(T *plugin)
  {
    T::add(plugin);
  }

  template<class T>
  void remove(T *plugin)
  {
    T::remove(plugin);
  }

};

} /* end namespace plugin */
} /* end namespace drizzled */
#endif /* DRIZZLED_PLUGIN_REGISTRY_H */
