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

#include <config.h>

#include <drizzled/sql_base.h>
#include <drizzled/sql_select.h>
#include <drizzled/memory/sql_alloc.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/optimizer/range_param.h>
#include <drizzled/optimizer/sel_arg.h>
#include <drizzled/optimizer/sel_tree.h>
#include <drizzled/optimizer/sel_imerge.h>

using namespace std;
using namespace drizzled;

static optimizer::SEL_ARG *key_or(optimizer::RangeParameter *param, optimizer::SEL_ARG *key1, optimizer::SEL_ARG *key2);
static bool eq_tree(optimizer::SEL_ARG* a, optimizer::SEL_ARG *b);

bool optimizer::sel_trees_can_be_ored(const SEL_TREE& tree1, const SEL_TREE& tree2, const RangeParameter& param)
{
  key_map common_keys= tree1.keys_map;
  common_keys&= tree2.keys_map;

  if (common_keys.none())
    return false;

  /* trees have a common key, check if they refer to same key part */
  for (uint32_t key_no= 0; key_no < param.keys; key_no++)
  {
    if (common_keys.test(key_no) && tree1.keys[key_no]->part == tree2.keys[key_no]->part)
      return true;
  }
  return false;
}


/*
  Perform OR operation on 2 index_merge lists, storing result in first list.

  NOTES
    The following conversion is implemented:
     (a_1 &&...&& a_N)||(b_1 &&...&& b_K) = AND_i,j(a_i || b_j) =>
      => (a_1||b_1).

    i.e. all conjuncts except the first one are currently dropped.
    This is done to avoid producing N*K ways to do index_merge.

    If (a_1||b_1) produce a condition that is always true, NULL is returned
    and index_merge is discarded (while it is actually possible to try
    harder).

    As a consequence of this, choice of keys to do index_merge read may depend
    on the order of conditions in WHERE part of the query.

  RETURN
    0     OK, result is stored in *im1
    other Error, both passed lists are unusable
*/

static int imerge_list_or_list(optimizer::RangeParameter *param, List<optimizer::SEL_IMERGE> *im1, List<optimizer::SEL_IMERGE> *im2)
{
  optimizer::SEL_IMERGE *imerge= &im1->front();
  im1->clear();
  im1->push_back(imerge);

  return imerge->or_sel_imerge_with_checks(*param, im2->front());
}


/*
  Perform OR operation on index_merge list and key tree.

  RETURN
     0     OK, result is stored in *im1.
     other Error
 */

static int imerge_list_or_tree(optimizer::RangeParameter& param, List<optimizer::SEL_IMERGE>& im1, optimizer::SEL_TREE& tree)
{
  List_iterator<optimizer::SEL_IMERGE> it(im1.begin());
  while (optimizer::SEL_IMERGE* imerge= it++)
  {
    if (imerge->or_sel_tree_with_checks(param, tree))
      it.remove();
  }
  return im1.is_empty();
}


optimizer::SEL_TREE* optimizer::tree_or(optimizer::RangeParameter *param, optimizer::SEL_TREE *tree1, optimizer::SEL_TREE *tree2)
{
  if (! tree1 || ! tree2)
    return 0;

  if (tree1->type == SEL_TREE::IMPOSSIBLE || tree2->type == SEL_TREE::ALWAYS)
    return tree2;

  if (tree2->type == SEL_TREE::IMPOSSIBLE || tree1->type == SEL_TREE::ALWAYS)
    return tree1;

  if (tree1->type == SEL_TREE::MAYBE)
    return tree1; // Can't use this

  if (tree2->type == SEL_TREE::MAYBE)
    return tree2;

  SEL_TREE *result= NULL;
  key_map  result_keys;
  result_keys.reset();
  if (sel_trees_can_be_ored(*tree1, *tree2, *param))
  {
    /* Join the trees key per key */
    SEL_ARG** key1= tree1->keys;
    SEL_ARG** key2= tree2->keys;
    SEL_ARG** end= key1+param->keys;
    for (; key1 != end;  key1++, key2++)
    {
      *key1= key_or(param, *key1, *key2);
      if (*key1)
      {
        result= tree1; // Added to tree1
        result_keys.set(key1 - tree1->keys);
      }
    }
    if (result)
      result->keys_map= result_keys;
  }
  else
  {
    /* ok, two trees have KEY type but cannot be used without index merge */
    if (tree1->merges.is_empty() && tree2->merges.is_empty())
    {
      if (param->remove_jump_scans && (remove_nonrange_trees(param, tree1) || remove_nonrange_trees(param, tree2)))
        return new SEL_TREE(SEL_TREE::ALWAYS);
      /* both trees are "range" trees, produce new index merge structure */
			result= new SEL_TREE();
			SEL_IMERGE* merge= new SEL_IMERGE();
			result->merges.push_back(merge);
      merge->or_sel_tree(param, tree1);
      merge->or_sel_tree(param, tree2);
      result->type= tree1->type;
    }
    else if (!tree1->merges.is_empty() && !tree2->merges.is_empty())
    {
      result= imerge_list_or_list(param, &tree1->merges, &tree2->merges)
        ? new SEL_TREE(SEL_TREE::ALWAYS)
        : tree1;
    }
    else
    {
      /* one tree is index merge tree and another is range tree */
      if (tree1->merges.is_empty())
        std::swap(tree1, tree2);

      if (param->remove_jump_scans && remove_nonrange_trees(param, tree2))
         return new SEL_TREE(SEL_TREE::ALWAYS);
      /* add tree2 to tree1->merges, checking if it collapses to ALWAYS */
      result= imerge_list_or_tree(*param, tree1->merges, *tree2)
        ? new SEL_TREE(SEL_TREE::ALWAYS)
        : tree1;
    }
  }
  return result;
}


