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
#include <drizzled/errmsg.h>
#include <drizzled/gettext.h>

static bool errmsg_has= false;

int errmsg_initializer(st_plugin_int *plugin)
{
  Error_message_handler *p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init(&p))
    {
      /* we're doing the errmsg plugin api,
        so we can't trust the errmsg api to emit our error messages
        so we will emit error messages to stderr */
      /* TRANSLATORS: The leading word "errmsg" is the name
        of the plugin api, and so should not be translated. */
      fprintf(stderr,
              _("errmsg plugin '%s' init() failed."),
              plugin->name.str);
      return 1;
    }
  }

  plugin->data= (void *)p;
  errmsg_has= true;

  return 0;

}

int errmsg_finalizer(st_plugin_int *plugin)
{
  Error_message_handler *p= static_cast<Error_message_handler *>(plugin->data);

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit(p))
    {
      /* we're doing the errmsg plugin api,
	 so we can't trust the errmsg api to emit our error messages
	 so we will emit error messages to stderr */
      /* TRANSLATORS: The leading word "errmsg" is the name
         of the plugin api, and so should not be translated. */
      fprintf(stderr,
              _("errmsg plugin '%s' deinit() failed."),
              plugin->name.str);
    }
  }

  return 0;
}

/* The plugin_foreach() iterator requires that we
   convert all the parameters of a plugin api entry point
   into just one single void ptr, plus the session.
   So we will take all the additional paramters of errmsg_vprintf,
   and marshall them into a struct of this type, and
   then just pass in a pointer to it.
*/
typedef struct errmsg_parms_st
{
  int priority;
  const char *format;
  va_list ap;
} errmsg_parms_t;


/* This gets called by plugin_foreach once for each loaded errmsg plugin */
static bool errmsg_iterate (Session *session, plugin_ref plugin, void *p)
{
  Error_message_handler *handler= plugin_data(plugin, Error_message_handler *);
  errmsg_parms_t *parms= (errmsg_parms_t *) p;

  if (handler)
  {
    if (handler->errmsg(session, parms->priority, parms->format, parms->ap))
    {
      /* we're doing the errmsg plugin api,
	 so we can't trust the errmsg api to emit our error messages
	 so we will emit error messages to stderr */
      /* TRANSLATORS: The leading word "errmsg" is the name
         of the plugin api, and so should not be translated. */
      fprintf(stderr,
              _("errmsg plugin '%s' errmsg() failed"),
              (char *)plugin_name(plugin));
      return true;
    }
  }
  return false;
}

bool errmsg_vprintf (Session *session, int priority,
                     char const *format, va_list ap)
{
  bool foreach_rv;
  errmsg_parms_t parms;

  /* check to see if any errmsg plugin has been loaded
     if not, just fall back to emitting the message to stderr */
  if (!errmsg_has)
  {
    /* if it turns out that the vfprintf doesnt do one single write
       (single writes are atomic), then this needs to be rewritten to
       vsprintf into a char buffer, and then write() that char buffer
       to stderr */
    vfprintf(stderr, format, ap);
    return false;
  }

  /* marshall the parameters so they will fit into the foreach */
  parms.priority= priority;
  parms.format= format;
  va_copy(parms.ap, ap);

  /* call errmsg_iterate
     once for each loaded errmsg plugin */
  foreach_rv= plugin_foreach(session, errmsg_iterate,
                             DRIZZLE_ERRMSG_PLUGIN, (void *) &parms);
  return foreach_rv;
}


