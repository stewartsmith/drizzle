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

#include "config.h"
#include "drizzled/show.h"
#include "drizzled/session.h"
#include "drizzled/statement/savepoint.h"
#include "drizzled/transaction_services.h"
#include "drizzled/named_savepoint.h"

#include <string>
#include <deque>

using namespace std;

namespace drizzled
{

bool statement::Savepoint::execute()
{
  if (! (getSession()->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
  {
    /* AUTOCOMMIT is on and not in a BEGIN */
    getSession()->my_ok();
  }
  else
  {
    /*
     * If AUTOCOMMIT is off and resource contexts are empty then we need
     * to start a transaction. It will be empty when SAVEPOINT starts the
     * transaction. Table affecting statements do this work in lockTables()
     * by calling startStatement().
     */
    if ( (getSession()->options & OPTION_NOT_AUTOCOMMIT) &&
         (getSession()->transaction.all.getResourceContexts().empty() == true) )
    {
      if (getSession()->startTransaction() == false)
      {
        return false;
      }
    }

    /*
     * Look through the savepoints.  If we find one with
     * the same name, delete it.
     */
    TransactionServices &transaction_services= TransactionServices::singleton();
    deque<NamedSavepoint> &savepoints= getSession()->transaction.savepoints;
    deque<NamedSavepoint>::iterator iter;

    for (iter= savepoints.begin();
         iter != savepoints.end();
         ++iter)
    {
      NamedSavepoint &sv= *iter;
      const string &sv_name= sv.getName();
      if (my_strnncoll(system_charset_info,
                       (unsigned char *) getSession()->lex->ident.str,
                       getSession()->lex->ident.length,
                       (unsigned char *) sv_name.c_str(),
                       sv_name.size()) == 0)
        break;
    }
    if (iter != savepoints.end())
    {
      NamedSavepoint &sv= *iter;
      (void) transaction_services.releaseSavepoint(*getSession(), sv);
      savepoints.erase(iter);
    }
    
    NamedSavepoint newsv(getSession()->lex->ident.str, getSession()->lex->ident.length);

    if (transaction_services.setSavepoint(*getSession(), newsv))
    {
      return true;
    }
    else
    {
      savepoints.push_front(newsv);
      getSession()->my_ok();
    }
  }
  return false;
}

} /* namespace drizzled */
