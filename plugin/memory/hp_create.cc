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

#include "heap_priv.h"
#include <drizzled/internal/my_sys.h>
#include <drizzled/common.h>
#include <drizzled/error.h>

#include <string.h>
#include <algorithm>

using namespace std;
using namespace drizzled;

static void init_block(HP_BLOCK *block,uint32_t chunk_length, uint32_t min_records,
                        uint32_t max_records);

static const int FIXED_REC_OVERHEAD = (sizeof(unsigned char));
static const int VARIABLE_REC_OVERHEAD = (sizeof(unsigned char**) + ALIGN_SIZE(sizeof(unsigned char)));

/* Minimum size that a chunk can take, 12 bytes on 32bit, 24 bytes on 64bit */
static const int VARIABLE_MIN_CHUNK_SIZE =
        ((sizeof(unsigned char**) + VARIABLE_REC_OVERHEAD + sizeof(unsigned char**) - 1) & ~(sizeof(unsigned char**) - 1));


/* Create a heap table */

int heap_create(const char *name, uint32_t keys, HP_KEYDEF *keydef,
                uint32_t columns,
                uint32_t key_part_size,
                uint32_t reclength, uint32_t keys_memory_size,
                uint32_t max_records, uint32_t min_records,
                HP_CREATE_INFO *create_info, HP_SHARE **res)
{
  uint32_t i, key_segs, max_length, length;
  uint32_t max_rows_for_stated_memory;
  HP_SHARE *share= 0;
  HA_KEYSEG *keyseg;

  if (not create_info->internal_table)
  {
    THR_LOCK_heap.lock();
    if ((share= hp_find_named_heap(name)) && share->open_count == 0)
    {
      hp_free(share);
      share= 0;
    }
  }

  if (!share)
  {
    size_t chunk_dataspace_length;
    uint32_t chunk_length;
    uint32_t fixed_data_length, fixed_column_count;
    HP_KEYDEF *keyinfo;

    if (create_info->max_chunk_size)
    {
      uint32_t configured_chunk_size= create_info->max_chunk_size;

      /* User requested variable-size records, let's see if they're possible */

      if (configured_chunk_size < key_part_size)
      {
        /* Eventual chunk_size cannot be smaller than key data,
          which allows all keys to fit into the first chunk */
        my_error(ER_CANT_USE_OPTION_HERE, MYF(0), "block_size");
        THR_LOCK_heap.unlock();
        return(ER_CANT_USE_OPTION_HERE);
      }

      /* max_chunk_size is near the full reclength, let's use fixed size */
      chunk_dataspace_length= reclength;
    }
    else
    {
      /* if max_chunk_size is not specified, put the whole record in one chunk */
      chunk_dataspace_length= reclength;
    }

    {
      fixed_data_length= reclength;
      fixed_column_count= columns;
    }

    /*
      We store unsigned char* del_link inside the data area of deleted records,
      so the data length should be at least sizeof(unsigned char*)
    */
    set_if_bigger(chunk_dataspace_length, sizeof (unsigned char**));

    {
      chunk_length= chunk_dataspace_length + FIXED_REC_OVERHEAD;
    }

    /* Align chunk length to the next pointer */
    chunk_length= (uint) (chunk_length + sizeof(unsigned char**) - 1) & ~(sizeof(unsigned char**) - 1);



    for (i= key_segs= max_length= 0, keyinfo= keydef; i < keys; i++, keyinfo++)
    {
      memset(&keyinfo->block, 0, sizeof(keyinfo->block));
      for (uint32_t j= length= 0; j < keyinfo->keysegs; j++)
      {
	length+= keyinfo->seg[j].length;
	if (keyinfo->seg[j].null_bit)
	{
	  length++;
	  if (!(keyinfo->flag & HA_NULL_ARE_EQUAL))
	    keyinfo->flag|= HA_NULL_PART_KEY;
	}
	switch (keyinfo->seg[j].type) {
	case HA_KEYTYPE_LONG_INT:
	case HA_KEYTYPE_DOUBLE:
	case HA_KEYTYPE_ULONG_INT:
	case HA_KEYTYPE_LONGLONG:
	case HA_KEYTYPE_ULONGLONG:
	  keyinfo->seg[j].flag|= HA_SWAP_KEY;
          break;
        case HA_KEYTYPE_VARBINARY1:
          /* Case-insensitiveness is handled in coll->hash_sort */
          keyinfo->seg[j].type= HA_KEYTYPE_VARTEXT1;
          /* fall_through */
        case HA_KEYTYPE_VARTEXT1:
          keyinfo->flag|= HA_VAR_LENGTH_KEY;
          length+= 2;
          /* Save number of bytes used to store length */
          keyinfo->seg[j].bit_start= 1;
          break;
        case HA_KEYTYPE_VARBINARY2:
          /* Case-insensitiveness is handled in coll->hash_sort */
          /* fall_through */
        case HA_KEYTYPE_VARTEXT2:
          keyinfo->flag|= HA_VAR_LENGTH_KEY;
          length+= 2;
          /* Save number of bytes used to store length */
          keyinfo->seg[j].bit_start= 2;
          /*
            Make future comparison simpler by only having to check for
            one type
          */
          keyinfo->seg[j].type= HA_KEYTYPE_VARTEXT1;
          break;
	default:
	  break;
	}
      }
      keyinfo->length= length;
      if (length > max_length)
	max_length= length;
      key_segs+= keyinfo->keysegs;
    }
    share= new HP_SHARE;

    if (keys && !(share->keydef= new HP_KEYDEF[keys]))
      goto err;
    if (keys && !(share->keydef->seg= new HA_KEYSEG[key_segs]))
      goto err;

    /*
       Max_records is used for estimating block sizes and for enforcement.
       Calculate the very maximum number of rows (if everything was one chunk) and
       then take either that value or configured max_records (pick smallest one)
    */
    max_rows_for_stated_memory= (uint32_t)(create_info->max_table_size /
      (keys_memory_size + chunk_length));
    max_records = ((max_records && max_records < max_rows_for_stated_memory) ?
                      max_records : max_rows_for_stated_memory);

    share->key_stat_version= 1;
    keyseg= keys ? share->keydef->seg : NULL;

    init_block(&share->recordspace.block, chunk_length, min_records, max_records);
    /* Fix keys */
    memcpy(share->keydef, keydef, (size_t) (sizeof(keydef[0]) * keys));
    for (i= 0, keyinfo= share->keydef; i < keys; i++, keyinfo++)
    {
      keyinfo->seg= keyseg;
      memcpy(keyseg, keydef[i].seg,
	     (size_t) (sizeof(keyseg[0]) * keydef[i].keysegs));
      keyseg+= keydef[i].keysegs;
      {
	init_block(&keyinfo->block, sizeof(HASH_INFO), min_records,
		   max_records);
        keyinfo->hash_buckets= 0;
      }
      if ((keyinfo->flag & HA_AUTO_KEY) && create_info->with_auto_increment)
        share->auto_key= i + 1;
    }
    share->min_records= min_records;
    share->max_records= max_records;
    share->max_table_size= create_info->max_table_size;
    share->index_length= 0;
    share->blength= 1;
    share->keys= keys;
    share->max_key_length= max_length;
    share->column_count= columns;
    share->changed= 0;
    share->auto_key= create_info->auto_key;
    share->auto_key_type= create_info->auto_key_type;
    share->auto_increment= create_info->auto_increment;

    share->fixed_data_length= fixed_data_length;
    share->fixed_column_count= fixed_column_count;

    share->recordspace.chunk_length= chunk_length;
    share->recordspace.chunk_dataspace_length= chunk_dataspace_length;
    share->recordspace.total_data_length= 0;

    {
      share->recordspace.offset_link= 1<<22; /* Make it likely to fail if anyone uses this offset */
      share->recordspace.offset_status= chunk_dataspace_length;
    }

    /* Must be allocated separately for rename to work */
    share->name.append(name);
    if (!create_info->internal_table)
    {
      heap_share_list.push_front(share);
    }
    else
      share->delete_on_close= 1;
  }
  if (!create_info->internal_table)
    THR_LOCK_heap.unlock();

  *res= share;
  return(0);

err:
  if (share && share->keydef)
    delete [] share->keydef->seg;
  if (share)
    delete [] share->keydef;
  delete share;
  if (not create_info->internal_table)
    THR_LOCK_heap.unlock();
  return(1);
} /* heap_create */


