/*****************************************************************************

Copyright (C) 2007, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

#include <config.h>

#include "data_dictionary.h"

#include <drizzled/current_session.h>

#include "trx0i_s.h"
#include "trx0trx.h" /* for TRX_QUE_STATE_STR_MAX_LEN */
#include "buf0buddy.h" /* for i_s_cmpmem */
#include "buf0buf.h" /* for buf_pool and PAGE_ZIP_MIN_SIZE */
#include "ha_prototypes.h" /* for innobase_convert_name() */
#include "srv0start.h" /* for srv_was_started */
#include "btr0pcur.h"	/* for file sys_tables related info. */
#include "btr0types.h"
#include "dict0load.h"	/* for file sys_tables related info. */
#include "dict0mem.h"
#include "dict0types.h"
#include "handler0vars.h"

using namespace drizzled;

InnodbSysTablesTool::InnodbSysTablesTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_SYS_TABLES")
{
  add_field("TABLE_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("NAME", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("FLAG", plugin::TableFunction::NUMBER, 0, false);
  add_field("N_COLS", plugin::TableFunction::NUMBER, 0, false);
  add_field("SPACE", plugin::TableFunction::NUMBER, 0, false);
}

InnodbSysTablesTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  heap= NULL;
}

bool InnodbSysTablesTool::Generator::populate()
{
  if (heap == NULL)
  {
    heap = mem_heap_create(1000);
    mutex_enter(&(dict_sys->mutex));
    mtr_start(&mtr);

    rec = dict_startscan_system(&pcur, &mtr, SYS_TABLES);
  }
  else
  {
    /* Get the next record */
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);
    rec = dict_getnext_system(&pcur, &mtr);
  }

  if (! rec)
  {
    mtr_commit(&mtr);
    mutex_exit(&dict_sys->mutex);
    mem_heap_free(heap);
    return false;
  }

  const char*	err_msg;
  dict_table_t*	table_rec;

  /* Create and populate a dict_table_t structure with
     information from SYS_TABLES row */
  err_msg = dict_process_sys_tables_rec(heap, rec, &table_rec,
                                        DICT_TABLE_LOAD_FROM_RECORD);

  mtr_commit(&mtr);
  mutex_exit(&dict_sys->mutex);

  if (!err_msg) {
    push(table_rec->id);
    push(table_rec->name);
    push(static_cast<uint64_t>(table_rec->flags));
    push(static_cast<uint64_t>(table_rec->n_cols));
    push(static_cast<uint64_t>(table_rec->space));
  } else {
/*    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_CANT_FIND_SYSTEM_REC,
                        err_msg);
*/  }

  /* Since dict_process_sys_tables_rec() is called with
     DICT_TABLE_LOAD_FROM_RECORD, the table_rec is created in
     dict_process_sys_tables_rec(), we will need to free it */
  if (table_rec) {
    dict_mem_table_free(table_rec);
  }

  mem_heap_empty(heap);

  return true;
}

InnodbSysTableStatsTool::InnodbSysTableStatsTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_SYS_TABLESTATS")
{
  add_field("TABLE_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("NAME", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("STATS_INITIALIZED", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("NUM_ROWS", plugin::TableFunction::NUMBER, 0, false);
  add_field("CLUST_INDEX_SIZE", plugin::TableFunction::NUMBER, 0, false);
  add_field("OTHER_INDEX_SIZE", plugin::TableFunction::NUMBER, 0, false);
  add_field("MODIFIED_COUNTER", plugin::TableFunction::NUMBER, 0, false);
  add_field("AUTOINC", plugin::TableFunction::NUMBER, 0, false);
  add_field("HANDLES_OPENED", plugin::TableFunction::NUMBER, 0, false);
}

InnodbSysTableStatsTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  heap= NULL;
}

