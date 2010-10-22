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

#include "config.h"

#include <plugin/function_engine/cursor.h>
#include <drizzled/session.h>
#include "drizzled/internal/my_sys.h"

#include <unistd.h>
#include <fcntl.h>

#include <string>

using namespace std;
using namespace drizzled;

/*****************************************************************************
** Data Function tables
*****************************************************************************/

FunctionCursor::FunctionCursor(plugin::StorageEngine &engine_arg,
                               TableShare &table_arg) :
  Cursor(engine_arg, table_arg),
  estimate_of_rows(100), // Completely fabricated, I used to use the value 2.
  rows_returned(0)
{}

int FunctionCursor::open(const char *name, int, uint32_t)
{
  tool= static_cast<Function *>(getEngine())->getFunction(name); 
//  assert(tool);

  record_id= 0;

  if (not tool)
    return HA_ERR_NO_SUCH_TABLE;

  return 0;
}

int FunctionCursor::close(void)
{
  tool= NULL;
  wipeCache();

  return 0;
}

int FunctionCursor::doStartTableScan(bool)
{
  rows_returned= 0;
  generator= tool->generator(table->getFields());

  return 0;
}


int FunctionCursor::rnd_next(unsigned char *)
{
  bool more_rows;
  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);

  /* Fix bug in the debug logic for field */
  for (Field **field= table->getFields() ; *field ; field++)
  {
    (*field)->setWriteSet();
  }

  more_rows= generator->sub_populate(table->getShare()->sizeFields());

  if (more_rows)
  {
    return 0;
  }
  else 
  {
    delete generator;
    generator= NULL;
  }
  rows_returned++;

  return more_rows ? 0 : HA_ERR_END_OF_FILE;
}

void FunctionCursor::position(const unsigned char *record)
{
  if (row_cache.size() <= record_id * table->getShare()->getRecordLength())
  {
    row_cache.resize(row_cache.size() + table->getShare()->getRecordLength() * 100); // Hardwired at adding an additional 100 rows of storage
  }
  memcpy(&row_cache[record_id * table->getShare()->getRecordLength()], record, table->getShare()->getRecordLength());
  internal::my_store_ptr(ref, ref_length, record_id);
  record_id++;
}


void FunctionCursor::wipeCache()
{
  if (rows_returned > estimate_of_rows)
    estimate_of_rows= rows_returned;

  row_cache.clear();
  record_id= 0;
}

int FunctionCursor::extra(enum ha_extra_function operation)
{
  switch (operation)
  {
  case drizzled::HA_EXTRA_CACHE:
    break;
  case drizzled::HA_EXTRA_NO_CACHE:
    break;
  case drizzled::HA_EXTRA_RESET_STATE:
    wipeCache();
    break;
  default:
    break;
  }

  return 0;
}

int FunctionCursor::doEndTableScan()
{ 
  delete generator; // Do this in case of an early exit from rnd_next()

  return 0;
}

int FunctionCursor::rnd_pos(unsigned char *buf, unsigned char *pos)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_count);
  size_t position_id= (size_t)internal::my_get_ptr(pos, ref_length);

  assert(position_id * table->getShare()->getRecordLength() < row_cache.size());
  memcpy(buf, &row_cache[position_id * table->getShare()->getRecordLength()], table->getShare()->getRecordLength());

  return 0;
}


int FunctionCursor::info(uint32_t flag)
{
  memset(&stats, 0, sizeof(stats));

  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;

  stats.records= estimate_of_rows;

  return 0;
}
