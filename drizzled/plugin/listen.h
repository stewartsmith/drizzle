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

#include "drizzled/plugin/plugin.h"

#include <vector>

namespace drizzled
{
namespace plugin
{

class Client;

/**
 * This class is used by client plugins to provide and manage the listening
 * interface for new client instances.
 */
class Listen : public Plugin
{
  Listen();
  Listen(const Listen&);
public:
  explicit Listen(std::string name_arg) : Plugin(name_arg) {}
  virtual ~Listen() {}

  /**
   * This provides a list of file descriptors to watch that will trigger new
   * Client instances. When activity is detected on one of the returned file
   * descriptors, getClient will be called with the file descriptor.
   * @fds[out] Vector of file descriptors to watch for activity.
   * @retval true on failure, false on success.
   */
  virtual bool getFileDescriptors(std::vector<int> &fds)= 0;

  /**
   * This provides a new Client object that can be used by a Session.
   * @param[in] fd File descriptor that had activity.
   */
  virtual drizzled::plugin::Client *getClient(int fd)= 0;
};

} /* end namespace drizzled::plugin */
} /* end namespace drizzled */

#endif /* DRIZZLED_PLUGIN_LISTEN_H */
