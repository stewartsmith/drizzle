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

#include <string>
#include <vector>
#include <map>

class Plugin_registry_impl
{
private:
  std::map<std::string, st_plugin_int *>
    plugin_map[DRIZZLE_MAX_PLUGIN_TYPE_NUM];

  Plugin_registry_impl(const Plugin_registry_impl&);
public:
  Plugin_registry_impl() {}

  st_plugin_int *find(const LEX_STRING *name, int type);

  void add(st_mysql_plugin *handle, st_plugin_int *plugin);

  void get_mask_list(uint32_t type,
                     std::vector<st_plugin_int *> &plugins,
                     uint32_t state_mask);

};
