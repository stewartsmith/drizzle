/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_SERVER_ITEM_FUNC_H
#define DRIZZLED_SERVER_ITEM_FUNC_H

/* Function items used by mysql */


#ifdef HAVE_IEEEFP_H
extern "C"				/* Bug in BSDI include file */
{
#include <ieeefp.h>
}
#endif

#include <drizzled/functions/func.h>
#include <drizzled/functions/additive_op.h>
#include <drizzled/functions/connection_id.h>
#include <drizzled/functions/decimal_typecast.h>
#include <drizzled/functions/divide.h>
#include <drizzled/functions/get_system_var.h>
#include <drizzled/functions/int.h>
#include <drizzled/functions/bit.h>
#include <drizzled/functions/bit_count.h>
#include <drizzled/functions/bit_length.h>
#include <drizzled/functions/field.h>
#include <drizzled/functions/find_in_set.h>
#include <drizzled/functions/found_rows.h>
#include <drizzled/functions/get_user_var.h>
#include <drizzled/functions/get_variable.h>
#include <drizzled/functions/integer.h>
#include <drizzled/functions/int_divide.h>
#include <drizzled/functions/length.h>
#include <drizzled/functions/lock.h>
#include <drizzled/functions/master_pos_wait.h>
#include <drizzled/functions/min_max.h>
#include <drizzled/functions/minus.h>
#include <drizzled/functions/mod.h>
#include <drizzled/functions/multiply.h>
#include <drizzled/functions/neg.h>
#include <drizzled/functions/numhybrid.h>
#include <drizzled/functions/num_op.h>
#include <drizzled/functions/num1.h>
#include <drizzled/functions/abs.h>
#include <drizzled/functions/plus.h>
#include <drizzled/functions/real.h>
#include <drizzled/functions/rollup_const.h>
#include <drizzled/functions/row_count.h>
#include <drizzled/functions/set_user_var.h>
#include <drizzled/functions/dec.h>
#include <drizzled/functions/int_val.h>
#include <drizzled/functions/acos.h>
#include <drizzled/functions/ascii.h>
#include <drizzled/functions/asin.h>
#include <drizzled/functions/atan.h>
#include <drizzled/functions/benchmark.h>
#include <drizzled/functions/char_length.h>
#include <drizzled/functions/ceiling.h>
#include <drizzled/functions/cos.h>
#include <drizzled/functions/exp.h>
#include <drizzled/functions/floor.h>
#include <drizzled/functions/last_insert.h>
#include <drizzled/functions/ln.h>
#include <drizzled/functions/log.h>
#include <drizzled/functions/units.h>
#include <drizzled/functions/ord.h>
#include <drizzled/functions/pow.h>
#include <drizzled/functions/rand.h>
#include <drizzled/functions/round.h>
#include <drizzled/functions/sin.h>
#include <drizzled/functions/sqrt.h>
#include <drizzled/functions/sign.h>
#include <drizzled/functions/signed.h>
#include <drizzled/functions/tan.h>
#include <drizzled/functions/update_hash.h>
#include <drizzled/functions/user_var_as_out_param.h>
#include <drizzled/functions/unsigned.h>

/* For type casts */

enum Cast_target
{
  ITEM_CAST_BINARY, ITEM_CAST_SIGNED_INT, ITEM_CAST_UNSIGNED_INT,
  ITEM_CAST_DATE, ITEM_CAST_TIME, ITEM_CAST_DATETIME, ITEM_CAST_CHAR,
  ITEM_CAST_DECIMAL
};

#endif /* DRIZZLE_SERVER_ITEM_FUNC_H */
