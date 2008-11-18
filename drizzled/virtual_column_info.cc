/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
#include <drizzled/virtual_column_info.h>

enum_field_types virtual_column_info::get_real_type()
{
  assert(data_inited);
  return data_inited ? field_type : DRIZZLE_TYPE_VIRTUAL;
}


void virtual_column_info::set_field_type(enum_field_types fld_type)
{
  /* Calling this function can only be done once. */
  assert(not data_inited);
  data_inited= true;
  field_type= fld_type;
}


bool virtual_column_info::get_field_stored()
{
  assert(data_inited);
  return data_inited ? is_stored : true;
}


void virtual_column_info::set_field_stored(bool stored)
{
  is_stored= stored;
}
