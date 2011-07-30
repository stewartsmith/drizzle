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

/*
  TODO:
  Fix that MAYBE_KEY are stored in the tree so that we can detect use
  of full hash keys for queries like:

  select s.id, kws.keyword_id from sites as s,kws where s.id=kws.site_id and kws.keyword_id in (204,205);

*/

/*
  This cursor contains:

  RangeAnalysisModule
    A module that accepts a condition, index (or partitioning) description,
    and builds lists of intervals (in index/partitioning space), such that
    all possible records that match the condition are contained within the
    intervals.
    The entry point for the range analysis module is get_mm_tree() function.

    The lists are returned in form of complicated structure of interlinked
    optimizer::SEL_TREE/optimizer::SEL_IMERGE/SEL_ARG objects.
    See quick_range_seq_next, find_used_partitions for examples of how to walk
    this structure.
    All direct "users" of this module are located within this cursor, too.


  Range/index_merge/groupby-minmax optimizer module
    A module that accepts a table, condition, and returns
     - a QUICK_*_SELECT object that can be used to retrieve rows that match
       the specified condition, or a "no records will match the condition"
       statement.

    The module entry points are
      test_quick_select()
      get_quick_select_for_ref()


  Record retrieval code for range/index_merge/groupby-min-max.
    Implementations of QUICK_*_SELECT classes.

  KeyTupleFormat
  ~~~~~~~~~~~~~~
  The code in this cursor (and elsewhere) makes operations on key value tuples.
  Those tuples are stored in the following format:

  The tuple is a sequence of key part values. The length of key part value
  depends only on its type (and not depends on the what value is stored)

    KeyTuple: keypart1-data, keypart2-data, ...

  The value of each keypart is stored in the following format:

    keypart_data: [isnull_byte] keypart-value-bytes

  If a keypart may have a NULL value (key_part->field->real_maybe_null() can
  be used to check this), then the first byte is a NULL indicator with the
  following valid values:
    1  - keypart has NULL value.
    0  - keypart has non-NULL value.

  <questionable-statement> If isnull_byte==1 (NULL value), then the following
  keypart->length bytes must be 0.
  </questionable-statement>

  keypart-value-bytes holds the value. Its format depends on the field type.
  The length of keypart-value-bytes may or may not depend on the value being
  stored. The default is that length is static and equal to
  KeyPartInfo::length.

  Key parts with (key_part_flag & HA_BLOB_PART) have length depending of the
  value:

     keypart-value-bytes: value_length value_bytes

  The value_length part itself occupies HA_KEY_BLOB_LENGTH=2 bytes.

  See key_copy() and key_restore() for code to move data between index tuple
  and table record

  CAUTION: the above description is only sergefp's understanding of the
           subject and may omit some details.
*/

#include <config.h>

#include <math.h>
#include <float.h>

#include <string>
#include <vector>
#include <algorithm>

#include <boost/dynamic_bitset.hpp>

#include <drizzled/check_stack_overrun.h>
#include <drizzled/error.h>
#include <drizzled/field/num.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/optimizer/cost_vector.h>
#include <drizzled/optimizer/quick_group_min_max_select.h>
#include <drizzled/optimizer/quick_index_merge_select.h>
#include <drizzled/optimizer/quick_range.h>
#include <drizzled/optimizer/quick_range_select.h>
#include <drizzled/optimizer/quick_ror_intersect_select.h>
#include <drizzled/optimizer/quick_ror_union_select.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/optimizer/range_param.h>
#include <drizzled/optimizer/sel_arg.h>
#include <drizzled/optimizer/sel_imerge.h>
#include <drizzled/optimizer/sel_tree.h>
#include <drizzled/optimizer/sum.h>
#include <drizzled/optimizer/table_read_plan.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/records.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_select.h>
#include <drizzled/table_reference.h>
#include <drizzled/session.h>
#include <drizzled/key.h>
#include <drizzled/unique.h>
#include <drizzled/temporal.h> /* Needed in get_mm_leaf() for timestamp -> datetime comparisons */
#include <drizzled/sql_lex.h>
#include <drizzled/system_variables.h>

using namespace std;

namespace drizzled {

#define HA_END_SPACE_KEY 0

/*
  Convert double value to #rows. Currently this does floor(), and we
  might consider using round() instead.
*/
static inline ha_rows double2rows(double x)
{
    return static_cast<ha_rows>(x);
}

static unsigned char is_null_string[2]= {1,0};


/**
  Get cost of reading nrows table records in a "disk sweep"

  A disk sweep read is a sequence of Cursor->rnd_pos(rowid) calls that made
  for an ordered sequence of rowids.

  We assume hard disk IO. The read is performed as follows:

   1. The disk head is moved to the needed cylinder
   2. The controller waits for the plate to rotate
   3. The data is transferred

  Time to do #3 is insignificant compared to #2+#1.

  Time to move the disk head is proportional to head travel distance.

  Time to wait for the plate to rotate depends on whether the disk head
  was moved or not.

  If disk head wasn't moved, the wait time is proportional to distance
  between the previous block and the block we're reading.

  If the head was moved, we don't know how much we'll need to wait for the
  plate to rotate. We assume the wait time to be a variate with a mean of
  0.5 of full rotation time.

  Our cost units are "random disk seeks". The cost of random disk seek is
  actually not a constant, it depends one range of cylinders we're going
  to access. We make it constant by introducing a fuzzy concept of "typical
  datafile length" (it's fuzzy as it's hard to tell whether it should
  include index cursor, temp.tables etc). Then random seek cost is:

    1 = half_rotation_cost + move_cost * 1/3 * typical_data_file_length

  We define half_rotation_cost as DISK_SEEK_BASE_COST=0.9.

  @param table             Table to be accessed
  @param nrows             Number of rows to retrieve
  @param interrupted       true <=> Assume that the disk sweep will be
                           interrupted by other disk IO. false - otherwise.
  @param cost         OUT  The cost.
*/

static void get_sweep_read_cost(Table *table,
                                ha_rows nrows,
                                bool interrupted,
                                optimizer::CostVector *cost)
{
  cost->zero();
  if (table->cursor->primary_key_is_clustered())
  {
    cost->setIOCount(table->cursor->read_time(table->getShare()->getPrimaryKey(),
                                             static_cast<uint32_t>(nrows),
                                             nrows));
  }
  else
  {
    double n_blocks=
      ceil(static_cast<double>(table->cursor->stats.data_file_length) / IO_SIZE);
    double busy_blocks=
      n_blocks * (1.0 - pow(1.0 - 1.0/n_blocks, static_cast<double>(nrows)));
    if (busy_blocks < 1.0)
      busy_blocks= 1.0;

    cost->setIOCount(busy_blocks);

    if (! interrupted)
    {
      /* Assume reading is done in one 'sweep' */
      cost->setAvgIOCost((DISK_SEEK_BASE_COST +
                          DISK_SEEK_PROP_COST*n_blocks/busy_blocks));
    }
  }
}

static optimizer::SEL_TREE * get_mm_parts(optimizer::RangeParameter *param,
                               COND *cond_func,
                               Field *field,
			                         Item_func::Functype type,
                               Item *value,
			                         Item_result cmp_type);

static optimizer::SEL_ARG *get_mm_leaf(optimizer::RangeParameter *param,
                                       COND *cond_func,
                                       Field *field,
                                       KEY_PART *key_part,
			                                 Item_func::Functype type,
                                       Item *value);

static optimizer::SEL_TREE *get_mm_tree(optimizer::RangeParameter *param, COND *cond);

static bool is_key_scan_ror(optimizer::Parameter *param, uint32_t keynr, uint8_t nparts);

static ha_rows check_quick_select(Session *session,
                                  optimizer::Parameter *param,
                                  uint32_t idx,
                                  bool index_only,
                                  optimizer::SEL_ARG *tree,
                                  bool update_tbl_stats,
                                  uint32_t *mrr_flags,
                                  uint32_t *bufsize,
                                  optimizer::CostVector *cost);

static optimizer::RangeReadPlan *get_key_scans_params(Session *session,
                                                      optimizer::Parameter *param,
                                                      optimizer::SEL_TREE *tree,
                                                      bool index_read_must_be_used,
                                                      bool update_tbl_stats,
                                                      double read_time);

static
optimizer::RorIntersectReadPlan *get_best_ror_intersect(const optimizer::Parameter *param,
                                                        optimizer::SEL_TREE *tree,
                                                        double read_time,
                                                        bool *are_all_covering);

static
optimizer::RorIntersectReadPlan *get_best_covering_ror_intersect(optimizer::Parameter *param,
                                                                 optimizer::SEL_TREE *tree,
                                                                 double read_time);

static
optimizer::TableReadPlan *get_best_disjunct_quick(Session *session,
                                                  optimizer::Parameter *param,
                                                  optimizer::SEL_IMERGE *imerge,
                                                  double read_time);

static
optimizer::GroupMinMaxReadPlan *get_best_group_min_max(optimizer::Parameter *param, optimizer::SEL_TREE *tree);

static optimizer::SEL_TREE *tree_and(optimizer::RangeParameter *param,
                                     optimizer::SEL_TREE *tree1,
                                     optimizer::SEL_TREE *tree2);

static optimizer::SEL_ARG *sel_add(optimizer::SEL_ARG *key1, optimizer::SEL_ARG *key2);

static optimizer::SEL_ARG *key_and(optimizer::RangeParameter *param,
                                   optimizer::SEL_ARG *key1,
                                   optimizer::SEL_ARG *key2,
                                   uint32_t clone_flag);

static bool get_range(optimizer::SEL_ARG **e1, optimizer::SEL_ARG **e2, optimizer::SEL_ARG *root1);

optimizer::SEL_ARG optimizer::null_element(optimizer::SEL_ARG::IMPOSSIBLE);

static bool null_part_in_key(KEY_PART *key_part,
                             const unsigned char *key,
                             uint32_t length);

bool sel_trees_can_be_ored(optimizer::SEL_TREE *tree1,
                           optimizer::SEL_TREE *tree2,
                           optimizer::RangeParameter *param);






/*
  Perform AND operation on two index_merge lists and store result in *im1.
*/

inline void imerge_list_and_list(List<optimizer::SEL_IMERGE> *im1, List<optimizer::SEL_IMERGE> *im2)
{
  im1->concat(im2);
}


/***************************************************************************
** Basic functions for SqlSelect and QuickRangeSelect
***************************************************************************/

