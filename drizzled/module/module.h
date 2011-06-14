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

/**
 * @file Defines a Plugin Module
 *
 * A plugin::Module is the fundamental functional element of the plugin system.
 * Plugins are inited and deinited by module. A module init can register one
 * or more plugin::Plugin objects. 
 */

#include <drizzled/common_fwd.h>
#include <string>
#include <vector>

namespace drizzled {

void module_shutdown(module::Registry&);

namespace module {

/* A plugin module */
class Module
{
public:
  typedef std::vector<sys_var *> Variables;
  typedef std::vector<std::string> Depends;

  Library *plugin_dl;
  bool isInited;
  Variables system_vars;         /* server variables for this plugin */
  Variables sys_vars;
  Depends depends_;

  Module(const Manifest *manifest_arg, Library *library_arg);
  ~Module();

  const std::string& getName() const
  {
    return name;
  }

  const Manifest& getManifest() const
  {
    return manifest;
  }

  void addMySysVar(sys_var *var)
  {
    sys_vars.push_back(var);
    addSysVar(var);
  }

  void addSysVar(sys_var *var)
  {
    system_vars.push_back(var);
  }

  Variables &getSysVars()
  {
    return system_vars;
  }

  const Depends &getDepends() const
  {
    return depends_;
  }

  void setVertexHandle(VertexHandle *vertex)
  {
    vertex_= vertex;
  }

  VertexHandle *getVertexHandle()
  {
    return vertex_;
  }
private:
  const std::string name;
  const Manifest &manifest;
  VertexHandle *vertex_;
};

} /* namespace module */
} /* namespace drizzled */

