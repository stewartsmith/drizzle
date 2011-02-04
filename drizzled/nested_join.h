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

#ifndef DRIZZLED_NESTED_JOIN_H
#define DRIZZLED_NESTED_JOIN_H

#include <drizzled/sql_list.h>
#include <drizzled/item.h>

#include <bitset>

namespace drizzled
{
class TableList;

class nested_join_st
{
public:
  /* list of elements in the nested join */
  List<TableList> join_list;

  /* bitmap of tables in the nested join */
  table_map used_tables;

  /* tables that rejects nulls           */
  table_map not_null_tables;

  /* the first nested table in the plan  */
  JoinTable *first_nested;

  /*
    Used to count tables in the nested join in 2 isolated places:
    1. In make_outerjoin_info().
    2. check_interleaving_with_nj/restore_prev_nj_state (these are called
    by the join optimizer.
    Before each use the counters are zeroed by reset_nj_counters.
  */
  uint32_t counter_;

  /* Bit used to identify this nested join*/
  std::bitset<64> nj_map;

  /*
    (Valid only for semi-join nests) Bitmap of tables outside the semi-join
    that are used within the semi-join's ON condition.
  */
  table_map sj_depends_on;
  /* Outer non-trivially correlated tables */
  table_map sj_corr_tables;

  List<Item> sj_outer_expr_list;

  /**
     True if this join nest node is completely covered by the query execution
     plan. This means two things.

     1. All tables on its @c join_list are covered by the plan.

     2. All child join nest nodes are fully covered.
   */
  bool is_fully_covered() const { return join_list.elements == counter_; }
};

} /* namespace drizzled */

#endif /* DRIZZLED_NESTED_JOIN_H */
