#ifndef DRIZZLED_SJ_TMP_TABLE_H
#define DRIZZLED_SJ_TMP_TABLE_H

#include <drizzled/server_includes.h>
#include "table.h"

/*
  Describes use of one temporary table to weed out join duplicates.
  The temporar

  Used to
    - create a temp table
    - when we reach the weed-out tab, walk through rowid-ed tabs and
      and copy rowids.
      For each table we need
       - rowid offset
       - null bit address.
*/

class SJ_TMP_TABLE : public Sql_alloc
{
public:
  /* Array of pointers to tables that should be "used" */
  class TAB
  {
  public:
    struct st_join_table *join_tab;
    uint rowid_offset;
    ushort null_byte;
    uchar null_bit;
  };
  TAB *tabs;
  TAB *tabs_end;

  uint null_bits;
  uint null_bytes;
  uint rowid_len;

  Table *tmp_table;

  MI_COLUMNDEF *start_recinfo;
  MI_COLUMNDEF *recinfo;

  /* Pointer to next table (next->start_idx > this->end_idx) */
  SJ_TMP_TABLE *next; 
};

Table *create_duplicate_weedout_tmp_table(THD *thd, 
					  uint uniq_tuple_length_arg,
					  SJ_TMP_TABLE *sjtbl);

#endif /* DRIZZLED_SJ_TMP_TABLE_H */
