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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Rename a table
*/

#include "heap_priv.h"
#include <string.h>
#include <cstdlib>

int heap_rename(const char *old_name, const char *new_name)
{
  HP_SHARE *info;

  THR_LOCK_heap.lock();
  if ((info = hp_find_named_heap(old_name)))
  {
    info->name.clear();
    info->name.append(new_name);
  }
  THR_LOCK_heap.unlock();
  return(0);
}
