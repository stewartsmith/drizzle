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

#include <drizzled/memory/sql_alloc.h>

namespace drizzled {
namespace optimizer {

/*
  SEL_IMERGE is a list of possible ways to do index merge, i.e. it is
  a condition in the following form:
   (t_1||t_2||...||t_N) && (next)

  where all t_i are optimizer::SEL_TREEs, next is another SEL_IMERGE and no pair
  (t_i,t_j) contains SEL_ARGS for the same index.

  optimizer::SEL_TREE contained in SEL_IMERGE always has merges=NULL.

  This class relies on memory manager to do the cleanup.
*/
class SEL_IMERGE : public memory::SqlAlloc
{
  enum { PREALLOCED_TREES= 10};
public:
  SEL_TREE *trees_prealloced[PREALLOCED_TREES];
  SEL_TREE **trees;             /* trees used to do index_merge   */
  SEL_TREE **trees_next;        /* last of these trees            */
  SEL_TREE **trees_end;         /* end of allocated space         */

  SEL_ARG  ***best_keys;        /* best keys to read in optimizer::SEL_TREEs */

  SEL_IMERGE();

  /*
     Add optimizer::SEL_TREE to this index_merge without any checks,

     NOTES
     This function implements the following:
     (x_1||...||x_N) || t = (x_1||...||x_N||t), where x_i, t are optimizer::SEL_TREEs

     RETURN
     0 - OK
     -1 - Out of memory.
   */
  void or_sel_tree(RangeParameter *param, SEL_TREE *tree);

  /*
     Perform OR operation on this SEL_IMERGE and supplied optimizer::SEL_TREE new_tree,
     combining new_tree with one of the trees in this SEL_IMERGE if they both
     have SEL_ARGs for the same key.

     SYNOPSIS
     or_sel_tree_with_checks()
     param    Parameter from SqlSelect::test_quick_select
     new_tree optimizer::SEL_TREE with type KEY or KEY_SMALLER.

     NOTES
     This does the following:
     (t_1||...||t_k)||new_tree =
     either
     = (t_1||...||t_k||new_tree)
     or
     = (t_1||....||(t_j|| new_tree)||...||t_k),

     where t_i, y are optimizer::SEL_TREEs.
     new_tree is combined with the first t_j it has a SEL_ARG on common
     key with. As a consequence of this, choice of keys to do index_merge
     read may depend on the order of conditions in WHERE part of the query.

     RETURN
     0  OK
     1  One of the trees was combined with new_tree to optimizer::SEL_TREE::ALWAYS,
     and (*this) should be discarded.
     -1  An error occurred.
   */
  int or_sel_tree_with_checks(RangeParameter&, SEL_TREE&);

  /*
     Perform OR operation on this index_merge and supplied index_merge list.

     RETURN
     0 - OK
     1 - One of conditions in result is always true and this SEL_IMERGE
     should be discarded.
     -1 - An error occurred
   */
  int or_sel_imerge_with_checks(RangeParameter&, SEL_IMERGE&);

};


} /* namespace optimizer */

} /* namespace drizzled */

