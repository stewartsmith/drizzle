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
  Sorts a database
*/

#include "config.h"

#include <float.h>
#include <limits.h>

#include <queue>
#include <algorithm>

#include "drizzled/drizzled.h"
#include "drizzled/sql_sort.h"
#include "drizzled/error.h"
#include "drizzled/probes.h"
#include "drizzled/session.h"
#include "drizzled/table.h"
#include "drizzled/table_list.h"
#include "drizzled/optimizer/range.h"
#include "drizzled/records.h"
#include "drizzled/internal/iocache.h"
#include "drizzled/internal/my_sys.h"
#include "plugin/myisam/myisam.h"
#include "drizzled/plugin/transactional_storage_engine.h"
#include "drizzled/atomics.h"
#include "drizzled/global_buffer.h"

using namespace std;

namespace drizzled
{

/* functions defined in this file */

static char **make_char_array(char **old_pos, register uint32_t fields,
                              uint32_t length);

static unsigned char *read_buffpek_from_file(internal::IO_CACHE *buffer_file,
                                             uint32_t count,
                                             unsigned char *buf);

static ha_rows find_all_keys(Session *session,
                             SORTPARAM *param,
                             optimizer::SqlSelect *select,
			     unsigned char * *sort_keys, 
                             internal::IO_CACHE *buffer_file,
			     internal::IO_CACHE *tempfile,
                             internal::IO_CACHE *indexfile);

static int write_keys(SORTPARAM *param,
                      unsigned char * *sort_keys,
		      uint32_t count,
                      internal::IO_CACHE *buffer_file,
                      internal::IO_CACHE *tempfile);

static void make_sortkey(SORTPARAM *param,
                         unsigned char *to,
                         unsigned char *ref_pos);
static void register_used_fields(SORTPARAM *param);
static int merge_index(SORTPARAM *param,
                       unsigned char *sort_buffer,
		       buffpek *buffpek,
		       uint32_t maxbuffer,
                       internal::IO_CACHE *tempfile,
		       internal::IO_CACHE *outfile);
static bool save_index(SORTPARAM *param,
                       unsigned char **sort_keys,
                       uint32_t count,
                       filesort_info *table_sort);
static uint32_t suffix_length(uint32_t string_length);
static uint32_t sortlength(Session *session,
                           SortField *sortorder,
                           uint32_t s_length,
                           bool *multi_byte_charset);
static sort_addon_field *get_addon_fields(Session *session,
                                             Field **ptabfield,
                                             uint32_t sortlength,
                                             uint32_t *plength);
static void unpack_addon_fields(sort_addon_field *addon_field,
                                unsigned char *buff);
/**
  Sort a table.
  Creates a set of pointers that can be used to read the rows
  in sorted order. This should be done with the functions
  in records.cc.

  Before calling filesort, one must have done
  table->file->info(HA_STATUS_VARIABLE)

  The result set is stored in table->io_cache or
  table->record_pointers.

  @param session           Current thread
  @param table		Table to sort
  @param sortorder	How to sort the table
  @param s_length	Number of elements in sortorder
  @param select		condition to apply to the rows
  @param max_rows	Return only this many rows
  @param sort_positions	Set to 1 if we want to force sorting by position
			(Needed by UPDATE/INSERT or ALTER Table)
  @param examined_rows	Store number of examined rows here

  @todo
    check why we do this (param.keys--)
  @note
    If we sort by position (like if sort_positions is 1) filesort() will
    call table->prepare_for_position().

  @retval
    HA_POS_ERROR	Error
  @retval
    \#			Number of rows
  @retval
    examined_rows	will be set to number of examined rows
*/

ha_rows filesort(Session *session, Table *table, SortField *sortorder, uint32_t s_length,
		 optimizer::SqlSelect *select, ha_rows max_rows,
                 bool sort_positions, ha_rows *examined_rows)
{
  int error;
  uint32_t memavl= 0, min_sort_memory;
  uint32_t maxbuffer;
  size_t allocated_sort_memory= 0;
  buffpek *buffpek_inst;
  ha_rows records= HA_POS_ERROR;
  unsigned char **sort_keys= 0;
  internal::IO_CACHE tempfile, buffpek_pointers, *selected_records_file, *outfile;
  SORTPARAM param;
  bool multi_byte_charset;

  filesort_info table_sort;
  TableList *tab= table->pos_in_table_list;
  Item_subselect *subselect= tab ? tab->containing_subselect() : 0;

  DRIZZLE_FILESORT_START(table->getShare()->getSchemaName(), table->getShare()->getTableName());

  /*
   Release InnoDB's adaptive hash index latch (if holding) before
   running a sort.
  */
  plugin::TransactionalStorageEngine::releaseTemporaryLatches(session);

  /*
    Don't use table->sort in filesort as it is also used by
    QuickIndexMergeSelect. Work with a copy and put it back at the end
    when index_merge select has finished with it.
  */
  memcpy(&table_sort, &table->sort, sizeof(filesort_info));
  table->sort.io_cache= NULL;

  outfile= table_sort.io_cache;
  my_b_clear(&tempfile);
  my_b_clear(&buffpek_pointers);
  buffpek_inst=0;
  error= 1;
  memset(&param, 0, sizeof(param));
  param.sort_length= sortlength(session, sortorder, s_length, &multi_byte_charset);
  param.ref_length= table->cursor->ref_length;
  param.addon_field= 0;
  param.addon_length= 0;
  if (!(table->cursor->getEngine()->check_flag(HTON_BIT_FAST_KEY_READ)) && !sort_positions)
  {
    /*
      Get the descriptors of all fields whose values are appended
      to sorted fields and get its total length in param.spack_length.
    */
    param.addon_field= get_addon_fields(session, table->getFields(),
                                        param.sort_length,
                                        &param.addon_length);
  }

  table_sort.addon_buf= 0;
  table_sort.addon_length= param.addon_length;
  table_sort.addon_field= param.addon_field;
  table_sort.unpack= unpack_addon_fields;
  if (param.addon_field)
  {
    param.res_length= param.addon_length;
    if (!(table_sort.addon_buf= (unsigned char *) malloc(param.addon_length)))
      goto err;
  }
  else
  {
    param.res_length= param.ref_length;
    /*
      The reference to the record is considered
      as an additional sorted field
    */
    param.sort_length+= param.ref_length;
  }
  param.rec_length= param.sort_length+param.addon_length;
  param.max_rows= max_rows;

  if (select && select->quick)
  {
    session->status_var.filesort_range_count++;
  }
  else
  {
    session->status_var.filesort_scan_count++;
  }
#ifdef CAN_TRUST_RANGE
  if (select && select->quick && select->quick->records > 0L)
  {
    records= min((ha_rows) (select->quick->records*2+EXTRA_RECORDS*2),
                 table->cursor->stats.records)+EXTRA_RECORDS;
    selected_records_file=0;
  }
  else
#endif
  {
    records= table->cursor->estimate_rows_upper_bound();
    /*
      If number of records is not known, use as much of sort buffer
      as possible.
    */
    if (records == HA_POS_ERROR)
      records--;  // we use 'records+1' below.
    selected_records_file= 0;
  }

  if (multi_byte_charset &&
      !(param.tmp_buffer= (char*) malloc(param.sort_length)))
    goto err;

  memavl= session->variables.sortbuff_size;
  min_sort_memory= max((uint32_t)MIN_SORT_MEMORY, param.sort_length*MERGEBUFF2);
  while (memavl >= min_sort_memory)
  {
    uint32_t old_memavl;
    uint32_t keys= memavl/(param.rec_length+sizeof(char*));
    param.keys= (uint32_t) min(records+1, (ha_rows)keys);
    if ((table_sort.sort_keys=
	 (unsigned char **) make_char_array((char **) table_sort.sort_keys,
                                            param.keys, param.rec_length)))
      break;
    old_memavl= memavl;
    if ((memavl= memavl/4*3) < min_sort_memory && old_memavl > min_sort_memory)
      memavl= min_sort_memory;
  }
  sort_keys= table_sort.sort_keys;
  if (memavl < min_sort_memory)
  {
    my_error(ER_OUT_OF_SORTMEMORY,MYF(ME_ERROR+ME_WAITTANG));
    goto err;
  }
  allocated_sort_memory= param.keys * param.rec_length;
  if (not global_sort_buffer.add(allocated_sort_memory))
  {
    std::cerr << "Global buffer hit!" << std::endl;
    my_error(ER_OUT_OF_SORTMEMORY, MYF(ME_ERROR+ME_WAITTANG));
    goto err;
  }

  if (open_cached_file(&buffpek_pointers,drizzle_tmpdir.c_str(),TEMP_PREFIX,
		       DISK_BUFFER_SIZE, MYF(MY_WME)))
    goto err;

  param.keys--;  			/* TODO: check why we do this */
  param.sort_form= table;
  param.end=(param.local_sortorder=sortorder)+s_length;
  if ((records=find_all_keys(session, &param,select,sort_keys, &buffpek_pointers,
			     &tempfile, selected_records_file)) ==
      HA_POS_ERROR)
    goto err;
  maxbuffer= (uint32_t) (my_b_tell(&buffpek_pointers)/sizeof(*buffpek_inst));

  if (maxbuffer == 0)			// The whole set is in memory
  {
    if (save_index(&param,sort_keys,(uint32_t) records, &table_sort))
      goto err;
  }
  else
  {
    if (table_sort.buffpek && table_sort.buffpek_len < maxbuffer)
    {
      if (table_sort.buffpek)
        free(table_sort.buffpek);
      table_sort.buffpek = 0;
    }
    if (!(table_sort.buffpek=
          (unsigned char *) read_buffpek_from_file(&buffpek_pointers, maxbuffer,
                                 table_sort.buffpek)))
      goto err;
    buffpek_inst= (buffpek *) table_sort.buffpek;
    table_sort.buffpek_len= maxbuffer;
    close_cached_file(&buffpek_pointers);
	/* Open cached file if it isn't open */
    if (! my_b_inited(outfile) &&
	open_cached_file(outfile,drizzle_tmpdir.c_str(),TEMP_PREFIX,READ_RECORD_BUFFER,
			  MYF(MY_WME)))
      goto err;
    if (reinit_io_cache(outfile,internal::WRITE_CACHE,0L,0,0))
      goto err;

    /*
      Use also the space previously used by string pointers in sort_buffer
      for temporary key storage.
    */
    param.keys=((param.keys*(param.rec_length+sizeof(char*))) /
		param.rec_length-1);
    maxbuffer--;				// Offset from 0
    if (merge_many_buff(&param,(unsigned char*) sort_keys,buffpek_inst,&maxbuffer, &tempfile))
      goto err;
    if (flush_io_cache(&tempfile) ||
	reinit_io_cache(&tempfile,internal::READ_CACHE,0L,0,0))
      goto err;
    if (merge_index(&param,(unsigned char*) sort_keys,buffpek_inst,maxbuffer,&tempfile, outfile))
      goto err;
  }
  if (records > param.max_rows)
    records=param.max_rows;
  error =0;

 err:
  if (param.tmp_buffer)
    if (param.tmp_buffer)
      free(param.tmp_buffer);
  if (!subselect || !subselect->is_uncacheable())
  {
    free(sort_keys);
    table_sort.sort_keys= 0;
    free(buffpek_inst);
    table_sort.buffpek= 0;
    table_sort.buffpek_len= 0;
  }
  close_cached_file(&tempfile);
  close_cached_file(&buffpek_pointers);
  if (my_b_inited(outfile))
  {
    if (flush_io_cache(outfile))
      error=1;
    {
      internal::my_off_t save_pos=outfile->pos_in_file;
      /* For following reads */
      if (reinit_io_cache(outfile,internal::READ_CACHE,0L,0,0))
	error=1;
      outfile->end_of_file=save_pos;
    }
  }
  if (error)
  {
    my_message(ER_FILSORT_ABORT, ER(ER_FILSORT_ABORT),
               MYF(ME_ERROR+ME_WAITTANG));
  }
  else
  {
    session->status_var.filesort_rows+= (uint32_t) records;
  }
  *examined_rows= param.examined_rows;
  global_sort_buffer.sub(allocated_sort_memory);
  memcpy(&table->sort, &table_sort, sizeof(filesort_info));
  DRIZZLE_FILESORT_DONE(error, records);
  return (error ? HA_POS_ERROR : records);
} /* filesort */


void Table::filesort_free_buffers(bool full)
{
  if (sort.record_pointers)
  {
    free((unsigned char*) sort.record_pointers);
    sort.record_pointers=0;
  }
  if (full)
  {
    if (sort.sort_keys )
    {
      if ((unsigned char*) sort.sort_keys)
        free((unsigned char*) sort.sort_keys);
      sort.sort_keys= 0;
    }
    if (sort.buffpek)
    {
      if ((unsigned char*) sort.buffpek)
        free((unsigned char*) sort.buffpek);
      sort.buffpek= 0;
      sort.buffpek_len= 0;
    }
  }
  if (sort.addon_buf)
  {
    free((char *) sort.addon_buf);
    free((char *) sort.addon_field);
    sort.addon_buf=0;
    sort.addon_field=0;
  }
}

/** Make a array of string pointers. */

static char **make_char_array(char **old_pos, register uint32_t fields,
                              uint32_t length)
{
  register char **pos;
  char *char_pos;

  if (old_pos ||
      (old_pos= (char**) malloc((uint32_t) fields*(length+sizeof(char*)))))
  {
    pos=old_pos; char_pos=((char*) (pos+fields)) -length;
    while (fields--) *(pos++) = (char_pos+= length);
  }

  return(old_pos);
} /* make_char_array */


/** Read 'count' number of buffer pointers into memory. */

static unsigned char *read_buffpek_from_file(internal::IO_CACHE *buffpek_pointers, uint32_t count,
                                     unsigned char *buf)
{
  uint32_t length= sizeof(buffpek)*count;
  unsigned char *tmp= buf;
  if (count > UINT_MAX/sizeof(buffpek))
    return 0; /* sizeof(buffpek)*count will overflow */
  if (!tmp)
    tmp= (unsigned char *)malloc(length);
  if (tmp)
  {
    if (reinit_io_cache(buffpek_pointers,internal::READ_CACHE,0L,0,0) ||
	my_b_read(buffpek_pointers, (unsigned char*) tmp, length))
    {
      free((char*) tmp);
      tmp=0;
    }
  }
  return(tmp);
}


/**
  Search after sort_keys and write them into tempfile.
  All produced sequences are guaranteed to be non-empty.

  @param param             Sorting parameter
  @param select            Use this to get source data
  @param sort_keys         Array of pointers to sort key + addon buffers.
  @param buffpek_pointers  File to write buffpeks describing sorted segments
                           in tempfile.
  @param tempfile          File to write sorted sequences of sortkeys to.
  @param indexfile         If !NULL, use it for source data (contains rowids)

  @note
    Basic idea:
    @verbatim
     while (get_next_sortkey())
     {
       if (no free space in sort_keys buffers)
       {
         sort sort_keys buffer;
         dump sorted sequence to 'tempfile';
         dump buffpek describing sequence location into 'buffpek_pointers';
       }
       put sort key into 'sort_keys';
     }
     if (sort_keys has some elements && dumped at least once)
       sort-dump-dump as above;
     else
       don't sort, leave sort_keys array to be sorted by caller.
  @endverbatim

  @retval
    Number of records written on success.
  @retval
    HA_POS_ERROR on error.
*/

static ha_rows find_all_keys(Session *session,
                             SORTPARAM *param, 
                             optimizer::SqlSelect *select,
			     unsigned char **sort_keys,
			     internal::IO_CACHE *buffpek_pointers,
			     internal::IO_CACHE *tempfile, internal::IO_CACHE *indexfile)
{
  int error,flag,quick_select;
  uint32_t idx,indexpos,ref_length;
  unsigned char *ref_pos,*next_pos,ref_buff[MAX_REFLENGTH];
  internal::my_off_t record;
  Table *sort_form;
  volatile Session::killed_state *killed= &session->killed;
  Cursor *file;
  boost::dynamic_bitset<> *save_read_set= NULL;
  boost::dynamic_bitset<> *save_write_set= NULL;

  idx=indexpos=0;
  error=quick_select=0;
  sort_form=param->sort_form;
  file= sort_form->cursor;
  ref_length=param->ref_length;
  ref_pos= ref_buff;
  quick_select=select && select->quick;
  record=0;
  flag= ((!indexfile && ! file->isOrdered())
	 || quick_select);
  if (indexfile || flag)
    ref_pos= &file->ref[0];
  next_pos=ref_pos;
  if (! indexfile && ! quick_select)
  {
    next_pos=(unsigned char*) 0;			/* Find records in sequence */
    file->startTableScan(1);
    file->extra_opt(HA_EXTRA_CACHE,
		    session->variables.read_buff_size);
  }

  ReadRecord read_record_info;
  if (quick_select)
  {
    if (select->quick->reset())
      return(HA_POS_ERROR);

    read_record_info.init_read_record(session, select->quick->head, select, 1, 1);
  }

  /* Remember original bitmaps */
  save_read_set=  sort_form->read_set;
  save_write_set= sort_form->write_set;
  /* Set up temporary column read map for columns used by sort */
  sort_form->tmp_set.reset();
  /* Temporary set for register_used_fields and register_field_in_read_map */
  sort_form->read_set= &sort_form->tmp_set;
  register_used_fields(param);
  if (select && select->cond)
    select->cond->walk(&Item::register_field_in_read_map, 1,
                       (unsigned char*) sort_form);
  sort_form->column_bitmaps_set(sort_form->tmp_set, sort_form->tmp_set);

  for (;;)
  {
    if (quick_select)
    {
      if ((error= read_record_info.read_record(&read_record_info)))
      {
        error= HA_ERR_END_OF_FILE;
        break;
      }
      file->position(sort_form->getInsertRecord());
    }
    else					/* Not quick-select */
    {
      if (indexfile)
      {
	if (my_b_read(indexfile,(unsigned char*) ref_pos,ref_length))
	{
	  error= errno ? errno : -1;		/* Abort */
	  break;
	}
	error=file->rnd_pos(sort_form->getInsertRecord(),next_pos);
      }
      else
      {
	error=file->rnd_next(sort_form->getInsertRecord());

	if (!flag)
	{
	  internal::my_store_ptr(ref_pos,ref_length,record); // Position to row
	  record+= sort_form->getShare()->db_record_offset;
	}
	else if (!error)
	  file->position(sort_form->getInsertRecord());
      }
      if (error && error != HA_ERR_RECORD_DELETED)
	break;
    }

    if (*killed)
    {
      if (!indexfile && !quick_select)
      {
        (void) file->extra(HA_EXTRA_NO_CACHE);
        file->endTableScan();
      }
      return(HA_POS_ERROR);
    }
    if (error == 0)
      param->examined_rows++;
    if (error == 0 && (!select || select->skip_record() == 0))
    {
      if (idx == param->keys)
      {
	if (write_keys(param,sort_keys,idx,buffpek_pointers,tempfile))
	  return(HA_POS_ERROR);
	idx=0;
	indexpos++;
      }
      make_sortkey(param,sort_keys[idx++],ref_pos);
    }
    else
      file->unlock_row();
    /* It does not make sense to read more keys in case of a fatal error */
    if (session->is_error())
      break;
  }
  if (quick_select)
  {
    /*
      index_merge quick select uses table->sort when retrieving rows, so free
      resoures it has allocated.
    */
    read_record_info.end_read_record();
  }
  else
  {
    (void) file->extra(HA_EXTRA_NO_CACHE);	/* End cacheing of records */
    if (!next_pos)
      file->endTableScan();
  }

  if (session->is_error())
    return(HA_POS_ERROR);

  /* Signal we should use orignal column read and write maps */
  sort_form->column_bitmaps_set(*save_read_set, *save_write_set);

  if (error != HA_ERR_END_OF_FILE)
  {
    sort_form->print_error(error,MYF(ME_ERROR | ME_WAITTANG));
    return(HA_POS_ERROR);
  }
  if (indexpos && idx &&
      write_keys(param,sort_keys,idx,buffpek_pointers,tempfile))
    return(HA_POS_ERROR);
  return(my_b_inited(tempfile) ?
	      (ha_rows) (my_b_tell(tempfile)/param->rec_length) :
	      idx);
} /* find_all_keys */


/**
  @details
  Sort the buffer and write:
  -# the sorted sequence to tempfile
  -# a buffpek describing the sorted sequence position to buffpek_pointers

    (was: Skriver en buffert med nycklar till filen)

  @param param             Sort parameters
  @param sort_keys         Array of pointers to keys to sort
  @param count             Number of elements in sort_keys array
  @param buffpek_pointers  One 'buffpek' struct will be written into this file.
                           The buffpek::{file_pos, count} will indicate where
                           the sorted data was stored.
  @param tempfile          The sorted sequence will be written into this file.

  @retval
    0 OK
  @retval
    1 Error
*/

static int
write_keys(SORTPARAM *param, register unsigned char **sort_keys, uint32_t count,
           internal::IO_CACHE *buffpek_pointers, internal::IO_CACHE *tempfile)
{
  size_t sort_length, rec_length;
  unsigned char **end;
  buffpek buffpek;

  sort_length= param->sort_length;
  rec_length= param->rec_length;
  internal::my_string_ptr_sort((unsigned char*) sort_keys, (uint32_t) count, sort_length);
  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, drizzle_tmpdir.c_str(), TEMP_PREFIX, DISK_BUFFER_SIZE,
                       MYF(MY_WME)))
    goto err;
  /* check we won't have more buffpeks than we can possibly keep in memory */
  if (my_b_tell(buffpek_pointers) + sizeof(buffpek) > (uint64_t)UINT_MAX)
    goto err;
  buffpek.file_pos= my_b_tell(tempfile);
  if ((ha_rows) count > param->max_rows)
    count=(uint32_t) param->max_rows;
  buffpek.count=(ha_rows) count;
  for (end=sort_keys+count ; sort_keys != end ; sort_keys++)
    if (my_b_write(tempfile, (unsigned char*) *sort_keys, (uint32_t) rec_length))
      goto err;
  if (my_b_write(buffpek_pointers, (unsigned char*) &buffpek, sizeof(buffpek)))
    goto err;
  return(0);

