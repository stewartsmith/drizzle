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

#include <drizzled/item.h>
#include <drizzled/lex_string.h>
#include <drizzled/sql_list.h>

#include <bitset>

namespace drizzled {

class NestedJoin
{
public:
  /*
    This constructor serves for creation of NestedJoin instances
  */
  NestedJoin() 
  :
  join_list(),
  used_tables(),
  not_null_tables(),
  first_nested(NULL),
  counter_(0),
  nj_map(),
  sj_depends_on(),
  sj_corr_tables(),
  sj_outer_expr_list()      
  { }    

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
     True if this join nest node is completely covered by the query execution
     plan. This means two things.

     1. All tables on its @c join_list are covered by the plan.

     2. All child join nest nodes are fully covered.
   */

  bool is_fully_covered() const { return join_list.size() == counter_; }

  /* To get the table_map sj_depends_on */
  table_map getSjDependsOn() const
  {
    return sj_depends_on;
  }

  /* To set the table_map sj_depends_on */
  void setSjDependsOn(const table_map &in_sj_depends_on)
  {
    sj_depends_on= in_sj_depends_on;
  }

  /* To get the table_map sj_corr_tables */
  table_map getSjCorrTables() const
  {
    return sj_corr_tables;
  }
  
  /* To set the table_map sj_corr_tables */
  void setSjCorrTables(const table_map &in_sj_corr_tables) 
  {
    sj_corr_tables= in_sj_corr_tables;
  }

  /* To get the List sj_outer_expr_list */
  const List<Item>& getSjOuterExprList() const
  {
    return sj_outer_expr_list;
  }
  
  /* To set the List sj_outer_expr_list */
  void setSjOuterExprList(const List<Item> &in_sj_outer_expr_list)
  {
    sj_outer_expr_list= in_sj_outer_expr_list;
  }

private:
  /*
    (Valid only for semi-join nests) Bitmap of tables outside the semi-join
    that are used within the semi-join's ON condition.
  */
  table_map sj_depends_on;
  
  /* Outer non-trivially correlated tables */
  table_map sj_corr_tables;

  List<Item> sj_outer_expr_list;

};

} /* namespace drizzled */

