/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

#include <drizzled/resource_context.h>

namespace drizzled
{

/**
  Storage engine specific thread local data.
*/
struct Ha_data
{
  /**
    Storage engine specific thread local data.
    Lifetime: one user connection.
  */
  void *ha_ptr;
  /**
   * Resource contexts for both the "statement" and "normal"
   * transactions.
   *
   * Resource context at index 0:
   *
   * Life time: one statement within a transaction. If @@autocommit is
   * on, also represents the entire transaction.
   *
   * Resource context at index 1:
   *
   * Life time: one transaction within a connection. 
   *
   * @note
   *
   * If the storage engine does not participate in a transaction, 
   * there will not be a resource context.
   */
  drizzled::ResourceContext resource_context[2];

  Ha_data() :
    ha_ptr(NULL)
  {}
};


} /* namespace drizzled */

