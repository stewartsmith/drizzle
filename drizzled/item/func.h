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

#pragma once


/* If you fix the parser to no longer create functions these can be moved to create.cc */
#include <drizzled/function/math/divide.h>
#include <drizzled/function/get_user_var.h>
#include <drizzled/function/math/int.h>
#include <drizzled/function/math/int_divide.h>
#include <drizzled/function/math/minus.h>
#include <drizzled/function/math/mod.h>
#include <drizzled/function/math/multiply.h>
#include <drizzled/function/math/neg.h>
#include <drizzled/function/math/plus.h>
#include <drizzled/function/rollup_const.h>
#include <drizzled/function/math/round.h>
#include <drizzled/function/user_var_as_out_param.h>

/* For type casts */

namespace drizzled
{

enum Cast_target
{
  ITEM_CAST_BINARY,
  ITEM_CAST_SIGNED,
  ITEM_CAST_UNSIGNED,
  ITEM_CAST_DATE,
  ITEM_CAST_TIME,
  ITEM_CAST_DATETIME,
  ITEM_CAST_CHAR,
  ITEM_CAST_BOOLEAN,
  ITEM_CAST_DECIMAL
};

} /* namespace drizzled */

