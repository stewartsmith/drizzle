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

#include <drizzled/item/uint.h>
#include <drizzled/item/outer_ref.h>

namespace drizzled
{

/*
  Prepare referenced outer field then call usual Item_direct_ref::fix_fields

  SYNOPSIS
    Item_outer_ref::fix_fields()
    session         thread handler
    reference   reference on reference where this item stored

  RETURN
    false   OK
    true    Error
*/

bool Item_outer_ref::fix_fields(Session *session, Item **reference)
{
  bool err;
  /* outer_ref->check_cols() will be made in Item_direct_ref::fix_fields */
  if ((*ref) && !(*ref)->fixed && ((*ref)->fix_fields(session, reference)))
    return true;
  err= Item_direct_ref::fix_fields(session, reference);
  if (!outer_ref)
    outer_ref= *ref;
  if ((*ref)->type() == Item::FIELD_ITEM)
    table_name= ((Item_field*)outer_ref)->table_name;
  return err;
}

void Item_outer_ref::fix_after_pullout(Select_Lex *new_parent,
                                       Item **ref_item)
{
  if (depended_from == new_parent)
  {
    *ref_item= outer_ref;
    outer_ref->fix_after_pullout(new_parent, ref_item);
  }
}


} /* namespace drizzled */
