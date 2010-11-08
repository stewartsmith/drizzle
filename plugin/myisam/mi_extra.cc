/* Copyright (C) 2000-2005 MySQL AB

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

#include "myisam_priv.h"
#include <drizzled/util/test.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <string.h>
#include <algorithm>

using namespace drizzled;
using namespace std;

static void mi_extra_keyflag(MI_INFO *info, enum ha_extra_function function);


/*
  Set options and buffers to optimize table handling

  SYNOPSIS
    mi_extra()
    info	open table
    function	operation
    extra_arg	Pointer to extra argument (normally pointer to uint32_t)
    		Used when function is one of:
		HA_EXTRA_WRITE_CACHE
		HA_EXTRA_CACHE
  RETURN VALUES
    0  ok
    #  error
*/

int mi_extra(MI_INFO *info, enum ha_extra_function function, void *extra_arg)
{
  int error=0;
  uint32_t cache_size;
  MYISAM_SHARE *share=info->s;

  switch (function) {
  case HA_EXTRA_RESET_STATE:		/* Reset state (don't free buffers) */
    info->lastinx= 0;			/* Use first index as def */
    info->last_search_keypage=info->lastpos= HA_OFFSET_ERROR;
    info->page_changed=1;
					/* Next/prev gives first/last */
    if (info->opt_flag & READ_CACHE_USED)
    {
      info->rec_cache.reinit_io_cache(internal::READ_CACHE,0,
                                      (bool) (info->lock_type != F_UNLCK),
                                      (bool) test(info->update & HA_STATE_ROW_CHANGED));
    }
    info->update= ((info->update & HA_STATE_CHANGED) | HA_STATE_NEXT_FOUND |
		   HA_STATE_PREV_FOUND);
    break;
  case HA_EXTRA_CACHE:
    if (info->lock_type == F_UNLCK &&
	(share->options & HA_OPTION_PACK_RECORD))
    {
      error=1;			/* Not possibly if not locked */
      errno=EACCES;
      break;
    }
    if (info->s->file_map) /* Don't use cache if mmap */
      break;
    if (info->opt_flag & WRITE_CACHE_USED)
    {
      info->opt_flag&= ~WRITE_CACHE_USED;
      if ((error= info->rec_cache.end_io_cache()))
	break;
    }
    if (!(info->opt_flag &
	  (READ_CACHE_USED | WRITE_CACHE_USED | MEMMAP_USED)))
    {
      cache_size= (extra_arg ? *(uint32_t*) extra_arg :
		   internal::my_default_record_cache_size);
      if (!(info->rec_cache.init_io_cache(info->dfile, (uint) min((uint32_t)info->state->data_file_length+1, cache_size),
                                          internal::READ_CACHE,0L,(bool) (info->lock_type != F_UNLCK),
                                          MYF(share->write_flag & MY_WAIT_IF_FULL))))
      {
	info->opt_flag|=READ_CACHE_USED;
	info->update&= ~HA_STATE_ROW_CHANGED;
      }
      if (share->concurrent_insert)
	info->rec_cache.end_of_file=info->state->data_file_length;
    }
    break;
  case HA_EXTRA_REINIT_CACHE:
    if (info->opt_flag & READ_CACHE_USED)
    {
      info->rec_cache.reinit_io_cache(internal::READ_CACHE,info->nextpos,
		      (bool) (info->lock_type != F_UNLCK),
		      (bool) test(info->update & HA_STATE_ROW_CHANGED));
      info->update&= ~HA_STATE_ROW_CHANGED;
      if (share->concurrent_insert)
	info->rec_cache.end_of_file=info->state->data_file_length;
    }
    break;
  case HA_EXTRA_WRITE_CACHE:
    if (info->lock_type == F_UNLCK)
    {
      error=1;			/* Not possibly if not locked */
      break;
    }

    cache_size= (extra_arg ? *(uint32_t*) extra_arg :
		 internal::my_default_record_cache_size);
    if (not (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED | OPT_NO_ROWS)) && !share->state.header.uniques)
    {
      if (not (info->rec_cache.init_io_cache(info->dfile, cache_size,
                                             internal::WRITE_CACHE,info->state->data_file_length,
                                             (bool) (info->lock_type != F_UNLCK),
                                             MYF(share->write_flag & MY_WAIT_IF_FULL))))
      {
        info->opt_flag|=WRITE_CACHE_USED;
        info->update&= ~(HA_STATE_ROW_CHANGED |
			 HA_STATE_WRITE_AT_END |
			 HA_STATE_EXTEND_BLOCK);
      }
    }
    break;
  case HA_EXTRA_PREPARE_FOR_UPDATE:
    if (info->s->data_file_type != DYNAMIC_RECORD)
      break;
    /* Remove read/write cache if dynamic rows */
  case HA_EXTRA_NO_CACHE:
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      error= info->rec_cache.end_io_cache();
      /* Sergei will insert full text index caching here */
    }
