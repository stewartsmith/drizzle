/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>

#include CSTDINT_H
#include <drizzled/common_includes.h>
#include <drizzled/item_func.h>
#include <zlib.h>

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

Item_func* create_crc32udf_item(MEM_ROOT* m)
{
  return  new (m) Item_func_crc32();
}

static struct udf_func crc32udf = {
  { C_STRING_WITH_LEN("crc32") },
  create_crc32udf_item
};

static int crc32udf_plugin_init(void *p)
{
  udf_func **f = (udf_func**) p;

  *f= &crc32udf;

  return 0;
}

static int crc32udf_plugin_deinit(void *p)
{
  udf_func *udff = (udf_func *) p;
  (void)udff;
  return 0;
}

mysql_declare_plugin(crc32)
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
mysql_declare_plugin_end;
