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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <drizzled/session.h>
#include <plugin/myisam/myisam.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/plugin/transactional_storage_engine.h>
#include <drizzled/table/instance.h>
#include <drizzled/table.h>
#include <drizzled/table_list.h>

namespace drizzled {
namespace table {

/*
  Open table which is already name-locked by this thread.

  SYNOPSIS
  reopen_name_locked_table()
  session         Thread handle
  table_list  TableList object for table to be open, TableList::table
  member should point to Table object which was used for
  name-locking.
  link_in     true  - if Table object for table to be opened should be
  linked into Session::open_tables list.
  false - placeholder used for name-locking is already in
  this list so we only need to preserve Table::next
  pointer.

  NOTE
  This function assumes that its caller already acquired table::Cache::mutex() mutex.

  RETURN VALUE
  false - Success
  true  - Error
*/

bool Concurrent::reopen_name_locked_table(TableList* table_list, Session *session)
{
  safe_mutex_assert_owner(table::Cache::mutex().native_handle());

  if (session->getKilled())
    return true;

  identifier::Table identifier(table_list->getSchemaName(), table_list->getTableName());
  if (open_unireg_entry(session, table_list->getTableName(), identifier))
  {
    intern_close_table();
    return true;
  }

  /*
    We want to prevent other connections from opening this table until end
    of statement as it is likely that modifications of table's metadata are
    not yet finished (for example CREATE TRIGGER have to change .TRG cursor,
    or we might want to drop table if CREATE TABLE ... SELECT fails).
    This also allows us to assume that no other connection will sneak in
    before we will get table-level lock on this table.
  */
  getMutableShare()->resetVersion();
  in_use = session;

  tablenr= session->open_tables.current_tablenr++;
  used_fields= 0;
  const_table= 0;
  null_row= false;
  maybe_null= false;
  force_index= false;
  status= STATUS_NO_RECORD;

  return false;
}


/*
  Load a table definition from cursor and open unireg table

  SYNOPSIS
  open_unireg_entry()
  session			Thread handle
  entry		Store open table definition here
  table_list		TableList with db, table_name
  alias		Alias name
  cache_key		Key for share_cache
  cache_key_length	length of cache_key

  NOTES
  Extra argument for open is taken from session->open_options
  One must have a lock on table::Cache::mutex() when calling this function

  RETURN
  0	ok
#	Error
*/

int table::Concurrent::open_unireg_entry(Session *session,
                                         const char *alias,
                                         identifier::Table &identifier)
{
  int error;
  TableShare::shared_ptr share;
  uint32_t discover_retry_count= 0;

  safe_mutex_assert_owner(table::Cache::mutex().native_handle());
retry:
  if (not (share= table::instance::Shared::make_shared(session, identifier, error)))
  {
    return 1;
  }

  while ((error= share->open_table_from_share(session,
                                              identifier,
                                              alias,
                                              (uint32_t) (HA_OPEN_KEYFILE |
                                                          HA_OPEN_RNDFILE |
                                                          HA_GET_INDEX |
                                                          HA_TRY_READ_ONLY),
                                              session->open_options, *this)))
  {
    if (error == 7)                             // Table def changed
    {
      share->resetVersion();                        // Mark share as old
      if (discover_retry_count++)               // Retry once
      {
        table::instance::release(share);
        return 1;
      }

      /*
        TODO->
        Here we should wait until all threads has released the table.
        For now we do one retry. This may cause a deadlock if there
        is other threads waiting for other tables used by this thread.

        Proper fix would be to if the second retry failed:
        - Mark that table def changed
        - Return from open table
        - Close all tables used by this thread
        - Start waiting that the share is released
        - Retry by opening all tables again
      */

      /*
        TO BE FIXED
        To avoid deadlock, only wait for release if no one else is
        using the share.
      */
      if (share->getTableCount() != 1)
      {
        table::instance::release(share);
        return 1;
      }

      /* Free share and wait until it's released by all threads */
      table::instance::release(share);

      if (not session->getKilled())
      {
        drizzle_reset_errors(session, 1);         // Clear warnings
        session->clear_error();                 // Clear error message
        goto retry;
      }

      return 1;
    }

    table::instance::release(share);

    return 1;
  }

  return 0;
}

void table::Concurrent::release(void)
{
  // During an ALTER TABLE we could see the proto go away when the
  // definition is pushed out of this table object. In this case we would
  // not release from the cache because we were not in the cache. We just
  // delete if this happens.
  if (getShare()->getType() == message::Table::STANDARD)
  {
    table::instance::release(getMutableShare());
  }
  else
  {
    delete _share;
  }
  _share= NULL;
}

} /* namespace table */
} /* namespace drizzled */
