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


#ifndef DRIZZLED_OPEN_TABLES_STATE_H
#define DRIZZLED_OPEN_TABLES_STATE_H


/**
  Class that holds information about tables which were opened and locked
  by the thread. It is also used to save/restore this information in
  push_open_tables_state()/pop_open_tables_state().
*/

class Open_tables_state
{
public:
  /**
    List of regular tables in use by this thread. Contains temporary and
    base tables that were opened with @see open_tables().
  */
  Table *open_tables;
  /**
    List of temporary tables used by this thread. Contains user-level
    temporary tables, created with CREATE TEMPORARY TABLE, and
    internal temporary tables, created, e.g., to resolve a SELECT,
    or for an intermediate table used in ALTER.
    XXX Why are internal temporary tables added to this list?
  */
  Table *temporary_tables;
  /**
    List of tables that were opened with HANDLER OPEN and are
    still in use by this thread.
  */
  Table *handler_tables;
  Table *derived_tables;
  /*
    During a MySQL session, one can lock tables in two modes: automatic
    or manual. In automatic mode all necessary tables are locked just before
    statement execution, and all acquired locks are stored in 'lock'
    member. Unlocking takes place automatically as well, when the
    statement ends.
    Manual mode comes into play when a user issues a 'LOCK TABLES'
    statement. In this mode the user can only use the locked tables.
    Trying to use any other tables will give an error. The locked tables are
    stored in 'locked_tables' member.  Manual locking is described in
    the 'LOCK_TABLES' chapter of the MySQL manual.
    See also lock_tables() for details.
  */
  DRIZZLE_LOCK *lock;
  /*
    Tables that were locked with explicit or implicit LOCK TABLES.
    (Implicit LOCK TABLES happens when we are prelocking tables for
     execution of statement which uses stored routines. See description
     Session::prelocked_mode for more info.)
  */
  DRIZZLE_LOCK *locked_tables;

  /*
    CREATE-SELECT keeps an extra lock for the table being
    created. This field is used to keep the extra lock available for
    lower level routines, which would otherwise miss that lock.
   */
  DRIZZLE_LOCK *extra_lock;

  ulong	version;
  uint32_t current_tablenr;

  enum enum_flags {
    BACKUPS_AVAIL = (1U << 0)     /* There are backups available */
  };

  /*
    Flags with information about the open tables state.
  */
  uint32_t state_flags;

  /*
    This constructor serves for creation of Open_tables_state instances
    which are used as backup storage.
  */
  Open_tables_state() : state_flags(0U) { }

  Open_tables_state(ulong version_arg);

  void set_open_tables_state(Open_tables_state *state)
  {
    *this= *state;
  }

  void reset_open_tables_state()
  {
    open_tables= temporary_tables= handler_tables= derived_tables= 0;
    extra_lock= lock= locked_tables= 0;
    state_flags= 0U;
  }
};

#endif /* DRIZZLED_OPEN_TABLES_STATE_H */
