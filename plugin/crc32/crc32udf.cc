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

#include <drizzled/server_includes.h>
#include <drizzled/sql_udf.h>
#include <drizzled/item/func.h>
#include <zlib.h>

#include <string>

using namespace std;

class crc32Function :public Item_int_func
{
  String value;
public:
  int64_t val_int();
  
  crc32Function() :Item_int_func() 
  { 
    unsigned_flag= 1; 
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

int64_t crc32Function::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  
  if (res == false)
  {
    null_value= 1;
    return 0; /* purecov: inspected */
  }
  null_value= 0;
  return (int64_t) crc32(0L, (unsigned char*)res->ptr(), res->length());
}

Create_function<crc32Function> crc32udf(string("crc32"));

static int initialize(PluginRegistry &registry)
{
  registry.add(&crc32udf);
  return 0;
}

static int finalize(PluginRegistry &registry)  
{
  registry.remove(&crc32udf);
  return 0;
}

drizzle_declare_plugin(crc32)
{
  "crc32",
  "1.0",
  "Stewart Smith",
  "UDF for computing CRC32",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  finalize,   /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
