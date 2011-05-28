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
#include <drizzled/item/null.h>
#include <drizzled/lex_string.h>
#include <drizzled/plugin/client.h>

namespace drizzled
{

bool Item_null::eq(const Item *item, bool) const
{ return item->type() == type(); }

double Item_null::val_real()
{
  // following assert is redundant, because fixed=1 assigned in constructor
  assert(fixed == 1);
  null_value=1;
  return 0.0;
}
int64_t Item_null::val_int()
{
  // following assert is redundant, because fixed=1 assigned in constructor
  assert(fixed == 1);
  null_value=1;
  return 0;
}
/* ARGSUSED */
String *Item_null::val_str(String *)
{
  // following assert is redundant, because fixed=1 assigned in constructor
  assert(fixed == 1);
  null_value=1;
  return 0;
}

type::Decimal *Item_null::val_decimal(type::Decimal *)
{
  return 0;
}


void Item_null::print(String *str)
{
  str->append(STRING_WITH_LEN("NULL"));
}


Item *Item_null::safe_charset_converter(const charset_info_st * const tocs)
{
  collation.set(tocs);
  return this;
}

/**
  Store null in field.

  This is used on INSERT.
  Allow NULL to be inserted in timestamp and auto_increment values.

  @param field          Field where we want to store NULL

  @retval
    0   ok
  @retval
    1   Field doesn't support NULL values and can't handle 'field = NULL'
*/

int Item_null::save_in_field(Field *field, bool no_conversions)
{
  return set_field_to_null_with_conversions(field, no_conversions);
}

/**
  Store null in field.

  @param field          Field where we want to store NULL

  @retval
    0    OK
  @retval
    1    Field doesn't support NULL values
*/

int Item_null::save_safe_in_field(Field *field)
{
  return set_field_to_null(field);
}

/**
  Pack data in buffer for sending.
*/

void Item_null::send(plugin::Client *client, String *)
{
  client->store();
}

} /* namespace drizzled */
