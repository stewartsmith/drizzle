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

namespace drizzled
{
namespace generator
{

Functions::Functions(Session &arg) :
  session(arg)
{
  function_list.reserve(plugin::Function::getMap().size() + FunctionContainer::getMap().size());

  std::transform(FunctionContainer::getMap().begin(),
                 FunctionContainer::getMap().end(),
                 std::back_inserter(function_list),
                 boost::bind(&FunctionContainer::Map::value_type::first, _1) );

  std::transform(plugin::Function::getMap().begin(),
                 plugin::Function::getMap().end(),
                 std::back_inserter(function_list),
                 boost::bind(&plugin::Function::Map::value_type::first, _1) );

  iter= function_list.begin();
}

} /* namespace generator */
} /* namespace drizzled */