err:
  return(1);
} /* write_keys */


/**
  Store length as suffix in high-byte-first order.
*/

static inline void store_length(unsigned char *to, uint32_t length, uint32_t pack_length)
{
  switch (pack_length) {
  case 1:
    *to= (unsigned char) length;
    break;
  case 2:
    mi_int2store(to, length);
    break;
  case 3:
    mi_int3store(to, length);
    break;
  default:
    mi_int4store(to, length);
    break;
  }
}


/** Make a sort-key from record. */

static void make_sortkey(register SORTPARAM *param,
			 register unsigned char *to, unsigned char *ref_pos)
{
  Field *field;
  SortField *sort_field;
  size_t length;

  for (sort_field=param->local_sortorder ;
       sort_field != param->end ;
       sort_field++)
  {
    bool maybe_null=0;
    if ((field=sort_field->field))
    {						// Field
      if (field->maybe_null())
      {
	if (field->is_null())
	{
	  if (sort_field->reverse)
	    memset(to, 255, sort_field->length+1);
	  else
	    memset(to, 0, sort_field->length+1);
	  to+= sort_field->length+1;
	  continue;
	}
	else
	  *to++=1;
      }
      field->sort_string(to, sort_field->length);
    }
    else
    {						// Item
      Item *item=sort_field->item;
      maybe_null= item->maybe_null;
      switch (sort_field->result_type) {
      case STRING_RESULT:
      {
        const CHARSET_INFO * const cs=item->collation.collation;
        char fill_char= ((cs->state & MY_CS_BINSORT) ? (char) 0 : ' ');
        int diff;
        uint32_t sort_field_length;

        if (maybe_null)
          *to++=1;
        /* All item->str() to use some extra byte for end null.. */
        String tmp((char*) to,sort_field->length+4,cs);
        String *res= item->str_result(&tmp);
        if (!res)
        {
          if (maybe_null)
            memset(to-1, 0, sort_field->length+1);
          else
          {
            /*
              This should only happen during extreme conditions if we run out
              of memory or have an item marked not null when it can be null.
              This code is here mainly to avoid a hard crash in this case.
            */
            assert(0);
            memset(to, 0, sort_field->length);	// Avoid crash
          }
          break;
        }
        length= res->length();
        sort_field_length= sort_field->length - sort_field->suffix_length;
        diff=(int) (sort_field_length - length);
        if (diff < 0)
        {
          diff=0;
          length= sort_field_length;
        }
        if (sort_field->suffix_length)
        {
          /* Store length last in result_string */
          store_length(to + sort_field_length, length,
                       sort_field->suffix_length);
        }
        if (sort_field->need_strxnfrm)
        {
          char *from=(char*) res->ptr();
          uint32_t tmp_length;
          if ((unsigned char*) from == to)
          {
            set_if_smaller(length,sort_field->length);
            memcpy(param->tmp_buffer,from,length);
            from=param->tmp_buffer;
          }
          tmp_length= my_strnxfrm(cs,to,sort_field->length,
                                  (unsigned char*) from, length);
          assert(tmp_length == sort_field->length);
        }
        else
        {
          my_strnxfrm(cs,(unsigned char*)to,length,(const unsigned char*)res->ptr(),length);
          cs->cset->fill(cs, (char *)to+length,diff,fill_char);
        }
        break;
      }
      case INT_RESULT:
	{
          int64_t value= item->val_int_result();
          if (maybe_null)
          {
	    *to++=1;
            if (item->null_value)
            {
              if (maybe_null)
                memset(to-1, 0, sort_field->length+1);
              else
              {
                memset(to, 0, sort_field->length);
              }
              break;
            }
          }
	  to[7]= (unsigned char) value;
	  to[6]= (unsigned char) (value >> 8);
	  to[5]= (unsigned char) (value >> 16);
	  to[4]= (unsigned char) (value >> 24);
	  to[3]= (unsigned char) (value >> 32);
	  to[2]= (unsigned char) (value >> 40);
	  to[1]= (unsigned char) (value >> 48);
          if (item->unsigned_flag)                    /* Fix sign */
            to[0]= (unsigned char) (value >> 56);
          else
            to[0]= (unsigned char) (value >> 56) ^ 128;	/* Reverse signbit */
	  break;
	}
      case DECIMAL_RESULT:
        {
          my_decimal dec_buf, *dec_val= item->val_decimal_result(&dec_buf);
          if (maybe_null)
          {
            if (item->null_value)
            {
              memset(to, 0, sort_field->length+1);
              to++;
              break;
            }
            *to++=1;
          }
          my_decimal2binary(E_DEC_FATAL_ERROR, dec_val, to,
                            item->max_length - (item->decimals ? 1:0),
                            item->decimals);
         break;
        }
      case REAL_RESULT:
	{
          double value= item->val_result();
	  if (maybe_null)
          {
            if (item->null_value)
            {
              memset(to, 0, sort_field->length+1);
              to++;
              break;
            }
	    *to++=1;
          }
	  change_double_for_sort(value,(unsigned char*) to);
	  break;
	}
      case ROW_RESULT:
      default:
	// This case should never be choosen
	assert(0);
	break;
      }
    }
    if (sort_field->reverse)
    {							/* Revers key */
      if (maybe_null)
        to[-1]= ~to[-1];
      length=sort_field->length;
      while (length--)
      {
	*to = (unsigned char) (~ *to);
	to++;
      }
    }
    else
      to+= sort_field->length;
  }

  if (param->addon_field)
  {
    /*
      Save field values appended to sorted fields.
      First null bit indicators are appended then field values follow.
      In this implementation we use fixed layout for field values -
      the same for all records.
    */
    sort_addon_field *addonf= param->addon_field;
    unsigned char *nulls= to;
    assert(addonf != 0);
    memset(nulls, 0, addonf->offset);
    to+= addonf->offset;
    for ( ; (field= addonf->field) ; addonf++)
    {
      if (addonf->null_bit && field->is_null())
      {
        nulls[addonf->null_offset]|= addonf->null_bit;
#ifdef HAVE_purify
	memset(to, 0, addonf->length);
#endif
      }
      else
      {
#ifdef HAVE_purify
        unsigned char *end= field->pack(to, field->ptr);
	uint32_t length= (uint32_t) ((to + addonf->length) - end);
	assert((int) length >= 0);
	if (length)
	  memset(end, 0, length);
#else
        (void) field->pack(to, field->ptr);
#endif
      }
      to+= addonf->length;
    }
  }
  else
  {
    /* Save filepos last */
    memcpy(to, ref_pos, (size_t) param->ref_length);
  }
  return;
}


