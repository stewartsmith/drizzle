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

#include "config.h"
#include "drizzled/resource_context.h"

#include <cassert>

using namespace std;

namespace drizzled
{

/** Clear, prepare for reuse. */
void ResourceContext::reset()
{
  resource= NULL;
  modified_data= false;
}

void ResourceContext::set_trx_read_write()
{
  assert(is_started());
  modified_data= true;
}


bool ResourceContext::is_trx_read_write() const
{
  assert(is_started());
  return modified_data;
}


bool ResourceContext::is_started() const
{
  return resource != NULL;
}

/** Mark this transaction read-write if the argument is read-write. */
void ResourceContext::coalesce_trx_with(const ResourceContext *stmt_trx)
{
  /*
    Must be called only after the transaction has been started.
    Can be called many times, e.g. when we have many
    read-write statements in a transaction.
  */
  assert(is_started());
  if (stmt_trx->is_trx_read_write())
    set_trx_read_write();
}

} /* namespace drizzled */
