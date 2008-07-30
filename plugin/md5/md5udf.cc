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

#include <mysql_priv.h>
#include <stdlib.h>
#include <ctype.h>
#include <drizzled/plugin.h>
#include <my_global.h>
#include <openssl/md5.h>

bool udf_init_md5udf(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  /* initid->ptr keeps state for between udf_init_foo and udf_deinit_foo */
  initid->ptr= NULL;

  if (args->arg_count != 1)
   {
      strcpy(message,"MD5() requires one arguments");
      return 1;
   }

   if (args->arg_type[0] != STRING_RESULT)
   {
      strcpy(message,"MD5() requires a string");
      return 1;
   }

  return 0;
}

char *udf_doit_md5(UDF_INIT *initid, UDF_ARGS *args, char *result,
			   unsigned long *length, char *is_null, char *error)
{
  MD5_CTX context;
  uchar digest[16];

  (void)initid;

  MD5_Init(&context);

  MD5_Update(&context, args->args[0], args->lengths[0]);

  MD5_Final(digest, &context);

  sprintf(result,
          "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
          digest[0], digest[1], digest[2], digest[3],
          digest[4], digest[5], digest[6], digest[7],
          digest[8], digest[9], digest[10], digest[11],
          digest[12], digest[13], digest[14], digest[15]);

  *length= 32;

  /* is_null is already zero, this is a demonstration */
  *is_null= 0;

  /* error is already zero, this is a demonstration */
  *error= 0;

  return result;
}

void udf_deinit_md5udf(UDF_INIT *initid)
{
  (void)initid;
  /* if we allocated initid->ptr, free it here */
  return;
}


static int md5udf_plugin_init(void *p)
{
  udf_func *udff= (udf_func *) p;
  static char md5str[4];

  strcpy(md5str, "md5");

  udff->name.str= md5str;
  udff->name.length= strlen("md5");
  udff->type= UDFTYPE_FUNCTION;
  udff->returns= STRING_RESULT;
  udff->func_init= udf_init_md5udf;
  udff->func_deinit= udf_deinit_md5udf;
  udff->func= (Udf_func_any) udf_doit_md5;

  return 0;
}

static int md5udf_plugin_deinit(void *p)
{
  udf_func *udff = (udf_func *) p;
  (void)udff;
  return 0;
}

mysql_declare_plugin(md5)
{
  MYSQL_UDF_PLUGIN,
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
mysql_declare_plugin_end;
