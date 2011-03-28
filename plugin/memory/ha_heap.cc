/* Copyright (C) 2000-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "heap_priv.h"
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/field/varstring.h>
#include <drizzled/plugin/daemon.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/util/test.h>
#include <drizzled/session/table_messages.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/system_variables.h>

#include <boost/thread/mutex.hpp>

#include "heap.h"
#include "ha_heap.h"

#include <string>

using namespace drizzled;
using namespace std;

static const string engine_name("MEMORY");

boost::mutex THR_LOCK_heap;

static const char *ha_heap_exts[] = {
  NULL
};

class HeapEngine : public plugin::StorageEngine
{
public:
  explicit HeapEngine(string name_arg) :
    plugin::StorageEngine(name_arg,
                          HTON_STATS_RECORDS_IS_EXACT |
                          HTON_NULL_IN_KEY |
                          HTON_FAST_KEY_READ |
                          HTON_NO_BLOBS |
                          HTON_HAS_RECORDS |
                          HTON_SKIP_STORE_LOCK |
                          HTON_TEMPORARY_ONLY)
  {
  }

  virtual ~HeapEngine()
  {
    hp_panic(HA_PANIC_CLOSE);
  }

  virtual Cursor *create(Table &table)
  {
    return new ha_heap(*this, table);
  }

  const char **bas_ext() const {
    return ha_heap_exts;
  }

  int doCreateTable(Session &session,
                    Table &table_arg,
                    const identifier::Table &identifier,
                    const message::Table &create_proto);

  /* For whatever reason, internal tables can be created by Cursor::open()
     for MEMORY.
     Instead of diving down a rat hole, let's just cry ourselves to sleep
     at night with this odd hackish workaround.
   */
  int heap_create_table(Session *session, const char *table_name,
                        Table *table_arg,
                        bool internal_table,
                        const message::Table &create_proto,
                        HP_SHARE **internal_share);

  int doRenameTable(Session&, const identifier::Table &from, const identifier::Table &to);

  int doDropTable(Session&, const identifier::Table &identifier);

  int doGetTableDefinition(Session& session,
                           const identifier::Table &identifier,
                           message::Table &table_message);

  uint32_t max_supported_keys()          const { return MAX_KEY; }
  uint32_t max_supported_key_part_length() const { return MAX_KEY_LENGTH; }

  uint32_t index_flags(enum  ha_key_alg ) const
  {
    return ( HA_ONLY_WHOLE_INDEX | HA_KEY_SCAN_NOT_ROR);
  }

  bool doDoesTableExist(Session& session, const identifier::Table &identifier);
  void doGetTableIdentifiers(CachedDirectory &directory,
                             const identifier::Schema &schema_identifier,
                             identifier::table::vector &set_of_identifiers);
};

void HeapEngine::doGetTableIdentifiers(CachedDirectory&,
                                       const identifier::Schema&,
                                       identifier::table::vector&)
{
}

bool HeapEngine::doDoesTableExist(Session& session, const identifier::Table &identifier)
{
  return session.getMessageCache().doesTableMessageExist(identifier);
}

int HeapEngine::doGetTableDefinition(Session &session,
                                     const identifier::Table &identifier,
                                     message::Table &table_proto)
{
  if (session.getMessageCache().getTableMessage(identifier, table_proto))
    return EEXIST;

  return ENOENT;
}
/*
  We have to ignore ENOENT entries as the MEMORY table is created on open and
  not when doing a CREATE on the table.
*/
int HeapEngine::doDropTable(Session &session, const identifier::Table &identifier)
{
  session.getMessageCache().removeTableMessage(identifier);

  int error= heap_delete_table(identifier.getPath().c_str());

  if (error == ENOENT)
    error= 0;

  return error;
}

static HeapEngine *heap_storage_engine= NULL;

static int heap_init(module::Context &context)
{
  heap_storage_engine= new HeapEngine(engine_name);
  context.add(heap_storage_engine);
  return 0;
}


