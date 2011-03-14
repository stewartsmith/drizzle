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

/*
  The following class is used to optimize comparing of date and bigint columns
  We need to save the original item ('ref') to be able to call
  ref->save_in_field(). This is used to create index search keys.

  An instance of Item_int_with_ref may have signed or unsigned integer value.

*/

namespace drizzled
{

class Item_int_with_ref :public Item_int
{
  Item *ref;
public:
  Item_int_with_ref(int64_t i, Item *ref_arg, bool unsigned_arg) :
    Item_int(i), ref(ref_arg)
  {
    unsigned_flag= unsigned_arg;
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    return ref->save_in_field(field, no_conversions);
  }
  Item *clone_item();
  virtual Item *real_item() { return ref; }
  virtual const Item *real_item() const { return ref; }
};

} /* namespace drizzled */