	/* make a select from mysql info
	   Error is set as following:
	   0 = ok
	   1 = Got some error (out of memory?)
	   */

optimizer::SqlSelect *optimizer::make_select(Table *head,
                                             table_map const_tables,
                                             table_map read_tables,
                                             COND *conds,
                                             bool allow_null_cond,
                                             int *error)
{
  *error= 0;

  if (! conds && ! allow_null_cond)
  {
    return 0;
  }
  optimizer::SqlSelect* select= new optimizer::SqlSelect;
  select->read_tables=read_tables;
  select->const_tables=const_tables;
  select->head=head;
  select->cond=conds;

  if (head->sort.io_cache)
  {
    memcpy(select->file, head->sort.io_cache, sizeof(internal::io_cache_st));
    select->records=(ha_rows) (select->file->end_of_file/
			       head->cursor->ref_length);
    delete head->sort.io_cache;
    head->sort.io_cache=0;
  }
  return(select);
}


optimizer::SqlSelect::SqlSelect()
  :
    quick(NULL),
    cond(NULL),
    file(static_cast<internal::io_cache_st *>(memory::sql_calloc(sizeof(internal::io_cache_st)))),
    free_cond(false)
{
  quick_keys.reset();
  needed_reg.reset();
  my_b_clear(file);
}


void optimizer::SqlSelect::cleanup()
{

  delete quick;
  quick= NULL;

  if (free_cond)
  {
    free_cond= 0;
    delete cond;
    cond= 0;
  }
  file->close_cached_file();
}


optimizer::SqlSelect::~SqlSelect()
{
  cleanup();
}


bool optimizer::SqlSelect::check_quick(Session *session,
                                       bool force_quick_range,
                                       ha_rows limit)
{
  key_map tmp;
  tmp.set();
  return (test_quick_select(session,
                           tmp,
                           0,
                           limit,
                           force_quick_range,
                           false) < 0);
}


bool optimizer::SqlSelect::skip_record()
{
  return (cond ? cond->val_int() == 0 : 0);
}


optimizer::QuickSelectInterface::QuickSelectInterface()
  :
    max_used_key_length(0),
    used_key_parts(0)
{}


/*
  Find the best index to retrieve first N records in given order

  SYNOPSIS
    get_index_for_order()
      table  Table to be accessed
      order  Required ordering
      limit  Number of records that will be retrieved

  DESCRIPTION
    Find the best index that allows to retrieve first #limit records in the
    given order cheaper then one would retrieve them using full table scan.

  IMPLEMENTATION
    Run through all table indexes and find the shortest index that allows
    records to be retrieved in given order. We look for the shortest index
    as we will have fewer index pages to read with it.

    This function is used only by UPDATE/DELETE, so we take into account how
    the UPDATE/DELETE code will work:
     * index can only be scanned in forward direction
     * HA_EXTRA_KEYREAD will not be used
    Perhaps these assumptions could be relaxed.

  RETURN
    Number of the index that produces the required ordering in the cheapest way
    MAX_KEY if no such index was found.
*/

uint32_t optimizer::get_index_for_order(Table *table, Order *order, ha_rows limit)
{
  uint32_t idx;
  uint32_t match_key= MAX_KEY, match_key_len= MAX_KEY_LENGTH + 1;
  Order *ord;

  for (ord= order; ord; ord= ord->next)
    if (!ord->asc)
      return MAX_KEY;

  for (idx= 0; idx < table->getShare()->sizeKeys(); idx++)
  {
    if (!(table->keys_in_use_for_query.test(idx)))
      continue;
    KeyPartInfo *keyinfo= table->key_info[idx].key_part;
    uint32_t n_parts=  table->key_info[idx].key_parts;
    uint32_t partno= 0;

    /*
      The below check is sufficient considering we now have either BTREE
      indexes (records are returned in order for any index prefix) or HASH
      indexes (records are not returned in order for any index prefix).
    */
    if (! (table->index_flags(idx) & HA_READ_ORDER))
      continue;
    for (ord= order; ord && partno < n_parts; ord= ord->next, partno++)
    {
      Item *item= order->item[0];
      if (! (item->type() == Item::FIELD_ITEM &&
           ((Item_field*)item)->field->eq(keyinfo[partno].field)))
        break;
    }

    if (! ord && table->key_info[idx].key_length < match_key_len)
    {
      /*
        Ok, the ordering is compatible and this key is shorter then
        previous match (we want shorter keys as we'll have to read fewer
        index pages for the same number of records)
      */
      match_key= idx;
      match_key_len= table->key_info[idx].key_length;
    }
  }

  if (match_key != MAX_KEY)
  {
    /*
      Found an index that allows records to be retrieved in the requested
      order. Now we'll check if using the index is cheaper then doing a table
      scan.
    */
    double full_scan_time= table->cursor->scan_time();
    double index_scan_time= table->cursor->read_time(match_key, 1, limit);
    if (index_scan_time > full_scan_time)
      match_key= MAX_KEY;
  }
  return match_key;
}



/*
  Fill param->needed_fields with bitmap of fields used in the query.
  SYNOPSIS
    fill_used_fields_bitmap()
      param Parameter from test_quick_select function.

  NOTES
    Clustered PK members are not put into the bitmap as they are implicitly
    present in all keys (and it is impossible to avoid reading them).
  RETURN
    0  Ok
    1  Out of memory.
*/

static int fill_used_fields_bitmap(optimizer::Parameter *param)
{
  Table *table= param->table;
  uint32_t pk;
  param->tmp_covered_fields.clear();
  param->needed_fields.resize(table->getShare()->sizeFields());
  param->needed_fields.reset();

  param->needed_fields|= *table->read_set;
  param->needed_fields|= *table->write_set;

  pk= param->table->getShare()->getPrimaryKey();
  if (pk != MAX_KEY && param->table->cursor->primary_key_is_clustered())
  {
    /* The table uses clustered PK and it is not internally generated */
    KeyPartInfo *key_part= param->table->key_info[pk].key_part;
    KeyPartInfo *key_part_end= key_part +
                                 param->table->key_info[pk].key_parts;
    for (;key_part != key_part_end; ++key_part)
      param->needed_fields.reset(key_part->fieldnr-1);
  }
  return 0;
}


/*
  Test if a key can be used in different ranges

  SYNOPSIS
    SqlSelect::test_quick_select()
      session               Current thread
      keys_to_use       Keys to use for range retrieval
      prev_tables       Tables assumed to be already read when the scan is
                        performed (but not read at the moment of this call)
      limit             Query limit
      force_quick_range Prefer to use range (instead of full table scan) even
                        if it is more expensive.

  NOTES
    Updates the following in the select parameter:
      needed_reg - Bits for keys with may be used if all prev regs are read
      quick      - Parameter to use when reading records.

    In the table struct the following information is updated:
      quick_keys           - Which keys can be used
      quick_rows           - How many rows the key matches
      quick_condition_rows - E(# rows that will satisfy the table condition)

  IMPLEMENTATION
    quick_condition_rows value is obtained as follows:

      It is a minimum of E(#output rows) for all considered table access
      methods (range and index_merge accesses over various indexes).

    The obtained value is not a true E(#rows that satisfy table condition)
    but rather a pessimistic estimate. To obtain a true E(#...) one would
    need to combine estimates of various access methods, taking into account
    correlations between sets of rows they will return.

    For example, if values of tbl.key1 and tbl.key2 are independent (a right
    assumption if we have no information about their correlation) then the
    correct estimate will be:

      E(#rows("tbl.key1 < c1 AND tbl.key2 < c2")) =
      = E(#rows(tbl.key1 < c1)) / total_rows(tbl) * E(#rows(tbl.key2 < c2)

    which is smaller than

       MIN(E(#rows(tbl.key1 < c1), E(#rows(tbl.key2 < c2)))

    which is currently produced.

  TODO
   * Change the value returned in quick_condition_rows from a pessimistic
     estimate to true E(#rows that satisfy table condition).
     (we can re-use some of E(#rows) calcuation code from index_merge/intersection
      for this)

   * Check if this function really needs to modify keys_to_use, and change the
     code to pass it by reference if it doesn't.

   * In addition to force_quick_range other means can be (an usually are) used
     to make this function prefer range over full table scan. Figure out if
     force_quick_range is really needed.

  RETURN
   -1 if impossible select (i.e. certainly no rows will be selected)
    0 if can't use quick_select
    1 if found usable ranges and quick select has been successfully created.
*/

int optimizer::SqlSelect::test_quick_select(Session *session,
                                            key_map keys_to_use,
				                                    table_map prev_tables,
				                                    ha_rows limit,
                                            bool force_quick_range,
                                            bool ordered_output)
{
  uint32_t idx;
  double scan_time;

  delete quick;
  quick= NULL;

  needed_reg.reset();
  quick_keys.reset();
  if (keys_to_use.none())
    return 0;
  records= head->cursor->stats.records;
  if (!records)
    records++;
  scan_time= (double) records / TIME_FOR_COMPARE + 1;
  read_time= (double) head->cursor->scan_time() + scan_time + 1.1;
  if (head->force_index)
    scan_time= read_time= DBL_MAX;
  if (limit < records)
    read_time= (double) records + scan_time + 1; // Force to use index
  else if (read_time <= 2.0 && !force_quick_range)
    return 0;				/* No need for quick select */

  keys_to_use&= head->keys_in_use_for_query;
  if (keys_to_use.any())
  {
    memory::Root alloc;
    optimizer::SEL_TREE *tree= NULL;
    KEY_PART *key_parts;
    KeyInfo *key_info;
    optimizer::Parameter param;

    if (check_stack_overrun(session, 2*STACK_MIN_SIZE, NULL))
      return 0;                           // Fatal error flag is set

    /* set up parameter that is passed to all functions */
    param.session= session;
    param.prev_tables= prev_tables | const_tables;
    param.read_tables= read_tables;
    param.current_table= head->map;
    param.table=head;
    param.keys=0;
    param.mem_root= &alloc;
    param.old_root= session->mem_root;
    param.needed_reg= &needed_reg;
    param.imerge_cost_buff_size= 0;
    param.using_real_indexes= true;
    param.remove_jump_scans= true;
    param.force_default_mrr= ordered_output;

    session->no_errors=1;				// Don't warn about NULL
    alloc.init(session->variables.range_alloc_block_size);
    param.key_parts= new (alloc) KEY_PART[head->getShare()->key_parts];
    if (fill_used_fields_bitmap(&param))
    {
      session->no_errors=0;
      alloc.free_root(MYF(0));			// Return memory & allocator

      return 0;				// Can't use range
    }
    key_parts= param.key_parts;
    session->mem_root= &alloc;

    /*
      Make an array with description of all key parts of all table keys.
      This is used in get_mm_parts function.
    */
    key_info= head->key_info;
    for (idx=0 ; idx < head->getShare()->sizeKeys() ; idx++, key_info++)
    {
      KeyPartInfo *key_part_info;
      if (! keys_to_use.test(idx))
	continue;

      param.key[param.keys]=key_parts;
      key_part_info= key_info->key_part;
      for (uint32_t part=0;
           part < key_info->key_parts;
           part++, key_parts++, key_part_info++)
      {
        key_parts->key= param.keys;
        key_parts->part= part;
        key_parts->length= key_part_info->length;
        key_parts->store_length= key_part_info->store_length;
        key_parts->field= key_part_info->field;
        key_parts->null_bit= key_part_info->null_bit;
        /* Only HA_PART_KEY_SEG is used */
        key_parts->flag= (uint8_t) key_part_info->key_part_flag;
      }
      param.real_keynr[param.keys++]=idx;
    }
    param.key_parts_end=key_parts;
    param.alloced_sel_args= 0;

    /* Calculate cost of full index read for the shortest covering index */
    if (!head->covering_keys.none())
    {
      int key_for_use= head->find_shortest_key(&head->covering_keys);
      double key_read_time=
        param.table->cursor->index_only_read_time(key_for_use, records) +
        (double) records / TIME_FOR_COMPARE;
      if (key_read_time < read_time)
        read_time= key_read_time;
    }

    optimizer::TableReadPlan *best_trp= NULL;
    optimizer::GroupMinMaxReadPlan *group_trp= NULL;
    double best_read_time= read_time;

    if (cond)
    {
      if ((tree= get_mm_tree(&param,cond)))
      {
        if (tree->type == optimizer::SEL_TREE::IMPOSSIBLE)
        {
          records=0L;                      /* Return -1 from this function. */
          read_time= (double) HA_POS_ERROR;
          goto free_mem;
        }
        /*
          If the tree can't be used for range scans, proceed anyway, as we
          can construct a group-min-max quick select
        */
        if (tree->type != optimizer::SEL_TREE::KEY && tree->type != optimizer::SEL_TREE::KEY_SMALLER)
          tree= NULL;
      }
    }

    /*
      Try to construct a QuickGroupMinMaxSelect.
      Notice that it can be constructed no matter if there is a range tree.
    */
    group_trp= get_best_group_min_max(&param, tree);
    if (group_trp)
    {
      param.table->quick_condition_rows= min(group_trp->records,
                                             head->cursor->stats.records);
      if (group_trp->read_cost < best_read_time)
      {
        best_trp= group_trp;
        best_read_time= best_trp->read_cost;
      }
    }

    if (tree)
    {
      /*
        It is possible to use a range-based quick select (but it might be
        slower than 'all' table scan).
      */
      if (tree->merges.is_empty())
      {
        optimizer::RangeReadPlan *range_trp= NULL;
        optimizer::RorIntersectReadPlan *rori_trp= NULL;
        bool can_build_covering= false;

        /* Get best 'range' plan and prepare data for making other plans */
        if ((range_trp= get_key_scans_params(session, &param, tree, false, true,
                                             best_read_time)))
        {
          best_trp= range_trp;
          best_read_time= best_trp->read_cost;
        }

        /*
          Simultaneous key scans and row deletes on several Cursor
          objects are not allowed so don't use ROR-intersection for
          table deletes.
        */
        if ((session->lex().sql_command != SQLCOM_DELETE))
        {
          /*
            Get best non-covering ROR-intersection plan and prepare data for
            building covering ROR-intersection.
          */
          if ((rori_trp= get_best_ror_intersect(&param, tree, best_read_time,
                                                &can_build_covering)))
          {
            best_trp= rori_trp;
            best_read_time= best_trp->read_cost;
            /*
              Try constructing covering ROR-intersect only if it looks possible
              and worth doing.
            */
            if (!rori_trp->is_covering && can_build_covering &&
                (rori_trp= get_best_covering_ror_intersect(&param, tree,
                                                           best_read_time)))
              best_trp= rori_trp;
          }
        }
      }
      else
      {
        /* Try creating index_merge/ROR-union scan. */
        optimizer::SEL_IMERGE *imerge= NULL;
        optimizer::TableReadPlan *best_conj_trp= NULL;
        optimizer::TableReadPlan *new_conj_trp= NULL;
        List<optimizer::SEL_IMERGE>::iterator it(tree->merges.begin());
        while ((imerge= it++))
        {
          new_conj_trp= get_best_disjunct_quick(session, &param, imerge, best_read_time);
          if (new_conj_trp)
            set_if_smaller(param.table->quick_condition_rows,
                           new_conj_trp->records);
          if (!best_conj_trp || (new_conj_trp && new_conj_trp->read_cost <
                                 best_conj_trp->read_cost))
            best_conj_trp= new_conj_trp;
        }
        if (best_conj_trp)
          best_trp= best_conj_trp;
      }
    }

    session->mem_root= param.old_root;

    /* If we got a read plan, create a quick select from it. */
    if (best_trp)
    {
      records= best_trp->records;
      if (! (quick= best_trp->make_quick(&param, true)) || quick->init())
      {
        delete quick;
        quick= NULL;
      }
    }

  free_mem:
    alloc.free_root(MYF(0));			// Return memory & allocator
    session->mem_root= param.old_root;
    session->no_errors=0;
  }

  /*
    Assume that if the user is using 'limit' we will only need to scan
    limit rows if we are using a key
  */
  return(records ? test(quick) : -1);
}

/*
  Get best plan for a optimizer::SEL_IMERGE disjunctive expression.
  SYNOPSIS
    get_best_disjunct_quick()
      session
      param     Parameter from check_quick_select function
      imerge    Expression to use
      read_time Don't create scans with cost > read_time

  NOTES
    index_merge cost is calculated as follows:
    index_merge_cost =
      cost(index_reads) +         (see #1)
      cost(rowid_to_row_scan) +   (see #2)
      cost(unique_use)            (see #3)

    1. cost(index_reads) =SUM_i(cost(index_read_i))
       For non-CPK scans,
         cost(index_read_i) = {cost of ordinary 'index only' scan}
       For CPK scan,
         cost(index_read_i) = {cost of non-'index only' scan}

    2. cost(rowid_to_row_scan)
      If table PK is clustered then
        cost(rowid_to_row_scan) =
          {cost of ordinary clustered PK scan with n_ranges=n_rows}

      Otherwise, we use the following model to calculate costs:
      We need to retrieve n_rows rows from cursor that occupies n_blocks blocks.
      We assume that offsets of rows we need are independent variates with
      uniform distribution in [0..max_file_offset] range.

      We'll denote block as "busy" if it contains row(s) we need to retrieve
      and "empty" if doesn't contain rows we need.

      Probability that a block is empty is (1 - 1/n_blocks)^n_rows (this
      applies to any block in cursor). Let x_i be a variate taking value 1 if
      block #i is empty and 0 otherwise.

      Then E(x_i) = (1 - 1/n_blocks)^n_rows;

      E(n_empty_blocks) = E(sum(x_i)) = sum(E(x_i)) =
        = n_blocks * ((1 - 1/n_blocks)^n_rows) =
       ~= n_blocks * exp(-n_rows/n_blocks).

      E(n_busy_blocks) = n_blocks*(1 - (1 - 1/n_blocks)^n_rows) =
       ~= n_blocks * (1 - exp(-n_rows/n_blocks)).

      Average size of "hole" between neighbor non-empty blocks is
           E(hole_size) = n_blocks/E(n_busy_blocks).

      The total cost of reading all needed blocks in one "sweep" is:

      E(n_busy_blocks)*
       (DISK_SEEK_BASE_COST + DISK_SEEK_PROP_COST*n_blocks/E(n_busy_blocks)).

    3. Cost of Unique use is calculated in Unique::get_use_cost function.

  ROR-union cost is calculated in the same way index_merge, but instead of
  Unique a priority queue is used.

  RETURN
    Created read plan
    NULL - Out of memory or no read scan could be built.
*/

static
optimizer::TableReadPlan *get_best_disjunct_quick(Session *session,
                                                  optimizer::Parameter *param,
                                                  optimizer::SEL_IMERGE *imerge,
                                                  double read_time)
{
  optimizer::SEL_TREE **ptree= NULL;
  optimizer::IndexMergeReadPlan *imerge_trp= NULL;
  uint32_t n_child_scans= imerge->trees_next - imerge->trees;
  optimizer::RangeReadPlan **range_scans= NULL;
  optimizer::RangeReadPlan **cur_child= NULL;
  optimizer::RangeReadPlan **cpk_scan= NULL;
  bool imerge_too_expensive= false;
  double imerge_cost= 0.0;
  ha_rows cpk_scan_records= 0;
  ha_rows non_cpk_scan_records= 0;
  bool pk_is_clustered= param->table->cursor->primary_key_is_clustered();
  bool all_scans_ror_able= true;
  bool all_scans_rors= true;
  uint32_t unique_calc_buff_size;
  optimizer::TableReadPlan **roru_read_plans= NULL;
  optimizer::TableReadPlan **cur_roru_plan= NULL;
  double roru_index_costs;
  ha_rows roru_total_records;
  double roru_intersect_part= 1.0;

  range_scans= new (*param->mem_root) optimizer::RangeReadPlan*[n_child_scans];

  /*
    Collect best 'range' scan for each of disjuncts, and, while doing so,
    analyze possibility of ROR scans. Also calculate some values needed by
    other parts of the code.
  */
  for (ptree= imerge->trees, cur_child= range_scans; ptree != imerge->trees_next; ptree++, cur_child++)
  {
    if (!(*cur_child= get_key_scans_params(session, param, *ptree, true, false, read_time)))
    {
      /*
        One of index scans in this index_merge is more expensive than entire
        table read for another available option. The entire index_merge (and
        any possible ROR-union) will be more expensive then, too. We continue
        here only to update SqlSelect members.
      */
      imerge_too_expensive= true;
    }
    if (imerge_too_expensive)
      continue;

    imerge_cost += (*cur_child)->read_cost;
    all_scans_ror_able &= ((*ptree)->n_ror_scans > 0);
    all_scans_rors &= (*cur_child)->is_ror;
    if (pk_is_clustered &&
        param->real_keynr[(*cur_child)->key_idx] ==
        param->table->getShare()->getPrimaryKey())
    {
      cpk_scan= cur_child;
      cpk_scan_records= (*cur_child)->records;
    }
    else
      non_cpk_scan_records += (*cur_child)->records;
  }

  if (imerge_too_expensive || (imerge_cost > read_time) ||
      ((non_cpk_scan_records+cpk_scan_records >= param->table->cursor->stats.records) && read_time != DBL_MAX))
  {
    /*
      Bail out if it is obvious that both index_merge and ROR-union will be
      more expensive
    */
    return NULL;
  }
  if (all_scans_rors)
  {
    roru_read_plans= (optimizer::TableReadPlan **) range_scans;
    goto skip_to_ror_scan;
  }
  if (cpk_scan)
  {
    /*
      Add one ROWID comparison for each row retrieved on non-CPK scan.  (it
      is done in QuickRangeSelect::row_in_ranges)
     */
    imerge_cost += non_cpk_scan_records / TIME_FOR_COMPARE_ROWID;
  }

  /* Calculate cost(rowid_to_row_scan) */
  {
    optimizer::CostVector sweep_cost;
    Join *join= param->session->lex().select_lex.join;
    bool is_interrupted= test(join && join->tables == 1);
    get_sweep_read_cost(param->table, non_cpk_scan_records, is_interrupted,
                        &sweep_cost);
    imerge_cost += sweep_cost.total_cost();
  }
  if (imerge_cost > read_time)
    goto build_ror_index_merge;

  /* Add Unique operations cost */
  unique_calc_buff_size=
    Unique::get_cost_calc_buff_size((ulong)non_cpk_scan_records,
                                    param->table->cursor->ref_length,
                                    param->session->variables.sortbuff_size);
  if (param->imerge_cost_buff_size < unique_calc_buff_size)
  {
    param->imerge_cost_buff= (uint*)param->mem_root->alloc(unique_calc_buff_size);
    param->imerge_cost_buff_size= unique_calc_buff_size;
  }

  imerge_cost +=
    Unique::get_use_cost(param->imerge_cost_buff, (uint32_t)non_cpk_scan_records,
                         param->table->cursor->ref_length,
                         param->session->variables.sortbuff_size);
  if (imerge_cost < read_time)
  {
    imerge_trp= new (*param->mem_root) optimizer::IndexMergeReadPlan;
    imerge_trp->read_cost= imerge_cost;
    imerge_trp->records= non_cpk_scan_records + cpk_scan_records;
    imerge_trp->records= min(imerge_trp->records, param->table->cursor->stats.records);
    imerge_trp->range_scans= range_scans;
    imerge_trp->range_scans_end= range_scans + n_child_scans;
    read_time= imerge_cost;
  }

build_ror_index_merge:
  if (!all_scans_ror_able || param->session->lex().sql_command == SQLCOM_DELETE)
    return(imerge_trp);

  /* Ok, it is possible to build a ROR-union, try it. */
  bool dummy;
  roru_read_plans= new (*param->mem_root) optimizer::TableReadPlan*[n_child_scans];
skip_to_ror_scan:
  roru_index_costs= 0.0;
  roru_total_records= 0;
  cur_roru_plan= roru_read_plans;

  /* Find 'best' ROR scan for each of trees in disjunction */
  for (ptree= imerge->trees, cur_child= range_scans;
       ptree != imerge->trees_next;
       ptree++, cur_child++, cur_roru_plan++)
  {
    /*
      Assume the best ROR scan is the one that has cheapest full-row-retrieval
      scan cost.
      Also accumulate index_only scan costs as we'll need them to calculate
      overall index_intersection cost.
    */
    double cost;
    if ((*cur_child)->is_ror)
    {
      /* Ok, we have index_only cost, now get full rows scan cost */
      cost= param->table->cursor->
              read_time(param->real_keynr[(*cur_child)->key_idx], 1,
                        (*cur_child)->records) +
              static_cast<double>((*cur_child)->records) / TIME_FOR_COMPARE;
    }
    else
      cost= read_time;

    optimizer::TableReadPlan *prev_plan= *cur_child;
    if (!(*cur_roru_plan= get_best_ror_intersect(param, *ptree, cost,
                                                 &dummy)))
    {
      if (prev_plan->is_ror)
        *cur_roru_plan= prev_plan;
      else
        return(imerge_trp);
      roru_index_costs += (*cur_roru_plan)->read_cost;
    }
    else
      roru_index_costs +=
        ((optimizer::RorIntersectReadPlan*)(*cur_roru_plan))->index_scan_costs;
    roru_total_records += (*cur_roru_plan)->records;
    roru_intersect_part *= (*cur_roru_plan)->records /
                           param->table->cursor->stats.records;
  }

  /*
    rows to retrieve=
      SUM(rows_in_scan_i) - table_rows * PROD(rows_in_scan_i / table_rows).
    This is valid because index_merge construction guarantees that conditions
    in disjunction do not share key parts.
  */
  roru_total_records -= (ha_rows)(roru_intersect_part*
                                  param->table->cursor->stats.records);
  /* ok, got a ROR read plan for each of the disjuncts
    Calculate cost:
    cost(index_union_scan(scan_1, ... scan_n)) =
      SUM_i(cost_of_index_only_scan(scan_i)) +
      queue_use_cost(rowid_len, n) +
      cost_of_row_retrieval
    See get_merge_buffers_cost function for queue_use_cost formula derivation.
  */
  double roru_total_cost;
  {
    optimizer::CostVector sweep_cost;
    Join *join= param->session->lex().select_lex.join;
    bool is_interrupted= test(join && join->tables == 1);
    get_sweep_read_cost(param->table, roru_total_records, is_interrupted,
                        &sweep_cost);
    roru_total_cost= roru_index_costs +
                     static_cast<double>(roru_total_records)*log((double)n_child_scans) /
                     (TIME_FOR_COMPARE_ROWID * M_LN2) +
                     sweep_cost.total_cost();
  }

  optimizer::RorUnionReadPlan *roru= NULL;
  if (roru_total_cost < read_time)
  {
    if ((roru= new (*param->mem_root) optimizer::RorUnionReadPlan))
    {
      roru->first_ror= roru_read_plans;
      roru->last_ror= roru_read_plans + n_child_scans;
      roru->read_cost= roru_total_cost;
      roru->records= roru_total_records;
      return roru;
    }
  }
  return(imerge_trp);
}



/*
  Create optimizer::RorScanInfo* structure with a single ROR scan on index idx using
  sel_arg set of intervals.

  SYNOPSIS
    make_ror_scan()
      param    Parameter from test_quick_select function
      idx      Index of key in param->keys
      sel_arg  Set of intervals for a given key

  RETURN
    NULL - out of memory
    ROR scan structure containing a scan for {idx, sel_arg}
*/

static
optimizer::RorScanInfo *make_ror_scan(const optimizer::Parameter *param, int idx, optimizer::SEL_ARG *sel_arg)
{
  uint32_t keynr;
  optimizer::RorScanInfo* ror_scan= new (*param->mem_root) optimizer::RorScanInfo;

  ror_scan->idx= idx;
  ror_scan->keynr= keynr= param->real_keynr[idx];
  ror_scan->key_rec_length= (param->table->key_info[keynr].key_length +
                             param->table->cursor->ref_length);
  ror_scan->sel_arg= sel_arg;
  ror_scan->records= param->table->quick_rows[keynr];

  ror_scan->covered_fields_size= param->table->getShare()->sizeFields();
  boost::dynamic_bitset<> tmp_bitset(param->table->getShare()->sizeFields());
  tmp_bitset.reset();

  KeyPartInfo *key_part= param->table->key_info[keynr].key_part;
  KeyPartInfo *key_part_end= key_part +
                               param->table->key_info[keynr].key_parts;
  for (; key_part != key_part_end; ++key_part)
  {
    if (param->needed_fields.test(key_part->fieldnr-1))
      tmp_bitset.set(key_part->fieldnr-1);
  }
  double rows= param->table->quick_rows[ror_scan->keynr];
  ror_scan->index_read_cost=
    param->table->cursor->index_only_read_time(ror_scan->keynr, rows);
  ror_scan->covered_fields= tmp_bitset.to_ulong();
  return ror_scan;
}


/*
  Compare two optimizer::RorScanInfo** by  E(#records_matched) * key_record_length.
  SYNOPSIS
    cmp_ror_scan_info()
      a ptr to first compared value
      b ptr to second compared value

  RETURN
   -1 a < b
    0 a = b
    1 a > b
*/

static int cmp_ror_scan_info(optimizer::RorScanInfo** a, optimizer::RorScanInfo** b)
{
  double val1= static_cast<double>((*a)->records) * (*a)->key_rec_length;
  double val2= static_cast<double>((*b)->records) * (*b)->key_rec_length;
  return (val1 < val2)? -1: (val1 == val2)? 0 : 1;
}


/*
  Compare two optimizer::RorScanInfo** by
   (#covered fields in F desc,
    #components asc,
    number of first not covered component asc)

  SYNOPSIS
    cmp_ror_scan_info_covering()
      a ptr to first compared value
      b ptr to second compared value

  RETURN
   -1 a < b
    0 a = b
    1 a > b
*/

static int cmp_ror_scan_info_covering(optimizer::RorScanInfo** a, optimizer::RorScanInfo** b)
{
  if ((*a)->used_fields_covered > (*b)->used_fields_covered)
    return -1;
  if ((*a)->used_fields_covered < (*b)->used_fields_covered)
    return 1;
  if ((*a)->key_components < (*b)->key_components)
    return -1;
  if ((*a)->key_components > (*b)->key_components)
    return 1;
  if ((*a)->first_uncovered_field < (*b)->first_uncovered_field)
    return -1;
  if ((*a)->first_uncovered_field > (*b)->first_uncovered_field)
    return 1;
  return 0;
}

/* Auxiliary structure for incremental ROR-intersection creation */
typedef struct st_ror_intersect_info
{
  st_ror_intersect_info()
    :
      param(NULL),
      covered_fields(),
      out_rows(0.0),
      is_covering(false),
      index_records(0),
      index_scan_costs(0.0),
      total_cost(0.0)
  {}

  st_ror_intersect_info(const optimizer::Parameter *in_param)
    :
      param(in_param),
      covered_fields(in_param->table->getShare()->sizeFields()),
      out_rows(in_param->table->cursor->stats.records),
      is_covering(false),
      index_records(0),
      index_scan_costs(0.0),
      total_cost(0.0)
  {
    covered_fields.reset();
  }

  const optimizer::Parameter *param;
  boost::dynamic_bitset<> covered_fields; /* union of fields covered by all scans */
  /*
    Fraction of table records that satisfies conditions of all scans.
    This is the number of full records that will be retrieved if a
    non-index_only index intersection will be employed.
  */
  double out_rows;
  /* true if covered_fields is a superset of needed_fields */
  bool is_covering;

  ha_rows index_records; /* sum(#records to look in indexes) */
  double index_scan_costs; /* SUM(cost of 'index-only' scans) */
  double total_cost;
} ROR_INTERSECT_INFO;


static void ror_intersect_cpy(ROR_INTERSECT_INFO *dst,
                              const ROR_INTERSECT_INFO *src)
{
  dst->param= src->param;
  dst->covered_fields= src->covered_fields;
  dst->out_rows= src->out_rows;
  dst->is_covering= src->is_covering;
  dst->index_records= src->index_records;
  dst->index_scan_costs= src->index_scan_costs;
  dst->total_cost= src->total_cost;
}


/*
  Get selectivity of a ROR scan wrt ROR-intersection.

  SYNOPSIS
    ror_scan_selectivity()
      info  ROR-interection
      scan  ROR scan

  NOTES
    Suppose we have a condition on several keys
    cond=k_11=c_11 AND k_12=c_12 AND ...  // parts of first key
         k_21=c_21 AND k_22=c_22 AND ...  // parts of second key
          ...
         k_n1=c_n1 AND k_n3=c_n3 AND ...  (1) //parts of the key used by *scan

    where k_ij may be the same as any k_pq (i.e. keys may have common parts).

    A full row is retrieved if entire condition holds.

    The recursive procedure for finding P(cond) is as follows:

    First step:
    Pick 1st part of 1st key and break conjunction (1) into two parts:
      cond= (k_11=c_11 AND R)

    Here R may still contain condition(s) equivalent to k_11=c_11.
    Nevertheless, the following holds:

      P(k_11=c_11 AND R) = P(k_11=c_11) * P(R | k_11=c_11).

    Mark k_11 as fixed field (and satisfied condition) F, save P(F),
    save R to be cond and proceed to recursion step.

    Recursion step:
    We have a set of fixed fields/satisfied conditions) F, probability P(F),
    and remaining conjunction R
    Pick next key part on current key and its condition "k_ij=c_ij".
    We will add "k_ij=c_ij" into F and update P(F).
    Lets denote k_ij as t,  R = t AND R1, where R1 may still contain t. Then

     P((t AND R1)|F) = P(t|F) * P(R1|t|F) = P(t|F) * P(R1|(t AND F)) (2)

    (where '|' mean conditional probability, not "or")

    Consider the first multiplier in (2). One of the following holds:
    a) F contains condition on field used in t (i.e. t AND F = F).
      Then P(t|F) = 1

    b) F doesn't contain condition on field used in t. Then F and t are
     considered independent.

     P(t|F) = P(t|(fields_before_t_in_key AND other_fields)) =
          = P(t|fields_before_t_in_key).

     P(t|fields_before_t_in_key) = #records(fields_before_t_in_key) /
                                   #records(fields_before_t_in_key, t)

    The second multiplier is calculated by applying this step recursively.

  IMPLEMENTATION
    This function calculates the result of application of the "recursion step"
    described above for all fixed key members of a single key, accumulating set
    of covered fields, selectivity, etc.

    The calculation is conducted as follows:
    Lets denote #records(keypart1, ... keypartK) as n_k. We need to calculate

     n_{k1}      n_{k2}
    --------- * ---------  * .... (3)
     n_{k1-1}    n_{k2-1}

    where k1,k2,... are key parts which fields were not yet marked as fixed
    ( this is result of application of option b) of the recursion step for
      parts of a single key).
    Since it is reasonable to expect that most of the fields are not marked
    as fixed, we calculate (3) as

                                  n_{i1}      n_{i2}
    (3) = n_{max_key_part}  / (   --------- * ---------  * ....  )
                                  n_{i1-1}    n_{i2-1}

    where i1,i2, .. are key parts that were already marked as fixed.

    In order to minimize number of expensive records_in_range calls we group
    and reduce adjacent fractions.

  RETURN
    Selectivity of given ROR scan.
*/

static double ror_scan_selectivity(const ROR_INTERSECT_INFO *info,
                                   const optimizer::RorScanInfo *scan)
{
  double selectivity_mult= 1.0;
  KeyPartInfo *key_part= info->param->table->key_info[scan->keynr].key_part;
  unsigned char key_val[MAX_KEY_LENGTH+MAX_FIELD_WIDTH]; /* key values tuple */
  unsigned char *key_ptr= key_val;
  optimizer::SEL_ARG *sel_arg= NULL;
  optimizer::SEL_ARG *tuple_arg= NULL;
  key_part_map keypart_map= 0;
  bool cur_covered;
  bool prev_covered= test(info->covered_fields.test(key_part->fieldnr-1));
  key_range min_range;
  key_range max_range;
  min_range.key= key_val;
  min_range.flag= HA_READ_KEY_EXACT;
  max_range.key= key_val;
  max_range.flag= HA_READ_AFTER_KEY;
  ha_rows prev_records= info->param->table->cursor->stats.records;

  for (sel_arg= scan->sel_arg; sel_arg;
       sel_arg= sel_arg->next_key_part)
  {
    cur_covered=
      test(info->covered_fields.test(key_part[sel_arg->part].fieldnr-1));
    if (cur_covered != prev_covered)
    {
      /* create (part1val, ..., part{n-1}val) tuple. */
      ha_rows records;
      if (!tuple_arg)
      {
        tuple_arg= scan->sel_arg;
        /* Here we use the length of the first key part */
        tuple_arg->store_min(key_part->store_length, &key_ptr, 0);
        keypart_map= 1;
      }
      while (tuple_arg->next_key_part != sel_arg)
      {
        tuple_arg= tuple_arg->next_key_part;
        tuple_arg->store_min(key_part[tuple_arg->part].store_length,
                             &key_ptr, 0);
        keypart_map= (keypart_map << 1) | 1;
      }
      min_range.length= max_range.length= (size_t) (key_ptr - key_val);
      min_range.keypart_map= max_range.keypart_map= keypart_map;
      records= (info->param->table->cursor->
                records_in_range(scan->keynr, &min_range, &max_range));
      if (cur_covered)
      {
        /* uncovered -> covered */
        selectivity_mult *= static_cast<double>(records) / prev_records;
        prev_records= HA_POS_ERROR;
      }
      else
      {
        /* covered -> uncovered */
        prev_records= records;
      }
    }
    prev_covered= cur_covered;
  }
  if (!prev_covered)
  {
    selectivity_mult *= static_cast<double>(info->param->table->quick_rows[scan->keynr]) / prev_records;
  }
  return selectivity_mult;
}


/*
  Check if adding a ROR scan to a ROR-intersection reduces its cost of
  ROR-intersection and if yes, update parameters of ROR-intersection,
  including its cost.

  SYNOPSIS
    ror_intersect_add()
      param        Parameter from test_quick_select
      info         ROR-intersection structure to add the scan to.
      ror_scan     ROR scan info to add.
      is_cpk_scan  If true, add the scan as CPK scan (this can be inferred
                   from other parameters and is passed separately only to
                   avoid duplicating the inference code)

  NOTES
    Adding a ROR scan to ROR-intersect "makes sense" iff the cost of ROR-
    intersection decreases. The cost of ROR-intersection is calculated as
    follows:

    cost= SUM_i(key_scan_cost_i) + cost_of_full_rows_retrieval

    When we add a scan the first increases and the second decreases.

    cost_of_full_rows_retrieval=
      (union of indexes used covers all needed fields) ?
        cost_of_sweep_read(E(rows_to_retrieve), rows_in_table) :
        0

    E(rows_to_retrieve) = #rows_in_table * ror_scan_selectivity(null, scan1) *
                           ror_scan_selectivity({scan1}, scan2) * ... *
                           ror_scan_selectivity({scan1,...}, scanN).
  RETURN
    true   ROR scan added to ROR-intersection, cost updated.
    false  It doesn't make sense to add this ROR scan to this ROR-intersection.
*/

static bool ror_intersect_add(ROR_INTERSECT_INFO *info,
                              optimizer::RorScanInfo* ror_scan, bool is_cpk_scan)
{
  double selectivity_mult= 1.0;

  selectivity_mult = ror_scan_selectivity(info, ror_scan);
  if (selectivity_mult == 1.0)
  {
    /* Don't add this scan if it doesn't improve selectivity. */
    return false;
  }

  info->out_rows *= selectivity_mult;

  if (is_cpk_scan)
  {
    /*
      CPK scan is used to filter out rows. We apply filtering for
      each record of every scan. Assuming 1/TIME_FOR_COMPARE_ROWID
      per check this gives us:
    */
    info->index_scan_costs += static_cast<double>(info->index_records) /
                              TIME_FOR_COMPARE_ROWID;
  }
  else
  {
    info->index_records += info->param->table->quick_rows[ror_scan->keynr];
    info->index_scan_costs += ror_scan->index_read_cost;
    boost::dynamic_bitset<> tmp_bitset= ror_scan->bitsToBitset();
    info->covered_fields|= tmp_bitset;
    if (! info->is_covering && info->param->needed_fields.is_subset_of(info->covered_fields))
    {
      info->is_covering= true;
    }
  }

  info->total_cost= info->index_scan_costs;
  if (! info->is_covering)
  {
    optimizer::CostVector sweep_cost;
    Join *join= info->param->session->lex().select_lex.join;
    bool is_interrupted= test(join && join->tables == 1);
    get_sweep_read_cost(info->param->table, double2rows(info->out_rows),
                        is_interrupted, &sweep_cost);
    info->total_cost += sweep_cost.total_cost();
  }
  return true;
}


/*
  Get best covering ROR-intersection.
  SYNOPSIS
    get_best_covering_ror_intersect()
      param     Parameter from test_quick_select function.
      tree      optimizer::SEL_TREE with sets of intervals for different keys.
      read_time Don't return table read plans with cost > read_time.

  RETURN
    Best covering ROR-intersection plan
    NULL if no plan found.

  NOTES
    get_best_ror_intersect must be called for a tree before calling this
    function for it.
    This function invalidates tree->ror_scans member values.

  The following approximate algorithm is used:
    I=set of all covering indexes
    F=set of all fields to cover
    S={}

    do
    {
      Order I by (#covered fields in F desc,
                  #components asc,
                  number of first not covered component asc);
      F=F-covered by first(I);
      S=S+first(I);
      I=I-first(I);
    } while F is not empty.
*/

static
optimizer::RorIntersectReadPlan *get_best_covering_ror_intersect(optimizer::Parameter *param,
                                                            optimizer::SEL_TREE *tree,
                                                            double read_time)
{
  optimizer::RorScanInfo **ror_scan_mark;
  optimizer::RorScanInfo **ror_scans_end= tree->ror_scans_end;

  for (optimizer::RorScanInfo **scan= tree->ror_scans; scan != ror_scans_end; ++scan)
    (*scan)->key_components=
      param->table->key_info[(*scan)->keynr].key_parts;

  /*
    Run covering-ROR-search algorithm.
    Assume set I is [ror_scan .. ror_scans_end)
  */

  /*I=set of all covering indexes */
  ror_scan_mark= tree->ror_scans;

  boost::dynamic_bitset<> *covered_fields= &param->tmp_covered_fields;
  if (covered_fields->empty())
  {
    covered_fields->resize(param->table->getShare()->sizeFields());
  }
  covered_fields->reset();

  double total_cost= 0.0f;
  ha_rows records=0;
  bool all_covered;

  do
  {
    /*
      Update changed sorting info:
        #covered fields,
	number of first not covered component
      Calculate and save these values for each of remaining scans.
    */
    for (optimizer::RorScanInfo **scan= ror_scan_mark; scan != ror_scans_end; ++scan)
    {
      /* subtract one bitset from the other */
      (*scan)->subtractBitset(*covered_fields);
      (*scan)->used_fields_covered=
        (*scan)->getBitCount();
      (*scan)->first_uncovered_field= (*scan)->findFirstNotSet();
    }

    internal::my_qsort(ror_scan_mark, ror_scans_end-ror_scan_mark,
                       sizeof(optimizer::RorScanInfo*),
                       (qsort_cmp)cmp_ror_scan_info_covering);

    /* I=I-first(I) */
    total_cost += (*ror_scan_mark)->index_read_cost;
    records += (*ror_scan_mark)->records;
    if (total_cost > read_time)
      return NULL;
    /* F=F-covered by first(I) */
    boost::dynamic_bitset<> tmp_bitset= (*ror_scan_mark)->bitsToBitset();
    *covered_fields|= tmp_bitset;
    all_covered= param->needed_fields.is_subset_of(*covered_fields);
  } while ((++ror_scan_mark < ror_scans_end) && ! all_covered);

  if (!all_covered || (ror_scan_mark - tree->ror_scans) == 1)
    return NULL;

  /*
    Ok, [tree->ror_scans .. ror_scan) holds covering index_intersection with
    cost total_cost.
  */
  /* Add priority queue use cost. */
  total_cost += static_cast<double>(records) *
                log((double)(ror_scan_mark - tree->ror_scans)) /
                (TIME_FOR_COMPARE_ROWID * M_LN2);

  if (total_cost > read_time)
    return NULL;

  optimizer::RorIntersectReadPlan* trp= new (*param->mem_root) optimizer::RorIntersectReadPlan;

  uint32_t best_num= (ror_scan_mark - tree->ror_scans);
  trp->first_scan= new (*param->mem_root) optimizer::RorScanInfo*[best_num];
  memcpy(trp->first_scan, tree->ror_scans, best_num*sizeof(optimizer::RorScanInfo*));
  trp->last_scan=  trp->first_scan + best_num;
  trp->is_covering= true;
  trp->read_cost= total_cost;
  trp->records= records;
  trp->cpk_scan= NULL;
  set_if_smaller(param->table->quick_condition_rows, records);

  return(trp);
}


/*
  Get best ROR-intersection plan using non-covering ROR-intersection search
  algorithm. The returned plan may be covering.

  SYNOPSIS
    get_best_ror_intersect()
      param            Parameter from test_quick_select function.
      tree             Transformed restriction condition to be used to look
                       for ROR scans.
      read_time        Do not return read plans with cost > read_time.
      are_all_covering [out] set to true if union of all scans covers all
                       fields needed by the query (and it is possible to build
                       a covering ROR-intersection)

  NOTES
    get_key_scans_params must be called before this function can be called.

    When this function is called by ROR-union construction algorithm it
    assumes it is building an uncovered ROR-intersection (and thus # of full
    records to be retrieved is wrong here). This is a hack.

  IMPLEMENTATION
    The approximate best non-covering plan search algorithm is as follows:

    find_min_ror_intersection_scan()
    {
      R= select all ROR scans;
      order R by (E(#records_matched) * key_record_length).

      S= first(R); -- set of scans that will be used for ROR-intersection
      R= R-first(S);
      min_cost= cost(S);
      min_scan= make_scan(S);
      while (R is not empty)
      {
        firstR= R - first(R);
        if (!selectivity(S + firstR < selectivity(S)))
          continue;

        S= S + first(R);
        if (cost(S) < min_cost)
        {
          min_cost= cost(S);
          min_scan= make_scan(S);
        }
      }
      return min_scan;
    }

    See ror_intersect_add function for ROR intersection costs.

    Special handling for Clustered PK scans
    Clustered PK contains all table fields, so using it as a regular scan in
    index intersection doesn't make sense: a range scan on CPK will be less
    expensive in this case.
    Clustered PK scan has special handling in ROR-intersection: it is not used
    to retrieve rows, instead its condition is used to filter row references
    we get from scans on other keys.

  RETURN
    ROR-intersection table read plan
    NULL if out of memory or no suitable plan found.
*/

static
optimizer::RorIntersectReadPlan *get_best_ror_intersect(const optimizer::Parameter *param,
                                                   optimizer::SEL_TREE *tree,
                                                   double read_time,
                                                   bool *are_all_covering)
{
  uint32_t idx= 0;
  double min_cost= DBL_MAX;

  if ((tree->n_ror_scans < 2) || ! param->table->cursor->stats.records)
    return NULL;

  /*
    Step1: Collect ROR-able SEL_ARGs and create optimizer::RorScanInfo for each of
    them. Also find and save clustered PK scan if there is one.
  */
  optimizer::RorScanInfo **cur_ror_scan= NULL;
  optimizer::RorScanInfo *cpk_scan= NULL;
  uint32_t cpk_no= 0;
  bool cpk_scan_used= false;

  tree->ror_scans= new (*param->mem_root) optimizer::RorScanInfo*[param->keys];
  cpk_no= ((param->table->cursor->primary_key_is_clustered()) ? param->table->getShare()->getPrimaryKey() : MAX_KEY);

  for (idx= 0, cur_ror_scan= tree->ror_scans; idx < param->keys; idx++)
  {
    optimizer::RorScanInfo *scan;
    if (! tree->ror_scans_map.test(idx))
      continue;
    if (! (scan= make_ror_scan(param, idx, tree->keys[idx])))
      return NULL;
    if (param->real_keynr[idx] == cpk_no)
    {
      cpk_scan= scan;
      tree->n_ror_scans--;
    }
    else
      *(cur_ror_scan++)= scan;
  }

  tree->ror_scans_end= cur_ror_scan;
  /*
    Ok, [ror_scans, ror_scans_end) is array of ptrs to initialized
    optimizer::RorScanInfo's.
    Step 2: Get best ROR-intersection using an approximate algorithm.
  */
  internal::my_qsort(tree->ror_scans, tree->n_ror_scans, sizeof(optimizer::RorScanInfo*),
                     (qsort_cmp)cmp_ror_scan_info);

  optimizer::RorScanInfo **intersect_scans= NULL; /* ROR scans used in index intersection */
  optimizer::RorScanInfo **intersect_scans_end= intersect_scans=  new (*param->mem_root) optimizer::RorScanInfo*[tree->n_ror_scans];
  intersect_scans_end= intersect_scans;

  /* Create and incrementally update ROR intersection. */
  ROR_INTERSECT_INFO intersect(param);
  ROR_INTERSECT_INFO intersect_best(param);

  /* [intersect_scans,intersect_scans_best) will hold the best intersection */
  optimizer::RorScanInfo **intersect_scans_best= NULL;
  cur_ror_scan= tree->ror_scans;
  intersect_scans_best= intersect_scans;
  while (cur_ror_scan != tree->ror_scans_end && ! intersect.is_covering)
  {
    /* S= S + first(R);  R= R - first(R); */
    if (! ror_intersect_add(&intersect, *cur_ror_scan, false))
    {
      cur_ror_scan++;
      continue;
    }

    *(intersect_scans_end++)= *(cur_ror_scan++);

    if (intersect.total_cost < min_cost)
    {
      /* Local minimum found, save it */
      ror_intersect_cpy(&intersect_best, &intersect);
      intersect_scans_best= intersect_scans_end;
      min_cost = intersect.total_cost;
    }
  }

  if (intersect_scans_best == intersect_scans)
  {
    return NULL;
  }

  *are_all_covering= intersect.is_covering;
  uint32_t best_num= intersect_scans_best - intersect_scans;
  ror_intersect_cpy(&intersect, &intersect_best);

  /*
    Ok, found the best ROR-intersection of non-CPK key scans.
    Check if we should add a CPK scan. If the obtained ROR-intersection is
    covering, it doesn't make sense to add CPK scan.
  */
  if (cpk_scan && ! intersect.is_covering)
  {
    if (ror_intersect_add(&intersect, cpk_scan, true) &&
        (intersect.total_cost < min_cost))
    {
      cpk_scan_used= true;
      intersect_best= intersect; //just set pointer here
    }
  }

  /* Ok, return ROR-intersect plan if we have found one */
  optimizer::RorIntersectReadPlan *trp= NULL;
  if (min_cost < read_time && (cpk_scan_used || best_num > 1))
  {
    trp= new (*param->mem_root) optimizer::RorIntersectReadPlan;
    trp->first_scan= new (*param->mem_root) optimizer::RorScanInfo*[best_num];
    memcpy(trp->first_scan, intersect_scans, best_num*sizeof(optimizer::RorScanInfo*));
    trp->last_scan=  trp->first_scan + best_num;
    trp->is_covering= intersect_best.is_covering;
    trp->read_cost= intersect_best.total_cost;
    /* Prevent divisons by zero */
    ha_rows best_rows = double2rows(intersect_best.out_rows);
    if (! best_rows)
      best_rows= 1;
    set_if_smaller(param->table->quick_condition_rows, best_rows);
    trp->records= best_rows;
    trp->index_scan_costs= intersect_best.index_scan_costs;
    trp->cpk_scan= cpk_scan_used? cpk_scan: NULL;
  }
  return trp;
}


/*
  Get best "range" table read plan for given optimizer::SEL_TREE, also update some info

  SYNOPSIS
    get_key_scans_params()
      session
      param                    Parameters from test_quick_select
      tree                     Make range select for this optimizer::SEL_TREE
      index_read_must_be_used  true <=> assume 'index only' option will be set
                               (except for clustered PK indexes)
      update_tbl_stats         true <=> update table->quick_* with information
                               about range scans we've evaluated.
      read_time                Maximum cost. i.e. don't create read plans with
                               cost > read_time.

  DESCRIPTION
    Find the best "range" table read plan for given optimizer::SEL_TREE.
    The side effects are
     - tree->ror_scans is updated to indicate which scans are ROR scans.
     - if update_tbl_stats=true then table->quick_* is updated with info
       about every possible range scan.

  RETURN
    Best range read plan
    NULL if no plan found or error occurred
*/

static optimizer::RangeReadPlan *get_key_scans_params(Session *session,
                                                      optimizer::Parameter *param,
                                                      optimizer::SEL_TREE *tree,
                                                      bool index_read_must_be_used,
                                                      bool update_tbl_stats,
                                                      double read_time)
{
  uint32_t idx;
  optimizer::SEL_ARG **key= NULL;
  optimizer::SEL_ARG **end= NULL;
  optimizer::SEL_ARG **key_to_read= NULL;
  ha_rows best_records= 0;
  uint32_t best_mrr_flags= 0;
  uint32_t best_buf_size= 0;
  optimizer::RangeReadPlan *read_plan= NULL;
  /*
    Note that there may be trees that have type optimizer::SEL_TREE::KEY but contain no
    key reads at all, e.g. tree for expression "key1 is not null" where key1
    is defined as "not null".
  */
  tree->ror_scans_map.reset();
  tree->n_ror_scans= 0;
  for (idx= 0,key=tree->keys, end=key+param->keys; key != end; key++,idx++)
  {
    if (*key)
    {
      ha_rows found_records;
      optimizer::CostVector cost;
      double found_read_time= 0.0;
      uint32_t mrr_flags, buf_size;
      uint32_t keynr= param->real_keynr[idx];
      if ((*key)->type == optimizer::SEL_ARG::MAYBE_KEY ||
          (*key)->maybe_flag)
        param->needed_reg->set(keynr);

      bool read_index_only= index_read_must_be_used ||
                            param->table->covering_keys.test(keynr);

      found_records= check_quick_select(session, param, idx, read_index_only, *key,
                                        update_tbl_stats, &mrr_flags,
                                        &buf_size, &cost);
      found_read_time= cost.total_cost();
      if ((found_records != HA_POS_ERROR) && param->is_ror_scan)
      {
        tree->n_ror_scans++;
        tree->ror_scans_map.set(idx);
      }
      if (read_time > found_read_time && found_records != HA_POS_ERROR)
      {
        read_time=    found_read_time;
        best_records= found_records;
        key_to_read=  key;
        best_mrr_flags= mrr_flags;
        best_buf_size=  buf_size;
      }
    }
  }

  if (key_to_read)
  {
    idx= key_to_read - tree->keys;
    read_plan= new (*param->mem_root) optimizer::RangeReadPlan(*key_to_read, idx, best_mrr_flags);
    read_plan->records= best_records;
    read_plan->is_ror= tree->ror_scans_map.test(idx);
    read_plan->read_cost= read_time;
    read_plan->mrr_buf_size= best_buf_size;
  }
  return read_plan;
}


optimizer::QuickSelectInterface *optimizer::IndexMergeReadPlan::make_quick(optimizer::Parameter *param, bool, memory::Root *)
{
  /* index_merge always retrieves full rows, ignore retrieve_full_rows */
  optimizer::QuickIndexMergeSelect* quick_imerge= new optimizer::QuickIndexMergeSelect(param->session, param->table);
  quick_imerge->records= records;
  quick_imerge->read_time= read_cost;
  for (optimizer::RangeReadPlan **range_scan= range_scans; range_scan != range_scans_end; range_scan++)
  {
    optimizer::QuickRangeSelect* quick= (optimizer::QuickRangeSelect*)((*range_scan)->make_quick(param, false, &quick_imerge->alloc));
    if (not quick)
    {
      delete quick;
      delete quick_imerge;
      return NULL;
    }
    quick_imerge->push_quick_back(quick);
  }
  return quick_imerge;
}

optimizer::QuickSelectInterface *optimizer::RorIntersectReadPlan::make_quick(optimizer::Parameter *param,
                                                                             bool retrieve_full_rows,
                                                                             memory::Root *parent_alloc)
{
  optimizer::QuickRorIntersectSelect *quick_intersect= NULL;
  optimizer::QuickRangeSelect *quick= NULL;
  memory::Root *alloc= NULL;

  if ((quick_intersect=
         new optimizer::QuickRorIntersectSelect(param->session,
                                                param->table,
                                                (retrieve_full_rows? (! is_covering) : false),
                                                parent_alloc)))
  {
    alloc= parent_alloc ? parent_alloc : &quick_intersect->alloc;
    for (; first_scan != last_scan; ++first_scan)
    {
      if (! (quick= optimizer::get_quick_select(param,
                                                (*first_scan)->idx,
                                                (*first_scan)->sel_arg,
                                                HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED,
                                                0,
                                                alloc)))
      {
        delete quick_intersect;
        return NULL;
      }
      quick_intersect->push_quick_back(quick);
    }
    if (cpk_scan)
    {
      if (! (quick= optimizer::get_quick_select(param,
                                                cpk_scan->idx,
                                                cpk_scan->sel_arg,
                                                HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED,
                                                0,
                                                alloc)))
      {
        delete quick_intersect;
        return NULL;
      }
      quick->resetCursor();
      quick_intersect->cpk_quick= quick;
    }
    quick_intersect->records= records;
    quick_intersect->read_time= read_cost;
  }
  return quick_intersect;
}


optimizer::QuickSelectInterface *optimizer::RorUnionReadPlan::make_quick(optimizer::Parameter *param, bool, memory::Root *)
{
  /*
    It is impossible to construct a ROR-union that will not retrieve full
    rows, ignore retrieve_full_rows parameter.
  */
  optimizer::QuickRorUnionSelect* quick_roru= new optimizer::QuickRorUnionSelect(param->session, param->table);
  for (optimizer::TableReadPlan** scan= first_ror; scan != last_ror; scan++)
  {
    optimizer::QuickSelectInterface* quick= (*scan)->make_quick(param, false, &quick_roru->alloc);
    if (not quick)
      return NULL;
    quick_roru->push_quick_back(quick);
  }
  quick_roru->records= records;
  quick_roru->read_time= read_cost;
  return quick_roru;
}


/*
  Build a optimizer::SEL_TREE for <> or NOT BETWEEN predicate

  SYNOPSIS
    get_ne_mm_tree()
      param       Parameter from SqlSelect::test_quick_select
      cond_func   item for the predicate
      field       field in the predicate
      lt_value    constant that field should be smaller
      gt_value    constant that field should be greaterr
      cmp_type    compare type for the field

  RETURN
    #  Pointer to tree built tree
    0  on error
*/
static optimizer::SEL_TREE *get_ne_mm_tree(optimizer::RangeParameter *param,
                                Item_func *cond_func,
                                Field *field,
                                Item *lt_value, Item *gt_value,
                                Item_result cmp_type)
{
  optimizer::SEL_TREE *tree= NULL;
  tree= get_mm_parts(param, cond_func, field, Item_func::LT_FUNC,
                     lt_value, cmp_type);
  if (tree)
  {
    tree= tree_or(param,
                  tree,
                  get_mm_parts(param, cond_func, field,
					        Item_func::GT_FUNC,
					        gt_value,
                  cmp_type));
  }
  return tree;
}


/*
  Build a optimizer::SEL_TREE for a simple predicate

  SYNOPSIS
    get_func_mm_tree()
      param       Parameter from SqlSelect::test_quick_select
      cond_func   item for the predicate
      field       field in the predicate
      value       constant in the predicate
      cmp_type    compare type for the field
      inv         true <> NOT cond_func is considered
                  (makes sense only when cond_func is BETWEEN or IN)

  RETURN
    Pointer to the tree built tree
*/
static optimizer::SEL_TREE *get_func_mm_tree(optimizer::RangeParameter *param,
                                  Item_func *cond_func,
                                  Field *field,
                                  Item *value,
                                  Item_result cmp_type,
                                  bool inv)
{
  optimizer::SEL_TREE *tree= NULL;

  switch (cond_func->functype())
  {

  case Item_func::NE_FUNC:
    tree= get_ne_mm_tree(param, cond_func, field, value, value, cmp_type);
    break;

  case Item_func::BETWEEN:
  {
    if (! value)
    {
      if (inv)
      {
        tree= get_ne_mm_tree(param,
                             cond_func,
                             field,
                             cond_func->arguments()[1],
                             cond_func->arguments()[2],
                             cmp_type);
      }
      else
      {
        tree= get_mm_parts(param,
                           cond_func,
                           field,
                           Item_func::GE_FUNC,
		                       cond_func->arguments()[1],
                           cmp_type);
        if (tree)
        {
          tree= tree_and(param,
                         tree,
                         get_mm_parts(param, cond_func, field,
					               Item_func::LE_FUNC,
					               cond_func->arguments()[2],
                         cmp_type));
        }
      }
    }
    else
      tree= get_mm_parts(param,
                         cond_func,
                         field,
                         (inv ?
                          (value == (Item*)1 ? Item_func::GT_FUNC :
                                               Item_func::LT_FUNC):
                          (value == (Item*)1 ? Item_func::LE_FUNC :
                                               Item_func::GE_FUNC)),
                         cond_func->arguments()[0],
                         cmp_type);
    break;
  }
  case Item_func::IN_FUNC:
  {
    Item_func_in *func= (Item_func_in*) cond_func;

    /*
      Array for IN() is constructed when all values have the same result
      type. Tree won't be built for values with different result types,
      so we check it here to avoid unnecessary work.
    */
    if (! func->arg_types_compatible)
      break;

    if (inv)
    {
      if (func->array && func->array->result_type() != ROW_RESULT)
      {
        /*
          We get here for conditions in form "t.key NOT IN (c1, c2, ...)",
          where c{i} are constants. Our goal is to produce a optimizer::SEL_TREE that
          represents intervals:

          ($MIN<t.key<c1) OR (c1<t.key<c2) OR (c2<t.key<c3) OR ...    (*)

          where $MIN is either "-inf" or NULL.

          The most straightforward way to produce it is to convert NOT IN
          into "(t.key != c1) AND (t.key != c2) AND ... " and let the range
          analyzer to build optimizer::SEL_TREE from that. The problem is that the
          range analyzer will use O(N^2) memory (which is probably a bug),
          and people do use big NOT IN lists (e.g. see BUG#15872, BUG#21282),
          will run out of memory.

          Another problem with big lists like (*) is that a big list is
          unlikely to produce a good "range" access, while considering that
          range access will require expensive CPU calculations (and for
          MyISAM even index accesses). In short, big NOT IN lists are rarely
          worth analyzing.

          Considering the above, we'll handle NOT IN as follows:
          * if the number of entries in the NOT IN list is less than
            NOT_IN_IGNORE_THRESHOLD, construct the optimizer::SEL_TREE (*) manually.
          * Otherwise, don't produce a optimizer::SEL_TREE.
        */
#define NOT_IN_IGNORE_THRESHOLD 1000
        memory::Root *tmp_root= param->mem_root;
        param->session->mem_root= param->old_root;
        /*
          Create one Item_type constant object. We'll need it as
          get_mm_parts only accepts constant values wrapped in Item_Type
          objects.
          We create the Item on param->mem_root which points to
          per-statement mem_root (while session->mem_root is currently pointing
          to mem_root local to range optimizer).
        */
        Item *value_item= func->array->create_item();
        param->session->mem_root= tmp_root;

        if (func->array->count > NOT_IN_IGNORE_THRESHOLD || ! value_item)
          break;

        /* Get a optimizer::SEL_TREE for "(-inf|NULL) < X < c_0" interval.  */
        uint32_t i=0;
        do
        {
          func->array->value_to_item(i, value_item);
          tree= get_mm_parts(param,
                             cond_func,
                             field, Item_func::LT_FUNC,
                             value_item,
                             cmp_type);
          if (! tree)
            break;
          i++;
        } while (i < func->array->count && tree->type == optimizer::SEL_TREE::IMPOSSIBLE);

        if (!tree || tree->type == optimizer::SEL_TREE::IMPOSSIBLE)
        {
          /* We get here in cases like "t.unsigned NOT IN (-1,-2,-3) */
          tree= NULL;
          break;
        }
        optimizer::SEL_TREE *tree2= NULL;
        for (; i < func->array->count; i++)
        {
          if (func->array->compare_elems(i, i-1))
          {
            /* Get a optimizer::SEL_TREE for "-inf < X < c_i" interval */
            func->array->value_to_item(i, value_item);
            tree2= get_mm_parts(param, cond_func, field, Item_func::LT_FUNC,
                                value_item, cmp_type);
            if (!tree2)
            {
              tree= NULL;
              break;
            }

            /* Change all intervals to be "c_{i-1} < X < c_i" */
            for (uint32_t idx= 0; idx < param->keys; idx++)
            {
              optimizer::SEL_ARG *new_interval, *last_val;
              if (((new_interval= tree2->keys[idx])) &&
                  (tree->keys[idx]) &&
                  ((last_val= tree->keys[idx]->last())))
              {
                new_interval->min_value= last_val->max_value;
                new_interval->min_flag= NEAR_MIN;
              }
            }
            /*
              The following doesn't try to allocate memory so no need to
              check for NULL.
            */
            tree= tree_or(param, tree, tree2);
          }
        }

        if (tree && tree->type != optimizer::SEL_TREE::IMPOSSIBLE)
        {
          /*
            Get the optimizer::SEL_TREE for the last "c_last < X < +inf" interval
            (value_item cotains c_last already)
          */
          tree2= get_mm_parts(param, cond_func, field, Item_func::GT_FUNC,
                              value_item, cmp_type);
          tree= tree_or(param, tree, tree2);
        }
      }
      else
      {
        tree= get_ne_mm_tree(param, cond_func, field,
                             func->arguments()[1], func->arguments()[1],
                             cmp_type);
        if (tree)
        {
          Item **arg, **end;
          for (arg= func->arguments()+2, end= arg+func->argument_count()-2;
               arg < end ; arg++)
          {
            tree=  tree_and(param, tree, get_ne_mm_tree(param, cond_func, field,
                                                        *arg, *arg, cmp_type));
          }
        }
      }
    }
    else
    {
      tree= get_mm_parts(param, cond_func, field, Item_func::EQ_FUNC,
                         func->arguments()[1], cmp_type);
      if (tree)
      {
        Item **arg, **end;
        for (arg= func->arguments()+2, end= arg+func->argument_count()-2;
             arg < end ; arg++)
        {
          tree= tree_or(param, tree, get_mm_parts(param, cond_func, field,
                                                  Item_func::EQ_FUNC,
                                                  *arg, cmp_type));
        }
      }
    }
    break;
  }
  default:
  {
    /*
       Here the function for the following predicates are processed:
       <, <=, =, >=, >, LIKE, IS NULL, IS NOT NULL.
       If the predicate is of the form (value op field) it is handled
       as the equivalent predicate (field rev_op value), e.g.
       2 <= a is handled as a >= 2.
    */
    Item_func::Functype func_type=
      (value != cond_func->arguments()[0]) ? cond_func->functype() :
        ((Item_bool_func2*) cond_func)->rev_functype();
    tree= get_mm_parts(param, cond_func, field, func_type, value, cmp_type);
  }
  }

  return(tree);
}


/*
  Build conjunction of all optimizer::SEL_TREEs for a simple predicate applying equalities

  SYNOPSIS
    get_full_func_mm_tree()
      param       Parameter from SqlSelect::test_quick_select
      cond_func   item for the predicate
      field_item  field in the predicate
      value       constant in the predicate
                  (for BETWEEN it contains the number of the field argument,
                   for IN it's always 0)
      inv         true <> NOT cond_func is considered
                  (makes sense only when cond_func is BETWEEN or IN)

  DESCRIPTION
    For a simple SARGable predicate of the form (f op c), where f is a field and
    c is a constant, the function builds a conjunction of all optimizer::SEL_TREES that can
    be obtained by the substitution of f for all different fields equal to f.

  NOTES
    If the WHERE condition contains a predicate (fi op c),
    then not only SELL_TREE for this predicate is built, but
    the trees for the results of substitution of fi for
    each fj belonging to the same multiple equality as fi
    are built as well.
    E.g. for WHERE t1.a=t2.a AND t2.a > 10
    a optimizer::SEL_TREE for t2.a > 10 will be built for quick select from t2
    and
    a optimizer::SEL_TREE for t1.a > 10 will be built for quick select from t1.

    A BETWEEN predicate of the form (fi [NOT] BETWEEN c1 AND c2) is treated
    in a similar way: we build a conjuction of trees for the results
    of all substitutions of fi for equal fj.
    Yet a predicate of the form (c BETWEEN f1i AND f2i) is processed
    differently. It is considered as a conjuction of two SARGable
    predicates (f1i <= c) and (f2i <=c) and the function get_full_func_mm_tree
    is called for each of them separately producing trees for
       AND j (f1j <=c ) and AND j (f2j <= c)
    After this these two trees are united in one conjunctive tree.
    It's easy to see that the same tree is obtained for
       AND j,k (f1j <=c AND f2k<=c)
    which is equivalent to
       AND j,k (c BETWEEN f1j AND f2k).
    The validity of the processing of the predicate (c NOT BETWEEN f1i AND f2i)
    which equivalent to (f1i > c OR f2i < c) is not so obvious. Here the
    function get_full_func_mm_tree is called for (f1i > c) and (f2i < c)
    producing trees for AND j (f1j > c) and AND j (f2j < c). Then this two
    trees are united in one OR-tree. The expression
      (AND j (f1j > c) OR AND j (f2j < c)
    is equivalent to the expression
      AND j,k (f1j > c OR f2k < c)
    which is just a translation of
      AND j,k (c NOT BETWEEN f1j AND f2k)

    In the cases when one of the items f1, f2 is a constant c1 we do not create
    a tree for it at all. It works for BETWEEN predicates but does not
    work for NOT BETWEEN predicates as we have to evaluate the expression
    with it. If it is true then the other tree can be completely ignored.
    We do not do it now and no trees are built in these cases for
    NOT BETWEEN predicates.

    As to IN predicates only ones of the form (f IN (c1,...,cn)),
    where f1 is a field and c1,...,cn are constant, are considered as
    SARGable. We never try to narrow the index scan using predicates of
    the form (c IN (c1,...,f,...,cn)).

  RETURN
    Pointer to the tree representing the built conjunction of optimizer::SEL_TREEs
*/

static optimizer::SEL_TREE *get_full_func_mm_tree(optimizer::RangeParameter *param,
                                       Item_func *cond_func,
                                       Item_field *field_item, Item *value,
                                       bool inv)
{
  optimizer::SEL_TREE *tree= 0;
  optimizer::SEL_TREE *ftree= 0;
  table_map ref_tables= 0;
  table_map param_comp= ~(param->prev_tables | param->read_tables |
		          param->current_table);

  for (uint32_t i= 0; i < cond_func->arg_count; i++)
  {
    Item *arg= cond_func->arguments()[i]->real_item();
    if (arg != field_item)
      ref_tables|= arg->used_tables();
  }

  Field *field= field_item->field;
  field->setWriteSet();

  Item_result cmp_type= field->cmp_type();
  if (!((ref_tables | field->getTable()->map) & param_comp))
    ftree= get_func_mm_tree(param, cond_func, field, value, cmp_type, inv);
  Item_equal *item_equal= field_item->item_equal;
  if (item_equal)
  {
    Item_equal_iterator it(item_equal->begin());
    Item_field *item;
    while ((item= it++))
    {
      Field *f= item->field;
      f->setWriteSet();

      if (field->eq(f))
        continue;
      if (!((ref_tables | f->getTable()->map) & param_comp))
      {
        tree= get_func_mm_tree(param, cond_func, f, value, cmp_type, inv);
        ftree= !ftree ? tree : tree_and(param, ftree, tree);
      }
    }
  }
  return(ftree);
}

