/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <config.h>

#include <plugin/function_engine/cursor.h>
#include <drizzled/session.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/field/blob.h>
#include <drizzled/table.h>
#include <drizzled/statistics_variables.h>

#include <unistd.h>
#include <fcntl.h>

#include <string>

using namespace std;
using namespace drizzled;

/*****************************************************************************
** Data Function tables
*****************************************************************************/

FunctionCursor::FunctionCursor(plugin::StorageEngine &engine_arg,
                               Table &table_arg) :
  Cursor(engine_arg, table_arg),
  estimate_of_rows(100), // Completely fabricated, I used to use the value 2.
  rows_returned(0)
{}

int FunctionCursor::open(const char *name, int, uint32_t)
{
  tool= static_cast<Function *>(getEngine())->getFunction(name); 
//  assert(tool);

  row_cache_position= 0;

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
  generator= tool->generator(getTable()->getFields());

  return 0;
}


int FunctionCursor::rnd_next(unsigned char *)
{
  bool more_rows;
  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);

  /* Fix bug in the debug logic for field */
  for (Field **field= getTable()->getFields() ; *field ; field++)
  {
    (*field)->setWriteSet();
  }

  more_rows= generator->sub_populate(getTable()->getShare()->sizeFields());

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

uint32_t FunctionCursor::max_row_length()
{
  uint32_t length= (uint32_t)(getTable()->getRecordLength() + getTable()->sizeFields()*2);

  uint32_t *ptr, *end;
  for (ptr= getTable()->getBlobField(), end=ptr + getTable()->sizeBlobFields();
       ptr != end ;
       ptr++)
  {
      length += 2 + ((Field_blob*)getTable()->getField(*ptr))->get_length();
  }

  return length;
}

unsigned int FunctionCursor::pack_row(const unsigned char *record)
{
  unsigned char *ptr;

  record_buffer.resize(max_row_length());

  /* Copy null bits */
  memcpy(&record_buffer[0], record, getTable()->getShare()->null_bytes);
  ptr= &record_buffer[0] + getTable()->getShare()->null_bytes;

  for (Field **field=getTable()->getFields() ; *field ; field++)
  {
    if (!((*field)->is_null()))
      ptr= (*field)->pack(ptr, record + (*field)->offset(record));
  }

  return((unsigned int) (ptr - &record_buffer[0]));
}

void FunctionCursor::position(const unsigned char *record)
{
  uint32_t max_length= max_row_length();

  if (row_cache.size() <= row_cache_position + max_length)
  {
    row_cache.resize(row_cache.size() +  max_length);
  }

  unsigned int r_pack_length;
  r_pack_length= pack_row(record);
  internal::my_store_ptr(ref, ref_length, row_cache_position);

  memcpy(&row_cache[row_cache_position], &record_buffer[0], r_pack_length);
  row_cache_position+= r_pack_length;
}


void FunctionCursor::wipeCache()
{
  if (rows_returned > estimate_of_rows)
    estimate_of_rows= rows_returned;

  row_cache.clear();
  row_cache_position= 0;
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

  const unsigned char *ptr;
  ptr= &row_cache[position_id];

  /* Copy null bits */
  memcpy(buf, ptr, getTable()->getNullBytes());
  ptr+= getTable()->getNullBytes();
  // and copy fields
  for (Field **field= getTable()->getFields() ; *field ; field++)
  {
    if (!((*field)->is_null()))
    {
      ptr= (*field)->unpack(buf + (*field)->offset(getTable()->getInsertRecord()), ptr);
    }
  }

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
