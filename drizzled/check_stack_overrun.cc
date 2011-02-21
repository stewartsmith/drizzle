/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/definitions.h>
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/check_stack_overrun.h>

namespace drizzled
{

/****************************************************************************
	Check stack size; Send error if there isn't enough stack to continue
****************************************************************************/
#if defined(STACK_DIRECTION) && (STACK_DIRECTION < 0)
static const bool stack_direction_negative = true;
#else
static const bool stack_direction_negative = false;
#endif

template <typename A_T, typename B_T>
inline static long used_stack(A_T A, B_T B)
{
  if (stack_direction_negative)
    return (long) (A - B);
  else
    return (long) (B - A);
}

extern size_t my_thread_stack_size;

bool check_stack_overrun(Session *session, long margin, void *)
{
  long stack_used;
  if ((stack_used=used_stack(session->thread_stack,(char*) &stack_used)) >=
      (long) (my_thread_stack_size - margin))
  {
    my_printf_error(ER_STACK_OVERRUN_NEED_MORE, ER(ER_STACK_OVERRUN_NEED_MORE),
                    MYF(ME_FATALERROR),
                    stack_used,my_thread_stack_size,margin);
    return true;
  }
  return false;
}

} /* namespace drizzled */
