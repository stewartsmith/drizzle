/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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
 * Defines the implementation of the default replicator.
 *
 * @see drizzled/plugin/transaction_replicator.h
 * @see drizzled/plugin/transaction_applier.h
 *
 * @details
 *
 * This is a very simple implementation.  All we do is pass along the 
 * event to the supplier.  This is meant as a skeleton replicator only.
 */

#include <config.h>
#include <drizzled/plugin.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin/transaction_applier.h>

#include "default_replicator.h"

#include <vector>
#include <string>

using namespace std;
using namespace drizzled;

plugin::ReplicationReturnCode
DefaultReplicator::replicate(plugin::TransactionApplier *in_applier,
                             Session &in_session,
                             message::Transaction &to_replicate)
{
  /* 
   * We do absolutely nothing but call the applier's apply() method, passing
   * along the supplied Transaction.  Yep, told you it was simple...
   */
  return in_applier->apply(in_session, to_replicate);
}

static DefaultReplicator *default_replicator= NULL; /* The singleton replicator */

static int init(module::Context &context)
{
  default_replicator= new DefaultReplicator("default_replicator");
  context.add(default_replicator);
  return 0;
}

DRIZZLE_PLUGIN(init,  NULL, NULL);
