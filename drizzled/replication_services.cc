/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
 *  Copyright (C) 2009-2010 Jay Pipes <jaypipes@gmail.com>
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

#include <config.h>
#include <drizzled/replication_services.h>
#include <drizzled/plugin/transaction_replicator.h>
#include <drizzled/plugin/transaction_applier.h>
#include <drizzled/message/transaction.pb.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/errmsg_print.h>

#include <string>
#include <vector>
#include <algorithm>

using namespace std;

namespace drizzled {

typedef std::vector<plugin::TransactionReplicator*> Replicators;
typedef std::vector<std::pair<std::string, plugin::TransactionApplier*> > Appliers;

/** 
  * Atomic boolean set to true if any *active* replicators
  * or appliers are actually registered.
  */
static bool is_active= false;
/**
  * The timestamp of the last time a Transaction message was successfully
  * applied (sent to an Applier)
  */
static atomic<uint64_t> last_applied_timestamp;
/** Our collection of registered replicator plugins */
static Replicators replicators;
/** Our collection of registered applier plugins and their requested replicator plugin names */
static Appliers appliers;
/** Our replication streams */
static ReplicationServices::ReplicationStreams replication_streams;

  /**
   * Strips underscores and lowercases supplied replicator name
   * or requested name, and appends the suffix "replicator" if missing...
   */
static void normalizeReplicatorName(string &name)
{
  transform(name.begin(), name.end(), name.begin(), ::tolower);
  if (name.find("replicator") == string::npos)
    name.append("replicator", 10);
  {
    size_t found_underscore= name.find('_');
    while (found_underscore != string::npos)
    {
      name.erase(found_underscore, 1);
      found_underscore= name.find('_');
    }
  }
}

bool ReplicationServices::evaluateRegisteredPlugins()
{
  /* 
   * We loop through appliers that have registered with us
   * and attempts to pair the applier with its requested
   * replicator.  If an applier has requested a replicator
   * that has either not been built or has not registered
   * with the replication services, we print an error and
   * return false
   */
  if (appliers.empty())
    return true;

  if (replicators.empty() && not appliers.empty())
  {
    errmsg_printf(error::ERROR,
                  N_("You registered a TransactionApplier plugin but no "
                     "TransactionReplicator plugins were registered.\n"));
    return false;
  }

  BOOST_FOREACH(Appliers::reference appl_iter, appliers)
  {
    plugin::TransactionApplier *applier= appl_iter.second;
    string requested_replicator_name= appl_iter.first;
    normalizeReplicatorName(requested_replicator_name);

    bool found= false;
    Replicators::iterator repl_iter;
    for (repl_iter= replicators.begin(); repl_iter != replicators.end(); ++repl_iter)
    {
      string replicator_name= (*repl_iter)->getName();
      normalizeReplicatorName(replicator_name);

      if (requested_replicator_name.compare(replicator_name) == 0)
      {
        found= true;
        break;
      }
    }
    if (not found)
    {
      errmsg_printf(error::ERROR,
                    N_("You registered a TransactionApplier plugin but no "
                       "TransactionReplicator plugins were registered that match the "
                       "requested replicator name of '%s'.\n"
                       "We have deactivated the TransactionApplier '%s'.\n"),
                       requested_replicator_name.c_str(),
                       applier->getName().c_str());
      applier->deactivate();
      return false;
    }
    else
    {
      replication_streams.push_back(make_pair(*repl_iter, applier));
    }
  }
  is_active= true;
  return true;
}

void ReplicationServices::attachReplicator(plugin::TransactionReplicator *in_replicator)
{
  replicators.push_back(in_replicator);
}

void ReplicationServices::detachReplicator(plugin::TransactionReplicator *in_replicator)
{
  replicators.erase(std::find(replicators.begin(), replicators.end(), in_replicator));
}

void ReplicationServices::attachApplier(plugin::TransactionApplier *in_applier, const string &requested_replicator_name)
{
  appliers.push_back(make_pair(requested_replicator_name, in_applier));
}

void ReplicationServices::detachApplier(plugin::TransactionApplier *)
{
}

bool ReplicationServices::isActive()
{
  return is_active;
}

plugin::ReplicationReturnCode ReplicationServices::pushTransactionMessage(Session &in_session,
                                                                          message::Transaction &to_push)
{
  plugin::ReplicationReturnCode result= plugin::SUCCESS;

  BOOST_FOREACH(ReplicationStreams::reference iter, replication_streams)
  {
    plugin::TransactionReplicator *cur_repl= iter.first;
    plugin::TransactionApplier *cur_appl= iter.second;

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
    }
    else
      return result;
  }
  return result;
}

ReplicationServices::ReplicationStreams &ReplicationServices::getReplicationStreams()
{
  return replication_streams;
}

} /* namespace drizzled */
