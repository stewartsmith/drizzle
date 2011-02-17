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

#include <config.h>

#include <drizzled/generator.h>

using namespace std;

namespace drizzled
{
namespace generator
{

AllFields::AllFields(Session &arg) :
  session(arg),
  field_iterator(0),
  all_tables_generator(arg)
{
  ((table_ptr= all_tables_generator));
  table_setup();
}

bool AllFields::table_setup()
{
  table_message.Clear();
  table_message.CopyFrom(*table_ptr);
  field_iterator= 0;

  return true;
}

} /* namespace generator */
} /* namespace drizzled */

bool operator!(const drizzled::generator::FieldPair &arg)
{
  if (arg.first == 0 and arg.second == 0)
    return true;

  return false;
}