/*
  Register fields used by sorting in the sorted table's read set
*/

static void register_used_fields(SORTPARAM *param)
{
  SortField *sort_field;
  Table *table=param->sort_form;

  for (sort_field= param->local_sortorder ;
       sort_field != param->end ;
       sort_field++)
  {
    Field *field;
    if ((field= sort_field->field))
    {
      if (field->getTable() == table)
        table->setReadSet(field->field_index);
    }
    else
    {						// Item
      sort_field->item->walk(&Item::register_field_in_read_map, 1,
                             (unsigned char *) table);
    }
  }

  if (param->addon_field)
  {
    sort_addon_field *addonf= param->addon_field;
    Field *field;
    for ( ; (field= addonf->field) ; addonf++)
      table->setReadSet(field->field_index);
  }
  else
  {
    /* Save filepos last */
    table->prepare_for_position();
  }
}


static bool save_index(SORTPARAM *param, unsigned char **sort_keys, uint32_t count,
                       filesort_info *table_sort)
{
  uint32_t offset,res_length;
  unsigned char *to;

  internal::my_string_ptr_sort((unsigned char*) sort_keys, (uint32_t) count, param->sort_length);
  res_length= param->res_length;
  offset= param->rec_length-res_length;
  if ((ha_rows) count > param->max_rows)
    count=(uint32_t) param->max_rows;
  if (!(to= table_sort->record_pointers=
        (unsigned char*) malloc(res_length*count)))
    return(1);
  for (unsigned char **end= sort_keys+count ; sort_keys != end ; sort_keys++)
  {
    memcpy(to, *sort_keys+offset, res_length);
    to+= res_length;
  }
  return(0);
}