#if !defined(TARGET_OS_SOLARIS)
    if (info->opt_flag & MEMMAP_USED)
      madvise((char*) share->file_map, share->state.state.data_file_length,
              MADV_RANDOM);
#endif
    break;
  case HA_EXTRA_FLUSH_CACHE:
    if (info->opt_flag & WRITE_CACHE_USED)
    {
      if ((error=flush_io_cache(&info->rec_cache)))
      {
        mi_print_error(info->s, HA_ERR_CRASHED);
	mi_mark_crashed(info);			/* Fatal error found */
      }
    }
    break;
  case HA_EXTRA_NO_READCHECK:
    info->opt_flag&= ~READ_CHECK_USED;		/* No readcheck */
    break;
  case HA_EXTRA_READCHECK:
    info->opt_flag|= READ_CHECK_USED;
    break;
  case HA_EXTRA_KEYREAD:			/* Read only keys to record */
  case HA_EXTRA_REMEMBER_POS:
    info->opt_flag |= REMEMBER_OLD_POS;
    memmove(info->lastkey+share->base.max_key_length*2,
            info->lastkey,info->lastkey_length);
    info->save_update=	info->update;
    info->save_lastinx= info->lastinx;
    info->save_lastpos= info->lastpos;
    info->save_lastkey_length=info->lastkey_length;
    if (function == HA_EXTRA_REMEMBER_POS)
      break;
    /* fall through */
  case HA_EXTRA_KEYREAD_CHANGE_POS:
    info->opt_flag |= KEY_READ_USED;
    info->read_record=_mi_read_key_record;
    break;
  case HA_EXTRA_NO_KEYREAD:
  case HA_EXTRA_RESTORE_POS:
    if (info->opt_flag & REMEMBER_OLD_POS)
    {
      memmove(info->lastkey,
              info->lastkey+share->base.max_key_length*2,
              info->save_lastkey_length);
      info->update=	info->save_update | HA_STATE_WRITTEN;
      info->lastinx=	info->save_lastinx;
      info->lastpos=	info->save_lastpos;
      info->lastkey_length=info->save_lastkey_length;
    }
    info->read_record=	share->read_record;
    info->opt_flag&= ~(KEY_READ_USED | REMEMBER_OLD_POS);
    break;
  case HA_EXTRA_NO_USER_CHANGE: /* Database is somehow locked agains changes */
    info->lock_type= F_EXTRA_LCK; /* Simulate as locked */
    break;
  case HA_EXTRA_WAIT_LOCK:
    info->lock_wait=0;
    break;
  case HA_EXTRA_NO_WAIT_LOCK:
    info->lock_wait=MY_DONT_WAIT;
    break;
  case HA_EXTRA_NO_KEYS:
    if (info->lock_type == F_UNLCK)
    {
      error=1;					/* Not possibly if not lock */
      break;
    }
    if (mi_is_any_key_active(share->state.key_map))
    {
      MI_KEYDEF *key=share->keyinfo;
      uint32_t i;
      for (i=0 ; i < share->base.keys ; i++,key++)
      {
        if (!(key->flag & HA_NOSAME) && info->s->base.auto_key != i+1)
        {
          mi_clear_key_active(share->state.key_map, i);
          info->update|= HA_STATE_CHANGED;
        }
      }

      if (!share->changed)
      {
	share->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED;
	share->changed=1;			/* Update on close */
	if (!share->global_changed)
	{
	  share->global_changed=1;
	  share->state.open_count++;
	}
      }
      share->state.state= *info->state;
      error=mi_state_info_write(share->kfile,&share->state,1 | 2);
    }
    break;
  case HA_EXTRA_FORCE_REOPEN:
    THR_LOCK_myisam.lock();
    share->last_version= 0L;			/* Impossible version */
    THR_LOCK_myisam.unlock();
    break;
  case HA_EXTRA_PREPARE_FOR_DROP:
    THR_LOCK_myisam.lock();
    share->last_version= 0L;			/* Impossible version */