static optimizer::SEL_ARG *
key_or(optimizer::RangeParameter *param, optimizer::SEL_ARG *key1, optimizer::SEL_ARG *key2)
{
  if (! key1)
  {
    if (key2)
    {
      key2->use_count--;
      key2->free_tree();
    }
    return 0;
  }
  if (! key2)
  {
    key1->use_count--;
    key1->free_tree();
    return 0;
  }
  key1->use_count--;
  key2->use_count--;

  if (key1->part != key2->part)
  {
    key1->free_tree();
    key2->free_tree();
    return 0;					// Can't optimize this
  }

  // If one of the key is MAYBE_KEY then the found region may be bigger
  if (key1->type == optimizer::SEL_ARG::MAYBE_KEY)
  {
    key2->free_tree();
    key1->use_count++;
    return key1;
  }
  if (key2->type == optimizer::SEL_ARG::MAYBE_KEY)
  {
    key1->free_tree();
    key2->use_count++;
    return key2;
  }

  if (key1->use_count > 0)
  {
    if (key2->use_count == 0 || key1->elements > key2->elements)
    {
      std::swap(key1,key2);
    }
    if (key1->use_count > 0 || !(key1=key1->clone_tree(param)))
      return 0;					// OOM
  }

  // Add tree at key2 to tree at key1
  bool key2_shared= key2->use_count != 0;
  key1->maybe_flag|= key2->maybe_flag;

  for (key2=key2->first(); key2; )
  {
    optimizer::SEL_ARG *tmp= key1->find_range(key2); // Find key1.min <= key2.min
    int cmp;

    if (! tmp)
    {
      tmp=key1->first(); // tmp.min > key2.min
      cmp= -1;
    }
    else if ((cmp=tmp->cmp_max_to_min(key2)) < 0)
    {						// Found tmp.max < key2.min
      optimizer::SEL_ARG *next= tmp->next;
      if (cmp == -2 && eq_tree(tmp->next_key_part,key2->next_key_part))
      {
        // Join near ranges like tmp.max < 0 and key2.min >= 0
        optimizer::SEL_ARG *key2_next=key2->next;
        if (key2_shared)
        {
          key2=new optimizer::SEL_ARG(*key2);
          key2->increment_use_count(key1->use_count+1);
          key2->next= key2_next; // New copy of key2
        }
        key2->copy_min(tmp);
        if (! (key1=key1->tree_delete(tmp)))
        {					// Only one key in tree
          key1= key2;
          key1->make_root();
          key2= key2_next;
          break;
        }
      }
      if (! (tmp= next)) // tmp.min > key2.min
        break; // Copy rest of key2
    }
    if (cmp < 0)
    {						// tmp.min > key2.min
      int tmp_cmp;
      if ((tmp_cmp= tmp->cmp_min_to_max(key2)) > 0) // if tmp.min > key2.max
      {
        if (tmp_cmp == 2 && eq_tree(tmp->next_key_part,key2->next_key_part))
        {					// ranges are connected
          tmp->copy_min_to_min(key2);
          key1->merge_flags(key2);
          if (tmp->min_flag & NO_MIN_RANGE &&
              tmp->max_flag & NO_MAX_RANGE)
          {
            if (key1->maybe_flag)
              return new optimizer::SEL_ARG(optimizer::SEL_ARG::MAYBE_KEY);
            return 0;
          }
          key2->increment_use_count(-1);	// Free not used tree
          key2= key2->next;
          continue;
        }
        else
        {
          optimizer::SEL_ARG *next= key2->next; // Keys are not overlapping
          if (key2_shared)
          {
            optimizer::SEL_ARG *cpy= new optimizer::SEL_ARG(*key2); // Must make copy
            if (! cpy)
              return 0; // OOM
            key1= key1->insert(cpy);
            key2->increment_use_count(key1->use_count+1);
          }
          else
            key1= key1->insert(key2);		// Will destroy key2_root
          key2= next;
          continue;
        }
      }
    }

    // tmp.max >= key2.min && tmp.min <= key.cmax(overlapping ranges)
    if (eq_tree(tmp->next_key_part,key2->next_key_part))
    {
      if (tmp->is_same(key2))
      {
        tmp->merge_flags(key2);			// Copy maybe flags
        key2->increment_use_count(-1);		// Free not used tree
      }
      else
      {
        optimizer::SEL_ARG *last= tmp;
        while (last->next && last->next->cmp_min_to_max(key2) <= 0 &&
               eq_tree(last->next->next_key_part,key2->next_key_part))
        {
          optimizer::SEL_ARG *save= last;
          last= last->next;
          key1= key1->tree_delete(save);
        }
        last->copy_min(tmp);
        if (last->copy_min(key2) || last->copy_max(key2))
        {					// Full range
          key1->free_tree();
          for (; key2; key2= key2->next)
            key2->increment_use_count(-1);	// Free not used tree
          if (key1->maybe_flag)
            return new optimizer::SEL_ARG(optimizer::SEL_ARG::MAYBE_KEY);
          return 0;
        }
      }
      key2= key2->next;
      continue;
    }

    if (cmp >= 0 && tmp->cmp_min_to_min(key2) < 0)
    {						// tmp.min <= x < key2.min
      optimizer::SEL_ARG *new_arg= tmp->clone_first(key2);
      if ((new_arg->next_key_part= key1->next_key_part))
        new_arg->increment_use_count(key1->use_count+1);
      tmp->copy_min_to_min(key2);
      key1= key1->insert(new_arg);
    }

    // tmp.min >= key2.min && tmp.min <= key2.max
    optimizer::SEL_ARG key(*key2); // Get copy we can modify
    for (;;)
    {
      if (tmp->cmp_min_to_min(&key) > 0)
      {						// key.min <= x < tmp.min
        optimizer::SEL_ARG *new_arg= key.clone_first(tmp);
        if ((new_arg->next_key_part=key.next_key_part))
          new_arg->increment_use_count(key1->use_count+1);
        key1= key1->insert(new_arg);
      }
      if ((cmp=tmp->cmp_max_to_max(&key)) <= 0)
      {						// tmp.min. <= x <= tmp.max
        tmp->maybe_flag|= key.maybe_flag;
        key.increment_use_count(key1->use_count+1);
        tmp->next_key_part= key_or(param, tmp->next_key_part, key.next_key_part);
        if (! cmp)				// Key2 is ready
          break;
        key.copy_max_to_min(tmp);
        if (! (tmp= tmp->next))
        {
          optimizer::SEL_ARG *tmp2= new optimizer::SEL_ARG(key);
          if (! tmp2)
            return 0;				// OOM
          key1= key1->insert(tmp2);
          key2= key2->next;
          goto end;
        }
        if (tmp->cmp_min_to_max(&key) > 0)
        {
          optimizer::SEL_ARG *tmp2= new optimizer::SEL_ARG(key);
          if (! tmp2)
            return 0;				// OOM
          key1= key1->insert(tmp2);
          break;
        }
      }
      else
      {
        optimizer::SEL_ARG *new_arg= tmp->clone_last(&key); // tmp.min <= x <= key.max
        tmp->copy_max_to_min(&key);
        tmp->increment_use_count(key1->use_count+1);
        /* Increment key count as it may be used for next loop */
        key.increment_use_count(1);
        new_arg->next_key_part= key_or(param, tmp->next_key_part, key.next_key_part);
        key1= key1->insert(new_arg);
        break;
      }
    }
    key2= key2->next;
  }

end:
  while (key2)
  {
    optimizer::SEL_ARG *next= key2->next;
    if (key2_shared)
    {
      optimizer::SEL_ARG *tmp= new optimizer::SEL_ARG(*key2);		// Must make copy
      if (! tmp)
        return 0;
      key2->increment_use_count(key1->use_count+1);
      key1= key1->insert(tmp);
    }
    else
      key1= key1->insert(key2);			// Will destroy key2_root
    key2= next;
  }
  key1->use_count++;
  return key1;
}


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
bool optimizer::remove_nonrange_trees(optimizer::RangeParameter *param, optimizer::SEL_TREE *tree)
{
  bool res= true;
  for (uint32_t i= 0; i < param->keys; i++)
  {
    if (tree->keys[i])
    {
      if (tree->keys[i]->part)
      {
        tree->keys[i]= NULL;
        tree->keys_map.reset(i);
      }
      else
        res= false;
    }
  }
  return res;
}


/* Compare if two trees are equal */
static bool eq_tree(optimizer::SEL_ARG *a, optimizer::SEL_ARG *b)
{
  if (a == b)
    return true;

  if (! a || ! b || ! a->is_same(b))
  {
    return false;
  }

  if (a->left != &optimizer::null_element && b->left != &optimizer::null_element)
  {
    if (! eq_tree(a->left,b->left))
      return false;
  }
  else if (a->left != &optimizer::null_element || b->left != &optimizer::null_element)
  {
    return false;
  }

  if (a->right != &optimizer::null_element && b->right != &optimizer::null_element)
  {
    if (! eq_tree(a->right,b->right))
      return false;
  }
  else if (a->right != &optimizer::null_element || b->right != &optimizer::null_element)
  {
    return false;
  }

  if (a->next_key_part != b->next_key_part)
  {						// Sub range
    if (! a->next_key_part != ! b->next_key_part ||
	      ! eq_tree(a->next_key_part, b->next_key_part))
      return false;
  }

  return true;
}
