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
#include "config.h"

#include <drizzled/unordered_map.h>

#include <drizzled/gettext.h>
#include "drizzled/plugin/function.h"

using namespace std;

namespace drizzled
{

typedef unordered_map<string, const plugin::Function *> UdfMap;
static UdfMap udf_registry;

bool plugin::Function::addPlugin(const plugin::Function *udf)
{
  string lower_name(udf->getName());
  transform(lower_name.begin(), lower_name.end(),
            lower_name.begin(), ::tolower);
  if (udf_registry.find(lower_name) != udf_registry.end())
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("A function named %s already exists!\n"),
                  udf->getName().c_str());
    return true;
  }
  pair<UdfMap::iterator, bool> ret=
    udf_registry.insert(make_pair(lower_name, udf));
  if (ret.second == false)
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("Could not add Function!\n"));
    return true;
  }
  return false;
}


void plugin::Function::removePlugin(const plugin::Function *udf)
{
  string lower_name(udf->getName());
  transform(lower_name.begin(), lower_name.end(),
            lower_name.begin(), ::tolower);
  udf_registry.erase(lower_name);
}

const plugin::Function *plugin::Function::get(const char *name, size_t length)
{
  string lower_name(name, length);
  transform(lower_name.begin(), lower_name.end(),
            lower_name.begin(), ::tolower);
  UdfMap::iterator iter= udf_registry.find(lower_name);
  if (iter == udf_registry.end())
  {
    return NULL;
  }
  return (*iter).second;
}

} /* namespace drizzled */
