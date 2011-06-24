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
#include <drizzled/statement/rollback_to_savepoint.h>
#include <drizzled/transaction_services.h>
#include <drizzled/named_savepoint.h>
#include <drizzled/util/functors.h>
#include <drizzled/session/transactions.h>

#include <string>

using namespace std;

namespace drizzled {

bool statement::RollbackToSavepoint::execute()
{
  /*
   * If AUTOCOMMIT is off and resource contexts are empty then we need
   * to start a transaction. It will be empty when ROLLBACK TO SAVEPOINT
   * starts the transaction. Table affecting statements do this work in
   * lockTables() by calling startStatement().
   */
  if ( (session().options & OPTION_NOT_AUTOCOMMIT) &&
       (transaction().all.getResourceContexts().empty() == true) )
  {
    if (session().startTransaction() == false)
    {
      return false;
    }
  }

  /*
   * Handle these situations:
   *
   * If the first savepoint on the deck matches the
   * name to find, simply call rollback_to_savepoint
   * for the resource managers.
   *
   * If the first savepoint on the deck does NOT match
   * the name to find, then we need to search through
   * the deque to find the savepoint we need.  If we
   * don't find it, we return an error.  If we do
   * find it, we must restructure the deque by removing
   * all savepoints "above" the one we find.
   */
  deque<NamedSavepoint> &savepoints= transaction().savepoints;

  /* Short-circuit for no savepoints */
  if (savepoints.empty())
  {
    my_error(ER_SP_DOES_NOT_EXIST, 
             MYF(0), 
             "SAVEPOINT", 
             lex().ident.str);
    return false;
  }

  {
    /* Short-circuit for first savepoint */
    NamedSavepoint &first_savepoint= savepoints.front();
    const string &first_savepoint_name= first_savepoint.getName();
    if (my_strnncoll(system_charset_info,
                     (unsigned char *) lex().ident.str, 
                     lex().ident.length,
                     (unsigned char *) first_savepoint_name.c_str(), 
                     first_savepoint_name.size()) == 0)
    {
      /* Found the named savepoint we want to rollback to */
      (void) TransactionServices::rollbackToSavepoint(session(), first_savepoint);

      if (transaction().all.hasModifiedNonTransData())
      {
        push_warning(&session(), 
                     DRIZZLE_ERROR::WARN_LEVEL_WARN,
                     ER_WARNING_NOT_COMPLETE_ROLLBACK,
                     ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
      }
      session().my_ok();
      return false;
    }
  }

  /* 
   * OK, from here on out it means that we have savepoints
   * but that the first savepoint isn't the one we're looking
   * for.  We need to search through the savepoints and find
   * the one we're looking for, removing all savepoints "above"
   * the one we need.
   */
  bool found= false;
  deque<NamedSavepoint> copy_savepoints(savepoints); /* used to restore if not found */
  deque<NamedSavepoint> new_savepoints;
  while (savepoints.empty() == false)
  {
    NamedSavepoint &sv= savepoints.front();
    const string &sv_name= sv.getName();
    if (! found && 
        my_strnncoll(system_charset_info,
                     (unsigned char *) lex().ident.str, 
                     lex().ident.length,
                     (unsigned char *) sv_name.c_str(), 
                     sv_name.size()) == 0)
    {
      /* Found the named savepoint we want to rollback to */
      found= true;

      (void) TransactionServices::rollbackToSavepoint(session(), sv);
    }
    if (found)
    {
      /* 
       * We push all savepoints "below" the found savepoint
       * to our new savepoint list, in reverse order to keep
       * the stack order correct. 
       */
      new_savepoints.push_back(sv);
    }
    savepoints.pop_front();
  }
  if (found)
  {
    if (transaction().all.hasModifiedNonTransData())
    {
      push_warning(&session(), 
                   DRIZZLE_ERROR::WARN_LEVEL_WARN,
                   ER_WARNING_NOT_COMPLETE_ROLLBACK,
                   ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
    }
    /* Store new savepoints list */
    transaction().savepoints= new_savepoints;
    session().my_ok();
  }
  else
  {
    /* restore the original savepoint list */
    transaction().savepoints= copy_savepoints;
    my_error(ER_SP_DOES_NOT_EXIST, 
             MYF(0), 
             "SAVEPOINT", 
             lex().ident.str);
  }
  return false;
}

} /* namespace drizzled */
