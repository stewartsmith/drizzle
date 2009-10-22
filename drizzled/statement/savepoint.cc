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

#include <drizzled/server_includes.h>
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/savepoint.h>

namespace drizzled
{

bool statement::Savepoint::execute()
{
  if (! (session->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
  {
    session->my_ok();
  }
  else
  {
    SAVEPOINT **sv, *newsv;
    for (sv= &session->transaction.savepoints; *sv; sv= &(*sv)->prev)
    {
      if (my_strnncoll(system_charset_info,
                       (unsigned char *) session->lex->ident.str, 
                       session->lex->ident.length,
                       (unsigned char *) (*sv)->name, 
                       (*sv)->length) == 0)
        return false;
    }
    if (*sv) /* old savepoint of the same name exists */
    {
      newsv= *sv;
      ha_release_savepoint(session, *sv); // it cannot fail
      *sv= (*sv)->prev;
    }
    else if ((newsv= (SAVEPOINT *) alloc_root(&session->transaction.mem_root,
                                              savepoint_alloc_size)) == 0)
    {
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
      return false;
    }
    newsv->name= strmake_root(&session->transaction.mem_root,
                              session->lex->ident.str, 
                              session->lex->ident.length);
    newsv->length= session->lex->ident.length;
    /*
       if we'll get an error here, don't add new savepoint to the list.
       we'll lose a little bit of memory in transaction mem_root, but it'll
       be free'd when transaction ends anyway
     */
    if (ha_savepoint(session, newsv))
    {
      return true;
    }
    else
    {
      newsv->prev= session->transaction.savepoints;
      session->transaction.savepoints= newsv;
      session->my_ok();
    }
  }
  return false;
}

} /* namespace drizzled */

