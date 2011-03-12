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
#include <plugin/show_dictionary/dictionary.h>

using namespace std;
using namespace drizzled;

ShowSchemas::ShowSchemas() :
  show_dictionary::Show("SHOW_SCHEMAS")
{
  add_field("SCHEMA_NAME");
}

ShowSchemas::Generator::Generator(Field **arg) :
  show_dictionary::Show::Generator(arg),
  schema_generator(getSession())
{
}

bool ShowSchemas::Generator::populate()
{
  drizzled::message::schema::shared_ptr schema_ptr;

  if (not isShowQuery())
    return false;

  while ((schema_ptr= schema_generator))
  {
    if (isWild(schema_ptr->name()))
      continue;

    /* Schema */
    push(schema_ptr->name());

    return true;
  }

  return false;
}
