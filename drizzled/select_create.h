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

#pragma once

#include <drizzled/select_insert.h>

namespace drizzled
{

class select_create: public select_insert {
  Order *group;
  TableList *create_table;
  bool is_if_not_exists;
  HA_CREATE_INFO *create_info;
  message::Table &table_proto;
  TableList *select_tables;
  AlterInfo *alter_info;
  Field **field;
  /* lock data for tmp table */
  DrizzleLock *m_lock;
  /* m_lock or session->extra_lock */
  DrizzleLock **m_plock;
  const identifier::Table& identifier;

public:
  select_create (TableList *table_arg,
                 bool is_if_not_exists_arg,
                 HA_CREATE_INFO *create_info_par,
                 message::Table &proto,
                 AlterInfo *alter_info_arg,
                 List<Item> &select_fields,enum_duplicates duplic, bool ignore,
                 TableList *select_tables_arg,
                 const identifier::Table& identifier_arg)
    :select_insert (NULL, NULL, &select_fields, 0, 0, duplic, ignore),
    create_table(table_arg),
    is_if_not_exists(is_if_not_exists_arg),
    create_info(create_info_par),
    table_proto(proto),
    select_tables(select_tables_arg),
    alter_info(alter_info_arg),
    m_plock(NULL),
    identifier(identifier_arg)
    {}
  int prepare(List<Item> &list, Select_Lex_Unit *u);

  void store_values(List<Item> &values);
  void send_error(drizzled::error_t errcode,const char *err);
  bool send_eof();
  void abort();
  virtual bool can_rollback_data() { return true; }

  // Needed for access from local class MY_HOOKS in prepare(), since session is proteted.
  const Session *get_session(void) { return session; }
  const HA_CREATE_INFO *get_create_info() { return create_info; }
  int prepare2(void) { return 0; }
};

} /* namespace drizzled */