bool InnodbSysTableStatsTool::Generator::populate()
{
  if (heap == NULL)
  {
    heap = mem_heap_create(1000);
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);
    rec = dict_startscan_system(&pcur, &mtr, SYS_TABLES);
  }
  else
  {
    /* Get the next record */
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);
    rec = dict_getnext_system(&pcur, &mtr);
  }

  if (!rec)
  {
    mtr_commit(&mtr);
    mutex_exit(&dict_sys->mutex);
    mem_heap_free(heap);
    return false;
  }

  const char*	err_msg;
  dict_table_t*	table_rec;

  /* Fetch the dict_table_t structure corresponding to
     this SYS_TABLES record */
  err_msg = dict_process_sys_tables_rec(heap, rec, &table_rec,
                                        DICT_TABLE_LOAD_FROM_CACHE);

  mtr_commit(&mtr);
  mutex_exit(&dict_sys->mutex);

  if (!err_msg) {
    push(table_rec->id);
    push(table_rec->name);
    if (table_rec->stat_initialized)
      push("Initialized");
    else
      push("Uninitialized");
    push(table_rec->stat_n_rows);
    push(static_cast<uint64_t>(table_rec->stat_clustered_index_size));
    push(static_cast<uint64_t>(table_rec->stat_sum_of_other_index_sizes));
    push(static_cast<uint64_t>(table_rec->stat_modified_counter));
    push(table_rec->autoinc);
    push(static_cast<uint64_t>(table_rec->n_mysql_handles_opened));
  } else {
    /*    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_CANT_FIND_SYSTEM_REC,
                        err_msg);*/
  }

  mem_heap_empty(heap);

  return true;
}

InnodbSysIndexesTool::InnodbSysIndexesTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_SYS_INDEXES")
{
  add_field("INDEX_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("NAME", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("TABLE_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("TYPE", plugin::TableFunction::NUMBER, 0, false);
  add_field("N_FIELDS", plugin::TableFunction::NUMBER, 0, false);
  add_field("PAGE_NO", plugin::TableFunction::NUMBER, 0, false);
  add_field("SPACE", plugin::TableFunction::NUMBER, 0, false);
}

InnodbSysIndexesTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  heap= NULL;
}

bool InnodbSysIndexesTool::Generator::populate()
{
  if (heap == NULL)
  {
    heap = mem_heap_create(1000);
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);

    /* Start scan the SYS_INDEXES table */
    rec = dict_startscan_system(&pcur, &mtr, SYS_INDEXES);
  }
  else
  {
    /* Get the next record */
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);
    rec = dict_getnext_system(&pcur, &mtr);
  }

  if (! rec)
  {
    mtr_commit(&mtr);
    mutex_exit(&dict_sys->mutex);
    mem_heap_free(heap);
    return false;
  }

  const char*	err_msg;;
  table_id_t	table_id;
  dict_index_t	index_rec;

  /* Populate a dict_index_t structure with information from
     a SYS_INDEXES row */
  err_msg = dict_process_sys_indexes_rec(heap, rec, &index_rec,
                                         &table_id);

  mtr_commit(&mtr);
  mutex_exit(&dict_sys->mutex);
  if (!err_msg) {
    push(index_rec.id);
    push(index_rec.name);
    push(static_cast<uint64_t>(table_id));
    push(static_cast<uint64_t>(index_rec.type));
    push(static_cast<uint64_t>(index_rec.n_fields));
    push(static_cast<uint64_t>(index_rec.page));
    push(static_cast<uint64_t>(index_rec.space));
  } else {
/*    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_CANT_FIND_SYSTEM_REC,
                        err_msg);*/
  }

  mem_heap_empty(heap);

  if (!rec)
  {
    mtr_commit(&mtr);
    mutex_exit(&dict_sys->mutex);
    mem_heap_free(heap);
    return false;
  }

  return true;
}

InnodbSysColumnsTool::InnodbSysColumnsTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_SYS_COLUMNS")
{
  add_field("TABLE_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("NAME", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("POS", plugin::TableFunction::NUMBER, 0, false);
  add_field("MTYPE", plugin::TableFunction::NUMBER, 0, false);
  add_field("PRTYPE", plugin::TableFunction::NUMBER, 0, false);
  add_field("LEN", plugin::TableFunction::NUMBER, 0, false);
}

InnodbSysColumnsTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  heap= NULL;
}