/** Merge buffers to make < MERGEBUFF2 buffers. */

int merge_many_buff(SORTPARAM *param, unsigned char *sort_buffer,
		    buffpek *buffpek_inst, uint32_t *maxbuffer, internal::IO_CACHE *t_file)
{
  register uint32_t i;
  internal::IO_CACHE t_file2,*from_file,*to_file,*temp;
  buffpek *lastbuff;

  if (*maxbuffer < MERGEBUFF2)
    return(0);
  if (flush_io_cache(t_file) ||
      open_cached_file(&t_file2,drizzle_tmpdir.c_str(),TEMP_PREFIX,DISK_BUFFER_SIZE,
			MYF(MY_WME)))
    return(1);

  from_file= t_file ; to_file= &t_file2;
  while (*maxbuffer >= MERGEBUFF2)
  {
    if (reinit_io_cache(from_file,internal::READ_CACHE,0L,0,0))
      goto cleanup;
    if (reinit_io_cache(to_file,internal::WRITE_CACHE,0L,0,0))
      goto cleanup;
    lastbuff=buffpek_inst;
    for (i=0 ; i <= *maxbuffer-MERGEBUFF*3/2 ; i+=MERGEBUFF)
    {
      if (merge_buffers(param,from_file,to_file,sort_buffer,lastbuff++,
			buffpek_inst+i,buffpek_inst+i+MERGEBUFF-1,0))
      goto cleanup;
    }
    if (merge_buffers(param,from_file,to_file,sort_buffer,lastbuff++,
		      buffpek_inst+i,buffpek_inst+ *maxbuffer,0))
      break;
    if (flush_io_cache(to_file))
      break;
    temp=from_file; from_file=to_file; to_file=temp;
    setup_io_cache(from_file);
    setup_io_cache(to_file);
    *maxbuffer= (uint32_t) (lastbuff-buffpek_inst)-1;
  }
cleanup:
  close_cached_file(to_file);			// This holds old result
  if (to_file == t_file)
  {
    *t_file=t_file2;				// Copy result file
    setup_io_cache(t_file);
  }

  return(*maxbuffer >= MERGEBUFF2);	/* Return 1 if interrupted */
} /* merge_many_buff */


