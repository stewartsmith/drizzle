
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
CmpTool::CmpTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_CMP")
{
  add_field("PAGE_SIZE", plugin::TableFunction::NUMBER);
  add_field("COMPRESS_OPS", plugin::TableFunction::NUMBER);
  add_field("COMPRESS_OPS_OK", plugin::TableFunction::NUMBER);
  add_field("COMPRESS_TIME", plugin::TableFunction::NUMBER);
  add_field("UNCOMPRESS_OPS", plugin::TableFunction::NUMBER);
  add_field("UNCOMPRESS_TIME", plugin::TableFunction::NUMBER);
}

CmpTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  record_number= 0;
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

  record_number++;

  return true;
}
