/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include "config.h"
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/rollback_to_savepoint.h>
#include "drizzled/transaction_services.h"

namespace drizzled
{

bool statement::RollbackToSavepoint::execute()
{
  SAVEPOINT *sv;
  for (sv= session->transaction.savepoints; sv; sv= sv->prev)
  {
    if (my_strnncoll(system_charset_info,
                     (unsigned char *) session->lex->ident.str, 
                     session->lex->ident.length,
                     (unsigned char *)sv->name, 
                     sv->length) == 0)
    {
      break;
    }
  }
  if (sv)
  {
    TransactionServices &transaction_services= TransactionServices::singleton();
    if (transaction_services.ha_rollback_to_savepoint(session, sv))
    {
      return true; /* cannot happen */
    }
    else
    {
      if (session->transaction.all.modified_non_trans_table)
      {
        push_warning(session, 
                     DRIZZLE_ERROR::WARN_LEVEL_WARN,
                     ER_WARNING_NOT_COMPLETE_ROLLBACK,
                     ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
      }
      session->my_ok();
    }
    session->transaction.savepoints= sv;
  }
  else
  {
    my_error(ER_SP_DOES_NOT_EXIST, 
             MYF(0), 
             "SAVEPOINT", 
             session->lex->ident.str);
  }
  return false;
}

} /* namespace drizzled */

