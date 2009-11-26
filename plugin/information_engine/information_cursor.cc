/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <plugin/information_engine/information_cursor.h>
#include <drizzled/session.h>


#include <string>

using namespace std;
using namespace drizzled;

/*****************************************************************************
** INFORMATION_ENGINE tables
*****************************************************************************/

InformationCursor::InformationCursor(plugin::StorageEngine &engine_arg,
                                     TableShare &table_arg) :
  Cursor(engine_arg, table_arg)
{}

uint32_t InformationCursor::index_flags(uint32_t, uint32_t, bool) const
{
  return 0;
}

int InformationCursor::open(const char *name, int, uint32_t)
{
  string tab_name(name);
  string i_s_prefix("./information_schema/");
  tab_name.erase(0, i_s_prefix.length());
  share= ((InformationEngine *)engine)->getShare(tab_name);

  assert(share);
  thr_lock_data_init(&share->lock, &lock, NULL);

  return 0;
}

int InformationCursor::close(void)
{
  ((InformationEngine *)engine)->freeShare(share);

  return 0;
}

int InformationCursor::rnd_init(bool)
{
  plugin::InfoSchemaTable *sch_table= share->getInfoSchemaTable();
  if (sch_table)
  {
    /*
     * If the vector of rows is not empty, then we clear it here. Otherwise, it is possible that the
     * vector will contain duplicate rows.
     */
    if (! sch_table->getRows().empty())
    {
      sch_table->clearRows();
    }
    sch_table->fillTable(ha_session(),
                         table);
    iter= sch_table->getRows().begin();
  }
  return 0;
}


int InformationCursor::rnd_next(unsigned char *buf)
{
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  plugin::InfoSchemaTable *sch_table= share->getInfoSchemaTable();

  if (iter != sch_table->getRows().end() &&
      ! sch_table->getRows().empty())
  {
    (*iter)->copyRecordInto(buf);
    ++iter;
    return 0;
  }

  sch_table->clearRows();

  return HA_ERR_END_OF_FILE;
}


int InformationCursor::rnd_pos(unsigned char *, unsigned char *)
{
  ha_statistic_increment(&SSV::ha_read_rnd_count);
  return 0;
}


void InformationCursor::position(const unsigned char *)
{
}


int InformationCursor::info(uint32_t flag)
{
  memset(&stats, 0, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  return 0;
}


THR_LOCK_DATA **InformationCursor::store_lock(Session *session,
                                         THR_LOCK_DATA **to,
                                         enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK Table or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !session_tablespace_op(session))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT)
      lock_type = TL_READ;

    lock.type= lock_type;
  }
  *to++= &lock;

  return to;
}