/*****************************************************************************
** MEMORY tables
*****************************************************************************/

ha_heap::ha_heap(plugin::StorageEngine &engine_arg,
                 Table &table_arg)
  :Cursor(engine_arg, table_arg), file(0), records_changed(0), key_stat_version(0),
  internal_table(0)
{}

/*
  Hash index statistics is updated (copied from HP_KEYDEF::hash_buckets to
  rec_per_key) after 1/MEMORY_STATS_UPDATE_THRESHOLD fraction of table records
  have been inserted/updated/deleted. delete_all_rows() and table flush cause
  immediate update.

  NOTE
   hash index statistics must be updated when number of table records changes
   from 0 to non-zero value and vice versa. Otherwise records_in_range may
   erroneously return 0 and 'range' may miss records.
*/
#define MEMORY_STATS_UPDATE_THRESHOLD 10

int ha_heap::doOpen(const drizzled::identifier::Table &identifier, int mode, uint32_t test_if_locked)
{
  if ((test_if_locked & HA_OPEN_INTERNAL_TABLE) || (!(file= heap_open(identifier.getPath().c_str(), mode)) && errno == ENOENT))
  {
    internal_table= test(test_if_locked & HA_OPEN_INTERNAL_TABLE);
    file= 0;
    HP_SHARE *internal_share= NULL;
    message::Table create_proto;

    if (not heap_storage_engine->heap_create_table(getTable()->in_use,
                                                   identifier.getPath().c_str(),
                                                   getTable(),
                                                   internal_table,
                                                   create_proto,
                                                   &internal_share))
    {
        file= internal_table ?
          heap_open_from_share(internal_share, mode) :
          heap_open_from_share_and_register(internal_share, mode);
      if (!file)
      {
         /* Couldn't open table; Remove the newly created table */
        THR_LOCK_heap.lock();
        hp_free(internal_share);
        THR_LOCK_heap.unlock();
      }
    }
  }
  ref_length= sizeof(HEAP_PTR);
  if (file)
  {
    /* Initialize variables for the opened table */
    set_keys_for_scanning();
    /*
      We cannot run update_key_stats() here because we do not have a
      lock on the table. The 'records' count might just be changed
      temporarily at this moment and we might get wrong statistics (Bug
      #10178). Instead we request for update. This will be done in
      ha_heap::info(), which is always called before key statistics are
      used.
    */
    key_stat_version= file->getShare()->key_stat_version - 1;
  }
  return (file ? 0 : 1);
}

int ha_heap::close(void)
{
  return internal_table ? hp_close(file) : heap_close(file);
}


/*
  Create a copy of this table

  DESCRIPTION
    Do same as default implementation but use file->s->name instead of
    table->getShare()->path. This is needed by Windows where the clone() call sees
    '/'-delimited path in table->getShare()->path, while ha_peap::open() was called
    with '\'-delimited path.
*/

Cursor *ha_heap::clone(memory::Root *)
{
  Cursor *new_handler= getTable()->getMutableShare()->db_type()->getCursor(*getTable());
  identifier::Table identifier(getTable()->getShare()->getSchemaName(),
                             getTable()->getShare()->getTableName(),
                             getTable()->getShare()->getPath());

  if (new_handler && !new_handler->ha_open(identifier, getTable()->db_stat,
                                           HA_OPEN_IGNORE_IF_LOCKED))
    return new_handler;
  return NULL;
}


const char *ha_heap::index_type(uint32_t )
{
  return ("HASH");
}


/*
  Compute which keys to use for scanning

  SYNOPSIS
    set_keys_for_scanning()
    no parameter

  DESCRIPTION
    Set the bitmap btree_keys, which is used when the upper layers ask
    which keys to use for scanning. For each btree index the
    corresponding bit is set.

  RETURN
    void
*/

void ha_heap::set_keys_for_scanning(void)
{
}


