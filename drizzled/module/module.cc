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

#include <algorithm>
#include <drizzled/module/module.h>
#include <drizzled/sys_var.h>
#include <drizzled/util/functors.h>
#include <drizzled/util/tokenize.h>
#include <drizzled/module/manifest.h>
#include <drizzled/module/vertex_handle.h>

namespace drizzled {
namespace module {

Module::Module(const Manifest *manifest_arg, Library *library_arg) :
  plugin_dl(library_arg),
  isInited(false),
  name(manifest_arg->name),
  manifest(*manifest_arg),
  vertex_(NULL)
{
  if (manifest.depends != NULL)
  {
    tokenize(manifest.depends, depends_, ",", true);
  }
}

Module::~Module()
{
  std::for_each(sys_vars.begin(), sys_vars.end(), DeletePtr());
  delete vertex_;
}

} /* namespace module */
} /* namespace drizzled */
