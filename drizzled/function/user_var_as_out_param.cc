/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>

#include <cassert>

#include <drizzled/function/user_var_as_out_param.h>
#include <drizzled/user_var_entry.h>
#include <drizzled/session.h>

namespace drizzled
{

bool Item_user_var_as_out_param::fix_fields(Session *session, Item **ref)
{
  assert(fixed == 0);
  if (Item::fix_fields(session, ref) ||
      !(entry= session->getVariable(name, true)))
    return true;
  entry->type= STRING_RESULT;
  /*
    Let us set the same collation which is used for loading
    of fields in LOAD DATA INFILE.
    (Since Item_user_var_as_out_param is used only there).
  */
  entry->collation.set(default_charset_info);
  entry->update_query_id= session->getQueryId();
  return false;
}

void Item_user_var_as_out_param::set_null_value(const charset_info_st * const cs)
{
  entry->update_hash(true, 0, 0, STRING_RESULT, cs,
                     DERIVATION_IMPLICIT, 0 /* unsigned_arg */);
}


void Item_user_var_as_out_param::set_value(const char *str, uint32_t length,
                                           const charset_info_st * const cs)
{
  entry->update_hash(false, (void*)str, length, STRING_RESULT, cs,
                DERIVATION_IMPLICIT, 0 /* unsigned_arg */);
}

double Item_user_var_as_out_param::val_real()
{
  assert(0);
  return 0.0;
}


int64_t Item_user_var_as_out_param::val_int()
{
  assert(0);
  return 0;
}


String* Item_user_var_as_out_param::val_str(String *)
{
  assert(0);
  return 0;
}

type::Decimal* Item_user_var_as_out_param::val_decimal(type::Decimal *)
{
  assert(0);
  return 0;
}


void Item_user_var_as_out_param::print(String *str)
{
  str->append('@');
  str->append(name.str,name.length);
}

} /* namespace drizzled */