/**
  Read data to buffer.

  @retval
    (uint32_t)-1 if something goes wrong
*/

uint32_t read_to_buffer(internal::IO_CACHE *fromfile, buffpek *buffpek_inst,
                        uint32_t rec_length)
{
  register uint32_t count;
  uint32_t length;

  if ((count= (uint32_t) min((ha_rows) buffpek_inst->max_keys,buffpek_inst->count)))
  {
    if (pread(fromfile->file,(unsigned char*) buffpek_inst->base, (length= rec_length*count),buffpek_inst->file_pos) == 0)
      return((uint32_t) -1);

    buffpek_inst->key= buffpek_inst->base;
    buffpek_inst->file_pos+= length;			/* New filepos */
    buffpek_inst->count-= count;
    buffpek_inst->mem_count= count;
  }
  return (count*rec_length);
} /* read_to_buffer */


class compare_functor
{
  qsort2_cmp key_compare;
  void *key_compare_arg;
  public:
  compare_functor(qsort2_cmp in_key_compare, void *in_compare_arg)
    : key_compare(in_key_compare), key_compare_arg(in_compare_arg) { }
  inline bool operator()(const buffpek *i, const buffpek *j) const
  {
    int val= key_compare(key_compare_arg,
                      &i->key, &j->key);
    return (val >= 0);
  }
};


