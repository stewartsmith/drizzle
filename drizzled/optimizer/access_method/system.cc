/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Padraig O'Sullivan
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <drizzled/join_table.h>
#include <drizzled/sql_select.h>
#include <drizzled/optimizer/access_method/system.h>
#include <drizzled/table.h>

using namespace drizzled;

void optimizer::System::getStats(Table& table, JoinTable& join_tab)
{
  table.status= STATUS_NO_RECORD;
  join_tab.read_first_record= reinterpret_cast<Read_record_func>(join_tab.joinReadSystem());
  join_tab.read_record.read_record= join_no_more_records;
}
