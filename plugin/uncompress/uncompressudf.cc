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
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/function/str/strfunc.h>

#include <zlib.h>

class Item_func_uncompress: public Item_str_func
{
  String buffer;
public:
  Item_func_uncompress(): Item_str_func(){}
  void fix_length_and_dec(){ maybe_null= 1; max_length= MAX_BLOB_WIDTH; }
  const char *func_name() const{return "uncompress";}
  String *val_str(String *) ;
};

String *Item_func_uncompress::val_str(String *str)
{
  assert(fixed == 1);
  String *res= args[0]->val_str(str);
  ulong new_size;
  int err;
  uint32_t code;

  if (!res)
    goto err;
  null_value= 0;
  if (res->is_empty())
    return res;

  /* If length is less than 4 bytes, data is corrupt */
  if (res->length() <= 4)
  {
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_ZLIB_Z_DATA_ERROR,
                        ER(ER_ZLIB_Z_DATA_ERROR));
    goto err;
  }

  /* Size of uncompressed data is stored as first 4 bytes of field */
  new_size= uint4korr(res->ptr()) & 0x3FFFFFFF;
  if (new_size > current_session->variables.max_allowed_packet)
  {
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_TOO_BIG_FOR_UNCOMPRESS,
                        ER(ER_TOO_BIG_FOR_UNCOMPRESS),
                        current_session->variables.max_allowed_packet);
    goto err;
  }
  if (buffer.realloc((uint32_t)new_size))
    goto err;

  if ((err= uncompress((Byte*)buffer.ptr(), &new_size,
                       ((const Bytef*)res->ptr())+4,res->length())) == Z_OK)
  {
    buffer.length((uint32_t) new_size);
    return &buffer;
  }

  code= ((err == Z_BUF_ERROR) ? ER_ZLIB_Z_BUF_ERROR :
	 ((err == Z_MEM_ERROR) ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_DATA_ERROR));
  push_warning(current_session, DRIZZLE_ERROR::WARN_LEVEL_ERROR, code, ER(code));

err:
  null_value= 1;
  return 0;
}


Item_func* create_uncompressudf_item(MEM_ROOT* m)
{
  return  new (m) Item_func_uncompress();
}

static struct udf_func uncompressudf = {
  { C_STRING_WITH_LEN("uncompress") },
  create_uncompressudf_item
};

static int uncompressudf_plugin_init(void *p)
{
  udf_func **f = (udf_func**) p;

  *f= &uncompressudf;

  return 0;
}

static int uncompressudf_plugin_deinit(void *p)
{
  udf_func *udff = (udf_func *) p;
  (void)udff;
  return 0;
}

drizzle_declare_plugin(uncompress)
{
  DRIZZLE_UDF_PLUGIN,
  "uncompress",
  "1.0",
  "Stewart Smith",
  "UDF for compress()",
  PLUGIN_LICENSE_GPL,
  uncompressudf_plugin_init, /* Plugin Init */
  uncompressudf_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
