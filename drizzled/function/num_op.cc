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

#include <cassert>

#include <drizzled/function/num_op.h>

namespace drizzled
{

/**
  Check arguments here to determine result's type for a numeric
  function of two arguments.
*/

void Item_num_op::find_num_type(void)
{
  assert(arg_count == 2);
  Item_result r0= args[0]->result_type();
  Item_result r1= args[1]->result_type();

  if (r0 == REAL_RESULT || r1 == REAL_RESULT ||
      r0 == STRING_RESULT || r1 ==STRING_RESULT)
  {
    count_real_length();
    max_length= float_length(decimals);
    hybrid_type= REAL_RESULT;
  }
  else if (r0 == DECIMAL_RESULT || r1 == DECIMAL_RESULT)
  {
    hybrid_type= DECIMAL_RESULT;
    result_precision();
  }
  else
  {
    assert(r0 == INT_RESULT && r1 == INT_RESULT);
    decimals= 0;
    hybrid_type=INT_RESULT;
    result_precision();
  }
  return;
}

} /* namespace drizzled */
