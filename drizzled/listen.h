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

#ifndef DRIZZLED_LISTEN_H
#define DRIZZLED_LISTEN_H

#include <drizzled/plugin/listen.h>
#include <drizzled/plugin/protocol.h>

/**
 * Class to handle all Listen plugin objects.
 */
class ListenHandler
{
private:
  std::vector<Listen *> listen_list;
  std::vector<Listen *> listen_fd_list;
  struct pollfd *fd_list;
  uint32_t fd_count;
  int wakeup_pipe[2];
  uint32_t error_count;

public:
  ListenHandler();
  ~ListenHandler();

  /**
   * Add a new Listen object to the list of listeners we manage.
   */
  void addListen(Listen *listen_obj);

  /**
   * Remove a Listen object from the list of listeners we manage.
   */
  void removeListen(Listen *listen_obj);

  /**
   * Bind to all configured listener interfaces.
   */
  bool bindAll(const char *host, uint32_t bind_timeout);

  /**
   * Accept a new connection (Protocol object) on one of the configured
   * listener interfaces.
   */
  Protocol *getProtocol(void);

  /**
   * Wakeup the listen loop from another thread.
   */
  void wakeup(void);
};

void add_listen(Listen *listen_obj);
void remove_listen(Listen *listen_obj);
void listen_abort(void);

#endif /* DRIZZLED_LISTEN_H */
