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

#ifndef DRIZZLED_SLOT_LISTEN_H
#define DRIZZLED_SLOT_LISTEN_H

#include <vector>

struct pollfd;

namespace drizzled
{

namespace plugin
{
class Client;
class Listen;
}

namespace slot
{

/**
 * Class to handle all Listen plugin objects.
 */
class Listen
{
private:
  std::vector<plugin::Listen *> listen_list;
  std::vector<plugin::Listen *> listen_fd_list;
  struct pollfd *fd_list;
  uint32_t fd_count;
  int wakeup_pipe[2];

public:
  Listen();
  ~Listen();

  /**
   * Add a new Listen object to the list of listeners we manage.
   */
  void add(plugin::Listen *listen_obj);

  /**
   * Remove a Listen object from the list of listeners we manage.
   */
  void remove(plugin::Listen *listen_obj);

  /**
   * Setup all configured listen plugins.
   */
  bool setup(void);

  /**
   * Accept a new connection (Client object) on one of the configured
   * listener interfaces.
   */
  plugin::Client *getClient(void) const;

  /**
   * Some internal functions drizzled require a temporary Client object to
   * create a valid session object, this just returns a dummy client object.
   */
  plugin::Client *getNullClient(void) const;

  /**
   * Shutdown and cleanup listen loop for server shutdown.
   */
  void shutdown(void);
};

} /* end namespace slot */
} /* end namespace drizzled */

#endif /* DRIZZLED_SLOT_LISTEN_H */
