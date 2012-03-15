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

/* Test if a record has changed since last read */

#include "heap_priv.h"
#include <drizzled/error_t.h>

int hp_rectest(register HP_INFO *info, register const unsigned char *old_record)
{

  if (hp_compare_record_data_to_chunkset(info->getShare(), old_record, info->current_ptr))
  {
    return((errno= drizzled::HA_ERR_RECORD_CHANGED)); /* Record have changed */
  }

  return(0);
} /* _heap_rectest */
