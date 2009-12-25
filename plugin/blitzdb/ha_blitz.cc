/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Toru Maesaka
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

#include "ha_blitz.h"
#include <sys/stat.h>

static BlitzShare *get_share(const char *table_name);
static int free_share(BlitzShare *share);
static pthread_mutex_t blitz_utility_mutex;
static TCMAP *blitz_table_cache;

/* Create relevant files for a new table and close them immediately.
   All we want to do here is somewhat like UNIX touch(1) */
int BlitzEngine::doCreateTable(Session *, const char *table_path,
                               Table &, drizzled::message::Table &proto) {
  BlitzData dict;
  int ecode;

  if ((ecode = dict.create_table(table_path, BLITZ_DATA_EXT)) != 0) {
    return ecode; 
  }

  if ((ecode = dict.create_table(table_path, BLITZ_SYSTEM_EXT)) != 0) {
    return ecode;
  }

  /* Write the table definition to system table */
  TCHDB *system_table;
  system_table = dict.open_table(table_path, BLITZ_SYSTEM_EXT, HDBOWRITER);

  if (system_table == NULL) {
    return HA_ERR_CRASHED_ON_USAGE;
  }

  if (!dict.write_table_definition(system_table, proto)) {
    dict.close_table(system_table);
    return HA_ERR_CRASHED_ON_USAGE;
  }

  dict.close_table(system_table);
  return 0;
}

int BlitzEngine::doRenameTable(Session *, const char *from, const char *to) {
  BlitzData dict;
  return (dict.rename_table(from, to)) ? 0 : -1;
}

int BlitzEngine::doDropTable(Session &, const string table_path) {
  int err;
  char name_buffer[FN_REFLEN];

  snprintf(name_buffer, FN_REFLEN, "%s%s", table_path.c_str(),
           BLITZ_DATA_EXT);

  if ((err = unlink(name_buffer)) == -1) {
    return err;
  }

  snprintf(name_buffer, FN_REFLEN, "%s%s", table_path.c_str(),
           BLITZ_SYSTEM_EXT);

  if ((err = unlink(name_buffer)) == -1) {
    return err;
  }
  return 0;
}

int BlitzEngine::doGetTableDefinition(Session&, const char *file_path,
                                      const char *, const char *,
                                      const bool,
                                      drizzled::message::Table *proto) {
  char name_buffer[FN_REFLEN];
  struct stat stat_info;

  pthread_mutex_lock(&proto_cache_mutex);

  snprintf(name_buffer, FN_REFLEN, "%s%s", file_path, BLITZ_SYSTEM_EXT);

  if (stat(name_buffer, &stat_info)) {
    pthread_mutex_unlock(&proto_cache_mutex);
    return errno;
  }

  if (proto) {
    BlitzData blitz;
    TCHDB *system_table;
    char *proto_string;
    int proto_string_length;

    system_table = blitz.open_table(file_path, BLITZ_SYSTEM_EXT, HDBOREADER);

    if (system_table == NULL) {
      pthread_mutex_unlock(&proto_cache_mutex);
      return HA_ERR_CRASHED_ON_USAGE;
    }

    proto_string = (char *)tchdbget(system_table,
                                    BLITZ_TABLE_PROTO_KEY.c_str(),
                                    BLITZ_TABLE_PROTO_KEY.length(),
                                    &proto_string_length);

    blitz.close_table(system_table);

    if (proto_string == NULL) {
      pthread_mutex_unlock(&proto_cache_mutex);
      return ENOMEM;
    }

    if (!proto->ParseFromArray(proto_string, proto_string_length)) {
      free(proto_string);
      pthread_mutex_unlock(&proto_cache_mutex);
      return HA_ERR_CRASHED_ON_USAGE;      
    }

    free(proto_string);
  }
  pthread_mutex_unlock(&proto_cache_mutex);
  return EEXIST;
}

