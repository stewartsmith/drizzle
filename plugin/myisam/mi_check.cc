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

/* Describe, check and repair of MyISAM tables */

/*
  About checksum calculation.

  There are two types of checksums. Table checksum and row checksum.

  Row checksum is an additional byte at the end of dynamic length
  records. It must be calculated if the table is configured for them.
  Otherwise they must not be used. The variable
  MYISAM_SHARE::calc_checksum determines if row checksums are used.
  MI_INFO::checksum is used as temporary storage during row handling.
  For parallel repair we must assure that only one thread can use this
  variable. There is no problem on the write side as this is done by one
  thread only. But when checking a record after read this could go
  wrong. But since all threads read through a common read buffer, it is
  sufficient if only one thread checks it.

  Table checksum is an eight byte value in the header of the index file.
  It can be calculated even if row checksums are not used. The variable
  MI_CHECK::glob_crc is calculated over all records.
  MI_SORT_PARAM::calc_checksum determines if this should be done. This
  variable is not part of MI_CHECK because it must be set per thread for
  parallel repair. The global glob_crc must be changed by one thread
  only. And it is sufficient to calculate the checksum once only.
*/

#include "myisam_priv.h"
#include <drizzled/internal/m_string.h>
#include <stdarg.h>
#ifdef HAVE_SYS_VADVISE_H
#include <sys/vadvise.h>
#endif
#ifdef HAVE_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <drizzled/util/test.h>
#include <drizzled/error.h>

#include <algorithm>

using namespace std;
using namespace drizzled;
using namespace drizzled::internal;


#define my_off_t2double(A)  ((double) (my_off_t) (A))

/* Functions defined in this file */

static int check_k_link(MI_CHECK *param, MI_INFO *info,uint32_t nr);
static int chk_index(MI_CHECK *param, MI_INFO *info,MI_KEYDEF *keyinfo,
		     my_off_t page, unsigned char *buff, ha_rows *keys,
		     ha_checksum *key_checksum, uint32_t level);
static uint32_t isam_key_length(MI_INFO *info,MI_KEYDEF *keyinfo);
static ha_checksum calc_checksum(ha_rows count);
static int writekeys(MI_SORT_PARAM *sort_param);
static int sort_one_index(MI_CHECK *param, MI_INFO *info,MI_KEYDEF *keyinfo,
			  my_off_t pagepos, int new_file);
int sort_key_read(MI_SORT_PARAM *sort_param,void *key);
int sort_get_next_record(MI_SORT_PARAM *sort_param);
int sort_key_cmp(MI_SORT_PARAM *sort_param, const void *a,const void *b);
int sort_key_write(MI_SORT_PARAM *sort_param, const void *a);
my_off_t get_record_for_key(MI_INFO *info,MI_KEYDEF *keyinfo,
                            unsigned char *key);
int sort_insert_key(MI_SORT_PARAM  *sort_param,
                    register SORT_KEY_BLOCKS *key_block,
                    unsigned char *key, my_off_t prev_block);
int sort_delete_record(MI_SORT_PARAM *sort_param);

/*static int flush_pending_blocks(MI_CHECK *param);*/
static SORT_KEY_BLOCKS	*alloc_key_blocks(MI_CHECK *param, uint32_t blocks,
					  uint32_t buffer_length);
static ha_checksum mi_byte_checksum(const unsigned char *buf, uint32_t length);
static void set_data_file_type(SORT_INFO *sort_info, MYISAM_SHARE *share);

void myisamchk_init(MI_CHECK *param)
{
  memset(param, 0, sizeof(*param));
  param->opt_follow_links=1;
  param->keys_in_use= ~(uint64_t) 0;
  param->search_after_block=HA_OFFSET_ERROR;
  param->auto_increment_value= 0;
  param->use_buffers=USE_BUFFER_INIT;
  param->read_buffer_length=READ_BUFFER_INIT;
  param->write_buffer_length=READ_BUFFER_INIT;
  param->sort_buffer_length=SORT_BUFFER_INIT;
  param->sort_key_blocks=BUFFERS_WHEN_SORTING;
  param->tmpfile_createflag=O_RDWR | O_TRUNC | O_EXCL;
  param->myf_rw=MYF(MY_NABP | MY_WME | MY_WAIT_IF_FULL);
  param->start_check_pos=0;
  param->max_record_length= INT64_MAX;
  param->key_cache_block_size= KEY_CACHE_BLOCK_SIZE;
  param->stats_method= MI_STATS_METHOD_NULLS_NOT_EQUAL;
}

	/* Check the status flags for the table */

int chk_status(MI_CHECK *param, register MI_INFO *info)
{
  MYISAM_SHARE *share=info->s;

  if (mi_is_crashed_on_repair(info))
    mi_check_print_warning(param,
			   "Table is marked as crashed and last repair failed");
  else if (mi_is_crashed(info))
    mi_check_print_warning(param,
			   "Table is marked as crashed");
  if (share->state.open_count != (uint) (info->s->global_changed ? 1 : 0))
  {
    /* Don't count this as a real warning, as check can correct this ! */
    uint32_t save=param->warning_printed;
    mi_check_print_warning(param,
			   share->state.open_count==1 ?
			   "%d client is using or hasn't closed the table properly" :
			   "%d clients are using or haven't closed the table properly",
			   share->state.open_count);
    /* If this will be fixed by the check, forget the warning */
    if (param->testflag & T_UPDATE_STATE)
      param->warning_printed=save;
  }
  return 0;
}

	/* Check delete links */

int chk_del(MI_CHECK *param, register MI_INFO *info, uint32_t test_flag)
{
  register ha_rows i;
  uint32_t delete_link_length;
  my_off_t empty, next_link, old_link= 0;
  char buff[22],buff2[22];

  param->record_checksum=0;
  delete_link_length=((info->s->options & HA_OPTION_PACK_RECORD) ? 20 :
		      info->s->rec_reflength+1);

  if (!(test_flag & T_SILENT))
    puts("- check record delete-chain");

  next_link=info->s->state.dellink;
  if (info->state->del == 0)
  {
    if (test_flag & T_VERBOSE)
    {
      puts("No recordlinks");
    }
  }
  else
  {
    if (test_flag & T_VERBOSE)
      printf("Recordlinks:    ");
    empty=0;
    for (i= info->state->del ; i > 0L && next_link != HA_OFFSET_ERROR ; i--)
    {
      if (*killed_ptr(param))
        return(1);
      if (test_flag & T_VERBOSE)
	printf(" %9s",llstr(next_link,buff));
      if (next_link >= info->state->data_file_length)
	goto wrong;
      if (my_pread(info->dfile, (unsigned char*) buff,delete_link_length,
		   next_link,MYF(MY_NABP)))
      {
	if (test_flag & T_VERBOSE) puts("");
	mi_check_print_error(param,"Can't read delete-link at filepos: %s",
		    llstr(next_link,buff));
	return(1);
      }
      if (*buff != '\0')
      {
	if (test_flag & T_VERBOSE) puts("");
	mi_check_print_error(param,"Record at pos: %s is not remove-marked",
		    llstr(next_link,buff));
	goto wrong;
      }
      if (info->s->options & HA_OPTION_PACK_RECORD)
      {
	my_off_t prev_link=mi_sizekorr(buff+12);
	if (empty && prev_link != old_link)
	{
	  if (test_flag & T_VERBOSE) puts("");
	  mi_check_print_error(param,"Deleted block at %s doesn't point back at previous delete link",llstr(next_link,buff2));
	  goto wrong;
	}
	old_link=next_link;
	next_link=mi_sizekorr(buff+4);
	empty+=mi_uint3korr(buff+1);
      }
      else
      {
	param->record_checksum+=(ha_checksum) next_link;
	next_link=_mi_rec_pos(info->s,(unsigned char*) buff+1);
	empty+=info->s->base.pack_reclength;
      }
    }
    if (test_flag & T_VERBOSE)
      puts("\n");
    if (empty != info->state->empty)
    {
      mi_check_print_warning(param,
			     "Found %s deleted space in delete link chain. Should be %s",
			     llstr(empty,buff2),
			     llstr(info->state->empty,buff));
    }
    if (next_link != HA_OFFSET_ERROR)
    {
      mi_check_print_error(param,
			   "Found more than the expected %s deleted rows in delete link chain",
			   llstr(info->state->del, buff));
      goto wrong;
    }
    if (i != 0)
    {
      mi_check_print_error(param,
			   "Found %s deleted rows in delete link chain. Should be %s",
			   llstr(info->state->del - i, buff2),
			   llstr(info->state->del, buff));
      goto wrong;
    }
  }
  return(0);

wrong:
  param->testflag|=T_RETRY_WITHOUT_QUICK;
  if (test_flag & T_VERBOSE) puts("");
  mi_check_print_error(param,"record delete-link-chain corrupted");
  return(1);
} /* chk_del */


	/* Check delete links in index file */

static int check_k_link(MI_CHECK *param, register MI_INFO *info, uint32_t nr)
{
  my_off_t next_link;
  uint32_t block_size=(nr+1)*MI_MIN_KEY_BLOCK_LENGTH;
  ha_rows records;
  char llbuff[21], llbuff2[21];
  unsigned char *buff;

  if (param->testflag & T_VERBOSE)
    printf("block_size %4u:", block_size);

  next_link=info->s->state.key_del[nr];
  records= (ha_rows) (info->state->key_file_length / block_size);
  while (next_link != HA_OFFSET_ERROR && records > 0)
  {
    if (*killed_ptr(param))
      return(1);
    if (param->testflag & T_VERBOSE)
      printf("%16s",llstr(next_link,llbuff));

    /* Key blocks must lay within the key file length entirely. */
    if (next_link + block_size > info->state->key_file_length)
    {
      mi_check_print_error(param, "Invalid key block position: %s  "
                           "key block size: %u  file_length: %s",
                           llstr(next_link, llbuff), block_size,
                           llstr(info->state->key_file_length, llbuff2));
      return(1);
    }

    /* Key blocks must be aligned at MI_MIN_KEY_BLOCK_LENGTH. */
    if (next_link & (MI_MIN_KEY_BLOCK_LENGTH - 1))
    {
      mi_check_print_error(param, "Mis-aligned key block: %s  "
                           "minimum key block length: %u",
                           llstr(next_link, llbuff), MI_MIN_KEY_BLOCK_LENGTH);
      return(1);
    }

    /*
      Read the key block with MI_MIN_KEY_BLOCK_LENGTH to find next link.
      If the key cache block size is smaller than block_size, we can so
      avoid unecessary eviction of cache block.
    */
    if (!(buff=key_cache_read(info->s->getKeyCache(),
                              info->s->kfile, next_link, DFLT_INIT_HITS,
                              (unsigned char*) info->buff, MI_MIN_KEY_BLOCK_LENGTH,
                              MI_MIN_KEY_BLOCK_LENGTH, 1)))
    {
      mi_check_print_error(param, "key cache read error for block: %s",
			   llstr(next_link,llbuff));
      return(1);
    }
    next_link=mi_sizekorr(buff);
    records--;
    param->key_file_blocks+=block_size;
  }
  if (param->testflag & T_VERBOSE)
  {
    if (next_link != HA_OFFSET_ERROR)
      printf("%16s\n",llstr(next_link,llbuff));
    else
      puts("");
  }
  return (next_link != HA_OFFSET_ERROR);
} /* check_k_link */


	/* Check sizes of files */

int chk_size(MI_CHECK *param, register MI_INFO *info)
{
  int error=0;
  register my_off_t skr,size;
  char buff[22],buff2[22];

  if (!(param->testflag & T_SILENT)) puts("- check file-size");

  /* The following is needed if called externally (not from myisamchk) */
  flush_key_blocks(info->s->getKeyCache(),
		   info->s->kfile, FLUSH_FORCE_WRITE);

  size= lseek(info->s->kfile, 0, SEEK_END);
  if ((skr=(my_off_t) info->state->key_file_length) != size)
  {
    /* Don't give error if file generated by myisampack */
    if (skr > size && mi_is_any_key_active(info->s->state.key_map))
    {
      error=1;
      mi_check_print_error(param,
			   "Size of indexfile is: %-8s        Should be: %s",
			   llstr(size,buff), llstr(skr,buff2));
    }
    else
      mi_check_print_warning(param,
			     "Size of indexfile is: %-8s      Should be: %s",
			     llstr(size,buff), llstr(skr,buff2));
  }
  if (!(param->testflag & T_VERY_SILENT) &&
      ! (info->s->options & HA_OPTION_COMPRESS_RECORD) &&
      uint64_t2double(info->state->key_file_length) >
      uint64_t2double(info->s->base.margin_key_file_length)*0.9)
    mi_check_print_warning(param,"Keyfile is almost full, %10s of %10s used",
			   llstr(info->state->key_file_length,buff),
			   llstr(info->s->base.max_key_file_length-1,buff));

  size=lseek(info->dfile,0L,SEEK_END);
  skr=(my_off_t) info->state->data_file_length;
  if (info->s->options & HA_OPTION_COMPRESS_RECORD)
    skr+= MEMMAP_EXTRA_MARGIN;
#ifdef USE_RELOC
  if (info->data_file_type == STATIC_RECORD &&
      skr < (my_off_t) info->s->base.reloc*info->s->base.min_pack_length)
    skr=(my_off_t) info->s->base.reloc*info->s->base.min_pack_length;
#endif
  if (skr != size)
  {
    info->state->data_file_length=size;	/* Skip other errors */
    if (skr > size && skr != size + MEMMAP_EXTRA_MARGIN)
    {
      error=1;
      mi_check_print_error(param,"Size of datafile is: %-9s         Should be: %s",
		    llstr(size,buff), llstr(skr,buff2));
      param->testflag|=T_RETRY_WITHOUT_QUICK;
    }
    else
    {
      mi_check_print_warning(param,
			     "Size of datafile is: %-9s       Should be: %s",
			     llstr(size,buff), llstr(skr,buff2));
    }
  }
  if (!(param->testflag & T_VERY_SILENT) &&
      !(info->s->options & HA_OPTION_COMPRESS_RECORD) &&
      uint64_t2double(info->state->data_file_length) >
      (uint64_t2double(info->s->base.max_data_file_length)*0.9))
    mi_check_print_warning(param, "Datafile is almost full, %10s of %10s used",
			   llstr(info->state->data_file_length,buff),
			   llstr(info->s->base.max_data_file_length-1,buff2));
  return(error);
} /* chk_size */


	/* Check keys */

