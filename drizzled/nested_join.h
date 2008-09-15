#ifndef DRIZZLED_NESTED_JOIN_H
#define DRIZZLED_NESTED_JOIN_H

struct nested_join_st
{
  List<TableList>  join_list;       /* list of elements in the nested join */
  table_map         used_tables;     /* bitmap of tables in the nested join */
  table_map         not_null_tables; /* tables that rejects nulls           */
  struct st_join_table *first_nested;/* the first nested table in the plan  */
  /* 
    Used to count tables in the nested join in 2 isolated places:
    1. In make_outerjoin_info(). 
    2. check_interleaving_with_nj/restore_prev_nj_state (these are called
       by the join optimizer. 
    Before each use the counters are zeroed by reset_nj_counters.
  */
  uint              counter_;
  nested_join_map   nj_map;          /* Bit used to identify this nested join*/
  /*
    (Valid only for semi-join nests) Bitmap of tables outside the semi-join
    that are used within the semi-join's ON condition.
  */
  table_map         sj_depends_on;
  /* Outer non-trivially correlated tables */
  table_map         sj_corr_tables;
  List<Item>        sj_outer_expr_list;
};

#endif /* DRIZZLED_NESTED_JOIN_H */
