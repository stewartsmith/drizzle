/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* This implements 'user defined functions' */

/*
   Known bugs:
  
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <my_pthread.h>

extern "C"
{
#include <stdarg.h>
#include <hash.h>
}

static bool udf_startup= false; /* We do not lock because startup is single threaded */
static MEM_ROOT mem;
static HASH udf_hash;

extern "C" uchar* get_hash_key(const uchar *buff, size_t *length,
			      my_bool not_used __attribute__((unused)))
{
  udf_func *udf= (udf_func*) buff;
  *length= (uint) udf->name.length;
  return (uchar*) udf->name.str;
}


void udf_init()
{
  init_sql_alloc(&mem, UDF_ALLOC_BLOCK_SIZE, 0);

  if (hash_init(&udf_hash, system_charset_info, 32, 0, 0, get_hash_key, NULL, 0))
  {
    sql_print_error("Can't allocate memory for udf structures");
    hash_free(&udf_hash);
    free_root(&mem, MYF(0));
    return;
  }
}

/* called by mysqld.cc clean_up() */
void udf_free()
{
  hash_free(&udf_hash);
  free_root(&mem, MYF(0));
}

/* This is only called if using_udf_functions != 0 */
udf_func *find_udf(const char *name, uint length)
{
  udf_func *udf;

  if (udf_startup == false)
    return NULL;

  udf= (udf_func*) hash_search(&udf_hash,
                               (uchar*) name,
                               length ? length : (uint) strlen(name));

  return (udf);
}

static bool add_udf(udf_func *udf)
{
  if (my_hash_insert(&udf_hash, (uchar*) udf))
    return false;

  using_udf_functions= 1;

  return true;
}

int initialize_udf(st_plugin_int *plugin)
{
  udf_func *udff;

  if (udf_startup == false)
  {
    udf_init();
    udf_startup= true;
  }
	  
  /* allocate the udf_func structure */
  udff = (udf_func *)my_malloc(sizeof(udf_func), MYF(MY_WME | MY_ZEROFILL));
  if (udff == NULL) return 1;

  plugin->data= (void *)udff;

  if (plugin->plugin->init)
  {
    /* todo, if the plugin doesnt have an init, bail out */

    if (plugin->plugin->init((void *)udff))
    {
      sql_print_error("Plugin '%s' init function returned error.",
                      plugin->name.str);
      goto err;
    }
  }
  
  add_udf(udff);

  return 0;
err:
  my_free(udff, MYF(0));
  return 1;
}

int finalize_udf(st_plugin_int *plugin)
{ 
  udf_func *udff = (udf_func *)plugin->data;

  /* TODO: Issue warning on failure */
  if (udff && plugin->plugin->deinit)
    (void)plugin->plugin->deinit(udff);

  if (udff)
    my_free(udff, MYF(0));

  return 0;
}