void ha_heap::update_key_stats()
{
  for (uint32_t i= 0; i < getTable()->getShare()->sizeKeys(); i++)
  {
    KeyInfo *key= &getTable()->key_info[i];

    if (!key->rec_per_key)
      continue;

    {
      if (key->flags & HA_NOSAME)
        key->rec_per_key[key->key_parts-1]= 1;
      else
      {
        ha_rows hash_buckets= file->getShare()->keydef[i].hash_buckets;
        uint32_t no_records= hash_buckets ? (uint) (file->getShare()->records/hash_buckets) : 2;
        if (no_records < 2)
          no_records= 2;
        key->rec_per_key[key->key_parts-1]= no_records;
      }
    }
  }
  records_changed= 0;
  /* At the end of update_key_stats() we can proudly claim they are OK. */
  key_stat_version= file->getShare()->key_stat_version;
}


int ha_heap::doInsertRecord(unsigned char * buf)
{
  int res;
  if (getTable()->next_number_field && buf == getTable()->getInsertRecord())
  {
    if ((res= update_auto_increment()))
      return res;
  }
  res= heap_write(file,buf);
  if (!res && (++records_changed*MEMORY_STATS_UPDATE_THRESHOLD >
               file->getShare()->records))
  {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    file->getShare()->key_stat_version++;
  }
  return res;
}

int ha_heap::doUpdateRecord(const unsigned char * old_data, unsigned char * new_data)
{
  int res;

  res= heap_update(file,old_data,new_data);
  if (!res && ++records_changed*MEMORY_STATS_UPDATE_THRESHOLD >
              file->getShare()->records)
  {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    file->getShare()->key_stat_version++;
  }
  return res;
}

int ha_heap::doDeleteRecord(const unsigned char * buf)
{
  int res;

  res= heap_delete(file,buf);
  if (!res && getTable()->getShare()->getType() == message::Table::STANDARD &&
      ++records_changed*MEMORY_STATS_UPDATE_THRESHOLD > file->getShare()->records)
  {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    file->getShare()->key_stat_version++;
  }
  return res;
}

int ha_heap::index_read_map(unsigned char *buf, const unsigned char *key,
                            key_part_map keypart_map,
                            enum ha_rkey_function find_flag)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_key_count);
  int error = heap_rkey(file,buf,active_index, key, keypart_map, find_flag);
  getTable()->status = error ? STATUS_NOT_FOUND : 0;
  return error;
}

int ha_heap::index_read_last_map(unsigned char *buf, const unsigned char *key,
                                 key_part_map keypart_map)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_key_count);
  int error= heap_rkey(file, buf, active_index, key, keypart_map,
		       HA_READ_PREFIX_LAST);
  getTable()->status= error ? STATUS_NOT_FOUND : 0;
  return error;
}

int ha_heap::index_read_idx_map(unsigned char *buf, uint32_t index, const unsigned char *key,
                                key_part_map keypart_map,
                                enum ha_rkey_function find_flag)
{
  ha_statistic_increment(&system_status_var::ha_read_key_count);
  int error = heap_rkey(file, buf, index, key, keypart_map, find_flag);
  getTable()->status = error ? STATUS_NOT_FOUND : 0;
  return error;
}

