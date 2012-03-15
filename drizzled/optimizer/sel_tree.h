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

class SEL_TREE : public drizzled::memory::SqlAlloc
{
public:
  /*
    Starting an effort to document this field:
    (for some i, keys[i]->type == optimizer::SEL_ARG::IMPOSSIBLE) =>
       (type == SEL_TREE::IMPOSSIBLE)
  */
  enum Type
  {
    IMPOSSIBLE,
    ALWAYS,
    MAYBE,
    KEY,
    KEY_SMALLER
  } type;

  SEL_TREE(enum Type type_arg) :type(type_arg) {}
  SEL_TREE() :type(KEY)
  {
    keys_map.reset();
    memset(keys, 0, sizeof(keys));
  }
  /*
    Note: there may exist SEL_TREE objects with sel_tree->type=KEY and
    keys[i]=0 for all i. (SergeyP: it is not clear whether there is any
    merit in range analyzer functions (e.g. get_mm_parts) returning a
    pointer to such SEL_TREE instead of NULL)
  */
  SEL_ARG *keys[MAX_KEY];
  key_map keys_map;        /* bitmask of non-NULL elements in keys */

  /*
    Possible ways to read rows using index_merge. The list is non-empty only
    if type==KEY. Currently can be non empty only if keys_map.none().
  */
  List<SEL_IMERGE> merges;

  /* The members below are filled/used only after get_mm_tree is done */
  key_map ror_scans_map;   /* bitmask of ROR scan-able elements in keys */
  uint32_t n_ror_scans;     /* number of set bits in ror_scans_map */

  RorScanInfo **ror_scans;     /* list of ROR key scans */
  RorScanInfo **ror_scans_end; /* last ROR scan */
  /* Note that #records for each key scan is stored in table->quick_rows */

};

/*
  Check if two optimizer::SEL_TREES can be combined into one (i.e. a single key range
  read can be constructed for "cond_of_tree1 OR cond_of_tree2" ) without
  using index_merge.
*/
bool sel_trees_can_be_ored(const SEL_TREE&, const SEL_TREE&, const RangeParameter&);

SEL_TREE *
tree_or(RangeParameter *param, SEL_TREE *tree1, SEL_TREE *tree2);

/*
   Remove the trees that are not suitable for record retrieval.
   SYNOPSIS
   param  Range analysis parameter
   tree   Tree to be processed, tree->type is KEY or KEY_SMALLER

   DESCRIPTION
   This function walks through tree->keys[] and removes the SEL_ARG* trees
   that are not "maybe" trees (*) and cannot be used to construct quick range
   selects.
   (*) - have type MAYBE or MAYBE_KEY. Perhaps we should remove trees of
   these types here as well.

   A SEL_ARG* tree cannot be used to construct quick select if it has
   tree->part != 0. (e.g. it could represent "keypart2 < const").

   WHY THIS FUNCTION IS NEEDED

   Normally we allow construction of optimizer::SEL_TREE objects that have SEL_ARG
   trees that do not allow quick range select construction. For example for
   " keypart1=1 AND keypart2=2 " the execution will proceed as follows:
   tree1= optimizer::SEL_TREE { SEL_ARG{keypart1=1} }
   tree2= optimizer::SEL_TREE { SEL_ARG{keypart2=2} } -- can't make quick range select
   from this
   call tree_and(tree1, tree2) -- this joins SEL_ARGs into a usable SEL_ARG
   tree.

   There is an exception though: when we construct index_merge optimizer::SEL_TREE,
   any SEL_ARG* tree that cannot be used to construct quick range select can
   be removed, because current range analysis code doesn't provide any way
   that tree could be later combined with another tree.
   Consider an example: we should not construct
   st1 = optimizer::SEL_TREE {
   merges = SEL_IMERGE {
   optimizer::SEL_TREE(t.key1part1 = 1),
   optimizer::SEL_TREE(t.key2part2 = 2)   -- (*)
   }
   };
   because
   - (*) cannot be used to construct quick range select,
   - There is no execution path that would cause (*) to be converted to
   a tree that could be used.

   The latter is easy to verify: first, notice that the only way to convert
   (*) into a usable tree is to call tree_and(something, (*)).

   Second look at what tree_and/tree_or function would do when passed a
   optimizer::SEL_TREE that has the structure like st1 tree has, and conlcude that
   tree_and(something, (*)) will not be called.

   RETURN
   0  Ok, some suitable trees left
   1  No tree->keys[] left.
 */
bool remove_nonrange_trees(RangeParameter *param, SEL_TREE *tree);

} /* namespace optimizer */

} /* namespace drizzled */

