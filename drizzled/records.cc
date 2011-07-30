/* Copyright (C) 2000-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/**
  @file

  @brief
  Functions for easy reading of records, possible through a cache
*/
#include <config.h>

#include <drizzled/drizzled.h>
#include <drizzled/error.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/records.h>
#include <drizzled/session.h>
#include <drizzled/table.h>
#include <drizzled/system_variables.h>

namespace drizzled {

static int rr_sequential(ReadRecord *info);
static int rr_quick(ReadRecord *info);
static int rr_from_tempfile(ReadRecord *info);
static int rr_unpack_from_tempfile(ReadRecord *info);
static int rr_unpack_from_buffer(ReadRecord *info);
static int rr_from_pointers(ReadRecord *info);
static int rr_from_cache(ReadRecord *info);
static int rr_cmp(unsigned char *a,unsigned char *b);
static int rr_index_first(ReadRecord *info);
static int rr_index(ReadRecord *info);

void ReadRecord::init_reard_record_sequential()
{
  read_record= rr_sequential;
}

int ReadRecord::init_read_record_idx(Session *,
                                     Table *table_arg,
                                     bool print_error_arg,
                                     uint32_t idx)
{
  table_arg->emptyRecord();
  table= table_arg;
  cursor=  table->cursor;
  record= table->getInsertRecord();
  print_error= print_error_arg;

  table->status=0;			/* And it's always found */
  if (not table->cursor->inited)
  {
    int error= table->cursor->startIndexScan(idx, 1);
    if (error != 0)
      return error;
  }
  /* read_record will be changed to rr_index in rr_index_first */
  read_record= rr_index_first;

  return 0;
}


int ReadRecord::init_read_record(Session *session_arg,
                                 Table *table_arg,
                                 optimizer::SqlSelect *select_arg,
                                 int use_record_cache,
                                 bool print_error_arg)
{
  internal::io_cache_st *tempfile;
  int error= 0;

  session= session_arg;
  table= table_arg;
  cursor= table->cursor;
  forms= &table;		/* Only one table */

  if (table->sort.addon_field)
  {
    rec_buf= table->sort.addon_buf;
    ref_length= table->sort.addon_length;
  }
  else
  {
    table->emptyRecord();
    record= table->getInsertRecord();
    ref_length= table->cursor->ref_length;
  }
  select= select_arg;
  print_error= print_error_arg;
  ignore_not_found_rows= 0;
  table->status=0;			/* And it's always found */

  if (select && my_b_inited(select->file))
  {
    tempfile= select->file;
  }
  else
  {
    tempfile= table->sort.io_cache;
  }

  if (tempfile && my_b_inited(tempfile)) // Test if ref-records was used
  {
    read_record= (table->sort.addon_field ?
                  rr_unpack_from_tempfile : rr_from_tempfile);

    io_cache=tempfile;
    io_cache->reinit_io_cache(internal::READ_CACHE,0L,0,0);
    ref_pos=table->cursor->ref;
    if (!table->cursor->inited)
    {
      error= table->cursor->startTableScan(0);
      if (error != 0)
        return error;
    }

    /*
      table->sort.addon_field is checked because if we use addon fields,
      it doesn't make sense to use cache - we don't read from the table
      and table->sort.io_cache is read sequentially
    */
    if (!table->sort.addon_field &&
        session->variables.read_rnd_buff_size &&
        !(table->cursor->getEngine()->check_flag(HTON_BIT_FAST_KEY_READ)) &&
        (table->db_stat & HA_READ_ONLY ||
        table->reginfo.lock_type <= TL_READ_NO_INSERT) &&
        (uint64_t) table->getShare()->getRecordLength() * (table->cursor->stats.records+
                                                table->cursor->stats.deleted) >
        (uint64_t) MIN_FILE_LENGTH_TO_USE_ROW_CACHE &&
        io_cache->end_of_file/ref_length * table->getShare()->getRecordLength() >
        (internal::my_off_t) MIN_ROWS_TO_USE_TABLE_CACHE &&
        !table->getShare()->blob_fields &&
        ref_length <= MAX_REFLENGTH)
    {
      if (init_rr_cache())
      {
        read_record= rr_from_cache;
      }
    }
  }
  else if (select && select->quick)
  {
    read_record= rr_quick;
  }
  else if (table->sort.record_pointers)
  {
    error= table->cursor->startTableScan(0);
    if (error != 0)
      return error;

    cache_pos=table->sort.record_pointers;
    cache_end= cache_pos+ table->sort.found_records * ref_length;
    read_record= (table->sort.addon_field ?  rr_unpack_from_buffer : rr_from_pointers);
  }
  else
  {
    read_record= rr_sequential;
    error= table->cursor->startTableScan(1);
    if (error != 0)
      return error;

    /* We can use record cache if we don't update dynamic length tables */
    if (!table->no_cache &&
        (use_record_cache > 0 ||
        (int) table->reginfo.lock_type <= (int) TL_READ_WITH_SHARED_LOCKS ||
        !(table->getShare()->db_options_in_use & HA_OPTION_PACK_RECORD)))
    {
      table->cursor->extra_opt(HA_EXTRA_CACHE, session->variables.read_buff_size);
    }
  }

  return 0;
} /* init_read_record */


void ReadRecord::end_read_record()
{                   /* free cache if used */
  if (cache)
  {
    global_read_rnd_buffer.sub(session->variables.read_rnd_buff_size);
    free((char*) cache);
    cache= NULL;
  }
  if (table)
  {
    table->filesort_free_buffers();
    (void) cursor->extra(HA_EXTRA_NO_CACHE);
    if (read_record != rr_quick) // otherwise quick_range does it
      (void) cursor->ha_index_or_rnd_end();

    table= NULL;
  }
}

static int rr_handle_error(ReadRecord *info, int error)
{
  if (error == HA_ERR_END_OF_FILE)
    error= -1;
  else
  {
    if (info->print_error)
      info->table->print_error(error, MYF(0));
    if (error < 0)                            // Fix negative BDB errno
      error= 1;
  }
  return error;
}

/** Read a record from head-database. */
static int rr_quick(ReadRecord *info)
{
  int tmp;
  while ((tmp= info->select->quick->get_next()))
  {
    if (info->session->getKilled())
    {
      my_error(ER_SERVER_SHUTDOWN, MYF(0));
      return 1;
    }
    if (tmp != HA_ERR_RECORD_DELETED)
    {
      tmp= rr_handle_error(info, tmp);
      break;
    }
  }

  return tmp;
}

/**
  Reads first row in an index scan.

  @param info  	Scan info

  @retval
    0   Ok
  @retval
    -1   End of records
  @retval
    1   Error
*/
static int rr_index_first(ReadRecord *info)
{
  int tmp= info->cursor->index_first(info->record);
  info->read_record= rr_index;
  if (tmp)
    tmp= rr_handle_error(info, tmp);
  return tmp;
}

/**
  Reads index sequentially after first row.

  Read the next index record (in forward direction) and translate return
  value.

  @param info  Scan info

  @retval
    0   Ok
  @retval
    -1   End of records
  @retval
    1   Error
*/
static int rr_index(ReadRecord *info)
{
  int tmp= info->cursor->index_next(info->record);
  if (tmp)
    tmp= rr_handle_error(info, tmp);
  return tmp;
}

int rr_sequential(ReadRecord *info)
{
  int tmp;
  while ((tmp= info->cursor->rnd_next(info->record)))
  {
    if (info->session->getKilled())
    {
      info->session->send_kill_message();
      return 1;
    }
    /*
      TODO> Fix this so that engine knows how to behave on its own.
      rnd_next can return RECORD_DELETED for MyISAM when one thread is
      reading and another deleting without locks.
    */
    if (tmp != HA_ERR_RECORD_DELETED)
    {
      tmp= rr_handle_error(info, tmp);
      break;
    }
  }

  return tmp;
}

static int rr_from_tempfile(ReadRecord *info)
{
  int tmp;
  for (;;)
  {
    if (my_b_read(info->io_cache,info->ref_pos,info->ref_length))
      return -1;					/* End of cursor */
    if (!(tmp=info->cursor->rnd_pos(info->record,info->ref_pos)))
      break;
    /* The following is extremely unlikely to happen */
    if (tmp == HA_ERR_RECORD_DELETED ||
        (tmp == HA_ERR_KEY_NOT_FOUND && info->ignore_not_found_rows))
      continue;
    tmp= rr_handle_error(info, tmp);
    break;
  }
  return tmp;
} /* rr_from_tempfile */

/**
  Read a result set record from a temporary cursor after sorting.

  The function first reads the next sorted record from the temporary cursor.
  into a buffer. If a success it calls a callback function that unpacks
  the fields values use in the result set from this buffer into their
  positions in the regular record buffer.

  @param info          Reference to the context including record descriptors

  @retval
    0   Record successfully read.
  @retval
    -1   There is no record to be read anymore.
*/
static int rr_unpack_from_tempfile(ReadRecord *info)
{
  if (my_b_read(info->io_cache, info->rec_buf, info->ref_length))
    return -1;
  Table *table= info->table;
  (*table->sort.unpack)(table->sort.addon_field, info->rec_buf);

  return 0;
}

static int rr_from_pointers(ReadRecord *info)
{
  int tmp;
  unsigned char *cache_pos;


  for (;;)
  {
    if (info->cache_pos == info->cache_end)
      return -1;					/* End of cursor */
    cache_pos= info->cache_pos;
    info->cache_pos+= info->ref_length;

    if (!(tmp=info->cursor->rnd_pos(info->record,cache_pos)))
      break;

    /* The following is extremely unlikely to happen */
    if (tmp == HA_ERR_RECORD_DELETED ||
        (tmp == HA_ERR_KEY_NOT_FOUND && info->ignore_not_found_rows))
      continue;
    tmp= rr_handle_error(info, tmp);
    break;
  }
  return tmp;
}

/**
  Read a result set record from a buffer after sorting.

  The function first reads the next sorted record from the sort buffer.
  If a success it calls a callback function that unpacks
  the fields values use in the result set from this buffer into their
  positions in the regular record buffer.

  @param info          Reference to the context including record descriptors

  @retval
    0   Record successfully read.
  @retval
    -1   There is no record to be read anymore.
*/
static int rr_unpack_from_buffer(ReadRecord *info)
{
  if (info->cache_pos == info->cache_end)
    return -1;                      /* End of buffer */
  Table *table= info->table;
  (*table->sort.unpack)(table->sort.addon_field, info->cache_pos);
  info->cache_pos+= info->ref_length;

  return 0;
}

/* cacheing of records from a database */
bool ReadRecord::init_rr_cache()
{
  uint32_t local_rec_cache_size;

  struct_length= 3 + MAX_REFLENGTH;
  reclength= ALIGN_SIZE(table->getShare()->getRecordLength() + 1);
  if (reclength < struct_length)
    reclength= ALIGN_SIZE(struct_length);

  error_offset= table->getShare()->getRecordLength();
  cache_records= (session->variables.read_rnd_buff_size /
                        (reclength + struct_length));
  local_rec_cache_size= cache_records * reclength;
  rec_cache_size= cache_records * ref_length;

  if (not global_read_rnd_buffer.add(session->variables.read_rnd_buff_size))
  {
    my_error(ER_OUT_OF_GLOBAL_READRNDMEMORY, MYF(ME_ERROR+ME_WAITTANG));
    return false;
  }

  // We have to allocate one more byte to use uint3korr (see comments for it)
  if (cache_records <= 2)
    return false;
  cache= (unsigned char*) malloc(local_rec_cache_size + cache_records * struct_length + 1);
#ifdef HAVE_VALGRIND
  // Avoid warnings in qsort
  memset(cache, 0, local_rec_cache_size + cache_records * struct_length + 1);
#endif
  read_positions= cache + local_rec_cache_size;
  cache_pos= cache_end= cache;

  return true;
} /* init_rr_cache */

static int rr_from_cache(ReadRecord *info)
{
  uint32_t length;
  internal::my_off_t rest_of_file;
  int16_t error;
  unsigned char *position,*ref_position,*record_pos;
  uint32_t record;

  for (;;)
  {
    if (info->cache_pos != info->cache_end)
    {
      if (info->cache_pos[info->error_offset])
      {
        shortget(error,info->cache_pos);
        if (info->print_error)
          info->table->print_error(error,MYF(0));
      }
      else
      {
        error=0;
        memcpy(info->record,info->cache_pos, (size_t) info->table->getShare()->getRecordLength());
      }
      info->cache_pos+= info->reclength;
      return ((int) error);
    }
    length=info->rec_cache_size;
    rest_of_file= info->io_cache->end_of_file - my_b_tell(info->io_cache);
    if ((internal::my_off_t) length > rest_of_file)
    {
      length= (uint32_t) rest_of_file;
    }

    if (!length || my_b_read(info->io_cache, info->getCache(), length))
    {
      return -1;			/* End of cursor */
    }

    length/=info->ref_length;
    position=info->getCache();
    ref_position=info->read_positions;
    for (uint32_t i= 0 ; i < length ; i++,position+=info->ref_length)
    {
      memcpy(ref_position,position,(size_t) info->ref_length);
      ref_position+=MAX_REFLENGTH;
      int3store(ref_position,(long) i);
      ref_position+=3;
    }
    internal::my_qsort(info->read_positions, length, info->struct_length,
                       (qsort_cmp) rr_cmp);

    position=info->read_positions;
    for (uint32_t i= 0 ; i < length ; i++)
    {
      memcpy(info->ref_pos, position, (size_t)info->ref_length);
      position+=MAX_REFLENGTH;
      record=uint3korr(position);
      position+=3;
      record_pos= info->getCache() + record * info->reclength;
      if ((error=(int16_t) info->cursor->rnd_pos(record_pos,info->ref_pos)))
      {
        record_pos[info->error_offset]=1;
        shortstore(record_pos,error);
      }
      else
        record_pos[info->error_offset]=0;
    }
    info->cache_end= (info->cache_pos= info->getCache())+length*info->reclength;
  }
} /* rr_from_cache */

static int rr_cmp(unsigned char *a,unsigned char *b)
{
  if (a[0] != b[0])
    return (int) a[0] - (int) b[0];
  if (a[1] != b[1])
    return (int) a[1] - (int) b[1];
  if (a[2] != b[2])
    return (int) a[2] - (int) b[2];
#if MAX_REFLENGTH == 4
  return (int) a[3] - (int) b[3];
#else
  if (a[3] != b[3])
    return (int) a[3] - (int) b[3];
  if (a[4] != b[4])
    return (int) a[4] - (int) b[4];
  if (a[5] != b[5])
    return (int) a[1] - (int) b[5];
  if (a[6] != b[6])
    return (int) a[6] - (int) b[6];
  return (int) a[7] - (int) b[7];
#endif
}

} /* namespace drizzled */