void BlitzEngine::doGetTableNames(CachedDirectory &directory, string&,
                                  set<string> &set_of_names) {
  CachedDirectory::Entries entries = directory.getEntries();

  for (CachedDirectory::Entries::iterator entry_iter = entries.begin(); 
       entry_iter != entries.end(); ++entry_iter) {

    CachedDirectory::Entry *entry = *entry_iter;
    string *filename = &entry->filename;

    assert(filename->size());

    const char *ext = strchr(filename->c_str(), '.');

    if (ext != NULL) {
      char uname[NAME_LEN + 1];
      uint32_t file_name_len;

      file_name_len = filename_to_tablename(filename->c_str(), uname,
                                            sizeof(uname));
      uname[file_name_len - sizeof(BLITZ_DATA_EXT) + 1]= '\0';  
      set_of_names.insert(uname);
    }
  }
}

ha_blitz::ha_blitz(drizzled::plugin::StorageEngine &engine_arg,
                   TableShare &table_arg) : Cursor(engine_arg, table_arg),
                                            table_scan(false),
                                            current_key(NULL),
                                            current_key_length(0),
                                            updateable_key(NULL),
                                            updateable_key_length(0),
                                            errkey_id(0) {}

int ha_blitz::open(const char *table_name, int, uint32_t) {
  if ((share = get_share(table_name)) == NULL)
    return HA_ERR_CRASHED_ON_USAGE;

  pthread_mutex_lock(&blitz_utility_mutex);

  secondary_row_buffer = NULL;
  secondary_row_buffer_size = 0;

  share->nkeys = table->s->keys;
  share->fixed_length_table = !(table->s->db_create_options
                                & HA_OPTION_PACK_RECORD);

  if (table->s->primary_key >= MAX_KEY) {
    share->primary_key_exists = false;
    ref_length = sizeof(updateable_key_length) + sizeof(uint64_t);
  } else {
    share->primary_key_exists = true;

    /* TODO: This makes sense but it is asking for more memory
             than it should need in general. Investigate how to
             optimize it. One idea is to find the largest possible
             key size within the array. */
    ref_length = BLITZ_MAX_KEY_LENGTH;
  }

  pthread_mutex_unlock(&blitz_utility_mutex);
  return 0;
}

int ha_blitz::close(void) {
  free(secondary_row_buffer);
  return free_share(share);
}

int ha_blitz::info(uint32_t flag) {
  if (flag & HA_STATUS_VARIABLE) {
    stats.records = share->dict.nrecords(); 
    stats.data_file_length = share->dict.table_size();
  }

  if (flag & HA_STATUS_ERRKEY)
    errkey = errkey_id;

  return 0;
}

int ha_blitz::rnd_init(bool scan) {
  /* Obtain the most suitable lock for the given statement type */
  sql_command_type = session_sql_command(current_session);
  table_scan = scan;

  /* This is unlocked at rnd_end() */
  critical_section_enter();

  /* Get the first record from TCHDB. Let the scanner take
     care of checking return value errors. */
  if (table_scan) {
    current_key = share->dict.next_key_and_row(NULL, 0,
                                               &current_key_length,
                                               &current_row,
                                               &current_row_length);
  }
  return 0;
}

int ha_blitz::rnd_next(unsigned char *drizzle_buf) {
  char *next_key;
  const char *next_row;
  int next_row_length;
  int next_key_length;

  free(updateable_key);
  updateable_key = NULL;

  if (current_key == NULL)
    return HA_ERR_END_OF_FILE;

  ha_statistic_increment(&SSV::ha_read_rnd_next_count);

  /* Unpack and copy the current row to Drizzle's result buffer */
  unpack_row(drizzle_buf, current_row, current_row_length);

  /* Retrieve both key and row of the next record with one allocation */
  next_key = share->dict.next_key_and_row(current_key, current_key_length,
                                          &next_key_length, &next_row,
                                          &next_row_length);

  /* Memory region for "current_row" will be freed as "updateable
     key" on the next iteration. This is because "current_key"
     points to the region of memory that contains "current_row" and
     "updateable_key" points to it. If there isn't another iteration
     then it is freed in rnd_end(). */
  current_row = next_row;
  current_row_length = next_row_length;

  /* Remember the current row because delete, update or replace
     function could be called after this function. This pointer is
     also used to free the previous key and row, which resides on
     the same buffer. */
  updateable_key = current_key;
  updateable_key_length = current_key_length;

  /* It is now memory-leak-safe to point current_key to next_key. */
  current_key = next_key;
  current_key_length = next_key_length;
  return 0;
}