	/* make a select tree of all keys in condition */

static optimizer::SEL_TREE *get_mm_tree(optimizer::RangeParameter *param, COND *cond)
{
  optimizer::SEL_TREE *tree=0;
  optimizer::SEL_TREE *ftree= 0;
  Item_field *field_item= 0;
  bool inv= false;
  Item *value= 0;

  if (cond->type() == Item::COND_ITEM)
  {
    List<Item>::iterator li(((Item_cond*) cond)->argument_list()->begin());

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      tree=0;
      while (Item* item=li++)
      {
	optimizer::SEL_TREE *new_tree= get_mm_tree(param,item);
	if (param->session->is_fatal_error ||
            param->alloced_sel_args > optimizer::SEL_ARG::MAX_SEL_ARGS)
	  return 0;	// out of memory
	tree=tree_and(param,tree,new_tree);
	if (tree && tree->type == optimizer::SEL_TREE::IMPOSSIBLE)
	  break;
      }
    }
    else
    {						// COND OR
      tree= get_mm_tree(param,li++);
      if (tree)
      {
	while (Item* item= li++)
	{
	  optimizer::SEL_TREE *new_tree= get_mm_tree(param,item);
	  if (!new_tree)
	    return 0;	// out of memory
	  tree=tree_or(param,tree,new_tree);
	  if (!tree || tree->type == optimizer::SEL_TREE::ALWAYS)
	    break;
	}
      }
    }
    return(tree);
  }
  /* Here when simple cond
     There are limits on what kinds of const items we can evaluate, grep for
     DontEvaluateMaterializedSubqueryTooEarly.
  */
  if (cond->const_item()  && !cond->is_expensive())
  {
    /*
      During the cond->val_int() evaluation we can come across a subselect
      item which may allocate memory on the session->mem_root and assumes
      all the memory allocated has the same life span as the subselect
      item itself. So we have to restore the thread's mem_root here.
    */
    memory::Root *tmp_root= param->mem_root;
    param->session->mem_root= param->old_root;
    tree= cond->val_int() ? new(tmp_root) optimizer::SEL_TREE(optimizer::SEL_TREE::ALWAYS) :
                            new(tmp_root) optimizer::SEL_TREE(optimizer::SEL_TREE::IMPOSSIBLE);
    param->session->mem_root= tmp_root;
    return(tree);
  }

