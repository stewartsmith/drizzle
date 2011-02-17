/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Andrew Hutchings
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

#include <drizzled/plugin/function.h>
#include <drizzled/function/math/int.h>

using namespace drizzled;

class CoercibilityFunction :public Item_int_func
{
public:
  CoercibilityFunction() :Item_int_func() 
  {
    unsigned_flag= true;
  }

  int64_t val_int();

  const char *func_name() const
  {
    return "coercibility";
  }

  void fix_length_and_dec()
  {
    max_length=10; maybe_null= 0;
  }

  table_map not_null_tables() const
  {
    return 0;
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }
};

int64_t CoercibilityFunction::val_int()
{
  assert(fixed == 1);
  null_value= 0;
  return (int64_t) args[0]->collation.derivation;
}

plugin::Create_function<CoercibilityFunction> *coercibility_function= NULL;

static int initialize(drizzled::module::Context &context)
{
  coercibility_function= new plugin::Create_function<CoercibilityFunction>("coercibility");
  context.add(coercibility_function);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "coercibility_function",
  "1.0",
  "Andrew Hutchings",
  "COERCIBILITY()",
  PLUGIN_LICENSE_GPL,
  initialize,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