int chk_key(MI_CHECK *param, register MI_INFO *info)
{
  uint32_t key,found_keys=0,full_text_keys=0,result=0;
  ha_rows keys;
  ha_checksum old_record_checksum,init_checksum;
  my_off_t all_keydata,all_totaldata,key_totlength,length;
  ulong   *rec_per_key_part;
  MYISAM_SHARE *share=info->s;
  MI_KEYDEF *keyinfo;
  char buff[22],buff2[22];

  if (!(param->testflag & T_SILENT))
    puts("- check key delete-chain");

  param->key_file_blocks=info->s->base.keystart;
  for (key=0 ; key < info->s->state.header.max_block_size_index ; key++)
    if (check_k_link(param,info,key))
    {
      if (param->testflag & T_VERBOSE) puts("");
      mi_check_print_error(param,"key delete-link-chain corrupted");
      return(-1);
    }

  if (!(param->testflag & T_SILENT)) puts("- check index reference");

  all_keydata=all_totaldata=key_totlength=0;
  old_record_checksum=0;
  init_checksum=param->record_checksum;
  if (!(share->options &
	(HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)))
    old_record_checksum=calc_checksum(info->state->records+info->state->del-1)*
      share->base.pack_reclength;
  rec_per_key_part= param->rec_per_key_part;
  for (key= 0,keyinfo= &share->keyinfo[0]; key < share->base.keys ;
       rec_per_key_part+=keyinfo->keysegs, key++, keyinfo++)
  {
    param->key_crc[key]=0;
    if (! mi_is_key_active(share->state.key_map, key))
    {
      /* Remember old statistics for key */
      assert(rec_per_key_part >= param->rec_per_key_part);
      memcpy(rec_per_key_part,
	     (share->state.rec_per_key_part +
              (rec_per_key_part - param->rec_per_key_part)),
	     keyinfo->keysegs*sizeof(*rec_per_key_part));
      continue;
    }
    found_keys++;

    param->record_checksum=init_checksum;

    memset(&param->unique_count, 0, sizeof(param->unique_count));
    memset(&param->notnull_count, 0, sizeof(param->notnull_count));

    if ((!(param->testflag & T_SILENT)))
      printf ("- check data record references index: %d\n",key+1);
    if (share->state.key_root[key] == HA_OFFSET_ERROR && (info->state->records == 0))
      goto do_stat;
    if (!_mi_fetch_keypage(info,keyinfo,share->state.key_root[key],
                           DFLT_INIT_HITS,info->buff,0))
    {
      mi_check_print_error(param,"Can't read indexpage from filepos: %s",
		  llstr(share->state.key_root[key],buff));
      if (!(param->testflag & T_INFO))
	return(-1);
      result= UINT32_MAX;
      continue;
    }
    param->key_file_blocks+=keyinfo->block_length;
    keys=0;
    param->keydata=param->totaldata=0;
    param->key_blocks=0;
    param->max_level=0;
    if (chk_index(param,info,keyinfo,share->state.key_root[key],info->buff,
		  &keys, param->key_crc+key,1))
      return(-1);
    if(!(0))
    {
      if (keys != info->state->records)
      {
	mi_check_print_error(param,"Found %s keys of %s",llstr(keys,buff),
		    llstr(info->state->records,buff2));
	if (!(param->testflag & T_INFO))
	return(-1);
	result= UINT32_MAX;
	continue;
      }
      if (found_keys - full_text_keys == 1 &&
	  ((share->options &
	    (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ||
	   (param->testflag & T_DONT_CHECK_CHECKSUM)))
	old_record_checksum=param->record_checksum;
      else if (old_record_checksum != param->record_checksum)
      {
	if (key)
	  mi_check_print_error(param,"Key %u doesn't point at same records that key 1",
		      key+1);
	else
	  mi_check_print_error(param,"Key 1 doesn't point at all records");
	if (!(param->testflag & T_INFO))
	  return(-1);
	result= UINT32_MAX;
	continue;
      }
    }
    if ((uint) share->base.auto_key -1 == key)
    {
      /* Check that auto_increment key is bigger than max key value */
      uint64_t auto_increment;
      info->lastinx=key;
      _mi_read_key_record(info, 0L, info->rec_buff);
      auto_increment= retrieve_auto_increment(info, info->rec_buff);
      if (auto_increment > info->s->state.auto_increment)
      {
        mi_check_print_warning(param, "Auto-increment value: %s is smaller "
                               "than max used value: %s",
                               llstr(info->s->state.auto_increment,buff2),
                               llstr(auto_increment, buff));
      }
      if (param->testflag & T_AUTO_INC)
      {
        set_if_bigger(info->s->state.auto_increment,
                      auto_increment);
        set_if_bigger(info->s->state.auto_increment,
                      param->auto_increment_value);
      }

      /* Check that there isn't a row with auto_increment = 0 in the table */
      mi_extra(info,HA_EXTRA_KEYREAD,0);
      memset(info->lastkey, 0, keyinfo->seg->length);
      if (!mi_rkey(info, info->rec_buff, key, (const unsigned char*) info->lastkey,
		   (key_part_map)1, HA_READ_KEY_EXACT))
      {
	/* Don't count this as a real warning, as myisamchk can't correct it */
	uint32_t save=param->warning_printed;
        mi_check_print_warning(param, "Found row where the auto_increment "
                               "column has the value 0");
	param->warning_printed=save;
      }
      mi_extra(info,HA_EXTRA_NO_KEYREAD,0);
    }

    length=(my_off_t) isam_key_length(info,keyinfo)*keys + param->key_blocks*2;
    if (param->testflag & T_INFO && param->totaldata != 0L && keys != 0L)
      printf("Key: %2d:  Keyblocks used: %3d%%  Packed: %4d%%  Max levels: %2d\n",
	     key+1,
	     (int) (my_off_t2double(param->keydata)*100.0/my_off_t2double(param->totaldata)),
	     (int) ((my_off_t2double(length) - my_off_t2double(param->keydata))*100.0/
		    my_off_t2double(length)),
	     param->max_level);
    all_keydata+=param->keydata; all_totaldata+=param->totaldata; key_totlength+=length;

do_stat:
    if (param->testflag & T_STATISTICS)
      update_key_parts(keyinfo, rec_per_key_part, param->unique_count,
                       param->stats_method == MI_STATS_METHOD_IGNORE_NULLS?
                       param->notnull_count: NULL,
                       (uint64_t)info->state->records);
  }
  if (param->testflag & T_INFO)
  {
    if (all_totaldata != 0L && found_keys > 0)
      printf("Total:    Keyblocks used: %3d%%  Packed: %4d%%\n\n",
	     (int) (my_off_t2double(all_keydata)*100.0/
		    my_off_t2double(all_totaldata)),
	     (int) ((my_off_t2double(key_totlength) -
		     my_off_t2double(all_keydata))*100.0/
		     my_off_t2double(key_totlength)));
    else if (all_totaldata != 0L && mi_is_any_key_active(share->state.key_map))
      puts("");
  }
  if (param->key_file_blocks != info->state->key_file_length &&
      param->keys_in_use != ~(uint64_t) 0)
    mi_check_print_warning(param, "Some data are unreferenced in keyfile");
  if (found_keys != full_text_keys)
    param->record_checksum=old_record_checksum-init_checksum;	/* Remove delete links */
  else
    param->record_checksum=0;
  return(result);
} /* chk_key */


static int chk_index_down(MI_CHECK *param, MI_INFO *info, MI_KEYDEF *keyinfo,
                     my_off_t page, unsigned char *buff, ha_rows *keys,
                     ha_checksum *key_checksum, uint32_t level)
{
  char llbuff[22],llbuff2[22];

  /* Key blocks must lay within the key file length entirely. */
  if (page + keyinfo->block_length > info->state->key_file_length)
  {
    /* Give it a chance to fit in the real file size. */
    my_off_t max_length= lseek(info->s->kfile, 0, SEEK_END);
    mi_check_print_error(param, "Invalid key block position: %s  "
                         "key block size: %u  file_length: %s",
                         llstr(page, llbuff), keyinfo->block_length,
                         llstr(info->state->key_file_length, llbuff2));
    if (page + keyinfo->block_length > max_length)
      goto err;
    /* Fix the remebered key file length. */
    info->state->key_file_length= (max_length &
                                   ~ (my_off_t) (keyinfo->block_length - 1));
  }

  /* Key blocks must be aligned at MI_MIN_KEY_BLOCK_LENGTH. */
  if (page & (MI_MIN_KEY_BLOCK_LENGTH - 1))
  {
    mi_check_print_error(param, "Mis-aligned key block: %s  "
                         "minimum key block length: %u",
                         llstr(page, llbuff), MI_MIN_KEY_BLOCK_LENGTH);
    goto err;
  }

  if (!_mi_fetch_keypage(info,keyinfo,page, DFLT_INIT_HITS,buff,0))
  {
    mi_check_print_error(param,"Can't read key from filepos: %s",
        llstr(page,llbuff));
    goto err;
  }
  param->key_file_blocks+=keyinfo->block_length;
  if (chk_index(param,info,keyinfo,page,buff,keys,key_checksum,level))
    goto err;

  return(0);

err:
  return(1);
}


/*
  "Ignore NULLs" statistics collection method: process first index tuple.

  SYNOPSIS
    mi_collect_stats_nonulls_first()
      keyseg   IN     Array of key part descriptions
      notnull  INOUT  Array, notnull[i] = (number of {keypart1...keypart_i}
                                           tuples that don't contain NULLs)
      key      IN     Key values tuple

  DESCRIPTION
    Process the first index tuple - find out which prefix tuples don't
    contain NULLs, and update the array of notnull counters accordingly.
*/

static
void mi_collect_stats_nonulls_first(HA_KEYSEG *keyseg, uint64_t *notnull,
                                    unsigned char *key)
{
  uint32_t first_null, kp;
  first_null= ha_find_null(keyseg, key) - keyseg;
  /*
    All prefix tuples that don't include keypart_{first_null} are not-null
    tuples (and all others aren't), increment counters for them.
  */
  for (kp= 0; kp < first_null; kp++)
    notnull[kp]++;
}


/*
  "Ignore NULLs" statistics collection method: process next index tuple.

  SYNOPSIS
    mi_collect_stats_nonulls_next()
      keyseg   IN     Array of key part descriptions
      notnull  INOUT  Array, notnull[i] = (number of {keypart1...keypart_i}
                                           tuples that don't contain NULLs)
      prev_key IN     Previous key values tuple
      last_key IN     Next key values tuple

  DESCRIPTION
    Process the next index tuple:
    1. Find out which prefix tuples of last_key don't contain NULLs, and
       update the array of notnull counters accordingly.
    2. Find the first keypart number where the prev_key and last_key tuples
       are different(A), or last_key has NULL value(B), and return it, so the
       caller can count number of unique tuples for each key prefix. We don't
       need (B) to be counted, and that is compensated back in
       update_key_parts().

  RETURN
    1 + number of first keypart where values differ or last_key tuple has NULL
*/

static
int mi_collect_stats_nonulls_next(HA_KEYSEG *keyseg, uint64_t *notnull,
                                  unsigned char *prev_key, unsigned char *last_key)
{
  uint32_t diffs[2];
  uint32_t first_null_seg, kp;
  HA_KEYSEG *seg;

  /*
     Find the first keypart where values are different or either of them is
     NULL. We get results in diffs array:
     diffs[0]= 1 + number of first different keypart
     diffs[1]=offset: (last_key + diffs[1]) points to first value in
                      last_key that is NULL or different from corresponding
                      value in prev_key.
  */
  ha_key_cmp(keyseg, prev_key, last_key, USE_WHOLE_KEY,
             SEARCH_FIND | SEARCH_NULL_ARE_NOT_EQUAL, diffs);
  seg= keyseg + diffs[0] - 1;

  /* Find first NULL in last_key */
  first_null_seg= ha_find_null(seg, last_key + diffs[1]) - keyseg;
  for (kp= 0; kp < first_null_seg; kp++)
    notnull[kp]++;

  /*
    Return 1+ number of first key part where values differ. Don't care if
    these were NULLs and not .... We compensate for that in
    update_key_parts.
  */
  return diffs[0];
}


	/* Check if index is ok */

static int chk_index(MI_CHECK *param, MI_INFO *info, MI_KEYDEF *keyinfo,
		     my_off_t page, unsigned char *buff, ha_rows *keys,
		     ha_checksum *key_checksum, uint32_t level)
{
  int flag;
  uint32_t used_length,comp_flag,nod_flag,key_length=0;
  unsigned char key[HA_MAX_POSSIBLE_KEY_BUFF],*temp_buff,*keypos,*endpos;
  my_off_t next_page,record;
  char llbuff[22];
  uint32_t diff_pos[2];

  if (!(temp_buff=(unsigned char*) malloc(keyinfo->block_length)))
  {
    mi_check_print_error(param,"Not enough memory for keyblock");
    return(-1);
  }

  if (keyinfo->flag & HA_NOSAME)
    comp_flag=SEARCH_FIND | SEARCH_UPDATE;	/* Not real duplicates */
  else
    comp_flag=SEARCH_SAME;			/* Keys in positionorder */
  nod_flag=mi_test_if_nod(buff);
  used_length=mi_getint(buff);
  keypos=buff+2+nod_flag;
  endpos=buff+used_length;

  param->keydata+=used_length; param->totaldata+=keyinfo->block_length;	/* INFO */
  param->key_blocks++;
  if (level > param->max_level)
    param->max_level=level;

  if (used_length > keyinfo->block_length)
  {
    mi_check_print_error(param,"Wrong pageinfo at page: %s",
			 llstr(page,llbuff));
    goto err;
  }
  for ( ;; )
  {
    if (*killed_ptr(param))
      goto err;
    memcpy(info->lastkey,key,key_length);
    info->lastkey_length=key_length;
    if (nod_flag)
    {
      next_page=_mi_kpos(nod_flag,keypos);
      if (chk_index_down(param,info,keyinfo,next_page,
                         temp_buff,keys,key_checksum,level+1))
	goto err;
    }
    if (keypos >= endpos ||
	(key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,key)) == 0)
      break;
    assert(key_length <= sizeof(key));
    if (keypos > endpos)
    {
      mi_check_print_error(param,"Wrong key block length at page: %s",llstr(page,llbuff));
      goto err;
    }
    if ((*keys)++ &&
	(flag=ha_key_cmp(keyinfo->seg,info->lastkey,key,key_length,
			 comp_flag, diff_pos)) >=0)
    {
      if (comp_flag & SEARCH_FIND && flag == 0)
	mi_check_print_error(param,"Found duplicated key at page %s",llstr(page,llbuff));
      else
	mi_check_print_error(param,"Key in wrong position at page %s",llstr(page,llbuff));
      goto err;
    }
    if (param->testflag & T_STATISTICS)
    {
      if (*keys != 1L)				/* not first_key */
      {
        if (param->stats_method == MI_STATS_METHOD_NULLS_NOT_EQUAL)
          ha_key_cmp(keyinfo->seg,info->lastkey,key,USE_WHOLE_KEY,
                     SEARCH_FIND | SEARCH_NULL_ARE_NOT_EQUAL,
                     diff_pos);
        else if (param->stats_method == MI_STATS_METHOD_IGNORE_NULLS)
        {
          diff_pos[0]= mi_collect_stats_nonulls_next(keyinfo->seg,
                                                  param->notnull_count,
                                                  info->lastkey, key);
        }
	param->unique_count[diff_pos[0]-1]++;
      }
      else
      {
        if (param->stats_method == MI_STATS_METHOD_IGNORE_NULLS)
          mi_collect_stats_nonulls_first(keyinfo->seg, param->notnull_count,
                                         key);
      }
    }
    (*key_checksum)+= mi_byte_checksum((unsigned char*) key,
				       key_length- info->s->rec_reflength);
    record= _mi_dpos(info,0,key+key_length);
    if (record >= info->state->data_file_length)
    {
      mi_check_print_error(param,"Found key at page %s that points to record outside datafile",llstr(page,llbuff));
      goto err;
    }
    param->record_checksum+=(ha_checksum) record;
  }
  if (keypos != endpos)
  {
    mi_check_print_error(param,"Keyblock size at page %s is not correct.  Block length: %d  key length: %d",
                llstr(page,llbuff), used_length, (keypos - buff));
    goto err;
  }
  free(temp_buff);
  return(0);
 err:
  free(temp_buff);
  return(1);
} /* chk_index */


	/* Calculate a checksum of 1+2+3+4...N = N*(N+1)/2 without overflow */

static ha_checksum calc_checksum(ha_rows count)
{
  uint64_t sum,a,b;

  sum=0;
  a=count; b=count+1;
  if (a & 1)
    b>>=1;
  else
    a>>=1;
  while (b)
  {
    if (b & 1)
      sum+=a;
    a<<=1; b>>=1;
  }
  return((ha_checksum) sum);
} /* calc_checksum */


	/* Calc length of key in normal isam */

static uint32_t isam_key_length(MI_INFO *info, register MI_KEYDEF *keyinfo)
{
  uint32_t length;
  HA_KEYSEG *keyseg;

  length= info->s->rec_reflength;
  for (keyseg=keyinfo->seg ; keyseg->type ; keyseg++)
    length+= keyseg->length;

  return(length);
} /* key_length */


	/* Check that record-link is ok */

int chk_data_link(MI_CHECK *param, MI_INFO *info,int extend)
{
  int	error,got_error,flag;
  uint	key, left_length= 0, b_type;
  ha_rows records, del_blocks;
  my_off_t used, empty, pos, splits, start_recpos= 0,
           del_length, link_used, start_block;
  unsigned char	*record= NULL, *to= NULL;
  char llbuff[22],llbuff2[22],llbuff3[22];
  ha_checksum intern_record_checksum;
  ha_checksum key_checksum[HA_MAX_POSSIBLE_KEY];
  MI_KEYDEF *keyinfo;
  MI_BLOCK_INFO block_info;

  if (!(param->testflag & T_SILENT))
  {
    if (extend)
      puts("- check records and index references");
    else
      puts("- check record links");
  }

  if (!mi_alloc_rec_buff(info, SIZE_MAX, &record))
  {
    mi_check_print_error(param,"Not enough memory for record");
    return(-1);
  }
  records=del_blocks=0;
  used=link_used=splits=del_length=0;
  intern_record_checksum=param->glob_crc=0;
  got_error=error=0;
  empty=info->s->pack.header_length;

  pos=my_b_tell(&param->read_cache);
  memset(key_checksum, 0, info->s->base.keys * sizeof(key_checksum[0]));
  while (pos < info->state->data_file_length)
  {
    if (*killed_ptr(param))
      goto err2;
    switch (info->s->data_file_type) {
    case STATIC_RECORD:
      if (my_b_read(&param->read_cache,(unsigned char*) record,
		    info->s->base.pack_reclength))
	goto err;
      start_recpos=pos;
      pos+=info->s->base.pack_reclength;
      splits++;
      if (*record == '\0')
      {
	del_blocks++;
	del_length+=info->s->base.pack_reclength;
	continue;					/* Record removed */
      }
      param->glob_crc+= mi_static_checksum(info,record);
      used+=info->s->base.pack_reclength;
      break;
    case DYNAMIC_RECORD:
      flag=block_info.second_read=0;
      block_info.next_filepos=pos;
      do
      {
	if (_mi_read_cache(&param->read_cache,(unsigned char*) block_info.header,
			   (start_block=block_info.next_filepos),
			   sizeof(block_info.header),
			   (flag ? 0 : READING_NEXT) | READING_HEADER))
	  goto err;
	if (start_block & (MI_DYN_ALIGN_SIZE-1))
	{
	  mi_check_print_error(param,"Wrong aligned block at %s",
			       llstr(start_block,llbuff));
	  goto err2;
	}
	b_type=_mi_get_block_info(&block_info,-1,start_block);
	if (b_type & (BLOCK_DELETED | BLOCK_ERROR | BLOCK_SYNC_ERROR |
		      BLOCK_FATAL_ERROR))
	{
	  if (b_type & BLOCK_SYNC_ERROR)
	  {
	    if (flag)
	    {
	      mi_check_print_error(param,"Unexpected byte: %d at link: %s",
			  (int) block_info.header[0],
			  llstr(start_block,llbuff));
	      goto err2;
	    }
	    pos=block_info.filepos+block_info.block_len;
	    goto next;
	  }
	  if (b_type & BLOCK_DELETED)
	  {
	    if (block_info.block_len < info->s->base.min_block_length)
	    {
	      mi_check_print_error(param,
				   "Deleted block with impossible length %lu at %s",
				   block_info.block_len,llstr(pos,llbuff));
	      goto err2;
	    }
	    if ((block_info.next_filepos != HA_OFFSET_ERROR &&
		 block_info.next_filepos >= info->state->data_file_length) ||
		(block_info.prev_filepos != HA_OFFSET_ERROR &&
		 block_info.prev_filepos >= info->state->data_file_length))
	    {
	      mi_check_print_error(param,"Delete link points outside datafile at %s",
			  llstr(pos,llbuff));
	      goto err2;
	    }
	    del_blocks++;
	    del_length+=block_info.block_len;
	    pos=block_info.filepos+block_info.block_len;
	    splits++;
	    goto next;
	  }
	  mi_check_print_error(param,"Wrong bytesec: %d-%d-%d at linkstart: %s",
			       block_info.header[0],block_info.header[1],
			       block_info.header[2],
			       llstr(start_block,llbuff));
	  goto err2;
	}
	if (info->state->data_file_length < block_info.filepos+
	    block_info.block_len)
	{
	  mi_check_print_error(param,
			       "Recordlink that points outside datafile at %s",
			       llstr(pos,llbuff));
	  got_error=1;
	  break;
	}
	splits++;
	if (!flag++)				/* First block */
	{
	  start_recpos=pos;
	  pos=block_info.filepos+block_info.block_len;
	  if (block_info.rec_len > (uint) info->s->base.max_pack_length)
	  {
	    mi_check_print_error(param,"Found too long record (%lu) at %s",
				 (ulong) block_info.rec_len,
				 llstr(start_recpos,llbuff));
	    got_error=1;
	    break;
	  }
	  if (info->s->base.blobs)
	  {
	    if (!(to= mi_alloc_rec_buff(info, block_info.rec_len,
					&info->rec_buff)))
	    {
	      mi_check_print_error(param,
				   "Not enough memory (%lu) for blob at %s",
				   (ulong) block_info.rec_len,
				   llstr(start_recpos,llbuff));
	      got_error=1;
	      break;
	    }
	  }
	  else
	    to= info->rec_buff;
	  left_length=block_info.rec_len;
	}
	if (left_length < block_info.data_len)
	{
	  mi_check_print_error(param,"Found too long record (%lu) at %s",
			       (ulong) block_info.data_len,
			       llstr(start_recpos,llbuff));
	  got_error=1;
	  break;
	}
	if (_mi_read_cache(&param->read_cache,(unsigned char*) to,block_info.filepos,
			   (uint) block_info.data_len,
			   flag == 1 ? READING_NEXT : 0))
	  goto err;
	to+=block_info.data_len;
	link_used+= block_info.filepos-start_block;
	used+= block_info.filepos - start_block + block_info.data_len;
	empty+=block_info.block_len-block_info.data_len;
	left_length-=block_info.data_len;
	if (left_length)
	{
	  if (b_type & BLOCK_LAST)
	  {
	    mi_check_print_error(param,
				 "Wrong record length %s of %s at %s",
				 llstr(block_info.rec_len-left_length,llbuff),
				 llstr(block_info.rec_len, llbuff2),
				 llstr(start_recpos,llbuff3));
	    got_error=1;
	    break;
	  }
	  if (info->state->data_file_length < block_info.next_filepos)
	  {
	    mi_check_print_error(param,
				 "Found next-recordlink that points outside datafile at %s",
				 llstr(block_info.filepos,llbuff));
	    got_error=1;
	    break;
	  }
	}
      } while (left_length);
      if (! got_error)
      {
	if (_mi_rec_unpack(info,record,info->rec_buff,block_info.rec_len) ==
	    MY_FILE_ERROR)
	{
	  mi_check_print_error(param,"Found wrong record at %s",
			       llstr(start_recpos,llbuff));
	  got_error=1;
	}
	else
	{
	  info->checksum=mi_checksum(info,record);
	  if (param->testflag & (T_EXTEND | T_MEDIUM | T_VERBOSE))
	  {
	    if (_mi_rec_check(info,record, info->rec_buff,block_info.rec_len,
                              test(info->s->calc_checksum)))
	    {
	      mi_check_print_error(param,"Found wrong packed record at %s",
			  llstr(start_recpos,llbuff));
	      got_error=1;
	    }
	  }
	  if (!got_error)
	    param->glob_crc+= info->checksum;
	}
      }
      else if (!flag)
	pos=block_info.filepos+block_info.block_len;
      break;
    case COMPRESSED_RECORD:
    case BLOCK_RECORD:
      assert(0);                                /* Impossible */
    } /* switch */
    if (! got_error)
    {
      intern_record_checksum+=(ha_checksum) start_recpos;
      records++;
      if (param->testflag & T_WRITE_LOOP && records % WRITE_COUNT == 0)
      {
	printf("%s\r", llstr(records,llbuff)); fflush(stdout);
      }

      /* Check if keys match the record */

      for (key=0,keyinfo= info->s->keyinfo; key < info->s->base.keys;
	   key++,keyinfo++)
      {
        if (mi_is_key_active(info->s->state.key_map, key))
	{
	  {
	    uint32_t key_length=_mi_make_key(info,key,info->lastkey,record,
					 start_recpos);
	    if (extend)
	    {
	      /* We don't need to lock the key tree here as we don't allow
		 concurrent threads when running myisamchk
	      */
              int search_result=
                _mi_search(info,keyinfo,info->lastkey,key_length,
                           SEARCH_SAME, info->s->state.key_root[key]);
              if (search_result)
              {
                mi_check_print_error(param,"Record at: %10s  "
                                     "Can't find key for index: %2d",
                                     llstr(start_recpos,llbuff),key+1);
                if (error++ > MAXERR || !(param->testflag & T_VERBOSE))
                  goto err2;
              }
	    }
	    else
	      key_checksum[key]+=mi_byte_checksum((unsigned char*) info->lastkey,
						  key_length);
	  }
	}
      }
    }
    else
    {
      got_error=0;
      if (error++ > MAXERR || !(param->testflag & T_VERBOSE))
	goto err2;
    }
  next:;				/* Next record */
  }
  if (param->testflag & T_WRITE_LOOP)
  {
    fputs("          \r",stdout); fflush(stdout);
  }
  if (records != info->state->records)
  {
    mi_check_print_error(param,"Record-count is not ok; is %-10s   Should be: %s",
		llstr(records,llbuff), llstr(info->state->records,llbuff2));
    error=1;
  }
  else if (param->record_checksum &&
	   param->record_checksum != intern_record_checksum)
  {
    mi_check_print_error(param,
			 "Keypointers and record positions doesn't match");
    error=1;
  }
  else if (param->glob_crc != info->state->checksum &&
	   (info->s->options &
	    (HA_OPTION_COMPRESS_RECORD)))
  {
    mi_check_print_warning(param,
			   "Record checksum is not the same as checksum stored in the index file\n");
    error=1;
  }
  else if (!extend)
  {
    for (key=0 ; key < info->s->base.keys;  key++)
    {
      if (key_checksum[key] != param->key_crc[key])
      {
	mi_check_print_error(param,"Checksum for key: %2d doesn't match checksum for records",
		    key+1);
	error=1;
      }
    }
  }

  if (del_length != info->state->empty)
  {
    mi_check_print_warning(param,
			   "Found %s deleted space.   Should be %s",
			   llstr(del_length,llbuff2),
			   llstr(info->state->empty,llbuff));
  }
  if (used+empty+del_length != info->state->data_file_length)
  {
    mi_check_print_warning(param,
			   "Found %s record-data and %s unused data and %s deleted-data",
			   llstr(used,llbuff),llstr(empty,llbuff2),
			   llstr(del_length,llbuff3));
    mi_check_print_warning(param,
			   "Total %s, Should be: %s",
			   llstr((used+empty+del_length),llbuff),
			   llstr(info->state->data_file_length,llbuff2));
  }
  if (del_blocks != info->state->del)
  {
    mi_check_print_warning(param,
			   "Found %10s deleted blocks       Should be: %s",
			   llstr(del_blocks,llbuff),
			   llstr(info->state->del,llbuff2));
  }
  if (splits != info->s->state.split)
  {
    mi_check_print_warning(param,
			   "Found %10s parts                Should be: %s parts",
			   llstr(splits,llbuff),
			   llstr(info->s->state.split,llbuff2));
  }
  if (param->testflag & T_INFO)
  {
    if (param->warning_printed || param->error_printed)
      puts("");
    if (used != 0 && ! param->error_printed)
    {
      printf("Records:%18s    M.recordlength:%9lu   Packed:%14.0f%%\n",
	     llstr(records,llbuff), (long)((used-link_used)/records),
	     (info->s->base.blobs ? 0.0 :
	      (uint64_t2double((uint64_t) info->s->base.reclength*records)-
	       my_off_t2double(used))/
	      uint64_t2double((uint64_t) info->s->base.reclength*records)*100.0));
      printf("Recordspace used:%9.0f%%   Empty space:%12d%%  Blocks/Record: %6.2f\n",
	     (uint64_t2double(used-link_used)/uint64_t2double(used-link_used+empty)*100.0),
	     (!records ? 100 : (int) (uint64_t2double(del_length+empty)/
				      my_off_t2double(used)*100.0)),
	     uint64_t2double(splits - del_blocks) / records);
    }
    printf("Record blocks:%12s    Delete blocks:%10s\n",
	   llstr(splits-del_blocks,llbuff),llstr(del_blocks,llbuff2));
    printf("Record data:  %12s    Deleted data: %10s\n",
	   llstr(used-link_used,llbuff),llstr(del_length,llbuff2));
    printf("Lost space:   %12s    Linkdata:     %10s\n",
	   llstr(empty,llbuff),llstr(link_used,llbuff2));
  }
  free(mi_get_rec_buff_ptr(info, record));
  return (error);
 err:
  mi_check_print_error(param,"got error: %d when reading datafile at record: %s",errno, llstr(records,llbuff));
 err2:
  free(mi_get_rec_buff_ptr(info, record));
  param->testflag|=T_RETRY_WITHOUT_QUICK;
  return(1);
} /* chk_data_link */


/**
  @brief Drop all indexes

  @param[in]    param           check parameters
  @param[in]    info            MI_INFO handle
  @param[in]    force           if to force drop all indexes

  @return       status
    @retval     0               OK
    @retval     != 0            Error

  @note
    Once allocated, index blocks remain part of the key file forever.
    When indexes are disabled, no block is freed. When enabling indexes,
    no block is freed either. The new indexes are create from new
    blocks. (Bug #4692)

    Before recreating formerly disabled indexes, the unused blocks
    must be freed. There are two options to do this:
    - Follow the tree of disabled indexes, add all blocks to the
      deleted blocks chain. Would require a lot of random I/O.
    - Drop all blocks by clearing all index root pointers and all
      delete chain pointers and resetting key_file_length to the end
      of the index file header. This requires to recreate all indexes,
      even those that may still be intact.
    The second method is probably faster in most cases.

    When disabling indexes, MySQL disables either all indexes or all
    non-unique indexes. When MySQL [re-]enables disabled indexes
    (T_CREATE_MISSING_KEYS), then we either have "lost" blocks in the
    index file, or there are no non-unique indexes. In the latter case,
    mi_repair*() would not be called as there would be no disabled
    indexes.

    If there would be more unique indexes than disabled (non-unique)
    indexes, we could do the first method. But this is not implemented
    yet. By now we drop and recreate all indexes when repair is called.

    However, there is an exception. Sometimes MySQL disables non-unique
    indexes when the table is empty (e.g. when copying a table in
    drizzled::alter_table()). When enabling the non-unique indexes, they
    are still empty. So there is no index block that can be lost. This
    optimization is implemented in this function.

    Note that in normal repair (T_CREATE_MISSING_KEYS not set) we
    recreate all enabled indexes unconditonally. We do not change the
    key_map. Otherwise we invert the key map temporarily (outside of
    this function) and recreate the then "seemingly" enabled indexes.
    When we cannot use the optimization, and drop all indexes, we
    pretend that all indexes were disabled. By the inversion, we will
    then recrate all indexes.
*/

static int mi_drop_all_indexes(MI_CHECK *param, MI_INFO *info, bool force)
{
  MYISAM_SHARE *share= info->s;
  MI_STATE_INFO *state= &share->state;
  uint32_t i;
  int error;

  /*
    If any of the disabled indexes has a key block assigned, we must
    drop and recreate all indexes to avoid losing index blocks.

    If we want to recreate disabled indexes only _and_ all of these
    indexes are empty, we don't need to recreate the existing indexes.
  */
  if (!force && (param->testflag & T_CREATE_MISSING_KEYS))
  {
    for (i= 0; i < share->base.keys; i++)
    {
      if ((state->key_root[i] != HA_OFFSET_ERROR) &&
          !mi_is_key_active(state->key_map, i))
      {
        /*
          This index has at least one key block and it is disabled.
          We would lose its block(s) if would just recreate it.
          So we need to drop and recreate all indexes.
        */
        break;
      }
    }
    if (i >= share->base.keys)
    {
      /*
        All of the disabled indexes are empty. We can just recreate them.
        Flush dirty blocks of this index file from key cache and remove
        all blocks of this index file from key cache.
      */
      error= flush_key_blocks(share->getKeyCache(), share->kfile,
                              FLUSH_FORCE_WRITE);
      goto end;
    }
    /*
      We do now drop all indexes and declare them disabled. With the
      T_CREATE_MISSING_KEYS flag, mi_repair*() will recreate all
      disabled indexes and enable them.
    */
    mi_clear_all_keys_active(state->key_map);
  }

  /* Remove all key blocks of this index file from key cache. */
  if ((error= flush_key_blocks(share->getKeyCache(), share->kfile,
                               FLUSH_IGNORE_CHANGED)))
    goto end;

  /* Clear index root block pointers. */
  for (i= 0; i < share->base.keys; i++)
    state->key_root[i]= HA_OFFSET_ERROR;

  /* Clear the delete chains. */
  for (i= 0; i < state->header.max_block_size_index; i++)
    state->key_del[i]= HA_OFFSET_ERROR;

  /* Reset index file length to end of index file header. */
  info->state->key_file_length= share->base.keystart;

  /* error= 0; set by last (error= flush_key_bocks()). */

 end:
  return(error);
}


	/* Recover old table by reading each record and writing all keys */
	/* Save new datafile-name in temp_filename */

int mi_repair(MI_CHECK *param, register MI_INFO *info,
	      char * name, int rep_quick)
{
  int error,got_error;
  ha_rows start_records,new_header_length;
  my_off_t del;
  int new_file;
  MYISAM_SHARE *share=info->s;
  char llbuff[22],llbuff2[22];
  SORT_INFO sort_info;
  MI_SORT_PARAM sort_param;

  memset(&sort_info, 0, sizeof(sort_info));
  memset(&sort_param, 0, sizeof(sort_param));
  start_records=info->state->records;
  new_header_length=(param->testflag & T_UNPACK) ? 0L :
    share->pack.header_length;
  got_error=1;
  new_file= -1;
  sort_param.sort_info=&sort_info;

  if (!(param->testflag & T_SILENT))
  {
    printf("- recovering (with keycache) MyISAM-table '%s'\n",name);
    printf("Data records: %s\n", llstr(info->state->records,llbuff));
  }
  param->testflag|=T_REP; /* for easy checking */

  if (info->s->options & (HA_OPTION_COMPRESS_RECORD))
    param->testflag|=T_CALC_CHECKSUM;

  if (!param->using_global_keycache)
    assert(0);

  if (param->read_cache.init_io_cache(info->dfile, (uint) param->read_buffer_length, READ_CACHE,share->pack.header_length,1,MYF(MY_WME)))
  {
    memset(&info->rec_cache, 0, sizeof(info->rec_cache));
    goto err;
  }
  if (not rep_quick)
  {
    if (info->rec_cache.init_io_cache(-1, (uint) param->write_buffer_length, WRITE_CACHE, new_header_length, 1, MYF(MY_WME | MY_WAIT_IF_FULL)))
    {
      goto err;
    }
  }
  info->opt_flag|=WRITE_CACHE_USED;
  if (!mi_alloc_rec_buff(info, SIZE_MAX, &sort_param.record) ||
      !mi_alloc_rec_buff(info, SIZE_MAX, &sort_param.rec_buff))
  {
    mi_check_print_error(param, "Not enough memory for extra record");
    goto err;
  }

  if (!rep_quick)
  {
    /* Get real path for data file */
    if ((new_file=my_create(internal::fn_format(param->temp_filename,
                                      share->data_file_name, "",
                                      DATA_TMP_EXT, 2+4),
                            0,param->tmpfile_createflag,
                            MYF(0))) < 0)
    {
      mi_check_print_error(param,"Can't create new tempfile: '%s'",
                           param->temp_filename);
      goto err;
    }
    if (new_header_length &&
        filecopy(param,new_file,info->dfile,0L,new_header_length,
                 "datafile-header"))
      goto err;
    info->s->state.dellink= HA_OFFSET_ERROR;
    info->rec_cache.file=new_file;
    if (param->testflag & T_UNPACK)
    {
      share->options&= ~HA_OPTION_COMPRESS_RECORD;
      mi_int2store(share->state.header.options,share->options);
    }
  }
  sort_info.info=info;
  sort_info.param = param;
  sort_param.read_cache=param->read_cache;
  sort_param.pos=sort_param.max_pos=share->pack.header_length;
  sort_param.filepos=new_header_length;
  param->read_cache.end_of_file=sort_info.filelength=
    lseek(info->dfile,0L,SEEK_END);
  sort_info.dupp=0;
  sort_param.fix_datafile= (bool) (! rep_quick);
  sort_param.master=1;
  sort_info.max_records= ~(ha_rows) 0;

  set_data_file_type(&sort_info, share);
  del=info->state->del;
  info->state->records=info->state->del=share->state.split=0;
  info->state->empty=0;
  param->glob_crc=0;
  if (param->testflag & T_CALC_CHECKSUM)
    sort_param.calc_checksum= 1;

  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  /* This function always recreates all enabled indexes. */
  if (param->testflag & T_CREATE_MISSING_KEYS)
    mi_set_all_keys_active(share->state.key_map, share->base.keys);
  mi_drop_all_indexes(param, info, true);

  lock_memory(param);			/* Everything is alloced */

  /* Re-create all keys, which are set in key_map. */
  while (!(error=sort_get_next_record(&sort_param)))
  {
    if (writekeys(&sort_param))
    {
      if (errno != HA_ERR_FOUND_DUPP_KEY)
	goto err;
      mi_check_print_info(param,"Duplicate key %2d for record at %10s against new record at %10s",
			  info->errkey+1,
			  llstr(sort_param.start_recpos,llbuff),
			  llstr(info->dupp_key_pos,llbuff2));
      if (param->testflag & T_VERBOSE)
      {
	_mi_make_key(info,(uint) info->errkey,info->lastkey,
                     sort_param.record,0L);
      }
      sort_info.dupp++;
      if ((param->testflag & (T_FORCE_UNIQUENESS|T_QUICK)) == T_QUICK)
      {
        param->testflag|=T_RETRY_WITHOUT_QUICK;
	param->error_printed=1;
	goto err;
      }
      continue;
    }
    if (sort_write_record(&sort_param))
      goto err;
  }
  if (error > 0 || write_data_suffix(&sort_info, (bool)!rep_quick) ||
      flush_io_cache(&info->rec_cache) || param->read_cache.error < 0)
    goto err;

  if (param->testflag & T_WRITE_LOOP)
  {
    fputs("          \r",stdout); fflush(stdout);
  }
  if (ftruncate(share->kfile, info->state->key_file_length))
  {
    mi_check_print_warning(param,
			   "Can't change size of indexfile, error: %d",
			   errno);
    goto err;
  }

  if (rep_quick && del+sort_info.dupp != info->state->del)
  {
    mi_check_print_error(param,"Couldn't fix table with quick recovery: Found wrong number of deleted records");
    mi_check_print_error(param,"Run recovery again without -q");
    got_error=1;
    param->retry_repair=1;
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    goto err;
  }
  if (param->testflag & T_SAFE_REPAIR)
  {
    /* Don't repair if we loosed more than one row */
    if (info->state->records+1 < start_records)
    {
      info->state->records=start_records;
      got_error=1;
      goto err;
    }
  }

  if (!rep_quick)
  {
    internal::my_close(info->dfile,MYF(0));
    info->dfile=new_file;
    info->state->data_file_length=sort_param.filepos;
    share->state.version=(ulong) time((time_t*) 0);	/* Force reopen */
  }
  else
  {
    info->state->data_file_length=sort_param.max_pos;
  }
  if (param->testflag & T_CALC_CHECKSUM)
    info->state->checksum=param->glob_crc;

  if (!(param->testflag & T_SILENT))
  {
    if (start_records != info->state->records)
      printf("Data records: %s\n", llstr(info->state->records,llbuff));
    if (sort_info.dupp)
      mi_check_print_warning(param,
			     "%s records have been removed",
			     llstr(sort_info.dupp,llbuff));
  }

  got_error=0;
  /* If invoked by external program that uses thr_lock */
  if (&share->state.state != info->state)
    memcpy( &share->state.state, info->state, sizeof(*info->state));

err:
  if (!got_error)
  {
    /* Replace the actual file with the temporary file */
    if (new_file >= 0)
    {
      internal::my_close(new_file,MYF(0));
      info->dfile=new_file= -1;
      if (change_to_newfile(share->data_file_name,MI_NAME_DEXT,
			    DATA_TMP_EXT, share->base.raid_chunks,
			    MYF(0)) ||
	  mi_open_datafile(info,share,-1))
	got_error=1;
    }
  }
  if (got_error)
  {
    if (! param->error_printed)
      mi_check_print_error(param,"%d for record at pos %s",errno,
		  llstr(sort_param.start_recpos,llbuff));
    if (new_file >= 0)
    {
      internal::my_close(new_file,MYF(0));
      my_delete(param->temp_filename, MYF(MY_WME));
      info->rec_cache.file=-1; /* don't flush data to new_file, it's closed */
    }
    mi_mark_crashed_on_repair(info);
  }

  void * rec_buff_ptr= NULL;
  rec_buff_ptr= mi_get_rec_buff_ptr(info, sort_param.rec_buff);
  free(rec_buff_ptr);
  rec_buff_ptr= mi_get_rec_buff_ptr(info, sort_param.record);
  free(rec_buff_ptr);
  rec_buff_ptr= NULL;

  free(sort_info.buff);
  param->read_cache.end_io_cache();
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  info->rec_cache.end_io_cache();
  got_error|= flush_blocks(param, share->getKeyCache(), share->kfile);
  if (not got_error && param->testflag & T_UNPACK)
  {
    share->state.header.options[0]&= (unsigned char) ~HA_OPTION_COMPRESS_RECORD;
    share->pack.header_length=0;
    share->data_file_type=sort_info.new_data_file_type;
  }
  share->state.changed|= (STATE_NOT_OPTIMIZED_KEYS | STATE_NOT_SORTED_PAGES |
			  STATE_NOT_ANALYZED);
  return(got_error);
}


/* Uppate keyfile when doing repair */

static int writekeys(MI_SORT_PARAM *sort_param)
{
  register uint32_t i;
  unsigned char    *key;
  MI_INFO  *info=   sort_param->sort_info->info;
  unsigned char    *buff=   sort_param->record;
  my_off_t filepos= sort_param->filepos;

  key=info->lastkey+info->s->base.max_key_length;
  for (i=0 ; i < info->s->base.keys ; i++)
  {
    if (mi_is_key_active(info->s->state.key_map, i))
    {
      {
	uint32_t key_length=_mi_make_key(info,i,key,buff,filepos);
	if (_mi_ck_write(info,i,key,key_length))
	  goto err;
      }
    }
  }
  return(0);

 err:
  if (errno == HA_ERR_FOUND_DUPP_KEY)
  {
    info->errkey=(int) i;			/* This key was found */
    while ( i-- > 0 )
    {
      if (mi_is_key_active(info->s->state.key_map, i))
      {
	{
	  uint32_t key_length=_mi_make_key(info,i,key,buff,filepos);
	  if (_mi_ck_delete(info,i,key,key_length))
	    break;
	}
      }
    }
  }
  /* Remove checksum that was added to glob_crc in sort_get_next_record */
  if (sort_param->calc_checksum)
    sort_param->sort_info->param->glob_crc-= info->checksum;
  return(-1);
} /* writekeys */


	/* Change all key-pointers that points to a records */

int movepoint(register MI_INFO *info, unsigned char *record, my_off_t oldpos,
	      my_off_t newpos, uint32_t prot_key)
{
  register uint32_t i;
  unsigned char *key;
  uint32_t key_length;

  key=info->lastkey+info->s->base.max_key_length;
  for (i=0 ; i < info->s->base.keys; i++)
  {
    if (i != prot_key && mi_is_key_active(info->s->state.key_map, i))
    {
      key_length=_mi_make_key(info,i,key,record,oldpos);
      if (info->s->keyinfo[i].flag & HA_NOSAME)
      {					/* Change pointer direct */
	uint32_t nod_flag;
	MI_KEYDEF *keyinfo;
	keyinfo=info->s->keyinfo+i;
	if (_mi_search(info,keyinfo,key,USE_WHOLE_KEY,
		       (uint) (SEARCH_SAME | SEARCH_SAVE_BUFF),
		       info->s->state.key_root[i]))
	  return(-1);
	nod_flag=mi_test_if_nod(info->buff);
	_mi_dpointer(info,info->int_keypos-nod_flag-
		     info->s->rec_reflength,newpos);
	if (_mi_write_keypage(info,keyinfo,info->last_keypage,
                              DFLT_INIT_HITS,info->buff))
	  return(-1);
      }
      else
      {					/* Change old key to new */
	if (_mi_ck_delete(info,i,key,key_length))
	  return(-1);
	key_length=_mi_make_key(info,i,key,record,newpos);
	if (_mi_ck_write(info,i,key,key_length))
	  return(-1);
      }
    }
  }
  return(0);
} /* movepoint */


	/* Tell system that we want all memory for our cache */

void lock_memory(MI_CHECK *)
{
} /* lock_memory */


	/* Flush all changed blocks to disk */

int flush_blocks(MI_CHECK *param, KEY_CACHE *key_cache, int file)
{
  if (flush_key_blocks(key_cache, file, FLUSH_RELEASE))
  {
    mi_check_print_error(param,"%d when trying to write bufferts",errno);
    return(1);
  }
  if (!param->using_global_keycache)
    end_key_cache(key_cache,1);
  return 0;
} /* flush_blocks */


	/* Sort index for more efficent reads */

int mi_sort_index(MI_CHECK *param, register MI_INFO *info, char * name)
{
  register uint32_t key;
  register MI_KEYDEF *keyinfo;
  int new_file;
  my_off_t index_pos[HA_MAX_POSSIBLE_KEY];
  uint32_t r_locks,w_locks;
  int old_lock;
  MYISAM_SHARE *share=info->s;
  MI_STATE_INFO old_state;

  /* cannot sort index files with R-tree indexes */
  for (key= 0,keyinfo= &share->keyinfo[0]; key < share->base.keys ;
       key++,keyinfo++)

  if (!(param->testflag & T_SILENT))
    printf("- Sorting index for MyISAM-table '%s'\n",name);

  /* Get real path for index file */
  internal::fn_format(param->temp_filename,name,"", MI_NAME_IEXT,2+4+32);
  if ((new_file=my_create(internal::fn_format(param->temp_filename,param->temp_filename,
				    "", INDEX_TMP_EXT,2+4),
			  0,param->tmpfile_createflag,MYF(0))) <= 0)
  {
    mi_check_print_error(param,"Can't create new tempfile: '%s'",
			 param->temp_filename);
    return(-1);
  }
  if (filecopy(param, new_file,share->kfile,0L,
	       (ulong) share->base.keystart, "headerblock"))
    goto err;

  param->new_file_pos=share->base.keystart;
  for (key= 0,keyinfo= &share->keyinfo[0]; key < share->base.keys ;
       key++,keyinfo++)
  {
    if (! mi_is_key_active(info->s->state.key_map, key))
      continue;

    if (share->state.key_root[key] != HA_OFFSET_ERROR)
    {
      index_pos[key]=param->new_file_pos;	/* Write first block here */
      if (sort_one_index(param,info,keyinfo,share->state.key_root[key],
			 new_file))
	goto err;
    }
    else
      index_pos[key]= HA_OFFSET_ERROR;		/* No blocks */
  }

  /* Flush key cache for this file if we are calling this outside myisamchk */
  flush_key_blocks(share->getKeyCache(), share->kfile, FLUSH_IGNORE_CHANGED);

  share->state.version=(ulong) time((time_t*) 0);
  old_state= share->state;			/* save state if not stored */
  r_locks=   share->r_locks;
  w_locks=   share->w_locks;
  old_lock=  info->lock_type;

	/* Put same locks as old file */
  share->r_locks= share->w_locks= share->tot_locks= 0;
  (void) _mi_writeinfo(info,WRITEINFO_UPDATE_KEYFILE);
  internal::my_close(share->kfile,MYF(MY_WME));
  share->kfile = -1;
  internal::my_close(new_file,MYF(MY_WME));
  if (change_to_newfile(share->index_file_name,MI_NAME_IEXT,INDEX_TMP_EXT,0,
			MYF(0)) ||
      mi_open_keyfile(share))
    goto err2;
  info->lock_type= F_UNLCK;			/* Force mi_readinfo to lock */
  _mi_readinfo(info,F_WRLCK,0);			/* Will lock the table */
  info->lock_type=  old_lock;
  share->r_locks=   r_locks;
  share->w_locks=   w_locks;
  share->tot_locks= r_locks+w_locks;
  share->state=     old_state;			/* Restore old state */

  info->state->key_file_length=param->new_file_pos;
  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  for (key=0 ; key < info->s->base.keys ; key++)
    info->s->state.key_root[key]=index_pos[key];
  for (key=0 ; key < info->s->state.header.max_block_size_index ; key++)
    info->s->state.key_del[key]=  HA_OFFSET_ERROR;

  info->s->state.changed&= ~STATE_NOT_SORTED_PAGES;
  return(0);

err:
  internal::my_close(new_file,MYF(MY_WME));
err2:
  my_delete(param->temp_filename,MYF(MY_WME));
  return(-1);
} /* mi_sort_index */


	 /* Sort records recursive using one index */

static int sort_one_index(MI_CHECK *param, MI_INFO *info, MI_KEYDEF *keyinfo,
			  my_off_t pagepos, int new_file)
{
  uint32_t length,nod_flag,used_length, key_length;
  unsigned char *buff,*keypos,*endpos;
  unsigned char key[HA_MAX_POSSIBLE_KEY_BUFF];
  my_off_t new_page_pos,next_page;
  char llbuff[22];

  new_page_pos=param->new_file_pos;
  param->new_file_pos+=keyinfo->block_length;

  if (!(buff=(unsigned char*) malloc(keyinfo->block_length)))
  {
    mi_check_print_error(param,"Not enough memory for key block");
    return(-1);
  }
  if (!_mi_fetch_keypage(info,keyinfo,pagepos,DFLT_INIT_HITS,buff,0))
  {
    mi_check_print_error(param,"Can't read key block from filepos: %s",
		llstr(pagepos,llbuff));
    goto err;
  }
  if ((nod_flag=mi_test_if_nod(buff)))
  {
    used_length=mi_getint(buff);
    keypos=buff+2+nod_flag;
    endpos=buff+used_length;
    for ( ;; )
    {
      if (nod_flag)
      {
	next_page=_mi_kpos(nod_flag,keypos);
	_mi_kpointer(info,keypos-nod_flag,param->new_file_pos); /* Save new pos */
	if (sort_one_index(param,info,keyinfo,next_page,new_file))
	{
	  goto err;
	}
      }
      if (keypos >= endpos ||
	  (key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,key)) == 0)
	break;
      assert(keypos <= endpos);
    }
  }

  /* Fill block with zero and write it to the new index file */
  length=mi_getint(buff);
  memset(buff+length, 0, keyinfo->block_length-length);
  if (my_pwrite(new_file,(unsigned char*) buff,(uint) keyinfo->block_length,
		new_page_pos,MYF(MY_NABP | MY_WAIT_IF_FULL)))
  {
    mi_check_print_error(param,"Can't write indexblock, error: %d",errno);
    goto err;
  }
  free(buff);
  return(0);
err:
  free(buff);
  return(1);
} /* sort_one_index */


	/*
	  Let temporary file replace old file.
	  This assumes that the new file was created in the same
	  directory as given by realpath(filename).
	  This will ensure that any symlinks that are used will still work.
	  Copy stats from old file to new file, deletes orignal and
	  changes new file name to old file name
	*/

