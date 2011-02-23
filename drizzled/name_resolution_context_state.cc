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
#include <drizzled/name_resolution_context.h>

#include <drizzled/name_resolution_context_state.h>
#include <drizzled/table_list.h>

namespace drizzled
{

void
Name_resolution_context_state::save_state(Name_resolution_context *context,
                                          TableList *table_list)
{
  save_table_list=                  context->table_list;
  save_first_name_resolution_table= context->first_name_resolution_table;
  save_resolve_in_select_list=      context->resolve_in_select_list;
  save_next_local=                  table_list->next_local;
  save_next_name_resolution_table=  table_list->next_name_resolution_table;
}

void
Name_resolution_context_state::restore_state(Name_resolution_context *context,
                                             TableList *table_list)
{
  table_list->next_local=                save_next_local;
  table_list->next_name_resolution_table= save_next_name_resolution_table;
  context->table_list=                   save_table_list;
  context->first_name_resolution_table=  save_first_name_resolution_table;
  context->resolve_in_select_list=       save_resolve_in_select_list;
}


TableList *Name_resolution_context_state::get_first_name_resolution_table()
{
  return save_first_name_resolution_table;
}

} /* namespace drizzled */
