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

#pragma once

#include <vector>
#include <drizzled/common_fwd.h>

namespace drizzled {

class TransactionContext
{
public:
  TransactionContext() :
    no_2pc(false),
    resource_contexts(),
    modified_non_trans_data(false)
  {}

  void reset() { no_2pc= false; modified_non_trans_data= false; resource_contexts.clear();}

  typedef std::vector<ResourceContext *> ResourceContexts;

  void setResourceContexts(ResourceContexts &new_contexts)
  {
    resource_contexts.assign(new_contexts.begin(), new_contexts.end());
  }

  ResourceContexts &getResourceContexts()
  {
    return resource_contexts;
  }
  /** Register a resource context in this transaction context */
  void registerResource(ResourceContext *resource)
  {
    resource_contexts.push_back(resource);
  }

  /**
   * Marks that this transaction has modified state
   * of some non-transactional data.
   */
  void markModifiedNonTransData()
  {
    modified_non_trans_data= true;
  }

  /**
   * Returns true if the transaction has modified
   * state of some non-transactional data.
   */
  bool hasModifiedNonTransData() const
  {
    return modified_non_trans_data;
  }

  /* true is not all entries in the resource contexts support 2pc */
  bool no_2pc;
private:
  /** Resource that registered in this transaction */
  ResourceContexts resource_contexts;
  /** Whether this transaction has changed non-transaction data state */
  bool modified_non_trans_data;
};

} /* namespace drizzled */

