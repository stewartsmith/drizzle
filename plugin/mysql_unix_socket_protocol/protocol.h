/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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


#ifndef PLUGIN_MYSQL_UNIX_SOCKET_PROTOCOL_PROTOCOL_H
#define PLUGIN_MYSQL_UNIX_SOCKET_PROTOCOL_PROTOCOL_H

#include <drizzled/plugin/listen_tcp.h>
#include <drizzled/plugin/client.h>
#include <drizzled/atomics.h>
#include "drizzled/plugin/table_function.h"

#include <boost/filesystem.hpp>

#include "plugin/mysql_protocol/mysql_protocol.h"

namespace drizzle_plugin
{
namespace mysql_unix_socket_protocol
{

class Protocol: public ListenMySQLProtocol
{
  const boost::filesystem::path _unix_socket_path;
public:
  Protocol(std::string name,
           bool using_mysql41_protocol,
           const boost::filesystem::path &unix_socket_path) :
    ListenMySQLProtocol(name, unix_socket_path.file_string(),
                        using_mysql41_protocol),
    _unix_socket_path(unix_socket_path)
  { }

  ~Protocol();
  bool getFileDescriptors(std::vector<int> &fds);

  in_port_t getPort(void) const;
  static ProtocolCounters *mysql_unix_counters;
  virtual ProtocolCounters *getCounters(void) const {return mysql_unix_counters; }
};


} /* namespace mysql_unix_socket_protocol */
} /* namespace drizzle_plugin */

#endif /* PLUGIN_MYSQL_UNIX_SOCKET_PROTOCOL_PROTOCOL_H */