int change_to_newfile(const char * filename, const char * old_ext,
		      const char * new_ext,
		      uint32_t raid_chunks,
		      myf MyFlags)
{
  (void)raid_chunks;
  char old_filename[FN_REFLEN],new_filename[FN_REFLEN];
  /* Get real path to filename */
  (void) internal::fn_format(old_filename,filename,"",old_ext,2+4+32);
  return my_redel(old_filename,
		  internal::fn_format(new_filename,old_filename,"",new_ext,2+4),
		  MYF(MY_WME | MY_LINK_WARNING | MyFlags));
} /* change_to_newfile */



	/* Copy a block between two files */

int filecopy(MI_CHECK *param, int to,int from,my_off_t start,
	     my_off_t length, const char *type)
{
  char tmp_buff[IO_SIZE],*buff;
  ulong buff_length;

  buff_length=(ulong) min(param->write_buffer_length, (size_t)length);
  if (!(buff=(char *)malloc(buff_length)))
  {
    buff=tmp_buff; buff_length=IO_SIZE;
  }

  lseek(from,start,SEEK_SET);
  while (length > buff_length)
  {
    if (my_read(from,(unsigned char*) buff,buff_length,MYF(MY_NABP)) ||
	my_write(to,(unsigned char*) buff,buff_length,param->myf_rw))
      goto err;
    length-= buff_length;
  }
  if (my_read(from,(unsigned char*) buff,(uint) length,MYF(MY_NABP)) ||
      my_write(to,(unsigned char*) buff,(uint) length,param->myf_rw))
    goto err;
  if (buff != tmp_buff)
    free(buff);
  return(0);
err:
  if (buff != tmp_buff)
    free(buff);
  mi_check_print_error(param,"Can't copy %s to tempfile, error %d",
		       type,errno);
  return(1);
}


