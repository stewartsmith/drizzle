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

my_bool udf_init_hello_world(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  /* this is how to fail */
  if (args->arg_count != 0)  {
    strncpy(message, "Too many arguments", MYSQL_ERRMSG_SIZE);
    return 1;
  }

  /* initid->ptr keeps state for between udf_init_foo and udf_deinit_foo */
  initid->ptr= NULL;

  return 0;
}

char *udf_doit_hello_world(UDF_INIT *initid, UDF_ARGS *args, char *result,
			   unsigned long *length, char *is_null, char *error)
{
  /* We don't use these, so void them out */
  (void)initid;
  (void)args;

  /* "result" is preallocated 255 bytes for me, if i want to use it */
  strncpy(result, "Hello, world!", 255);

  /* if set to 255 or less, MySQL treats the result as a varchar
     if set to greater than 255, MySQL treats the result as a blob */
  *length= strlen("Hello, world!");

  /* is_null is already zero, this is a demonstration */
  *is_null= 0;

  /* error is already zero, this is a demonstration */
  *error= 0;

  return result;
}

void udf_deinit_hello_world(UDF_INIT *initid)
{
  /* We don't use this, so void it out */
  (void)initid;

  /* if we allocated initid->ptr, free it here */
  return;
}


static int hello_world_plugin_init(void *p)
{
  udf_func *udff= (udf_func *) p;

  udff->name.str= (char *)"hello_world";
  udff->name.length= strlen("hello_world");
  udff->type= UDFTYPE_FUNCTION;
  udff->returns= STRING_RESULT;
  udff->func_init= udf_init_hello_world;
  udff->func_deinit= udf_deinit_hello_world;
  udff->func= (Udf_func_any) udf_doit_hello_world;

  return 0;
}

static int hello_world_plugin_deinit(void *p)
{
  /* We don't use this, so void it out */
  (void)p;

  /* There is nothing to de-init here, but if 
   * something is needed from the udf_func, it 
   * can be had from p with:
   *   udf_func *udff = (udf_func *) p;
   */

  return 0;
}

struct st_mysql_udf hello_world_plugin=
{ MYSQL_UDF_INTERFACE_VERSION  };

mysql_declare_plugin(hello_world)
{
  MYSQL_UDF_PLUGIN,
  &hello_world_plugin,
  "hello_world",
  "Mark Atwood",
  "Hello, world!",
  PLUGIN_LICENSE_GPL,
  hello_world_plugin_init, /* Plugin Init */
  hello_world_plugin_deinit, /* Plugin Deinit */
  0x0100,  /* 1.0 */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
