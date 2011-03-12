/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *  Copyright (C) 2010 Stewart Smith
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
#include <drizzled/plugin/function.h>
#include <plugin/math_functions/functions.h>
#include <plugin/math_functions/abs.h>
#include <plugin/math_functions/acos.h>
#include <plugin/math_functions/asin.h>
#include <plugin/math_functions/atan.h>
#include <plugin/math_functions/cos.h>
#include <plugin/math_functions/log.h>
#include <plugin/math_functions/sin.h>
#include <plugin/math_functions/pow.h>
#include <plugin/math_functions/ln.h>
#include <plugin/math_functions/sqrt.h>
#include <plugin/math_functions/ceiling.h>
#include <plugin/math_functions/exp.h>
#include <plugin/math_functions/floor.h>
#include <plugin/math_functions/ord.h>

using namespace drizzled;

static int init(drizzled::module::Context &context)
{
  context.add(new plugin::Create_function<Item_func_abs>("abs"));
  context.add(new plugin::Create_function<Item_func_acos>("acos"));
  context.add(new plugin::Create_function<Item_func_asin>("asin"));
  context.add(new plugin::Create_function<Item_func_atan>("atan"));
  context.add(new plugin::Create_function<Item_func_atan>("atan2"));
  context.add(new plugin::Create_function<Item_func_cos>("cos"));
  context.add(new plugin::Create_function<Item_func_log>("log"));
  context.add(new plugin::Create_function<Item_func_log2>("log2"));
  context.add(new plugin::Create_function<Item_func_log10>("log10"));
  context.add(new plugin::Create_function<Item_func_sin>("sin"));
  context.add(new plugin::Create_function<Item_func_pow>("pow"));
  context.add(new plugin::Create_function<Item_func_pow>("power"));
  context.add(new plugin::Create_function<Item_func_ln>("ln"));
  context.add(new plugin::Create_function<Item_func_sqrt>("sqrt"));
  context.add(new plugin::Create_function<Item_func_ceiling>("ceil"));
  context.add(new plugin::Create_function<Item_func_ceiling>("ceiling"));
  context.add(new plugin::Create_function<Item_func_exp>("exp"));
  context.add(new plugin::Create_function<Item_func_floor>("floor"));
  context.add(new plugin::Create_function<Item_func_ord>("ord"));

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "Math Functions",
  "1.0",
  "Brian Aker, Stewart Smith",
  "Math Functions.",
  PLUGIN_LICENSE_GPL,
  init,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
