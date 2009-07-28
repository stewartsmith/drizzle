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

#ifndef DRIZZLE_SERVER_SQL_COMMANDS_H
#define DRIZZLE_SERVER_SQL_COMMANDS_H

#include <drizzled/server_includes.h>
#include <drizzled/definitions.h>
#include <drizzled/logging.h>
#include <drizzled/db.h>
#include <drizzled/error.h>
#include <drizzled/query_id.h>
#include <drizzled/sql_parse.h>
#include <drizzled/sql_base.h>
#include <drizzled/show.h>
#include <drizzled/rename.h>
#include <drizzled/function/time/unix_timestamp.h>
#include <drizzled/function/get_system_var.h>
#include <drizzled/item/null.h>
#include <drizzled/sql_load.h>
#include <drizzled/connect.h>
#include <drizzled/lock.h>

class Session;
class TableList;
class Item;

namespace drizzled
{

/**
 * @class SqlCommand
 * @brief Represents a command to be executed
 */
class SqlCommand
{
public:
  SqlCommand(enum enum_sql_command in_comm_type,
             Session *in_session)
    :
      comm_type(in_comm_type),
      session(in_session),
      all_tables(NULL),
      first_table(NULL),
      show_lock(NULL),
      need_start_waiting(NULL)
  {}

  SqlCommand(enum enum_sql_command in_comm_type,
             Session *in_session,
             TableList *in_all_tables)
    :
      comm_type(in_comm_type),
      session(in_session),
      all_tables(in_all_tables),
      first_table(NULL),
      show_lock(NULL),
      need_start_waiting(NULL)
  {}

  virtual ~SqlCommand() {}

  /**
   *
   */
  virtual int execute();

  void setTableList(TableList *in_all_tables)
  {
    all_tables= in_all_tables;
  }

  void setFirstTable(TableList *in_first_table)
  {
    first_table= in_first_table;
  }

  void setShowLock(pthread_mutex_t *in_lock)
  {
    show_lock= in_lock;
  }

  void setNeedStartWaiting(bool *in_need_start_waiting)
  {
    need_start_waiting= in_need_start_waiting;
  }

protected:
  enum enum_sql_command comm_type;
  Session *session;
  TableList *all_tables;
  TableList *first_table;

  /**
   * A mutex that is only needed by the SHOW STATUS
   * command but declare it in the base class since
   * we will not know what command we are working with
   * at runtime.
   */
  pthread_mutex_t *show_lock;
  bool *need_start_waiting;
};

/**
 * @class ShowStatusCommand
 * @brief Represents the SHOW STATUS command
 */
class ShowStatusCommand : public SqlCommand
{
public:
  ShowStatusCommand(enum enum_sql_command in_comm_type,
                    Session *in_session)
    :
      SqlCommand(in_comm_type, in_session)
  {}

  int execute();
};

} /* end namespace drizzled */

#endif /* DRIZZLE_SERVER_SQL_COMMANDS_H */
