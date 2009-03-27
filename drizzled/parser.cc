/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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
#include <drizzled/parser.h>
#include <drizzled/gettext.h>

int parser_initializer(st_plugin_int *plugin)
{
  parser_t *p;

  p= new parser_t;

  if (p == NULL) 
    return 1;
  memset(p, 0, sizeof(parser_t));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init((void *)p))
    {
      /* TRANSLATORS: The leading word "parser" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR, _("parser plugin '%s' init() failed"),
		      plugin->name.str);
      goto err;
    }
  }

  return 0;

err:
  delete p;

  return 1;
}

int parser_finalizer(st_plugin_int *plugin)
{
  parser_t *p= (parser_t *) plugin->data;

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      /* TRANSLATORS: The leading word "parser" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR, _("parser plugin '%s' deinit() failed"),
		      plugin->name.str);
    }
  }

  if (p) 
    delete p;

  return 0;
}

/* The plugin_foreach() iterator requires that we
   convert all the parameters of a plugin api entry point
   into just one single void ptr, plus the session.
   So we will take all the additional paramters of parser_do1,
   and marshall them into a struct of this type, and
   then just pass in a pointer to it.
*/
typedef struct parser_do1_parms_st
{
  void *parm1;
  void *parm2;
} parser_do1_parms_t;

/* This gets called by plugin_foreach once for each loaded parser plugin */
static bool parser_do1_iterate (Session *session, plugin_ref plugin, void *p)
{
  parser_t *l= plugin_data(plugin, parser_t *);
  parser_do1_parms_t *parms= (parser_do1_parms_t *) p;

  /* call this loaded parser plugin's parser_func1 function pointer */
  if (l && l->parser_func1)
  {
    if (l->parser_func1(session, parms->parm1, parms->parm2))
    {
      /* TRANSLATORS: The leading word "parser" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR, _("parser plugin '%s' parser_func1() failed"),
		      (char *)plugin_name(plugin));
      return true;
    }
  }
  return false;
}

/* This is the parser_do1 entry point.
   This gets called by the rest of the Drizzle server code */
bool parser_do1 (Session *session, void *parm1, void *parm2)
{
  parser_do1_parms_t parms;
  bool foreach_rv;

  /* marshall the parameters so they will fit into the foreach */
  parms.parm1= parm1;
  parms.parm2= parm2;

  /* call parser_do1_iterate
     once for each loaded parser plugin */
  foreach_rv= plugin_foreach(session,
			     parser_do1_iterate,
			     DRIZZLE_PARSER_PLUGIN,
			     (void *) &parms);
  return foreach_rv;
}

/* The plugin_foreach() iterator requires that we
   convert all the parameters of a plugin api entry point
   into just one single void ptr, plus the session.
   So we will take all the additional paramters of parser_do2,
   and marshall them into a struct of this type, and
   then just pass in a pointer to it.
*/
typedef struct parser_do2_parms_st
{
  void *parm3;
  void *parm4;
} parser_do2_parms_t;

/* This gets called by plugin_foreach once for each loaded parser plugin */
static bool parser_do2_iterate (Session *session, plugin_ref plugin, void *p)
{
  parser_t *l= plugin_data(plugin, parser_t *);
  parser_do2_parms_t *parms= (parser_do2_parms_t *) p;

  /* call this loaded parser plugin's parser_func1 function pointer */
  if (l && l->parser_func2)
  {
    if (l->parser_func2(session, parms->parm3, parms->parm4))
    {
      /* TRANSLATORS: The leading word "parser" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR, _("parser plugin '%s' parser_func2() failed"),
		      (char *)plugin_name(plugin));

      return true;
    }
  }
  return false;
}

/* This is the parser_do2 entry point.
   This gets called by the rest of the Drizzle server code */
bool parser_do2 (Session *session, void *parm3, void *parm4)
{
  parser_do2_parms_t parms;
  bool foreach_rv;

  /* marshall the parameters so they will fit into the foreach */
  parms.parm3= parm3;
  parms.parm4= parm4;

  /* call parser_do2_iterate
     once for each loaded parser plugin */
  foreach_rv= plugin_foreach(session,
			     parser_do2_iterate,
			     DRIZZLE_PARSER_PLUGIN,
			     (void *) &parms);
  return foreach_rv;
}
