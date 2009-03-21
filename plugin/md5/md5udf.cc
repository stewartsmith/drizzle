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
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

#include <openssl/md5.h>

#include <stdio.h>

using namespace std;

class Item_func_md5 : public Item_str_func
{
public:
  Item_func_md5() : Item_str_func() {}
  const char *func_name() const { return "md5"; }
  String *val_str(String*);
  void fix_length_and_dec() {
    max_length=32;
    args[0]->collation.set(
      get_charset_by_csname(args[0]->collation.collation->csname,
                            MY_CS_BINSORT), DERIVATION_COERCIBLE);
  }

};


String *Item_func_md5::val_str(String *str)
{
  assert(fixed == 1);
  String * sptr= args[0]->val_str(str);
  str->set_charset(&my_charset_bin);
  if (sptr)
  {
    MD5_CTX context;
    unsigned char digest[16];

    null_value=0;
    MD5_Init (&context);
    MD5_Update (&context,(unsigned char *) sptr->ptr(), sptr->length());
    MD5_Final (digest, &context);
    if (str->alloc(32))				// Ensure that memory is free
    {
      null_value=1;
      return 0;
    }
    snprintf((char *) str->ptr(), 33,
	    "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	    digest[0], digest[1], digest[2], digest[3],
	    digest[4], digest[5], digest[6], digest[7],
	    digest[8], digest[9], digest[10], digest[11],
	    digest[12], digest[13], digest[14], digest[15]);
    str->length((uint) 32);
    return str;
  }
  null_value=1;
  return 0;
}


Create_function<Item_func_md5> md5udf(string("md5"));

static int md5udf_plugin_init(void *p)
{
  Function_builder **f = (Function_builder**) p;

  *f= &md5udf;

  return 0;
}

static int md5udf_plugin_deinit(void *p)
{
  Function_builder *udff = (Function_builder *) p;
  (void)udff;
  return 0;
}

drizzle_declare_plugin(md5)
{
  DRIZZLE_UDF_PLUGIN,
  "md5",
  "1.0",
  "Stewart Smith",
  "UDF for computing md5sum",
  PLUGIN_LICENSE_GPL,
  md5udf_plugin_init, /* Plugin Init */
  md5udf_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
