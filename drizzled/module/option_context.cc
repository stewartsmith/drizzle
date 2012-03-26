/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

#include <config.h>
#include <boost/algorithm/string.hpp>
#include "drizzled/module/option_context.h"

namespace drizzled {
namespace module {
  
option_context::option_context(const std::string &module_name_in,
                               po::options_description_easy_init po_options_in) :
  module_name(module_name_in),
  po_options(po_options_in)
{ }

option_context& option_context::operator()(const char* name, const char* description)
{
  po_options(prepend_name(module_name, name).c_str(), description);
  return *this;
}

option_context& option_context::operator()(const char* name, const po::value_semantic* s)
{
  po_options(prepend_name(module_name, name).c_str(), s);
  return *this;
}

option_context& option_context::operator()(const char* name, const po::value_semantic* s, const char* description)
{
  po_options(prepend_name(module_name, name).c_str(), s, description);
  return *this;
}

std::string option_context::prepend_name(std::string in_module_name, const char *name_in)
{
  in_module_name.push_back('.');
  std::replace(in_module_name.begin(), in_module_name.end(), '_', '-');
  boost::to_lower(in_module_name);
  in_module_name.append(name_in);
  return in_module_name;
}

} /* namespace module */
} /* namespace drizzled */

