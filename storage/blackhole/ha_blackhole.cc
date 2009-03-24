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

#include <drizzled/server_includes.h>
#include <drizzled/table.h>
#include "ha_blackhole.h"

/* Static declarations for StorageEngine */

static handler *blackhole_create_handler(StorageEngine *engine,
                                         TABLE_SHARE *table,
                                         MEM_ROOT *mem_root)
{
  return new (mem_root) ha_blackhole(engine, table);
}


/* Static declarations for shared structures */

static pthread_mutex_t blackhole_mutex;
static HASH blackhole_open_tables;

static st_blackhole_share *get_share(const char *table_name);
static void free_share(st_blackhole_share *share);

/*****************************************************************************
** BLACKHOLE tables
*****************************************************************************/

ha_blackhole::ha_blackhole(StorageEngine *engine,
                           TABLE_SHARE *table_arg)
  :handler(engine, table_arg)
{}


static const char *ha_blackhole_exts[] = {
  NULL
};

const char **ha_blackhole::bas_ext() const
{
  return ha_blackhole_exts;
}


uint32_t ha_blackhole::index_flags(uint32_t inx, uint32_t, bool) const
{
  return ((table_share->key_info[inx].algorithm == HA_KEY_ALG_FULLTEXT) ?
          0 : HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE |
          HA_READ_ORDER | HA_KEYREAD_ONLY);
}

int ha_blackhole::open(const char *name, int, uint32_t)
{
  if (!(share= get_share(name)))
    return(HA_ERR_OUT_OF_MEM);

  thr_lock_data_init(&share->lock, &lock, NULL);
  return(0);
}

int ha_blackhole::close(void)
{
  free_share(share);
  return(0);
}

int ha_blackhole::create(const char *, Table *, HA_CREATE_INFO *)
{
  return(0);
}

const char *ha_blackhole::index_type(uint32_t)
{
  return("BTREE");
}

int ha_blackhole::write_row(unsigned char *)
{
  return(table->next_number_field ? update_auto_increment() : 0);
}

int ha_blackhole::rnd_init(bool)
{
  return(0);
}


int ha_blackhole::rnd_next(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int ha_blackhole::rnd_pos(unsigned char *, unsigned char *)
{
  assert(0);
  return(0);
}


void ha_blackhole::position(const unsigned char *)
{
  assert(0);
  return;
}


int ha_blackhole::info(uint32_t flag)
{
  memset(&stats, 0, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  return(0);
}

int ha_blackhole::external_lock(Session *, int)
{
  return(0);
}


THR_LOCK_DATA **ha_blackhole::store_lock(Session *session,
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
         lock_type <= TL_WRITE) && !session_in_lock_tables(session)
        && !session_tablespace_op(session))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT && !session_in_lock_tables(session))
      lock_type = TL_READ;

    lock.type= lock_type;
  }
  *to++= &lock;
  return(to);
}


int ha_blackhole::index_read_map(unsigned char *, const unsigned char *,
                                 key_part_map, enum ha_rkey_function)
{
  return(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_read_idx_map(unsigned char *, uint32_t, const unsigned char *,
                                     key_part_map, enum ha_rkey_function)
{
  return(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_read_last_map(unsigned char *, const unsigned char *, key_part_map)
{
  return(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_next(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_prev(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_first(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int ha_blackhole::index_last(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


static st_blackhole_share *get_share(const char *table_name)
{
  st_blackhole_share *share;
  uint32_t length;

  length= (uint) strlen(table_name);
  pthread_mutex_lock(&blackhole_mutex);

  if (!(share= (st_blackhole_share*) hash_search(&blackhole_open_tables,
                                                 (unsigned char*) table_name, length)))
  {
    if (!(share= (st_blackhole_share*) malloc(sizeof(st_blackhole_share) +
                                              length)))
      goto error;
    memset(share, 0, sizeof(st_blackhole_share) + length);

    share->table_name_length= length;
    strcpy(share->table_name, table_name);

    if (my_hash_insert(&blackhole_open_tables, (unsigned char*) share))
    {
      free((unsigned char*) share);
      share= NULL;
      goto error;
    }

    thr_lock_init(&share->lock);
  }
  share->use_count++;

error:
  pthread_mutex_unlock(&blackhole_mutex);
  return share;
}

static void free_share(st_blackhole_share *share)
{
  pthread_mutex_lock(&blackhole_mutex);
  if (!--share->use_count)
    hash_delete(&blackhole_open_tables, (unsigned char*) share);
  pthread_mutex_unlock(&blackhole_mutex);
}

static void blackhole_free_key(st_blackhole_share *share)
{
  thr_lock_delete(&share->lock);
  free((unsigned char*) share);
}

static unsigned char* blackhole_get_key(st_blackhole_share *share, size_t *length, bool)
{
  *length= share->table_name_length;
  return (unsigned char*) share->table_name;
}

static int blackhole_init(void *p)
{
  StorageEngine *blackhole_engine;
  blackhole_engine= (StorageEngine *)p;
  blackhole_engine->state= SHOW_OPTION_YES;
  blackhole_engine->create= blackhole_create_handler;
  blackhole_engine->flags= HTON_CAN_RECREATE;

  pthread_mutex_init(&blackhole_mutex, MY_MUTEX_INIT_FAST);
  (void) hash_init(&blackhole_open_tables, system_charset_info,32,0,0,
                   (hash_get_key) blackhole_get_key,
                   (hash_free_key) blackhole_free_key, 0);

  return 0;
}

static int blackhole_fini(void *)
{
  hash_free(&blackhole_open_tables);
  pthread_mutex_destroy(&blackhole_mutex);

  return 0;
}

drizzle_declare_plugin(blackhole)
{
  DRIZZLE_STORAGE_ENGINE_PLUGIN,
  "BLACKHOLE",
  "1.0",
  "MySQL AB",
  "/dev/null storage engine (anything you write to it disappears)",
  PLUGIN_LICENSE_GPL,
  blackhole_init, /* Plugin Init */
  blackhole_fini, /* Plugin Deinit */
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
drizzle_declare_plugin_end;
