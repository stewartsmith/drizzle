/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <plugin/myisam/myisam.h>

namespace drizzled {

/*
  Param to create temporary tables when doing SELECT:s
  NOTE
    This structure is copied using memcpy as a part of JOIN.
*/

class Tmp_Table_Param :public memory::SqlAlloc
{
private:
  /* Prevent use of these (not safe because of lists and copy_field) */
  Tmp_Table_Param(const Tmp_Table_Param &);
  void operator=(Tmp_Table_Param &);

public:
  KeyInfo *keyinfo;
  List<Item> copy_funcs;
  List<Item> save_copy_funcs;
  CopyField *copy_field, *copy_field_end;
  CopyField *save_copy_field, *save_copy_field_end;
  unsigned char	    *group_buff;
  Item	    **items_to_copy;			/* Fields in tmp table */
  MI_COLUMNDEF *recinfo,*start_recinfo;
  ha_rows end_write_records;
  uint32_t	field_count;
  uint32_t	sum_func_count;
  uint32_t	func_count;
  uint32_t  hidden_field_count;
  uint32_t	group_parts,group_length,group_null_parts;
  uint32_t	quick_group;
  bool using_indirect_summary_function;
  bool schema_table;

  /*
    True if GROUP BY and its aggregate functions are already computed
    by a table access method (e.g. by loose index scan). In this case
    query execution should not perform aggregation and should treat
    aggregate functions as normal functions.
  */
  bool precomputed_group_by;

  bool force_copy_fields;

  /* If >0 convert all blob fields to varchar(convert_blob_length) */
  uint32_t  convert_blob_length;

  const charset_info_st *table_charset;

  Tmp_Table_Param() :
    keyinfo(0),
    copy_funcs(),
    save_copy_funcs(),
    copy_field(0),
    copy_field_end(0),
    save_copy_field(0),
    save_copy_field_end(0),
    group_buff(0),
    items_to_copy(0),
    recinfo(0),
    start_recinfo(0),
    end_write_records(0),
    field_count(0),
    sum_func_count(0),
    func_count(0),
    hidden_field_count(0),
    group_parts(0),
    group_length(0),
    group_null_parts(0),
    quick_group(0),
    using_indirect_summary_function(false),
    schema_table(false),
    precomputed_group_by(false),
    force_copy_fields(false),
    convert_blob_length(0),
    table_charset(0)
  {}

  ~Tmp_Table_Param()
  {
    cleanup();
  }
  void init(void);
  void cleanup(void);
};

} /* namespace drizzled */

