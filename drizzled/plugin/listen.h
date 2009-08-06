/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_PLUGIN_LISTEN_H
#define DRIZZLED_PLUGIN_LISTEN_H

#include <netinet/in.h>

namespace drizzled
{
namespace plugin
{

class Client;

/**
 * This class is used by new listen/protocol modules to provide the TCP port to
 * listen on, as well as a protocol factory when new connections are accepted.
 */
class Listen
{
public:
  Listen() {}
  virtual ~Listen() {}

  /**
   * This returns the port drizzled will bind to for accepting new connections.
   */
  virtual in_port_t getPort(void) const= 0;

  /**
   * This provides a new Client object that can be used by a Session.
   */
  virtual drizzled::plugin::Client *clientFactory(void) const= 0;
};

} /* end namespace drizzled::plugin */
} /* end namespace drizzled */

#endif /* DRIZZLED_PLUGIN_LISTEN_H */
