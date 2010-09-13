/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include "config.h"
#include <drizzled/plugin/function.h>
#include "plugin/math_functions/functions.h"
#include "plugin/math_functions/abs.h"
#include "plugin/math_functions/acos.h"

using namespace drizzled;

plugin::Create_function<drizzled::Item_func_abs> *abs_function= NULL;
plugin::Create_function<drizzled::Item_func_acos> *acos_function= NULL;

static int init(drizzled::module::Context &context)
{
  abs_function= new plugin::Create_function<Item_func_abs>("abs");
  acos_function= new plugin::Create_function<Item_func_acos>("acos");

  context.add(abs_function);
  context.add(acos_function);

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "Math Functions",
  "1.0",
  "Brian Aker",
  "Math Functions.",
  PLUGIN_LICENSE_GPL,
  init,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
