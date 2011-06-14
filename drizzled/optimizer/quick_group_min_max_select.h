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

#include <drizzled/optimizer/range.h>

#include <vector>

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
class QuickGroupMinMaxSelect : public QuickSelectInterface
{

private:

  Cursor *cursor; /**< The Cursor used to get data. */
  Join *join; /**< Descriptor of the current query */
  KeyInfo *index_info; /**< The index chosen for data access */
  unsigned char *record; /**< Buffer where the next record is returned. */
  unsigned char *tmp_record; /**< Temporary storage for next_min(), next_max(). */
  unsigned char *group_prefix; /**< Key prefix consisting of the GROUP fields. */
  uint32_t group_prefix_len; /**< Length of the group prefix. */
  uint32_t group_key_parts; /**< A number of keyparts in the group prefix */
  unsigned char *last_prefix; /**< Prefix of the last group for detecting EOF. */
  bool have_min; /**< Specify whether we are computing */
  bool have_max; /**< a MIN, a MAX, or both. */
  bool seen_first_key; /**< Denotes whether the first key was retrieved.*/
  KeyPartInfo *min_max_arg_part; /** The keypart of the only argument field of all MIN/MAX functions. */
  uint32_t min_max_arg_len; /**< The length of the MIN/MAX argument field */
  unsigned char *key_infix; /**< Infix of constants from equality predicates. */
  uint32_t key_infix_len;
  std::vector<QuickRange *> min_max_ranges; /**< Array of range ptrs for the MIN/MAX field. */
  uint32_t real_prefix_len; /**< Length of key prefix extended with key_infix. */
  uint32_t real_key_parts;  /**< A number of keyparts in the above value.      */
  List<Item_sum> *min_functions;
  List<Item_sum> *max_functions;
  List<Item_sum>::iterator *min_functions_it;
  List<Item_sum>::iterator *max_functions_it;

public:

  /*
    The following two members are public to allow easy access from
    GroupMinMaxReadPlan::make_quick()
  */
  memory::Root alloc; /**< Memory pool for this and quick_prefix_select data. */
  QuickRangeSelect *quick_prefix_select; /**< For retrieval of group prefixes. */

private:

  /**
   * Determine the prefix of the next group.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::next_prefix()
   *
   * DESCRIPTION
   * Determine the prefix of the next group that satisfies the query conditions.
   * If there is a range condition referencing the group attributes, use a
   * QuickRangeSelect object to retrieve the *first* key that satisfies the
   * condition. If there is a key infix of constants, append this infix
   * immediately after the group attributes. The possibly extended prefix is
   * stored in this->group_prefix. The first key of the found group is stored in
   * this->record, on which relies this->next_min().
   *
   * RETURN
   * @retval 0                    on success
   * @retval HA_ERR_KEY_NOT_FOUND if there is no key with the formed prefix
   * @retval HA_ERR_END_OF_FILE   if there are no more keys
   * @retval other                if some error occurred
   */
  int next_prefix();

  /**
   * Find the minimal key in a group that satisfies some range conditions for the
   * min/max argument field.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::next_min_in_range()
   *
   * DESCRIPTION
   * Given the sequence of ranges min_max_ranges, find the minimal key that is
   * in the left-most possible range. If there is no such key, then the current
   * group does not have a MIN key that satisfies the WHERE clause. If a key is
   * found, its value is stored in this->record.
   *
   * RETURN
   * @retval 0                    on success
   * @retval HA_ERR_KEY_NOT_FOUND if there is no key with the given prefix in any of the ranges
   * @retval HA_ERR_END_OF_FILE   - "" -
   * @retval other                if some error
   */
  int next_min_in_range();

  /**
   * Find the maximal key in a group that satisfies some range conditions for the
   * min/max argument field.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::next_max_in_range()
   *
   * DESCRIPTION
   * Given the sequence of ranges min_max_ranges, find the maximal key that is
   * in the right-most possible range. If there is no such key, then the current
   * group does not have a MAX key that satisfies the WHERE clause. If a key is
   * found, its value is stored in this->record.
   *
   * RETURN
   * @retval 0                    on success
   * @retval HA_ERR_KEY_NOT_FOUND if there is no key with the given prefix in any of the ranges
   * @retval HA_ERR_END_OF_FILE   - "" -
   * @retval other                if some error
   */
  int next_max_in_range();

