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

#ifndef DRIZZLED_NESTED_JOIN_H
#define DRIZZLED_NESTED_JOIN_H

struct nested_join_st
{
  List<TableList>  join_list;       /* list of elements in the nested join */
  table_map         used_tables;     /* bitmap of tables in the nested join */
  table_map         not_null_tables; /* tables that rejects nulls           */
  struct st_join_table *first_nested;/* the first nested table in the plan  */
  /* 
    Used to count tables in the nested join in 2 isolated places:
    1. In make_outerjoin_info(). 
    2. check_interleaving_with_nj/restore_prev_nj_state (these are called
       by the join optimizer. 
    Before each use the counters are zeroed by reset_nj_counters.
  */
  uint              counter_;
  nested_join_map   nj_map;          /* Bit used to identify this nested join*/
  /*
    (Valid only for semi-join nests) Bitmap of tables outside the semi-join
    that are used within the semi-join's ON condition.
  */
  table_map         sj_depends_on;
  /* Outer non-trivially correlated tables */
  table_map         sj_corr_tables;
  List<Item>        sj_outer_expr_list;
};

#endif /* DRIZZLED_NESTED_JOIN_H */
