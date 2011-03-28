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
#include <drizzled/statement/drop_schema.h>
#include <drizzled/plugin/event_observer.h>
#include <drizzled/sql_lex.h>
#include <drizzled/schema.h>

#include <string>

using namespace std;

namespace drizzled
{

bool statement::DropSchema::execute()
{
  if (session().inTransaction())
  {
    my_error(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, MYF(0));
    return true;
  }

  identifier::Schema schema_identifier(std::string(lex().name.str, lex().name.length));

  if (not schema::check(session(), schema_identifier))
  {
    my_error(ER_WRONG_DB_NAME, schema_identifier);

    return false;
  }

  if (session().inTransaction())
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION, 
        ER(ER_LOCK_OR_ACTIVE_TRANSACTION), 
        MYF(0));
    return true;
  }
  
  bool res = true;
  std::string path = schema_identifier.getSQLPath();
  if (unlikely(plugin::EventObserver::beforeDropDatabase(session(), path))) 
  {
    my_error(ER_EVENT_OBSERVER_PLUGIN, schema_identifier);
  }
  else
  {
    res= schema::drop(session(), schema_identifier, drop_if_exists);
    if (unlikely(plugin::EventObserver::afterDropDatabase(session(), path, res)))
    {
      my_error(ER_EVENT_OBSERVER_PLUGIN, MYF(0), path.c_str());
      res = false;
    }

  }

  return res;
}

} /* namespace drizzled */

