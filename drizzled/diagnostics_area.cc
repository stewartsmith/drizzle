/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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
#include <drizzled/session.h>
#include <drizzled/diagnostics_area.h>

namespace drizzled
{

/**
  Clear this diagnostics area.

  Normally called at the end of a statement.
*/
void Diagnostics_area::reset_diagnostics_area()
{
  can_overwrite_status= false;
  /** Don't take chances in production */
  m_message[0]= '\0';
  m_sql_errno= EE_OK;
  m_server_status= 0;
  m_affected_rows= 0;
  m_found_rows= 0;
  m_last_insert_id= 0;
  m_total_warn_count= 0;
  is_sent= false;
  /** Tiny reset in debug mode to see garbage right away */
  m_status= DA_EMPTY;
}

const char *Diagnostics_area::message() const
{
  assert(m_status == DA_ERROR || m_status == DA_OK);
  return m_message;
}


error_t Diagnostics_area::sql_errno() const
{
  assert(m_status == DA_ERROR);
  return m_sql_errno;
}

uint32_t Diagnostics_area::server_status() const
{
  assert(m_status == DA_OK || m_status == DA_EOF);
  return m_server_status;
}

ha_rows Diagnostics_area::affected_rows() const
{ assert(m_status == DA_OK); return m_affected_rows; }

ha_rows Diagnostics_area::found_rows() const
{ assert(m_status == DA_OK); return m_found_rows; }

uint64_t Diagnostics_area::last_insert_id() const
{ assert(m_status == DA_OK); return m_last_insert_id; }

uint32_t Diagnostics_area::total_warn_count() const
{
  assert(m_status == DA_OK || m_status == DA_EOF);
  return m_total_warn_count;
}

/**
  Set OK status -- ends commands that do not return a
  result set, e.g. INSERT/UPDATE/DELETE.
*/
void Diagnostics_area::set_ok_status(Session *session,
                                     ha_rows affected_rows_arg,
                                     ha_rows found_rows_arg,
                                     uint64_t last_insert_id_arg,
                                     const char *message_arg)
{
  assert(! is_set());
  /*
    In production, refuse to overwrite an error or a custom response
    with an OK packet.
  */
  if (is_error() || is_disabled())
    return;
  /** Only allowed to report success if has not yet reported an error */

  m_server_status= session->server_status;
  m_total_warn_count= session->total_warn_count;
  m_affected_rows= affected_rows_arg;
  m_found_rows= found_rows_arg;
  m_last_insert_id= last_insert_id_arg;
  if (message_arg)
    strncpy(m_message, message_arg, sizeof(m_message) - 1);
  else
    m_message[0]= '\0';
  m_status= DA_OK;
}

/**
  Set EOF status.
*/
void Diagnostics_area::set_eof_status(Session *session)
{
  /** Only allowed to report eof if has not yet reported an error */

  assert(! is_set());
  /*
    In production, refuse to overwrite an error or a custom response
    with an EOF packet.
  */
  if (is_error() || is_disabled())
    return;

  m_server_status= session->server_status;
  /*
    If inside a stored procedure, do not return the total
    number of warnings, since they are not available to the client
    anyway.
  */
  m_total_warn_count= session->total_warn_count;

  m_status= DA_EOF;
}

/**
  Set ERROR status.
*/
void Diagnostics_area::set_error_status(error_t sql_errno_arg,
                                        const char *message_arg)
{
  /*
    Only allowed to report error if has not yet reported a success
    The only exception is when we flush the message to the client,
    an error can happen during the flush.
  */
  assert(! is_set() || can_overwrite_status);
  /*
    In production, refuse to overwrite a custom response with an
    ERROR packet.
  */
  if (is_disabled())
    return;

  m_sql_errno= sql_errno_arg;
  strncpy(m_message, message_arg, sizeof(m_message) - 1);

  m_status= DA_ERROR;
}

/**
  Mark the diagnostics area as 'DISABLED'.

  This is used in rare cases when the COM_ command at hand sends a response
  in a custom format. One example is the query cache, another is
  COM_STMT_PREPARE.
*/
void Diagnostics_area::disable_status()
{
  assert(! is_set());
  m_status= DA_DISABLED;
}

} /* namespace drizzled */
