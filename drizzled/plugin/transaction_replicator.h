/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Jay Pipes
 *
 *  Authors:
 *
 *    Jay Pipes <jaypipes@gmail.com>
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

#include <drizzled/plugin/replication.h>
#include <drizzled/plugin/plugin.h>

#include <drizzled/visibility.h>

/**
 * @file Defines the API for a TransactionReplicator.  
 *
 * All a replicator does is replicate/reproduce
 * events, optionally transforming them before sending them off to a TransactionApplier.
 *
 * An applier is responsible for applying events, not a replicator...
 */

namespace drizzled {
namespace plugin {

/**
 * Class which replicates Transaction messages
 */
class DRIZZLED_API TransactionReplicator : public Plugin
{
  TransactionReplicator();
  TransactionReplicator(const TransactionReplicator &);
  TransactionReplicator& operator=(const TransactionReplicator &);
public:
  explicit TransactionReplicator(std::string name_arg)
    : Plugin(name_arg, "TransactionReplicator")
  {
  }
  virtual ~TransactionReplicator() {}

  /**
   * Replicate a Transaction message to a TransactionApplier.
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
   * @param Pointer to the applier of the command message
   * @param Transaction message to be replicated
   */
  virtual ReplicationReturnCode replicate(TransactionApplier *in_applier, 
                                          Session &session,
                                          message::Transaction &to_replicate)= 0;
  static bool addPlugin(TransactionReplicator *replicator);
  static void removePlugin(TransactionReplicator *replicator);
};

} /* namespace plugin */
} /* namespace drizzled */

