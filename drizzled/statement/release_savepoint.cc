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
#include <drizzled/statement/release_savepoint.h>
#include <drizzled/transaction_services.h>
#include <drizzled/named_savepoint.h>
#include <drizzled/sql_lex.h>
#include <drizzled/session/transactions.h>
#include <string>

using namespace std;

namespace drizzled {

bool statement::ReleaseSavepoint::execute()
{
  /*
   * Look through the deque of savepoints.  If we
   * find one with the same name, we release it and
   * unbind it from our deque.
   */
  deque<NamedSavepoint> &savepoints= transaction().savepoints;
  deque<NamedSavepoint>::iterator iter;

  for (iter= savepoints.begin(); iter != savepoints.end(); ++iter)
  {
    NamedSavepoint &sv= *iter;
    const string &sv_name= sv.getName();
    if (my_strnncoll(system_charset_info,
                     (unsigned char *) lex().ident.str,
                     lex().ident.length,
                     (unsigned char *) sv_name.c_str(),
                     sv_name.size()) == 0)
      break;
  }
  if (iter != savepoints.end())
  {
    NamedSavepoint &sv= *iter;
    (void) TransactionServices::releaseSavepoint(session(), sv);
    savepoints.erase(iter);
    session().my_ok();
  }
  else
  {
    my_error(ER_SP_DOES_NOT_EXIST, 
             MYF(0), 
             "SAVEPOINT", 
             lex().ident.str);
  }
  return false;
}

} /* namespace drizzled */
