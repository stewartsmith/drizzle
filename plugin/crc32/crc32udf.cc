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
#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include <drizzled/algorithm/crc32.h>

#include <string>

using namespace std;
using namespace drizzled;

class Crc32Function : public Item_int_func
{
public:
  int64_t val_int();
  
  Crc32Function()
  { 
    unsigned_flag= true; 
  }
  
  const char *func_name() const 
  { 
    return "crc32"; 
  }
  
  void fix_length_and_dec() 
  { 
    max_length= 10; 
  }
  
  bool check_argument_count(int n) 
  { 
    return (n == 1); 
  }
};

int64_t Crc32Function::val_int()
{
  assert(fixed == true);
  String value;
  String *res=args[0]->val_str(&value);
  
  if (res == NULL)
  {
    null_value= true;
    return 0;
  }

  null_value= false;
  return static_cast<int64_t>(drizzled::algorithm::crc32(res->ptr(), res->length()));
}

static int initialize(module::Context &context)
{
  context.add(new plugin::Create_function<Crc32Function>("crc32"));
  return 0;
}

DRIZZLE_PLUGIN(initialize, NULL, NULL);
