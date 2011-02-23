/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <config.h>

#include <plugin/catalog/module.h>

namespace plugin {
namespace catalog {
namespace tables {

Catalogs::Catalogs() :
  drizzled::plugin::TableFunction("DATA_DICTIONARY", "CATALOGS")
{
  add_field("CATALOG_NAME", drizzled::plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("CATALOG_CREATION_TIME");
  add_field("CATALOG_UPDATE_TIME");
  add_field("CATALOG_UUID", drizzled::plugin::TableFunction::STRING, 36, true);
  add_field("CATALOG_VERSION", drizzled::plugin::TableFunction::NUMBER, 0, true);
}

Catalogs::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg)
{
}

bool Catalogs::Generator::populate()
{
  drizzled::message::catalog::shared_ptr tmp;

  while ((tmp= catalog_generator))
  {
    // CATALOG_NAME
    push(tmp->name());

    /* SCHEMA_CREATION_TIME */
    time_t time_arg= tmp->creation_timestamp();
    char buffer[40];
    struct tm tm_buffer;

    localtime_r(&time_arg, &tm_buffer);
    strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm_buffer);
    push(buffer);

    /* SCHEMA_UPDATE_TIME */
    time_arg= tmp->update_timestamp();
    localtime_r(&time_arg, &tm_buffer);
    strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm_buffer);
    push(buffer);

    /* SCHEMA_UUID */
    push(tmp->uuid());

    /* SCHEMA_VERSION */
    push(tmp->version());

    return true;
  }

  return false;
}

} /* namespace tables */
} /* namespace catalog */
} /* namespace plugin */
