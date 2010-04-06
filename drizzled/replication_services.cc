/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
 *  Copyright (c) 2009-2010 Jay Pipes <jaypipes@gmail.com>
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

/**
 * @file Server-side utility which is responsible for managing the 
 * communication between the kernel and the replication plugins:
 *
 * - TransactionReplicator
 * - TransactionApplier
 * - Publisher
 * - Subscriber
 *
 * ReplicationServices is a bridge between replication modules and the kernel,
 * and its primary function is to  */

#include "config.h"
#include "drizzled/replication_services.h"
#include "drizzled/plugin/transaction_replicator.h"
#include "drizzled/plugin/transaction_applier.h"
#include "drizzled/message/transaction.pb.h"
#include "drizzled/gettext.h"
#include "drizzled/session.h"
#include "drizzled/error.h"

#include <vector>

using namespace std;

namespace drizzled
{

ReplicationServices::ReplicationServices()
{
  is_active= false;
}

void ReplicationServices::evaluateActivePlugins()
{
  /* 
   * We loop through replicators and appliers, evaluating
   * whether or not there is at least one active replicator
   * and one active applier.  If not, we set is_active
   * to false.
   */
  bool tmp_is_active= false;

  if (replicators.empty() || appliers.empty())
  {
    is_active= false;
    return;
  }

  /* 
   * Determine if any remaining replicators and if those
   * replicators are active...if not, set is_active
   * to false
   */
  for (Replicators::iterator repl_iter= replicators.begin();
       repl_iter != replicators.end();
       ++repl_iter)
  {
    if ((*repl_iter)->isEnabled())
    {
      tmp_is_active= true;
      break;
    }
  }
  if (! tmp_is_active)
  {
    /* No active replicators. Set is_active to false and exit. */
    is_active= false;
    return;
  }

  /* 
   * OK, we know there's at least one active replicator.
   *
   * Now determine if any remaining replicators and if those
   * replicators are active...if not, set is_active
   * to false
   */
  for (Appliers::iterator appl_iter= appliers.begin();
       appl_iter != appliers.end();
       ++appl_iter)
  {
    if ((*appl_iter)->isEnabled())
    {
      is_active= true;
      return;
    }
  }
  /* If we get here, there are no active appliers */
  is_active= false;
}

void ReplicationServices::attachReplicator(plugin::TransactionReplicator *in_replicator)
{
  replicators.push_back(in_replicator);
  evaluateActivePlugins();
}

void ReplicationServices::detachReplicator(plugin::TransactionReplicator *in_replicator)
{
  replicators.erase(std::find(replicators.begin(), replicators.end(), in_replicator));
  evaluateActivePlugins();
}

void ReplicationServices::attachApplier(plugin::TransactionApplier *in_applier)
{
  appliers.push_back(in_applier);
  evaluateActivePlugins();
}

void ReplicationServices::detachApplier(plugin::TransactionApplier *in_applier)
{
  appliers.erase(std::find(appliers.begin(), appliers.end(), in_applier));
  evaluateActivePlugins();
}

bool ReplicationServices::isActive() const
{
  return is_active;
}

plugin::ReplicationReturnCode ReplicationServices::pushTransactionMessage(Session &in_session,
                                                                          message::Transaction &to_push)
{
  vector<plugin::TransactionReplicator *>::iterator repl_iter= replicators.begin();
  vector<plugin::TransactionApplier *>::iterator appl_start_iter, appl_iter;
  appl_start_iter= appliers.begin();

  plugin::TransactionReplicator *cur_repl;
  plugin::TransactionApplier *cur_appl;

  plugin::ReplicationReturnCode result= plugin::SUCCESS;

  while (repl_iter != replicators.end())
  {
    cur_repl= *repl_iter;
    if (! cur_repl->isEnabled())
    {
      ++repl_iter;
      continue;
    }
    
    appl_iter= appl_start_iter;
    while (appl_iter != appliers.end())
    {
      cur_appl= *appl_iter;

      if (! cur_appl->isEnabled())
      {
        ++appl_iter;
        continue;
      }

      result= cur_repl->replicate(cur_appl, in_session, to_push);

      if (result == plugin::SUCCESS)
      {
        /* 
         * We update the timestamp for the last applied Transaction so that
         * publisher plugins can ask the replication services when the
         * last known applied Transaction was using the getLastAppliedTimestamp()
         * method.
         */
        last_applied_timestamp.fetch_and_store(to_push.transaction_context().end_timestamp());
        ++appl_iter;
      }
      else
        return result;
    }
    ++repl_iter;
  }
  return result;
}

} /* namespace drizzled */
