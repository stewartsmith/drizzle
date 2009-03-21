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

class Item_func_crc32 :public Item_int_func
{
  String value;
public:
  Item_func_crc32() :Item_int_func() { unsigned_flag= 1; }
  const char *func_name() const { return "crc32"; }
  void fix_length_and_dec() { max_length=10; }
  int64_t val_int();
};

int64_t Item_func_crc32::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (int64_t) crc32(0L, (unsigned char*)res->ptr(), res->length());
}

Create_function<Item_func_crc32> crc32udf(string("crc32"));

static int crc32udf_plugin_init(void *p)
{
  Function_builder **f = (Function_builder**) p;

  *f= &crc32udf;

  return 0;
}

static int crc32udf_plugin_deinit(void *p)
{
  Function_builder *udff = (Function_builder *) p;
  (void)udff;
  return 0;
}

drizzle_declare_plugin(crc32)
{
  DRIZZLE_UDF_PLUGIN,
  "crc32",
  "1.0",
  "Stewart Smith",
  "UDF for computing CRC32",
  PLUGIN_LICENSE_GPL,
  crc32udf_plugin_init, /* Plugin Init */
  crc32udf_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
