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


#ifndef PLUGIN_DRIZZLE_PROTOCOL_DRIZZLE_PROTOCOL_H
#define PLUGIN_DRIZZLE_PROTOCOL_DRIZZLE_PROTOCOL_H

#include <drizzled/plugin/listen_tcp.h>
#include <drizzled/plugin/client.h>
#include <drizzled/atomics.h>
#include "drizzled/plugin/table_function.h"

#include "plugin/mysql_protocol/mysql_protocol.h"

namespace drizzle_plugin
{
namespace drizzle_protocol
{

class ListenDrizzleProtocol: public ListenMySQLProtocol
{
public:
  ListenDrizzleProtocol(std::string name, 
                        const std::string &bind_address,
                        bool using_mysql41_protocol):
    ListenMySQLProtocol(name, bind_address, using_mysql41_protocol)
  { }

  ~ListenDrizzleProtocol();
  in_port_t getPort(void) const;
  static ProtocolCounters *drizzle_counters;
  virtual ProtocolCounters *getCounters(void) const { return drizzle_counters; }
};


} /* namespace drizzle_protocol */
} /* namespace drizzle_plugin */
 
#endif /* PLUGIN_DRIZZLE_PROTOCOL_DRIZZLE_PROTOCOL_H */
