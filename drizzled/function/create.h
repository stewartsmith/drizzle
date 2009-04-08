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

#ifndef DRIZZLED_FUNCTION_CREATE_H
#define DRIZZLED_FUNCTION_CREATE_H


#include <drizzled/item.h>
#include <drizzled/sql_list.h>
#include <drizzled/item/bin_string.h>

#include <string>
#include <vector>
#include <functional>

class Function_builder
  : public std::unary_function<MEM_ROOT*, Item_func *>
{
  std::string name;
  std::vector<std::string> aliases;
public:
  Function_builder(std::string in_name) : name(in_name) {}
  virtual result_type operator()(argument_type session) const= 0;
  virtual ~Function_builder() {}

  std::string getName() const
  {
    return name;
  }

  const std::vector<std::string>& getAliases() const
  {
    return aliases;
  }

  void addAlias(std::string alias)
  {
    aliases.push_back(alias);
  }
};

template<class T>
class Create_function : public Function_builder
{
public:
  typedef T Function_class;
  Create_function(std::string in_name): Function_builder(in_name) {}
  virtual result_type operator()(argument_type root) const
  {
    return new (root) Function_class();
  }
};



#endif /* DRIZZLED_FUNCTION_CREATE_H */
