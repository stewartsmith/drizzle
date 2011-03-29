/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

#pragma once

#include <deque>
#include <drizzled/named_savepoint.h>
#include <drizzled/xid.h>

namespace drizzled {
namespace session {

/**
 * Structure used to manage "statement transactions" and
 * "normal transactions". In autocommit mode, the normal transaction is
 * equivalent to the statement transaction.
 *
 * Storage engines will be registered here when they participate in
 * a transaction. No engine is registered more than once.
 */
class Transactions 
{
public:
  std::deque<NamedSavepoint> savepoints;

  /**
   * The normal transaction (since BEGIN WORK).
   *
   * Contains a list of all engines that have participated in any of the
   * statement transactions started within the context of the normal
   * transaction.
   *
   * @note In autocommit mode, this is empty.
   */
  TransactionContext all;

  /**
   * The statment transaction.
   *
   * Contains a list of all engines participating in the given statement.
   *
   * @note In autocommit mode, this will be used to commit/rollback the
   * normal transaction.
   */
  TransactionContext stmt;

  XID_STATE xid_state;

  void cleanup()
  {
    savepoints.clear();
  }
};

} /* namespace session */
} /* namespace drizzled */