#ifdef __WIN__REMOVE_OBSOLETE_WORKAROUND
    /* Close the isam and data files as Win32 can't drop an open table */
    if (flush_key_blocks(share->key_cache, share->kfile,
			 (function == HA_EXTRA_FORCE_REOPEN ?
			  FLUSH_RELEASE : FLUSH_IGNORE_CHANGED)))
    {
      error=errno;
      share->changed=1;
      mi_print_error(info->s, HA_ERR_CRASHED);
      mi_mark_crashed(info);			/* Fatal error found */
    }
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      error=end_io_cache(&info->rec_cache);
    }
    if (info->lock_type != F_UNLCK && ! info->was_locked)
    {
      info->was_locked=info->lock_type;
      if (mi_lock_database(info,F_UNLCK))
	error=errno;
      info->lock_type = F_UNLCK;
    }
    if (share->kfile >= 0)
      _mi_decrement_open_count(info);
    if (share->kfile >= 0 && internal::my_close(share->kfile,MYF(0)))
      error=errno;
    {
      list<MI_INFO *>::iterator it= myisam_open_list.begin();
      while (it != myisam_open_list.end())
      {
	MI_INFO *tmpinfo= *it;
	if (tmpinfo->s == info->s)
	{
	  if (tmpinfo->dfile >= 0 && internal::my_close(tmpinfo->dfile,MYF(0)))
	    error = errno;
	  tmpinfo->dfile= -1;
	}
        ++it;
      }
    }
    share->kfile= -1;				/* Files aren't open anymore */
#endif
    THR_LOCK_myisam.unlock();
    break;
  case HA_EXTRA_FLUSH:
    if (!share->temporary)
      flush_key_blocks(share->getKeyCache(), share->kfile, FLUSH_KEEP);
#ifdef HAVE_PWRITE
    _mi_decrement_open_count(info);
#endif
    if (share->not_flushed)
    {
      share->not_flushed= false;
    }
    if (share->base.blobs)
      mi_alloc_rec_buff(info, SIZE_MAX, &info->rec_buff);
    break;
  case HA_EXTRA_NORMAL:				/* Theese isn't in use */
    info->quick_mode=0;
    break;
  case HA_EXTRA_QUICK:
    info->quick_mode=1;
    break;
  case HA_EXTRA_NO_ROWS:
    if (!share->state.header.uniques)
      info->opt_flag|= OPT_NO_ROWS;
    break;
  case HA_EXTRA_PRELOAD_BUFFER_SIZE:
    info->preload_buff_size= *((uint32_t *) extra_arg);
    break;
  case HA_EXTRA_CHANGE_KEY_TO_UNIQUE:
  case HA_EXTRA_CHANGE_KEY_TO_DUP:
    mi_extra_keyflag(info, function);
    break;
  case HA_EXTRA_KEY_CACHE:
  case HA_EXTRA_NO_KEY_CACHE:
  default:
    break;
  }

  return(error);
} /* mi_extra */


/*
    Start/Stop Inserting Duplicates Into a Table, WL#1648.
 */
static void mi_extra_keyflag(MI_INFO *info, enum ha_extra_function function)
{
  uint32_t  idx;

  for (idx= 0; idx< info->s->base.keys; idx++)
  {
    switch (function) {
    case HA_EXTRA_CHANGE_KEY_TO_UNIQUE:
      info->s->keyinfo[idx].flag|= HA_NOSAME;
      break;
    case HA_EXTRA_CHANGE_KEY_TO_DUP:
      info->s->keyinfo[idx].flag&= ~(HA_NOSAME);
      break;
    default:
      break;
    }
  }
}


int mi_reset(MI_INFO *info)
{
  int error= 0;
  MYISAM_SHARE *share=info->s;
  /*
    Free buffers and reset the following flags:
    EXTRA_CACHE, EXTRA_WRITE_CACHE, EXTRA_KEYREAD, EXTRA_QUICK

    If the row buffer cache is large (for dynamic tables), reduce it
    to save memory.
  */
  if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
  {
    info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
    error= info->rec_cache.end_io_cache();
  }
  if (share->base.blobs)
    mi_alloc_rec_buff(info, SIZE_MAX, &info->rec_buff);
#if !defined(TARGET_OS_SOLARIS)
  if (info->opt_flag & MEMMAP_USED)
    madvise((char*) share->file_map, share->state.state.data_file_length,
            MADV_RANDOM);
#endif
  info->opt_flag&= ~(KEY_READ_USED | REMEMBER_OLD_POS);
  info->quick_mode=0;
  info->lastinx= 0;			/* Use first index as def */
  info->last_search_keypage= info->lastpos= HA_OFFSET_ERROR;
  info->page_changed= 1;
  info->update= ((info->update & HA_STATE_CHANGED) | HA_STATE_NEXT_FOUND |
                 HA_STATE_PREV_FOUND);
  return(error);
}
