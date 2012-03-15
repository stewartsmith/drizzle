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

#include <drizzled/select_result_interceptor.h>
#include <drizzled/tmp_table_param.h>

namespace drizzled
{

class select_union :public select_result_interceptor
{
  Tmp_Table_Param tmp_table_param;
public:
  Table *table;

  select_union() :table(0) { }
  ~select_union() { }

  int prepare(List<Item> &list, Select_Lex_Unit *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  bool flush();
  void cleanup();
  bool create_result_table(Session *session, List<Item> *column_types,
                           bool is_distinct, uint64_t options,
                           const char *alias);
};

} /* namespace drizzled */

