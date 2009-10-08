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