  table_map ref_tables= 0;
  table_map param_comp= ~(param->prev_tables | param->read_tables |
		          param->current_table);
  if (cond->type() != Item::FUNC_ITEM)
  {						// Should be a field
    ref_tables= cond->used_tables();
    if ((ref_tables & param->current_table) ||
	(ref_tables & ~(param->prev_tables | param->read_tables)))
      return 0;
    return(new optimizer::SEL_TREE(optimizer::SEL_TREE::MAYBE));
  }

  Item_func *cond_func= (Item_func*) cond;
  if (cond_func->functype() == Item_func::BETWEEN ||
      cond_func->functype() == Item_func::IN_FUNC)
    inv= ((Item_func_opt_neg *) cond_func)->negated;
  else if (cond_func->select_optimize() == Item_func::OPTIMIZE_NONE)
    return 0;

  param->cond= cond;

  switch (cond_func->functype()) {
  case Item_func::BETWEEN:
    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM)
    {
      field_item= (Item_field*) (cond_func->arguments()[0]->real_item());
      ftree= get_full_func_mm_tree(param, cond_func, field_item, NULL, inv);
    }

    /*
      Concerning the code below see the NOTES section in
      the comments for the function get_full_func_mm_tree()
    */
    for (uint32_t i= 1 ; i < cond_func->arg_count ; i++)
    {
      if (cond_func->arguments()[i]->real_item()->type() == Item::FIELD_ITEM)
      {
        field_item= (Item_field*) (cond_func->arguments()[i]->real_item());
        optimizer::SEL_TREE *tmp= get_full_func_mm_tree(param, cond_func,
                                    field_item, (Item*)(intptr_t)i, inv);
        if (inv)
          tree= !tree ? tmp : tree_or(param, tree, tmp);
        else
          tree= tree_and(param, tree, tmp);
      }
      else if (inv)
      {
        tree= 0;
        break;
      }
    }

    ftree = tree_and(param, ftree, tree);
    break;
  case Item_func::IN_FUNC:
  {
    Item_func_in *func=(Item_func_in*) cond_func;
    if (func->key_item()->real_item()->type() != Item::FIELD_ITEM)
      return 0;
    field_item= (Item_field*) (func->key_item()->real_item());
    ftree= get_full_func_mm_tree(param, cond_func, field_item, NULL, inv);
    break;
  }
  case Item_func::MULT_EQUAL_FUNC:
  {
    Item_equal *item_equal= (Item_equal *) cond;
    if (!(value= item_equal->get_const()))
      return 0;
    Item_equal_iterator it(item_equal->begin());
    ref_tables= value->used_tables();
    while ((field_item= it++))
    {
      Field *field= field_item->field;
      field->setWriteSet();

      Item_result cmp_type= field->cmp_type();
      if (!((ref_tables | field->getTable()->map) & param_comp))
      {
        tree= get_mm_parts(param, cond, field, Item_func::EQ_FUNC,
		           value,cmp_type);
        ftree= !ftree ? tree : tree_and(param, ftree, tree);
      }
    }

    return(ftree);
  }
  default:
    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM)
    {
      field_item= (Item_field*) (cond_func->arguments()[0]->real_item());
      value= cond_func->arg_count > 1 ? cond_func->arguments()[1] : 0;
    }
    else if (cond_func->have_rev_func() &&
             cond_func->arguments()[1]->real_item()->type() ==
                                                            Item::FIELD_ITEM)
    {
      field_item= (Item_field*) (cond_func->arguments()[1]->real_item());
      value= cond_func->arguments()[0];
    }
    else
      return 0;
    ftree= get_full_func_mm_tree(param, cond_func, field_item, value, inv);
  }

  return(ftree);
}


static optimizer::SEL_TREE *
get_mm_parts(optimizer::RangeParameter *param,
             COND *cond_func,
             Field *field,
	           Item_func::Functype type,
	           Item *value, Item_result)
{
  if (field->getTable() != param->table)
    return 0;

  KEY_PART *key_part = param->key_parts;
  KEY_PART *end = param->key_parts_end;
  optimizer::SEL_TREE *tree=0;
  if (value &&
      value->used_tables() & ~(param->prev_tables | param->read_tables))
    return 0;
  for (; key_part != end; key_part++)
  {
    if (field->eq(key_part->field))
    {
      optimizer::SEL_ARG *sel_arg=0;
      if (!tree)
        tree= new optimizer::SEL_TREE;
      if (!value || !(value->used_tables() & ~param->read_tables))
      {
        sel_arg= get_mm_leaf(param,cond_func, key_part->field,key_part,type,value);
        if (! sel_arg)
          continue;
        if (sel_arg->type == optimizer::SEL_ARG::IMPOSSIBLE)
        {
          tree->type=optimizer::SEL_TREE::IMPOSSIBLE;
          return(tree);
        }
      }
      else
      {
        // This key may be used later
        sel_arg= new optimizer::SEL_ARG(optimizer::SEL_ARG::MAYBE_KEY);
      }
      sel_arg->part=(unsigned char) key_part->part;
      tree->keys[key_part->key]=sel_add(tree->keys[key_part->key],sel_arg);
      tree->keys_map.set(key_part->key);
    }
  }

  return tree;
}


