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

/* classes to use when handling where clause */

#pragma once

#include <drizzled/field.h>
#include <drizzled/item/sum.h>
#include <drizzled/table_reference.h>

#include <queue>

#include <boost/dynamic_bitset.hpp>

namespace drizzled {

typedef struct st_key_part
{
  uint16_t key;
  uint16_t part;
  /* See KeyPartInfo for meaning of the next two: */
  uint16_t store_length;
  uint16_t length;
  uint8_t null_bit;
  /**
    Keypart flags (0 when this structure is used by partition pruning code
    for fake partitioning index description)
  */
  uint8_t flag;
  Field *field;
} KEY_PART;


namespace optimizer {

/**
  Quick select interface.
  This class is a parent for all QUICK_*_SELECT classes.

  The usage scenario is as follows:
  1. Create quick select
    quick= new QUICK_XXX_SELECT(...);

  2. Perform lightweight initialization. This can be done in 2 ways:
  2.a: Regular initialization
    if (quick->init())
    {
      //the only valid action after failed init() call is delete
      delete quick;
    }
  2.b: Special initialization for quick selects merged by QUICK_ROR_*_SELECT
    if (quick->init_ror_merged_scan())
      delete quick;

  3. Perform zero, one, or more scans.
    while (...)
    {
      // initialize quick select for scan. This may allocate
      // buffers and/or prefetch rows.
      if (quick->reset())
      {
        //the only valid action after failed reset() call is delete
        delete quick;
        //abort query
      }

      // perform the scan
      do
      {
        res= quick->get_next();
      } while (res && ...)
    }

  4. Delete the select:
    delete quick;

*/
class QuickSelectInterface
{
public:
  bool sorted;
  ha_rows records; /**< estimate of # of records to be retrieved */
  double read_time; /**< time to perform this retrieval */
  Table *head;
  /**
    Index this quick select uses, or MAX_KEY for quick selects
    that use several indexes
  */
  uint32_t index;
  /**
    Total length of first used_key_parts parts of the key.
    Applicable if index!= MAX_KEY.
  */
  uint32_t max_used_key_length;
  /**
    Maximum number of (first) key parts this quick select uses for retrieval.
    eg. for "(key1p1=c1 AND key1p2=c2) OR key1p1=c2" used_key_parts == 2.
    Applicable if index!= MAX_KEY.

    For QUICK_GROUP_MIN_MAX_SELECT it includes MIN/MAX argument keyparts.
  */
  uint32_t used_key_parts;
  /**
   * The rowid of last row retrieved by this quick select. This is used only when
   * doing ROR-index_merge selects
   */
  unsigned char *last_rowid;

  /**
   * Table record buffer used by this quick select.
   */
  unsigned char *record;

  QuickSelectInterface();
  virtual ~QuickSelectInterface(){};

  /**
   * Do post-constructor initialization.
   *
   * @details
   *
   * Performs initializations that should have been in constructor if
   * it was possible to return errors from constructors. The join optimizer may
   * create and then delete quick selects without retrieving any rows so init()
   * must not contain any IO or CPU intensive code.
   *
   * If init() call fails the only valid action is to delete this quick select,
   * reset() and get_next() must not be called.
   *
   * @retval
   *  0      OK
   * @retval
   *  other  Error code
  */
  virtual int init() = 0;

  /**
   * Initializes quick select for row retrieval.
   *
   * @details
   *
   * Should be called when it is certain that row retrieval will be
   * necessary. This call may do heavyweight initialization like buffering first
   * N records etc. If reset() call fails get_next() must not be called.
   * Note that reset() may be called several times if
   * - the quick select is executed in a subselect
   * - a JOIN buffer is used
   *
   * @retval 
   *  0      OK
   * @retval
   *  other  Error code
   */
  virtual int reset(void) = 0;
  /** Gets next record to retrieve */
  virtual int get_next() = 0;

  /** Range end should be called when we have looped over the whole index */
  virtual void range_end() {}

  virtual bool reverse_sorted() const = 0;

  virtual bool unique_key_range() const
  {
    return false;
  }

  enum 
  {
    QS_TYPE_RANGE= 0,
    QS_TYPE_INDEX_MERGE= 1,
    QS_TYPE_RANGE_DESC= 2,
    QS_TYPE_ROR_INTERSECT= 4,
    QS_TYPE_ROR_UNION= 5,
    QS_TYPE_GROUP_MIN_MAX= 6
  };

  /** Returns the type of this quick select - one of the QS_TYPE_* values */
  virtual int get_type() const = 0;

