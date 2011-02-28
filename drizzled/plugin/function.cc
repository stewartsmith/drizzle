/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2000 MySQL AB
 *  Copyright (C) 2008, 2009 Sun Microsystems, Inc.
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

/* This implements 'user defined functions' */
#include <config.h>

#include <boost/unordered_map.hpp>

#include <drizzled/gettext.h>
#include <drizzled/plugin/function.h>
#include <drizzled/function_container.h>

#include <drizzled/util/string.h>

namespace drizzled
{

static plugin::Function::UdfMap udf_registry;

const plugin::Function::UdfMap &plugin::Function::getMap()
{
  return udf_registry;
}

bool plugin::Function::addPlugin(const plugin::Function *udf)
{
  if (FunctionContainer::getMap().find(udf->getName()) != FunctionContainer::getMap().end())
  {
    errmsg_printf(error::ERROR,
                  _("A function named %s already exists!\n"),
                  udf->getName().c_str());
    return true;
  }

  if (udf_registry.find(udf->getName()) != udf_registry.end())
  {
    errmsg_printf(error::ERROR,
                  _("A function named %s already exists!\n"),
                  udf->getName().c_str());
    return true;
  }

  std::pair<UdfMap::iterator, bool> ret=
    udf_registry.insert(make_pair(udf->getName(), udf));

  if (ret.second == false)
  {
    errmsg_printf(error::ERROR,
                  _("Could not add Function!\n"));
    return true;
  }

  return false;
}


void plugin::Function::removePlugin(const plugin::Function *udf)
{
  udf_registry.erase(udf->getName());
}

const plugin::Function *plugin::Function::get(const std::string &name)
{
  UdfMap::iterator iter= udf_registry.find(name);
  if (iter == udf_registry.end())
  {
    return NULL;
  }
  return iter->second;
}

} /* namespace drizzled */
