/*
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

 *  Copyright (C) 2008 Mark Atwood
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
#include <drizzled/errmsg.h>

typedef struct errmsg_parms_st
{
  int priority;
  const char *format;
  va_list ap;
} errmsg_parms_t;

int errmsg_initializer(st_plugin_int *plugin)
{
  errmsg_t *p;

  fprintf(stderr, "MRA %s plugin:%s dl:%s\n",
	  __func__, plugin->name.str, plugin->plugin_dl->dl.str);

  p= (errmsg_t *) malloc(sizeof(errmsg_t));
  if (p == NULL) return 1;
  memset(p, 0, sizeof(errmsg_t));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init((void *)p))
    {
      sql_print_error("Errmsg plugin '%s' init function returned error.",
                      plugin->name.str);
      goto err;
    }
  }
  return 0;

err:
  free(p);
  return 1;
}

int errmsg_finalizer(st_plugin_int *plugin)
{ 
  errmsg_t *p = (errmsg_t *) plugin->data;

  fprintf(stderr, "MRA %s plugin:%s dl:%s\n",
	  __func__, plugin->name.str, plugin->plugin_dl->dl.str);

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      sql_print_error("Errmsg plugin '%s' deinit function returned error.",
                      plugin->name.str);
    }
  }

  if (p) free(p);

  return 0;
}

static bool errmsg_iterate (THD *thd, plugin_ref plugin, void *p)
{
  errmsg_t *l= plugin_data(plugin, errmsg_t *);
  errmsg_parms_t *parms= (errmsg_parms_t *) p;

  if (l && l->errmsg_func)
  {
    if (l->errmsg_func(thd, parms->priority, parms->format, parms->ap))
      return true;
  }
  return false;
}

void errmsg_vprintf (THD *thd, int priority, const char *format, va_list ap)
{
  errmsg_parms_t parms;

  parms.priority= priority;
  parms.format= format;
  parms.ap= ap;

  if (plugin_foreach(thd, errmsg_iterate, DRIZZLE_ERRMSG_PLUGIN,
		     (void *) &parms))
  {
    sql_print_error("Errmsg plugin had an error.");
  }
  return;
}

void errmsg_printf (THD *thd, int priority, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  errmsg_vprintf(thd, priority, format, args);
  va_end(args);
  return;
}
