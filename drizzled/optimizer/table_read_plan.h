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

#ifndef DRIZZLED_OPTIMIZER_TABLE_READ_PLAN_H
#define DRIZZLED_OPTIMIZER_TABLE_READ_PLAN_H

#include "drizzled/util/functors.h"
#include <algorithm>

namespace drizzled
{

struct st_ror_scan_info;

namespace optimizer
{

class Parameter;
class SEL_ARG;
class SEL_TREE;

/*
  Table rows retrieval plan. Range optimizer creates QuickSelectInterface-derived
  objects from table read plans.
*/
class TableReadPlan
{
public:
  /*
    Plan read cost, with or without cost of full row retrieval, depending
    on plan creation parameters.
  */
  double read_cost;
  ha_rows records; /* estimate of #rows to be examined */

  /*
    If true, the scan returns rows in rowid order. This is used only for
    scans that can be both ROR and non-ROR.
  */
  bool is_ror;

  /*
    Create quick select for this plan.
    SYNOPSIS
     make_quick()
       param               Parameter from test_quick_select
       retrieve_full_rows  If true, created quick select will do full record
                           retrieval.
       parent_alloc        Memory pool to use, if any.

    NOTES
      retrieve_full_rows is ignored by some implementations.

    RETURN
      created quick select
      NULL on any error.
  */
  virtual QuickSelectInterface *make_quick(Parameter *param,
                                           bool retrieve_full_rows,
                                           memory::Root *parent_alloc= NULL) = 0;

  /* Table read plans are allocated on memory::Root and are never deleted */
  static void *operator new(size_t size, memory::Root *mem_root)
  { 
    return (void*) mem_root->alloc_root((uint32_t) size); 
  }

  static void operator delete(void *, size_t)
  { }

  static void operator delete(void *, memory::Root *)
    { /* Never called */ }

  virtual ~TableReadPlan() {} /* Remove gcc warning */

};


/*
  Plan for a QuickRangeSelect scan.
  RangeReadPlan::make_quick ignores retrieve_full_rows parameter because
  QuickRangeSelect doesn't distinguish between 'index only' scans and full
  record retrieval scans.
*/
class RangeReadPlan : public TableReadPlan
{

public:

  RangeReadPlan(SEL_ARG *key_arg, uint32_t idx_arg, uint32_t mrr_flags_arg)
    :
      key(key_arg),
      key_idx(idx_arg),
      mrr_flags(mrr_flags_arg),
      mrr_buf_size(0)
  {}

  virtual ~RangeReadPlan() {}                     /* Remove gcc warning */

  QuickSelectInterface *make_quick(Parameter *param, bool, memory::Root *parent_alloc);

  void setMRRBufferSize(uint32_t in_mrr_buf_size)
  {
    mrr_buf_size= in_mrr_buf_size;
  }

  uint32_t getKeyIndex() const
  {
    return key_idx;
  }

  uint32_t getMRRBufferSize() const
  {
    return mrr_buf_size;
  }

private:

  /** set of intervals to be used in "range" method retrieval */
  SEL_ARG *key;
  /** key number in Parameter::key */
  uint32_t     key_idx;
  uint32_t     mrr_flags;
  uint32_t     mrr_buf_size;

};


/* Plan for QuickRorIntersectSelect scan. */
class RorIntersectReadPlan : public TableReadPlan
{
public:

  RorIntersectReadPlan() 
    :
      ror_range_scans(),
      cpk_scan(NULL),
      is_covering(false),
      index_scan_costs(0.0)
  {}

  virtual ~RorIntersectReadPlan() 
  {
    std::for_each(ror_range_scans.begin(),
                  ror_range_scans.end(),
                  DeletePtr());
    ror_range_scans.clear();
  }

  QuickSelectInterface *make_quick(Parameter *param,
                                   bool retrieve_full_rows,
                                   memory::Root *parent_alloc);

  void setRowRetrievalNecessary(bool in_is_covering)
  {
    is_covering= in_is_covering;
  }

  void setCostOfIndexScans(double in_index_scan_costs)
  {
    index_scan_costs= in_index_scan_costs;
  }

  /**
   * @return true if row retrival phase is necessary.
   */
  bool isRowRetrievalNecessary() const
  {
    return ! is_covering;
  }

  /**
   * @return the sum of the cost of each index scan
   */
  double getCostOfIndexScans() const
  {
    return index_scan_costs;
  }

