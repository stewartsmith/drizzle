/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

/**
 * @file Implementation of the AlterInfo class
 */

#include "drizzled/server_includes.h"
#include "drizzled/alter_info.h"
#include "drizzled/alter_drop.h"
#include "drizzled/alter_column.h"
#include "drizzled/key.h"
#include "drizzled/create_field.h"

AlterInfo::AlterInfo() :
  flags(),
  keys_onoff(LEAVE_AS_IS),
  tablespace_op(NO_TABLESPACE_OP),
  no_parts(0),
  build_method(HA_BUILD_DEFAULT),
  datetime_field(NULL),
  error_if_not_empty(false)
{}

void AlterInfo::reset()
{
  drop_list.empty();
  alter_list.empty();
  key_list.empty();
  create_list.empty();
  flags.reset();
  keys_onoff= LEAVE_AS_IS;
  tablespace_op= NO_TABLESPACE_OP;
  no_parts= 0;
  build_method= HA_BUILD_DEFAULT;
  datetime_field= 0;
  error_if_not_empty= false;
}

/**
  Construct a copy of this object to be used for mysql_alter_table
  and mysql_create_table.

  Historically, these two functions modify their AlterInfo
  arguments. This behaviour breaks re-execution of prepared
  statements and stored procedures and is compensated by always
  supplying a copy of AlterInfo to these functions.

  @return You need to use check the error in Session for out
  of memory condition after calling this function.
*/
AlterInfo::AlterInfo(const AlterInfo &rhs, MEM_ROOT *mem_root) :
  drop_list(rhs.drop_list, mem_root),
  alter_list(rhs.alter_list, mem_root),
  key_list(rhs.key_list, mem_root),
  create_list(rhs.create_list, mem_root),
  flags(rhs.flags),
  keys_onoff(rhs.keys_onoff),
  tablespace_op(rhs.tablespace_op),
  no_parts(rhs.no_parts),
  build_method(rhs.build_method),
  datetime_field(rhs.datetime_field),
  error_if_not_empty(rhs.error_if_not_empty)
{
  /*
    Make deep copies of used objects.
    This is not a fully deep copy - clone() implementations
    of Alter_drop, Alter_column, Key, foreign_key, Key_part_spec
    do not copy string constants. At the same length the only
    reason we make a copy currently is that ALTER/CREATE TABLE
    code changes input AlterInfo definitions, but string
    constants never change.
  */
  list_copy_and_replace_each_value(drop_list, mem_root);
  list_copy_and_replace_each_value(alter_list, mem_root);
  list_copy_and_replace_each_value(key_list, mem_root);
  list_copy_and_replace_each_value(create_list, mem_root);
}