bool InnodbSysColumnsTool::Generator::populate()
{
  if (heap == NULL)
  {
    heap = mem_heap_create(1000);
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);
    rec = dict_startscan_system(&pcur, &mtr, SYS_COLUMNS);
  }
  else
  {
    /* Get the next record */
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);
    rec = dict_getnext_system(&pcur, &mtr);
  }

  if (! rec)
  {
    mtr_commit(&mtr);
    mutex_exit(&dict_sys->mutex);
    mem_heap_free(heap);
    return false;
  }

  const char*	err_msg;
  dict_col_t	column_rec;
  table_id_t	table_id;
  const char*	col_name;

  /* populate a dict_col_t structure with information from
     a SYS_COLUMNS row */
  err_msg = dict_process_sys_columns_rec(heap, rec, &column_rec,
						       &table_id, &col_name);

  mtr_commit(&mtr);
  mutex_exit(&dict_sys->mutex);

  if (!err_msg) {
    push(table_id);
    push(col_name);
    push(static_cast<uint64_t>(column_rec.ind));
    push(static_cast<uint64_t>(column_rec.mtype));
    push(static_cast<uint64_t>(column_rec.prtype));
    push(static_cast<uint64_t>(column_rec.len));
  } else {
/*    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_CANT_FIND_SYSTEM_REC,
                        err_msg);*/
  }

  mem_heap_empty(heap);

  return true;
}

InnodbSysFieldsTool::InnodbSysFieldsTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_SYS_FIELDS")
{
  add_field("INDEX_ID", plugin::TableFunction::NUMBER, 0, false);
  add_field("NAME", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("POS", plugin::TableFunction::NUMBER, 0, false);
}

InnodbSysFieldsTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  heap= NULL;
}

bool InnodbSysFieldsTool::Generator::populate()
{
  if (heap == NULL)
  {
    heap = mem_heap_create(1000);
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);

    /* will save last index id so that we know whether we move to
       the next index. This is used to calculate prefix length */
    last_id = 0;

    rec = dict_startscan_system(&pcur, &mtr, SYS_FIELDS);
  }
  else
  {
    /* Get the next record */
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);
    rec = dict_getnext_system(&pcur, &mtr);
  }

  if (! rec)
  {
    mtr_commit(&mtr);
    mutex_exit(&dict_sys->mutex);
    mem_heap_free(heap);

    return false;
  }

  ulint		pos;
  const char*	err_msg;
  index_id_t	index_id;
  dict_field_t	field_rec;

  /* Populate a dict_field_t structure with information from
     a SYS_FIELDS row */
  err_msg = dict_process_sys_fields_rec(heap, rec, &field_rec,
                                        &pos, &index_id, last_id);

  mtr_commit(&mtr);
  mutex_exit(&dict_sys->mutex);

  if (!err_msg) {
    push(index_id);
    push(field_rec.name);
    push(static_cast<uint64_t>(pos));

    last_id = index_id;
  } else {
/*    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_CANT_FIND_SYSTEM_REC,
                        err_msg);*/
  }

  mem_heap_empty(heap);

  return true;
}

