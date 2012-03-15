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
#include <drizzled/statement/start_transaction.h>
#include <drizzled/session/transactions.h>

namespace drizzled {

bool statement::StartTransaction::execute()
{
  if (session().inTransaction())
  {
    push_warning_printf(&session(), DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRANSACTION_ALREADY_STARTED,
                        ER(ER_TRANSACTION_ALREADY_STARTED));
    return false;
  }

  if (transaction().xid_state.xa_state != XA_NOTR)
  {
    my_error(ER_XAER_RMFAIL, MYF(0),
        xa_state_names[transaction().xid_state.xa_state]);
    return false;
  }
  /*
     Breakpoints for backup testing.
   */
  if (! session().startTransaction(start_transaction_opt))
  {
    return true;
  }
  session().my_ok();
  return false;
}

}
