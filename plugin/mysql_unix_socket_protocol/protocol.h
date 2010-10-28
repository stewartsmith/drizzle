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

#include "plugin/mysql_protocol/mysql_protocol.h"

namespace mysql_unix_socket_protocol
{

class Protocol: public ListenMySQLProtocol
{
public:
  Protocol(std::string name_arg, bool using_mysql41_protocol_arg):
    ListenMySQLProtocol(name_arg, using_mysql41_protocol_arg)
  { }

  ~Protocol();
  bool getFileDescriptors(std::vector<int> &fds);

  const char* getHost(void) const;
  in_port_t getPort(void) const;
};


} /* namespace mysql_unix_socket_protocol */

#endif /* PLUGIN_MYSQL_UNIX_SOCKET_PROTOCOL_PROTOCOL_H */
