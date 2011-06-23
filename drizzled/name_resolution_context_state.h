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

namespace drizzled {

/*
  Store and restore the current state of a name resolution context.
*/

class Name_resolution_context_state
{
private:
  TableList *save_table_list;
  TableList *save_first_name_resolution_table;
  TableList *save_next_name_resolution_table;
  bool        save_resolve_in_select_list;
  TableList *save_next_local;

public:
  /* Save the state of a name resolution context. */
  void save_state(Name_resolution_context *context, TableList *table_list);

  /* Restore a name resolution context from saved state. */
  void restore_state(Name_resolution_context *context, TableList *table_list);

  TableList *get_first_name_resolution_table();

};

} /* namespace drizzled */

