/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems
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

#include "drizzled/sql_base.h"
#include "drizzled/sql_select.h"
#include "drizzled/memory/sql_alloc.h"
#include "drizzled/optimizer/range.h"
#include "drizzled/optimizer/range_param.h"
#include "drizzled/optimizer/sel_arg.h"
#include "drizzled/optimizer/sel_tree.h"
#include "drizzled/optimizer/sel_imerge.h"

using namespace std;
using namespace drizzled;

optimizer::SEL_IMERGE::SEL_IMERGE() 
  :
    trees(&trees_prealloced[0]),
    trees_next(trees),
    trees_end(trees + PREALLOCED_TREES)
{}


int optimizer::SEL_IMERGE::or_sel_tree(optimizer::RangeParameter *param, optimizer::SEL_TREE *tree)
{
  if (trees_next == trees_end)
  {
    const int realloc_ratio= 2;		/* Double size for next round */
    uint32_t old_elements= (trees_end - trees);
    uint32_t old_size= sizeof(optimizer::SEL_TREE**) * old_elements;
    uint32_t new_size= old_size * realloc_ratio;
    optimizer::SEL_TREE **new_trees= NULL;
    if (! (new_trees= (optimizer::SEL_TREE**) param->mem_root->alloc_root(new_size)))
      return -1;
    memcpy(new_trees, trees, old_size);
    trees= new_trees;
    trees_next= trees + old_elements;
    trees_end= trees + old_elements * realloc_ratio;
  }
  *(trees_next++)= tree;
  return 0;
}


int optimizer::SEL_IMERGE::or_sel_tree_with_checks(optimizer::RangeParameter *param, optimizer::SEL_TREE *new_tree)
{
  for (optimizer::SEL_TREE** tree = trees;
       tree != trees_next;
       tree++)
  {
    if (sel_trees_can_be_ored(*tree, new_tree, param))
    {
      *tree = tree_or(param, *tree, new_tree);
      if (!*tree)
        return 1;
      if (((*tree)->type == optimizer::SEL_TREE::MAYBE) ||
          ((*tree)->type == optimizer::SEL_TREE::ALWAYS))
        return 1;
      /* optimizer::SEL_TREE::IMPOSSIBLE is impossible here */
      return 0;
    }
  }

  /* New tree cannot be combined with any of existing trees. */
  return or_sel_tree(param, new_tree);
}


int optimizer::SEL_IMERGE::or_sel_imerge_with_checks(optimizer::RangeParameter *param, optimizer::SEL_IMERGE* imerge)
{
  for (optimizer::SEL_TREE** tree= imerge->trees;
       tree != imerge->trees_next;
       tree++)
  {
    if (or_sel_tree_with_checks(param, *tree))
      return 1;
  }
  return 0;
}