/**
  Merge buffers to one buffer.

  @param param        Sort parameter
  @param from_file    File with source data (buffpeks point to this file)
  @param to_file      File to write the sorted result data.
  @param sort_buffer  Buffer for data to store up to MERGEBUFF2 sort keys.
  @param lastbuff     OUT Store here buffpek describing data written to to_file
  @param Fb           First element in source buffpeks array
  @param Tb           Last element in source buffpeks array
  @param flag

  @retval
    0      OK
  @retval
    other  error
*/

int merge_buffers(SORTPARAM *param, internal::IO_CACHE *from_file,
                  internal::IO_CACHE *to_file, unsigned char *sort_buffer,
                  buffpek *lastbuff, buffpek *Fb, buffpek *Tb,
                  int flag)
{
  int error;
  uint32_t rec_length,res_length,offset;
  size_t sort_length;
  uint32_t maxcount;
  ha_rows max_rows,org_max_rows;
  internal::my_off_t to_start_filepos;
  unsigned char *strpos;
  buffpek *buffpek_inst;
  qsort2_cmp cmp;
  void *first_cmp_arg;
  volatile Session::killed_state *killed= &current_session->killed;
  Session::killed_state not_killable;

  current_session->status_var.filesort_merge_passes++;
  if (param->not_killable)
  {
    killed= &not_killable;
    not_killable= Session::NOT_KILLED;
  }

  error=0;
  rec_length= param->rec_length;
  res_length= param->res_length;
  sort_length= param->sort_length;
  offset= rec_length-res_length;
  maxcount= (uint32_t) (param->keys/((uint32_t) (Tb-Fb) +1));
  to_start_filepos= my_b_tell(to_file);
  strpos= (unsigned char*) sort_buffer;
  org_max_rows=max_rows= param->max_rows;

  /* The following will fire if there is not enough space in sort_buffer */
  assert(maxcount!=0);

  if (param->unique_buff)
  {
    cmp= param->compare;
    first_cmp_arg= (void *) &param->cmp_context;
  }
  else
  {
    cmp= internal::get_ptr_compare(sort_length);
    first_cmp_arg= (void*) &sort_length;
  }
  priority_queue<buffpek *, vector<buffpek *>, compare_functor >
    queue(compare_functor(cmp, first_cmp_arg));
  for (buffpek_inst= Fb ; buffpek_inst <= Tb ; buffpek_inst++)
  {
    buffpek_inst->base= strpos;
    buffpek_inst->max_keys= maxcount;
    strpos+= (uint32_t) (error= (int) read_to_buffer(from_file, buffpek_inst,
                                                                         rec_length));
    if (error == -1)
      goto err;
    buffpek_inst->max_keys= buffpek_inst->mem_count;	// If less data in buffers than expected
    queue.push(buffpek_inst);
  }

  if (param->unique_buff)
  {
    /*
       Called by Unique::get()
       Copy the first argument to param->unique_buff for unique removal.
       Store it also in 'to_file'.

       This is safe as we know that there is always more than one element
       in each block to merge (This is guaranteed by the Unique:: algorithm
    */
    buffpek_inst= queue.top();
    memcpy(param->unique_buff, buffpek_inst->key, rec_length);
    if (my_b_write(to_file, (unsigned char*) buffpek_inst->key, rec_length))
    {
      error=1; goto err;
    }
    buffpek_inst->key+= rec_length;
    buffpek_inst->mem_count--;
    if (!--max_rows)
    {
      error= 0;
      goto end;
    }
    /* Top element has been used */
    queue.pop();
    queue.push(buffpek_inst);
  }
  else
    cmp= 0;                                        // Not unique

  while (queue.size() > 1)
  {
    if (*killed)
    {
      error= 1; goto err;
    }
    for (;;)
    {
      buffpek_inst= queue.top();
      if (cmp)                                        // Remove duplicates
      {
        if (!(*cmp)(first_cmp_arg, &(param->unique_buff),
                    (unsigned char**) &buffpek_inst->key))
              goto skip_duplicate;
            memcpy(param->unique_buff, buffpek_inst->key, rec_length);
      }
      if (flag == 0)
      {
        if (my_b_write(to_file,(unsigned char*) buffpek_inst->key, rec_length))
        {
          error=1; goto err;
        }
      }
      else
      {
        if (my_b_write(to_file, (unsigned char*) buffpek_inst->key+offset, res_length))
        {
          error=1; goto err;
        }
      }
      if (!--max_rows)
      {
        error= 0;
        goto end;
      }

    skip_duplicate:
      buffpek_inst->key+= rec_length;
      if (! --buffpek_inst->mem_count)
      {
        if (!(error= (int) read_to_buffer(from_file,buffpek_inst,
                                          rec_length)))
        {
          queue.pop();
          break;                        /* One buffer have been removed */
        }
        else if (error == -1)
          goto err;
      }
      /* Top element has been replaced */
      queue.pop();
      queue.push(buffpek_inst);
    }
  }
  buffpek_inst= queue.top();
  buffpek_inst->base= sort_buffer;
  buffpek_inst->max_keys= param->keys;

  /*
    As we know all entries in the buffer are unique, we only have to
    check if the first one is the same as the last one we wrote
  */
  if (cmp)
  {
    if (!(*cmp)(first_cmp_arg, &(param->unique_buff), (unsigned char**) &buffpek_inst->key))
    {
      buffpek_inst->key+= rec_length;         // Remove duplicate
      --buffpek_inst->mem_count;
    }
  }

  do
  {
    if ((ha_rows) buffpek_inst->mem_count > max_rows)
    {                                        /* Don't write too many records */
      buffpek_inst->mem_count= (uint32_t) max_rows;
      buffpek_inst->count= 0;                        /* Don't read more */
    }
    max_rows-= buffpek_inst->mem_count;
    if (flag == 0)
    {
      if (my_b_write(to_file,(unsigned char*) buffpek_inst->key,
                     (rec_length*buffpek_inst->mem_count)))
      {
        error= 1; goto err;
      }
    }
    else
    {
      register unsigned char *end;
      strpos= buffpek_inst->key+offset;
      for (end= strpos+buffpek_inst->mem_count*rec_length ;
           strpos != end ;
           strpos+= rec_length)
      {
        if (my_b_write(to_file, (unsigned char *) strpos, res_length))
        {
          error=1; goto err;
        }
      }
    }
  }
  while ((error=(int) read_to_buffer(from_file,buffpek_inst, rec_length))
         != -1 && error != 0);

end:
  lastbuff->count= min(org_max_rows-max_rows, param->max_rows);
  lastbuff->file_pos= to_start_filepos;
err:
  return(error);
} /* merge_buffers */


	/* Do a merge to output-file (save only positions) */

