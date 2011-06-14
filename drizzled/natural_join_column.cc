/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <drizzled/natural_join_column.h>
#include <drizzled/table_list.h>
#include <drizzled/session.h>
#include <drizzled/sql_lex.h>

namespace drizzled {

Natural_join_column::Natural_join_column(Field *field_param,
                                         TableList *tab)
{
  assert(tab->table == field_param->getTable());
  table_field= field_param;
  table_ref= tab;
  is_common= false;
}


const char *Natural_join_column::name()
{
  return table_field->field_name;
}


Item *Natural_join_column::create_item(Session *session)
{
  return new Item_field(session, &session->lex().current_select->context, table_field);
}


Field *Natural_join_column::field()
{
  return table_field;
}


const char *Natural_join_column::table_name()
{
  assert(table_ref);
  return table_ref->alias;
}


const char *Natural_join_column::db_name()
{
  /*
    Test that TableList::db is the same as TableShare::db to
    ensure consistency. 
  */
  assert(!strcmp(table_ref->getSchemaName(), table_ref->table->getShare()->getSchemaName()));

  return table_ref->getSchemaName();
}

} /* namespace drizzled */