static optimizer::SEL_ARG *
get_mm_leaf(optimizer::RangeParameter *param,
            COND *conf_func,
            Field *field,
            KEY_PART *key_part,
            Item_func::Functype type,
            Item *value)
{
  uint32_t maybe_null=(uint32_t) field->real_maybe_null();
  bool optimize_range;
  optimizer::SEL_ARG *tree= NULL;
  memory::Root *alloc= param->mem_root;
  unsigned char *str;
  int err= 0;

  /*
    We need to restore the runtime mem_root of the thread in this
    function because it evaluates the value of its argument, while
    the argument can be any, e.g. a subselect. The subselect
    items, in turn, assume that all the memory allocated during
    the evaluation has the same life span as the item itself.
    TODO: opitimizer/range.cc should not reset session->mem_root at all.
  */
  param->session->mem_root= param->old_root;
  if (!value)					// IS NULL or IS NOT NULL
  {
    if (field->getTable()->maybe_null)		// Can't use a key on this
      goto end;
    if (!maybe_null)				// Not null field
    {
      if (type == Item_func::ISNULL_FUNC)
        tree= &optimizer::null_element;
      goto end;
    }
    tree= new (*alloc) optimizer::SEL_ARG(field,is_null_string,is_null_string);
    if (type == Item_func::ISNOTNULL_FUNC)
    {
      tree->min_flag=NEAR_MIN;		    /* IS NOT NULL ->  X > NULL */
      tree->max_flag=NO_MAX_RANGE;
    }
    goto end;
  }

  /*
    1. Usually we can't use an index if the column collation
       differ from the operation collation.

    2. However, we can reuse a case insensitive index for
       the binary searches:

       WHERE latin1_swedish_ci_column = 'a' COLLATE lati1_bin;

       WHERE latin1_swedish_ci_colimn = BINARY 'a '

  */
  if (field->result_type() == STRING_RESULT &&
      value->result_type() == STRING_RESULT &&
      ((Field_str*)field)->charset() != conf_func->compare_collation() &&
      !(conf_func->compare_collation()->state & MY_CS_BINSORT))
    goto end;

  if (param->using_real_indexes)
    optimize_range= field->optimize_range(param->real_keynr[key_part->key], key_part->part);
  else
    optimize_range= true;

  if (type == Item_func::LIKE_FUNC)
  {
    bool like_error;
    char buff1[MAX_FIELD_WIDTH];
    unsigned char *min_str,*max_str;
    String tmp(buff1,sizeof(buff1),value->collation.collation),*res;
    size_t length, offset, min_length, max_length;
    uint32_t field_length= field->pack_length()+maybe_null;

    if (!optimize_range)
      goto end;
    if (!(res= value->val_str(&tmp)))
    {
      tree= &optimizer::null_element;
      goto end;
    }

    /*
      TODO:
      Check if this was a function. This should have be optimized away
      in the sql_select.cc
    */
    if (res != &tmp)
    {
      tmp.copy(*res);				// Get own copy
      res= &tmp;
    }
    if (field->cmp_type() != STRING_RESULT)
      goto end;                                 // Can only optimize strings

    offset=maybe_null;
    length=key_part->store_length;

    if (length != key_part->length  + maybe_null)
    {
      /* key packed with length prefix */
      offset+= HA_KEY_BLOB_LENGTH;
      field_length= length - HA_KEY_BLOB_LENGTH;
    }
    else
    {
      if (unlikely(length < field_length))
      {
        /*
          This can only happen in a table created with UNIREG where one key
          overlaps many fields
        */
        length= field_length;
      }
      else
        field_length= length;
    }
    length+=offset;
    min_str= alloc->alloc(length*2);
    max_str=min_str+length;
    if (maybe_null)
      max_str[0]= min_str[0]=0;

    field_length-= maybe_null;
    int escape_code= make_escape_code(field->charset(),
                                      ((Item_func_like*)(param->cond))->escape);
    like_error= my_like_range(field->charset(),
                              res->ptr(), res->length(),
                              escape_code,
                              internal::wild_one, internal::wild_many,
                              field_length,
                              (char*) min_str+offset, (char*) max_str+offset,
                              &min_length, &max_length);
    if (like_error)				// Can't optimize with LIKE
      goto end;

    if (offset != maybe_null)			// BLOB or VARCHAR
    {
      int2store(min_str+maybe_null,min_length);
      int2store(max_str+maybe_null,max_length);
    }
    tree= new (alloc) optimizer::SEL_ARG(field, min_str, max_str);
    goto end;
  }

  if (! optimize_range &&
      type != Item_func::EQ_FUNC &&
      type != Item_func::EQUAL_FUNC)
    goto end;                                   // Can't optimize this

  /*
    We can't always use indexes when comparing a string index to a number
    cmp_type() is checked to allow compare of dates to numbers
  */
  if (field->result_type() == STRING_RESULT &&
      value->result_type() != STRING_RESULT &&
      field->cmp_type() != value->result_type())
    goto end;

  /*
   * Some notes from Jay...
   *
   * OK, so previously, and in MySQL, what the optimizer does here is
   * override the sql_mode variable to ignore out-of-range or bad date-
   * time values.  It does this because the optimizer is populating the
   * field variable with the incoming value from the comparison field,
   * and the value may exceed the bounds of a proper column type.
   *
   * For instance, assume the following:
   *
   * CREATE TABLE t1 (ts TIMESTAMP);
   * INSERT INTO t1 ('2009-03-04 00:00:00');
   * CREATE TABLE t2 (dt1 DATETIME, dt2 DATETIME);
   * INSERT INT t2 ('2003-12-31 00:00:00','2999-12-31 00:00:00');
   *
   * If we issue this query:
   *
   * SELECT * FROM t1, t2 WHERE t1.ts BETWEEN t2.dt1 AND t2.dt2;
   *
   * We will come into bounds issues.  Field_timestamp::store() will be
   * called with a datetime value of "2999-12-31 00:00:00" and will throw
   * an error for out-of-bounds.  MySQL solves this via a hack with sql_mode
   * but Drizzle always throws errors on bad data storage in a Field class.
   *
   * Therefore, to get around the problem of the Field class being used for
   * "storage" here without actually storing anything...we must check to see
   * if the value being stored in a Field_timestamp here is out of range.  If
   * it is, then we must convert to the highest Timestamp value (or lowest,
   * depending on whether the datetime is before or after the epoch.
   */
  if (field->is_timestamp())
  {
    /*
     * The left-side of the range comparison is a timestamp field.  Therefore,
     * we must check to see if the value in the right-hand side is outside the
     * range of the UNIX epoch, and cut to the epoch bounds if it is.
     */
    /* Datetime and date columns are Item::FIELD_ITEM ... and have a result type of STRING_RESULT */
    if (value->real_item()->type() == Item::FIELD_ITEM
        && value->result_type() == STRING_RESULT)
    {
      char buff[DateTime::MAX_STRING_LENGTH];
      String tmp(buff, sizeof(buff), &my_charset_bin);
      String *res= value->val_str(&tmp);

      if (!res)
        goto end;
      else
      {
        /*
         * Create a datetime from the string and compare to fixed timestamp
         * instances representing the epoch boundaries.
         */
        DateTime value_datetime;

        if (! value_datetime.from_string(res->c_ptr(), (size_t) res->length()))
          goto end;

        Timestamp max_timestamp;
        Timestamp min_timestamp;

        (void) max_timestamp.from_time_t((time_t) INT32_MAX);
        (void) min_timestamp.from_time_t((time_t) 0);

        /* We rely on Temporal class operator overloads to do our comparisons. */
        if (value_datetime < min_timestamp)
        {
          /*
           * Datetime in right-hand side column is before UNIX epoch, so adjust to
           * lower bound.
           */
          char new_value_buff[DateTime::MAX_STRING_LENGTH];
          int new_value_length;
          String new_value_string(new_value_buff, sizeof(new_value_buff), &my_charset_bin);

          new_value_length= min_timestamp.to_string(new_value_string.c_ptr(),
				    DateTime::MAX_STRING_LENGTH);
	  assert((new_value_length+1) < DateTime::MAX_STRING_LENGTH);
          new_value_string.length(new_value_length);
          err= value->save_str_value_in_field(field, &new_value_string);
        }
        else if (value_datetime > max_timestamp)
        {
          /*
           * Datetime in right hand side column is after UNIX epoch, so adjust
           * to the higher bound of the epoch.
           */
          char new_value_buff[DateTime::MAX_STRING_LENGTH];
          int new_value_length;
          String new_value_string(new_value_buff, sizeof(new_value_buff), &my_charset_bin);

          new_value_length= max_timestamp.to_string(new_value_string.c_ptr(),
					DateTime::MAX_STRING_LENGTH);
	  assert((new_value_length+1) < DateTime::MAX_STRING_LENGTH);
          new_value_string.length(new_value_length);
          err= value->save_str_value_in_field(field, &new_value_string);
        }
        else
          err= value->save_in_field(field, 1);
      }
    }
    else /* Not a datetime -> timestamp comparison */
      err= value->save_in_field(field, 1);
  }
  else /* Not a timestamp comparison */
    err= value->save_in_field(field, 1);

  if (err > 0)
  {
    if (field->cmp_type() != value->result_type())
    {
      if ((type == Item_func::EQ_FUNC || type == Item_func::EQUAL_FUNC) &&
          value->result_type() == item_cmp_type(field->result_type(),
                                                value->result_type()))
      {
        tree= new (alloc) optimizer::SEL_ARG(field, 0, 0);
        tree->type= optimizer::SEL_ARG::IMPOSSIBLE;
        goto end;
      }
      else
      {
        /*
          TODO: We should return trees of the type SEL_ARG::IMPOSSIBLE
          for the cases like int_field > 999999999999999999999999 as well.
        */
        tree= 0;
        if (err == 3 && field->type() == DRIZZLE_TYPE_DATE &&
            (type == Item_func::GT_FUNC || type == Item_func::GE_FUNC ||
             type == Item_func::LT_FUNC || type == Item_func::LE_FUNC) )
        {
          /*
            We were saving DATETIME into a DATE column, the conversion went ok
            but a non-zero time part was cut off.

            In MySQL's SQL dialect, DATE and DATETIME are compared as datetime
            values. Index over a DATE column uses DATE comparison. Changing
            from one comparison to the other is possible:

            datetime(date_col)< '2007-12-10 12:34:55' -> date_col<='2007-12-10'
            datetime(date_col)<='2007-12-10 12:34:55' -> date_col<='2007-12-10'

            datetime(date_col)> '2007-12-10 12:34:55' -> date_col>='2007-12-10'
            datetime(date_col)>='2007-12-10 12:34:55' -> date_col>='2007-12-10'

            but we'll need to convert '>' to '>=' and '<' to '<='. This will
            be done together with other types at the end of this function
            (grep for field_is_equal_to_item)
          */
        }
        else
          goto end;
      }
    }

    /*
      guaranteed at this point:  err > 0; field and const of same type
      If an integer got bounded (e.g. to within 0..255 / -128..127)
      for < or >, set flags as for <= or >= (no NEAR_MAX / NEAR_MIN)
    */
    else if (err == 1 && field->result_type() == INT_RESULT)
    {
      if (type == Item_func::LT_FUNC && (value->val_int() > 0))
        type = Item_func::LE_FUNC;
      else if (type == Item_func::GT_FUNC &&
               !((Field_num*)field)->unsigned_flag &&
               !((Item_int*)value)->unsigned_flag &&
               (value->val_int() < 0))
        type = Item_func::GE_FUNC;
    }
    else if (err == 1)
    {
      tree= new (alloc) optimizer::SEL_ARG(field, 0, 0);
      tree->type= optimizer::SEL_ARG::IMPOSSIBLE;
      goto end;
    }
  }
  else if (err < 0)
  {
    /* This happens when we try to insert a NULL field in a not null column */
    tree= &optimizer::null_element;                        // cmp with NULL is never true
    goto end;
  }

  /*
    Any predicate except "<=>"(null-safe equality operator) involving NULL as a
    constant is always FALSE
    Put IMPOSSIBLE Tree(null_element) here.
  */
  if (type != Item_func::EQUAL_FUNC && field->is_real_null())
  {
    tree= &optimizer::null_element;
    goto end;
  }

  str= alloc->alloc(key_part->store_length+1);
  if (maybe_null)
    *str= field->is_real_null();        // Set to 1 if null
  field->get_key_image(str+maybe_null, key_part->length);
  tree= new (alloc) optimizer::SEL_ARG(field, str, str);

  /*
    Check if we are comparing an UNSIGNED integer with a negative constant.
    In this case we know that:
    (a) (unsigned_int [< | <=] negative_constant) == false
    (b) (unsigned_int [> | >=] negative_constant) == true
    In case (a) the condition is false for all values, and in case (b) it
    is true for all values, so we can avoid unnecessary retrieval and condition
    testing, and we also get correct comparison of unsinged integers with
    negative integers (which otherwise fails because at query execution time
    negative integers are cast to unsigned if compared with unsigned).
   */
  if (field->result_type() == INT_RESULT &&
      value->result_type() == INT_RESULT &&
      ((Field_num*)field)->unsigned_flag && !((Item_int*)value)->unsigned_flag)
  {
    int64_t item_val= value->val_int();
    if (item_val < 0)
    {
      if (type == Item_func::LT_FUNC || type == Item_func::LE_FUNC)
      {
        tree->type= optimizer::SEL_ARG::IMPOSSIBLE;
        goto end;
      }
      if (type == Item_func::GT_FUNC || type == Item_func::GE_FUNC)
      {
        tree= 0;
        goto end;
      }
    }
  }

  switch (type) {
  case Item_func::LT_FUNC:
    if (field_is_equal_to_item(field,value))
      tree->max_flag=NEAR_MAX;
    /* fall through */
  case Item_func::LE_FUNC:
    if (!maybe_null)
      tree->min_flag=NO_MIN_RANGE;		/* From start */
    else
    {						// > NULL
      tree->min_value=is_null_string;
      tree->min_flag=NEAR_MIN;
    }
    break;
  case Item_func::GT_FUNC:
    /* Don't use open ranges for partial key_segments */
    if (field_is_equal_to_item(field,value) &&
        !(key_part->flag & HA_PART_KEY_SEG))
      tree->min_flag=NEAR_MIN;
    /* fall through */
  case Item_func::GE_FUNC:
    tree->max_flag=NO_MAX_RANGE;
    break;
  default:
    break;
  }

end:
  param->session->mem_root= alloc;
  return(tree);
}


/******************************************************************************
** Tree manipulation functions
** If tree is 0 it means that the condition can't be tested. It refers
** to a non existent table or to a field in current table with isn't a key.
** The different tree flags:
** IMPOSSIBLE:	 Condition is never true
** ALWAYS:	 Condition is always true
** MAYBE:	 Condition may exists when tables are read
** MAYBE_KEY:	 Condition refers to a key that may be used in join loop
** KEY_RANGE:	 Condition uses a key
******************************************************************************/

/*
  Add a new key test to a key when scanning through all keys
  This will never be called for same key parts.
*/

static optimizer::SEL_ARG *
sel_add(optimizer::SEL_ARG *key1, optimizer::SEL_ARG *key2)
{
  optimizer::SEL_ARG *root= NULL;
  optimizer::SEL_ARG **key_link= NULL;

  if (!key1)
    return key2;
  if (!key2)
    return key1;

  key_link= &root;
  while (key1 && key2)
  {
    if (key1->part < key2->part)
    {
      *key_link= key1;
      key_link= &key1->next_key_part;
      key1=key1->next_key_part;
    }
    else
    {
      *key_link= key2;
      key_link= &key2->next_key_part;
      key2=key2->next_key_part;
    }
  }
  *key_link=key1 ? key1 : key2;
  return root;
}

#define CLONE_KEY1_MAYBE 1
#define CLONE_KEY2_MAYBE 2

static uint32_t swap_clone_flag(uint32_t a)
{
  return ((a & 1) << 1) | ((a & 2) >> 1);
}

static optimizer::SEL_TREE *
tree_and(optimizer::RangeParameter *param, optimizer::SEL_TREE *tree1, optimizer::SEL_TREE *tree2)
{
  if (!tree1)
    return(tree2);
  if (!tree2)
    return(tree1);
  if (tree1->type == optimizer::SEL_TREE::IMPOSSIBLE || tree2->type == optimizer::SEL_TREE::ALWAYS)
    return(tree1);
  if (tree2->type == optimizer::SEL_TREE::IMPOSSIBLE || tree1->type == optimizer::SEL_TREE::ALWAYS)
    return(tree2);
  if (tree1->type == optimizer::SEL_TREE::MAYBE)
  {
    if (tree2->type == optimizer::SEL_TREE::KEY)
      tree2->type=optimizer::SEL_TREE::KEY_SMALLER;
    return(tree2);
  }
  if (tree2->type == optimizer::SEL_TREE::MAYBE)
  {
    tree1->type=optimizer::SEL_TREE::KEY_SMALLER;
    return(tree1);
  }
  key_map  result_keys;
  result_keys.reset();

  /* Join the trees key per key */
  optimizer::SEL_ARG **key1,**key2,**end;
  for (key1= tree1->keys,key2= tree2->keys,end=key1+param->keys ;
       key1 != end ; key1++,key2++)
  {
    uint32_t flag=0;
    if (*key1 || *key2)
    {
      if (*key1 && !(*key1)->simple_key())
        flag|=CLONE_KEY1_MAYBE;
      if (*key2 && !(*key2)->simple_key())
        flag|=CLONE_KEY2_MAYBE;
      *key1=key_and(param, *key1, *key2, flag);
      if (*key1 && (*key1)->type == optimizer::SEL_ARG::IMPOSSIBLE)
      {
        tree1->type= optimizer::SEL_TREE::IMPOSSIBLE;
        return(tree1);
      }
      result_keys.set(key1 - tree1->keys);
    }
  }
  tree1->keys_map= result_keys;
  /* dispose index_merge if there is a "range" option */
  if (result_keys.any())
  {
    tree1->merges.clear();
    return(tree1);
  }

  /* ok, both trees are index_merge trees */
  imerge_list_and_list(&tree1->merges, &tree2->merges);
  return(tree1);
}



/* And key trees where key1->part < key2 -> part */

static optimizer::SEL_ARG *
and_all_keys(optimizer::RangeParameter *param,
             optimizer::SEL_ARG *key1,
             optimizer::SEL_ARG *key2,
             uint32_t clone_flag)
{
  optimizer::SEL_ARG *next= NULL;
  ulong use_count=key1->use_count;

  if (key1->size() != 1)
  {
    key2->use_count+=key1->size()-1; //psergey: why we don't count that key1 has n-k-p?
    key2->increment_use_count((int) key1->size()-1);
  }
  if (key1->type == optimizer::SEL_ARG::MAYBE_KEY)
  {
    key1->right= key1->left= &optimizer::null_element;
    key1->next= key1->prev= 0;
  }
  for (next= key1->first(); next ; next=next->next)
  {
    if (next->next_key_part)
    {
      optimizer::SEL_ARG *tmp= key_and(param, next->next_key_part, key2, clone_flag);
      if (tmp && tmp->type == optimizer::SEL_ARG::IMPOSSIBLE)
      {
        key1=key1->tree_delete(next);
        continue;
      }
      next->next_key_part=tmp;
      if (use_count)
        next->increment_use_count(use_count);
      if (param->alloced_sel_args > optimizer::SEL_ARG::MAX_SEL_ARGS)
        break;
    }
    else
      next->next_key_part=key2;
  }
  if (! key1)
    return &optimizer::null_element;			// Impossible ranges
  key1->use_count++;
  return key1;
}


/*
  Produce a SEL_ARG graph that represents "key1 AND key2"

  SYNOPSIS
    key_and()
      param   Range analysis context (needed to track if we have allocated
              too many SEL_ARGs)
      key1    First argument, root of its RB-tree
      key2    Second argument, root of its RB-tree

  RETURN
    RB-tree root of the resulting SEL_ARG graph.
    NULL if the result of AND operation is an empty interval {0}.
*/

static optimizer::SEL_ARG *
key_and(optimizer::RangeParameter *param,
        optimizer::SEL_ARG *key1,
        optimizer::SEL_ARG *key2,
        uint32_t clone_flag)
{
  if (! key1)
    return key2;
  if (! key2)
    return key1;
  if (key1->part != key2->part)
  {
    if (key1->part > key2->part)
    {
      std::swap(key1, key2);
      clone_flag=swap_clone_flag(clone_flag);
    }
    // key1->part < key2->part
    key1->use_count--;
    if (key1->use_count > 0)
      if (! (key1= key1->clone_tree(param)))
        return 0;				// OOM
    return and_all_keys(param, key1, key2, clone_flag);
  }

  if (((clone_flag & CLONE_KEY2_MAYBE) &&
       ! (clone_flag & CLONE_KEY1_MAYBE) &&
       key2->type != optimizer::SEL_ARG::MAYBE_KEY) ||
      key1->type == optimizer::SEL_ARG::MAYBE_KEY)
  {						// Put simple key in key2
    std::swap(key1, key2);
    clone_flag= swap_clone_flag(clone_flag);
  }

  /* If one of the key is MAYBE_KEY then the found region may be smaller */
  if (key2->type == optimizer::SEL_ARG::MAYBE_KEY)
  {
    if (key1->use_count > 1)
    {
      key1->use_count--;
      if (! (key1=key1->clone_tree(param)))
        return 0;				// OOM
      key1->use_count++;
    }
    if (key1->type == optimizer::SEL_ARG::MAYBE_KEY)
    {						// Both are maybe key
      key1->next_key_part= key_and(param,
                                   key1->next_key_part,
                                   key2->next_key_part,
                                   clone_flag);
      if (key1->next_key_part &&
          key1->next_key_part->type == optimizer::SEL_ARG::IMPOSSIBLE)
        return key1;
    }
    else
    {
      key1->maybe_smaller();
      if (key2->next_key_part)
      {
        key1->use_count--;			// Incremented in and_all_keys
        return and_all_keys(param, key1, key2, clone_flag);
      }
      key2->use_count--;			// Key2 doesn't have a tree
    }
    return key1;
  }

  key1->use_count--;
  key2->use_count--;
  optimizer::SEL_ARG *e1= key1->first();
  optimizer::SEL_ARG *e2= key2->first();
  optimizer::SEL_ARG *new_tree= NULL;

  while (e1 && e2)
  {
    int cmp= e1->cmp_min_to_min(e2);
    if (cmp < 0)
    {
      if (get_range(&e1, &e2, key1))
        continue;
    }
    else if (get_range(&e2, &e1, key2))
      continue;
    optimizer::SEL_ARG *next= key_and(param,
                                      e1->next_key_part,
                                      e2->next_key_part,
                                      clone_flag);
    e1->increment_use_count(1);
    e2->increment_use_count(1);
    if (! next || next->type != optimizer::SEL_ARG::IMPOSSIBLE)
    {
      optimizer::SEL_ARG *new_arg= e1->clone_and(e2);
      new_arg->next_key_part=next;
      if (! new_tree)
      {
        new_tree=new_arg;
      }
      else
        new_tree=new_tree->insert(new_arg);
    }
    if (e1->cmp_max_to_max(e2) < 0)
      e1=e1->next;				// e1 can't overlapp next e2
    else
      e2=e2->next;
  }
  key1->free_tree();
  key2->free_tree();
  if (! new_tree)
    return &optimizer::null_element;			// Impossible range
  return new_tree;
}


static bool
get_range(optimizer::SEL_ARG **e1, optimizer::SEL_ARG **e2, optimizer::SEL_ARG *root1)
{
  (*e1)= root1->find_range(*e2);			// first e1->min < e2->min
  if ((*e1)->cmp_max_to_min(*e2) < 0)
  {
    if (! ((*e1)=(*e1)->next))
      return 1;
    if ((*e1)->cmp_min_to_max(*e2) > 0)
    {
      (*e2)=(*e2)->next;
      return 1;
    }
  }
  return 0;
}


/****************************************************************************
  MRR Range Sequence Interface implementation that walks a SEL_ARG* tree.
 ****************************************************************************/

/* MRR range sequence, SEL_ARG* implementation: stack entry */
typedef struct st_range_seq_entry
{
  /*
    Pointers in min and max keys. They point to right-after-end of key
    images. The 0-th entry has these pointing to key tuple start.
  */
  unsigned char *min_key, *max_key;

  /*
    Flags, for {keypart0, keypart1, ... this_keypart} subtuple.
    min_key_flag may have NULL_RANGE set.
  */
  uint32_t min_key_flag, max_key_flag;

  /* Number of key parts */
  uint32_t min_key_parts, max_key_parts;
  optimizer::SEL_ARG *key_tree;
} RANGE_SEQ_ENTRY;


/*
  MRR range sequence, SEL_ARG* implementation: SEL_ARG graph traversal context
*/
typedef struct st_sel_arg_range_seq
{
  uint32_t keyno;      /* index of used tree in optimizer::SEL_TREE structure */
  uint32_t real_keyno; /* Number of the index in tables */
  optimizer::Parameter *param;
  optimizer::SEL_ARG *start; /* Root node of the traversed SEL_ARG* graph */

  RANGE_SEQ_ENTRY stack[MAX_REF_PARTS];
  int i; /* Index of last used element in the above array */

  bool at_start; /* true <=> The traversal has just started */
} SEL_ARG_RANGE_SEQ;


/*
  Range sequence interface, SEL_ARG* implementation: Initialize the traversal

  SYNOPSIS
    init()
      init_params  SEL_ARG tree traversal context
      n_ranges     [ignored] The number of ranges obtained
      flags        [ignored] HA_MRR_SINGLE_POINT, HA_MRR_FIXED_KEY

  RETURN
    Value of init_param
*/

static range_seq_t sel_arg_range_seq_init(void *init_param, uint32_t, uint32_t)
{
  SEL_ARG_RANGE_SEQ *seq= (SEL_ARG_RANGE_SEQ*)init_param;
  seq->at_start= true;
  seq->stack[0].key_tree= NULL;
  seq->stack[0].min_key= seq->param->min_key;
  seq->stack[0].min_key_flag= 0;
  seq->stack[0].min_key_parts= 0;

  seq->stack[0].max_key= seq->param->max_key;
  seq->stack[0].max_key_flag= 0;
  seq->stack[0].max_key_parts= 0;
  seq->i= 0;
  return init_param;
}