  /**
   * Initialize this quick select as a merged scan inside a ROR-union or a ROR-
   * intersection scan. The caller must not additionally call init() if this
   * function is called.
   *
   * @param If true, the quick select may use table->Cursor,
   *        otherwise it must create and use a separate Cursor
   *        object.
   *
   * @retval
   *  0     Ok
   * @retval
   *  other Error
   */
  virtual int init_ror_merged_scan(bool)
  {
    assert(0);
    return 1;
  }

  /**
   * Save ROWID of last retrieved row in file->ref. This used in ROR-merging.
   */
  virtual void save_last_pos(){};

  /**
   * Append comma-separated list of keys this quick select uses to key_names;
   * append comma-separated list of corresponding used lengths to used_lengths.
   * 
   * @note This is used by during explain plan.
   */
  virtual void add_keys_and_lengths(std::string *key_names,
                                    std::string *used_lengths)=0;

  /**
   * Append text representation of quick select structure (what and how is
   * merged) to str. The result is added to "Extra" field in EXPLAIN output.
   *
   * @note
   *
   * This function is implemented only by quick selects that merge other quick
   * selects output and/or can produce output suitable for merging.
   */
  virtual void add_info_string(std::string *)
  {}

  /**
   * Returns true if any index used by this quick select
   * uses field which is marked in passed bitmap.
   */
  virtual bool is_keys_used(const boost::dynamic_bitset<>& fields);
};

/**
 * MRR range sequence, array<QuickRange> implementation: sequence traversal
 * context.
 */
typedef struct st_quick_range_seq_ctx
{
  QuickRange **first;
  QuickRange **cur;
  QuickRange **last;
} QuickRangeSequenceContext;

range_seq_t quick_range_seq_init(void *init_param, uint32_t n_ranges, uint32_t flags);

uint32_t quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);

/**
 * Executor class for SELECT statements.
 *
 * @details
 *
 * The QuickSelectInterface member variable is the implementor
 * of the SELECT execution.
 */
class SqlSelect : public memory::SqlAlloc 
{
 public:
  QuickSelectInterface *quick; /**< If quick-select used */
  COND *cond; /**< where condition */
  Table	*head;
  internal::io_cache_st *file; /**< Positions to used records */
  ha_rows records; /**< Records in use if read from file */
  double read_time; /**< Time to read rows */
  key_map quick_keys; /**< Possible quick keys */
  key_map needed_reg; /**< Possible quick keys after prev tables. */
  table_map const_tables;
  table_map read_tables;
  bool free_cond;

  SqlSelect();
  ~SqlSelect();
  void cleanup();
  bool check_quick(Session *session, bool force_quick_range, ha_rows limit);
  bool skip_record();
  int test_quick_select(Session *session, key_map keys, table_map prev_tables,
                        ha_rows limit, bool force_quick_range,
                        bool ordered_output);
};

QuickRangeSelect *get_quick_select_for_ref(Session *session, 
                                           Table *table,
                                           table_reference_st *ref,
                                           ha_rows records);

/*
  Create a QuickRangeSelect from given key and SEL_ARG tree for that key.

  SYNOPSIS
    get_quick_select()
      param
      idx            Index of used key in param->key.
      key_tree       SEL_ARG tree for the used key
      mrr_flags      MRR parameter for quick select
      mrr_buf_size   MRR parameter for quick select
      parent_alloc   If not NULL, use it to allocate memory for
                     quick select data. Otherwise use quick->alloc.
  NOTES
    The caller must call QUICK_SELECT::init for returned quick select.

    CAUTION! This function may change session->mem_root to a memory::Root which will be
    deallocated when the returned quick select is deleted.

  RETURN
    NULL on error
    otherwise created quick select
*/
QuickRangeSelect *get_quick_select(Parameter *param,
                                   uint32_t index,
                                   SEL_ARG *key_tree, 
                                   uint32_t mrr_flags,
                                   uint32_t mrr_buf_size, 
                                   memory::Root *alloc);

uint32_t get_index_for_order(Table *table, Order *order, ha_rows limit);

SqlSelect *make_select(Table *head, 
                       table_map const_tables,
                       table_map read_tables, 
                       COND *conds,
                       bool allow_null_cond,
                       int *error);

bool get_quick_keys(Parameter *param, 
                    QuickRangeSelect *quick,
                    KEY_PART *key,
                    SEL_ARG *key_tree, 
                    unsigned char *min_key,
                    uint32_t min_key_flag,
                    unsigned char *max_key,
                    uint32_t max_key_flag);

} /* namespace optimizer */

} /* namespace drizzled */

