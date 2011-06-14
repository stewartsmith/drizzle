/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#pragma once

#include <drizzled/statement.h>

namespace drizzled {
namespace statement {

class Execute : public Statement
{
  bool is_quiet;
  bool is_concurrent;
  bool should_wait;
  drizzled::execute_string_t to_execute;

  bool parseVariable(void);

  bool runStatement(plugin::NullClient *client, const std::string &arg);

  bool execute_shell();
public:
  Execute(Session *in_session, drizzled::execute_string_t, bool is_quiet_arg, bool is_concurrent, bool should_wait);


  bool execute();
};

} /* namespace statement */

} /* namespace drizzled */