static void step_down_to(SEL_ARG_RANGE_SEQ *arg, optimizer::SEL_ARG *key_tree)
{
  RANGE_SEQ_ENTRY *cur= &arg->stack[arg->i+1];
  RANGE_SEQ_ENTRY *prev= &arg->stack[arg->i];

  cur->key_tree= key_tree;
  cur->min_key= prev->min_key;
  cur->max_key= prev->max_key;
  cur->min_key_parts= prev->min_key_parts;
  cur->max_key_parts= prev->max_key_parts;

  uint16_t stor_length= arg->param->key[arg->keyno][key_tree->part].store_length;
  cur->min_key_parts += key_tree->store_min(stor_length, &cur->min_key,
                                            prev->min_key_flag);
  cur->max_key_parts += key_tree->store_max(stor_length, &cur->max_key,
                                            prev->max_key_flag);

  cur->min_key_flag= prev->min_key_flag | key_tree->min_flag;
  cur->max_key_flag= prev->max_key_flag | key_tree->max_flag;

  if (key_tree->is_null_interval())
    cur->min_key_flag |= NULL_RANGE;
  (arg->i)++;
}


/*
  Range sequence interface, SEL_ARG* implementation: get the next interval

  SYNOPSIS
    sel_arg_range_seq_next()
      rseq        Value returned from sel_arg_range_seq_init
      range  OUT  Store information about the range here

  DESCRIPTION
    This is "get_next" function for Range sequence interface implementation
    for SEL_ARG* tree.

  IMPLEMENTATION
    The traversal also updates those param members:
      - is_ror_scan
      - range_count
      - max_key_part

  RETURN
    0  Ok
    1  No more ranges in the sequence
*/

//psergey-merge-todo: support check_quick_keys:max_keypart
static uint32_t sel_arg_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range)
{
  optimizer::SEL_ARG *key_tree;
  SEL_ARG_RANGE_SEQ *seq= (SEL_ARG_RANGE_SEQ*)rseq;
  if (seq->at_start)
  {
    key_tree= seq->start;
    seq->at_start= false;
    goto walk_up_n_right;
  }

  key_tree= seq->stack[seq->i].key_tree;
  /* Ok, we're at some "full tuple" position in the tree */

  /* Step down if we can */
  if (key_tree->next && key_tree->next != &optimizer::null_element)
  {
    //step down; (update the tuple, we'll step right and stay there)
    seq->i--;
    step_down_to(seq, key_tree->next);
    key_tree= key_tree->next;
    seq->param->is_ror_scan= false;
    goto walk_right_n_up;
  }

  /* Ok, can't step down, walk left until we can step down */
  while (1)
  {
    if (seq->i == 1) // can't step left
      return 1;
    /* Step left */
    seq->i--;
    key_tree= seq->stack[seq->i].key_tree;

    /* Step down if we can */
    if (key_tree->next && key_tree->next != &optimizer::null_element)
    {
      // Step down; update the tuple
      seq->i--;
      step_down_to(seq, key_tree->next);
      key_tree= key_tree->next;
      break;
    }
  }

  /*
    Ok, we've stepped down from the path to previous tuple.
    Walk right-up while we can
  */
walk_right_n_up:
  while (key_tree->next_key_part && key_tree->next_key_part != &optimizer::null_element &&
         key_tree->next_key_part->part == key_tree->part + 1 &&
         key_tree->next_key_part->type == optimizer::SEL_ARG::KEY_RANGE)
  {
    {
      RANGE_SEQ_ENTRY *cur= &seq->stack[seq->i];
      uint32_t min_key_length= cur->min_key - seq->param->min_key;
      uint32_t max_key_length= cur->max_key - seq->param->max_key;
      uint32_t len= cur->min_key - cur[-1].min_key;
      if (! (min_key_length == max_key_length &&
          ! memcmp(cur[-1].min_key, cur[-1].max_key, len) &&
          ! key_tree->min_flag && !key_tree->max_flag))
      {
        seq->param->is_ror_scan= false;
        if (! key_tree->min_flag)
          cur->min_key_parts +=
            key_tree->next_key_part->store_min_key(seq->param->key[seq->keyno],
                                                   &cur->min_key,
                                                   &cur->min_key_flag);
        if (! key_tree->max_flag)
          cur->max_key_parts +=
            key_tree->next_key_part->store_max_key(seq->param->key[seq->keyno],
                                                   &cur->max_key,
                                                   &cur->max_key_flag);
        break;
      }
    }

    /*
      Ok, current atomic interval is in form "t.field=const" and there is
      next_key_part interval. Step right, and walk up from there.
    */
    key_tree= key_tree->next_key_part;

walk_up_n_right:
    while (key_tree->prev && key_tree->prev != &optimizer::null_element)
    {
      /* Step up */
      key_tree= key_tree->prev;
    }
    step_down_to(seq, key_tree);
  }

  /* Ok got a tuple */
  RANGE_SEQ_ENTRY *cur= &seq->stack[seq->i];

  range->ptr= (char*)(size_t)(key_tree->part);
  {
    range->range_flag= cur->min_key_flag | cur->max_key_flag;

    range->start_key.key=    seq->param->min_key;
    range->start_key.length= cur->min_key - seq->param->min_key;
    range->start_key.keypart_map= make_prev_keypart_map(cur->min_key_parts);
    range->start_key.flag= (cur->min_key_flag & NEAR_MIN ? HA_READ_AFTER_KEY :
                                                           HA_READ_KEY_EXACT);

    range->end_key.key=    seq->param->max_key;
    range->end_key.length= cur->max_key - seq->param->max_key;
    range->end_key.flag= (cur->max_key_flag & NEAR_MAX ? HA_READ_BEFORE_KEY :
                                                         HA_READ_AFTER_KEY);
    range->end_key.keypart_map= make_prev_keypart_map(cur->max_key_parts);

    if (!(cur->min_key_flag & ~NULL_RANGE) && !cur->max_key_flag &&
        (uint32_t)key_tree->part+1 == seq->param->table->key_info[seq->real_keyno].key_parts &&
        (seq->param->table->key_info[seq->real_keyno].flags & (HA_NOSAME)) ==
        HA_NOSAME &&
        range->start_key.length == range->end_key.length &&
        !memcmp(seq->param->min_key,seq->param->max_key,range->start_key.length))
      range->range_flag= UNIQUE_RANGE | (cur->min_key_flag & NULL_RANGE);

    if (seq->param->is_ror_scan)
    {
      /*
        If we get here, the condition on the key was converted to form
        "(keyXpart1 = c1) AND ... AND (keyXpart{key_tree->part - 1} = cN) AND
          somecond(keyXpart{key_tree->part})"
        Check if
          somecond is "keyXpart{key_tree->part} = const" and
          uncovered "tail" of KeyX parts is either empty or is identical to
          first members of clustered primary key.
      */
      if (!(!(cur->min_key_flag & ~NULL_RANGE) && !cur->max_key_flag &&
            (range->start_key.length == range->end_key.length) &&
            !memcmp(range->start_key.key, range->end_key.key, range->start_key.length) &&
            is_key_scan_ror(seq->param, seq->real_keyno, key_tree->part + 1)))
        seq->param->is_ror_scan= false;
    }
  }
  seq->param->range_count++;
  seq->param->max_key_part= max(seq->param->max_key_part,(uint32_t)key_tree->part);
  return 0;
}


/*
  Calculate cost and E(#rows) for a given index and intervals tree

  SYNOPSIS
    check_quick_select()
      param             Parameter from test_quick_select
      idx               Number of index to use in Parameter::key optimizer::SEL_TREE::key
      index_only        true  - assume only index tuples will be accessed
                        false - assume full table rows will be read
      tree              Transformed selection condition, tree->key[idx] holds
                        the intervals for the given index.
      update_tbl_stats  true <=> update table->quick_* with information
                        about range scan we've evaluated.
      mrr_flags   INOUT MRR access flags
      cost        OUT   Scan cost

  NOTES
    param->is_ror_scan is set to reflect if the key scan is a ROR (see
    is_key_scan_ror function for more info)
    param->table->quick_*, param->range_count (and maybe others) are
    updated with data of given key scan, see quick_range_seq_next for details.

  RETURN
    Estimate # of records to be retrieved.
    HA_POS_ERROR if estimate calculation failed due to table Cursor problems.
*/

static
ha_rows check_quick_select(Session *session,
                           optimizer::Parameter *param,
                           uint32_t idx,
                           bool index_only,
                           optimizer::SEL_ARG *tree,
                           bool update_tbl_stats,
                           uint32_t *mrr_flags,
                           uint32_t *bufsize,
                           optimizer::CostVector *cost)
{
  SEL_ARG_RANGE_SEQ seq;
  RANGE_SEQ_IF seq_if = {sel_arg_range_seq_init, sel_arg_range_seq_next};
  Cursor *cursor= param->table->cursor;
  ha_rows rows;
  uint32_t keynr= param->real_keynr[idx];

  /* Handle cases when we don't have a valid non-empty list of range */
  if (! tree)
    return(HA_POS_ERROR);
  if (tree->type == optimizer::SEL_ARG::IMPOSSIBLE)
    return(0L);
  if (tree->type != optimizer::SEL_ARG::KEY_RANGE || tree->part != 0)
    return(HA_POS_ERROR);

  seq.keyno= idx;
  seq.real_keyno= keynr;
  seq.param= param;
  seq.start= tree;

  param->range_count=0;
  param->max_key_part=0;

  param->is_ror_scan= true;
  if (param->table->index_flags(keynr) & HA_KEY_SCAN_NOT_ROR)
    param->is_ror_scan= false;

  *mrr_flags= param->force_default_mrr? HA_MRR_USE_DEFAULT_IMPL: 0;
  *mrr_flags|= HA_MRR_NO_ASSOCIATION;

  bool pk_is_clustered= cursor->primary_key_is_clustered();
  if (index_only &&
      (param->table->index_flags(keynr) & HA_KEYREAD_ONLY) &&
      !(pk_is_clustered && keynr == param->table->getShare()->getPrimaryKey()))
     *mrr_flags |= HA_MRR_INDEX_ONLY;

  if (session->lex().sql_command != SQLCOM_SELECT)
    *mrr_flags |= HA_MRR_USE_DEFAULT_IMPL;

  *bufsize= param->session->variables.read_rnd_buff_size;
  rows= cursor->multi_range_read_info_const(keynr, &seq_if, (void*)&seq, 0,
                                          bufsize, mrr_flags, cost);
  if (rows != HA_POS_ERROR)
  {
    param->table->quick_rows[keynr]=rows;
    if (update_tbl_stats)
    {
      param->table->quick_keys.set(keynr);
      param->table->quick_key_parts[keynr]=param->max_key_part+1;
      param->table->quick_n_ranges[keynr]= param->range_count;
      param->table->quick_condition_rows=
        min(param->table->quick_condition_rows, rows);
    }
  }
  /* Figure out if the key scan is ROR (returns rows in ROWID order) or not */
  enum ha_key_alg key_alg= param->table->key_info[seq.real_keyno].algorithm;
  if ((key_alg != HA_KEY_ALG_BTREE) && (key_alg!= HA_KEY_ALG_UNDEF))
  {
    /*
      All scans are non-ROR scans for those index types.
      TODO: Don't have this logic here, make table engines return
      appropriate flags instead.
    */
    param->is_ror_scan= false;
  }
  else
  {
    /* Clustered PK scan is always a ROR scan (TODO: same as above) */
    if (param->table->getShare()->getPrimaryKey() == keynr && pk_is_clustered)
      param->is_ror_scan= true;
  }

  return(rows); //psergey-merge:todo: maintain first_null_comp.
}


/*
  Check if key scan on given index with equality conditions on first n key
  parts is a ROR scan.

  SYNOPSIS
    is_key_scan_ror()
      param  Parameter from test_quick_select
      keynr  Number of key in the table. The key must not be a clustered
             primary key.
      nparts Number of first key parts for which equality conditions
             are present.

  NOTES
    ROR (Rowid Ordered Retrieval) key scan is a key scan that produces
    ordered sequence of rowids (ha_xxx::cmp_ref is the comparison function)

    This function is needed to handle a practically-important special case:
    an index scan is a ROR scan if it is done using a condition in form

        "key1_1=c_1 AND ... AND key1_n=c_n"

    where the index is defined on (key1_1, ..., key1_N [,a_1, ..., a_n])

    and the table has a clustered Primary Key defined as
      PRIMARY KEY(a_1, ..., a_n, b1, ..., b_k)

    i.e. the first key parts of it are identical to uncovered parts ot the
    key being scanned. This function assumes that the index flags do not
    include HA_KEY_SCAN_NOT_ROR flag (that is checked elsewhere).

    Check (1) is made in quick_range_seq_next()

  RETURN
    true   The scan is ROR-scan
    false  Otherwise
*/

static bool is_key_scan_ror(optimizer::Parameter *param, uint32_t keynr, uint8_t nparts)
{
  KeyInfo *table_key= param->table->key_info + keynr;
  KeyPartInfo *key_part= table_key->key_part + nparts;
  KeyPartInfo *key_part_end= (table_key->key_part +
                                table_key->key_parts);
  uint32_t pk_number;

  for (KeyPartInfo *kp= table_key->key_part; kp < key_part; kp++)
  {
    uint16_t fieldnr= param->table->key_info[keynr].
                    key_part[kp - table_key->key_part].fieldnr - 1;
    if (param->table->getField(fieldnr)->key_length() != kp->length)
      return false;
  }

  if (key_part == key_part_end)
    return true;

  key_part= table_key->key_part + nparts;
  pk_number= param->table->getShare()->getPrimaryKey();
  if (!param->table->cursor->primary_key_is_clustered() || pk_number == MAX_KEY)
    return false;

  KeyPartInfo *pk_part= param->table->key_info[pk_number].key_part;
  KeyPartInfo *pk_part_end= pk_part +
                              param->table->key_info[pk_number].key_parts;
  for (;(key_part!=key_part_end) && (pk_part != pk_part_end);
       ++key_part, ++pk_part)
  {
    if ((key_part->field != pk_part->field) ||
        (key_part->length != pk_part->length))
      return false;
  }
  return (key_part == key_part_end);
}


optimizer::QuickRangeSelect *
optimizer::get_quick_select(Parameter *param,
                            uint32_t idx,
                            optimizer::SEL_ARG *key_tree,
                            uint32_t mrr_flags,
                            uint32_t mrr_buf_size,
                            memory::Root *parent_alloc)
{
  optimizer::QuickRangeSelect *quick= new optimizer::QuickRangeSelect(param->session,
                                                                      param->table,
                                                                      param->real_keynr[idx],
                                                                      test(parent_alloc),
                                                                      NULL);

  if (quick)
  {
	  if (get_quick_keys(param,
                       quick,
                       param->key[idx],
                       key_tree,
                       param->min_key,
                       0,
		                   param->max_key,
                       0))
    {
      delete quick;
      quick= NULL;
    }
    else
    {
      quick->mrr_flags= mrr_flags;
      quick->mrr_buf_size= mrr_buf_size;
      quick->key_parts= parent_alloc
        ? (KEY_PART*)parent_alloc->memdup(param->key[idx], sizeof(KEY_PART)* param->table->key_info[param->real_keynr[idx]].key_parts)
        : (KEY_PART*)quick->alloc.memdup(param->key[idx], sizeof(KEY_PART)* param->table->key_info[param->real_keynr[idx]].key_parts);
    }
  }
  return quick;
}


/*
** Fix this to get all possible sub_ranges
*/
bool
optimizer::get_quick_keys(optimizer::Parameter *param,
                          optimizer::QuickRangeSelect *quick,
                          KEY_PART *key,
	                        optimizer::SEL_ARG *key_tree,
                          unsigned char *min_key,
                          uint32_t min_key_flag,
	                        unsigned char *max_key,
                          uint32_t max_key_flag)
{
  optimizer::QuickRange *range= NULL;
  uint32_t flag;
  int min_part= key_tree->part - 1; // # of keypart values in min_key buffer
  int max_part= key_tree->part - 1; // # of keypart values in max_key buffer

  if (key_tree->left != &optimizer::null_element)
  {
    if (get_quick_keys(param,
                       quick,
                       key,
                       key_tree->left,
		                   min_key,
                       min_key_flag,
                       max_key,
                       max_key_flag))
    {
      return 1;
    }
  }
  unsigned char *tmp_min_key= min_key,*tmp_max_key= max_key;
  min_part+= key_tree->store_min(key[key_tree->part].store_length,
                                 &tmp_min_key,min_key_flag);
  max_part+= key_tree->store_max(key[key_tree->part].store_length,
                                 &tmp_max_key,max_key_flag);

  if (key_tree->next_key_part &&
      key_tree->next_key_part->part == key_tree->part+1 &&
      key_tree->next_key_part->type == optimizer::SEL_ARG::KEY_RANGE)
  {						  // const key as prefix
    if ((tmp_min_key - min_key) == (tmp_max_key - max_key) &&
        memcmp(min_key, max_key, (uint32_t)(tmp_max_key - max_key))==0 &&
        key_tree->min_flag==0 && key_tree->max_flag==0)
    {
      if (get_quick_keys(param,
                         quick,
                         key,
                         key_tree->next_key_part,
                         tmp_min_key,
                         min_key_flag | key_tree->min_flag,
                         tmp_max_key,
                         max_key_flag | key_tree->max_flag))
      {
        return 1;
      }
      goto end;					// Ugly, but efficient
    }
    {
      uint32_t tmp_min_flag=key_tree->min_flag,tmp_max_flag=key_tree->max_flag;
      if (! tmp_min_flag)
      {
        min_part+= key_tree->next_key_part->store_min_key(key,
                                                          &tmp_min_key,
                                                          &tmp_min_flag);
      }
      if (! tmp_max_flag)
      {
        max_part+= key_tree->next_key_part->store_max_key(key,
                                                          &tmp_max_key,
                                                          &tmp_max_flag);
      }
      flag=tmp_min_flag | tmp_max_flag;
    }
  }
  else
  {
    flag= key_tree->min_flag | key_tree->max_flag;
  }

  /*
    Ensure that some part of min_key and max_key are used.  If not,
    regard this as no lower/upper range
  */
  if (tmp_min_key != param->min_key)
  {
    flag&= ~NO_MIN_RANGE;
  }
  else
  {
    flag|= NO_MIN_RANGE;
  }
  if (tmp_max_key != param->max_key)
  {
    flag&= ~NO_MAX_RANGE;
  }
  else
  {
    flag|= NO_MAX_RANGE;
  }
  if (flag == 0)
  {
    uint32_t length= (uint32_t) (tmp_min_key - param->min_key);
    if (length == (uint32_t) (tmp_max_key - param->max_key) &&
	      ! memcmp(param->min_key,param->max_key,length))
    {
      KeyInfo *table_key= quick->head->key_info+quick->index;
      flag= EQ_RANGE;
      if ((table_key->flags & (HA_NOSAME)) == HA_NOSAME &&
	        key->part == table_key->key_parts-1)
      {
        if (! (table_key->flags & HA_NULL_PART_KEY) ||
            ! null_part_in_key(key,
                               param->min_key,
                               (uint32_t) (tmp_min_key - param->min_key)))
        {
          flag|= UNIQUE_RANGE;
        }
        else
        {
          flag|= NULL_RANGE;
        }
      }
    }
  }

  /* Get range for retrieving rows in QUICK_SELECT::get_next */
  range= new optimizer::QuickRange(param->min_key,
			                                     (uint32_t) (tmp_min_key - param->min_key),
                                           min_part >=0 ? make_keypart_map(min_part) : 0,
			                                     param->max_key,
			                                     (uint32_t) (tmp_max_key - param->max_key),
                                           max_part >=0 ? make_keypart_map(max_part) : 0,
			                                     flag);

  set_if_bigger(quick->max_used_key_length, (uint32_t)range->min_length);
  set_if_bigger(quick->max_used_key_length, (uint32_t)range->max_length);
  set_if_bigger(quick->used_key_parts, (uint32_t) key_tree->part+1);
  quick->ranges.push_back(&range);

 end:
  if (key_tree->right != &optimizer::null_element)
  {
    return get_quick_keys(param,
                          quick,
                          key,
                          key_tree->right,
			                    min_key,
                          min_key_flag,
			                    max_key,
                          max_key_flag);
  }
  return 0;
}

/*
  Return true if any part of the key is NULL

  SYNOPSIS
    null_part_in_key()
      key_part  Array of key parts (index description)
      key       Key values tuple
      length    Length of key values tuple in bytes.

  RETURN
    true   The tuple has at least one "keypartX is NULL"
    false  Otherwise
*/

static bool null_part_in_key(KEY_PART *key_part, const unsigned char *key, uint32_t length)
{
  for (const unsigned char *end=key+length ;
       key < end;
       key+= key_part++->store_length)
  {
    if (key_part->null_bit && *key)
      return 1;
  }
  return 0;
}


bool optimizer::QuickSelectInterface::is_keys_used(const boost::dynamic_bitset<>& fields)
{
  return is_key_used(head, index, fields);
}


/*
  Create quick select from ref/ref_or_null scan.

  SYNOPSIS
    get_quick_select_for_ref()
      session      Thread handle
      table    Table to access
      ref      ref[_or_null] scan parameters
      records  Estimate of number of records (needed only to construct
               quick select)
  NOTES
    This allocates things in a new memory root, as this may be called many
    times during a query.

  RETURN
    Quick select that retrieves the same rows as passed ref scan
    NULL on error.
*/

