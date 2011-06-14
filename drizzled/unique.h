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

#include <drizzled/tree.h>
/*
   Unique -- class for unique (removing of duplicates).
   Puts all values to the TREE. If the tree becomes too big,
   it's dumped to the file. User can request sorted values, or
   just iterate through them. In the last case tree merging is performed in
   memory simultaneously with iteration, so it should be ~2-3x faster.
 */

namespace drizzled
{

namespace internal
{
struct st_io_cache;
}

class Unique : public memory::SqlAlloc
{
  size_t max_in_memory_size;
  TREE tree;
  unsigned char *record_pointers;
  uint32_t size;

public:
  ulong elements;
  Unique(qsort_cmp2 comp_func, void *comp_func_fixed_arg,
	 uint32_t size_arg, size_t max_in_memory_size_arg);
  ~Unique();
  ulong elements_in_tree() { return tree.elements_in_tree; }
  inline bool unique_add(void *ptr)
  {
    return (not tree_insert(&tree, ptr, 0, tree.custom_arg));
  }

  bool get(Table *table);
  static double get_use_cost(uint32_t *buffer, uint32_t nkeys, uint32_t key_size,
                             size_t max_in_memory_size);
  inline static int get_cost_calc_buff_size(ulong nkeys, uint32_t key_size,
                                            size_t sortbuff_size)
  {
    register size_t max_elems_in_tree=
      (1 + sortbuff_size / ALIGN_SIZE(sizeof(TREE_ELEMENT)+key_size));
    return (int) (sizeof(uint32_t)*(1 + nkeys/max_elems_in_tree));
  }

  void reset();
  bool walk(tree_walk_action action, void *walk_action_arg);

  friend int unique_write_to_file(unsigned char* key, uint32_t count, Unique *unique);
  friend int unique_write_to_ptrs(unsigned char* key, uint32_t count, Unique *unique);
};

} /* namespace drizzled */

