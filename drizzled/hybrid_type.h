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

#include <drizzled/type/decimal.h>

namespace drizzled {

/*************************************************************************/
/*
  A framework to easily handle different return types for hybrid items
  (hybrid item is an item whose operand can be of any type, e.g. integer,
  real, decimal).
*/

class Hybrid_type
{
public:
  int64_t integer;

  double real;
  /*
    Use two decimal buffers interchangeably to speed up += operation
    which has no native support in decimal library.
    Hybrid_type+= arg is implemented as dec_buf[1]= dec_buf[0] + arg.
    The third decimal is used as a handy temporary storage.
  */
  type::Decimal dec_buf[3];
  int used_dec_buf_no;

  /*
    Traits moved to a separate class to
      a) be able to easily change object traits in runtime
      b) they work as a differentiator for the union above
  */
  const Hybrid_type_traits *traits;

  Hybrid_type() {}
  /* XXX: add traits->copy() when needed */
  Hybrid_type(const Hybrid_type &rhs) :traits(rhs.traits) {}
};

} /* namespace drizzled */