int ha_blitz::rnd_end() {
  if (current_key)
    free(current_key);
  if (updateable_key)
    free(updateable_key);

  current_key = NULL;
  updateable_key = NULL;
  current_key_length = 0;
  updateable_key_length = 0;
  table_scan = false;
  
  if (thread_locked)
    critical_section_exit();

  return 0;
}

int ha_blitz::rnd_pos(unsigned char *copy_to, unsigned char *pos) {
  char *row;
  char *key = NULL;
  int key_length, row_length;

  memcpy(&key_length, pos, sizeof(key_length));
  key = reinterpret_cast<char *>(pos + sizeof(key_length));

  /* TODO: Find a better error type. */
  if (key == NULL)
    return HA_ERR_KEY_NOT_FOUND;

  row = share->dict.get_row(key, key_length, &row_length);

  if (row == NULL)
    return HA_ERR_KEY_NOT_FOUND;

  unpack_row(copy_to, row, row_length);

  /* Let the thread remember the key location on memory if
     the thread is not doing a table scan. This is because
     either update_row() or delete_row() might be called
     after this function. */
  if (!table_scan) {
    current_key = key;
    current_key_length = key_length;
  }

  free(row);
  return 0;
}

void ha_blitz::position(const unsigned char *) {
  int length = sizeof(updateable_key_length);
  memcpy(ref, &updateable_key_length, length);
  memcpy(ref + length, reinterpret_cast<unsigned char *>(updateable_key),
         updateable_key_length);
}

const char *ha_blitz::index_type(uint32_t key_num) {
  return (key_num == table->s->primary_key) ? "HASH" : "BTREE";
}

int ha_blitz::index_init(uint32_t key_num, bool) {
  active_index = key_num;
  sql_command_type = session_sql_command(current_session);

  /* This is unlikely to happen but just for assurance, re-obtain
     the lock if this thread already has a certain lock. This makes
     sure that this thread will get the appropriate lock for the
     current statement. */
  if (thread_locked)
    critical_section_exit();

  critical_section_enter();
  return 0;
}

int ha_blitz::index_first(unsigned char *buf) {
  if (active_index == table->s->primary_key) {
    int length;
    char *first_row = share->dict.first_row(&length);

    if (first_row == NULL)
      return HA_ERR_KEY_NOT_FOUND;

    memcpy((char *)buf, first_row, length);
    free(first_row);
    return 0;
  }
  return HA_ERR_UNSUPPORTED;
}

int ha_blitz::index_read(unsigned char *buf, const unsigned char *key,
                         uint32_t key_len, enum ha_rkey_function find_flag) {

  /* In the future we will not need this condition because all access
     to the index will be done through index_read_idx(). For now we need
     it because we're only concerned with making PK work. */
  if (active_index == table->s->primary_key)
    return index_read_idx(buf, active_index, key, key_len, find_flag);

  return HA_ERR_UNSUPPORTED;
}

/* This is where the read related index logic lives. It is used by both
   BlitzDB and the Database Kernel (specifically, by the optimizer). */
