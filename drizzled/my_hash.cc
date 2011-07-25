/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* The hash functions used for saving keys */
/* One of key_length or key_length_offset must be given */
/* Key length of 0 isn't allowed */

#include <config.h>
#include <drizzled/my_hash.h>
#include <drizzled/charset.h>
#include <vector>

namespace drizzled {

const uint32_t NO_RECORD= UINT32_MAX;

const int LOWFIND= 1;
const int LOWUSED= 2;
const int HIGHFIND= 4;
const int HIGHUSED= 8;

static uint32_t hash_mask(uint32_t hashnr, uint32_t buffmax, uint32_t maxlength);
static void movelink(HASH_LINK *array, uint32_t pos, uint32_t next_link, uint32_t newlink);
static int hashcmp(const HASH *hash, HASH_LINK *pos, const unsigned char *key, size_t length);

static uint32_t calc_hash(const HASH *hash, const unsigned char *key, size_t length)
{
  uint32_t nr1=1, nr2=4;
  hash->charset->coll->hash_sort(hash->charset, key,length, &nr1, &nr2);
  return nr1;
}

#define dynamic_element(array,array_index,type) ((type)((array)->buffer) +(array_index))

void
_hash_init(HASH *hash,uint32_t growth_size, const charset_info_st * const charset,
           uint32_t size, size_t key_offset, size_t key_length,
           hash_get_key get_key,
           hash_free_key free_element, uint32_t flags)
{
  hash->records=0;
  my_init_dynamic_array_ci(&hash->array, sizeof(HASH_LINK), size, growth_size);
  hash->key_offset=key_offset;
  hash->key_length=key_length;
  hash->blength=1;
  hash->get_key=get_key;
  hash->free=free_element;
  hash->flags=flags;
  hash->charset=charset;
}


/*
  Call hash->free on all elements in hash.

  SYNOPSIS
  hash_free_elements()
  hash   hash table

  NOTES:
  Sets records to 0
*/

static inline void hash_free_elements(HASH *hash)
{
  if (hash->free)
  {
    HASH_LINK *data=dynamic_element(&hash->array,0,HASH_LINK*);
    HASH_LINK *end= data + hash->records;
    while (data < end)
      (*hash->free)((data++)->data);
  }
  hash->records=0;
}


/*
  Free memory used by hash.

  SYNOPSIS
  hash_free()
  hash   the hash to delete elements of

  NOTES: Hash can't be reused without calling hash_init again.
*/

void hash_free(HASH *hash)
{
  hash_free_elements(hash);
  hash->free= 0;
  delete_dynamic(&hash->array);
}

/* some helper functions */

/*
  This function is char* instead of unsigned char* as HPUX11 compiler can't
  handle inline functions that are not defined as native types
*/

static inline char*
hash_key(const HASH *hash, const unsigned char *record, size_t *length,
         bool first)
{
  if (hash->get_key)
    return (char*) (*hash->get_key)(record,length,first);
  *length=hash->key_length;
  return (char*) record+hash->key_offset;
}

/* Calculate pos according to keys */

static uint32_t hash_mask(uint32_t hashnr,uint32_t buffmax,uint32_t maxlength)
{
  if ((hashnr & (buffmax-1)) < maxlength) return (hashnr & (buffmax-1));
  return (hashnr & ((buffmax >> 1) -1));
}

static uint32_t hash_rec_mask(const HASH *hash, HASH_LINK *pos,
                              uint32_t buffmax, uint32_t maxlength)
{
  size_t length;
  unsigned char *key= (unsigned char*) hash_key(hash,pos->data,&length,0);
  return hash_mask(calc_hash(hash,key,length),buffmax,maxlength);
}



static
inline
unsigned int rec_hashnr(HASH *hash,const unsigned char *record)
{
  size_t length;
  unsigned char *key= (unsigned char*) hash_key(hash,record,&length,0);
  return calc_hash(hash,key,length);
}


unsigned char* hash_search(const HASH *hash, const unsigned char *key,
                           size_t length)
{
  HASH_SEARCH_STATE state;
  return hash_first(hash, key, length, &state);
}

/*
  Search after a record based on a key

  NOTE
  Assigns the number of the found record to HASH_SEARCH_STATE state
*/

unsigned char* hash_first(const HASH *hash, const unsigned char *key,
                          size_t length,
                          HASH_SEARCH_STATE *current_record)
{
  HASH_LINK *pos;
  uint32_t flag,idx;

  flag=1;
  if (hash->records)
  {
    idx=hash_mask(calc_hash(hash,key,length ? length : hash->key_length),
                  hash->blength,hash->records);
    do
    {
      pos= dynamic_element(&hash->array,idx,HASH_LINK*);
      if (!hashcmp(hash,pos,key,length))
      {
        *current_record= idx;
        return (pos->data);
      }
      if (flag)
      {
        /* Reset flag */
        flag=0;
        if (hash_rec_mask(hash,pos,hash->blength,hash->records) != idx)
          /* Wrong link */
          break;
      }
    }
    while ((idx=pos->next) != NO_RECORD);
  }
  *current_record= NO_RECORD;
  return 0;
}

/* Change link from pos to new_link */

static void movelink(HASH_LINK *array, uint32_t find,
                     uint32_t next_link, uint32_t newlink)
{
  HASH_LINK *old_link;
  do
  {
    old_link=array+next_link;
  }
  while ((next_link=old_link->next) != find);
  old_link->next= newlink;
  return;
}

/*
  Compare a key in a record to a whole key. Return 0 if identical

  SYNOPSIS
  hashcmp()
  hash   hash table
  pos    position of hash record to use in comparison
  key    key for comparison
  length length of key

  NOTES:
  If length is 0, comparison is done using the length of the
  record being compared against.

  RETURN
  = 0  key of record == key
  != 0 key of record != key
*/

static int hashcmp(const HASH *hash, HASH_LINK *pos, const unsigned char *key,
                   size_t length)
{
  size_t rec_keylength;
  unsigned char *rec_key= (unsigned char*) hash_key(hash, pos->data,
                                                    &rec_keylength,1);
  return ((length && length != rec_keylength) ||
          my_strnncoll(hash->charset, rec_key, rec_keylength,
                       key, rec_keylength));
}


/* Write a hash-key to the hash-index */

bool my_hash_insert(HASH *info,const unsigned char *record)
{
  int flag;
  size_t idx;
  uint32_t halfbuff,hash_nr,first_index;
  unsigned char *ptr_to_rec=NULL,*ptr_to_rec2=NULL;
  HASH_LINK *data,*empty,*gpos=NULL,*gpos2=NULL,*pos;

  if (HASH_UNIQUE & info->flags)
  {
    unsigned char *key= (unsigned char*) hash_key(info, record, &idx, 1);
    if (hash_search(info, key, idx))
      /* Duplicate entry */
      return true;
  }

  flag=0;
  if (!(empty=(HASH_LINK*) alloc_dynamic(&info->array)))
    /* No more memory */
    return true;

  data=dynamic_element(&info->array,0,HASH_LINK*);
  halfbuff= info->blength >> 1;

  idx= first_index= info->records-halfbuff;
  /* If some records */
  if (idx != info->records)
  {
    do
    {
      pos=data+idx;
      hash_nr=rec_hashnr(info,pos->data);
      /* First loop; Check if ok */
      if (flag == 0)
        if (hash_mask(hash_nr,info->blength,info->records) != first_index)
          break;
      if (!(hash_nr & halfbuff))
      {
        /* Key will not move */
        if (!(flag & LOWFIND))
        {
          if (flag & HIGHFIND)
          {
            flag=LOWFIND | HIGHFIND;
            /* key shall be moved to the current empty position */
            gpos=empty;
            ptr_to_rec=pos->data;
            /* This place is now free */
            empty=pos;
          }
          else
          {
            /* key isn't changed */
            flag=LOWFIND | LOWUSED;
            gpos=pos;
            ptr_to_rec=pos->data;
          }
        }
        else
        {
          if (!(flag & LOWUSED))
          {
            /* Change link of previous LOW-key */
            gpos->data=ptr_to_rec;
            gpos->next= (uint32_t) (pos-data);
            flag= (flag & HIGHFIND) | (LOWFIND | LOWUSED);
          }
          gpos=pos;
          ptr_to_rec=pos->data;
        }
      }
      else
      {
        /* key will be moved */
        if (!(flag & HIGHFIND))
        {
          flag= (flag & LOWFIND) | HIGHFIND;
          /* key shall be moved to the last (empty) position */
          gpos2 = empty; empty=pos;
          ptr_to_rec2=pos->data;
        }
        else
        {
          if (!(flag & HIGHUSED))
          {
            /* Change link of previous hash-key and save */
            gpos2->data=ptr_to_rec2;
            gpos2->next=(uint32_t) (pos-data);
            flag= (flag & LOWFIND) | (HIGHFIND | HIGHUSED);
          }
          gpos2=pos;
          ptr_to_rec2=pos->data;
        }
      }
    }
    while ((idx=pos->next) != NO_RECORD);

    if ((flag & (LOWFIND | LOWUSED)) == LOWFIND)
    {
      gpos->data=ptr_to_rec;
      gpos->next=NO_RECORD;
    }
    if ((flag & (HIGHFIND | HIGHUSED)) == HIGHFIND)
    {
      gpos2->data=ptr_to_rec2;
      gpos2->next=NO_RECORD;
    }
  }
  /* Check if we are at the empty position */

  idx=hash_mask(rec_hashnr(info,record),info->blength,info->records+1);
  pos=data+idx;
  if (pos == empty)
  {
    pos->data=(unsigned char*) record;
    pos->next=NO_RECORD;
  }
  else
  {
    /* Check if more records in same hash-nr family */
    empty[0]=pos[0];
    gpos=data+hash_rec_mask(info,pos,info->blength,info->records+1);
    if (pos == gpos)
    {
      pos->data=(unsigned char*) record;
      pos->next=(uint32_t) (empty - data);
    }
    else
    {
      pos->data=(unsigned char*) record;
      pos->next=NO_RECORD;
      movelink(data,(uint32_t) (pos-data),(uint32_t) (gpos-data),(uint32_t) (empty-data));
    }
  }
  if (++info->records == info->blength)
    info->blength+= info->blength;
  return 0;
}

} /* namespace drizzled */
