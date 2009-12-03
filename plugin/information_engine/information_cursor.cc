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

  return HA_ERR_END_OF_FILE;
}

class FindRowByChecksum
{
  uint32_t checksum;
public:
  FindRowByChecksum(uint32_t in_crc)
    :
      checksum(in_crc)
  {}

  inline bool operator()(const plugin::InfoSchemaRecord *rec) const
  {
    return rec->checksumMatches(checksum);
  }
};

void InformationCursor::position(const unsigned char *record)
{
  /* compute a hash of the row */
  uint32_t cs= drizzled::hash::crc32((const char *) record, table->s->reclength);
  /* copy the hash into ref */
  memcpy(ref, &cs, sizeof(uint32_t));
}

int InformationCursor::rnd_pos(unsigned char *buf, unsigned char *pos)
{
  ha_statistic_increment(&SSV::ha_read_rnd_count);
  /* get the checksum */
  uint32_t cs;
  memcpy(&cs, pos, sizeof(uint32_t));

  /* search for that data */
  plugin::InfoSchemaTable *sch_table= share->getInfoSchemaTable();
  plugin::InfoSchemaTable::Rows::iterator it= find_if(sch_table->getRows().begin(),
                                                      sch_table->getRows().end(),
                                                      FindRowByChecksum(cs));
  if (it != sch_table->getRows().end())
  {
    (*it)->copyRecordInto(buf);
  }

  return 0;
}


int InformationCursor::info(uint32_t flag)
{
  memset(&stats, 0, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  return 0;
}
