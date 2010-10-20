/* Copyright (C) 2003, 2005 MySQL AB

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

/*
  Preload indexes into key cache
*/

#include "myisam_priv.h"
#include <stdlib.h>
#include <drizzled/util/test.h>

using namespace drizzled;

/*
  Preload pages of the index file for a table into the key cache

  SYNOPSIS
    mi_preload()
      info          open table
      map           map of indexes to preload into key cache
      ignore_leaves only non-leaves pages are to be preloaded

  RETURN VALUE
    0 if a success. error code - otherwise.

  NOTES.
    At present pages for all indexes are preloaded.
    In future only pages for indexes specified in the key_map parameter
    of the table will be preloaded.
*/

int mi_preload(MI_INFO *info, uint64_t key_map, bool ignore_leaves)
{
  uint32_t i;
  uint32_t length, block_length= 0;
  unsigned char *buff= NULL;
  MYISAM_SHARE* share= info->s;
  uint32_t keys= share->state.header.keys;
  MI_KEYDEF *keyinfo= share->keyinfo;
  internal::my_off_t key_file_length= share->state.state.key_file_length;
  internal::my_off_t pos= share->base.keystart;

  if (!keys || !mi_is_any_key_active(key_map) || key_file_length == pos)
    return(0);

  block_length= keyinfo[0].block_length;

  if (ignore_leaves)
  {
    /* Check whether all indexes use the same block size */
    for (i= 1 ; i < keys ; i++)
    {
      if (keyinfo[i].block_length != block_length)
        return(errno= HA_ERR_NON_UNIQUE_BLOCK_SIZE);
    }
  }
  else
    block_length= share->getKeyCache()->key_cache_block_size;

  length= info->preload_buff_size/block_length * block_length;
  set_if_bigger(length, block_length);

  if (!(buff= (unsigned char *) malloc(length)))
    return(errno= HA_ERR_OUT_OF_MEM);

  if (flush_key_blocks(share->getKeyCache(), share->kfile, FLUSH_RELEASE))
    goto err;

  do
  {
    /* Read the next block of index file into the preload buffer */
    if ((internal::my_off_t) length > (key_file_length-pos))
      length= (uint32_t) (key_file_length-pos);
    if (my_pread(share->kfile, (unsigned char*) buff, length, pos, MYF(MY_FAE|MY_FNABP)))
      goto err;

    if (ignore_leaves)
    {
      unsigned char *end= buff+length;
      do
      {
        if (mi_test_if_nod(buff))
        {
          if (key_cache_insert(share->getKeyCache(),
                               share->kfile, pos, DFLT_INIT_HITS,
                              (unsigned char*) buff, block_length))
	    goto err;
	}
        pos+= block_length;
      }
      while ((buff+= block_length) != end);
      buff= end-length;
    }
    else
    {
      if (key_cache_insert(share->getKeyCache(),
                           share->kfile, pos, DFLT_INIT_HITS,
                           (unsigned char*) buff, length))
	goto err;
      pos+= length;
    }
  }
  while (pos != key_file_length);

  free((char*) buff);
  return(0);

err:
  free((char*) buff);
  return(errno= errno);
}

