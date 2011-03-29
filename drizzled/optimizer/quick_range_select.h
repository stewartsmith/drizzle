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

#include <drizzled/dynamic_array.h>
#include <drizzled/optimizer/range.h>

#include <boost/dynamic_bitset.hpp>
#include <vector>

namespace drizzled {
namespace optimizer {

/**
 * Quick select that does a range scan on a single key. 
 *
 * The records are returned in key order.
 * 
 */
class QuickRangeSelect : public QuickSelectInterface
{
protected:
  Cursor *cursor;
  DYNAMIC_ARRAY ranges; /**< ordered array of range ptrs */

  /** Members to deal with case when this quick select is a ROR-merged scan */
  bool in_ror_merged_scan;
  boost::dynamic_bitset<> *column_bitmap;
  boost::dynamic_bitset<> *save_read_set;
  boost::dynamic_bitset<> *save_write_set;
  bool free_file; /**< True when this->file is "owned" by this quick select */

  /* Range pointers to be used when not using MRR interface */
  QuickRange **cur_range; /**< current element in ranges  */
  QuickRange *last_range;

  /** Members needed to use the MRR interface */
  QuickRangeSequenceContext qr_traversal_ctx;
  uint32_t mrr_buf_size; /**< copy from session->variables.read_rnd_buff_size */

  /** Info about index we're scanning */
  KEY_PART *key_parts;
  KeyPartInfo *key_part_info;

  bool dont_free; /**< Used by QuickSelectDescending */

  /**
   * Compare if found key is over max-value
   * @return 0 if key <= range->max_key
   * @todo: Figure out why can't this function be as simple as cmp_prev().
   */
  int cmp_next(QuickRange *range);

  /**
   * @return 0 if found key is inside range (found key >= range->min_key).
   */
  int cmp_prev(QuickRange *range);

  /**
   * Check if current row will be retrieved by this QuickRangeSelect
   *
   * NOTES
   * It is assumed that currently a scan is being done on another index
   * which reads all necessary parts of the index that is scanned by this
   * quick select.
   * The implementation does a binary search on sorted array of disjoint
   * ranges, without taking size of range into account.
   *
   * This function is used to filter out clustered PK scan rows in
   * index_merge quick select.
   *
   * RETURN
   * @retval true  if current row will be retrieved by this quick select
   * false if not
   */
  bool row_in_ranges();

public:

  uint32_t mrr_flags; /**< Flags to be used with MRR interface */

  memory::Root alloc;

  QuickRangeSelect(Session *session,
                     Table *table,
                     uint32_t index_arg,
                     bool no_alloc,
                     memory::Root *parent_alloc);

  ~QuickRangeSelect();

  int init();

  int reset(void);

  /**
   * Get next possible record using quick-struct.
   *
   * SYNOPSIS
   * QuickRangeSelect::get_next()
   *
   * NOTES
   * Record is read into table->getInsertRecord()
   *
   * RETURN
   * @retval 0			Found row
   * @retval HA_ERR_END_OF_FILE	No (more) rows in range
   * @retaval #	Error code
   */
  int get_next();

  void range_end();

  /**
   * Get the next record with a different prefix.
   *
   * SYNOPSIS
   * QuickRangeSelect::get_next_prefix()
   * @param[in] prefix_length  length of cur_prefix
   * @param[in] cur_prefix     prefix of a key to be searched for
   *
   * DESCRIPTION
   * Each subsequent call to the method retrieves the first record that has a
   * prefix with length prefix_length different from cur_prefix, such that the
   * record with the new prefix is within the ranges described by
   * this->ranges. The record found is stored into the buffer pointed by
   * this->record.
   * The method is useful for GROUP-BY queries with range conditions to
   * discover the prefix of the next group that satisfies the range conditions.
   *
   * @todo
   * This method is a modified copy of QuickRangeSelect::get_next(), so both
   * methods should be unified into a more general one to reduce code
   * duplication.
   *
   * RETURN
   * @retval 0                  on success
   * @retval HA_ERR_END_OF_FILE if returned all keys
   * @retval other              if some error occurred
   */
  int get_next_prefix(uint32_t prefix_length,
                      key_part_map keypart_map,
                      unsigned char *cur_prefix);

