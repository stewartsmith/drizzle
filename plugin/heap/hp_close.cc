/* Copyright (C) 2000-2002 MySQL AB

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

/* close a heap-database */

#include "heap_priv.h"
#include <cstdlib>

using namespace std;

	/* Close a database open by hp_open() */
	/* Data is normally not deallocated */

int heap_close(HP_INFO *info)
{
  int tmp;
  THR_LOCK_heap.lock();
  tmp= hp_close(info);
  THR_LOCK_heap.unlock();

  return(tmp);
}


int hp_close(HP_INFO *info)
{
  int error=0;
  info->s->changed=0;
  heap_open_list.remove(info);
  if (!--info->s->open_count && info->s->delete_on_close)
    hp_free(info->s);				/* Table was deleted */
  free((unsigned char*) info);
  return(error);
}
