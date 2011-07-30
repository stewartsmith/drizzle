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
#include <drizzled/util/find_ptr.h>
#include <drizzled/util/string.h>

namespace drizzled {

static plugin::Function::UdfMap udf_registry;

const plugin::Function::UdfMap &plugin::Function::getMap()
{
  return udf_registry;
}

bool plugin::Function::addPlugin(const plugin::Function *udf)
{
  if (FunctionContainer::getMap().count(udf->getName()))
  {
    errmsg_printf(error::ERROR, _("A function named %s already exists!\n"), udf->getName().c_str());
    return true;
  }

  if (udf_registry.count(udf->getName()))
  {
    errmsg_printf(error::ERROR, _("A function named %s already exists!\n"), udf->getName().c_str());
    return true;
  }

  if (not udf_registry.insert(make_pair(udf->getName(), udf)).second)
  {
    errmsg_printf(error::ERROR, _("Could not add Function!\n"));
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
  return find_ptr2(udf_registry, name);
}

} /* namespace drizzled */
