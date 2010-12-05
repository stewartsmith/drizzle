/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_PLUGIN_PLUGIN_H
#define DRIZZLED_PLUGIN_PLUGIN_H

#include <string>
#include <vector>
#include <map>

namespace drizzled
{
namespace module
{
class Module;
}

namespace plugin
{

class Plugin
{
private:
  const std::string name;
  bool is_active;
  module::Module *module;
  const std::string type_name;

  Plugin();
  Plugin(const Plugin&);
  Plugin& operator=(const Plugin &);
public:
  typedef std::map<std::string, Plugin *> map;
  typedef std::vector<Plugin *> vector;

  explicit Plugin(std::string in_name, std::string in_type_name);
  virtual ~Plugin() {}

  /*
   * This method is called for all plug-ins on shutdown,
   * _before_ the plug-ins are deleted. It can be used
   * when shutdown code references other plug-ins.
   */
  virtual void shutdownPlugin()
  {
  }

  // This is run after all plugins have been initialized.
  virtual void prime()
  {
  }
 
  void activate()
  {
    is_active= true;
  }
 
  void deactivate()
  {
    is_active= false;
  }
 
  bool isActive() const
  {
    return is_active;
  }

  const std::string &getName() const
  {
    return name;
  } 

  void setModule(module::Module *module_arg)
  {
    module= module_arg;
  }

  const std::string& getTypeName() const
  {
    return type_name;
  }

  const std::string& getModuleName() const;
};
} /* end namespace plugin */
} /* end namespace drizzled */

#endif /* DRIZZLED_PLUGIN_PLUGIN_H */