int ha_blitz::index_read_idx(unsigned char *buf, uint32_t key_num,
                             const unsigned char *key, uint32_t key_len,
                             enum ha_rkey_function /*find_flag*/) {
  char *fetched_row;
  int fetched_len;

  /* A PK in BlitzDB is the 'actual' key in the data dictionary.
     Therefore we do a direct lookup on the data dictionary. All
     other indexes are clustered btree. */
  if (key_num == table->s->primary_key) {
    fetched_row = share->dict.get_row((const char *)key, key_len, &fetched_len);

    if (fetched_row == NULL)
      return HA_ERR_KEY_NOT_FOUND;

    /* Found the row. Copy it to drizzle's result buffer. TODO: Memory copy
       operation SUCKS. Find a way to make TC use this buffer directly. */
    memcpy(buf, fetched_row, fetched_len);
    free(fetched_row);

    /* Now keep track of the key. This is because another function that
       needs this information such as delete_row() might be called before
       BlitzDB reaches index_end() */
    memcpy(key_buffer, key, key_len);
    updateable_key = key_buffer;
    updateable_key_length = key_len;
    return 0;
  }

  return HA_ERR_UNSUPPORTED;
}

int ha_blitz::index_end(void) {
  updateable_key = NULL;
  updateable_key_length = 0;

  if (thread_locked)
    critical_section_exit();

  return 0;
}

int ha_blitz::write_row(unsigned char *drizzle_row) {
  size_t row_length = max_row_length();
  unsigned char *row_buf = get_pack_buffer(row_length);
  int rv;
  
  ha_statistic_increment(&SSV::ha_write_count);

  /* Serialize a primary key for this row. If a PK doesn't exist,
     an internal hidden ID will be packed into primary_key_buffer. */
  generate_table_key();

  row_length = pack_row(row_buf, drizzle_row);

  /* Write index key(s) to the appropriate BlitzTree object
     EXCEPT for the PRIMARY KEY. This is because PK is treated
     as a data-dictionary-key in BlitzDB. The internal key ID
     for a PK (if it exists) seems guaranteed to be 0. Therefore
     if a PK exists in this table, we skip the first element of
     the KEY array. */
  uint32_t i = (share->primary_key_exists) ? 1 : 0;

  /* NOTE: This means that the key is UNIQUE. Remeber it for future
     usage. if (key->flags & HA_NOSAME) { no duplicates } */
  while (i < share->nkeys) {
    /* This is where we iterate over the keys and write them
       to the appropriate BTREE file(s). Don't implement this
       body yet since we're only interested in supporting one
       index to begin with, which is PK.

       KEY *current_index = &table->key_info[i]; */
    i++;
  }

  /* Write the 'real' row to the Data Dictionary. If a primary key exists,
     it will be used as the key _and_ checked for key duplication. Otherwise
     a hidden 8 byte key (incremental uint64_t) will be used as the key. */
  if (share->primary_key_exists) {
    rv = share->dict.write_unique_row(primary_key_buffer, primary_key_length,
                                      row_buf, row_length);
    if (rv == HA_ERR_FOUND_DUPP_KEY)
      this->errkey_id = table->s->primary_key;

  } else {
    rv = share->dict.write_row(primary_key_buffer, primary_key_length,
                               row_buf, row_length);
  }

  return rv;
}

int ha_blitz::update_row(const unsigned char *,
                         unsigned char *new_row) {
  size_t row_length = max_row_length();
  unsigned char *row_buf = get_pack_buffer(row_length);
  int rv = -1;

  ha_statistic_increment(&SSV::ha_update_count);

  if (thread_locked) {
    /* This is a really simple case where an UPDATE statement is
       requested to be processed in middle of a table scan. */
    if (table_scan) {
      row_length = pack_row(row_buf, new_row);
      rv = share->dict.write_row(updateable_key, updateable_key_length,
                                 row_buf, row_length); 
      return rv;
    }

    /* When updating cached rows, Drizzle and MySQL calls rnd_pos()
       prior to calling update_row(). If this is a keyless table,
       BlitzDB has kept a pointer to the key on memory and the length
       of it in 'current_key' and 'current_key_length'.

       Furthermore, it is impossible for the key value to have updated 
       if a primary key isn't defined. This is because Drizzle knows
       nothing about BlitzDB's internal row-id for keyless tables. */
    if (share->primary_key_exists) {
      /* Take a diff beween primary keys of the old row and the new row.
         If it has changed then do the appropriate thing:

         (1) If updated, then see if the new primary key doesn't clash
             with an existing row. If it doesn't then write a new row
             with it then delete a row from BlitzDB based on the old
             primary key.

         This is unimplemented at the moment because index support for
         BlitzDB hasn't been worked on yet. */
    }

    row_length = pack_row(row_buf, new_row);
    rv = share->dict.write_row(current_key, current_key_length,
                               row_buf, row_length); 

    /* Don't free this pointer because drizzled will */
    if (current_key) {
      current_key = NULL;
      current_key_length = 0;
    }
  }
  return rv;
}

