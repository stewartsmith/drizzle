/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 David Shrewsbury
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

#include <string>
#include <vector>
#include <drizzled/session.h>

namespace slave
{

class SQLExecutor
{
public:

  SQLExecutor(const std::string &user, const std::string &schema);

  void markInErrorState()
  {
    _in_error_state= true;
  }

  void clearErrorState()
  {
    _in_error_state= false;
  }

  const std::string &getErrorMessage()
  {
    return _error_message;
  }

  /**
   * Execute a batch of SQL statements.
   *
   * @param sql Batch of SQL statements to execute.
   *
   * @retval true Success
   * @retval false Failure
   */
  bool executeSQL(std::vector<std::string> &sql);

protected:
  drizzled::Session::shared_ptr _session;

private:
  bool _in_error_state;
  std::string _error_message;

};

} /* namespace slave */

