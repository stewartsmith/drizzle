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

#include <boost/foreach.hpp>
#include <drizzled/plugin/plugin.h>
#include <drizzled/atomics.h>
#include <vector>
#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

typedef std::vector<Listen *> ListenVector;
typedef std::pair<std::string*, drizzled::atomic<uint64_t>*> ListenCounter;
/**
 * This class is used by client plugins to provide and manage the listening
 * interface for new client instances.
 */
class DRIZZLED_API Listen : public Plugin
{
protected:
  std::vector<ListenCounter*> counters;
public:
  explicit Listen(std::string name_arg)
    : Plugin(name_arg, "Listen")
  {}

  virtual ~Listen()
  {
    BOOST_FOREACH(ListenCounter* counter, counters)
    {
      delete counter->first;
      delete counter;
    }
  }

  static ListenVector &getListenProtocols();

  std::vector<ListenCounter*>& getListenCounters()
  {
    return counters;
  }
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
  virtual plugin::Client *getClient(int fd)= 0;

  /**
   * Add a new Listen object to the list of listeners we manage.
   */
  static bool addPlugin(Listen*);

  /**
   * Remove a Listen object from the list of listeners we manage.
   */
  static void removePlugin(Listen*);

  /**
   * Setup all configured listen plugins.
   */
  static bool setup(void);

  /**
   * Accept a new connection (Client object) on one of the configured
   * listener interfaces.
   */
  static plugin::Client *getClient();

  /**
   * Some internal functions drizzled require a temporary Client object to
   * create a valid session object, this just returns a dummy client object.
   */
  static plugin::Client *getNullClient();

  /**
   * Shutdown and cleanup listen loop for server shutdown.
   */
  static void shutdown();

};

} /* namespace plugin */

} /* namespace drizzled */

