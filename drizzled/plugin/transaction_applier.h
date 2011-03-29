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

/**
 * @file Defines the API for a TransactionApplier
 *
 * A TransactionApplier applies an event it has received from a TransactionReplicator (via 
 * a replicator's replicate() call, or it has read using a TransactionReader's read()
 * call.
 */

#include <drizzled/plugin/plugin.h>
#include <drizzled/plugin/replication.h>

#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

/**
 * Base class for appliers of Transaction messages
 */
class DRIZZLED_API TransactionApplier : public Plugin
{
  TransactionApplier();
  TransactionApplier(const TransactionApplier &);
  TransactionApplier& operator=(const TransactionApplier &);
public:
  explicit TransactionApplier(std::string name_arg)
    : Plugin(name_arg, "TransactionApplier")
  {
  }
  virtual ~TransactionApplier() {}
  /**
   * Apply something to a target.
   *
   * @note
   *
   * It is important to note that memory allocation for the 
   * supplied pointer is not guaranteed after the completion 
   * of this function -- meaning the caller can dispose of the
   * supplied message.  Therefore, appliers which are
   * implementing an asynchronous replication system must copy
   * the supplied message to their own controlled memory storage
   * area.
   *
   * @param Transaction message to be replicated
   */
  virtual ReplicationReturnCode apply(Session &in_session,
                                      const message::Transaction &to_apply)= 0;

  static bool addPlugin(TransactionApplier *applier);
  static void removePlugin(TransactionApplier *applier);
};

} /* namespace plugin */
} /* namespace drizzled */

