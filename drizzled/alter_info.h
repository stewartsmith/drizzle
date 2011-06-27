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

/**
 * @file Declaration of the AlterInfo class
 */

#pragma once

#include <drizzled/alter_column.h>
#include <drizzled/base.h>
#include <drizzled/enum.h>
#include <drizzled/key.h>
#include <drizzled/message/table.pb.h>

#include <bitset>
#include <list>

namespace drizzled {

enum enum_alter_info_flags
{
  ALTER_ADD_COLUMN= 0,
  ALTER_DROP_COLUMN,
  ALTER_CHANGE_COLUMN,
  ALTER_COLUMN_STORAGE,
  ALTER_COLUMN_FORMAT,
  ALTER_COLUMN_ORDER,
  ALTER_ADD_INDEX,
  ALTER_DROP_INDEX,
  ALTER_RENAME,
  ALTER_ORDER,
  ALTER_OPTIONS,
  ALTER_COLUMN_DEFAULT,
  ALTER_KEYS_ONOFF,
  ALTER_STORAGE,
  ALTER_ROW_FORMAT,
  ALTER_CONVERT,
  ALTER_FORCE,
  ALTER_RECREATE,
  ALTER_TABLE_REORG,
  ALTER_FOREIGN_KEY
};

/**
 * Contains information about the parsed CREATE or ALTER TABLE statement.
 *
 * This structure contains a list of columns or indexes to be created,
 * altered or dropped.
 */
class AlterInfo : boost::noncopyable
{
public:
  typedef std::list<AlterColumn> alter_list_t;

  alter_list_t alter_list;
  List<Key> key_list;
  List<CreateField> create_list;
  message::AddedFields added_fields_proto;
  std::bitset<32> flags;
  uint32_t no_parts;
  bool error_if_not_empty;

  AlterInfo();
  AlterInfo(const AlterInfo&, memory::Root*);
};

} /* namespace drizzled */

