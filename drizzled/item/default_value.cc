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

#include <drizzled/error.h>
#include <drizzled/name_resolution_context.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/item/default_value.h>

namespace drizzled
{

bool Item_default_value::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == DEFAULT_VALUE_ITEM &&
    ((Item_default_value *)item)->arg->eq(arg, binary_cmp);
}


bool Item_default_value::fix_fields(Session *session, Item **)
{
  Item *real_arg;
  Item_field *field_arg;
  Field *def_field;
  assert(fixed == 0);

  if (!arg)
  {
    fixed= 1;
    return false;
  }
  if (!arg->fixed && arg->fix_fields(session, &arg))
    goto error;


  real_arg= arg->real_item();
  if (real_arg->type() != FIELD_ITEM)
  {
    my_error(ER_NO_DEFAULT_FOR_FIELD, MYF(0), arg->name);
    goto error;
  }

  field_arg= (Item_field *)real_arg;
  if (field_arg->field->flags & NO_DEFAULT_VALUE_FLAG)
  {
    my_error(ER_NO_DEFAULT_FOR_FIELD, MYF(0), field_arg->field->field_name);
    goto error;
  }
  if (!(def_field= (Field*) memory::sql_alloc(field_arg->field->size_of())))
    goto error;
  memcpy(def_field, field_arg->field, field_arg->field->size_of());
  def_field->move_field_offset((ptrdiff_t)
                               (def_field->getTable()->getDefaultValues() - def_field->getTable()->record[0]));
  set_field(def_field);
  return false;

error:
  context->process_error(session);
  return true;
}


void Item_default_value::print(String *str)
{
  if (!arg)
  {
    str->append(STRING_WITH_LEN("default"));
    return;
  }
  str->append(STRING_WITH_LEN("default("));
  arg->print(str);
  str->append(')');
}


int Item_default_value::save_in_field(Field *field_arg, bool no_conversions)
{
  if (!arg)
  {
    if (field_arg->flags & NO_DEFAULT_VALUE_FLAG)
    {
      if (field_arg->reset())
      {
        my_message(ER_CANT_CREATE_GEOMETRY_OBJECT,
                   ER(ER_CANT_CREATE_GEOMETRY_OBJECT), MYF(0));
        return -1;
      }

      {
        push_warning_printf(field_arg->getTable()->in_use,
                            DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_NO_DEFAULT_FOR_FIELD,
                            ER(ER_NO_DEFAULT_FOR_FIELD),
                            field_arg->field_name);
      }
      return 1;
    }
    field_arg->set_default();
    return 0;
  }
  return Item_field::save_in_field(field_arg, no_conversions);
}


/**
  This method like the walk method traverses the item tree, but at the
  same time it can replace some nodes in the tree.
*/

Item *Item_default_value::transform(Item_transformer transformer, unsigned char *args)
{
  Item *new_item= arg->transform(transformer, args);
  if (!new_item)
    return NULL;
  arg= new_item;
  return (this->*transformer)(args);
}


} /* namespace drizzled */
