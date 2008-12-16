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

#include <drizzled/server_includes.h>
#include <drizzled/sql_udf.h>
#include <drizzled/function/math/int.h>

class Item_func_uncompressed_length : public Item_int_func
{
  String value;
public:
  Item_func_uncompressed_length():Item_int_func(){}
  const char *func_name() const{return "uncompressed_length";}
  void fix_length_and_dec() { max_length=10; }
  int64_t val_int();
};

int64_t Item_func_uncompressed_length::val_int()
{
  assert(fixed == 1);
  String *res= args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  if (res->is_empty()) return 0;

  /*
    res->ptr() using is safe because we have tested that string is not empty,
    res->c_ptr() is not used because:
      - we do not need \0 terminated string to get first 4 bytes
      - c_ptr() tests simbol after string end (uninitialiozed memory) which
        confuse valgrind
  */
  return uint4korr(res->ptr()) & 0x3FFFFFFF;
}

Item_func* create_uncompressed_lengthudf_item(MEM_ROOT* m)
{
  return  new (m) Item_func_uncompressed_length();
}

static struct udf_func uncompressed_lengthudf = {
  { C_STRING_WITH_LEN("uncompressed_length") },
  create_uncompressed_lengthudf_item
};

static int uncompressed_lengthudf_plugin_init(void *p)
{
  udf_func **f = (udf_func**) p;

  *f= &uncompressed_lengthudf;

  return 0;
}

static int uncompressed_lengthudf_plugin_deinit(void *p)
{
  udf_func *udff = (udf_func *) p;
  (void)udff;
  return 0;
}

mysql_declare_plugin(uncompressed_length)
{
  DRIZZLE_UDF_PLUGIN,
  "uncompressed_length",
  "1.0",
  "Stewart Smith",
  "UDF for compress()",
  PLUGIN_LICENSE_GPL,
  uncompressed_lengthudf_plugin_init, /* Plugin Init */
  uncompressed_lengthudf_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
