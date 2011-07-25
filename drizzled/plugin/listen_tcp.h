/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/plugin/listen.h>
#include <netinet/in.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

/**
 * This class is used by client plugins to provide and manage TCP listening
 * interfaces for new client instances.
 */
class DRIZZLED_API ListenTcp : public Listen
{
protected:
  /** Count of errors encountered in acceptTcp. */
  uint32_t accept_error_count;

  /**
   * Accept new TCP connection. This is provided to be used in getClient for
   * derived class implementations.
   * @param[in] fd File descriptor that had activity.
   * @retval Newly accepted file descriptor.
   */
  int acceptTcp(int fd);

public:
  ListenTcp(std::string name_arg)
    : Listen(name_arg),
      accept_error_count(0)
  {}

  /**
   * This will bind the port to the host interfaces.
   * @fds[out] Vector of file descriptors that were bound.
   * @retval true on failure, false on success.
   */
  virtual bool getFileDescriptors(std::vector<int>&);

  /**
   * Get the host address to bind to.
   * @retval The host address.
   */
  virtual const std::string getHost() const;

  /**
   * Get the port to bind to.
   * @retval The port number.
   */
  virtual in_port_t getPort() const= 0;
};

} /* end namespace plugin */
} /* end namespace drizzled */