  /**
   * Retrieve the minimal key in the next group.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::next_min()
   *
   * DESCRIPTION
   * Find the minimal key within this group such that the key satisfies the query
   * conditions and NULL semantics. The found key is loaded into this->record.
   *
   * IMPLEMENTATION
   * Depending on the values of min_max_ranges.elements, key_infix_len, and
   * whether there is a  NULL in the MIN field, this function may directly
   * return without any data access. In this case we use the key loaded into
   * this->record by the call to this->next_prefix() just before this call.
   *
   * RETURN
   * @retval 0                    on success
   * @retval HA_ERR_KEY_NOT_FOUND if no MIN key was found that fulfills all conditions.
   * @retval HA_ERR_END_OF_FILE   - "" -
   * @retval other                if some error occurred
   */
  int next_min();

  /**
   * Retrieve the maximal key in the next group.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::next_max()
   *
   * DESCRIPTION
   * Lookup the maximal key of the group, and store it into this->record.
   *
   * RETURN
   * @retval 0                    on success
   * @retval HA_ERR_KEY_NOT_FOUND if no MAX key was found that fulfills all conditions.
   * @retval HA_ERR_END_OF_FILE	 - "" -
   * @retval other                if some error occurred
   */
  int next_max();

  /**
   * Update all MIN function results with the newly found value.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::update_min_result()
   *
   * DESCRIPTION
   * The method iterates through all MIN functions and updates the result value
   * of each function by calling Item_sum::reset(), which in turn picks the new
   * result value from this->head->getInsertRecord(), previously updated by
   * next_min(). The updated value is stored in a member variable of each of the
   * Item_sum objects, depending on the value type.
   *
   * IMPLEMENTATION
   * The update must be done separately for MIN and MAX, immediately after
   * next_min() was called and before next_max() is called, because both MIN and
   * MAX take their result value from the same buffer this->head->getInsertRecord()
   * (i.e.  this->record).
   *
   * RETURN
   * None
   */
  void update_min_result();

  /**
   * Update all MAX function results with the newly found value.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::update_max_result()
   *
   * DESCRIPTION
   * The method iterates through all MAX functions and updates the result value
   * of each function by calling Item_sum::reset(), which in turn picks the new
   * result value from this->head->getInsertRecord(), previously updated by
   * next_max(). The updated value is stored in a member variable of each of the
   * Item_sum objects, depending on the value type.
   *
   * IMPLEMENTATION
   * The update must be done separately for MIN and MAX, immediately after
   * next_max() was called, because both MIN and MAX take their result value
   * from the same buffer this->head->getInsertRecord() (i.e.  this->record).
   *
   * RETURN
   * None
   */
  void update_max_result();

public:

  /*
     Construct new quick select for group queries with min/max.

     SYNOPSIS
     QuickGroupMinMaxSelect::QuickGroupMinMaxSelect()
     table             The table being accessed
     join              Descriptor of the current query
     have_min          true if the query selects a MIN function
     have_max          true if the query selects a MAX function
     min_max_arg_part  The only argument field of all MIN/MAX functions
     group_prefix_len  Length of all key parts in the group prefix
     prefix_key_parts  All key parts in the group prefix
     index_info        The index chosen for data access
     use_index         The id of index_info
     read_cost         Cost of this access method
     records           Number of records returned
     key_infix_len     Length of the key infix appended to the group prefix
     key_infix         Infix of constants from equality predicates
     parent_alloc      Memory pool for this and quick_prefix_select data

     RETURN
     None
   */
  QuickGroupMinMaxSelect(Table *table, 
                         Join *join, 
                         bool have_min,
                         bool have_max, 
                         KeyPartInfo *min_max_arg_part,
                         uint32_t group_prefix_len, 
                         uint32_t group_key_parts,
                         uint32_t used_key_parts, 
                         KeyInfo *index_info,
                         uint use_index, 
                         double read_cost, 
                         ha_rows records,
                         uint key_infix_len, 
                         unsigned char *key_infix,
                         memory::Root *parent_alloc);

  ~QuickGroupMinMaxSelect();

