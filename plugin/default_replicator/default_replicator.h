/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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
 * Defines the API of the default serial event log.
 *
 * @see drizzled/plugin/replicator.h
 * @see drizzled/plugin/applier.h
 *
 * @details
 *
 * The SerialEventLog applies events it receives from the TransactionServices
 * server component to a simple log file on disk.
 * 
 * Events are received in no guaranteed order and the serial event log
 * is in charge of writing these events to the log as they are received.
 */

#ifndef DRIZZLE_PLUGIN_SERIAL_EVENT_LOG_H
#define DRIZZLE_PLUGIN_SERIAL_EVENT_LOG_H

#include <drizzled/server_includes.h>
#include <drizzled/atomics.h>
#include <drizzled/plugin/replicator.h>
#include <drizzled/plugin/applier.h>

#include <vector>
#include <string>

class DefaultReplicator: public drizzled::plugin::Replicator
{
public:
  DefaultReplicator() {}

  /** Destructor */
  ~DefaultReplicator() {}
  /**
   * Replicate a Command message to an Applier.
   *
   * @note
   *
   * It is important to note that memory allocation for the 
   * supplied pointer is not guaranteed after the completion 
   * of this function -- meaning the caller can dispose of the
   * supplied message.  Therefore, replicators and appliers 
   * implementing an asynchronous replication system must copy
   * the supplied message to their own controlled memory storage
   * area.
   *
   * @param Command message to be replicated
   */
  void replicate(drizzled::plugin::Applier *in_applier, drizzled::message::Command *to_replicate);
  
  /** 
   * Returns whether the default replicator is active.
   */
  bool isActive();
};

#endif /* DRIZZLE_PLUGIN_SERIAL_EVENT_LOG_H */
