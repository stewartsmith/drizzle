/* Copyright (C) 2001 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Function to handle quick removal of duplicates
  This code is used when doing multi-table deletes to find the rows in
  reference tables that needs to be deleted.

  The basic idea is as follows:

  Store first all strings in a binary tree, ignoring duplicates.

  The unique entries will be returned in sort order, to ensure that we do the
  deletes in disk order.
*/

#include <config.h>

#include <math.h>

#include <queue>

#include <drizzled/sql_sort.h>
#include <drizzled/session.h>
#include <drizzled/sql_list.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/unique.h>
#include <drizzled/table.h>

#if defined(CMATH_NAMESPACE)
using namespace CMATH_NAMESPACE;
#endif

using namespace std;

namespace drizzled {

int unique_write_to_ptrs(unsigned char* key,
                         uint32_t, Unique *unique)
{
  memcpy(unique->record_pointers, key, unique->size);
  unique->record_pointers+=unique->size;
  return 0;
}

Unique::Unique(qsort_cmp2 comp_func, void * comp_func_fixed_arg,
	       uint32_t size_arg, size_t max_in_memory_size_arg)
  : max_in_memory_size(max_in_memory_size_arg),
    size(size_arg),
    elements(0)
{
  // Second element is max size for memory use in unique sort
  init_tree(&tree, 0, 0, size, comp_func, false,
            NULL, comp_func_fixed_arg);
}


/*
  Calculate log2(n!)

  NOTES
    Stirling's approximate formula is used:

      n! ~= sqrt(2*M_PI*n) * (n/M_E)^n

    Derivation of formula used for calculations is as follows:

    log2(n!) = log(n!)/log(2) = log(sqrt(2*M_PI*n)*(n/M_E)^n) / log(2) =

      = (log(2*M_PI*n)/2 + n*log(n/M_E)) / log(2).
*/

inline double log2_n_fact(double x)
{
  return (log(2*M_PI*x)/2 + x*log(x/M_E)) / M_LN2;
}


/*
  Calculate cost of using Unique for processing nkeys elements of size
  key_size using max_in_memory_size memory.

  SYNOPSIS
    Unique::get_use_cost()
      buffer    space for temporary data, use Unique::get_cost_calc_buff_size
                to get # bytes needed.
      nkeys     #of elements in Unique
      key_size  size of each elements in bytes
      max_in_memory_size amount of memory Unique will be allowed to use

  RETURN
    Cost in disk seeks.

  NOTES
    cost(using_unqiue) =
      cost(create_trees) +  (see #1)
      cost(merge) +         (see #2)
      cost(read_result)     (see #3)

    1. Cost of trees creation
      For each Unique::put operation there will be 2*log2(n+1) elements
      comparisons, where n runs from 1 tree_size (we assume that all added
      elements are different). Together this gives:

      n_compares = 2*(log2(2) + log2(3) + ... + log2(N+1)) = 2*log2((N+1)!)

      then cost(tree_creation) = n_compares*ROWID_COMPARE_COST;

      Total cost of creating trees:
      (n_trees - 1)*max_size_tree_cost + non_max_size_tree_cost.

      Approximate value of log2(N!) is calculated by log2_n_fact function.

       
      (The Next two are historical, we do all unique operations in memory or fail)

    2. Cost of merging.
      If only one tree is created by Unique no merging will be necessary.
      Otherwise, we model execution of merge_many_buff function and count
      #of merges. (The reason behind this is that number of buffers is small,
      while size of buffers is big and we don't want to loose precision with
      O(x)-style formula)

    3. If only one tree is created by Unique no disk io will happen.
      Otherwise, ceil(key_len*n_keys) disk seeks are necessary. We assume
      these will be random seeks.
*/

double Unique::get_use_cost(uint32_t *, uint32_t nkeys, uint32_t key_size,
                            size_t max_in_memory_size_arg)
{
  ulong max_elements_in_tree;
  ulong last_tree_elems;
  double result;

  max_elements_in_tree= ((ulong) max_in_memory_size_arg /
                         ALIGN_SIZE(sizeof(TREE_ELEMENT)+key_size));

  last_tree_elems= nkeys % max_elements_in_tree;

  /* Calculate cost of creating trees */
  result= 2*log2_n_fact(last_tree_elems + 1.0);
  result /= TIME_FOR_COMPARE_ROWID;

  return result;
}

Unique::~Unique()
{
  delete_tree(&tree);
}


/*
  Clear the tree.
  You must call reset() if you want to reuse Unique after walk().
*/

void
Unique::reset()
{
  reset_tree(&tree);
  assert(elements == 0);
}


/*
  DESCRIPTION
    Walks consecutively through all unique elements:
    if all elements are in memory, then it simply invokes 'tree_walk', else
    all flushed trees are loaded to memory piece-by-piece, pieces are
    sorted, and action is called for each unique value.
    Note: so as merging resets file_ptrs state, this method can change
    internal Unique state to undefined: if you want to reuse Unique after
    walk() you must call reset() first!
  SYNOPSIS
    Unique:walk()
  All params are 'IN':
    action  function-visitor, typed in include/tree.h
            function is called for each unique element
    arg     argument for visitor, which is passed to it on each call
  RETURN VALUE
    0    OK
    <> 0 error
 */

bool Unique::walk(tree_walk_action action, void *walk_action_arg)
{
  return tree_walk(&tree, action, walk_action_arg, left_root_right);
}

/*
  Modify the Table element so that when one calls init_records()
  the rows will be read in priority order.
*/

bool Unique::get(Table *table)
{
  table->sort.found_records= elements+tree.elements_in_tree;

  if ((record_pointers=table->sort.record_pointers= (unsigned char*)
       malloc(size * tree.elements_in_tree)))
  {
    (void) tree_walk(&tree, (tree_walk_action) unique_write_to_ptrs,
                     this, left_root_right);
    return 0;
  }
  /* Not enough memory */
  return 1;
}

} /* namespace drizzled */
