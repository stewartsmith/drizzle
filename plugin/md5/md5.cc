/* vim: expandtab:shiftwidth=2:tabstop=2:smarttab: 
   Copyright (C) 2006 MySQL AB

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

#include <cstdio>
#include <cstddef>

#include <gcrypt.h>

#include <drizzled/charset.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/item/func.h>
#include <drizzled/plugin/function.h>

using namespace std;
using namespace drizzled;

class Md5Function : public Item_str_func
{
public:
  Md5Function() : Item_str_func() {}
  String *val_str(String*);

  void fix_length_and_dec() 
  {
    max_length= 32;
    args[0]->collation.set(
      get_charset_by_csname(args[0]->collation.collation->csname,
                            MY_CS_BINSORT), DERIVATION_COERCIBLE);
  }

  const char *func_name() const 
  { 
    return "md5"; 
  }

  bool check_argument_count(int n) 
  { 
    return (n == 1); 
  }
};


String *Md5Function::val_str(String *str)
{
  assert(fixed == true);

  String *sptr= args[0]->val_str(str);
  if (sptr == NULL) 
  {
    null_value= true;
    return 0;
  }
  str->alloc(32);

  null_value= false;

  str->set_charset(&my_charset_bin);

  gcry_md_hd_t md5_context;
  gcry_md_open(&md5_context, GCRY_MD_MD5, 0);
  gcry_md_write(md5_context, sptr->ptr(), sptr->length());  
  unsigned char *digest= gcry_md_read(md5_context, 0);

  snprintf((char *) str->ptr(), 33,
    "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    digest[0], digest[1], digest[2], digest[3],
    digest[4], digest[5], digest[6], digest[7],
    digest[8], digest[9], digest[10], digest[11],
    digest[12], digest[13], digest[14], digest[15]);
  str->length((uint32_t) 32);

  gcry_md_close(md5_context);

  return str;
}


plugin::Create_function<Md5Function> *md5udf= NULL;

static int initialize(module::Context &context)
{
  /* Initialize libgcrypt */
  if (not gcry_check_version(GCRYPT_VERSION))
  {
    errmsg_printf(error::ERROR, _("libgcrypt library version mismatch"));
    return 1;
  }
  /* Disable secure memory.  */
  gcry_control (GCRYCTL_DISABLE_SECMEM, 0);

  /* Tell Libgcrypt that initialization has completed. */
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);

  md5udf= new plugin::Create_function<Md5Function>("md5");
  context.add(md5udf);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "md5",
  "1.0",
  "Stewart Smith",
  "UDF for computing md5sum",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
