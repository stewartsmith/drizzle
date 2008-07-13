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
#include <drizzle_version.h>
#include <mysql/plugin.h>
#include <my_global.h>
#include <my_dir.h>
#include <zlib.h>

my_bool udf_init_crc32udf(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  /* initid->ptr keeps state for between udf_init_foo and udf_deinit_foo */
  initid->ptr= NULL;

  if (args->arg_count != 1)
   {
      strcpy(message,"CRC32() requires one arguments");
      return 1;
   }

   if (args->arg_type[0] != STRING_RESULT)
   {
      strcpy(message,"CRC32() requires a string");
      return 1;
   }

  return 0;
}

long long udf_doit_crc32(UDF_INIT *initid, UDF_ARGS *args, char *result,
                         unsigned long *length, char *is_null, char *error)
{
  return (longlong) crc32(0L, (uchar*)args->args[0], args->lengths[0]);
}

void udf_deinit_crc32udf(UDF_INIT *initid)
{
  /* if we allocated initid->ptr, free it here */
  return;
}


static int crc32udf_plugin_init(void *p)
{
  udf_func *udff= (udf_func *) p;

  udff->name.str= "crc32";
  udff->name.length= strlen("crc32");
  udff->type= UDFTYPE_FUNCTION;
  udff->returns= INT_RESULT;
  udff->func_init= udf_init_crc32udf;
  udff->func_deinit= udf_deinit_crc32udf;
  udff->func= (Udf_func_any) udf_doit_crc32;

  return 0;
}

static int crc32udf_plugin_deinit(void *p)
{
  udf_func *udff = (udf_func *) p;

  return 0;
}

struct st_mysql_udf crc32udf_plugin=
{ MYSQL_UDF_INTERFACE_VERSION  };

mysql_declare_plugin(crc32udf)
{
  MYSQL_UDF_PLUGIN,
  &crc32udf_plugin,
  "crc32",
  "Stewart Smith",
  "UDF for computing CRC32",
  PLUGIN_LICENSE_GPL,
  crc32udf_plugin_init, /* Plugin Init */
  crc32udf_plugin_deinit, /* Plugin Deinit */
  0x0100,  /* 1.0 */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