static void init_block(HP_BLOCK *block, uint32_t chunk_length, uint32_t min_records,
		       uint32_t max_records)
{
  uint32_t recbuffer,records_in_block;

  max_records= max(min_records,max_records);
  if (!max_records)
    max_records= 1000;			/* As good as quess as anything */

  /* we want to start each chunk at 8 bytes boundary, round recbuffer to the next 8 */
  recbuffer= (uint) (chunk_length + sizeof(unsigned char**) - 1) & ~(sizeof(unsigned char**) - 1);
  records_in_block= max_records / 10;
  if (records_in_block < 10 && max_records)
    records_in_block= 10;
  if (!records_in_block || records_in_block*recbuffer >
      (internal::my_default_record_cache_size-sizeof(HP_PTRS)*HP_MAX_LEVELS))
    records_in_block= (internal::my_default_record_cache_size - sizeof(HP_PTRS) *
		      HP_MAX_LEVELS) / recbuffer + 1;
  block->records_in_block= records_in_block;
  block->recbuffer= recbuffer;
  block->last_allocated= 0L;

  for (uint32_t i= 0; i <= HP_MAX_LEVELS; i++)
  {
    block->level_info[i].records_under_level=
      (!i ? 1 : i == 1 ? records_in_block :
       HP_PTRS_IN_NOD * block->level_info[i - 1].records_under_level);
  }
}


static inline void heap_try_free(HP_SHARE *share)
{
  if (share->open_count == 0)
    hp_free(share);
  else
    share->delete_on_close= 1;
}


int heap_delete_table(const char *name)
{
  int result;
  register HP_SHARE *share;

  THR_LOCK_heap.lock();
  if ((share= hp_find_named_heap(name)))
  {
    heap_try_free(share);
    result= 0;
  }
  else
  {
    result= errno=ENOENT;
  }
  THR_LOCK_heap.unlock();
  return(result);
}


void hp_free(HP_SHARE *share)
{
  heap_share_list.remove(share);        /* If not internal table */
  hp_clear(share);			/* Remove blocks from memory */
  if (share->keydef)
    delete [] share->keydef->seg;
  delete [] share->keydef;
  delete share;
}
