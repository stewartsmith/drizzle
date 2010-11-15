/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

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

#include "config.h"

#include "data_dictionary.h"

#include "drizzled/current_session.h"

extern "C" {
#include "trx0i_s.h"
#include "trx0trx.h" /* for TRX_QUE_STATE_STR_MAX_LEN */
#include "buf0buddy.h" /* for i_s_cmpmem */
#include "buf0buf.h" /* for buf_pool and PAGE_ZIP_MIN_SIZE */
#include "ha_prototypes.h" /* for innobase_convert_name() */
#include "srv0start.h" /* for srv_was_started */
}
#include "handler0vars.h"

using namespace drizzled;

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
  buf_pool_mutex_enter();
}

CmpmemTool::Generator::~Generator()
{
  buf_pool_mutex_exit();
}

bool CmpmemTool::Generator::populate()
{
  if (record_number > BUF_BUDDY_SIZES)
  {
    return false;
  }

  buf_buddy_stat_t* buddy_stat = &buf_buddy_stat[record_number];

  push(static_cast<uint64_t>(BUF_BUDDY_LOW << record_number));
  push(static_cast<uint64_t>(buddy_stat->used));
  uint64_t pages_free= (UNIV_LIKELY(record_number < BUF_BUDDY_SIZES) ? UT_LIST_GET_LEN(buf_pool->zip_free[record_number]) : 0);
  push(pages_free);

  push(buddy_stat->relocated);
  push(buddy_stat->relocated_usec / 1000000);


  if (inner_reset)
  {
    buddy_stat->relocated = 0;
    buddy_stat->relocated_usec = 0;
  }

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
    errmsg_printf(ERRMSG_LVL_ERROR, _("Warning: data in %s truncated due to memory limit of %d bytes\n"), 
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
                                  current_session, TRUE);
   push(bufend);

   if (row->lock_index != NULL)
   {
     bufend = innobase_convert_name(buf, sizeof(buf),
                                    row->lock_index,
                                    strlen(row->lock_index),
                                    current_session, FALSE);
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
    {
      push(row->trx_query);
    }
    else
    {
      push();
    }
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