optimizer::QuickRangeSelect *optimizer::get_quick_select_for_ref(Session *session,
                                                                 Table *table,
                                                                 table_reference_st *ref,
                                                                 ha_rows records)
{
  memory::Root *old_root= NULL;
  memory::Root *alloc= NULL;
  KeyInfo *key_info = &table->key_info[ref->key];
  KEY_PART *key_part;
  optimizer::QuickRange *range= NULL;
  uint32_t part;
  optimizer::CostVector cost;

  old_root= session->mem_root;
  /* The following call may change session->mem_root */
  optimizer::QuickRangeSelect *quick= new optimizer::QuickRangeSelect(session, table, ref->key, 0, 0);
  /* save mem_root set by QuickRangeSelect constructor */
  alloc= session->mem_root;
  /*
    return back default mem_root (session->mem_root) changed by
    QuickRangeSelect constructor
  */
  session->mem_root= old_root;

  if (! quick)
    return 0;			/* no ranges found */
  if (quick->init())
    goto err;
  quick->records= records;

  if (cp_buffer_from_ref(session, ref) && session->is_fatal_error)
    goto err;                                   // out of memory
  range= new (*alloc) optimizer::QuickRange;

  range->min_key= range->max_key= ref->key_buff;
  range->min_length= range->max_length= ref->key_length;
  range->min_keypart_map= range->max_keypart_map=
    make_prev_keypart_map(ref->key_parts);
  range->flag= (ref->key_length == key_info->key_length && (key_info->flags & HA_END_SPACE_KEY) == 0) ? EQ_RANGE : 0;

  quick->key_parts=key_part= new (quick->alloc) KEY_PART[ref->key_parts];

  for (part=0 ; part < ref->key_parts ;part++,key_part++)
  {
    key_part->part=part;
    key_part->field=        key_info->key_part[part].field;
    key_part->length=       key_info->key_part[part].length;
    key_part->store_length= key_info->key_part[part].store_length;
    key_part->null_bit=     key_info->key_part[part].null_bit;
    key_part->flag=         (uint8_t) key_info->key_part[part].key_part_flag;
  }
  quick->ranges.push_back(&range);

  /*
     Add a NULL range if REF_OR_NULL optimization is used.
     For example:
       if we have "WHERE A=2 OR A IS NULL" we created the (A=2) range above
       and have ref->null_ref_key set. Will create a new NULL range here.
  */
  if (ref->null_ref_key)
  {
    optimizer::QuickRange *null_range= NULL;

    *ref->null_ref_key= 1;		// Set null byte then create a range
    null_range= new (alloc)
          optimizer::QuickRange(ref->key_buff, ref->key_length,
                                 make_prev_keypart_map(ref->key_parts),
                                 ref->key_buff, ref->key_length,
                                 make_prev_keypart_map(ref->key_parts), EQ_RANGE);
    *ref->null_ref_key= 0;		// Clear null byte
    quick->ranges.push_back(&null_range);
  }

  /* Call multi_range_read_info() to get the MRR flags and buffer size */
  quick->mrr_flags= HA_MRR_NO_ASSOCIATION |
                    (table->key_read ? HA_MRR_INDEX_ONLY : 0);
  if (session->lex().sql_command != SQLCOM_SELECT)
    quick->mrr_flags |= HA_MRR_USE_DEFAULT_IMPL;

  quick->mrr_buf_size= session->variables.read_rnd_buff_size;
  if (table->cursor->multi_range_read_info(quick->index, 1, (uint32_t)records, &quick->mrr_buf_size, &quick->mrr_flags, &cost))
    goto err;

  return quick;
err:
  delete quick;
  return 0;
}


/*
  Range sequence interface implementation for array<QuickRange>: initialize

  SYNOPSIS
    quick_range_seq_init()
      init_param  Caller-opaque paramenter: QuickRangeSelect* pointer
      n_ranges    Number of ranges in the sequence (ignored)
      flags       MRR flags (currently not used)

  RETURN
    Opaque value to be passed to quick_range_seq_next
*/

range_seq_t optimizer::quick_range_seq_init(void *init_param, uint32_t, uint32_t)
{
  optimizer::QuickRangeSelect *quick= (optimizer::QuickRangeSelect*)init_param;
  quick->qr_traversal_ctx.first=  (optimizer::QuickRange**)quick->ranges.buffer;
  quick->qr_traversal_ctx.cur=    (optimizer::QuickRange**)quick->ranges.buffer;
  quick->qr_traversal_ctx.last=   quick->qr_traversal_ctx.cur +
                                  quick->ranges.size();
  return &quick->qr_traversal_ctx;
}


/*
  Range sequence interface implementation for array<QuickRange>: get next

  SYNOPSIS
    quick_range_seq_next()
      rseq        Value returned from quick_range_seq_init
      range  OUT  Store information about the range here

  RETURN
    0  Ok
    1  No more ranges in the sequence
*/
uint32_t optimizer::quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range)
{
  QuickRangeSequenceContext *ctx= (QuickRangeSequenceContext*) rseq;

  if (ctx->cur == ctx->last)
    return 1; /* no more ranges */

  optimizer::QuickRange *cur= *(ctx->cur);
  key_range *start_key= &range->start_key;
  key_range *end_key= &range->end_key;

  start_key->key= cur->min_key;
  start_key->length= cur->min_length;
  start_key->keypart_map= cur->min_keypart_map;
  start_key->flag= ((cur->flag & NEAR_MIN) ? HA_READ_AFTER_KEY :
                                             (cur->flag & EQ_RANGE) ?
                                             HA_READ_KEY_EXACT : HA_READ_KEY_OR_NEXT);
  end_key->key= cur->max_key;
  end_key->length= cur->max_length;
  end_key->keypart_map= cur->max_keypart_map;
  /*
    We use HA_READ_AFTER_KEY here because if we are reading on a key
    prefix. We want to find all keys with this prefix.
  */
  end_key->flag= (cur->flag & NEAR_MAX ? HA_READ_BEFORE_KEY :
                                         HA_READ_AFTER_KEY);
  range->range_flag= cur->flag;
  ctx->cur++;
  return 0;
}


static inline uint32_t get_field_keypart(KeyInfo *index, Field *field);

static inline optimizer::SEL_ARG * get_index_range_tree(uint32_t index,
                                                        optimizer::SEL_TREE *range_tree,
                                                        optimizer::Parameter *param,
                                                        uint32_t *param_idx);

static bool get_constant_key_infix(KeyInfo *index_info,
                                   optimizer::SEL_ARG *index_range_tree,
                                   KeyPartInfo *first_non_group_part,
                                   KeyPartInfo *min_max_arg_part,
                                   KeyPartInfo *last_part,
                                   Session *session,
                                   unsigned char *key_infix,
                                   uint32_t *key_infix_len,
                                   KeyPartInfo **first_non_infix_part);

static bool check_group_min_max_predicates(COND *cond, Item_field *min_max_arg_item);

static void
cost_group_min_max(Table* table,
                   KeyInfo *index_info,
                   uint32_t used_key_parts,
                   uint32_t group_key_parts,
                   optimizer::SEL_TREE *range_tree,
                   optimizer::SEL_ARG *index_tree,
                   ha_rows quick_prefix_records,
                   bool have_min,
                   bool have_max,
                   double *read_cost,
                   ha_rows *records);


/*
  Test if this access method is applicable to a GROUP query with MIN/MAX
  functions, and if so, construct a new TRP object.

  SYNOPSIS
    get_best_group_min_max()
    param    Parameter from test_quick_select
    sel_tree Range tree generated by get_mm_tree

  DESCRIPTION
    Test whether a query can be computed via a QuickGroupMinMaxSelect.
    Queries computable via a QuickGroupMinMaxSelect must satisfy the
    following conditions:
    A) Table T has at least one compound index I of the form:
       I = <A_1, ...,A_k, [B_1,..., B_m], C, [D_1,...,D_n]>
    B) Query conditions:
    B0. Q is over a single table T.
    B1. The attributes referenced by Q are a subset of the attributes of I.
    B2. All attributes QA in Q can be divided into 3 overlapping groups:
        - SA = {S_1, ..., S_l, [C]} - from the SELECT clause, where C is
          referenced by any number of MIN and/or MAX functions if present.
        - WA = {W_1, ..., W_p} - from the WHERE clause
        - GA = <G_1, ..., G_k> - from the GROUP BY clause (if any)
             = SA              - if Q is a DISTINCT query (based on the
                                 equivalence of DISTINCT and GROUP queries.
        - NGA = QA - (GA union C) = {NG_1, ..., NG_m} - the ones not in
          GROUP BY and not referenced by MIN/MAX functions.
        with the following properties specified below.
    B3. If Q has a GROUP BY WITH ROLLUP clause the access method is not
        applicable.

    SA1. There is at most one attribute in SA referenced by any number of
         MIN and/or MAX functions which, which if present, is denoted as C.
    SA2. The position of the C attribute in the index is after the last A_k.
    SA3. The attribute C can be referenced in the WHERE clause only in
         predicates of the forms:
         - (C {< | <= | > | >= | =} const)
         - (const {< | <= | > | >= | =} C)
         - (C between const_i and const_j)
         - C IS NULL
         - C IS NOT NULL
         - C != const
    SA4. If Q has a GROUP BY clause, there are no other aggregate functions
         except MIN and MAX. For queries with DISTINCT, aggregate functions
         are allowed.
    SA5. The select list in DISTINCT queries should not contain expressions.
    GA1. If Q has a GROUP BY clause, then GA is a prefix of I. That is, if
         G_i = A_j => i = j.
    GA2. If Q has a DISTINCT clause, then there is a permutation of SA that
         forms a prefix of I. This permutation is used as the GROUP clause
         when the DISTINCT query is converted to a GROUP query.
    GA3. The attributes in GA may participate in arbitrary predicates, divided
         into two groups:
         - RNG(G_1,...,G_q ; where q <= k) is a range condition over the
           attributes of a prefix of GA
         - PA(G_i1,...G_iq) is an arbitrary predicate over an arbitrary subset
           of GA. Since P is applied to only GROUP attributes it filters some
           groups, and thus can be applied after the grouping.
    GA4. There are no expressions among G_i, just direct column references.
    NGA1.If in the index I there is a gap between the last GROUP attribute G_k,
         and the MIN/MAX attribute C, then NGA must consist of exactly the
         index attributes that constitute the gap. As a result there is a
         permutation of NGA that coincides with the gap in the index
         <B_1, ..., B_m>.
    NGA2.If BA <> {}, then the WHERE clause must contain a conjunction EQ of
         equality conditions for all NG_i of the form (NG_i = const) or
         (const = NG_i), such that each NG_i is referenced in exactly one
         conjunct. Informally, the predicates provide constants to fill the
         gap in the index.
    WA1. There are no other attributes in the WHERE clause except the ones
         referenced in predicates RNG, PA, PC, EQ defined above. Therefore
         WA is subset of (GA union NGA union C) for GA,NGA,C that pass the
         above tests. By transitivity then it also follows that each WA_i
         participates in the index I (if this was already tested for GA, NGA
         and C).

    C) Overall query form:
       SELECT EXPR([A_1,...,A_k], [B_1,...,B_m], [MIN(C)], [MAX(C)])
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND EQ(B_1,...,B_m)]
         [AND PC(C)]
         [AND PA(A_i1,...,A_iq)]
       GROUP BY A_1,...,A_k
       [HAVING PH(A_1, ..., B_1,..., C)]
    where EXPR(...) is an arbitrary expression over some or all SELECT fields,
    or:
       SELECT DISTINCT A_i1,...,A_ik
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND PA(A_i1,...,A_iq)];

  NOTES
    If the current query satisfies the conditions above, and if
    (mem_root! = NULL), then the function constructs and returns a new TRP
    object, that is later used to construct a new QuickGroupMinMaxSelect.
    If (mem_root == NULL), then the function only tests whether the current
    query satisfies the conditions above, and, if so, sets
    is_applicable = true.

    Queries with DISTINCT for which index access can be used are transformed
    into equivalent group-by queries of the form:

    SELECT A_1,...,A_k FROM T
     WHERE [RNG(A_1,...,A_p ; where p <= k)]
      [AND PA(A_i1,...,A_iq)]
    GROUP BY A_1,...,A_k;

    The group-by list is a permutation of the select attributes, according
    to their order in the index.

  TODO
  - What happens if the query groups by the MIN/MAX field, and there is no
    other field as in: "select min(a) from t1 group by a" ?
  - We assume that the general correctness of the GROUP-BY query was checked
    before this point. Is this correct, or do we have to check it completely?
  - Lift the limitation in condition (B3), that is, make this access method
    applicable to ROLLUP queries.

  RETURN
    If mem_root != NULL
    - valid GroupMinMaxReadPlan object if this QUICK class can be used for
      the query
    -  NULL o/w.
    If mem_root == NULL
    - NULL
*/
static optimizer::GroupMinMaxReadPlan *
get_best_group_min_max(optimizer::Parameter *param, optimizer::SEL_TREE *tree)
{
  Session *session= param->session;
  Join *join= session->lex().current_select->join;
  Table *table= param->table;
  bool have_min= false;              /* true if there is a MIN function. */
  bool have_max= false;              /* true if there is a MAX function. */
  Item_field *min_max_arg_item= NULL; // The argument of all MIN/MAX functions
  KeyPartInfo *min_max_arg_part= NULL; /* The corresponding keypart. */
  uint32_t group_prefix_len= 0; /* Length (in bytes) of the key prefix. */
  KeyInfo *index_info= NULL;    /* The index chosen for data access. */
  uint32_t index= 0;            /* The id of the chosen index. */
  uint32_t group_key_parts= 0;  // Number of index key parts in the group prefix.
  uint32_t used_key_parts= 0;   /* Number of index key parts used for access. */
  unsigned char key_infix[MAX_KEY_LENGTH]; /* Constants from equality predicates.*/
  uint32_t key_infix_len= 0;          /* Length of key_infix. */
  optimizer::GroupMinMaxReadPlan *read_plan= NULL; /* The eventually constructed TRP. */
  uint32_t key_part_nr;
  Order *tmp_group= NULL;
  Item *item= NULL;
  Item_field *item_field= NULL;

  /* Perform few 'cheap' tests whether this access method is applicable. */
  if (! join)
    return NULL;        /* This is not a select statement. */

  if ((join->tables != 1) ||  /* The query must reference one table. */
      ((! join->group_list) && /* Neither GROUP BY nor a DISTINCT query. */
       (! join->select_distinct)) ||
      (join->select_lex->olap == ROLLUP_TYPE)) /* Check (B3) for ROLLUP */
    return NULL;
  if (table->getShare()->sizeKeys() == 0)        /* There are no indexes to use. */
    return NULL;

  /* Analyze the query in more detail. */
  List<Item>::iterator select_items_it(join->fields_list.begin());

  /* Check (SA1,SA4) and store the only MIN/MAX argument - the C attribute.*/
  if (join->make_sum_func_list(join->all_fields, join->fields_list, 1))
    return NULL;

  if (join->sum_funcs[0])
  {
    Item_sum *min_max_item= NULL;
    Item_sum **func_ptr= join->sum_funcs;
    while ((min_max_item= *(func_ptr++)))
    {
      if (min_max_item->sum_func() == Item_sum::MIN_FUNC)
        have_min= true;
      else if (min_max_item->sum_func() == Item_sum::MAX_FUNC)
        have_max= true;
      else
        return NULL;

      /* The argument of MIN/MAX. */
      Item *expr= min_max_item->args[0]->real_item();
      if (expr->type() == Item::FIELD_ITEM) /* Is it an attribute? */
      {
        if (! min_max_arg_item)
          min_max_arg_item= (Item_field*) expr;
        else if (! min_max_arg_item->eq(expr, 1))
          return NULL;
      }
      else
        return NULL;
    }
  }

  /* Check (SA5). */
  if (join->select_distinct)
  {
    while ((item= select_items_it++))
    {
      if (item->type() != Item::FIELD_ITEM)
        return NULL;
    }
  }

  /* Check (GA4) - that there are no expressions among the group attributes. */
  for (tmp_group= join->group_list; tmp_group; tmp_group= tmp_group->next)
  {
    if ((*tmp_group->item)->type() != Item::FIELD_ITEM)
      return NULL;
  }

  /*
    Check that table has at least one compound index such that the conditions
    (GA1,GA2) are all true. If there is more than one such index, select the
    first one. Here we set the variables: group_prefix_len and index_info.
  */
  KeyInfo *cur_index_info= table->key_info;
  KeyInfo *cur_index_info_end= cur_index_info + table->getShare()->sizeKeys();
  KeyPartInfo *cur_part= NULL;
  KeyPartInfo *end_part= NULL; /* Last part for loops. */
  /* Last index part. */
  KeyPartInfo *last_part= NULL;
  KeyPartInfo *first_non_group_part= NULL;
  KeyPartInfo *first_non_infix_part= NULL;
  uint32_t key_infix_parts= 0;
  uint32_t cur_group_key_parts= 0;
  uint32_t cur_group_prefix_len= 0;
  /* Cost-related variables for the best index so far. */
  double best_read_cost= DBL_MAX;
  ha_rows best_records= 0;
  optimizer::SEL_ARG *best_index_tree= NULL;
  ha_rows best_quick_prefix_records= 0;
  uint32_t best_param_idx= 0;
  double cur_read_cost= DBL_MAX;
  ha_rows cur_records;
  optimizer::SEL_ARG *cur_index_tree= NULL;
  ha_rows cur_quick_prefix_records= 0;
  uint32_t cur_param_idx= MAX_KEY;
  key_map used_key_parts_map;
  uint32_t cur_key_infix_len= 0;
  unsigned char cur_key_infix[MAX_KEY_LENGTH];
  uint32_t cur_used_key_parts= 0;
  uint32_t pk= param->table->getShare()->getPrimaryKey();

  for (uint32_t cur_index= 0;
       cur_index_info != cur_index_info_end;
       cur_index_info++, cur_index++)
  {
    /* Check (B1) - if current index is covering. */
    if (! table->covering_keys.test(cur_index))
      goto next_index;

    /*
      If the current storage manager is such that it appends the primary key to
      each index, then the above condition is insufficient to check if the
      index is covering. In such cases it may happen that some fields are
      covered by the PK index, but not by the current index. Since we can't
      use the concatenation of both indexes for index lookup, such an index
      does not qualify as covering in our case. If this is the case, below
      we check that all query fields are indeed covered by 'cur_index'.
    */
    if (pk < MAX_KEY && cur_index != pk &&
        (table->cursor->getEngine()->check_flag(HTON_BIT_PRIMARY_KEY_IN_READ_INDEX)))
    {
      /* For each table field */
      for (uint32_t i= 0; i < table->getShare()->sizeFields(); i++)
      {
        Field *cur_field= table->getField(i);
        /*
          If the field is used in the current query ensure that it's
          part of 'cur_index'
        */
        if ((cur_field->isReadSet()) &&
            ! cur_field->part_of_key_not_clustered.test(cur_index))
          goto next_index;                  // Field was not part of key
      }
    }

    /*
      Check (GA1) for GROUP BY queries.
    */
    if (join->group_list)
    {
      cur_part= cur_index_info->key_part;
      end_part= cur_part + cur_index_info->key_parts;
      /* Iterate in parallel over the GROUP list and the index parts. */
      for (tmp_group= join->group_list;
           tmp_group && (cur_part != end_part);
           tmp_group= tmp_group->next, cur_part++)
      {
        /*
          TODO:
          tmp_group::item is an array of Item, is it OK to consider only the
          first Item? If so, then why? What is the array for?
        */
        /* Above we already checked that all group items are fields. */
        assert((*tmp_group->item)->type() == Item::FIELD_ITEM);
        Item_field *group_field= (Item_field *) (*tmp_group->item);
        if (group_field->field->eq(cur_part->field))
        {
          cur_group_prefix_len+= cur_part->store_length;
          ++cur_group_key_parts;
        }
        else
          goto next_index;
      }
    }
    /*
      Check (GA2) if this is a DISTINCT query.
      If GA2, then Store a new order_st object in group_fields_array at the
      position of the key part of item_field->field. Thus we get the order_st
      objects for each field ordered as the corresponding key parts.
      Later group_fields_array of order_st objects is used to convert the query
      to a GROUP query.
    */
    else if (join->select_distinct)
    {
      select_items_it= join->fields_list.begin();
      used_key_parts_map.reset();
      uint32_t max_key_part= 0;
      while ((item= select_items_it++))
      {
        item_field= (Item_field*) item; /* (SA5) already checked above. */
        /* Find the order of the key part in the index. */
        key_part_nr= get_field_keypart(cur_index_info, item_field->field);
        /*
          Check if this attribute was already present in the select list.
          If it was present, then its corresponding key part was alredy used.
        */
        if (used_key_parts_map.test(key_part_nr))
          continue;
        if (key_part_nr < 1 || key_part_nr > join->fields_list.size())
          goto next_index;
        cur_part= cur_index_info->key_part + key_part_nr - 1;
        cur_group_prefix_len+= cur_part->store_length;
        used_key_parts_map.set(key_part_nr);
        ++cur_group_key_parts;
        max_key_part= max(max_key_part,key_part_nr);
      }
      /*
        Check that used key parts forms a prefix of the index.
        To check this we compare bits in all_parts and cur_parts.
        all_parts have all bits set from 0 to (max_key_part-1).
        cur_parts have bits set for only used keyparts.
      */
      key_map all_parts;
      key_map cur_parts;
      for (uint32_t pos= 0; pos < max_key_part; pos++)
        all_parts.set(pos);
      cur_parts= used_key_parts_map >> 1;
      if (all_parts != cur_parts)
        goto next_index;
    }
    else
      assert(false);

    /* Check (SA2). */
    if (min_max_arg_item)
    {
      key_part_nr= get_field_keypart(cur_index_info, min_max_arg_item->field);
      if (key_part_nr <= cur_group_key_parts)
        goto next_index;
      min_max_arg_part= cur_index_info->key_part + key_part_nr - 1;
    }

    /*
      Check (NGA1, NGA2) and extract a sequence of constants to be used as part
      of all search keys.
    */

    /*
      If there is MIN/MAX, each keypart between the last group part and the
      MIN/MAX part must participate in one equality with constants, and all
      keyparts after the MIN/MAX part must not be referenced in the query.

      If there is no MIN/MAX, the keyparts after the last group part can be
      referenced only in equalities with constants, and the referenced keyparts
      must form a sequence without any gaps that starts immediately after the
      last group keypart.
    */
    last_part= cur_index_info->key_part + cur_index_info->key_parts;
    first_non_group_part= (cur_group_key_parts < cur_index_info->key_parts) ?
                          cur_index_info->key_part + cur_group_key_parts :
                          NULL;
    first_non_infix_part= min_max_arg_part ?
                          (min_max_arg_part < last_part) ?
                             min_max_arg_part :
                             NULL :
                           NULL;
    if (first_non_group_part &&
        (! min_max_arg_part || (min_max_arg_part - first_non_group_part > 0)))
    {
      if (tree)
      {
        uint32_t dummy;
        optimizer::SEL_ARG *index_range_tree= get_index_range_tree(cur_index,
                                                                   tree,
                                                                   param,
                                                                   &dummy);
        if (! get_constant_key_infix(cur_index_info,
                                     index_range_tree,
                                     first_non_group_part,
                                     min_max_arg_part,
                                     last_part,
                                     session,
                                     cur_key_infix,
                                     &cur_key_infix_len,
                                     &first_non_infix_part))
        {
          goto next_index;
        }
      }
      else if (min_max_arg_part &&
               (min_max_arg_part - first_non_group_part > 0))
      {
        /*
          There is a gap but no range tree, thus no predicates at all for the
          non-group keyparts.
        */
        goto next_index;
      }
      else if (first_non_group_part && join->conds)
      {
        /*
          If there is no MIN/MAX function in the query, but some index
          key part is referenced in the WHERE clause, then this index
          cannot be used because the WHERE condition over the keypart's
          field cannot be 'pushed' to the index (because there is no
          range 'tree'), and the WHERE clause must be evaluated before
          GROUP BY/DISTINCT.
        */
        /*
          Store the first and last keyparts that need to be analyzed
          into one array that can be passed as parameter.
        */
        KeyPartInfo *key_part_range[2];
        key_part_range[0]= first_non_group_part;
        key_part_range[1]= last_part;

        /* Check if cur_part is referenced in the WHERE clause. */
        if (join->conds->walk(&Item::find_item_in_field_list_processor,
                              0,
                              (unsigned char*) key_part_range))
          goto next_index;
      }
    }

    /*
      Test (WA1) partially - that no other keypart after the last infix part is
      referenced in the query.
    */
    if (first_non_infix_part)
    {
      cur_part= first_non_infix_part +
                (min_max_arg_part && (min_max_arg_part < last_part));
      for (; cur_part != last_part; cur_part++)
      {
        if (cur_part->field->isReadSet())
          goto next_index;
      }
    }

    /* If we got to this point, cur_index_info passes the test. */
    key_infix_parts= cur_key_infix_len ?
                     (first_non_infix_part - first_non_group_part) : 0;
    cur_used_key_parts= cur_group_key_parts + key_infix_parts;

    /* Compute the cost of using this index. */
    if (tree)
    {
      /* Find the SEL_ARG sub-tree that corresponds to the chosen index. */
      cur_index_tree= get_index_range_tree(cur_index,
                                           tree,
                                           param,
                                           &cur_param_idx);
      /* Check if this range tree can be used for prefix retrieval. */
      optimizer::CostVector dummy_cost;
      uint32_t mrr_flags= HA_MRR_USE_DEFAULT_IMPL;
      uint32_t mrr_bufsize= 0;
      cur_quick_prefix_records= check_quick_select(session,
                                                   param,
                                                   cur_param_idx,
                                                   false /*don't care*/,
                                                   cur_index_tree,
                                                   true,
                                                   &mrr_flags,
                                                   &mrr_bufsize,
                                                   &dummy_cost);
    }
    cost_group_min_max(table,
                       cur_index_info,
                       cur_used_key_parts,
                       cur_group_key_parts,
                       tree,
                       cur_index_tree,
                       cur_quick_prefix_records,
                       have_min,
                       have_max,
                       &cur_read_cost,
                       &cur_records);
    /*
      If cur_read_cost is lower than best_read_cost use cur_index.
      Do not compare doubles directly because they may have different
      representations (64 vs. 80 bits).
    */
    if (cur_read_cost < best_read_cost - (DBL_EPSILON * cur_read_cost))
    {
      assert(tree != 0 || cur_param_idx == MAX_KEY);
      index_info= cur_index_info;
      index= cur_index;
      best_read_cost= cur_read_cost;
      best_records= cur_records;
      best_index_tree= cur_index_tree;
      best_quick_prefix_records= cur_quick_prefix_records;
      best_param_idx= cur_param_idx;
      group_key_parts= cur_group_key_parts;
      group_prefix_len= cur_group_prefix_len;
      key_infix_len= cur_key_infix_len;

      if (key_infix_len)
      {
        memcpy(key_infix, cur_key_infix, sizeof (key_infix));
      }

      used_key_parts= cur_used_key_parts;
    }

  next_index:
    cur_group_key_parts= 0;
    cur_group_prefix_len= 0;
    cur_key_infix_len= 0;
  }
  if (! index_info) /* No usable index found. */
    return NULL;

  /* Check (SA3) for the where clause. */
  if (join->conds && min_max_arg_item &&
      ! check_group_min_max_predicates(join->conds, min_max_arg_item))
    return NULL;

  /* The query passes all tests, so construct a new TRP object. */
  read_plan= new (*param->mem_root) optimizer::GroupMinMaxReadPlan(have_min,
                                                        have_max,
                                                        min_max_arg_part,
                                                        group_prefix_len,
                                                        used_key_parts,
                                                        group_key_parts,
                                                        index_info,
                                                        index,
                                                        key_infix_len,
                                                        (key_infix_len > 0) ? key_infix : NULL,
                                                        tree,
                                                        best_index_tree,
                                                        best_param_idx,
                                                        best_quick_prefix_records);
  if (tree && read_plan->quick_prefix_records == 0)
    return NULL;
  read_plan->read_cost= best_read_cost;
  read_plan->records= best_records;
  return read_plan;
}