static int merge_index(SORTPARAM *param, unsigned char *sort_buffer,
		       buffpek *buffpek_inst, uint32_t maxbuffer,
		       internal::IO_CACHE *tempfile, internal::IO_CACHE *outfile)
{
  if (merge_buffers(param,tempfile,outfile,sort_buffer,buffpek_inst,buffpek_inst,
		    buffpek_inst+maxbuffer,1))
    return(1);
  return(0);
} /* merge_index */


static uint32_t suffix_length(uint32_t string_length)
{
  if (string_length < 256)
    return 1;
  if (string_length < 256L*256L)
    return 2;
  if (string_length < 256L*256L*256L)
    return 3;
  return 4;                                     // Can't sort longer than 4G
}



/**
  Calculate length of sort key.

  @param session			  Thread Cursor
  @param sortorder		  Order of items to sort
  @param s_length	          Number of items to sort
  @param[out] multi_byte_charset Set to 1 if we are using multi-byte charset
                                 (In which case we have to use strxnfrm())

  @note
    sortorder->length is updated for each sort item.
  @n
    sortorder->need_strxnfrm is set 1 if we have to use strxnfrm

  @return
    Total length of sort buffer in bytes
*/

static uint32_t
sortlength(Session *session, SortField *sortorder, uint32_t s_length,
           bool *multi_byte_charset)
{
  register uint32_t length;
  const CHARSET_INFO *cs;
  *multi_byte_charset= 0;

  length=0;
  for (; s_length-- ; sortorder++)
  {
    sortorder->need_strxnfrm= 0;
    sortorder->suffix_length= 0;
    if (sortorder->field)
    {
      cs= sortorder->field->sort_charset();
      sortorder->length= sortorder->field->sort_length();

      if (use_strnxfrm((cs=sortorder->field->sort_charset())))
      {
        sortorder->need_strxnfrm= 1;
        *multi_byte_charset= 1;
        sortorder->length= cs->coll->strnxfrmlen(cs, sortorder->length);
      }
      if (sortorder->field->maybe_null())
	length++;				// Place for NULL marker
    }
    else
    {
      sortorder->result_type= sortorder->item->result_type();
      if (sortorder->item->result_as_int64_t())
        sortorder->result_type= INT_RESULT;
      switch (sortorder->result_type) {
      case STRING_RESULT:
	sortorder->length=sortorder->item->max_length;
        set_if_smaller(sortorder->length,
                       session->variables.max_sort_length);
	if (use_strnxfrm((cs=sortorder->item->collation.collation)))
	{
          sortorder->length= cs->coll->strnxfrmlen(cs, sortorder->length);
	  sortorder->need_strxnfrm= 1;
	  *multi_byte_charset= 1;
	}
        else if (cs == &my_charset_bin)
        {
          /* Store length last to be able to sort blob/varbinary */
          sortorder->suffix_length= suffix_length(sortorder->length);
          sortorder->length+= sortorder->suffix_length;
        }
	break;
      case INT_RESULT:
	sortorder->length=8;			// Size of intern int64_t
	break;
      case DECIMAL_RESULT:
        sortorder->length=
          my_decimal_get_binary_size(sortorder->item->max_length -
                                     (sortorder->item->decimals ? 1 : 0),
                                     sortorder->item->decimals);
        break;
      case REAL_RESULT:
	sortorder->length=sizeof(double);
	break;
      case ROW_RESULT:
      default:
	// This case should never be choosen
	assert(0);
	break;
      }
      if (sortorder->item->maybe_null)
	length++;				// Place for NULL marker
    }
    set_if_smaller(sortorder->length,
                   (size_t)session->variables.max_sort_length);
    length+=sortorder->length;
  }
  sortorder->field= (Field*) 0;			// end marker
  return length;
}