/*
  Repair table or given index using sorting

  SYNOPSIS
    mi_repair_by_sort()
    param		Repair parameters
    info		MyISAM handler to repair
    name		Name of table (for warnings)
    rep_quick		set to <> 0 if we should not change data file

  RESULT
    0	ok
    <>0	Error
*/

int mi_repair_by_sort(MI_CHECK *param, register MI_INFO *info,
		      const char * name, int rep_quick)
{
  int got_error;
  uint32_t i;
  ulong length;
  ha_rows start_records;
  my_off_t new_header_length,del;
  int new_file;
  MI_SORT_PARAM sort_param;
  MYISAM_SHARE *share=info->s;
  HA_KEYSEG *keyseg;
  ulong   *rec_per_key_part;
  char llbuff[22];
  SORT_INFO sort_info;
  uint64_t key_map= 0;

  start_records=info->state->records;
  got_error=1;
  new_file= -1;
  new_header_length=(param->testflag & T_UNPACK) ? 0 :
    share->pack.header_length;
  if (!(param->testflag & T_SILENT))
  {
    printf("- recovering (with sort) MyISAM-table '%s'\n",name);
    printf("Data records: %s\n", llstr(start_records,llbuff));
  }
  param->testflag|=T_REP; /* for easy checking */

  if (info->s->options & (HA_OPTION_COMPRESS_RECORD))
    param->testflag|=T_CALC_CHECKSUM;

  memset(&sort_info, 0, sizeof(sort_info));
  memset(&sort_param, 0, sizeof(sort_param));
  if (!(sort_info.key_block=
        alloc_key_blocks(param, (uint) param->sort_key_blocks, share->base.max_key_block_length))
      || param->read_cache.init_io_cache(info->dfile, (uint) param->read_buffer_length, READ_CACHE,share->pack.header_length,1,MYF(MY_WME))
      || (! rep_quick && info->rec_cache.init_io_cache(info->dfile, (uint) param->write_buffer_length, WRITE_CACHE,new_header_length,1, MYF(MY_WME | MY_WAIT_IF_FULL) & param->myf_rw)))
  {
    goto err;
  }
  sort_info.key_block_end=sort_info.key_block+param->sort_key_blocks;
  info->opt_flag|=WRITE_CACHE_USED;
  info->rec_cache.file=info->dfile;		/* for sort_delete_record */

  if (!mi_alloc_rec_buff(info, SIZE_MAX, &sort_param.record) ||
      !mi_alloc_rec_buff(info, SIZE_MAX, &sort_param.rec_buff))
  {
    mi_check_print_error(param, "Not enough memory for extra record");
    goto err;
  }
  if (!rep_quick)
  {
    /* Get real path for data file */
    if ((new_file=my_create(internal::fn_format(param->temp_filename,
                                      share->data_file_name, "",
                                      DATA_TMP_EXT, 2+4),
                            0,param->tmpfile_createflag,
                            MYF(0))) < 0)
    {
      mi_check_print_error(param,"Can't create new tempfile: '%s'",
                           param->temp_filename);
      goto err;
    }
    if (new_header_length &&
        filecopy(param, new_file,info->dfile,0L,new_header_length,
		 "datafile-header"))
      goto err;
    if (param->testflag & T_UNPACK)
    {
      share->options&= ~HA_OPTION_COMPRESS_RECORD;
      mi_int2store(share->state.header.options,share->options);
    }
    share->state.dellink= HA_OFFSET_ERROR;
    info->rec_cache.file=new_file;
  }

  info->update= (short) (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  /* Optionally drop indexes and optionally modify the key_map. */
  mi_drop_all_indexes(param, info, false);
  key_map= share->state.key_map;
  if (param->testflag & T_CREATE_MISSING_KEYS)
  {
    /* Invert the copied key_map to recreate all disabled indexes. */
    key_map= ~key_map;
  }

  sort_info.info=info;
  sort_info.param = param;

  set_data_file_type(&sort_info, share);
  sort_param.filepos=new_header_length;
  sort_info.dupp=0;
  sort_info.buff=0;
  param->read_cache.end_of_file=sort_info.filelength=
    lseek(param->read_cache.file,0L,SEEK_END);

  sort_param.wordlist=NULL;

  if (share->data_file_type == DYNAMIC_RECORD)
    length=max(share->base.min_pack_length+1,share->base.min_block_length);
  else if (share->data_file_type == COMPRESSED_RECORD)
    length=share->base.min_block_length;
  else
    length=share->base.pack_reclength;
  sort_info.max_records=
    ((param->testflag & T_CREATE_MISSING_KEYS) ? info->state->records :
     (ha_rows) (sort_info.filelength/length+1));
  sort_param.key_cmp=sort_key_cmp;
  sort_param.lock_in_memory=lock_memory;
  sort_param.sort_info=&sort_info;
  sort_param.fix_datafile= (bool) (! rep_quick);
  sort_param.master =1;

  del=info->state->del;
  param->glob_crc=0;
  if (param->testflag & T_CALC_CHECKSUM)
    sort_param.calc_checksum= 1;

  rec_per_key_part= param->rec_per_key_part;
  for (sort_param.key=0 ; sort_param.key < share->base.keys ;
       rec_per_key_part+=sort_param.keyinfo->keysegs, sort_param.key++)
  {
    sort_param.read_cache=param->read_cache;
    sort_param.keyinfo=share->keyinfo+sort_param.key;
    sort_param.seg=sort_param.keyinfo->seg;
    /*
      Skip this index if it is marked disabled in the copied
      (and possibly inverted) key_map.
    */
    if (! mi_is_key_active(key_map, sort_param.key))
    {
      /* Remember old statistics for key */
      assert(rec_per_key_part >= param->rec_per_key_part);
      memcpy(rec_per_key_part,
	     (share->state.rec_per_key_part +
              (rec_per_key_part - param->rec_per_key_part)),
	     sort_param.keyinfo->keysegs*sizeof(*rec_per_key_part));
      continue;
    }

    if ((!(param->testflag & T_SILENT)))
      printf ("- Fixing index %d\n",sort_param.key+1);
    sort_param.max_pos=sort_param.pos=share->pack.header_length;
    keyseg=sort_param.seg;
    memset(sort_param.unique, 0, sizeof(sort_param.unique));
    sort_param.key_length=share->rec_reflength;
    for (i=0 ; keyseg[i].type != HA_KEYTYPE_END; i++)
    {
      sort_param.key_length+=keyseg[i].length;
      if (keyseg[i].flag & HA_SPACE_PACK)
	sort_param.key_length+=get_pack_length(keyseg[i].length);
      if (keyseg[i].flag & (HA_BLOB_PART | HA_VAR_LENGTH_PART))
	sort_param.key_length+=2 + test(keyseg[i].length >= 127);
      if (keyseg[i].flag & HA_NULL_PART)
	sort_param.key_length++;
    }
    info->state->records=info->state->del=share->state.split=0;
    info->state->empty=0;

    {
      sort_param.key_read=sort_key_read;
      sort_param.key_write=sort_key_write;
    }

    if (_create_index_by_sort(&sort_param,
			      (bool) (!(param->testflag & T_VERBOSE)),
			      (uint) param->sort_buffer_length))
    {
      param->retry_repair=1;
      goto err;
    }
    /* No need to calculate checksum again. */
    sort_param.calc_checksum= 0;
    sort_param.wordroot.free_root(MYF(0));

    /* Set for next loop */
    sort_info.max_records= (ha_rows) info->state->records;

    if (param->testflag & T_STATISTICS)
      update_key_parts(sort_param.keyinfo, rec_per_key_part, sort_param.unique,
                       param->stats_method == MI_STATS_METHOD_IGNORE_NULLS?
                       sort_param.notnull: NULL,
                       (uint64_t) info->state->records);
    /* Enable this index in the permanent (not the copied) key_map. */
    mi_set_key_active(share->state.key_map, sort_param.key);

    if (sort_param.fix_datafile)
    {
      param->read_cache.end_of_file=sort_param.filepos;
      if (write_data_suffix(&sort_info, 1) || info->rec_cache.end_io_cache())
      {
	goto err;
      }
      if (param->testflag & T_SAFE_REPAIR)
      {
	/* Don't repair if we loosed more than one row */
	if (info->state->records+1 < start_records)
	{
	  info->state->records=start_records;
	  goto err;
	}
      }
      share->state.state.data_file_length = info->state->data_file_length=
	sort_param.filepos;
      /* Only whole records */
      share->state.version=(ulong) time((time_t*) 0);
      internal::my_close(info->dfile,MYF(0));
      info->dfile=new_file;
      share->data_file_type=sort_info.new_data_file_type;
      share->pack.header_length=(ulong) new_header_length;
      sort_param.fix_datafile=0;
    }
    else
      info->state->data_file_length=sort_param.max_pos;

    param->read_cache.file=info->dfile;		/* re-init read cache */
    param->read_cache.reinit_io_cache(READ_CACHE,share->pack.header_length, 1,1);
  }

  if (param->testflag & T_WRITE_LOOP)
  {
    fputs("          \r",stdout); fflush(stdout);
  }

  if (rep_quick && del+sort_info.dupp != info->state->del)
  {
    mi_check_print_error(param,"Couldn't fix table with quick recovery: Found wrong number of deleted records");
    mi_check_print_error(param,"Run recovery again without -q");
    got_error=1;
    param->retry_repair=1;
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    goto err;
  }

  if (rep_quick & T_FORCE_UNIQUENESS)
  {
    my_off_t skr=info->state->data_file_length+
      (share->options & HA_OPTION_COMPRESS_RECORD ?
       MEMMAP_EXTRA_MARGIN : 0);
#ifdef USE_RELOC
    if (share->data_file_type == STATIC_RECORD &&
	skr < share->base.reloc*share->base.min_pack_length)
      skr=share->base.reloc*share->base.min_pack_length;
#endif
    if (skr != sort_info.filelength && !info->s->base.raid_type)
      if (ftruncate(info->dfile, skr))
	mi_check_print_warning(param,
			       "Can't change size of datafile,  error: %d",
			       errno);
  }
  if (param->testflag & T_CALC_CHECKSUM)
    info->state->checksum=param->glob_crc;

  if (ftruncate(share->kfile, info->state->key_file_length))
    mi_check_print_warning(param,
			   "Can't change size of indexfile, error: %d",
			   errno);

  if (!(param->testflag & T_SILENT))
  {
    if (start_records != info->state->records)
      printf("Data records: %s\n", llstr(info->state->records,llbuff));
    if (sort_info.dupp)
      mi_check_print_warning(param,
			     "%s records have been removed",
			     llstr(sort_info.dupp,llbuff));
  }
  got_error=0;

  if (&share->state.state != info->state)
    memcpy( &share->state.state, info->state, sizeof(*info->state));

err:
  got_error|= flush_blocks(param, share->getKeyCache(), share->kfile);
  info->rec_cache.end_io_cache();
  if (!got_error)
  {
    /* Replace the actual file with the temporary file */
    if (new_file >= 0)
    {
      internal::my_close(new_file,MYF(0));
      info->dfile=new_file= -1;
      if (change_to_newfile(share->data_file_name,MI_NAME_DEXT,
			    DATA_TMP_EXT, share->base.raid_chunks,
			    MYF(0)) ||
	  mi_open_datafile(info,share,-1))
	got_error=1;
    }
  }
  if (got_error)
  {
    if (! param->error_printed)
      mi_check_print_error(param,"%d when fixing table",errno);
    if (new_file >= 0)
    {
      internal::my_close(new_file,MYF(0));
      my_delete(param->temp_filename, MYF(MY_WME));
      if (info->dfile == new_file)
        info->dfile= -1;
    }
    mi_mark_crashed_on_repair(info);
  }
  else if (key_map == share->state.key_map)
    share->state.changed&= ~STATE_NOT_OPTIMIZED_KEYS;
  share->state.changed|=STATE_NOT_SORTED_PAGES;

  void * rec_buff_ptr= NULL;
  rec_buff_ptr= mi_get_rec_buff_ptr(info, sort_param.rec_buff);
  free(rec_buff_ptr);
  rec_buff_ptr= mi_get_rec_buff_ptr(info, sort_param.record);
  free(rec_buff_ptr);
  rec_buff_ptr= NULL;

  free((unsigned char*) sort_info.key_block);
  free(sort_info.buff);
  param->read_cache.end_io_cache();
  info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  if (!got_error && (param->testflag & T_UNPACK))
  {
    share->state.header.options[0]&= (unsigned char) ~HA_OPTION_COMPRESS_RECORD;
    share->pack.header_length=0;
  }
  return(got_error);
}

	/* Read next record and return next key */

int sort_key_read(MI_SORT_PARAM *sort_param, void *key)
{
  int error;
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_INFO *info=sort_info->info;

  if ((error=sort_get_next_record(sort_param)))
    return(error);
  if (info->state->records == sort_info->max_records)
  {
    mi_check_print_error(sort_info->param,
			 "Key %d - Found too many records; Can't continue",
                         sort_param->key+1);
    return(1);
  }
  sort_param->real_key_length=
    (info->s->rec_reflength+
     _mi_make_key(info, sort_param->key, (unsigned char*) key,
		  sort_param->record, sort_param->filepos));
#ifdef HAVE_VALGRIND
  memset((unsigned char *)key+sort_param->real_key_length, 0,
         (sort_param->key_length-sort_param->real_key_length));
#endif
  return(sort_write_record(sort_param));
} /* sort_key_read */


/*
  Read next record from file using parameters in sort_info.

  SYNOPSIS
    sort_get_next_record()
      sort_param                Information about and for the sort process

  NOTE

    Dynamic Records With Non-Quick Parallel Repair

      For non-quick parallel repair we use a synchronized read/write
      cache. This means that one thread is the master who fixes the data
      file by reading each record from the old data file and writing it
      to the new data file. By doing this the records in the new data
      file are written contiguously. Whenever the write buffer is full,
      it is copied to the read buffer. The slaves read from the read
      buffer, which is not associated with a file. Thus read_cache.file
      is -1. When using _mi_read_cache(), the slaves must always set
      flag to READING_NEXT so that the function never tries to read from
      file. This is safe because the records are contiguous. There is no
      need to read outside the cache. This condition is evaluated in the
      variable 'parallel_flag' for quick reference. read_cache.file must
      be >= 0 in every other case.

  RETURN
    -1          end of file
    0           ok
    > 0         error
*/

int sort_get_next_record(MI_SORT_PARAM *sort_param)
{
  int searching;
  int parallel_flag;
  uint32_t found_record,b_type,left_length;
  my_off_t pos;
  unsigned char *to= NULL;
  MI_BLOCK_INFO block_info;
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_CHECK *param=sort_info->param;
  MI_INFO *info=sort_info->info;
  MYISAM_SHARE *share=info->s;
  char llbuff[22],llbuff2[22];

  if (*killed_ptr(param))
    return(1);

  switch (share->data_file_type) {
  case STATIC_RECORD:
    for (;;)
    {
      if (my_b_read(&sort_param->read_cache,sort_param->record,
		    share->base.pack_reclength))
      {
	if (sort_param->read_cache.error)
	  param->out_flag |= O_DATA_LOST;
        param->retry_repair=1;
        param->testflag|=T_RETRY_WITHOUT_QUICK;
	return(-1);
      }
      sort_param->start_recpos=sort_param->pos;
      if (!sort_param->fix_datafile)
      {
	sort_param->filepos=sort_param->pos;
        if (sort_param->master)
	  share->state.split++;
      }
      sort_param->max_pos=(sort_param->pos+=share->base.pack_reclength);
      if (*sort_param->record)
      {
	if (sort_param->calc_checksum)
	  param->glob_crc+= (info->checksum=
			     mi_static_checksum(info,sort_param->record));
	return(0);
      }
      if (!sort_param->fix_datafile && sort_param->master)
      {
	info->state->del++;
	info->state->empty+=share->base.pack_reclength;
      }
    }
  case DYNAMIC_RECORD:
    pos= sort_param->pos;
    searching= (sort_param->fix_datafile && (param->testflag & T_EXTEND));
    parallel_flag= (sort_param->read_cache.file < 0) ? READING_NEXT : 0;
    for (;;)
    {
      found_record=block_info.second_read= 0;
      left_length=1;
      if (searching)
      {
	pos=MY_ALIGN(pos,MI_DYN_ALIGN_SIZE);
        param->testflag|=T_RETRY_WITHOUT_QUICK;
	sort_param->start_recpos=pos;
      }
      do
      {
	if (pos > sort_param->max_pos)
	  sort_param->max_pos=pos;
	if (pos & (MI_DYN_ALIGN_SIZE-1))
	{
	  if ((param->testflag & T_VERBOSE) || searching == 0)
	    mi_check_print_info(param,"Wrong aligned block at %s",
				llstr(pos,llbuff));
	  if (searching)
	    goto try_next;
	}
	if (found_record && pos == param->search_after_block)
	  mi_check_print_info(param,"Block: %s used by record at %s",
		     llstr(param->search_after_block,llbuff),
		     llstr(sort_param->start_recpos,llbuff2));
	if (_mi_read_cache(&sort_param->read_cache,
                           (unsigned char*) block_info.header,pos,
			   MI_BLOCK_INFO_HEADER_LENGTH,
			   (! found_record ? READING_NEXT : 0) |
                           parallel_flag | READING_HEADER))
	{
	  if (found_record)
	  {
	    mi_check_print_info(param,
				"Can't read whole record at %s (errno: %d)",
				llstr(sort_param->start_recpos,llbuff),errno);
	    goto try_next;
	  }
	  return(-1);
	}
	if (searching && ! sort_param->fix_datafile)
	{
	  param->error_printed=1;
          param->retry_repair=1;
          param->testflag|=T_RETRY_WITHOUT_QUICK;
	  return(1);	/* Something wrong with data */
	}
	b_type=_mi_get_block_info(&block_info,-1,pos);
	if ((b_type & (BLOCK_ERROR | BLOCK_FATAL_ERROR)) ||
	   ((b_type & BLOCK_FIRST) &&
	     (block_info.rec_len < (uint) share->base.min_pack_length ||
	      block_info.rec_len > (uint) share->base.max_pack_length)))
	{
	  uint32_t i;
	  if (param->testflag & T_VERBOSE || searching == 0)
	    mi_check_print_info(param,
				"Wrong bytesec: %3d-%3d-%3d at %10s; Skipped",
		       block_info.header[0],block_info.header[1],
		       block_info.header[2],llstr(pos,llbuff));
	  if (found_record)
	    goto try_next;
	  block_info.second_read=0;
	  searching=1;
	  /* Search after block in read header string */
	  for (i=MI_DYN_ALIGN_SIZE ;
	       i < MI_BLOCK_INFO_HEADER_LENGTH ;
	       i+= MI_DYN_ALIGN_SIZE)
	    if (block_info.header[i] >= 1 &&
		block_info.header[i] <= MI_MAX_DYN_HEADER_BYTE)
	      break;
	  pos+=(ulong) i;
	  sort_param->start_recpos=pos;
	  continue;
	}
	if (b_type & BLOCK_DELETED)
	{
	  bool error=0;
	  if (block_info.block_len+ (uint) (block_info.filepos-pos) <
	      share->base.min_block_length)
	  {
	    if (!searching)
	      mi_check_print_info(param,
				  "Deleted block with impossible length %u at %s",
				  block_info.block_len,llstr(pos,llbuff));
	    error=1;
	  }
	  else
	  {
	    if ((block_info.next_filepos != HA_OFFSET_ERROR &&
		 block_info.next_filepos >=
		 info->state->data_file_length) ||
		(block_info.prev_filepos != HA_OFFSET_ERROR &&
		 block_info.prev_filepos >= info->state->data_file_length))
	    {
	      if (!searching)
		mi_check_print_info(param,
				    "Delete link points outside datafile at %s",
				    llstr(pos,llbuff));
	      error=1;
	    }
	  }
	  if (error)
	  {
	    if (found_record)
	      goto try_next;
	    searching=1;
	    pos+= MI_DYN_ALIGN_SIZE;
	    sort_param->start_recpos=pos;
	    block_info.second_read=0;
	    continue;
	  }
	}
	else
	{
	  if (block_info.block_len+ (uint) (block_info.filepos-pos) <
	      share->base.min_block_length ||
	      block_info.block_len > (uint) share->base.max_pack_length+
	      MI_SPLIT_LENGTH)
	  {
	    if (!searching)
	      mi_check_print_info(param,
				  "Found block with impossible length %u at %s; Skipped",
				  block_info.block_len+ (uint) (block_info.filepos-pos),
				  llstr(pos,llbuff));
	    if (found_record)
	      goto try_next;
	    searching=1;
	    pos+= MI_DYN_ALIGN_SIZE;
	    sort_param->start_recpos=pos;
	    block_info.second_read=0;
	    continue;
	  }
	}
	if (b_type & (BLOCK_DELETED | BLOCK_SYNC_ERROR))
	{
          if (!sort_param->fix_datafile && sort_param->master &&
              (b_type & BLOCK_DELETED))
	  {
	    info->state->empty+=block_info.block_len;
	    info->state->del++;
	    share->state.split++;
	  }
	  if (found_record)
	    goto try_next;
	  if (searching)
	  {
	    pos+=MI_DYN_ALIGN_SIZE;
	    sort_param->start_recpos=pos;
	  }
	  else
	    pos=block_info.filepos+block_info.block_len;
	  block_info.second_read=0;
	  continue;
	}

	if (!sort_param->fix_datafile && sort_param->master)
	  share->state.split++;
	if (! found_record++)
	{
	  sort_param->find_length=left_length=block_info.rec_len;
	  sort_param->start_recpos=pos;
	  if (!sort_param->fix_datafile)
	    sort_param->filepos=sort_param->start_recpos;
	  if (sort_param->fix_datafile && (param->testflag & T_EXTEND))
	    sort_param->pos=block_info.filepos+1;
	  else
	    sort_param->pos=block_info.filepos+block_info.block_len;
	  if (share->base.blobs)
	  {
	    if (!(to=mi_alloc_rec_buff(info,block_info.rec_len,
				       &(sort_param->rec_buff))))
	    {
	      if (param->max_record_length >= block_info.rec_len)
	      {
		mi_check_print_error(param,"Not enough memory for blob at %s (need %lu)",
				     llstr(sort_param->start_recpos,llbuff),
				     (ulong) block_info.rec_len);
		return(1);
	      }
	      else
	      {
		mi_check_print_info(param,"Not enough memory for blob at %s (need %lu); Row skipped",
				    llstr(sort_param->start_recpos,llbuff),
				    (ulong) block_info.rec_len);
		goto try_next;
	      }
	    }
	  }
	  else
	    to= sort_param->rec_buff;
	}
	if (left_length < block_info.data_len || ! block_info.data_len)
	{
	  mi_check_print_info(param,
			      "Found block with too small length at %s; Skipped",
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
	if (block_info.filepos + block_info.data_len >
	    sort_param->read_cache.end_of_file)
	{
	  mi_check_print_info(param,
			      "Found block that points outside data file at %s",
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
        /*
          Copy information that is already read. Avoid accessing data
          below the cache start. This could happen if the header
          streched over the end of the previous buffer contents.
        */
        {
          uint32_t header_len= (uint) (block_info.filepos - pos);
          uint32_t prefetch_len= (MI_BLOCK_INFO_HEADER_LENGTH - header_len);

          if (prefetch_len > block_info.data_len)
            prefetch_len= block_info.data_len;
          if (prefetch_len)
          {
            memcpy(to, block_info.header + header_len, prefetch_len);
            block_info.filepos+= prefetch_len;
            block_info.data_len-= prefetch_len;
            left_length-= prefetch_len;
            to+= prefetch_len;
          }
        }
        if (block_info.data_len &&
            _mi_read_cache(&sort_param->read_cache,to,block_info.filepos,
                           block_info.data_len,
                           (found_record == 1 ? READING_NEXT : 0) |
                           parallel_flag))
	{
	  mi_check_print_info(param,
			      "Read error for block at: %s (error: %d); Skipped",
			      llstr(block_info.filepos,llbuff),errno);
	  goto try_next;
	}
	left_length-=block_info.data_len;
	to+=block_info.data_len;
	pos=block_info.next_filepos;
	if (pos == HA_OFFSET_ERROR && left_length)
	{
	  mi_check_print_info(param,"Wrong block with wrong total length starting at %s",
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
	if (pos + MI_BLOCK_INFO_HEADER_LENGTH > sort_param->read_cache.end_of_file)
	{
	  mi_check_print_info(param,"Found link that points at %s (outside data file) at %s",
			      llstr(pos,llbuff2),
			      llstr(sort_param->start_recpos,llbuff));
	  goto try_next;
	}
      } while (left_length);

      if (_mi_rec_unpack(info,sort_param->record,sort_param->rec_buff,
			 sort_param->find_length) != MY_FILE_ERROR)
      {
	if (sort_param->read_cache.error < 0)
	  return(1);
	if (sort_param->calc_checksum)
	  info->checksum= mi_checksum(info, sort_param->record);
	if ((param->testflag & (T_EXTEND | T_REP)) || searching)
	{
	  if (_mi_rec_check(info, sort_param->record, sort_param->rec_buff,
                            sort_param->find_length,
                            (param->testflag & T_QUICK) &&
                            sort_param->calc_checksum &&
                            test(info->s->calc_checksum)))
	  {
	    mi_check_print_info(param,"Found wrong packed record at %s",
				llstr(sort_param->start_recpos,llbuff));
	    goto try_next;
	  }
	}
	if (sort_param->calc_checksum)
	  param->glob_crc+= info->checksum;
	return(0);
      }
      if (!searching)
        mi_check_print_info(param,"Key %d - Found wrong stored record at %s",
                            sort_param->key+1,
                            llstr(sort_param->start_recpos,llbuff));
    try_next:
      pos=(sort_param->start_recpos+=MI_DYN_ALIGN_SIZE);
      searching=1;
    }
  case COMPRESSED_RECORD:
  case BLOCK_RECORD:
    assert(0);                                  /* Impossible */
  }
  return(1);                               /* Impossible */
}


/*
  Write record to new file.

  SYNOPSIS
    sort_write_record()
      sort_param                Sort parameters.

  NOTE
    This is only called by a master thread if parallel repair is used.

  RETURN
    0           OK
    1           Error
*/

int sort_write_record(MI_SORT_PARAM *sort_param)
{
  int flag;
  ulong block_length,reclength;
  unsigned char *from;
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_CHECK *param=sort_info->param;
  MI_INFO *info=sort_info->info;
  MYISAM_SHARE *share=info->s;

  if (sort_param->fix_datafile)
  {
    switch (sort_info->new_data_file_type) {
    case STATIC_RECORD:
      if (my_b_write(&info->rec_cache,sort_param->record,
		     share->base.pack_reclength))
      {
	mi_check_print_error(param,"%d when writing to datafile",errno);
	return(1);
      }
      sort_param->filepos+=share->base.pack_reclength;
      info->s->state.split++;
      /* sort_info->param->glob_crc+=mi_static_checksum(info, sort_param->record); */
      break;
    case DYNAMIC_RECORD:
      if (! info->blobs)
	from=sort_param->rec_buff;
      else
      {
	/* must be sure that local buffer is big enough */
	reclength=info->s->base.pack_reclength+
	  _my_calc_total_blob_length(info,sort_param->record)+
	  ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
	  MI_DYN_DELETE_BLOCK_HEADER;
	if (sort_info->buff_length < reclength)
	{
          void *tmpptr= NULL;
	  tmpptr= realloc(sort_info->buff, reclength);
          if(tmpptr)
          {
	    sort_info->buff_length=reclength;
            sort_info->buff= (unsigned char *)tmpptr;
          }
          else
          {
            mi_check_print_error(param,"Could not realloc() sort_info->buff "
                                 " to %ul bytes", reclength);
            return(1);
          }
	}
	from= sort_info->buff+ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER);
      }
      /* We can use info->checksum here as only one thread calls this. */
      info->checksum=mi_checksum(info,sort_param->record);
      reclength=_mi_rec_pack(info,from,sort_param->record);
      flag=0;
      /* sort_info->param->glob_crc+=info->checksum; */

      do
      {
	block_length=reclength+ 3 + test(reclength >= (65520-3));
	if (block_length < share->base.min_block_length)
	  block_length=share->base.min_block_length;
	info->update|=HA_STATE_WRITE_AT_END;
	block_length=MY_ALIGN(block_length,MI_DYN_ALIGN_SIZE);
	if (block_length > MI_MAX_BLOCK_LENGTH)
	  block_length=MI_MAX_BLOCK_LENGTH;
	if (_mi_write_part_record(info,0L,block_length,
				  sort_param->filepos+block_length,
				  &from,&reclength,&flag))
	{
	  mi_check_print_error(param,"%d when writing to datafile",errno);
	  return(1);
	}
	sort_param->filepos+=block_length;
	info->s->state.split++;
      } while (reclength);
      /* sort_info->param->glob_crc+=info->checksum; */
      break;
    case COMPRESSED_RECORD:
    case BLOCK_RECORD:
      assert(0);                                  /* Impossible */
    }
  }
  if (sort_param->master)
  {
    info->state->records++;
    if ((param->testflag & T_WRITE_LOOP) &&
        (info->state->records % WRITE_COUNT) == 0)
    {
      char llbuff[22];
      printf("%s\r", llstr(info->state->records,llbuff));
      fflush(stdout);
    }
  }
  return(0);
} /* sort_write_record */


	/* Compare two keys from _create_index_by_sort */

int sort_key_cmp(MI_SORT_PARAM *sort_param, const void *a, const void *b)
{
  uint32_t not_used[2];
  return (ha_key_cmp(sort_param->seg, *((unsigned char* const *) a), *((unsigned char* const *) b),
		     USE_WHOLE_KEY, SEARCH_SAME, not_used));
} /* sort_key_cmp */


int sort_key_write(MI_SORT_PARAM *sort_param, const void *a)
{
  uint32_t diff_pos[2];
  char llbuff[22],llbuff2[22];
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_CHECK *param= sort_info->param;
  int cmp;

  if (sort_info->key_block->inited)
  {
    cmp=ha_key_cmp(sort_param->seg,sort_info->key_block->lastkey,
		   (unsigned char*) a, USE_WHOLE_KEY,SEARCH_FIND | SEARCH_UPDATE,
		   diff_pos);
    if (param->stats_method == MI_STATS_METHOD_NULLS_NOT_EQUAL)
      ha_key_cmp(sort_param->seg,sort_info->key_block->lastkey,
                 (unsigned char*) a, USE_WHOLE_KEY,
                 SEARCH_FIND | SEARCH_NULL_ARE_NOT_EQUAL, diff_pos);
    else if (param->stats_method == MI_STATS_METHOD_IGNORE_NULLS)
    {
      diff_pos[0]= mi_collect_stats_nonulls_next(sort_param->seg,
                                                 sort_param->notnull,
                                                 sort_info->key_block->lastkey,
                                                 (unsigned char*)a);
    }
    sort_param->unique[diff_pos[0]-1]++;
  }
  else
  {
    cmp= -1;
    if (param->stats_method == MI_STATS_METHOD_IGNORE_NULLS)
      mi_collect_stats_nonulls_first(sort_param->seg, sort_param->notnull,
                                     (unsigned char*)a);
  }
  if ((sort_param->keyinfo->flag & HA_NOSAME) && cmp == 0)
  {
    sort_info->dupp++;
    sort_info->info->lastpos=get_record_for_key(sort_info->info,
						sort_param->keyinfo,
						(unsigned char*) a);
    mi_check_print_warning(param,
			   "Duplicate key for record at %10s against record at %10s",
			   llstr(sort_info->info->lastpos,llbuff),
			   llstr(get_record_for_key(sort_info->info,
						    sort_param->keyinfo,
						    sort_info->key_block->
						    lastkey),
				 llbuff2));
    param->testflag|=T_RETRY_WITHOUT_QUICK;
    return (sort_delete_record(sort_param));
  }
  return (sort_insert_key(sort_param,sort_info->key_block,
			  (unsigned char*) a, HA_OFFSET_ERROR));
} /* sort_key_write */


	/* get pointer to record from a key */

my_off_t get_record_for_key(MI_INFO *info, MI_KEYDEF *keyinfo,
                            unsigned char *key) {
  return _mi_dpos(info,0,key+_mi_keylength(keyinfo,key));
} /* get_record_for_key */


	/* Insert a key in sort-key-blocks */

int sort_insert_key(MI_SORT_PARAM *sort_param,
                    register SORT_KEY_BLOCKS *key_block, unsigned char *key,
                    my_off_t prev_block)
{
  uint32_t a_length,t_length,nod_flag;
  my_off_t filepos,key_file_length;
  unsigned char *anc_buff,*lastkey;
  MI_KEY_PARAM s_temp;
  MI_INFO *info;
  MI_KEYDEF *keyinfo=sort_param->keyinfo;
  SORT_INFO *sort_info= sort_param->sort_info;
  MI_CHECK *param=sort_info->param;

  anc_buff=key_block->buff;
  info=sort_info->info;
  lastkey=key_block->lastkey;
  nod_flag= (key_block == sort_info->key_block ? 0 :
	     info->s->base.key_reflength);

  if (!key_block->inited)
  {
    key_block->inited=1;
    if (key_block == sort_info->key_block_end)
    {
      mi_check_print_error(param,"To many key-block-levels; Try increasing sort_key_blocks");
      return(1);
    }
    a_length=2+nod_flag;
    key_block->end_pos=anc_buff+2;
    lastkey=0;					/* No previous key in block */
  }
  else
    a_length=mi_getint(anc_buff);

	/* Save pointer to previous block */
  if (nod_flag)
    _mi_kpointer(info,key_block->end_pos,prev_block);

  t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,
				(unsigned char*) 0,lastkey,lastkey,key,
				 &s_temp);
  (*keyinfo->store_key)(keyinfo, key_block->end_pos+nod_flag,&s_temp);
  a_length+=t_length;
  mi_putint(anc_buff,a_length,nod_flag);
  key_block->end_pos+=t_length;
  if (a_length <= keyinfo->block_length)
  {
    _mi_move_key(keyinfo,key_block->lastkey,key);
    key_block->last_length=a_length-t_length;
    return(0);
  }

	/* Fill block with end-zero and write filled block */
  mi_putint(anc_buff,key_block->last_length,nod_flag);
  memset(anc_buff+key_block->last_length, 0,
         keyinfo->block_length - key_block->last_length);
  key_file_length=info->state->key_file_length;
  if ((filepos=_mi_new(info,keyinfo,DFLT_INIT_HITS)) == HA_OFFSET_ERROR)
    return(1);

  /* If we read the page from the key cache, we have to write it back to it */
  if (key_file_length == info->state->key_file_length)
  {
    if (_mi_write_keypage(info, keyinfo, filepos, DFLT_INIT_HITS, anc_buff))
      return(1);
  }
  else if (my_pwrite(info->s->kfile,(unsigned char*) anc_buff,
		     (uint) keyinfo->block_length,filepos, param->myf_rw))
    return(1);

	/* Write separator-key to block in next level */
  if (sort_insert_key(sort_param,key_block+1,key_block->lastkey,filepos))
    return(1);

	/* clear old block and write new key in it */
  key_block->inited=0;
  return(sort_insert_key(sort_param, key_block,key,prev_block));
} /* sort_insert_key */


	/* Delete record when we found a duplicated key */

int sort_delete_record(MI_SORT_PARAM *sort_param)
{
  uint32_t i;
  int old_file,error;
  unsigned char *key;
  SORT_INFO *sort_info=sort_param->sort_info;
  MI_CHECK *param=sort_info->param;
  MI_INFO *info=sort_info->info;

  if ((param->testflag & (T_FORCE_UNIQUENESS|T_QUICK)) == T_QUICK)
  {
    mi_check_print_error(param,
			 "Quick-recover aborted; Run recovery without switch -q or with switch -qq");
    return(1);
  }
  if (info->s->options & HA_OPTION_COMPRESS_RECORD)
  {
    mi_check_print_error(param,
			 "Recover aborted; Can't run standard recovery on compressed tables with errors in data-file. Use switch 'myisamchk --safe-recover' to fix it\n",stderr);;
    return(1);
  }

  old_file=info->dfile;
  info->dfile=info->rec_cache.file;
  if (sort_info->current_key)
  {
    key=info->lastkey+info->s->base.max_key_length;
    if ((error=(*info->s->read_rnd)(info,sort_param->record,info->lastpos,0)) &&
	error != HA_ERR_RECORD_DELETED)
    {
      mi_check_print_error(param,"Can't read record to be removed");
      info->dfile=old_file;
      return(1);
    }

    for (i=0 ; i < sort_info->current_key ; i++)
    {
      uint32_t key_length=_mi_make_key(info,i,key,sort_param->record,info->lastpos);
      if (_mi_ck_delete(info,i,key,key_length))
      {
	mi_check_print_error(param,"Can't delete key %d from record to be removed",i+1);
	info->dfile=old_file;
	return(1);
      }
    }
    if (sort_param->calc_checksum)
      param->glob_crc-=(*info->s->calc_checksum)(info, sort_param->record);
  }
  error=flush_io_cache(&info->rec_cache) || (*info->s->delete_record)(info);
  info->dfile=old_file;				/* restore actual value */
  info->state->records--;
  return(error);
} /* sort_delete_record */

	/* Fix all pending blocks and flush everything to disk */

int flush_pending_blocks(MI_SORT_PARAM *sort_param)
{
  uint32_t nod_flag,length;
  my_off_t filepos,key_file_length;
  SORT_KEY_BLOCKS *key_block;
  SORT_INFO *sort_info= sort_param->sort_info;
  myf myf_rw=sort_info->param->myf_rw;
  MI_INFO *info=sort_info->info;
  MI_KEYDEF *keyinfo=sort_param->keyinfo;

  filepos= HA_OFFSET_ERROR;			/* if empty file */
  nod_flag=0;
  for (key_block=sort_info->key_block ; key_block->inited ; key_block++)
  {
    key_block->inited=0;
    length=mi_getint(key_block->buff);
    if (nod_flag)
      _mi_kpointer(info,key_block->end_pos,filepos);
    key_file_length=info->state->key_file_length;
    memset(key_block->buff+length, 0, keyinfo->block_length-length);
    if ((filepos=_mi_new(info,keyinfo,DFLT_INIT_HITS)) == HA_OFFSET_ERROR)
      return(1);

    /* If we read the page from the key cache, we have to write it back */
    if (key_file_length == info->state->key_file_length)
    {
      if (_mi_write_keypage(info, keyinfo, filepos,
                            DFLT_INIT_HITS, key_block->buff))
	return(1);
    }
    else if (my_pwrite(info->s->kfile,(unsigned char*) key_block->buff,
		       (uint) keyinfo->block_length,filepos, myf_rw))
      return(1);
    nod_flag=1;
  }
  info->s->state.key_root[sort_param->key]=filepos; /* Last is root for tree */
  return(0);
} /* flush_pending_blocks */

	/* alloc space and pointers for key_blocks */

static SORT_KEY_BLOCKS *alloc_key_blocks(MI_CHECK *param, uint32_t blocks,
                                         uint32_t buffer_length)
{
  register uint32_t i;
  SORT_KEY_BLOCKS *block;

  if (!(block=(SORT_KEY_BLOCKS*) malloc((sizeof(SORT_KEY_BLOCKS)+
                                        buffer_length+IO_SIZE)*blocks)))
  {
    mi_check_print_error(param,"Not enough memory for sort-key-blocks");
    return(0);
  }
  for (i=0 ; i < blocks ; i++)
  {
    block[i].inited=0;
    block[i].buff=(unsigned char*) (block+blocks)+(buffer_length+IO_SIZE)*i;
  }
  return(block);
} /* alloc_key_blocks */


	/* Check if file is almost full */

int test_if_almost_full(MI_INFO *info)
{
  if (info->s->options & HA_OPTION_COMPRESS_RECORD)
    return 0;
  return (my_off_t)(lseek(info->s->kfile, 0L, SEEK_END) / 10 * 9) >
         (my_off_t) info->s->base.max_key_file_length ||
         (my_off_t)(lseek(info->dfile, 0L, SEEK_END) / 10 * 9) >
         (my_off_t) info->s->base.max_data_file_length;
}


	/* write suffix to data file if neaded */

int write_data_suffix(SORT_INFO *sort_info, bool fix_datafile)
{
  MI_INFO *info=sort_info->info;

  if (info->s->options & HA_OPTION_COMPRESS_RECORD && fix_datafile)
  {
    unsigned char buff[MEMMAP_EXTRA_MARGIN];
    memset(buff, 0, sizeof(buff));
    if (my_b_write(&info->rec_cache,buff,sizeof(buff)))
    {
      mi_check_print_error(sort_info->param,
			   "%d when writing to datafile",errno);
      return 1;
    }
    sort_info->param->read_cache.end_of_file+=sizeof(buff);
  }
  return 0;
}

	/* Update state and myisamchk_time of indexfile */

int update_state_info(MI_CHECK *param, MI_INFO *info,uint32_t update)
{
  MYISAM_SHARE *share=info->s;

  if (update & UPDATE_OPEN_COUNT)
  {
    share->state.open_count=0;
    share->global_changed=0;
  }
  if (update & UPDATE_STAT)
  {
    uint32_t i, key_parts= mi_uint2korr(share->state.header.key_parts);
    share->state.rec_per_key_rows=info->state->records;
    share->state.changed&= ~STATE_NOT_ANALYZED;
    if (info->state->records)
    {
      for (i=0; i<key_parts; i++)
      {
        if (!(share->state.rec_per_key_part[i]=param->rec_per_key_part[i]))
          share->state.changed|= STATE_NOT_ANALYZED;
      }
    }
  }
  if (update & (UPDATE_STAT | UPDATE_SORT | UPDATE_TIME | UPDATE_AUTO_INC))
  {
    if (update & UPDATE_TIME)
    {
      share->state.check_time= (long) time((time_t*) 0);
      if (!share->state.create_time)
	share->state.create_time=share->state.check_time;
    }
    /*
      When tables are locked we haven't synched the share state and the
      real state for a while so we better do it here before synching
      the share state to disk. Only when table is write locked is it
      necessary to perform this synch.
    */
    if (info->lock_type == F_WRLCK)
      share->state.state= *info->state;
    if (mi_state_info_write(share->kfile,&share->state,1+2))
      goto err;
    share->changed=0;
  }
  {						/* Force update of status */
    int error;
    uint32_t r_locks=share->r_locks,w_locks=share->w_locks;
    share->r_locks= share->w_locks= share->tot_locks= 0;
    error=_mi_writeinfo(info,WRITEINFO_NO_UNLOCK);
    share->r_locks=r_locks;
    share->w_locks=w_locks;
    share->tot_locks=r_locks+w_locks;
    if (!error)
      return 0;
  }
err:
  mi_check_print_error(param,"%d when updating keyfile",errno);
  return 1;
}

	/*
	  Update auto increment value for a table
	  When setting the 'repair_only' flag we only want to change the
	  old auto_increment value if its wrong (smaller than some given key).
	  The reason is that we shouldn't change the auto_increment value
	  for a table without good reason when only doing a repair; If the
	  user have inserted and deleted rows, the auto_increment value
	  may be bigger than the biggest current row and this is ok.

	  If repair_only is not set, we will update the flag to the value in
	  param->auto_increment is bigger than the biggest key.
	*/

void update_auto_increment_key(MI_CHECK *param, MI_INFO *info,
			       bool repair_only)
{
  unsigned char *record= 0;

  if (!info->s->base.auto_key ||
      ! mi_is_key_active(info->s->state.key_map, info->s->base.auto_key - 1))
  {
    if (!(param->testflag & T_VERY_SILENT))
      mi_check_print_info(param,
			  "Table: %s doesn't have an auto increment key\n",
			  param->isam_file_name);
    return;
  }
  if (!(param->testflag & T_SILENT) &&
      !(param->testflag & T_REP))
    printf("Updating MyISAM file: %s\n", param->isam_file_name);
  /*
    We have to use an allocated buffer instead of info->rec_buff as
    _mi_put_key_in_record() may use info->rec_buff
  */
  if (!mi_alloc_rec_buff(info, SIZE_MAX, &record))
  {
    mi_check_print_error(param,"Not enough memory for extra record");
    return;
  }

  mi_extra(info,HA_EXTRA_KEYREAD,0);
  if (mi_rlast(info, record, info->s->base.auto_key-1))
  {
    if (errno != HA_ERR_END_OF_FILE)
    {
      mi_extra(info,HA_EXTRA_NO_KEYREAD,0);
      free(mi_get_rec_buff_ptr(info, record));
      mi_check_print_error(param,"%d when reading last record",errno);
      return;
    }
    if (!repair_only)
      info->s->state.auto_increment=param->auto_increment_value;
  }
  else
  {
    uint64_t auto_increment= retrieve_auto_increment(info, record);
    set_if_bigger(info->s->state.auto_increment,auto_increment);
    if (!repair_only)
      set_if_bigger(info->s->state.auto_increment, param->auto_increment_value);
  }
  mi_extra(info,HA_EXTRA_NO_KEYREAD,0);
  free(mi_get_rec_buff_ptr(info, record));
  update_state_info(param, info, UPDATE_AUTO_INC);
  return;
}


/*
  Update statistics for each part of an index

  SYNOPSIS
    update_key_parts()
      keyinfo           IN  Index information (only key->keysegs used)
      rec_per_key_part  OUT Store statistics here
      unique            IN  Array of (#distinct tuples)
      notnull_tuples    IN  Array of (#tuples), or NULL
      records               Number of records in the table

  DESCRIPTION
    This function is called produce index statistics values from unique and
    notnull_tuples arrays after these arrays were produced with sequential
    index scan (the scan is done in two places: chk_index() and
    sort_key_write()).

    This function handles all 3 index statistics collection methods.

    Unique is an array:
      unique[0]= (#different values of {keypart1}) - 1
      unique[1]= (#different values of {keypart1,keypart2} tuple)-unique[0]-1
      ...

    For MI_STATS_METHOD_IGNORE_NULLS method, notnull_tuples is an array too:
      notnull_tuples[0]= (#of {keypart1} tuples such that keypart1 is not NULL)
      notnull_tuples[1]= (#of {keypart1,keypart2} tuples such that all
                          keypart{i} are not NULL)
      ...
    For all other statistics collection methods notnull_tuples==NULL.

    Output is an array:
    rec_per_key_part[k] =
     = E(#records in the table such that keypart_1=c_1 AND ... AND
         keypart_k=c_k for arbitrary constants c_1 ... c_k)

     = {assuming that values have uniform distribution and index contains all
        tuples from the domain (or that {c_1, ..., c_k} tuple is choosen from
        index tuples}

     = #tuples-in-the-index / #distinct-tuples-in-the-index.

    The #tuples-in-the-index and #distinct-tuples-in-the-index have different
    meaning depending on which statistics collection method is used:

    MI_STATS_METHOD_*  how are nulls compared?  which tuples are counted?
     NULLS_EQUAL            NULL == NULL           all tuples in table
     NULLS_NOT_EQUAL        NULL != NULL           all tuples in table
     IGNORE_NULLS               n/a             tuples that don't have NULLs
*/

void update_key_parts(MI_KEYDEF *keyinfo, ulong *rec_per_key_part,
                      uint64_t *unique, uint64_t *notnull,
                      uint64_t records)
{
  uint64_t count=0,tmp, unique_tuples;
  uint64_t tuples= records;
  uint32_t parts;
  for (parts=0 ; parts < keyinfo->keysegs  ; parts++)
  {
    count+=unique[parts];
    unique_tuples= count + 1;
    if (notnull)
    {
      tuples= notnull[parts];
      /*
        #(unique_tuples not counting tuples with NULLs) =
          #(unique_tuples counting tuples with NULLs as different) -
          #(tuples with NULLs)
      */
      unique_tuples -= (records - notnull[parts]);
    }

    if (unique_tuples == 0)
      tmp= 1;
    else if (count == 0)
      tmp= tuples; /* 1 unique tuple */
    else
      tmp= (tuples + unique_tuples/2) / unique_tuples;

    /*
      for some weird keys (e.g. FULLTEXT) tmp can be <1 here.
      let's ensure it is not
    */
    if (tmp < 1)
      tmp= 1;
    if (tmp >= (uint64_t) ~(ulong) 0)
      tmp=(uint64_t) ~(ulong) 0;

    *rec_per_key_part=(ulong) tmp;
    rec_per_key_part++;
  }
}


static ha_checksum mi_byte_checksum(const unsigned char *buf, uint32_t length)
{
  ha_checksum crc;
  const unsigned char *end=buf+length;
  for (crc=0; buf != end; buf++)
    crc=((crc << 1) + *((unsigned char*) buf)) +
      test(crc & (((ha_checksum) 1) << (8*sizeof(ha_checksum)-1)));
  return crc;
}

static bool mi_too_big_key_for_sort(MI_KEYDEF *key, ha_rows rows)
{
  uint32_t key_maxlength=key->maxlength;
  return (key->flag & (HA_BINARY_PACK_KEY | HA_VAR_LENGTH_KEY) &&
	  ((uint64_t) rows * key_maxlength >
	   (uint64_t) MAX_FILE_SIZE));
}

/*
  Deactivate all not unique index that can be recreated fast
  These include packed keys on which sorting will use more temporary
  space than the max allowed file length or for which the unpacked keys
  will take much more space than packed keys.
  Note that 'rows' may be zero for the case when we don't know how many
  rows we will put into the file.
 */

void mi_disable_non_unique_index(MI_INFO *info, ha_rows rows)
{
  MYISAM_SHARE *share=info->s;
  MI_KEYDEF    *key=share->keyinfo;
  uint32_t          i;

  assert(info->state->records == 0 &&
              (!rows || rows >= MI_MIN_ROWS_TO_DISABLE_INDEXES));
  for (i=0 ; i < share->base.keys ; i++,key++)
  {
    if (!(key->flag & (HA_NOSAME | HA_AUTO_KEY)) &&
        ! mi_too_big_key_for_sort(key,rows) && info->s->base.auto_key != i+1)
    {
      mi_clear_key_active(share->state.key_map, i);
      info->update|= HA_STATE_CHANGED;
    }
  }
}


/*
  Return true if we can use repair by sorting
  One can set the force argument to force to use sorting
  even if the temporary file would be quite big!
*/

bool mi_test_if_sort_rep(MI_INFO *info, ha_rows rows,
			    uint64_t key_map, bool force)
{
  MYISAM_SHARE *share=info->s;
  MI_KEYDEF *key=share->keyinfo;
  uint32_t i;

  /*
    mi_repair_by_sort only works if we have at least one key. If we don't
    have any keys, we should use the normal repair.
  */
  if (! mi_is_any_key_active(key_map))
    return false;				/* Can't use sort */
  for (i=0 ; i < share->base.keys ; i++,key++)
  {
    if (!force && mi_too_big_key_for_sort(key,rows))
      return false;
  }
  return true;
}


static void
set_data_file_type(SORT_INFO *sort_info, MYISAM_SHARE *share)
{
  if ((sort_info->new_data_file_type=share->data_file_type) ==
      COMPRESSED_RECORD && sort_info->param->testflag & T_UNPACK)
  {
    MYISAM_SHARE tmp;

    if (share->options & HA_OPTION_PACK_RECORD)
      sort_info->new_data_file_type = DYNAMIC_RECORD;
    else
      sort_info->new_data_file_type = STATIC_RECORD;

    /* Set delete_function for sort_delete_record() */
    memcpy(&tmp, share, sizeof(*share));
    tmp.options= ~HA_OPTION_COMPRESS_RECORD;
    mi_setup_functions(&tmp);
    share->delete_record=tmp.delete_record;
  }
}