int ha_heap::index_next(unsigned char * buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_next_count);
  int error=heap_rnext(file,buf);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::index_prev(unsigned char * buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_prev_count);
  int error=heap_rprev(file,buf);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::index_first(unsigned char * buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_first_count);
  int error=heap_rfirst(file, buf, active_index);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::index_last(unsigned char * buf)
{
  assert(inited==INDEX);
  ha_statistic_increment(&system_status_var::ha_read_last_count);
  int error=heap_rlast(file, buf, active_index);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::doStartTableScan(bool scan)
{
  return scan ? heap_scan_init(file) : 0;
}

int ha_heap::rnd_next(unsigned char *buf)
{
  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);
  int error=heap_scan(file, buf);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

int ha_heap::rnd_pos(unsigned char * buf, unsigned char *pos)
{
  int error;
  HEAP_PTR heap_position;
  ha_statistic_increment(&system_status_var::ha_read_rnd_count);
  memcpy(&heap_position, pos, sizeof(HEAP_PTR));
  error=heap_rrnd(file, buf, heap_position);
  getTable()->status=error ? STATUS_NOT_FOUND: 0;
  return error;
}

void ha_heap::position(const unsigned char *)
{
  *(HEAP_PTR*) ref= heap_position(file);	// Ref is aligned
}

int ha_heap::info(uint32_t flag)
{
  HEAPINFO hp_info;
  (void) heap_info(file,&hp_info,flag);

  errkey=                     hp_info.errkey;
  stats.records=              hp_info.records;
  stats.deleted=              hp_info.deleted;
  stats.mean_rec_length=      hp_info.reclength;
  stats.data_file_length=     hp_info.data_length;
  stats.index_file_length=    hp_info.index_length;
  stats.max_data_file_length= hp_info.max_records * hp_info.reclength;
  stats.delete_length=        hp_info.deleted * hp_info.reclength;
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= hp_info.auto_increment;
  /*
    If info() is called for the first time after open(), we will still
    have to update the key statistics. Hoping that a table lock is now
    in place.
  */
  if (key_stat_version != file->getShare()->key_stat_version)
    update_key_stats();
  return 0;
}

int ha_heap::extra(enum ha_extra_function operation)
{
  return heap_extra(file,operation);
}


int ha_heap::reset()
{
  return heap_reset(file);
}


int ha_heap::delete_all_rows()
{
  heap_clear(file);
  if (getTable()->getShare()->getType() == message::Table::STANDARD)
  {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    file->getShare()->key_stat_version++;
  }
  return 0;
}

/*
  Disable indexes.

  SYNOPSIS
    disable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      disable all non-unique keys
                HA_KEY_SWITCH_ALL          disable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE dis. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     dis. all keys and make persistent

  DESCRIPTION
    Disable indexes and clear keys to use for scanning.

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_NONUNIQ_SAVE  is not implemented with HEAP.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented with HEAP.

  RETURN
    0  ok
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_heap::disable_indexes(uint32_t mode)
{
  int error;

  if (mode == HA_KEY_SWITCH_ALL)
  {
    if (!(error= heap_disable_indexes(file)))
      set_keys_for_scanning();
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
  }
  return error;
}


/*
  Enable indexes.

  SYNOPSIS
    enable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      enable all non-unique keys
                HA_KEY_SWITCH_ALL          enable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE en. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     en. all keys and make persistent

  DESCRIPTION
    Enable indexes and set keys to use for scanning.
    The indexes might have been disabled by disable_index() before.
    The function works only if both data and indexes are empty,
    since the heap storage engine cannot repair the indexes.
    To be sure, call Cursor::delete_all_rows() before.

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_NONUNIQ_SAVE  is not implemented with HEAP.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented with HEAP.

  RETURN
    0  ok
    HA_ERR_CRASHED  data or index is non-empty. Delete all rows and retry.
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_heap::enable_indexes(uint32_t mode)
{
  int error;

  if (mode == HA_KEY_SWITCH_ALL)
  {
    if (!(error= heap_enable_indexes(file)))
      set_keys_for_scanning();
  }
  else
  {
    /* mode not implemented */
    error= HA_ERR_WRONG_COMMAND;
  }
  return error;
}


/*
  Test if indexes are disabled.

  SYNOPSIS
    indexes_are_disabled()
    no parameters

  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
   [2  non-unique indexes are disabled - NOT YET IMPLEMENTED]
*/

int ha_heap::indexes_are_disabled(void)
{
  return heap_indexes_are_disabled(file);
}

void ha_heap::drop_table(const char *)
{
  file->getShare()->delete_on_close= 1;
  close();
}