int ha_blitz::delete_row(const unsigned char *) {
  bool rv = false;

  ha_statistic_increment(&SSV::ha_delete_count);

  if (thread_locked)
    rv = share->dict.delete_row(updateable_key, updateable_key_length);

  return (rv) ? 0 : -1;
}

int ha_blitz::delete_all_rows(void) {
  return (share->dict.delete_all_rows()) ? 0 : -1;
}

uint32_t ha_blitz::max_row_length(void) {
  uint32_t length = (table->getRecordLength() + table->sizeFields() * 2);
  uint32_t *pos = table->getBlobField();
  uint32_t *end = pos + table->sizeBlobFields();

  while (pos != end) {
    length += 2 + ((Field_blob*)table->field[*pos])->get_length();
    pos++;
  }

  return length;
}

/* Note that primary_key_buffer is a thread local buffer */
size_t ha_blitz::generate_table_key(void) {
  if (!share->primary_key_exists) {
    uint64_t next_id = share->dict.next_hidden_row_id();
    int8store(primary_key_buffer, next_id);
    primary_key_length = sizeof(next_id);
    return primary_key_length; 
  }

  /* Getting here means that there is a PK in this table. Get the
     binary representation of the PK, pack it to BlitzDB's key buffer
     and return the size of it. */
  primary_key_length = pack_index_key(primary_key_buffer,
                                      table->s->primary_key);
  return primary_key_length;
}

size_t ha_blitz::pack_index_key(char *pack_to, int key_num) {
  KEY *key = &table->key_info[key_num];
  KEY_PART_INFO *key_part = key->key_part;
  KEY_PART_INFO *key_part_end = key_part + key->key_parts;
  size_t packed_length = 0;

  unsigned char *pos = (unsigned char *)pack_to;

  /* Loop through key part(s) and pack them. Don't worry about NULL key
     values since this functionality is currently disabled in BlitzDB.*/
  for (; key_part != key_part_end; key_part++) {
    key_part->field->pack(pos, key_part->field->ptr);
    pos += key_part->field->pack_length();
    packed_length += key_part->field->pack_length();
  }

  return packed_length;
}

size_t ha_blitz::pack_row(unsigned char *row_buffer,
                          unsigned char *row_to_pack) {
  unsigned char *pos;

  /* Nothing special to do if the table is fixed length */
  if (share->fixed_length_table) {
    memcpy(row_buffer, row_to_pack, table->s->reclength);
    return (size_t)table->s->reclength;
  }

  /* Copy NULL bits */
  memcpy(row_buffer, row_to_pack, table->s->null_bytes);
  pos = row_buffer + table->s->null_bytes;

  /* Pack each field into the buffer */
  for (Field **field = table->field; *field; field++) {
    if (!((*field)->is_null()))
      pos = (*field)->pack(pos, row_to_pack + (*field)->offset(row_to_pack));
  }

  return (size_t)(pos - row_buffer);
}

bool ha_blitz::unpack_row(unsigned char *to, const char *from,
                          const size_t from_len) {
  const unsigned char *pos;

  /* Nothing special to do */
  if (share->fixed_length_table) {
    memcpy(to, from, from_len);
    return true;
  }

  /* Start by copying NULL bits which is the beginning block
     of a Drizzle row. */
  pos = (const unsigned char *)from;
  memcpy(to, pos, table->s->null_bytes);
  pos += table->s->null_bytes;

  /* Unpack all fields in the provided row */
  for (Field **field = table->field; *field; field++) {
    if (!((*field)->is_null())) {
      pos = (*field)->unpack(to + (*field)->offset(table->record[0]), pos);
    }
  }

  return true;
}