  /** Vector of pointers to ROR range scans used in this intersection */
  std::vector<struct st_ror_scan_info *> ror_range_scans;
  struct st_ror_scan_info *cpk_scan;  /* Clustered PK scan, if there is one */

private:

  /** true if no row retrieval phase is necessary */
  bool is_covering; 
  /* SUM(cost(index_scan)) */
  double index_scan_costs; 

};


/*
  Plan for QuickRorUnionSelect scan.
  QuickRorUnionSelect always retrieves full rows, so retrieve_full_rows
  is ignored by make_quick.
*/

class RorUnionReadPlan : public TableReadPlan
{
public:
  RorUnionReadPlan() {}                          /* Remove gcc warning */
  virtual ~RorUnionReadPlan() {}                 /* Remove gcc warning */
  QuickSelectInterface *make_quick(Parameter *param,
                                   bool retrieve_full_rows,
                                   memory::Root *parent_alloc);
  /** vector of plans for merged scans */
  std::vector<TableReadPlan *> merged_scans;
};


/*
  Plan for QuickIndexMergeSelect scan.
  QuickRorIntersectSelect always retrieves full rows, so retrieve_full_rows
  is ignored by make_quick.
*/

class IndexMergeReadPlan : public TableReadPlan
{
public:
  IndexMergeReadPlan() {}                        /* Remove gcc warning */
  virtual ~IndexMergeReadPlan() {}               /* Remove gcc warning */
  QuickSelectInterface *make_quick(Parameter *param,
                                   bool retrieve_full_rows,
                                   memory::Root *parent_alloc);
  RangeReadPlan **range_scans; /* array of ptrs to plans of merged scans */
  RangeReadPlan **range_scans_end; /* end of the array */
};


/*
  Plan for a QuickGroupMinMaxSelect scan.
*/

class GroupMinMaxReadPlan : public TableReadPlan
{

public:

  GroupMinMaxReadPlan(bool have_min_arg, 
                      bool have_max_arg,
                      KEY_PART_INFO *min_max_arg_part_arg,
                      uint32_t group_prefix_len_arg, 
                      uint32_t used_key_parts_arg,
                      uint32_t group_key_parts_arg, 
                      KEY *index_info_arg,
                      uint32_t index_arg, 
                      uint32_t key_infix_len_arg,
                      unsigned char *key_infix_arg,
                      SEL_TREE *tree_arg, 
                      SEL_ARG *index_tree_arg,
                      uint32_t param_idx_arg, 
                      ha_rows quick_prefix_records_arg)
    :
      quick_prefix_records(quick_prefix_records_arg),
      have_min(have_min_arg),
      have_max(have_max_arg),
      min_max_arg_part(min_max_arg_part_arg),
      group_prefix_len(group_prefix_len_arg),
      used_key_parts(used_key_parts_arg),
      group_key_parts(group_key_parts_arg),
      index_info(index_info_arg),
      index(index_arg),
      key_infix_len(key_infix_len_arg),
      range_tree(tree_arg),
      index_tree(index_tree_arg),
      param_idx(param_idx_arg)
    {
      if (key_infix_len)
        memcpy(this->key_infix, key_infix_arg, key_infix_len);
    }
  virtual ~GroupMinMaxReadPlan() {}             /* Remove gcc warning */

  QuickSelectInterface *make_quick(Parameter *param,
                                   bool retrieve_full_rows,
                                   memory::Root *parent_alloc);

  /* Number of records selected by the ranges in index_tree. */
  ha_rows quick_prefix_records;

private:

  bool have_min;
  bool have_max;
  KEY_PART_INFO *min_max_arg_part;
  uint32_t group_prefix_len;
  uint32_t used_key_parts;
  uint32_t group_key_parts;
  KEY *index_info;
  uint32_t index;
  uint32_t key_infix_len;
  unsigned char key_infix[MAX_KEY_LENGTH];
  SEL_TREE *range_tree; /* Represents all range predicates in the query. */
  SEL_ARG *index_tree; /* The SEL_ARG sub-tree corresponding to index_info. */
  uint32_t param_idx; /* Index of used key in param->key. */
};


} /* namespace optimizer */

} /* namespace drizzled */

#endif /* DRIZZLED_OPTIMIZER_TABLE_READ_PLAN_H */
