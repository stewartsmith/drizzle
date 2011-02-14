/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#ifndef DRIZZLED_GENERATOR_SCHEMA_H
#define DRIZZLED_GENERATOR_SCHEMA_H

#include <drizzled/session.h>
#include <drizzled/plugin/authorization.h>
#include <drizzled/plugin/storage_engine.h>

namespace drizzled {
namespace generator {

class Schema
{
  Session &session;
  message::schema::shared_ptr schema;

  identifier::Schema::vector schema_names;
  identifier::Schema::vector::const_iterator schema_iterator;

public:

  Schema(Session &arg);

  operator const drizzled::message::schema::shared_ptr()
  {
    while (schema_iterator != schema_names.end())
    {
      identifier::Schema schema_identifier(*schema_iterator);

      if (not plugin::Authorization::isAuthorized(*session.user(), schema_identifier, false))
      {
        schema_iterator++;
        continue;
      }

      schema= plugin::StorageEngine::getSchemaDefinition(schema_identifier);
      schema_iterator++;

      if (schema)
        return schema;
    }

    return message::schema::shared_ptr();
  }

  operator const drizzled::identifier::Schema*()
  {
    while (schema_iterator != schema_names.end())
    {
      const drizzled::identifier::Schema *_ptr= &(*schema_iterator);
      schema_iterator++;

      if (not plugin::Authorization::isAuthorized(*session.user(), *_ptr, false))
        continue;

      return _ptr;
    }

    return NULL;
  }
};

} /* namespace generator */
} /* namespace drizzled */

#endif /* DRIZZLED_GENERATOR_SCHEMA_H */
