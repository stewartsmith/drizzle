/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/rollback.h>

namespace drizzled
{

namespace statement
{

Rollback::Rollback(Session *in_session, bool tx_chain_arg, bool tx_release_arg) :
  Statement(in_session),
  tx_chain(tx_chain_arg),
  tx_release(tx_release_arg)
  {
    set_command(SQLCOM_ROLLBACK);
  }

bool Rollback::execute()
{
  if (not session().endTransaction(tx_release ? ROLLBACK_RELEASE : tx_chain ? ROLLBACK_AND_CHAIN : ROLLBACK))
  {
    return true;
  }
  session().my_ok();

  return false;
}

} /* namespace statement */
} /* namespace drizzled */
