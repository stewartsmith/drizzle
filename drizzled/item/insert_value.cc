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
#include <drizzled/item/insert_value.h>
#include <drizzled/item/ref.h>
#include <drizzled/item/copy_string.h>
#include <drizzled/item/default_value.h>
#include <drizzled/field/null.h>

namespace drizzled
{

bool Item_insert_value::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == INSERT_VALUE_ITEM &&
    ((Item_default_value *)item)->arg->eq(arg, binary_cmp);
}

bool Item_insert_value::fix_fields(Session *session, Item **)
{
  assert(fixed == 0);
  /* We should only check that arg is in first table */
  if (!arg->fixed)
  {
    bool res;
    TableList *orig_next_table= context->last_name_resolution_table;
    context->last_name_resolution_table= context->first_name_resolution_table;
    res= arg->fix_fields(session, &arg);
    context->last_name_resolution_table= orig_next_table;
    if (res)
      return true;
  }

  if (arg->type() == REF_ITEM)
  {
    Item_ref *ref= (Item_ref *)arg;
    if (ref->ref[0]->type() != FIELD_ITEM)
    {
      my_error(ER_BAD_FIELD_ERROR, MYF(0), "", "VALUES() function");
      return true;
    }
    arg= ref->ref[0];
  }
  /*
    According to our SQL grammar, VALUES() function can reference
    only to a column.
  */
  assert(arg->type() == FIELD_ITEM);

  Item_field *field_arg= (Item_field *)arg;

  if (field_arg->field->getTable()->insert_values.size())
  {
    Field *def_field= (Field*) memory::sql_alloc(field_arg->field->size_of());
    if (!def_field)
      return true;
    memcpy(def_field, field_arg->field, field_arg->field->size_of());
    def_field->move_field_offset((ptrdiff_t)
                                 (&def_field->getTable()->insert_values[0] - def_field->getTable()->record[0]));
    set_field(def_field);
  }
  else
  {
    Field *tmp_field= field_arg->field;
    /* charset doesn't matter here, it's to avoid sigsegv only */
    tmp_field= new Field_null(0, 0, field_arg->field->field_name);
    if (tmp_field)
    {
      tmp_field->init(field_arg->field->getTable());
      set_field(tmp_field);
    }
  }
  return false;
}


void Item_insert_value::print(String *str)
{
  str->append(STRING_WITH_LEN("values("));
  arg->print(str);
  str->append(')');
}


} /* namespace drizzled */
