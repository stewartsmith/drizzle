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

#include <drizzled/error.h>
#include <drizzled/function/set_user_var.h>
#include <drizzled/select_result_interceptor.h>
#include <drizzled/base.h>

#include <vector>

namespace drizzled {

class select_dumpvar :public select_result_interceptor {
  ha_rows row_count;

public:
  std::vector<var *> var_list;
  select_dumpvar()  { var_list.clear(); row_count= 0;}
  ~select_dumpvar() {}

  int prepare(List<Item> &list, Select_Lex_Unit *u);

  void cleanup()
  {
    row_count= 0;
  }


  bool send_data(List<Item> &items);

  bool send_eof();

};

} /* namespace drizzled */