InnodbSysForeignTool::InnodbSysForeignTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_SYS_FOREIGN")
{
  add_field("ID", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("FOR_NAME", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("REF_NAME", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("N_COLS", plugin::TableFunction::NUMBER, 0, false);
  add_field("TYPE", plugin::TableFunction::NUMBER, 0, false);
}

InnodbSysForeignTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  heap= NULL;
}

bool InnodbSysForeignTool::Generator::populate()
{
  if (heap == NULL)
  {
    heap = mem_heap_create(1000);
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);

    rec = dict_startscan_system(&pcur, &mtr, SYS_FOREIGN);
  }
  else
  {
    /* Get the next record */
    mtr_start(&mtr);
    mutex_enter(&dict_sys->mutex);
    rec = dict_getnext_system(&pcur, &mtr);
  }

  if (! rec)
  {
    mtr_commit(&mtr);
    mutex_exit(&dict_sys->mutex);
    mem_heap_free(heap);
    return false;
  }

  const char*	err_msg;
  dict_foreign_t	foreign_rec;

  /* Populate a dict_foreign_t structure with information from
     a SYS_FOREIGN row */
  err_msg = dict_process_sys_foreign_rec(heap, rec, &foreign_rec);

  mtr_commit(&mtr);
  mutex_exit(&dict_sys->mutex);

  if (!err_msg) {
    push(foreign_rec.id);
    push(foreign_rec.foreign_table_name);
    push(foreign_rec.referenced_table_name);
    push(static_cast<uint64_t>(foreign_rec.n_fields));
    push(static_cast<uint64_t>(foreign_rec.type));
  } else {
/*    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_CANT_FIND_SYSTEM_REC,
                        err_msg);
*/  }

  mem_heap_empty(heap);

  return true;
}

InnodbSysForeignColsTool::InnodbSysForeignColsTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_SYS_FOREIGN_COLS")
{
  add_field("ID", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("FOR_COL_NAME", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("REF_COL_NAME", plugin::TableFunction::STRING, NAME_LEN + 1, false);
  add_field("POS", plugin::TableFunction::NUMBER, 0, false);
}

InnodbSysForeignColsTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  heap= NULL;
}

bool InnodbSysForeignColsTool::Generator::populate()
{
  if (heap == NULL)
  {
    heap = mem_heap_create(1000);
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);

    rec = dict_startscan_system(&pcur, &mtr, SYS_FOREIGN_COLS);
  }
  else
  {
    /* Get the next record */
    mutex_enter(&dict_sys->mutex);
    mtr_start(&mtr);
    rec = dict_getnext_system(&pcur, &mtr);
  }

  if (! rec)
  {
    mtr_commit(&mtr);
    mutex_exit(&dict_sys->mutex);
    mem_heap_free(heap);

    return false;
  }

  const char*	err_msg;
  const char*	name;
  const char*	for_col_name;
  const char*	ref_col_name;
  ulint		pos;

  /* Extract necessary information from a SYS_FOREIGN_COLS row */
  err_msg = dict_process_sys_foreign_col_rec(heap, rec, &name, &for_col_name,
                                             &ref_col_name, &pos);

  mtr_commit(&mtr);
  mutex_exit(&dict_sys->mutex);

  if (!err_msg) {
    push(name);
    push(for_col_name);
    push(ref_col_name);
    push(static_cast<uint64_t>(pos));
  } else {
/*    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_CANT_FIND_SYSTEM_REC,
                        err_msg);
*/
  }

  mem_heap_empty(heap);

  return true;
}

/*
 * Fill the dynamic table data_dictionary.INNODB_CMP and INNODB_CMP_RESET
 *
 */
CmpTool::CmpTool(bool in_reset) :
  plugin::TableFunction("DATA_DICTIONARY", in_reset ? "INNODB_CMP_RESET" : "INNODB_CMP"),
  outer_reset(in_reset)
{
  add_field("PAGE_SIZE", plugin::TableFunction::NUMBER, 0, false);
  add_field("COMPRESS_OPS", plugin::TableFunction::NUMBER, 0, false);
  add_field("COMPRESS_OPS_OK", plugin::TableFunction::NUMBER, 0, false);
  add_field("COMPRESS_TIME", plugin::TableFunction::NUMBER, 0, false);
  add_field("UNCOMPRESS_OPS", plugin::TableFunction::NUMBER, 0, false);
  add_field("UNCOMPRESS_TIME", plugin::TableFunction::NUMBER, 0, false);
}

CmpTool::Generator::Generator(Field **arg, bool in_reset) :
  plugin::TableFunction::Generator(arg),
  record_number(0),
  inner_reset(in_reset)
{
}

bool CmpTool::Generator::populate()
{
  if (record_number == (PAGE_ZIP_NUM_SSIZE - 1))
  {
    return false;
  }

  page_zip_stat_t*        zip_stat = &page_zip_stat[record_number];

  push(static_cast<uint64_t>(PAGE_ZIP_MIN_SIZE << record_number));

  /* The cumulated counts are not protected by any
     mutex.  Thus, some operation in page0zip.c could
     increment a counter between the time we read it and
     clear it.  We could introduce mutex protection, but it
     could cause a measureable performance hit in
     page0zip.c. */
  push(static_cast<uint64_t>(zip_stat->compressed));
  push(static_cast<uint64_t>(zip_stat->compressed_ok));
  push(zip_stat->compressed_usec / 1000000);
  push(static_cast<uint64_t>(zip_stat->decompressed));
  push(zip_stat->decompressed_usec / 1000000);

  if (inner_reset)
  {
    memset(zip_stat, 0, sizeof *zip_stat);
  }

  record_number++;

  return true;
}

/*
 * Fill the dynamic table data_dictionary.INNODB_CMPMEM and INNODB_CMPMEM_RESET
 *
 */
CmpmemTool::CmpmemTool(bool in_reset) :
  plugin::TableFunction("DATA_DICTIONARY", in_reset ? "INNODB_CMPMEM_RESET" : "INNODB_CMPMEM"),
  outer_reset(in_reset)
{
  add_field("BUF_POOL", plugin::TableFunction::NUMBER, 0, false);
  add_field("PAGE_SIZE", plugin::TableFunction::NUMBER, 0, false);
  add_field("PAGES_USED", plugin::TableFunction::NUMBER, 0, false);
  add_field("PAGES_FREE", plugin::TableFunction::NUMBER, 0, false);
  add_field("RELOCATION_OPS", plugin::TableFunction::NUMBER, 0, false);
  add_field("RELOCATION_TIME", plugin::TableFunction::NUMBER, 0, false);
}

CmpmemTool::Generator::Generator(Field **arg, bool in_reset) :
  plugin::TableFunction::Generator(arg),
  record_number(0),
  inner_reset(in_reset)
{
}

CmpmemTool::Generator::~Generator()
{
}

bool CmpmemTool::Generator::populate()
{
  if (record_number >= (BUF_BUDDY_SIZES+1)*srv_buf_pool_instances)
  {
    return false;
  }

  uint32_t buddy_nr= record_number % (BUF_BUDDY_SIZES+1);
  uint32_t buf_pool_nr= (record_number/(BUF_BUDDY_SIZES+1));

  buf_pool_t *buf_pool= buf_pool_from_array(buf_pool_nr);

  buf_pool_mutex_enter(buf_pool);

  buf_buddy_stat_t* buddy_stat = &buf_pool->buddy_stat[buddy_nr];


  push(static_cast<uint64_t>(buf_pool_nr));
  push(static_cast<uint64_t>(BUF_BUDDY_LOW << buddy_nr));
  push(static_cast<uint64_t>(buddy_stat->used));


  uint64_t pages_free= (UNIV_LIKELY(buddy_nr < BUF_BUDDY_SIZES) ? UT_LIST_GET_LEN(buf_pool->zip_free[buddy_nr]) : 0);
  push(pages_free);

  push(buddy_stat->relocated);
  push(buddy_stat->relocated_usec / 1000000);


  if (inner_reset)
  {
    buddy_stat->relocated = 0;
    buddy_stat->relocated_usec = 0;
  }

  buf_pool_mutex_exit(buf_pool);
  record_number++;

  return true;
}

/*
 * Fill the dynamic table data_dictionary.INNODB_TRX INNODB_LOCKS INNODB_LOCK_WAITS
 *
 */
InnodbTrxTool::InnodbTrxTool(const char* in_table_name) :
  plugin::TableFunction("DATA_DICTIONARY", in_table_name),
  table_name(in_table_name)
{
  if (innobase_strcasecmp(table_name, "INNODB_TRX") == 0)
  {
    add_field("TRX_ID");
    add_field("TRX_STATE");
    add_field("TRX_STARTED", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_REQUESTED_LOCK_ID");
    add_field("TRX_WAIT_STARTED", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_WEIGHT", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_DRIZZLE_THREAD_ID", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_QUERY", plugin::TableFunction::STRING, TRX_I_S_TRX_QUERY_MAX_LEN, true);
    add_field("TRX_OPERATION_STATE", plugin::TableFunction::STRING, TRX_I_S_TRX_OP_STATE_MAX_LEN, true);
//    add_field("TRX_TABLES_IN_USE", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_TABLES_LOCKED", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_LOCK_STRUCTS", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_LOCK_MEMORY_BYTES", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_ROWS_LOCKED", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_ROWS_MODIFIED", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_CONCURRENCY_TICKETS", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_ISOLATION_LEVEL", plugin::TableFunction::STRING, TRX_I_S_TRX_ISOLATION_LEVEL_MAX_LEN, false);
    add_field("TRX_UNIQUE_CHECKS", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_FOREIGN_KEY_CHECKS", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_LAST_FOREIGN_KEY_ERROR", plugin::TableFunction::STRING,
              TRX_I_S_TRX_FK_ERROR_MAX_LEN, true);
    add_field("TRX_ADAPTIVE_HASH_LATCHED", plugin::TableFunction::NUMBER, 0, false);
    add_field("TRX_ADAPTIVE_HASH_TIMEOUT", plugin::TableFunction::NUMBER, 0, false);
  }
  else if (innobase_strcasecmp(table_name, "INNODB_LOCKS") == 0)
  {
    add_field("LOCK_ID");
    add_field("LOCK_TRX_ID");
    add_field("LOCK_MODE");
    add_field("LOCK_TYPE");
    add_field("LOCK_TABLE");
    add_field("LOCK_INDEX");
    add_field("LOCK_SPACE", plugin::TableFunction::NUMBER, 0, false);
    add_field("LOCK_PAGE", plugin::TableFunction::NUMBER, 0, false);
    add_field("LOCK_REC", plugin::TableFunction::NUMBER, 0, false);
    add_field("LOCK_DATA");
  }
  else if (innobase_strcasecmp(table_name, "INNODB_LOCK_WAITS") == 0)
  {
    add_field("REQUESTING_TRX_ID");  
    add_field("REQUESTED_LOCK_ID");
    add_field("BLOCKING_TRX_ID");
    add_field("BLOCKING_LOCK_ID");
  } 
}

InnodbTrxTool::Generator::Generator(Field **arg, const char* in_table_name) :
  plugin::TableFunction::Generator(arg),
  table_name(in_table_name)
{
  /* update the cache */
  trx_i_s_cache_start_write(trx_i_s_cache);
  trx_i_s_possibly_fetch_data_into_cache(trx_i_s_cache);
  trx_i_s_cache_end_write(trx_i_s_cache);

  if (trx_i_s_cache_is_truncated(trx_i_s_cache))
  {
    errmsg_printf(error::ERROR, _("Warning: data in %s truncated due to memory limit of %d bytes\n"), 
                  table_name, TRX_I_S_MEM_LIMIT);
  } 

  trx_i_s_cache_start_read(trx_i_s_cache);

  if (innobase_strcasecmp(table_name, "INNODB_TRX") == 0)
    number_rows= trx_i_s_cache_get_rows_used(trx_i_s_cache, I_S_INNODB_TRX);
  else if (innobase_strcasecmp(table_name, "INNODB_LOCKS") == 0)
    number_rows= trx_i_s_cache_get_rows_used(trx_i_s_cache, I_S_INNODB_LOCKS);
  else if (innobase_strcasecmp(table_name, "INNODB_LOCK_WAITS") == 0)
    number_rows= trx_i_s_cache_get_rows_used(trx_i_s_cache, I_S_INNODB_LOCK_WAITS);

  record_number= 0;
}

InnodbTrxTool::Generator::~Generator()
{
  trx_i_s_cache_end_read(trx_i_s_cache);
}

bool InnodbTrxTool::Generator::populate()
{
  if (record_number == number_rows)
  {
    return false;
  }

  if (innobase_strcasecmp(table_name, "INNODB_TRX") == 0)
  {
    populate_innodb_trx();
  }
  else if (innobase_strcasecmp(table_name, "INNODB_LOCKS") == 0)
  {
    populate_innodb_locks();
  }
  else if (innobase_strcasecmp(table_name, "INNODB_LOCK_WAITS") == 0)
  {
    populate_innodb_lock_waits();
  }
  else 
  {
    return false;
  }
  record_number++;

  return true;
}

void InnodbTrxTool::Generator::populate_innodb_locks()
{

  char lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
  i_s_locks_row_t* row;

  /* note that the decoded database or table name is
     never expected to be longer than NAME_LEN;
     NAME_LEN for database name
     2 for surrounding quotes around database name
     NAME_LEN for table name
     2 for surrounding quotes around table name
     1 for the separating dot (.)
     9 for the #mysql50# prefix 
  */

   char buf[2 * NAME_LEN + 14];
   const char* bufend;

   char lock_trx_id[TRX_ID_MAX_LEN + 1];

   row = (i_s_locks_row_t*)
          trx_i_s_cache_get_nth_row(
          trx_i_s_cache, I_S_INNODB_LOCKS, record_number);


   trx_i_s_create_lock_id(row, lock_id, sizeof(lock_id));
   push(lock_id);

   ut_snprintf(lock_trx_id, sizeof(lock_trx_id),
               TRX_ID_FMT, row->lock_trx_id);
   push(lock_trx_id);

   push(row->lock_mode);
   push(row->lock_type);

   bufend = innobase_convert_name(buf, sizeof(buf),
                                  row->lock_table,
                                  strlen(row->lock_table),
                                  &getSession(), TRUE);
   push(bufend);

   if (row->lock_index != NULL)
   {
     bufend = innobase_convert_name(buf, sizeof(buf),
                                    row->lock_index,
                                    strlen(row->lock_index),
                                    &getSession(), FALSE);
     push(bufend);     
   }
   else 
   {
     push("");
   }   
 
   push(static_cast<uint64_t>(row->lock_space));
   push(static_cast<uint64_t>(row->lock_page)); 
   push(static_cast<uint64_t>(row->lock_rec));
   push(row->lock_data);
}

void InnodbTrxTool::Generator::populate_innodb_trx()
{
    char lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
    i_s_trx_row_t* row;
    char trx_id[TRX_ID_MAX_LEN + 1];
    row = (i_s_trx_row_t*) trx_i_s_cache_get_nth_row(trx_i_s_cache, I_S_INNODB_TRX, record_number);

    /* trx_id */
    ut_snprintf(trx_id, sizeof(trx_id), TRX_ID_FMT, row->trx_id);

    push(trx_id);
    push(row->trx_state);
    push(static_cast<uint64_t>(row->trx_started));

    if (row->trx_wait_started != 0)
    {
      push(trx_i_s_create_lock_id(row->requested_lock_row, lock_id, sizeof(lock_id)));
      push(static_cast<uint64_t>(row->trx_wait_started));
    }
    else
    {
      push(static_cast<uint64_t>(0));
      push(static_cast<uint64_t>(0));
    }

    push(static_cast<int64_t>(row->trx_weight));
    push(static_cast<uint64_t>(row->trx_mysql_thread_id));
    if (row->trx_query)
      push(row->trx_query);
    else
      push();

    if (row->trx_operation_state)
      push(row->trx_operation_state);
    else
      push();

//    push(row->trx_tables_in_use);
    push(static_cast<uint64_t>(row->trx_tables_locked));
    push(static_cast<uint64_t>(row->trx_lock_structs));
    push(static_cast<uint64_t>(row->trx_lock_memory_bytes));
    push(static_cast<uint64_t>(row->trx_rows_locked));
    push(static_cast<uint64_t>(row->trx_rows_modified));
    push(static_cast<uint64_t>(row->trx_concurrency_tickets));
    push(row->trx_isolation_level);
    push(static_cast<uint64_t>(row->trx_unique_checks));
    push(static_cast<uint64_t>(row->trx_foreign_key_checks));
    if (row->trx_foreign_key_error)
      push(row->trx_foreign_key_error);
    else
      push();

    push(static_cast<uint64_t>(row->trx_has_search_latch));
    push(static_cast<uint64_t>(row->trx_search_latch_timeout));
}

void InnodbTrxTool::Generator::populate_innodb_lock_waits()
{
  char requested_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
  char blocking_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];

  i_s_lock_waits_row_t* row;

  char requesting_trx_id[TRX_ID_MAX_LEN + 1];
  char blocking_trx_id[TRX_ID_MAX_LEN + 1];

  row = (i_s_lock_waits_row_t*)
         trx_i_s_cache_get_nth_row(
         trx_i_s_cache, I_S_INNODB_LOCK_WAITS, record_number);

  ut_snprintf(requesting_trx_id, sizeof(requesting_trx_id),
              TRX_ID_FMT, row->requested_lock_row->lock_trx_id);
  push(requesting_trx_id);

  push(trx_i_s_create_lock_id(row->requested_lock_row, requested_lock_id, 
       sizeof(requested_lock_id)));

  ut_snprintf(blocking_trx_id, sizeof(blocking_trx_id),
              TRX_ID_FMT, row->blocking_lock_row->lock_trx_id);
  push(blocking_trx_id);

  push(trx_i_s_create_lock_id(row->blocking_lock_row, blocking_lock_id, 
       sizeof(blocking_lock_id)));
}
