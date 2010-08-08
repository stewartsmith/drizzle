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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "heap_priv.h"

#include <drizzled/common.h>
#include <drizzled/error.h>

#include <string.h>
#include <algorithm>

using namespace std;
using namespace drizzled;

static int keys_compare(heap_rb_param *param, unsigned char *key1, unsigned char *key2);
static void init_block(HP_BLOCK *block,uint32_t chunk_length, uint32_t min_records,
                        uint32_t max_records);

#define FIXED_REC_OVERHEAD (sizeof(unsigned char))
#define VARIABLE_REC_OVERHEAD (sizeof(unsigned char**) + ALIGN_SIZE(sizeof(unsigned char)))

/* Minimum size that a chunk can take, 12 bytes on 32bit, 24 bytes on 64bit */
#define VARIABLE_MIN_CHUNK_SIZE \
        ((sizeof(unsigned char**) + VARIABLE_REC_OVERHEAD + sizeof(unsigned char**) - 1) & ~(sizeof(unsigned char**) - 1))


/* Create a heap table */

int heap_create(const char *name, uint32_t keys, HP_KEYDEF *keydef,
    uint32_t columns, HP_COLUMNDEF *columndef,
    uint32_t max_key_fieldnr, uint32_t key_part_size,
    uint32_t reclength, uint32_t keys_memory_size,
    uint32_t max_records, uint32_t min_records,
    HP_CREATE_INFO *create_info, HP_SHARE **res)
{
  uint32_t i, j, key_segs, max_length, length;
  uint32_t max_rows_for_stated_memory;
  HP_SHARE *share= 0;
  HA_KEYSEG *keyseg;

  if (!create_info->internal_table)
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
    uint32_t chunk_length, is_variable_size;
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

      if ((reclength - configured_chunk_size) >= VARIABLE_MIN_CHUNK_SIZE<<1)
      {
        /* Allow variable size only if we're saving at least two smallest chunks */
        /* There has to be at least one field after indexed fields */
        /* Note that NULL bits are already included in key_part_size */
        is_variable_size= 1;
        chunk_dataspace_length= configured_chunk_size;
      }
      else
      {
        /* max_chunk_size is near the full reclength, let's use fixed size */
        is_variable_size= 0;
        chunk_dataspace_length= reclength;
      }
    }
    else if (create_info->is_dynamic)
    {
      /* User asked for dynamic records - use 256 as the chunk size */
      if ((key_part_size + VARIABLE_REC_OVERHEAD) > 256)
        chunk_dataspace_length= key_part_size;
      else
        chunk_dataspace_length= 256 - VARIABLE_REC_OVERHEAD;

      is_variable_size= 1;
    }
    else
    {
      /* if max_chunk_size is not specified, put the whole record in one chunk */
      is_variable_size= 0;
      chunk_dataspace_length= reclength;
    }

    if (is_variable_size)
    {
      /* Check whether we have any variable size records past key data */
      uint32_t has_variable_fields= 0;

      fixed_data_length= key_part_size;
      fixed_column_count= max_key_fieldnr;

      for (i= max_key_fieldnr; i < columns; i++)
      {
        HP_COLUMNDEF* column= columndef + i;
        if (column->type == DRIZZLE_TYPE_VARCHAR && column->length >= 32)
        {
            /* The field has to be >= 5.0.3 true VARCHAR and have substantial length */
            /* TODO: do we want to calculate minimum length? */
            has_variable_fields= 1;
            break;
        }

        if (has_variable_fields)
        {
          break;
        }

        if ((column->offset + column->length) <= chunk_dataspace_length)
        {
          /* Still no variable-size columns, add one fixed-length */
          fixed_column_count= i + 1;
          fixed_data_length= column->offset + column->length;
        }
      }

      if (!has_variable_fields)
      {
        /* There is no need to use variable-size records without variable-size columns */
        /* Reset sizes if it's not variable size anymore */
        is_variable_size= 0;
        chunk_dataspace_length= reclength;
        fixed_data_length= reclength;
        fixed_column_count= columns;
      }
    }
    else
    {
      fixed_data_length= reclength;
      fixed_column_count= columns;
    }

    /*
      We store unsigned char* del_link inside the data area of deleted records,
      so the data length should be at least sizeof(unsigned char*)
    */
    set_if_bigger(chunk_dataspace_length, sizeof (unsigned char**));

    if (is_variable_size)
    {
      chunk_length= chunk_dataspace_length + VARIABLE_REC_OVERHEAD;
    }
    else
    {
      chunk_length= chunk_dataspace_length + FIXED_REC_OVERHEAD;
    }

    /* Align chunk length to the next pointer */
    chunk_length= (uint) (chunk_length + sizeof(unsigned char**) - 1) & ~(sizeof(unsigned char**) - 1);



    for (i= key_segs= max_length= 0, keyinfo= keydef; i < keys; i++, keyinfo++)
    {
      memset(&keyinfo->block, 0, sizeof(keyinfo->block));
      memset(&keyinfo->rb_tree , 0, sizeof(keyinfo->rb_tree));
      for (j= length= 0; j < keyinfo->keysegs; j++)
      {
	length+= keyinfo->seg[j].length;
	if (keyinfo->seg[j].null_bit)
	{
	  length++;
	  if (!(keyinfo->flag & HA_NULL_ARE_EQUAL))
	    keyinfo->flag|= HA_NULL_PART_KEY;
	  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
	    keyinfo->rb_tree.size_of_element++;
	}
	switch (keyinfo->seg[j].type) {
	case HA_KEYTYPE_LONG_INT:
	case HA_KEYTYPE_DOUBLE:
	case HA_KEYTYPE_ULONG_INT:
	case HA_KEYTYPE_LONGLONG:
	case HA_KEYTYPE_ULONGLONG:
	case HA_KEYTYPE_UINT24:
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
      length+= keyinfo->rb_tree.size_of_element +
	       ((keyinfo->algorithm == HA_KEY_ALG_BTREE) ? sizeof(unsigned char*) : 0);
      if (length > max_length)
	max_length= length;
      key_segs+= keyinfo->keysegs;
      if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
      {
        key_segs++; /* additional HA_KEYTYPE_END segment */
        if (keyinfo->flag & HA_VAR_LENGTH_KEY)
          keyinfo->get_key_length= hp_rb_var_key_length;
        else if (keyinfo->flag & HA_NULL_PART_KEY)
          keyinfo->get_key_length= hp_rb_null_key_length;
        else
          keyinfo->get_key_length= hp_rb_key_length;
      }
    }
    share= NULL;
    if (!(share= (HP_SHARE*) malloc(sizeof(HP_SHARE))))
      goto err;

    memset(share, 0, sizeof(HP_SHARE));

    if (keys && !(share->keydef= (HP_KEYDEF*) malloc(keys*sizeof(HP_KEYDEF))))
      goto err;

    memset(share->keydef, 0, keys*sizeof(HP_KEYDEF));

    if (keys && !(share->keydef->seg= (HA_KEYSEG*) malloc(key_segs*sizeof(HA_KEYSEG))))
      goto err;
    if (!(share->column_defs= (HP_COLUMNDEF*)
	  malloc(columns*sizeof(HP_COLUMNDEF))))
      goto err;

    memset(share->column_defs, 0, columns*sizeof(HP_COLUMNDEF));

    /*
       Max_records is used for estimating block sizes and for enforcement.
       Calculate the very maximum number of rows (if everything was one chunk) and
       then take either that value or configured max_records (pick smallest one)
    */
    max_rows_for_stated_memory= (uint32_t)(create_info->max_table_size /
      (keys_memory_size + chunk_length));
    max_records = ((max_records && max_records < max_rows_for_stated_memory) ?
                      max_records : max_rows_for_stated_memory);

    memcpy(share->column_defs, columndef, (size_t) (sizeof(columndef[0]) * columns));

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

      if (keydef[i].algorithm == HA_KEY_ALG_BTREE)
      {
	/* additional HA_KEYTYPE_END keyseg */
	keyseg->type=     HA_KEYTYPE_END;
	keyseg->length=   sizeof(unsigned char*);
	keyseg->flag=     0;
	keyseg->null_bit= 0;
	keyseg++;

	init_tree(&keyinfo->rb_tree, 0, 0, sizeof(unsigned char*),
		  (qsort_cmp2)keys_compare, true, NULL, NULL);
	keyinfo->delete_key= hp_rb_delete_key;
	keyinfo->write_key= hp_rb_write_key;
      }
      else
      {
	init_block(&keyinfo->block, sizeof(HASH_INFO), min_records,
		   max_records);
	keyinfo->delete_key= hp_delete_key;
	keyinfo->write_key= hp_write_key;
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
    share->recordspace.is_variable_size= is_variable_size;
    share->recordspace.total_data_length= 0;

    if (is_variable_size) {
      share->recordspace.offset_link= chunk_dataspace_length;
      share->recordspace.offset_status= share->recordspace.offset_link + sizeof(unsigned char**);
    } else {
      share->recordspace.offset_link= 1<<22; /* Make it likely to fail if anyone uses this offset */
      share->recordspace.offset_status= chunk_dataspace_length;
    }

    /* Must be allocated separately for rename to work */
    if (!(share->name= strdup(name)))
    {
      goto err;
    }
    thr_lock_init(&share->lock);
    pthread_mutex_init(&share->intern_lock,MY_MUTEX_INIT_FAST);
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
  if(share && share->keydef && share->keydef->seg)
    free(share->keydef->seg);
  if(share && share->keydef)
    free(share->keydef);
  if(share && share->column_defs)
    free(share->column_defs);
  if(share)
    free(share);
  if (!create_info->internal_table)
    THR_LOCK_heap.unlock();
  return(1);
} /* heap_create */


static int keys_compare(heap_rb_param *param, unsigned char *key1, unsigned char *key2)
{
  uint32_t not_used[2];
  return ha_key_cmp(param->keyseg, key1, key2, param->key_length,
		    param->search_flag, not_used);
}

static void init_block(HP_BLOCK *block, uint32_t chunk_length, uint32_t min_records,
		       uint32_t max_records)
{
  uint32_t i,recbuffer,records_in_block;

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

  for (i= 0; i <= HP_MAX_LEVELS; i++)
    block->level_info[i].records_under_level=
      (!i ? 1 : i == 1 ? records_in_block :
       HP_PTRS_IN_NOD * block->level_info[i - 1].records_under_level);
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


void heap_drop_table(HP_INFO *info)
{
  THR_LOCK_heap.lock();
  heap_try_free(info->s);
  THR_LOCK_heap.unlock();
  return;
}


void hp_free(HP_SHARE *share)
{
  heap_share_list.remove(share);        /* If not internal table */
  hp_clear(share);			/* Remove blocks from memory */
  thr_lock_delete(&share->lock);
  pthread_mutex_destroy(&share->intern_lock);
  if (share->keydef && share->keydef->seg)
    free(share->keydef->seg);
  if (share->keydef)
    free(share->keydef);
  free(share->column_defs);
  free((unsigned char*) share->name);
  free((unsigned char*) share);
  return;
}
