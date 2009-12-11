
#ifndef DRIZZLED_OPTIMIZER_TABLE_READ_PLAN_H
#define DRIZZLED_OPTIMIZER_TABLE_READ_PLAN_H

class SEL_TREE;
struct st_ror_scan_info;

namespace drizzled
{

namespace optimizer
{

class Parameter;
class SEL_ARG;

/*
  Table rows retrieval plan. Range optimizer creates QuickSelectInterface-derived
  objects from table read plans.
*/
class TABLE_READ_PLAN
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
                                           MEM_ROOT *parent_alloc= NULL) = 0;

  /* Table read plans are allocated on MEM_ROOT and are never deleted */
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { 
    return (void*) alloc_root(mem_root, (uint32_t) size); 
  }

  static void operator delete(void *, size_t)
  { 
    TRASH(ptr, size); 
  }

  static void operator delete(void *, MEM_ROOT *)
    { /* Never called */ }

  virtual ~TABLE_READ_PLAN() {} /* Remove gcc warning */

};


/*
  Plan for a QuickRangeSelect scan.
  TRP_RANGE::make_quick ignores retrieve_full_rows parameter because
  QuickRangeSelect doesn't distinguish between 'index only' scans and full
  record retrieval scans.
*/
class TRP_RANGE : public TABLE_READ_PLAN
{

public:

  SEL_ARG *key; /* set of intervals to be used in "range" method retrieval */
  uint32_t     key_idx; /* key number in Parameter::key */
  uint32_t     mrr_flags;
  uint32_t     mrr_buf_size;

  TRP_RANGE(SEL_ARG *key_arg, uint32_t idx_arg, uint32_t mrr_flags_arg)
    :
      key(key_arg),
      key_idx(idx_arg),
      mrr_flags(mrr_flags_arg)
  {}
  virtual ~TRP_RANGE() {}                     /* Remove gcc warning */

  QuickSelectInterface *make_quick(Parameter *param, bool, MEM_ROOT *parent_alloc);

};


/* Plan for QuickRorIntersectSelect scan. */

class TRP_ROR_INTERSECT : public TABLE_READ_PLAN
{
public:
  TRP_ROR_INTERSECT() {}                      /* Remove gcc warning */
  virtual ~TRP_ROR_INTERSECT() {}             /* Remove gcc warning */
  QuickSelectInterface *make_quick(Parameter *param,
                                   bool retrieve_full_rows,
                                   MEM_ROOT *parent_alloc);

  /* Array of pointers to ROR range scans used in this intersection */
  struct st_ror_scan_info **first_scan;
  struct st_ror_scan_info **last_scan; /* End of the above array */
  struct st_ror_scan_info *cpk_scan;  /* Clustered PK scan, if there is one */
  bool is_covering; /* true if no row retrieval phase is necessary */
  double index_scan_costs; /* SUM(cost(index_scan)) */
};


/*
  Plan for QuickRorUnionSelect scan.
  QuickRorUnionSelect always retrieves full rows, so retrieve_full_rows
  is ignored by make_quick.
*/

class TRP_ROR_UNION : public TABLE_READ_PLAN
{
public:
  TRP_ROR_UNION() {}                          /* Remove gcc warning */
  virtual ~TRP_ROR_UNION() {}                 /* Remove gcc warning */
  QuickSelectInterface *make_quick(Parameter *param,
                                   bool retrieve_full_rows,
                                   MEM_ROOT *parent_alloc);
  TABLE_READ_PLAN **first_ror; /* array of ptrs to plans for merged scans */
  TABLE_READ_PLAN **last_ror;  /* end of the above array */
};


/*
  Plan for QuickIndexMergeSelect scan.
  QuickRorIntersectSelect always retrieves full rows, so retrieve_full_rows
  is ignored by make_quick.
*/

class TRP_INDEX_MERGE : public TABLE_READ_PLAN
{
public:
  TRP_INDEX_MERGE() {}                        /* Remove gcc warning */
  virtual ~TRP_INDEX_MERGE() {}               /* Remove gcc warning */
  QuickSelectInterface *make_quick(Parameter *param,
                                   bool retrieve_full_rows,
                                   MEM_ROOT *parent_alloc);
  TRP_RANGE **range_scans; /* array of ptrs to plans of merged scans */
  TRP_RANGE **range_scans_end; /* end of the array */
};


/*
  Plan for a QuickGroupMinMaxSelect scan.
*/

class TRP_GROUP_MIN_MAX : public TABLE_READ_PLAN
{
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
  /* Number of records selected by the ranges in index_tree. */
public:
  ha_rows quick_prefix_records;

public:
  TRP_GROUP_MIN_MAX(bool have_min_arg, 
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
      param_idx(param_idx_arg),
      quick_prefix_records(quick_prefix_records_arg)
    {
      if (key_infix_len)
        memcpy(this->key_infix, key_infix_arg, key_infix_len);
    }
  virtual ~TRP_GROUP_MIN_MAX() {}             /* Remove gcc warning */

  QuickSelectInterface *make_quick(Parameter *param,
                                   bool retrieve_full_rows,
                                   MEM_ROOT *parent_alloc);
};


} /* namespace optimizer */

} /* namespace drizzled */

#endif /* DRIZZLED_OPTIMIZER_TABLE_READ_PLAN_H */