/**
  Get descriptors of fields appended to sorted fields and
  calculate its total length.

  The function first finds out what fields are used in the result set.
  Then it calculates the length of the buffer to store the values of
  these fields together with the value of sort values.
  If the calculated length is not greater than max_length_for_sort_data
  the function allocates memory for an array of descriptors containing
  layouts for the values of the non-sorted fields in the buffer and
  fills them.

  @param session                 Current thread
  @param ptabfield           Array of references to the table fields
  @param sortlength          Total length of sorted fields
  @param[out] plength        Total length of appended fields

  @note
    The null bits for the appended values are supposed to be put together
    and stored the buffer just ahead of the value of the first field.

  @return
    Pointer to the layout descriptors for the appended fields, if any
  @retval
    NULL   if we do not store field values with sort data.
*/

static sort_addon_field *
get_addon_fields(Session *session, Field **ptabfield, uint32_t sortlength, uint32_t *plength)
{
  Field **pfield;
  Field *field;
  sort_addon_field *addonf;
  uint32_t length= 0;
  uint32_t fields= 0;
  uint32_t null_fields= 0;

  /*
    If there is a reference to a field in the query add it
    to the the set of appended fields.
    Note for future refinement:
    This this a too strong condition.
    Actually we need only the fields referred in the
    result set. And for some of them it makes sense to use
    the values directly from sorted fields.
  */
  *plength= 0;

  for (pfield= ptabfield; (field= *pfield) ; pfield++)
  {
    if (!(field->isReadSet()))
      continue;
    if (field->flags & BLOB_FLAG)
      return 0;
    length+= field->max_packed_col_length(field->pack_length());
    if (field->maybe_null())
      null_fields++;
    fields++;
  }
  if (!fields)
    return 0;
  length+= (null_fields+7)/8;

  if (length+sortlength > session->variables.max_length_for_sort_data ||
      !(addonf= (sort_addon_field *) malloc(sizeof(sort_addon_field)*
                                            (fields+1))))
    return 0;

  *plength= length;
  length= (null_fields+7)/8;
  null_fields= 0;
  for (pfield= ptabfield; (field= *pfield) ; pfield++)
  {
    if (!(field->isReadSet()))
      continue;
    addonf->field= field;
    addonf->offset= length;
    if (field->maybe_null())
    {
      addonf->null_offset= null_fields/8;
      addonf->null_bit= 1<<(null_fields & 7);
      null_fields++;
    }
    else
    {
      addonf->null_offset= 0;
      addonf->null_bit= 0;
    }
    addonf->length= field->max_packed_col_length(field->pack_length());
    length+= addonf->length;
    addonf++;
  }
  addonf->field= 0;     // Put end marker

  return (addonf-fields);
}


/**
  Copy (unpack) values appended to sorted fields from a buffer back to
  their regular positions specified by the Field::ptr pointers.

  @param addon_field     Array of descriptors for appended fields
  @param buff            Buffer which to unpack the value from

  @note
    The function is supposed to be used only as a callback function
    when getting field values for the sorted result set.

  @return
    void.
*/

static void
unpack_addon_fields(struct sort_addon_field *addon_field, unsigned char *buff)
{
  Field *field;
  sort_addon_field *addonf= addon_field;

  for ( ; (field= addonf->field) ; addonf++)
  {
    if (addonf->null_bit && (addonf->null_bit & buff[addonf->null_offset]))
    {
      field->set_null();
      continue;
    }
    field->set_notnull();
    field->unpack(field->ptr, buff + addonf->offset);
  }
}

/*
** functions to change a double or float to a sortable string
** The following should work for IEEE
*/

#define DBL_EXP_DIG (sizeof(double)*8-DBL_MANT_DIG)

void change_double_for_sort(double nr,unsigned char *to)
{
  unsigned char *tmp=(unsigned char*) to;
  if (nr == 0.0)
  {						/* Change to zero string */
    tmp[0]=(unsigned char) 128;
    memset(tmp+1, 0, sizeof(nr)-1);
  }
  else
  {
#ifdef WORDS_BIGENDIAN
    memcpy(tmp,&nr,sizeof(nr));
#else
    {
      unsigned char *ptr= (unsigned char*) &nr;
#if defined(__FLOAT_WORD_ORDER) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
      tmp[0]= ptr[3]; tmp[1]=ptr[2]; tmp[2]= ptr[1]; tmp[3]=ptr[0];
      tmp[4]= ptr[7]; tmp[5]=ptr[6]; tmp[6]= ptr[5]; tmp[7]=ptr[4];
#else
      tmp[0]= ptr[7]; tmp[1]=ptr[6]; tmp[2]= ptr[5]; tmp[3]=ptr[4];
      tmp[4]= ptr[3]; tmp[5]=ptr[2]; tmp[6]= ptr[1]; tmp[7]=ptr[0];
#endif
    }
#endif
    if (tmp[0] & 128)				/* Negative */
    {						/* make complement */
      uint32_t i;
      for (i=0 ; i < sizeof(nr); i++)
	tmp[i]=tmp[i] ^ (unsigned char) 255;
    }
    else
    {					/* Set high and move exponent one up */
      uint16_t exp_part=(((uint16_t) tmp[0] << 8) | (uint16_t) tmp[1] |
		       (uint16_t) 32768);
      exp_part+= (uint16_t) 1 << (16-1-DBL_EXP_DIG);
      tmp[0]= (unsigned char) (exp_part >> 8);
      tmp[1]= (unsigned char) exp_part;
    }
  }
}

} /* namespace drizzled */
