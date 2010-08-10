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

#include "heap_priv.h"

#include <string.h>
#include <cstdlib>

using namespace std;

/*
  Open heap table based on HP_SHARE structure

  NOTE
    This doesn't register the table in the open table list.
*/

HP_INFO *heap_open_from_share(HP_SHARE *share, int mode)
{
  HP_INFO *info;

  if (!(info= (HP_INFO*) malloc(sizeof(HP_INFO) + 2 * share->max_key_length)))
  {
    return(0);
  }
  memset(info, 0, sizeof(HP_INFO) + 2 * share->max_key_length);
  share->open_count++;
  info->lock.init(&share->lock);
  info->setShare(share);
  info->lastkey= (unsigned char*) (info + 1);
  info->recbuf= (unsigned char*) (info->lastkey + share->max_key_length);
  info->mode= mode;
  info->current_record= UINT32_MAX;		/* No current record */
  info->lastinx= info->errkey= -1;
  return(info);
}


/*
  Open heap table based on HP_SHARE structure and register it
*/

HP_INFO *heap_open_from_share_and_register(HP_SHARE *share, int mode)
{
  HP_INFO *info;

  THR_LOCK_heap.lock();
  if ((info= heap_open_from_share(share, mode)))
  {
    heap_open_list.push_front(info);
  }
  THR_LOCK_heap.unlock();
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

  THR_LOCK_heap.lock();
  if (!(share= hp_find_named_heap(name)))
  {
    errno= ENOENT;
    THR_LOCK_heap.unlock();
    return(0);
  }
  if ((info= heap_open_from_share(share, mode)))
  {
    heap_open_list.push_front(info);
  }
  THR_LOCK_heap.unlock();
  return(info);
}


/* map name to a heap-nr. If name isn't found return 0 */

HP_SHARE *hp_find_named_heap(const char *name)
{
  list<HP_SHARE *>::iterator it= heap_share_list.begin();
  while (it != heap_share_list.end())
  {
    if (!strcmp(name, (*it)->name))
    {
      return (*it);
    }
    ++it;
  }
  return((HP_SHARE *) 0);
}


