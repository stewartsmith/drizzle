/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

namespace drizzled
{

namespace optimizer
{

/**
  Test if the predicate compares a field with constants.

  @param func_item        Predicate item
  @param[out] args        Here we store the field followed by constants
  @param[out] inv_order   Is set to 1 if the predicate is of the form
                          'const op field'

  @retval
    0        func_item is a simple predicate: a field is compared with
    constants
  @retval
    1        Otherwise
*/
bool simple_pred(Item_func *func_item, Item **args, bool &inv_order);

/**
  Substitutes constants for some COUNT(), MIN() and MAX() functions.

  @param tables                list of leaves of join table tree
  @param all_fields            All fields to be returned
  @param conds                 WHERE clause

  @note
    This function is only called for queries with sum functions and no
    GROUP BY part.

  @retval
    0                    no errors
  @retval
    1                    if all items were resolved
  @retval
    HA_ERR_KEY_NOT_FOUND on impossible conditions
  @retval
    HA_ERR_... if a deadlock or a lock wait timeout happens, for example
*/
int sum_query(TableList *tables, List<Item> &all_fields, COND *conds);

} /* namespace optimizer */

} /* namespace drizzled */

