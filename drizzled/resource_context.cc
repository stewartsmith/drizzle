/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>
#include <drizzled/resource_context.h>

#include <cassert>

using namespace std;

namespace drizzled
{

/** Clear, prepare for reuse. */
void ResourceContext::reset()
{
  monitored= NULL;
  xa_resource_manager= NULL;
  trx_storage_engine= NULL;
  modified_data= false;
}

void ResourceContext::markModifiedData()
{
  assert(isStarted());
  modified_data= true;
}

bool ResourceContext::hasModifiedData() const
{
  assert(isStarted());
  return modified_data;
}

bool ResourceContext::isStarted() const
{
  return monitored != NULL;
}

void ResourceContext::coalesceWith(const ResourceContext *stmt_ctx)
{
  /*
    Must be called only after the transaction has been started.
    Can be called many times, e.g. when we have many
    read-write statements in a transaction.
  */
  assert(isStarted());
  if (stmt_ctx->hasModifiedData())
    markModifiedData();
}

} /* namespace drizzled */