/*
  Check that the MIN/MAX attribute participates only in range predicates
  with constants.

  SYNOPSIS
    check_group_min_max_predicates()
    cond              tree (or subtree) describing all or part of the WHERE
                      clause being analyzed
    min_max_arg_item  the field referenced by the MIN/MAX function(s)
    min_max_arg_part  the keypart of the MIN/MAX argument if any

  DESCRIPTION
    The function walks recursively over the cond tree representing a WHERE
    clause, and checks condition (SA3) - if a field is referenced by a MIN/MAX
    aggregate function, it is referenced only by one of the following
    predicates: {=, !=, <, <=, >, >=, between, is null, is not null}.

  RETURN
    true  if cond passes the test
    false o/w
*/
static bool check_group_min_max_predicates(COND *cond, Item_field *min_max_arg_item)
{
  assert(cond && min_max_arg_item);

  cond= cond->real_item();
  Item::Type cond_type= cond->type();
  if (cond_type == Item::COND_ITEM) /* 'AND' or 'OR' */
  {
    List<Item>::iterator li(((Item_cond*) cond)->argument_list()->begin());
    Item *and_or_arg= NULL;
    while ((and_or_arg= li++))
    {
      if (! check_group_min_max_predicates(and_or_arg, min_max_arg_item))
        return false;
    }
    return true;
  }

  /*
    TODO:
    This is a very crude fix to handle sub-selects in the WHERE clause
    (Item_subselect objects). With the test below we rule out from the
    optimization all queries with subselects in the WHERE clause. What has to
    be done, is that here we should analyze whether the subselect references
    the MIN/MAX argument field, and disallow the optimization only if this is
    so.
  */
  if (cond_type == Item::SUBSELECT_ITEM)
    return false;

  /* We presume that at this point there are no other Items than functions. */
  assert(cond_type == Item::FUNC_ITEM);

  /* Test if cond references only group-by or non-group fields. */
  Item_func *pred= (Item_func*) cond;
  Item **arguments= pred->arguments();
  Item *cur_arg= NULL;
  for (uint32_t arg_idx= 0; arg_idx < pred->argument_count (); arg_idx++)
  {
    cur_arg= arguments[arg_idx]->real_item();
    if (cur_arg->type() == Item::FIELD_ITEM)
    {
      if (min_max_arg_item->eq(cur_arg, 1))
      {
       /*
         If pred references the MIN/MAX argument, check whether pred is a range
         condition that compares the MIN/MAX argument with a constant.
       */
        Item_func::Functype pred_type= pred->functype();
        if (pred_type != Item_func::EQUAL_FUNC     &&
            pred_type != Item_func::LT_FUNC        &&
            pred_type != Item_func::LE_FUNC        &&
            pred_type != Item_func::GT_FUNC        &&
            pred_type != Item_func::GE_FUNC        &&
            pred_type != Item_func::BETWEEN        &&
            pred_type != Item_func::ISNULL_FUNC    &&
            pred_type != Item_func::ISNOTNULL_FUNC &&
            pred_type != Item_func::EQ_FUNC        &&
            pred_type != Item_func::NE_FUNC)
          return false;

        /* Check that pred compares min_max_arg_item with a constant. */
        Item *args[3];
        memset(args, 0, 3 * sizeof(Item*));
        bool inv= false;
        /* Test if this is a comparison of a field and a constant. */
        if (! optimizer::simple_pred(pred, args, inv))
          return false;

        /* Check for compatible string comparisons - similar to get_mm_leaf. */
        if (args[0] && args[1] && !args[2] && // this is a binary function
            min_max_arg_item->result_type() == STRING_RESULT &&
            /*
              Don't use an index when comparing strings of different collations.
            */
            ((args[1]->result_type() == STRING_RESULT &&
              ((Field_str*) min_max_arg_item->field)->charset() !=
              pred->compare_collation())
             ||
             /*
               We can't always use indexes when comparing a string index to a
               number.
             */
             (args[1]->result_type() != STRING_RESULT &&
              min_max_arg_item->field->cmp_type() != args[1]->result_type())))
        {
          return false;
        }
      }
    }
    else if (cur_arg->type() == Item::FUNC_ITEM)
    {
      if (! check_group_min_max_predicates(cur_arg, min_max_arg_item))
        return false;
    }
    else if (cur_arg->const_item())
    {
      return true;
    }
    else
      return false;
  }

  return true;
}


/*
  Extract a sequence of constants from a conjunction of equality predicates.

  SYNOPSIS
    get_constant_key_infix()
    index_info             [in]  Descriptor of the chosen index.
    index_range_tree       [in]  Range tree for the chosen index
    first_non_group_part   [in]  First index part after group attribute parts
    min_max_arg_part       [in]  The keypart of the MIN/MAX argument if any
    last_part              [in]  Last keypart of the index
    session                    [in]  Current thread
    key_infix              [out] Infix of constants to be used for index lookup
    key_infix_len          [out] Lenghth of the infix
    first_non_infix_part   [out] The first keypart after the infix (if any)

  DESCRIPTION
    Test conditions (NGA1, NGA2) from get_best_group_min_max(). Namely,
    for each keypart field NGF_i not in GROUP-BY, check that there is a
    constant equality predicate among conds with the form (NGF_i = const_ci) or
    (const_ci = NGF_i).
    Thus all the NGF_i attributes must fill the 'gap' between the last group-by
    attribute and the MIN/MAX attribute in the index (if present). If these
    conditions hold, copy each constant from its corresponding predicate into
    key_infix, in the order its NG_i attribute appears in the index, and update
    key_infix_len with the total length of the key parts in key_infix.

  RETURN
    true  if the index passes the test
    false o/w
*/
static bool
get_constant_key_infix(KeyInfo *,
                       optimizer::SEL_ARG *index_range_tree,
                       KeyPartInfo *first_non_group_part,
                       KeyPartInfo *min_max_arg_part,
                       KeyPartInfo *last_part,
                       Session *,
                       unsigned char *key_infix,
                       uint32_t *key_infix_len,
                       KeyPartInfo **first_non_infix_part)
{
  optimizer::SEL_ARG *cur_range= NULL;
  KeyPartInfo *cur_part= NULL;
  /* End part for the first loop below. */
  KeyPartInfo *end_part= min_max_arg_part ? min_max_arg_part : last_part;

  *key_infix_len= 0;
  unsigned char *key_ptr= key_infix;
  for (cur_part= first_non_group_part; cur_part != end_part; cur_part++)
  {
    /*
      Find the range tree for the current keypart. We assume that
      index_range_tree points to the leftmost keypart in the index.
    */
    for (cur_range= index_range_tree; cur_range;
         cur_range= cur_range->next_key_part)
    {
      if (cur_range->field->eq(cur_part->field))
        break;
    }
    if (! cur_range)
    {
      if (min_max_arg_part)
        return false; /* The current keypart has no range predicates at all. */
      else
      {
        *first_non_infix_part= cur_part;
        return true;
      }
    }

    /* Check that the current range tree is a single point interval. */
    if (cur_range->prev || cur_range->next)
      return false; /* This is not the only range predicate for the field. */
    if ((cur_range->min_flag & NO_MIN_RANGE) ||
        (cur_range->max_flag & NO_MAX_RANGE) ||
        (cur_range->min_flag & NEAR_MIN) ||
        (cur_range->max_flag & NEAR_MAX))
      return false;

    uint32_t field_length= cur_part->store_length;
    if ((cur_range->maybe_null &&
         cur_range->min_value[0] && cur_range->max_value[0]) ||
        !memcmp(cur_range->min_value, cur_range->max_value, field_length))
    {
      /* cur_range specifies 'IS NULL' or an equality condition. */
      memcpy(key_ptr, cur_range->min_value, field_length);
      key_ptr+= field_length;
      *key_infix_len+= field_length;
    }
    else
      return false;
  }

  if (!min_max_arg_part && (cur_part == last_part))
    *first_non_infix_part= last_part;

  return true;
}


/*
  Find the key part referenced by a field.

  SYNOPSIS
    get_field_keypart()
    index  descriptor of an index
    field  field that possibly references some key part in index

  NOTES
    The return value can be used to get a KeyPartInfo pointer by
    part= index->key_part + get_field_keypart(...) - 1;

  RETURN
    Positive number which is the consecutive number of the key part, or
    0 if field does not reference any index field.
*/
static inline uint
get_field_keypart(KeyInfo *index, Field *field)
{
  KeyPartInfo *part= NULL;
  KeyPartInfo *end= NULL;

  for (part= index->key_part, end= part + index->key_parts; part < end; part++)
  {
    if (field->eq(part->field))
      return part - index->key_part + 1;
  }
  return 0;
}


/*
  Find the SEL_ARG sub-tree that corresponds to the chosen index.

  SYNOPSIS
    get_index_range_tree()
    index     [in]  The ID of the index being looked for
    range_tree[in]  Tree of ranges being searched
    param     [in]  Parameter from SqlSelect::test_quick_select
    param_idx [out] Index in the array Parameter::key that corresponds to 'index'

  DESCRIPTION

    A optimizer::SEL_TREE contains range trees for all usable indexes. This procedure
    finds the SEL_ARG sub-tree for 'index'. The members of a optimizer::SEL_TREE are
    ordered in the same way as the members of Parameter::key, thus we first find
    the corresponding index in the array Parameter::key. This index is returned
    through the variable param_idx, to be used later as argument of
    check_quick_select().

  RETURN
    Pointer to the SEL_ARG subtree that corresponds to index.
*/
optimizer::SEL_ARG *get_index_range_tree(uint32_t index,
                                         optimizer::SEL_TREE* range_tree,
                                         optimizer::Parameter *param,
                                         uint32_t *param_idx)
{
  uint32_t idx= 0; /* Index nr in param->key_parts */
  while (idx < param->keys)
  {
    if (index == param->real_keynr[idx])
      break;
    idx++;
  }
  *param_idx= idx;
  return range_tree->keys[idx];
}


/*
  Compute the cost of a quick_group_min_max_select for a particular index.

  SYNOPSIS
    cost_group_min_max()
    table                [in] The table being accessed
    index_info           [in] The index used to access the table
    used_key_parts       [in] Number of key parts used to access the index
    group_key_parts      [in] Number of index key parts in the group prefix
    range_tree           [in] Tree of ranges for all indexes
    index_tree           [in] The range tree for the current index
    quick_prefix_records [in] Number of records retrieved by the internally
			      used quick range select if any
    have_min             [in] True if there is a MIN function
    have_max             [in] True if there is a MAX function
    read_cost           [out] The cost to retrieve rows via this quick select
    records             [out] The number of rows retrieved

  DESCRIPTION
    This method computes the access cost of a GroupMinMaxReadPlan instance and
    the number of rows returned. It updates this->read_cost and this->records.

  NOTES
    The cost computation distinguishes several cases:
    1) No equality predicates over non-group attributes (thus no key_infix).
       If groups are bigger than blocks on the average, then we assume that it
       is very unlikely that block ends are aligned with group ends, thus even
       if we look for both MIN and MAX keys, all pairs of neighbor MIN/MAX
       keys, except for the first MIN and the last MAX keys, will be in the
       same block.  If groups are smaller than blocks, then we are going to
       read all blocks.
    2) There are equality predicates over non-group attributes.
       In this case the group prefix is extended by additional constants, and
       as a result the min/max values are inside sub-groups of the original
       groups. The number of blocks that will be read depends on whether the
       ends of these sub-groups will be contained in the same or in different
       blocks. We compute the probability for the two ends of a subgroup to be
       in two different blocks as the ratio of:
       - the number of positions of the left-end of a subgroup inside a group,
         such that the right end of the subgroup is past the end of the buffer
         containing the left-end, and
       - the total number of possible positions for the left-end of the
         subgroup, which is the number of keys in the containing group.
       We assume it is very unlikely that two ends of subsequent subgroups are
       in the same block.
    3) The are range predicates over the group attributes.
       Then some groups may be filtered by the range predicates. We use the
       selectivity of the range predicates to decide how many groups will be
       filtered.

  TODO
     - Take into account the optional range predicates over the MIN/MAX
       argument.
     - Check if we have a PK index and we use all cols - then each key is a
       group, and it will be better to use an index scan.

  RETURN
    None
*/
void cost_group_min_max(Table* table,
                        KeyInfo *index_info,
                        uint32_t used_key_parts,
                        uint32_t group_key_parts,
                        optimizer::SEL_TREE *range_tree,
                        optimizer::SEL_ARG *,
                        ha_rows quick_prefix_records,
                        bool have_min,
                        bool have_max,
                        double *read_cost,
                        ha_rows *records)
{
  ha_rows table_records;
  uint32_t num_groups;
  uint32_t num_blocks;
  uint32_t keys_per_block;
  uint32_t keys_per_group;
  uint32_t keys_per_subgroup; /* Average number of keys in sub-groups */
                          /* formed by a key infix. */
  double p_overlap; /* Probability that a sub-group overlaps two blocks. */
  double quick_prefix_selectivity;
  double io_cost;
  double cpu_cost= 0; /* TODO: CPU cost of index_read calls? */

  table_records= table->cursor->stats.records;
  keys_per_block= (table->cursor->stats.block_size / 2 /
                   (index_info->key_length + table->cursor->ref_length)
                        + 1);
  num_blocks= (uint32_t) (table_records / keys_per_block) + 1;

  /* Compute the number of keys in a group. */
  keys_per_group= index_info->rec_per_key[group_key_parts - 1];
  if (keys_per_group == 0) /* If there is no statistics try to guess */
    /* each group contains 10% of all records */
    keys_per_group= (uint32_t)(table_records / 10) + 1;
  num_groups= (uint32_t)(table_records / keys_per_group) + 1;

  /* Apply the selectivity of the quick select for group prefixes. */
  if (range_tree && (quick_prefix_records != HA_POS_ERROR))
  {
    quick_prefix_selectivity= (double) quick_prefix_records /
                              (double) table_records;
    num_groups= (uint32_t) rint(num_groups * quick_prefix_selectivity);
    set_if_bigger(num_groups, 1U);
  }

  if (used_key_parts > group_key_parts)
  { /*
      Compute the probability that two ends of a subgroup are inside
      different blocks.
    */
    keys_per_subgroup= index_info->rec_per_key[used_key_parts - 1];
    if (keys_per_subgroup >= keys_per_block) /* If a subgroup is bigger than */
      p_overlap= 1.0;       /* a block, it will overlap at least two blocks. */
    else
    {
      double blocks_per_group= (double) num_blocks / (double) num_groups;
      p_overlap= (blocks_per_group * (keys_per_subgroup - 1)) / keys_per_group;
      p_overlap= min(p_overlap, 1.0);
    }
    io_cost= (double) min(num_groups * (1 + p_overlap), (double)num_blocks);
  }
  else
    io_cost= (keys_per_group > keys_per_block) ?
             (have_min && have_max) ? (double) (num_groups + 1) :
                                      (double) num_groups :
             (double) num_blocks;

  /*
    TODO: If there is no WHERE clause and no other expressions, there should be
    no CPU cost. We leave it here to make this cost comparable to that of index
    scan as computed in SqlSelect::test_quick_select().
  */
  cpu_cost= (double) num_groups / TIME_FOR_COMPARE;

  *read_cost= io_cost + cpu_cost;
  *records= num_groups;
}


/*
  Construct a new quick select object for queries with group by with min/max.

  SYNOPSIS
    GroupMinMaxReadPlan::make_quick()
    param              Parameter from test_quick_select
    retrieve_full_rows ignored
    parent_alloc       Memory pool to use, if any.

  NOTES
    Make_quick ignores the retrieve_full_rows parameter because
    QuickGroupMinMaxSelect always performs 'index only' scans.
    The other parameter are ignored as well because all necessary
    data to create the QUICK object is computed at this TRP creation
    time.

  RETURN
    New QuickGroupMinMaxSelect object if successfully created,
    NULL otherwise.
*/
optimizer::QuickSelectInterface *
optimizer::GroupMinMaxReadPlan::make_quick(optimizer::Parameter *param, bool, memory::Root *parent_alloc)
{
  optimizer::QuickGroupMinMaxSelect *quick= new optimizer::QuickGroupMinMaxSelect(param->table,
                                               param->session->lex().current_select->join,
                                               have_min,
                                               have_max,
                                               min_max_arg_part,
                                               group_prefix_len,
                                               group_key_parts,
                                               used_key_parts,
                                               index_info,
                                               index,
                                               read_cost,
                                               records,
                                               key_infix_len,
                                               key_infix,
                                               parent_alloc);
  if (quick->init())
  {
    delete quick;
    return NULL;
  }

  if (range_tree)
  {
    assert(quick_prefix_records > 0);
    if (quick_prefix_records == HA_POS_ERROR)
    {
      quick->quick_prefix_select= NULL; /* Can't construct a quick select. */
    }
    else
    {
      /* Make a QuickRangeSelect to be used for group prefix retrieval. */
      quick->quick_prefix_select= optimizer::get_quick_select(param,
                                                              param_idx,
                                                              index_tree,
                                                              HA_MRR_USE_DEFAULT_IMPL,
                                                              0,
                                                              &quick->alloc);
    }

    /*
      Extract the SEL_ARG subtree that contains only ranges for the MIN/MAX
      attribute, and create an array of QuickRanges to be used by the
      new quick select.
    */
    if (min_max_arg_part)
    {
      optimizer::SEL_ARG *min_max_range= index_tree;
      while (min_max_range) /* Find the tree for the MIN/MAX key part. */
      {
        if (min_max_range->field->eq(min_max_arg_part->field))
          break;
        min_max_range= min_max_range->next_key_part;
      }
      /* Scroll to the leftmost interval for the MIN/MAX argument. */
      while (min_max_range && min_max_range->prev)
        min_max_range= min_max_range->prev;
      /* Create an array of QuickRanges for the MIN/MAX argument. */
      while (min_max_range)
      {
        if (quick->add_range(min_max_range))
        {
          delete quick;
          quick= NULL;
          return NULL;
        }
        min_max_range= min_max_range->next;
      }
    }
  }
  else
    quick->quick_prefix_select= NULL;

  quick->update_key_stat();
  quick->adjust_prefix_ranges();

  return quick;
}


optimizer::QuickSelectInterface *optimizer::RangeReadPlan::make_quick(optimizer::Parameter *param, bool, memory::Root *parent_alloc)
{
  optimizer::QuickRangeSelect *quick= optimizer::get_quick_select(param, key_idx, key, mrr_flags, mrr_buf_size, parent_alloc);
  if (quick)
  {
    quick->records= records;
    quick->read_time= read_cost;
  }
  return quick;
}


uint32_t optimizer::RorScanInfo::findFirstNotSet() const
{
  boost::dynamic_bitset<> map= bitsToBitset();
  for (boost::dynamic_bitset<>::size_type i= 0; i < map.size(); i++)
  {
    if (not map.test(i))
      return i;
  }
  return map.size();
}


size_t optimizer::RorScanInfo::getBitCount() const
{
  boost::dynamic_bitset<> tmp_bitset= bitsToBitset();
  return tmp_bitset.count();
}


void optimizer::RorScanInfo::subtractBitset(const boost::dynamic_bitset<>& in_bitset)
{
  boost::dynamic_bitset<> tmp_bitset= bitsToBitset();
  tmp_bitset-= in_bitset;
  covered_fields= tmp_bitset.to_ulong();
}


boost::dynamic_bitset<> optimizer::RorScanInfo::bitsToBitset() const
{
  string res;
  uint64_t conv= covered_fields;
  while (conv)
  {
    res.push_back((conv & 1) + '0');
    conv>>= 1;
  }
  if (! res.empty())
  {
    std::reverse(res.begin(), res.end());
  }
  string final(covered_fields_size - res.length(), '0');
  final.append(res);
  return boost::dynamic_bitset<>(final);
}


} /* namespace drizzled */