  bool reverse_sorted() const
  {
    return false;
  }

  /**
   * @return true if there is only one range and this uses the whole primary key
   */
  bool unique_key_range() const;

  /**
   * Initialize this quick select to be a ROR-merged scan.
   *
   * SYNOPSIS
   * QuickRangeSelect::init_ror_merged_scan()
   * @param[in] reuse_handler If true, use head->cursor, otherwise create a separate Cursor object
   *
   * NOTES
   * This function creates and prepares for subsequent use a separate Cursor
   * object if it can't reuse head->cursor. The reason for this is that during
   * ROR-merge several key scans are performed simultaneously, and a single
   * Cursor is only capable of preserving context of a single key scan.
   *
   * In ROR-merge the quick select doing merge does full records retrieval,
   * merged quick selects read only keys.
   *
   * RETURN
   * @reval 0  ROR child scan initialized, ok to use.
   * @retval 1  error
   */
  int init_ror_merged_scan(bool reuse_handler);

  void save_last_pos();

  int get_type() const
  {
    return QS_TYPE_RANGE;
  }

  void add_keys_and_lengths(std::string *key_names, std::string *used_lengths);

  void add_info_string(std::string *str);

  void resetCursor()
  {
    cursor= NULL;
  }

private:

  /* Used only by QuickSelectDescending */
  QuickRangeSelect(const QuickRangeSelect& org) : QuickSelectInterface()
  {
    memmove(this, &org, sizeof(*this));
    /*
      Use default MRR implementation for reverse scans. No table engine
      currently can do an MRR scan with output in reverse index order.
    */
    mrr_flags|= HA_MRR_USE_DEFAULT_IMPL;
    mrr_buf_size= 0;
  }

  friend class ::drizzled::RorIntersectReadPlan; 

  friend
  QuickRangeSelect *get_quick_select_for_ref(Session *session, Table *table,
                                             struct table_reference_st *ref,
                                             ha_rows records);

  friend bool get_quick_keys(Parameter *param, 
                             QuickRangeSelect *quick,
                             KEY_PART *key, 
                             SEL_ARG *key_tree,
                             unsigned char *min_key, 
                             uint32_t min_key_flag,
                             unsigned char *max_key, 
                             uint32_t max_key_flag);

  friend QuickRangeSelect *get_quick_select(Parameter *,
                                            uint32_t idx,
                                            SEL_ARG *key_tree,
                                            uint32_t mrr_flags,
                                            uint32_t mrr_buf_size,
                                            memory::Root *alloc);
  friend class QuickSelectDescending;

  friend class QuickIndexMergeSelect;

  friend class QuickRorIntersectSelect;

  friend class QuickGroupMinMaxSelect;

  friend uint32_t quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);

  friend range_seq_t quick_range_seq_init(void *init_param,
                                          uint32_t n_ranges, 
                                          uint32_t flags);

  friend void select_describe(Join *join, 
                              bool need_tmp_table, 
                              bool need_order,
                              bool distinct,
                              const char *message);
};

class QuickSelectDescending : public QuickRangeSelect
{
public:

  QuickSelectDescending(QuickRangeSelect *q, 
                        uint32_t used_key_parts,
                        bool *create_err);

  int get_next();

  bool reverse_sorted() const
  { 
    return true; 
  }

  int get_type() const
  { 
    return QS_TYPE_RANGE_DESC;
  }

private:

  bool range_reads_after_key(QuickRange *range);

  int reset(void) 
  { 
    rev_it= rev_ranges.begin();
    return QuickRangeSelect::reset();
  }

  std::vector<QuickRange *> rev_ranges;

  std::vector<QuickRange *>::iterator rev_it;

};

} /* namespace optimizer */

} /* namespace drizzled */

