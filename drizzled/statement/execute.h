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

#ifndef DRIZZLED_STATEMENT_EXECUTE_H
#define DRIZZLED_STATEMENT_EXECUTE_H

#include <drizzled/statement.h>

namespace drizzled
{
class Session;

namespace statement
{

class Execute : public Statement
{
  bool is_var;
  LEX_STRING to_execute;

  bool parseVariable(void);

public:
  Execute(Session *in_session);

  void setQuery(LEX_STRING &arg)
  {
    to_execute= arg;
  }

  void setVar()
  {
    is_var= true;
  }

  bool execute();
};

} /* namespace statement */

} /* namespace drizzled */

#endif /* DRIZZLED_STATEMENT_EXECUTE_H */
