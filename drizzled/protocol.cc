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

bool remove_protocol_factory(ProtocolFactory *)
{
  protocol_factory= NULL;
  return false;
}
