/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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



#pragma once

#include <string>
#include <vector>
#include <map>

#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

class DRIZZLED_API Plugin : boost::noncopyable
{
private:
  const std::string _name;
  bool _is_active;
  module::Module *_module;
  const std::string _type_name;

public:
  typedef std::pair<const std::string, const std::string> map_key;
  typedef std::map<const map_key, plugin::Plugin *> map;
  typedef std::vector<Plugin *> vector;

  explicit Plugin(const std::string &name, const std::string &type_name);
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

  virtual void startup(drizzled::Session &)
  {
  }
 
  void activate()
  {
    _is_active= true;
  }
 
  void deactivate()
  {
    _is_active= false;
  }
 
  bool isActive() const
  {
    return _is_active;
  }

  const std::string &getName() const
  {
    return _name;
  } 

  void setModule(module::Module *module)
  {
    _module= module;
  }

  const std::string& getTypeName() const
  {
    return _type_name;
  }

  virtual bool removeLast() const
  {
    return false;
  }

  const std::string& getModuleName() const;
};
} /* end namespace plugin */
} /* end namespace drizzled */

