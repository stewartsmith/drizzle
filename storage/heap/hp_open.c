/* Copyright (C) 2000-2004, 2006 MySQL AB

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

/* open a heap-database */

#include "heapdef.h"
#ifdef VMS
#include "hp_static.c"			/* Stupid vms-linker */
#endif

#include <mysys/my_sys.h>

/*
  Open heap table based on HP_SHARE structure
  
  NOTE
    This doesn't register the table in the open table list.
*/

HP_INFO *heap_open_from_share(HP_SHARE *share, int mode)
{
  HP_INFO *info;

  if (!(info= (HP_INFO*) my_malloc((uint) sizeof(HP_INFO) +
				  2 * share->max_key_length,
				  MYF(MY_ZEROFILL))))
  {
    return(0);
  }
  share->open_count++; 
  thr_lock_data_init(&share->lock,&info->lock,NULL);
  info->s= share;
  info->lastkey= (uchar*) (info + 1);
  info->recbuf= (uchar*) (info->lastkey + share->max_key_length);
  info->mode= mode;
  info->current_record= (uint32_t) ~0L;		/* No current record */
  info->lastinx= info->errkey= -1;
  return(info);
}


/*
  Open heap table based on HP_SHARE structure and register it
*/

HP_INFO *heap_open_from_share_and_register(HP_SHARE *share, int mode)
{
  HP_INFO *info;

  pthread_mutex_lock(&THR_LOCK_heap);
  if ((info= heap_open_from_share(share, mode)))
  {
    info->open_list.data= (void*) info;
    heap_open_list= list_add(heap_open_list,&info->open_list);
  }
  pthread_mutex_unlock(&THR_LOCK_heap);
  return(info);
}


/*
  Open heap table based on name

  NOTE
    This register the table in the open table list. so that it can be
    found by future heap_open() calls.
*/

HP_INFO *heap_open(const char *name, int mode)
{
  HP_INFO *info;
  HP_SHARE *share;

  pthread_mutex_lock(&THR_LOCK_heap);
  if (!(share= hp_find_named_heap(name)))
  {
    my_errno= ENOENT;
    pthread_mutex_unlock(&THR_LOCK_heap);
    return(0);
  }
  if ((info= heap_open_from_share(share, mode)))
  {
    info->open_list.data= (void*) info;
    heap_open_list= list_add(heap_open_list,&info->open_list);
  }
  pthread_mutex_unlock(&THR_LOCK_heap);
  return(info);
}


/* map name to a heap-nr. If name isn't found return 0 */

HP_SHARE *hp_find_named_heap(const char *name)
{
  LIST *pos;
  HP_SHARE *info;

  for (pos= heap_share_list; pos; pos= pos->next)
  {
    info= (HP_SHARE*) pos->data;
    if (!strcmp(name, info->name))
    {
      return(info);
    }
  }
  return((HP_SHARE *) 0);
}


