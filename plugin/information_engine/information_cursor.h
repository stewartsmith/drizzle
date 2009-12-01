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

#ifndef PLUGIN_INFORMATION_ENGINE_INFORMATION_CURSOR_H
#define PLUGIN_INFORMATION_ENGINE_INFORMATION_CURSOR_H

#include <drizzled/server_includes.h>
#include <drizzled/cursor.h>
#include <mysys/thr_lock.h>

#include <drizzled/plugin/info_schema_table.h>
#include <plugin/information_engine/information_engine.h>

class InformationCursor: public Cursor
{
private:
  THR_LOCK_DATA lock;      /* MySQL lock */
  InformationEngine::Share *share;
  drizzled::plugin::InfoSchemaTable::Rows::iterator iter;

public:
  InformationCursor(drizzled::plugin::StorageEngine &engine, TableShare &table_arg);
  ~InformationCursor() {}

  uint32_t index_flags(uint32_t inx, uint32_t part, bool all_parts) const;

  int open(const char *name, int mode, uint32_t test_if_locked);

  int close(void);

  int rnd_init(bool scan);

  /* get the next row and copy it into buf */
  int rnd_next(unsigned char *buf);

  /* locate row pointed to by pos, copy it into buf */
  int rnd_pos(unsigned char *buf, unsigned char *pos);

  /* record position of a record for reordering */
  void position(const unsigned char *record);

  int info(uint32_t flag);

  /**
   * @return an upper bound estimate for the number of rows in the table
   */
  ha_rows estimate_rows_upper_bound()
  {
    if (share)
    {
      drizzled::plugin::InfoSchemaTable *sch_table= share->getInfoSchemaTable();
      if (sch_table)
      {
        if (! sch_table->getRows().empty())
        {
          /* we multiply by 3 here to ensure enough memory is allocated for sort buffers */
          return (3 * sch_table->getRows().size());
        }
      }
    }
    return HA_POS_ERROR;
  }

  THR_LOCK_DATA **store_lock(Session *session,
                             THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
};

#endif /* PLUGIN_INFORMATION_ENGINE_INFORMATION_CURSOR_H */
