/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

#include <plugin/error_dictionary/errors.h>
#include <drizzled/error/sql_state.h>

namespace drizzle_plugin
{

error_dictionary::Errors::Errors() :
  drizzled::plugin::TableFunction("DATA_DICTIONARY", "ERRORS")
{
  add_field("ERROR_CODE", drizzled::plugin::TableFunction::NUMBER);
  add_field("ERROR_NAME");
  add_field("ERROR_MESSAGE");
  add_field("ERROR_SQL_STATE");
}

error_dictionary::Errors::Generator::Generator(drizzled::Field **arg) :
  drizzled::plugin::TableFunction::Generator(arg),
  _error_map(drizzled::ErrorMap::get_error_message_map()),
  _iter(drizzled::ErrorMap::get_error_message_map().begin())
{ }

bool error_dictionary::Errors::Generator::populate()
{
  if (_iter == _error_map.end())
    return false;

  push(uint64_t((*_iter).first));
  push((*_iter).second.first);
  push((*_iter).second.second);
  push(drizzled::error::convert_to_sqlstate((*_iter).first));

  ++_iter;

  return true;
}

}
