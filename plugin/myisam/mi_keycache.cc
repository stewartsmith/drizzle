/* Copyright (C) 2003 MySQL AB

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

/*
  Key cache assignments
*/

#include "myisamdef.h"

using namespace std;

/*
  Assign pages of the index file for a table to a key cache

  SYNOPSIS
    mi_assign_to_key_cache()
      info          open table
      key_cache_ptr pointer to the key cache handle
      assign_lock   Mutex to lock during assignment

  PREREQUESTS
    One must have a READ lock or a WRITE lock on the table when calling
    the function to ensure that there is no other writers to it.

    The caller must also ensure that one doesn't call this function from
    two different threads with the same table.

  NOTES
    At present pages for all indexes must be assigned to the same key cache.

  RETURN VALUE
    0  If a success
    #  Error code
*/

int mi_assign_to_key_cache(MI_INFO *, KEY_CACHE *) 
{
  return 0;
}


/*
  Change all MyISAM entries that uses one key cache to another key cache

  SYNOPSIS
    mi_change_key_cache()
    old_key_cache	Old key cache
    new_key_cache	New key cache

  NOTES
    This is used when we delete one key cache.

    To handle the case where some other threads tries to open an MyISAM
    table associated with the to-be-deleted key cache while this operation
    is running, we have to call 'multi_key_cache_change()' from this
    function while we have a lock on the MyISAM table list structure.

    This is safe as long as it's only MyISAM that is using this specific
    key cache.
*/


void mi_change_key_cache(KEY_CACHE *, KEY_CACHE *)
{
}