int HeapEngine::doRenameTable(Session &session, const identifier::Table &from, const identifier::Table &to)
{
  session.getMessageCache().renameTableMessage(from, to);
  return heap_rename(from.getPath().c_str(), to.getPath().c_str());
}


ha_rows ha_heap::records_in_range(uint32_t inx, key_range *min_key,
                                  key_range *max_key)
{
  KeyInfo *key= &getTable()->key_info[inx];

  if (!min_key || !max_key ||
      min_key->length != max_key->length ||
      min_key->length != key->key_length ||
      min_key->flag != HA_READ_KEY_EXACT ||
      max_key->flag != HA_READ_AFTER_KEY)
    return HA_POS_ERROR;			// Can only use exact keys

  if (stats.records <= 1)
    return stats.records;

  /* Assert that info() did run. We need current statistics here. */
  assert(key_stat_version == file->getShare()->key_stat_version);
  return key->rec_per_key[key->key_parts-1];
}

int HeapEngine::doCreateTable(Session &session,
                              Table &table_arg,
                              const identifier::Table &identifier,
                              const message::Table& create_proto)
{
  int error;
  HP_SHARE *internal_share;
  const char *table_name= identifier.getPath().c_str();

  error= heap_create_table(&session, table_name, &table_arg,
                           false, 
                           create_proto,
                           &internal_share);

  if (error == 0)
  {
    session.getMessageCache().storeTableMessage(identifier, create_proto);
  }

  return error;
}


