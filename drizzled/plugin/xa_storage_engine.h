/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_PLUGIN_XA_STORAGE_ENGINE_H
#define DRIZZLED_PLUGIN_XA_STORAGE_ENGINE_H

#include "drizzled/plugin/transactional_storage_engine.h"

namespace drizzled
{

class XID;

namespace plugin
{

/**
 * A type of storage engine which supports distributed
 * transactions in the XA protocol.
 */
class XaStorageEngine :public TransactionalStorageEngine
{
public:
  XaStorageEngine(const std::string name_arg,
                  const std::bitset<HTON_BIT_SIZE> &flags_arg= HTON_NO_FLAGS);

  virtual ~XaStorageEngine();

  int prepare(Session *session, bool normal_transaction)
  {
    return doPrepare(session, normal_transaction);
  }

  int commitXid(XID *xid)
  {
    return doCommitXid(xid);
  }

  int rollbackXid(XID *xid)
  {
    return doRollbackXid(xid);
  }

  int recover(XID * append_to, size_t len)
  {
    return doRecover(append_to, len);
  }

  /** 
   * The below static class methods wrap the interaction
   * of the vector of registered XA storage engines.
   */
  static int commitOrRollbackXID(XID *xid, bool commit);
  static int recoverAllXids(HASH *commit_list);

  /* Class Methods for operating on plugin */
  static bool addPlugin(plugin::XaStorageEngine *engine);
  static void removePlugin(plugin::XaStorageEngine *engine);

private:
  /**
   * Does the PREPARE stage of the two-phase commit.
   */
  virtual int doPrepare(Session *session, bool normal_transaction)= 0;
  /**
   * Rolls back a transaction identified by a XID.
   */
  virtual int doRollbackXid(XID *xid)= 0;
  /**
   * Commits a transaction identified by a XID.
   */
  virtual int doCommitXid(XID *xid)= 0;
  /**
   * Notifies the transaction manager of any transactions
   * which had been marked prepared but not committed at
   * crash time or that have been heurtistically completed
   * by the storage engine.
   *
   * @param[out] Reference to a vector of XIDs to add to
   *
   * @retval
   *  Returns the number of transactions left to recover
   *  for this engine.
   */
  virtual int doRecover(XID * append_to, size_t len)= 0;

};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_XA_STORAGE_ENGINE_H */
