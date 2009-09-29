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
#include <drizzled/server_includes.h>
#include <drizzled/gettext.h>
#include <drizzled/name_map.h>
#include "drizzled/plugin/function.h"

using namespace std;

namespace drizzled
{

NameMap<const plugin::Function *> udf_registry;

void plugin::Function::add(const plugin::Function *udf)
{
  udf_registry.add(udf);
}


void plugin::Function::remove(const plugin::Function *udf)
{
  udf_registry.remove(udf);
}

const plugin::Function *plugin::Function::get(const char *name, size_t length)
{
  return udf_registry.find(name, length);
}

} /* namespace drizzled */
