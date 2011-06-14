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
#include <drizzled/statement/create_schema.h>
#include <drizzled/schema.h>
#include <drizzled/plugin/event_observer.h>
#include <drizzled/message.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/sql_lex.h>
#include <drizzled/plugin/authorization.h>

#include <string>

using namespace std;

namespace drizzled
{

bool statement::CreateSchema::execute()
{
  if (not validateSchemaOptions())
    return true;

  if (session().inTransaction())
  {
    my_error(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, MYF(0));
    return true;
  }

  identifier::Schema schema_identifier(string(lex().name.str, lex().name.length));
  if (not check(schema_identifier))
    return false;

  drizzled::message::schema::init(schema_message, lex().name.str);

  message::set_definer(schema_message, *session().user());

  bool res = false;
  std::string path = schema_identifier.getSQLPath();

  if (unlikely(plugin::EventObserver::beforeCreateDatabase(session(), path)))
  {
    my_error(ER_EVENT_OBSERVER_PLUGIN, MYF(0), path.c_str());
  }
  else
  {
    res= schema::create(session(), schema_message, lex().exists());
    if (unlikely(plugin::EventObserver::afterCreateDatabase(session(), path, res)))
    {
      my_error(ER_EVENT_OBSERVER_PLUGIN, schema_identifier);
      res = false;
    }

  }

  return not res;
}

bool statement::CreateSchema::check(const identifier::Schema &identifier)
{
  if (not identifier.isValid())
    return false;

  if (not plugin::Authorization::isAuthorized(*session().user(), identifier))
    return false;

  if (not lex().exists())
  {
    if (plugin::StorageEngine::doesSchemaExist(identifier))
    {
      my_error(ER_DB_CREATE_EXISTS, identifier);

      return false;
    }
  }

  return true;
}

// We don't actually test anything at this point, we assume it is all bad.
bool statement::CreateSchema::validateSchemaOptions()
{
  size_t num_engine_options= schema_message.engine().options_size();
  bool rc= num_engine_options ? false : true;

  for (size_t y= 0; y < num_engine_options; ++y)
  {
    my_error(ER_UNKNOWN_SCHEMA_OPTION, MYF(0),
             schema_message.engine().options(y).name().c_str(),
             schema_message.engine().options(y).state().c_str());

    rc= false;
  }

  return rc;
}

} /* namespace drizzled */

