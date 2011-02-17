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

#include <drizzled/field.h>
#include <drizzled/hybrid_type.h>
#include <drizzled/hybrid_type_traits.h>
#include <drizzled/item.h>

#include <math.h>

namespace drizzled
{

static const Hybrid_type_traits real_traits_instance;

Item_result Hybrid_type_traits::type() const
{
  return REAL_RESULT;
}


void Hybrid_type_traits::fix_length_and_dec(Item *item, Item *arg) const
{
  item->decimals= NOT_FIXED_DEC;
  item->max_length= item->float_length(arg->decimals);
}


/* Hybrid_type operations. */
void Hybrid_type_traits::set_zero(Hybrid_type *val) const
{
  val->real= 0.0;
}


void Hybrid_type_traits::add(Hybrid_type *val, Field *f) const
{
  val->real+= f->val_real();
}


void Hybrid_type_traits::div(Hybrid_type *val, uint64_t u) const
{
  val->real/= uint64_t2double(u);
}


int64_t Hybrid_type_traits::val_int(Hybrid_type *val,
                                    bool) const
{
  return (int64_t) rint(val->real);
}


double Hybrid_type_traits::val_real(Hybrid_type *val) const
{
  return val->real;
}


type::Decimal *
Hybrid_type_traits::val_decimal(Hybrid_type *val, type::Decimal *) const
{
  double2_class_decimal(E_DEC_FATAL_ERROR, val->real, val->dec_buf);
  return val->dec_buf;
}


String *
Hybrid_type_traits::val_str(Hybrid_type *val, String *to,
                            uint8_t decimals) const
{
  to->set_real(val->real, decimals, &my_charset_bin);
  return to;
}


const Hybrid_type_traits *Hybrid_type_traits::instance()
{
  return &real_traits_instance;
}


Hybrid_type_traits::Hybrid_type_traits()
{}

Hybrid_type_traits::~Hybrid_type_traits()
{}

} /* namespace drizzled */
