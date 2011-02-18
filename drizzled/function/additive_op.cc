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

#include <drizzled/function/additive_op.h>
#include <drizzled/type/decimal.h>

#include <algorithm>

using namespace std;

namespace drizzled
{

/**
  Set precision of results for additive operations (+ and -)
*/
void Item_func_additive_op::result_precision()
{
  decimals= max(args[0]->decimals, args[1]->decimals);
  int max_int_part= max(args[0]->decimal_precision() - args[0]->decimals,
                        args[1]->decimal_precision() - args[1]->decimals);
  int precision= min(max_int_part + 1 + decimals, DECIMAL_MAX_PRECISION);

  /* Integer operations keep unsigned_flag if one of arguments is unsigned */
  if (result_type() == INT_RESULT)
    unsigned_flag= args[0]->unsigned_flag | args[1]->unsigned_flag;
  else
    unsigned_flag= args[0]->unsigned_flag & args[1]->unsigned_flag;

  max_length= class_decimal_precision_to_length(precision, decimals,
                                             unsigned_flag);
}

} /* namespace drizzled */
