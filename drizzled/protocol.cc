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
#include <drizzled/protocol.h>
#include "drizzled/plugin_registry.h"
#include <drizzled/gettext.h>


extern char *opt_protocol;

ProtocolFactory *protocol_factory= NULL;

Protocol *get_protocol()
{
  assert(protocol_factory != NULL);
  return (*protocol_factory)();
}

bool add_protocol_factory(ProtocolFactory *factory)
{
  if (factory->getName() != opt_protocol)
    return true;

  if (protocol_factory != NULL)
  {
    fprintf(stderr, "You cannot load more then one protocol plugin\n");
    exit(1);
  }
  protocol_factory= factory;

  return false;
}

int protocol_initializer(st_plugin_int *plugin)
{

  ProtocolFactory *factory= NULL;

  assert(plugin->plugin->init); /* Find poorly designed plugins */

  if (plugin->plugin->init((void *)&factory))
  {
    /* 
      TRANSLATORS> The leading word "protocol" is the name
      of the plugin api, and so should not be translated. 
    */
    errmsg_printf(ERRMSG_LVL_ERROR, _("protocol plugin '%s' init() failed"),
	                plugin->name.str);
      return 1;
  }

  Plugin_registry &registry= Plugin_registry::get_plugin_registry();
  if (factory != NULL)
    registry.registerPlugin(factory);
  
  plugin->data= factory;

  return 0;
}

int protocol_finalizer(st_plugin_int *plugin)
{
  /* We know which one we initialized since its data pointer is filled */
  if (plugin->plugin->deinit && plugin->data)
  {
    if (plugin->plugin->deinit((void *)plugin->data))
    {
      /* TRANSLATORS: The leading word "protocol" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("protocol plugin '%s' deinit() failed"),
                    plugin->name.str);
    }
  }

  return 0;
}
