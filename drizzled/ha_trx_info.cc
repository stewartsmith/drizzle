/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include <drizzled/ha_trx_info.h>
#include <drizzled/handlerton.h>
#include <drizzled/session.h>


void Ha_trx_info::register_ha(Session_TRANS *trans, handlerton *ht_arg)
{
  assert(m_flags == 0);
  assert(m_ht == NULL);
  assert(m_next == NULL);

  m_ht= ht_arg;
  m_flags= (int) TRX_READ_ONLY; /* Assume read-only at start. */

  m_next= trans->ha_list;
  trans->ha_list= this;
}


/** Clear, prepare for reuse. */
void Ha_trx_info::reset()
{
  m_next= NULL;
  m_ht= NULL;
  m_flags= 0;
}

void Ha_trx_info::set_trx_read_write()
{
  assert(is_started());
  m_flags|= (int) TRX_READ_WRITE;
}


bool Ha_trx_info::is_trx_read_write() const
{
  assert(is_started());
  return m_flags & (int) TRX_READ_WRITE;
}


bool Ha_trx_info::is_started() const
{
  return m_ht != NULL;
}


/** Mark this transaction read-write if the argument is read-write. */
void Ha_trx_info::coalesce_trx_with(const Ha_trx_info *stmt_trx)
{
  /*
    Must be called only after the transaction has been started.
    Can be called many times, e.g. when we have many
    read-write statements in a transaction.
  */
  assert(is_started());
  if (stmt_trx->is_trx_read_write())
    set_trx_read_write();
}


Ha_trx_info *Ha_trx_info::next() const
{
  assert(is_started());
  return m_next;
}


handlerton *Ha_trx_info::ht() const
{
  assert(is_started());
  return m_ht;
}
