/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/**************************************************//**
@file include/mysql_addons.h
This file contains functions that need to be added to
MySQL code but have not been added yet.

Whenever you add a function here submit a MySQL bug
report (feature request) with the implementation. Then
write the bug number in the comment before the
function in this file.

When MySQL commits the function it can be deleted from
here. In a perfect world this file exists but is empty.

Created November 07, 2007 Vasil Dimov
*******************************************************/

#if defined(BUILD_DRIZZLE)
#if defined(__cplusplus)
extern "C"
{
#else
#include <stdbool.h>
#endif
/**
 *
  Return the thread id of a user thread

  @param session  user thread connection handle
  @return  thread id
*/
#if defined(__cplusplus)
extern "C"
#endif
unsigned long session_get_thread_id(const void *session);

/**
  Check if a user thread is running a non-transactional update
  @param session  user thread
  @retval 0 the user thread is not running a non-transactional update
  @retval 1 the user thread is running a non-transactional update
*/
int session_non_transactional_update(const void *session);

/**
  Mark transaction to rollback and mark error as fatal to a sub-statement.
  @param  session   Thread handle
  @param  all   TRUE <=> rollback main transaction.
*/
void session_mark_transaction_to_rollback(void *session, bool all);

#if defined(__cplusplus)
}
#endif
#endif