  /**
   * Eventually create and add a new quick range object.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::add_range()
   * @param[in] sel_range  Range object from which a new object is created
   *
   * NOTES
   * Construct a new QuickRange object from a SEL_ARG object, and
   * add it to the array min_max_ranges. If sel_arg is an infinite
   * range, e.g. (x < 5 or x > 4), then skip it and do not construct
   * a quick range.
   *
   * RETURN
   * @retval false on success
   * @retval true  otherwise
   */
  bool add_range(SEL_ARG *sel_range);

  /**
   * Determine the total number and length of the keys that will be used for
   * index lookup.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::update_key_stat()
   *
   * DESCRIPTION
   * The total length of the keys used for index lookup depends on whether
   * there are any predicates referencing the min/max argument, and/or if
   * the min/max argument field can be NULL.
   * This function does an optimistic analysis whether the search key might
   * be extended by a constant for the min/max keypart. It is 'optimistic'
   * because during actual execution it may happen that a particular range
   * is skipped, and then a shorter key will be used. However this is data
   * dependent and can't be easily estimated here.
   *
   * RETURN
   * None
   */
  void update_key_stat();

  /**
   * Opens the ranges if there are more conditions in quick_prefix_select than
   * the ones used for jumping through the prefixes.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::adjust_prefix_ranges()
   *
   * NOTES
   * quick_prefix_select is made over the conditions on the whole key.
   * It defines a number of ranges of length x.
   * However when jumping through the prefixes we use only the the first
   * few most significant keyparts in the range key. However if there
   * are more keyparts to follow the ones we are using we must make the
   * condition on the key inclusive (because x < "ab" means
   * x[0] < 'a' OR (x[0] == 'a' AND x[1] < 'b').
   * To achive the above we must turn off the NEAR_MIN/NEAR_MAX
   */
  void adjust_prefix_ranges();

  bool alloc_buffers();

  /**
   * Do post-constructor initialization.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::init()
   *
   * DESCRIPTION
   * The method performs initialization that cannot be done in the constructor
   * such as memory allocations that may fail. It allocates memory for the
   * group prefix and inifix buffers, and for the lists of MIN/MAX item to be
   * updated during execution.
   *
   * RETURN
   * @retval 0      OK
   * @retval other  Error code
   */
  int init();

  /**
   * Initialize a quick group min/max select for key retrieval.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::reset()
   *
   * DESCRIPTION
   * Initialize the index chosen for access and find and store the prefix
   * of the last group. The method is expensive since it performs disk access.
   *
   * RETURN
   * @retval 0      OK
   * @retval other  Error code
   */
  int reset();

  /**
   * Get the next key containing the MIN and/or MAX key for the next group.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::get_next()
   *
   * DESCRIPTION
   * The method finds the next subsequent group of records that satisfies the
   * query conditions and finds the keys that contain the MIN/MAX values for
   * the key part referenced by the MIN/MAX function(s). Once a group and its
   * MIN/MAX values are found, store these values in the Item_sum objects for
   * the MIN/MAX functions. The rest of the values in the result row are stored
   * in the Item_field::result_field of each select field. If the query does
   * not contain MIN and/or MAX functions, then the function only finds the
   * group prefix, which is a query answer itself.
   *
   * NOTES
   * If both MIN and MAX are computed, then we use the fact that if there is
   * no MIN key, there can't be a MAX key as well, so we can skip looking
   * for a MAX key in this case.
   *
   * RETURN
   * @retval 0                  on success
   * @retval HA_ERR_END_OF_FILE if returned all keys
   * @retval other              if some error occurred
   */
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

  /**
   * Append comma-separated list of keys this quick select uses to key_names;
   * append comma-separated list of corresponding used lengths to used_lengths.
   *
   * SYNOPSIS
   * QuickGroupMinMaxSelect::add_keys_and_lengths()
   * @param[out] key_names Names of used indexes
   * @param[out] used_lengths Corresponding lengths of the index names
   *
   * DESCRIPTION
   * This method is used by select_describe to extract the names of the
   * indexes used by a quick select.
   */
  void add_keys_and_lengths(std::string *key_names, std::string *used_lengths);

};


} /* namespace optimizer */

} /* namespace drizzled */

