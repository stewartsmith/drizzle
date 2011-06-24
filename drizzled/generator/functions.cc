/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <drizzled/generator.h>
#include <drizzled/function_container.h>
#include <drizzled/plugin/function.h>
#include <drizzled/session.h>

using namespace std;

namespace drizzled {
namespace generator {

Functions::Functions(Session&)
{
  functions.reserve(plugin::Function::getMap().size() + FunctionContainer::getMap().size());

  BOOST_FOREACH(FunctionContainer::Map::const_reference it, FunctionContainer::getMap()) 
    functions.push_back(it.first);

  BOOST_FOREACH(plugin::Function::Map::const_reference it, plugin::Function::getMap())
    functions.push_back(it.first);

  iter= functions.begin();
}

} /* namespace generator */
} /* namespace drizzled */
