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

/* classes to use when handling where clause */

#ifndef DRIZZLED_OPTIMIZER_RANGE_H
#define DRIZZLED_OPTIMIZER_RANGE_H

#include "drizzled/field.h"
#include "drizzled/item/sum.h"

#include <queue>

class JOIN;
class TRP_ROR_INTERSECT; 
typedef class Item COND;

typedef struct st_handler_buffer HANDLER_BUFFER;

typedef struct st_key_part
{
  uint16_t key;
  uint16_t part;
  /* See KEY_PART_INFO for meaning of the next two: */
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


namespace drizzled
{

namespace optimizer
{

class Parameter;
class SEL_ARG;

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

  virtual bool reverse_sorted() = 0;
  virtual bool unique_key_range()
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
  virtual int get_type() = 0;

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
  virtual void add_keys_and_lengths(String *key_names, String *used_lengths)=0;

  /**
   * Append text representation of quick select structure (what and how is
   * merged) to str. The result is added to "Extra" field in EXPLAIN output.
   *
   * @note
   *
   * This function is implemented only by quick selects that merge other quick
   * selects output and/or can produce output suitable for merging.
   */
  virtual void add_info_string(String *) 
  {}
  
  /**
   * Returns true if any index used by this quick select
   * uses field which is marked in passed bitmap.
   */
  virtual bool is_keys_used(const MyBitmap *fields);
};

struct st_qsel_param;
class QuickRange;
class QuickRangeSelect;

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
  Rowid-Ordered Retrieval (ROR) index intersection quick select.
  This quick select produces intersection of row sequences returned
  by several QuickRangeSelects it "merges".

  All merged QuickRangeSelects must return rowids in rowid order.
  QUICK_ROR_INTERSECT_SELECT will return rows in rowid order, too.

  All merged quick selects retrieve {rowid, covered_fields} tuples (not full
  table records).
  QUICK_ROR_INTERSECT_SELECT retrieves full records if it is not being used
  by QUICK_ROR_INTERSECT_SELECT and all merged quick selects together don't
  cover needed all fields.

  If one of the merged quick selects is a Clustered PK range scan, it is
  used only to filter rowid sequence produced by other merged quick selects.
*/
class QUICK_ROR_INTERSECT_SELECT : public QuickSelectInterface
{
public:
  QUICK_ROR_INTERSECT_SELECT(Session *session, Table *table,
                             bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc);
  ~QUICK_ROR_INTERSECT_SELECT();

  int init();
  int reset(void);
  int get_next();
  bool reverse_sorted()
  {
    return false;
  }
  bool unique_key_range()
  {
    return false;
  }
  int get_type()
  {
    return QS_TYPE_ROR_INTERSECT;
  }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
  bool is_keys_used(const MyBitmap *fields);
  int init_ror_merged_scan(bool reuse_handler);
  bool push_quick_back(QuickRangeSelect *quick_sel_range);

  /**
   * Range quick selects this intersection consists of, not including
   * cpk_quick.
   */
  List<QuickRangeSelect> quick_selects;

  /**
   * Merged quick select that uses Clustered PK, if there is one. This quick
   * select is not used for row retrieval, it is used for row retrieval.
   */
  QuickRangeSelect *cpk_quick;

  MEM_ROOT alloc; /**< Memory pool for this and merged quick selects data. */
  Session *session; /**< Pointer to the current session */
  bool need_to_fetch_row; /**< if true, do retrieve full table records. */
  /** in top-level quick select, true if merged scans where initialized */
  bool scans_inited;
};


/*
 * This function object is defined in drizzled/optimizer/range.cc
 * We need this here for the priority_queue definition in the
 * QUICK_ROR_UNION_SELECT class.
 */
class compare_functor;

/**
  Rowid-Ordered Retrieval index union select.
  This quick select produces union of row sequences returned by several
  quick select it "merges".

  All merged quick selects must return rowids in rowid order.
  QUICK_ROR_UNION_SELECT will return rows in rowid order, too.

  All merged quick selects are set not to retrieve full table records.
  ROR-union quick select always retrieves full records.

*/
class QUICK_ROR_UNION_SELECT : public QuickSelectInterface
{
public:
  QUICK_ROR_UNION_SELECT(Session *session, Table *table);
  ~QUICK_ROR_UNION_SELECT();

  int  init();
  int  reset(void);
  int  get_next();
  bool reverse_sorted()
  {
    return false;
  }
  bool unique_key_range()
  {
    return false;
  }
  int get_type()
  {
    return QS_TYPE_ROR_UNION;
  }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
  bool is_keys_used(const MyBitmap *fields);

  bool push_quick_back(QuickSelectInterface *quick_sel_range);

  List<QuickSelectInterface> quick_selects; /**< Merged quick selects */

  /** Priority queue for merge operation */
  std::priority_queue<QuickSelectInterface *, std::vector<QuickSelectInterface *>, compare_functor > *queue;
  MEM_ROOT alloc; /**< Memory pool for this and merged quick selects data. */

  Session *session; /**< current thread */
  unsigned char *cur_rowid; /**< buffer used in get_next() */
  unsigned char *prev_rowid; /**< rowid of last row returned by get_next() */
  bool have_prev_rowid; /**< true if prev_rowid has valid data */
  uint32_t rowid_length; /**< table rowid length */
private:
  bool scans_inited;
};

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
  bool reverse_sorted()
  {
    return false;
  }
  bool unique_key_range()
  {
    return false;
  }
  int get_type()
  {
    return QS_TYPE_GROUP_MIN_MAX;
  }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
};

/**
 * Executor class for SELECT statements.
 *
 * @details
 *
 * The QuickSelectInterface member variable is the implementor
 * of the SELECT execution.
 */
class SqlSelect : public Sql_alloc 
{
 public:
  QuickSelectInterface *quick; /**< If quick-select used */
  COND *cond; /**< where condition */
  Table	*head;
  IO_CACHE file; /**< Positions to used records */
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
                                             struct table_reference_st *ref,
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

    CAUTION! This function may change session->mem_root to a MEM_ROOT which will be
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
                                   MEM_ROOT *alloc);

uint32_t get_index_for_order(Table *table, order_st *order, ha_rows limit);

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

#endif /* DRIZZLED_OPTIMIZER_RANGE_H */
