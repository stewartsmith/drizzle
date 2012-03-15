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

#include <drizzled/field/varstring.h>
#include <drizzled/session.h>
#include <drizzled/stored_key.h>
#include <drizzled/table.h>

namespace drizzled {

StoredKey::StoredKey(Session *session,
                     Field *field_arg, 
                     unsigned char *ptr,
                     unsigned char *null, 
                     uint32_t length) :
  null_key(0), 
  null_ptr(null), 
  err(0)
  {
    if (field_arg->type() == DRIZZLE_TYPE_BLOB)
    {
      /*
        Key segments are always packed with a 2 byte length prefix.
        See mi_rkey for details.
      */
      to_field= new Field_varstring(ptr,
                                    length,
                                    2,
                                    null,
                                    1,
                                    field_arg->field_name,
                                    field_arg->charset());
      to_field->init(field_arg->getTable());
    }
    else
    {
      to_field= field_arg->new_key_field(session->mem_root, field_arg->getTable(),
                                         ptr, null, 1);
    }

    to_field->setWriteSet();
  }

StoredKey::store_key_result StoredKey::copy()
{
  store_key_result result;
  Session *session= to_field->getTable()->in_use;
  enum_check_fields saved_count_cuted_fields= session->count_cuted_fields;
  session->count_cuted_fields= CHECK_FIELD_IGNORE;
  result= copy_inner();
  session->count_cuted_fields= saved_count_cuted_fields;

  return result;
}

} /* namespace drizzled */
