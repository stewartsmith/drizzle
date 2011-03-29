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

#include <drizzled/base.h>
#include <drizzled/error_t.h>
#include <drizzled/sql_error.h>
#include <drizzled/sql_list.h>

namespace drizzled {

/**
  Stores status of the currently executed statement.
  Cleared at the beginning of the statement, and then
  can hold either OK, ERROR, or EOF status.
  Can not be assigned twice per statement.
*/
class Diagnostics_area
{
public:
  enum enum_diagnostics_status
  {
    /** The area is cleared at start of a statement. */
    DA_EMPTY= 0,
    /** Set whenever one calls my_ok(). */
    DA_OK,
    /** Set whenever one calls my_eof(). */
    DA_EOF,
    /** Set whenever one calls my_error() or my_message(). */
    DA_ERROR,
    /** Set in case of a custom response, such as one from COM_STMT_PREPARE. */
    DA_DISABLED
  };
  /** True if status information is sent to the client. */
  bool is_sent;
  /** Set to make set_error_status after set_{ok,eof}_status possible. */
  bool can_overwrite_status;

  void set_ok_status(Session *session, ha_rows affected_rows_arg,
                     ha_rows found_rows_arg, uint64_t last_insert_id_arg,
                     const char *message);
  void set_eof_status(Session *session);
  void set_error_status(drizzled::error_t sql_errno_arg, const char *message_arg);

  void disable_status();

  void reset_diagnostics_area();

  bool is_set() const { return m_status != DA_EMPTY; }
  bool is_error() const { return m_status == DA_ERROR; }
  bool is_eof() const { return m_status == DA_EOF; }
  bool is_ok() const { return m_status == DA_OK; }
  bool is_disabled() const { return m_status == DA_DISABLED; }
  enum_diagnostics_status status() const { return m_status; }

  const char *message() const;
  drizzled::error_t sql_errno() const;
  uint32_t server_status() const;
  ha_rows affected_rows() const;
  ha_rows found_rows() const;
  uint64_t last_insert_id() const;
  uint32_t total_warn_count() const;

  List<DRIZZLE_ERROR> m_warn_list;

  Diagnostics_area() { reset_diagnostics_area(); }

private:
  /** Message buffer. Can be used by OK or ERROR status. */
  char m_message[DRIZZLE_ERRMSG_SIZE];
  /**
    SQL error number. One of ER_ codes from share/errmsg.txt.
    Set by set_error_status.
  */
  drizzled::error_t m_sql_errno;

  /**
    Copied from session->server_status when the diagnostics area is assigned.
    We need this member as some places in the code use the following pattern:
    session->server_status|= ...
    my_eof(session);
    session->server_status&= ~...
    Assigned by OK, EOF or ERROR.
  */
  uint32_t m_server_status;

  /**
    The number of rows affected by the last statement. This is
    semantically close to session->row_count_func, but has a different
    life cycle. session->row_count_func stores the value returned by
    function ROW_COUNT() and is cleared only by statements that
    update its value, such as INSERT, UPDATE, DELETE and few others.
    This member is cleared at the beginning of the next statement.

    We could possibly merge the two, but life cycle of session->row_count_func
    can not be changed.
  */
  ha_rows m_affected_rows;
  /**
    This is like m_affected_rows, but contains the number of rows found, not
    only affected.
  */
  ha_rows m_found_rows;
  /**
    Similarly to the previous member, this is a replacement of
    session->first_successful_insert_id_in_prev_stmt, which is used
    to implement LAST_INSERT_ID().
  */
  uint64_t m_last_insert_id;
  /** The total number of warnings. */
  uint32_t m_total_warn_count;
  enum_diagnostics_status m_status;
  /**
    @todo: the following Session members belong here:
    - warn_count,
  */
};

} /* namespace drizzled */

