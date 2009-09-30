/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
 *
 *  Authors:
 *
 *  Jay Pipes <joinfu@sun.com>
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

/**
 * @file
 *
 * Defines the API of a simple reader of Command messages from the
 * Command log file.  
 *
 * @details
 *
 * This class is used by other plugins, for instance
 * the async_replication module, in order to read the command log and
 * return Command messages.
 */

#ifndef PLUGIN_COMMAND_LOG_COMMAND_LOG_READER_H
#define PLUGIN_COMMAND_LOG_COMMAND_LOG_READER_H

#include "command_log.h"

#include <drizzled/server_includes.h>
#include <drizzled/plugin/command_reader.h>

/**
 * A class which reads Command messages from the Command log file
 */
class CommandLogReader :public drizzled::plugin::CommandReader
{
private:
  /** The Command log object this reader uses */
  const CommandLog &log;
public:
  CommandLogReader(std::string name_arg, const CommandLog &in_log)
    : drizzled::plugin::CommandReader(name_arg), log(in_log)
  {}

  /** Destructor */
  ~CommandLogReader() {}
  /**
   * Read and fill a Command message with the supplied
   * Command message global transaction ID.
   *
   * @param[in] Global transaction ID to find
   * @param[out] Pointer to a command message to fill
   *
   * @retval
   *  true if Command message was read successfully and the supplied pointer to message was filled
   * @retval
   *  false if not found or read successfully
   */
  bool read(const drizzled::ReplicationServices::GlobalTransactionId &to_read_trx_id, 
            drizzled::message::Command *to_fill);
};

#endif /* PLUGIN_COMMAND_LOG_COMMAND_LOG_READER_H */
