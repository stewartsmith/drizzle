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

#include <plugin/slave/sql_executor.h>

namespace slave
{

class ReplicationSchema : public SQLExecutor
{
public:
  ReplicationSchema() : SQLExecutor("slave", "replication")
  { }

  bool create();

  /**
   * Set initial value of the last applied COMMIT_ID value in applier_state.
   *
   * This is used when the server is started with --slave.max-commit-id to
   * begin reading from the master transaction log at a given point. This
   * method will persist the value to the applier_state table. If it wasn't
   * permanently stored immediately, we risk the possibility of losing the
   * value if the server is again restarted without ever having received
   * another event from the master (which causes persistence of the value).
   * An edge case, but still possible.
   *
   * @param[in] value The initial value.
   */
  bool setInitialMaxCommitId(uint64_t value);
};

} /* namespace slave */

