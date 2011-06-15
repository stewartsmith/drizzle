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

#include <drizzled/plugin/client.h>
#include <drizzled/plugin/query_cache.h>
#include <drizzled/plugin/transactional_storage_engine.h>
#include <drizzled/select_result.h>
#include <drizzled/sql_lex.h>
#include <drizzled/open_tables_state.h>

namespace drizzled {

class select_send : public select_result 
{
public:
  bool send_eof()
  {
    /*
      We may be passing the control from mysqld to the client: release the
      InnoDB adaptive hash S-latch to avoid thread deadlocks if it was reserved
      by session
    */
    plugin::TransactionalStorageEngine::releaseTemporaryLatches(session);

    /* Unlock tables before sending packet to gain some speed */
    if (session->open_tables.lock)
    {
      session->unlockTables(session->open_tables.lock);
      session->open_tables.lock= 0;
    }
    session->my_eof();
    return false;
  }

  void send_fields(List<Item>& list)
  {
    session->getClient()->sendFields(list);
  }

  /* Send data to client. Returns 0 if ok */

  bool send_data(List<Item>& items)
  {
    if (unit->offset_limit_cnt)
    {						// using limit offset,count
      unit->offset_limit_cnt--;
      return false;
    }

    /*
      We may be passing the control from mysqld to the client: release the
      InnoDB adaptive hash S-latch to avoid thread deadlocks if it was reserved
      by session
    */
    plugin::TransactionalStorageEngine::releaseTemporaryLatches(session);

    List<Item>::iterator li(items.begin());
    char buff[MAX_FIELD_WIDTH];
    String buffer(buff, sizeof(buff), &my_charset_bin);

    while (Item* item= li++)
    {
      item->send(session->getClient(), &buffer);
    }
    /* Insert this record to the Resultset into the cache */
    if (session->query_cache_key != "" && session->getResultsetMessage() != NULL)
      plugin::QueryCache::insertRecord(session, items);

    session->sent_row_count++;
    if (session->is_error())
      return true;
    return session->getClient()->flush();
  }
};

} /* namespace drizzled */

