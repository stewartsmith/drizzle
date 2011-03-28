/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/common_fwd.h>

namespace drizzled {

/** The states in which a nested loop join can be in */
enum enum_nested_loop_state
{
  NESTED_LOOP_KILLED= -2,
  NESTED_LOOP_ERROR= -1,
  NESTED_LOOP_OK= 0,
  NESTED_LOOP_NO_MORE_ROWS= 1,
  NESTED_LOOP_QUERY_LIMIT= 3,
  NESTED_LOOP_CURSOR_LIMIT= 4
};

typedef enum_nested_loop_state (*Next_select_func)(Join *, JoinTable *, bool);
typedef int (*Read_record_func)(JoinTable *tab);
Next_select_func setup_end_select_func(Join *join);

} /* namespace drizzled */

