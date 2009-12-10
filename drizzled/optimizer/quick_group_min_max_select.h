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

#ifndef DRIZZLED_OPTIMIZER_QUICK_GROUP_MIN_MAX_SELECT_H
#define DRIZZLED_OPTIMIZER_QUICK_GROUP_MIN_MAX_SELECT_H

#include "drizzled/optimizer/range.h"

namespace drizzled
{

namespace optimizer
{

/**
  Index scan for GROUP-BY queries with MIN/MAX aggregate functions.

  This class provides a specialized index access method for GROUP-BY queries
  of the forms:

       SELECT A_1,...,A_k, [B_1,...,B_m], [MIN(C)], [MAX(C)]
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND EQ(B_1,...,B_m)]
         [AND PC(C)]
         [AND PA(A_i1,...,A_iq)]
       GROUP BY A_1,...,A_k;

    or

       SELECT DISTINCT A_i1,...,A_ik
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND PA(A_i1,...,A_iq)];

  where all selected fields are parts of the same index.
  The class of queries that can be processed by this quick select is fully
  specified in the description of get_best_trp_group_min_max() in optimizer/range.cc.

  The get_next() method directly produces result tuples, thus obviating the
  need to call end_send_group() because all grouping is already done inside
  get_next().

  Since one of the requirements is that all select fields are part of the same
  index, this class produces only index keys, and not complete records.
*/
class QUICK_GROUP_MIN_MAX_SELECT : public QuickSelectInterface
{
private:
  Cursor *cursor; /**< The Cursor used to get data. */
  JOIN *join; /**< Descriptor of the current query */
  KEY *index_info; /**< The index chosen for data access */
  unsigned char *record; /**< Buffer where the next record is returned. */
  unsigned char *tmp_record; /**< Temporary storage for next_min(), next_max(). */
  unsigned char *group_prefix; /**< Key prefix consisting of the GROUP fields. */
  uint32_t group_prefix_len; /**< Length of the group prefix. */
  uint32_t group_key_parts; /**< A number of keyparts in the group prefix */
  unsigned char *last_prefix; /**< Prefix of the last group for detecting EOF. */
  bool have_min; /**< Specify whether we are computing */
  bool have_max; /**< a MIN, a MAX, or both. */
  bool seen_first_key; /**< Denotes whether the first key was retrieved.*/
  KEY_PART_INFO *min_max_arg_part; /** The keypart of the only argument field of all MIN/MAX functions. */
  uint32_t min_max_arg_len; /**< The length of the MIN/MAX argument field */
  unsigned char *key_infix; /**< Infix of constants from equality predicates. */
  uint32_t key_infix_len;
  DYNAMIC_ARRAY min_max_ranges; /**< Array of range ptrs for the MIN/MAX field. */
  uint32_t real_prefix_len; /**< Length of key prefix extended with key_infix. */
  uint32_t real_key_parts;  /**< A number of keyparts in the above value.      */
  List<Item_sum> *min_functions;
  List<Item_sum> *max_functions;
  List_iterator<Item_sum> *min_functions_it;
  List_iterator<Item_sum> *max_functions_it;
public:
  /*
    The following two members are public to allow easy access from
    TRP_GROUP_MIN_MAX::make_quick()
  */
  MEM_ROOT alloc; /**< Memory pool for this and quick_prefix_select data. */
  QuickRangeSelect *quick_prefix_select; /**< For retrieval of group prefixes. */
private:
  int next_prefix();
  int next_min_in_range();
  int next_max_in_range();
  int next_min();
  int next_max();
  void update_min_result();
  void update_max_result();
public:
  QUICK_GROUP_MIN_MAX_SELECT(Table *table, JOIN *join, bool have_min,
                             bool have_max, KEY_PART_INFO *min_max_arg_part,
                             uint32_t group_prefix_len, uint32_t group_key_parts,
                             uint32_t used_key_parts, KEY *index_info, uint
                             use_index, double read_cost, ha_rows records, uint
                             key_infix_len, unsigned char *key_infix, MEM_ROOT
                             *parent_alloc);
  ~QUICK_GROUP_MIN_MAX_SELECT();
  bool add_range(SEL_ARG *sel_range);
  void update_key_stat();
  void adjust_prefix_ranges();
  bool alloc_buffers();
  int init();
  int reset();
  int get_next();

  bool reverse_sorted() const
  {
    return false;
  }

  bool unique_key_range() const
  {
    return false;
  }

  int get_type() const
  {
    return QS_TYPE_GROUP_MIN_MAX;
  }

  void add_keys_and_lengths(String *key_names, String *used_lengths);
};


} /* namespace optimizer */

} /* namespace drizzled */

#endif /* DRIZZLED_OPTIMIZER_QUICK_GROUP_MIN_MAX_SELECT_H */
