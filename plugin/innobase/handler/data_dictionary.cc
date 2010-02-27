
#include "config.h"

#include "data_dictionary.h"

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
 * Fill the dynamic table data_dictionary.innodb_cmp
 *
 */
CmpTool::CmpTool(bool in_reset) :
  plugin::TableFunction("DATA_DICTIONARY", in_reset ? "INNODB_CMP_RESET" : "INNODB_CMP"),
  outer_reset(in_reset)
{
  add_field("PAGE_SIZE", plugin::TableFunction::NUMBER);
  add_field("COMPRESS_OPS", plugin::TableFunction::NUMBER);
  add_field("COMPRESS_OPS_OK", plugin::TableFunction::NUMBER);
  add_field("COMPRESS_TIME", plugin::TableFunction::NUMBER);
  add_field("UNCOMPRESS_OPS", plugin::TableFunction::NUMBER);
  add_field("UNCOMPRESS_TIME", plugin::TableFunction::NUMBER);
}

CmpTool::Generator::Generator(Field **arg, bool in_reset) :
  plugin::TableFunction::Generator(arg)
{
  record_number= 0;
  inner_reset= in_reset;
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
  push(zip_stat->compressed);
  push(zip_stat->compressed_ok);
  push(zip_stat->compressed_usec / 1000000);
  push(zip_stat->decompressed);
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
  plugin::TableFunction("DATA_DICTIONARY", in_reset ? "INNODB_CMPMEM" : "INNODB_CMPMEM_RESET"),
  outer_reset(in_reset)
{
  add_field("PAGE_SIZE", plugin::TableFunction::NUMBER);
  add_field("PAGES_USED", plugin::TableFunction::NUMBER);
  add_field("PAGES_FREE", plugin::TableFunction::NUMBER);
  add_field("RELOCATION_OPS", plugin::TableFunction::NUMBER);
  add_field("RELOCATION_TIME", plugin::TableFunction::NUMBER);
}

CmpmemTool::Generator::Generator(Field **arg, bool in_reset) :
  plugin::TableFunction::Generator(arg)
{
  record_number= 0;
  inner_reset= in_reset;
  buf_pool_mutex_enter();
}

CmpmemTool::Generator::~Generator()
{
  buf_pool_mutex_exit();
}

bool CmpmemTool::Generator::populate()
{
  if (record_number == BUF_BUDDY_SIZES)
  {
    return false;
  }

  buf_buddy_stat_t* buddy_stat = &buf_buddy_stat[record_number];

  push(static_cast<uint64_t>(BUF_BUDDY_LOW << record_number));
  push(buddy_stat->used);
  push(UNIV_LIKELY(record_number < BUF_BUDDY_SIZES) 
                   ? UT_LIST_GET_LEN(buf_pool->zip_free[record_number]) : 0);

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
 * Fill the dynamic table data_dictionary.innodb_trx
 *
 */
InnodbTrxTool::InnodbTrxTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_TRX")
{
  add_field("TRX_ID");
  add_field("TRX_STATE");
  add_field("TRX_STARTED", plugin::TableFunction::NUMBER);
  add_field("TRX_REQUESTED_LOCK_ID");
  add_field("TRX_WAIT_STARTED", plugin::TableFunction::NUMBER);
  add_field("TRX_WEIGHT", plugin::TableFunction::NUMBER);
  add_field("TRX_DRIZZLE_THREAD_ID", plugin::TableFunction::NUMBER);
  add_field("TRX_QUERY");
}

InnodbTrxTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  /* update the cache */
  trx_i_s_cache_start_write(trx_i_s_cache);
  trx_i_s_possibly_fetch_data_into_cache(trx_i_s_cache);
  trx_i_s_cache_end_write(trx_i_s_cache);

  if (trx_i_s_cache_is_truncated(trx_i_s_cache))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Warning: data in %s truncated due to memory limit of %d bytes\n"), 
                  "INNODB_TRX", TRX_I_S_MEM_LIMIT);
  } 

  trx_i_s_cache_start_read(trx_i_s_cache);

  number_rows= trx_i_s_cache_get_rows_used(trx_i_s_cache, I_S_INNODB_TRX);

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

  char lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
  i_s_trx_row_t* row;
  char trx_id[TRX_ID_MAX_LEN + 1];
  row = (i_s_trx_row_t*) trx_i_s_cache_get_nth_row(trx_i_s_cache, I_S_INNODB_TRX, record_number);
  
  /* trx_id */
  ut_snprintf(trx_id, sizeof(trx_id), TRX_ID_FMT, row->trx_id);

  push(trx_id);
  push(row->trx_state);
  push(row->trx_started);
  
  if (row->trx_wait_started != 0) 
  {
    push(trx_i_s_create_lock_id(row->requested_lock_row, lock_id, sizeof(lock_id)));
    push(row->trx_wait_started);
  } 
  else 
  {
    push(static_cast<uint64_t>(0));
    push(static_cast<uint64_t>(0));
  }

  push(static_cast<int64_t>(row->trx_weight));
  push(row->trx_mysql_thread_id);
  push(row->trx_query);

  record_number++;

  return true;
}
