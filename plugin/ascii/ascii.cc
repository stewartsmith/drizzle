/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <drizzled/plugin.h>
#include <drizzled/function/math/int.h>
#include <drizzled/plugin/function.h>

using namespace std;
using namespace drizzled;

class AsciiFunction :public Item_int_func
{
  String value;
public:
  int64_t val_int();
  AsciiFunction() :Item_int_func() {}
  
  const char *func_name() const 
  { 
    return "ascii"; 
  }

  void fix_length_and_dec() 
  { 
    max_length= 3;
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }
};


int64_t AsciiFunction::val_int()
{
  assert(fixed == true);
  String *res= args[0]->val_str(&value);
  
  if (res == NULL)
  {
    null_value= true;
    return 0;
  }

  null_value= false;
  return (int64_t) ( res->length() != 0        ? 
                     (unsigned char) (*res)[0] :
                     (unsigned char) 0 
                   );
}

plugin::Create_function<AsciiFunction> *asciiudf= NULL;

static int initialize(module::Context &context)
{
  asciiudf= new plugin::Create_function<AsciiFunction>("ascii");
  context.add(asciiudf);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "ascii",
  "1.0",
  "Devananda van der Veen",
  "Return the ASCII value of a character",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