int HeapEngine::heap_create_table(Session *session, const char *table_name,
                                  Table *table_arg,
                                  bool internal_table, 
                                  const message::Table &create_proto,
                                  HP_SHARE **internal_share)
{
  uint32_t key, parts, mem_per_row_keys= 0;
  uint32_t keys= table_arg->getShare()->sizeKeys();
  uint32_t auto_key= 0, auto_key_type= 0;
  uint32_t max_key_fieldnr = 0, key_part_size = 0, next_field_pos = 0;
  uint32_t column_count= table_arg->getShare()->sizeFields();
  std::vector<HP_KEYDEF> keydef;
  int error;
  bool found_real_auto_increment= 0;

  /* 
   * We cannot create tables with more rows than UINT32_MAX.  This is a
   * limitation of the HEAP engine.  Here, since TableShare::getMaxRows()
   * can return a number more than that, we trap it here instead of casting
   * to a truncated integer.
   */
  uint64_t num_rows= table_arg->getShare()->getMaxRows();
  if (num_rows > UINT32_MAX)
    return -1;

  for (key= parts= 0; key < keys; key++)
    parts+= table_arg->key_info[key].key_parts;

  keydef.resize(keys);
  std::vector<HA_KEYSEG> seg_buffer;
  seg_buffer.resize(parts);
  HA_KEYSEG *seg= &seg_buffer[0];

  for (key= 0; key < keys; key++)
  {
    KeyInfo *pos= &table_arg->key_info[key];
    KeyPartInfo *key_part=     pos->key_part;
    KeyPartInfo *key_part_end= key_part + pos->key_parts;

    keydef[key].keysegs=   (uint) pos->key_parts;
    keydef[key].flag=      (pos->flags & (HA_NOSAME | HA_NULL_ARE_EQUAL));
    keydef[key].seg=       seg;

    mem_per_row_keys+= sizeof(char*) * 2; // = sizeof(HASH_INFO)

    for (; key_part != key_part_end; key_part++, seg++)
    {
      Field *field= key_part->field;

      {
        if ((seg->type = field->key_type()) != (int) HA_KEYTYPE_TEXT &&
            seg->type != HA_KEYTYPE_VARTEXT1 &&
            seg->type != HA_KEYTYPE_VARTEXT2 &&
            seg->type != HA_KEYTYPE_VARBINARY1 &&
            seg->type != HA_KEYTYPE_VARBINARY2)
          seg->type= HA_KEYTYPE_BINARY;
      }
      seg->start=   (uint) key_part->offset;
      seg->length=  (uint) key_part->length;
      seg->flag=    key_part->key_part_flag;

      next_field_pos= seg->start + seg->length;
      if (field->type() == DRIZZLE_TYPE_VARCHAR)
      {
        next_field_pos+= (uint8_t)(((Field_varstring*)field)->pack_length_no_ptr());
      }

      if (next_field_pos > key_part_size) {
        key_part_size= next_field_pos;
      }

      if (field->flags & ENUM_FLAG)
        seg->charset= &my_charset_bin;
      else
        seg->charset= field->charset();
      if (field->null_ptr)
      {
	seg->null_bit= field->null_bit;
	seg->null_pos= (uint) (field->null_ptr - (unsigned char*) table_arg->getInsertRecord());
      }
      else
      {
	seg->null_bit= 0;
	seg->null_pos= 0;
      }
      if (field->flags & AUTO_INCREMENT_FLAG &&
          table_arg->found_next_number_field &&
          key == table_arg->getShare()->next_number_index)
      {
        /*
          Store key number and type for found auto_increment key
          We have to store type as seg->type can differ from it
        */
        auto_key= key+ 1;
	auto_key_type= field->key_type();
      }
      if ((uint)field->position() + 1 > max_key_fieldnr)
      {
        /* Do not use seg->fieldnr as it's not reliable in case of temp tables */
        max_key_fieldnr= field->position() + 1;
      }
    }
  }

  if (key_part_size < table_arg->getShare()->null_bytes + ((table_arg->getShare()->last_null_bit_pos+7) >> 3))
  {
    /* Make sure to include null fields regardless of the presense of keys */
    key_part_size = table_arg->getShare()->null_bytes + ((table_arg->getShare()->last_null_bit_pos+7) >> 3);
  }



  if (table_arg->found_next_number_field)
  {
    keydef[table_arg->getShare()->next_number_index].flag|= HA_AUTO_KEY;
    found_real_auto_increment= table_arg->getShare()->next_number_key_offset == 0;
  }
  HP_CREATE_INFO hp_create_info;
  hp_create_info.auto_key= auto_key;
  hp_create_info.auto_key_type= auto_key_type;
  hp_create_info.auto_increment= (create_proto.options().has_auto_increment_value() ?
				  create_proto.options().auto_increment_value() - 1 : 0);
  hp_create_info.max_table_size=session->variables.max_heap_table_size;
  hp_create_info.with_auto_increment= found_real_auto_increment;
  hp_create_info.internal_table= internal_table;
  hp_create_info.max_chunk_size= table_arg->getShare()->block_size;

  error= heap_create(table_name,
                     keys, &keydef[0],
                     column_count,
                     key_part_size,
                     table_arg->getShare()->getRecordLength(), mem_per_row_keys,
                     static_cast<uint32_t>(num_rows), /* We check for overflow above, so cast is fine here. */
                     0, // Factor out MIN
                     &hp_create_info, internal_share);

  return (error);
}


void ha_heap::get_auto_increment(uint64_t, uint64_t, uint64_t,
                                 uint64_t *first_value,
                                 uint64_t *nb_reserved_values)
{
  ha_heap::info(HA_STATUS_AUTO);
  *first_value= stats.auto_increment_value;
  /* such table has only table-level locking so reserves up to +inf */
  *nb_reserved_values= UINT64_MAX;
}


int ha_heap::cmp_ref(const unsigned char *ref1, const unsigned char *ref2)
{
  return memcmp(ref1, ref2, sizeof(HEAP_PTR));
}


DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "MEMORY",
  "1.0",
  "MySQL AB",
  "Hash based, stored in memory, useful for temporary tables",
  PLUGIN_LICENSE_GPL,
  heap_init,
  NULL,                       /* depends */
  NULL                        /* config options                  */
}
DRIZZLE_DECLARE_PLUGIN_END;
