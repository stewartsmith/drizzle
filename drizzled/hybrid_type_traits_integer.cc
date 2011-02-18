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

#include <drizzled/definitions.h>
#include <drizzled/field.h>
#include <drizzled/hybrid_type.h>
#include <drizzled/hybrid_type_traits_integer.h>
#include <drizzled/item.h>

namespace drizzled
{

/* Hybrid_type_traits_integer */
static const Hybrid_type_traits_integer integer_traits_instance;


Item_result Hybrid_type_traits_integer::type() const
{
  return INT_RESULT;
}


void
Hybrid_type_traits_integer::fix_length_and_dec(Item *item, Item *) const
{
  item->decimals= 0;
  item->max_length= MY_INT64_NUM_DECIMAL_DIGITS;
  item->unsigned_flag= 0;
}


/* Hybrid_type operations. */
void Hybrid_type_traits_integer::set_zero(Hybrid_type *val) const
{
  val->integer= 0;
}


void Hybrid_type_traits_integer::add(Hybrid_type *val, Field *f) const
{
  val->integer+= f->val_int();
}


void Hybrid_type_traits_integer::div(Hybrid_type *val, uint64_t u) const
{
  val->integer/= (int64_t) u;
}


int64_t Hybrid_type_traits_integer::val_int(Hybrid_type *val, bool) const
{
  return val->integer;
}


double Hybrid_type_traits_integer::val_real(Hybrid_type *val) const
{
  return (double) val->integer;
}


type::Decimal *Hybrid_type_traits_integer::val_decimal(Hybrid_type *val,
                                                    type::Decimal *) const
{
  int2_class_decimal(E_DEC_FATAL_ERROR, val->integer, 0, &val->dec_buf[2]);
  return &val->dec_buf[2];
}


String *Hybrid_type_traits_integer::val_str(Hybrid_type *val, String *buf,
                                            uint8_t) const
{
  buf->set(val->integer, &my_charset_bin);
  return buf;
}


const Hybrid_type_traits_integer *Hybrid_type_traits_integer::instance()
{
  return &integer_traits_instance;
}


Hybrid_type_traits_integer::Hybrid_type_traits_integer()
{}


} /* namespace drizzled */
