/* Copyright (C) 2000-2002, 2004 MySQL AB

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

/* This file is included in all heap-files */
#pragma once

#include <config.h>
#include <drizzled/base.h>

#include "heap.h"			/* Structs & some defines */

#include <list>


namespace boost {
  class mutex;
}

/*
  When allocating keys /rows in the internal block structure, do it
  within the following boundaries.

  The challenge is to find the balance between allocate as few blocks
  as possible and keep memory consumption down.
*/

#define CHUNK_STATUS_DELETED 0    /* this chunk has been deleted and can be reused */
#define CHUNK_STATUS_ACTIVE  1    /* this chunk represents the first part of a live record */

	/* Some extern variables */

extern std::list<HP_SHARE *> heap_share_list;
extern std::list<HP_INFO *> heap_open_list;

#define test_active(info) \
if (!(info->update & HA_STATE_AKTIV))\
{ errno= drizzled::HA_ERR_NO_ACTIVE_RECORD; return(-1); }
#define hp_find_hash(A,B) ((HASH_INFO*) hp_find_block((A),(B)))

	/* Find pos for record and update it in info->current_ptr */
#define hp_find_record(info,pos) (info)->current_ptr= hp_find_block(&(info)->getShare()->recordspace.block,pos)

#define get_chunk_status(info,ptr) (ptr[(info)->offset_status])

typedef struct st_hp_hash_info
{
  struct st_hp_hash_info *next_key;
  unsigned char *ptr_to_rec;
} HASH_INFO;

	/* Prototypes for intern functions */

extern HP_SHARE *hp_find_named_heap(const char *name);
extern int hp_rectest(HP_INFO *info,const unsigned char *old);
extern unsigned char *hp_find_block(HP_BLOCK *info,uint32_t pos);
extern int hp_get_new_block(HP_BLOCK *info, size_t* alloc_length);
extern void hp_free(HP_SHARE *info);
extern unsigned char *hp_free_level(HP_BLOCK *block,uint32_t level,HP_PTRS *pos,
                                    unsigned char *last_pos);
extern int hp_write_key(HP_INFO *info, HP_KEYDEF *keyinfo,
			const unsigned char *record, unsigned char *recpos);
extern int hp_delete_key(HP_INFO *info,HP_KEYDEF *keyinfo,
			 const unsigned char *record,unsigned char *recpos,int flag);
extern HASH_INFO *_heap_find_hash(HP_BLOCK *block,uint32_t pos);
extern unsigned char *hp_search(HP_INFO *info,HP_KEYDEF *keyinfo,const unsigned char *key,
		       uint32_t nextflag);
extern unsigned char *hp_search_next(HP_INFO *info, HP_KEYDEF *keyinfo,
			    const unsigned char *key, HASH_INFO *pos);
extern uint32_t hp_rec_hashnr(HP_KEYDEF *keyinfo,const unsigned char *rec);
extern uint32_t hp_mask(uint32_t hashnr,uint32_t buffmax,uint32_t maxlength);
extern void hp_movelink(HASH_INFO *pos,HASH_INFO *next_link,
			 HASH_INFO *newlink);
extern int hp_rec_key_cmp(HP_KEYDEF *keydef,const unsigned char *rec1,
			  const unsigned char *rec2,
                          bool diff_if_only_endspace_difference);
extern void hp_make_key(HP_KEYDEF *keydef,unsigned char *key,const unsigned char *rec);
extern bool hp_if_null_in_key(HP_KEYDEF *keyinfo, const unsigned char *record);
extern int hp_close(HP_INFO *info);
extern void hp_clear(HP_SHARE *info);

   /* Chunkset management (alloc/free/encode/decode) functions */

extern unsigned char *hp_allocate_chunkset(HP_DATASPACE *info, uint32_t chunk_count);
extern void hp_free_chunks(HP_DATASPACE *info, unsigned char *pos);
extern void hp_clear_dataspace(HP_DATASPACE *info);

extern uint32_t hp_get_encoded_data_length(HP_SHARE *info, const unsigned char *record, uint32_t *chunk_count);
extern void hp_copy_record_data_to_chunkset(HP_SHARE *info, const unsigned char *record, unsigned char *pos);
extern void hp_extract_record(HP_SHARE *info, unsigned char *record, const unsigned char *pos);
extern bool hp_compare_record_data_to_chunkset(HP_SHARE *info, const unsigned char *record, unsigned char *pos);

extern boost::mutex THR_LOCK_heap;