unsigned char *ha_blitz::get_pack_buffer(const size_t size) {
  unsigned char *buf = pack_buffer;

  /* This is a shitty case where the row size is larger than 2KB. */
  if (size > BLITZ_MAX_ROW_STACK) {
    if (size > secondary_row_buffer_size) {
      void *new_ptr = realloc(secondary_row_buffer, size);

      if (new_ptr == NULL) {
        errno = HA_ERR_OUT_OF_MEM;
        return NULL;
      }

      secondary_row_buffer_size = size;
      secondary_row_buffer = (unsigned char *)new_ptr;
      buf = secondary_row_buffer;
    }
  }
  return buf;
}

static BlitzEngine *blitz_engine = NULL;

static BlitzShare *get_share(const char *table_name) {
  int length, vlen;
  BlitzShare *share;

  pthread_mutex_lock(&blitz_utility_mutex);
  length = (int)strlen(table_name);

  /* Look up the table cache to see if the table resource is available */
  const void *cached = tcmapget(blitz_table_cache, table_name, length, &vlen);
  
  /* Check before dereferencing */
  if (cached) {
    share = *(BlitzShare **)cached;
    share->use_count++;
    pthread_mutex_unlock(&blitz_utility_mutex);
    return share;
  }

  /* Table wasn't cached so create a new table handler */
  share = new BlitzShare();

  /* Allocate and open all necessary resources for this table */
  if (!share->dict.startup(table_name)) {
    share->dict.shutdown();
    delete share;
    pthread_mutex_unlock(&blitz_utility_mutex);
    return NULL;
  }

  share->table_name.append(table_name);
  share->use_count = 1;

  /* Cache the memory address of the object */
  tcmapput(blitz_table_cache, table_name, length, &share, sizeof(share));
  pthread_mutex_unlock(&blitz_utility_mutex);
  return share;
}

static int free_share(BlitzShare *share) {
  pthread_mutex_lock(&blitz_utility_mutex);

  /* BlitzShare could still be used by another thread. Check the
     reference counter to see if it's safe to free it */
  if (--share->use_count == 0) {
    if (!share->dict.shutdown()) {
      pthread_mutex_unlock(&blitz_utility_mutex);
      return HA_ERR_CRASHED_ON_USAGE;
    }
    tcmapout2(blitz_table_cache, share->table_name.c_str());
    delete share;
  }

  pthread_mutex_unlock(&blitz_utility_mutex);
  return 0;
}

static int blitz_init(drizzled::plugin::Registry &registry) {
  blitz_engine = new BlitzEngine("BLITZDB"); 
  if ((blitz_table_cache = tcmapnew()) == NULL) {
    delete blitz_engine;
    return HA_ERR_OUT_OF_MEM;
  }
  pthread_mutex_init(&blitz_utility_mutex, MY_MUTEX_INIT_FAST);
  registry.add(blitz_engine);
  return 0;
}

static int blitz_deinit(drizzled::plugin::Registry &registry) {
  pthread_mutex_destroy(&blitz_utility_mutex);
  tcmapdel(blitz_table_cache);
  registry.remove(blitz_engine);
  delete blitz_engine;
  return 0;
}

DRIZZLE_DECLARE_PLUGIN {
  DRIZZLE_VERSION_ID,
  "BLITZDB",
  "0.3",
  "Toru Maesaka",
  "BlitzDB Storage Engine",
  PLUGIN_LICENSE_GPL,
  blitz_init,    /*  Plugin Init      */
  blitz_deinit,  /*  Plugin Deinit    */
  NULL,          /*  status variables */
  NULL,          /*  system variables */
  NULL           /*  config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
