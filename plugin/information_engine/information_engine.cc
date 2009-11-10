/* Copyright (C) 2005 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <plugin/information_engine/information_engine.h>
#include <drizzled/plugin/info_schema_table.h>

using namespace std;

static const string engine_name("INFORMATION_ENGINE");

/*****************************************************************************
** INFORMATION_ENGINE tables
*****************************************************************************/

InformationCursor::InformationCursor(drizzled::plugin::StorageEngine *engine_arg,
                                     TableShare *table_arg) :
  Cursor(engine_arg, table_arg)
{}

uint32_t InformationCursor::index_flags(uint32_t, uint32_t, bool) const
{
  return 0;
}

int InformationCursor::open(const char *name, int, uint32_t)
{
  InformationShare *shareable;

  if (!(shareable= InformationShare::get(name)))
    return(HA_ERR_OUT_OF_MEM);

  thr_lock_data_init(&shareable->lock, &lock, NULL);

  return 0;
}

int InformationCursor::close(void)
{
  InformationShare::free(share);

  return 0;
}

void InformationEngine::doGetTableNames(CachedDirectory&, string& db, set<string>& set_of_names)
{
  if (db.compare("information_schema"))
    return;

  drizzled::plugin::InfoSchemaTable::getTableNames(set_of_names);
}



int InformationCursor::rnd_init(bool)
{
  return 0;
}


int InformationCursor::rnd_next(unsigned char *)
{
  return HA_ERR_END_OF_FILE;
}


int InformationCursor::rnd_pos(unsigned char *, unsigned char *)
{
  assert(1);

  return 0;
}


void InformationCursor::position(const unsigned char *)
{
  assert(1);
}


int InformationCursor::info(uint32_t flag)
{
  memset(&stats, 0, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  return(0);
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

static drizzled::plugin::StorageEngine *information_engine= NULL;

static int init(drizzled::plugin::Registry &registry)
{
  information_engine= new InformationEngine(engine_name);
  registry.add(information_engine);
  
  InformationShare::start();

  return 0;
}

static int finalize(drizzled::plugin::Registry &registry)
{
  registry.remove(information_engine);
  delete information_engine;

  InformationShare::stop();

  return 0;
}

drizzle_declare_plugin
{
  "INFORMATION_ENGINE",
  "1.0",
  "Sun Microsystems ala Brian Aker",
  "Engine which provides information schema tables",
  PLUGIN_LICENSE_GPL,
  init,     /* Plugin Init */
  finalize,     /* Plugin Deinit */
  NULL,               /* status variables */
  NULL,               /* system variables */
  NULL                /* config options   */
}
drizzle_declare_plugin_end;
