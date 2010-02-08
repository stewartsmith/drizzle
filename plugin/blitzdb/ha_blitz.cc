/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 - 2010 Toru Maesaka
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

using namespace drizzled;

static int free_share(BlitzShare *share);
static pthread_mutex_t blitz_utility_mutex;
static TCMAP *blitz_table_cache;

int BlitzEngine::doCreateTable(Session *, const char *path, Table &table,
                               drizzled::message::Table &proto) {
  BlitzData dict;
  BlitzTree btree;
  int ecode;

  /* Create relevant files for a new table and close them immediately.
     All we want to do here is somewhat like UNIX touch(1). */
  if ((ecode = dict.create_data_table(proto, table, path)) != 0)
    return ecode;

  if ((ecode = dict.create_system_table(path)) != 0)
    return ecode;

  /* Create b+tree index(es) for this table. */
  for (uint32_t i = 0; i < table.s->keys; i++) {
    if ((ecode = btree.create(path, i)) != 0)
      return ecode;
  }

  /* Write the table definition to system table. */
  if ((ecode = dict.open_system_table(path, HDBOWRITER)) != 0)
    return ecode; 

  if (!dict.write_table_definition(proto)) {
    dict.close_system_table();
    return HA_ERR_CRASHED_ON_USAGE;
  }

  dict.close_system_table();
  return 0;
}

int BlitzEngine::doRenameTable(Session *, const char *from, const char *to) {
  BlitzData dict;
  BlitzTree btree;
  int err;

  if ((err = dict.open_data_table(from, HDBOREADER)) != 0)
    return err;

  uint32_t nkeys = dict.read_meta_keycount();
  dict.close_data_table();

  for (uint32_t i = 0; i < nkeys; i++) {
    if ((err = btree.rename(from, to, i)) != 0)
      return err;
  }

  return (dict.rename_table(from, to)) ? 0 : -1;
}

int BlitzEngine::doDropTable(Session &, const string path) {
  BlitzData dict;
  BlitzTree btree;
  char buf[FN_REFLEN];
  uint32_t nkeys;
  int err;

  /* We open the dictionary to extract meta data from it */
  if ((err = dict.open_data_table(path.c_str(), HDBOREADER)) != 0)
    return err;

  nkeys = dict.read_meta_keycount();

  /* We no longer need the dictionary to be open */
  dict.close_data_table();

  /* Drop the Data Dictionary */
  snprintf(buf, FN_REFLEN, "%s%s", path.c_str(), BLITZ_DATA_EXT);
  if ((err = unlink(buf)) == -1) {
    return err;
  }

  /* Drop the System Table */
  snprintf(buf, FN_REFLEN, "%s%s", path.c_str(), BLITZ_SYSTEM_EXT);
  if ((err = unlink(buf)) == -1) {
    return err;
  }

  /* Drop Index file(s) */
  for (uint32_t i = 0; i < nkeys; i++) {
    if ((err = btree.drop(path.c_str(), i)) != 0) {
      return err;
    }
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
    BlitzData db;
    char *proto_string;
    int proto_string_len;

    if (db.open_system_table(file_path, HDBOREADER) != 0) {
      pthread_mutex_unlock(&proto_cache_mutex);
      return HA_ERR_CRASHED_ON_USAGE;
    }

    proto_string = db.get_system_entry(BLITZ_TABLE_PROTO_KEY.c_str(),
                                       BLITZ_TABLE_PROTO_KEY.length(),
                                       &proto_string_len);
            
    if (db.close_system_table() != 0) {
      pthread_mutex_unlock(&proto_cache_mutex);
      return HA_ERR_CRASHED_ON_USAGE;
    }

    if (proto_string == NULL) {
      pthread_mutex_unlock(&proto_cache_mutex);
      return ENOMEM;
    }

    if (!proto->ParseFromArray(proto_string, proto_string_len)) {
      free(proto_string);
      pthread_mutex_unlock(&proto_cache_mutex);
      return HA_ERR_CRASHED_ON_USAGE;      
    }

    free(proto_string);
  }

  pthread_mutex_unlock(&proto_cache_mutex);
  return EEXIST;
}

void BlitzEngine::doGetTableNames(drizzled::CachedDirectory &directory, string&,
                                  set<string> &set_of_names) {
  drizzled::CachedDirectory::Entries entries = directory.getEntries();

  for (drizzled::CachedDirectory::Entries::iterator entry_iter = entries.begin();
       entry_iter != entries.end(); ++entry_iter) {

    drizzled::CachedDirectory::Entry *entry = *entry_iter;
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
                                            thread_locked(false),
                                            key_buffer(NULL),
                                            current_key(NULL),
                                            current_key_len(0),
                                            updateable_key(NULL),
                                            updateable_key_len(0),
                                            errkey_id(0) {}

int ha_blitz::open(const char *table_name, int, uint32_t) {
  if ((share = get_share(table_name)) == NULL)
    return HA_ERR_CRASHED_ON_USAGE;

  pthread_mutex_lock(&blitz_utility_mutex);

  /* TODO: Investigate whether this is enough. This _might_ not
           be enough for long multi byte character keys. */
  if ((key_buffer = (char *)malloc(BLITZ_MAX_KEY_LEN)) == NULL) {
    free_share(share);
    pthread_mutex_unlock(&blitz_utility_mutex);
    return HA_ERR_OUT_OF_MEM;
  }

  key_buffer_len = BLITZ_MAX_KEY_LEN;

  secondary_row_buffer = NULL;
  secondary_row_buffer_size = 0;

  /* 'ref_length' determines the size of the buffer that the kernel
     will use to uniquely identify a row. The actual allocation is
     done by the kernel so all we do here is specify the size of it.*/
  if (share->primary_key_exists) {
    ref_length = table->key_info[table->s->primary_key].key_length;
  } else {
    ref_length = sizeof(updateable_key_len) + sizeof(uint64_t);
  }

  pthread_mutex_unlock(&blitz_utility_mutex);
  return 0;
}

int ha_blitz::close(void) {
  free(key_buffer);
  free(secondary_row_buffer);
  key_buffer = NULL;
  secondary_row_buffer = NULL;
  return free_share(share);
}

int ha_blitz::info(uint32_t flag) {
  if (flag & HA_STATUS_VARIABLE) {
    stats.records = share->dict.nrecords(); 
    stats.data_file_length = share->dict.table_size();
  }

  /* TODO: Check if it's worthwhile to add 1 to this assignment */
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value = share->auto_increment_value;

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
                                               &current_key_len,
                                               &current_row,
                                               &current_row_len);
  }
  return 0;
}

int ha_blitz::rnd_next(unsigned char *drizzle_buf) {
  char *next_key;
  const char *next_row;
  int next_row_len;
  int next_key_len;

  free(updateable_key);
  updateable_key = NULL;

  if (current_key == NULL) {
    table->status = STATUS_NOT_FOUND;
    return HA_ERR_END_OF_FILE;
  }

  ha_statistic_increment(&SSV::ha_read_rnd_next_count);

  /* Unpack and copy the current row to Drizzle's result buffer. */
  unpack_row(drizzle_buf, current_row, current_row_len);

  /* Retrieve both key and row of the next record with one allocation. */
  next_key = share->dict.next_key_and_row(current_key, current_key_len,
                                          &next_key_len, &next_row,
                                          &next_row_len);

  /* Memory region for "current_row" will be freed as "updateable
     key" on the next iteration. This is because "current_key"
     points to the region of memory that contains "current_row" and
     "updateable_key" points to it. If there isn't another iteration
     then it is freed in rnd_end(). */
  current_row = next_row;
  current_row_len = next_row_len;

  /* Remember the current row because delete, update or replace
     function could be called after this function. This pointer is
     also used to free the previous key and row, which resides on
     the same buffer. */
  updateable_key = current_key;
  updateable_key_len = current_key_len;

  /* It is now memory-leak-safe to point current_key to next_key. */
  current_key = next_key;
  current_key_len = next_key_len;
  table->status = 0;
  return 0;
}

int ha_blitz::rnd_end() {
  if (current_key)
    free(current_key);
  if (updateable_key)
    free(updateable_key);

  current_key = NULL;
  updateable_key = NULL;
  current_key_len = 0;
  updateable_key_len = 0;
  table_scan = false;
  
  if (thread_locked)
    critical_section_exit();

  return 0;
}

int ha_blitz::rnd_pos(unsigned char *copy_to, unsigned char *pos) {
  char *row;
  char *key = NULL;
  int key_len, row_len;

  memcpy(&key_len, pos, sizeof(key_len));
  key = reinterpret_cast<char *>(pos + sizeof(key_len));

  /* TODO: Find a better error type. */
  if (key == NULL)
    return HA_ERR_KEY_NOT_FOUND;

  row = share->dict.get_row(key, key_len, &row_len);

  if (row == NULL)
    return HA_ERR_KEY_NOT_FOUND;

  unpack_row(copy_to, row, row_len);

  /* Let the thread remember the key location on memory if
     the thread is not doing a table scan. This is because
     either update_row() or delete_row() might be called
     after this function. */
  if (!table_scan) {
    current_key = key;
    current_key_len = key_len;
  }

  free(row);
  return 0;
}

void ha_blitz::position(const unsigned char *) {
  int length = sizeof(updateable_key_len);
  memcpy(ref, &updateable_key_len, length);
  memcpy(ref + length, reinterpret_cast<unsigned char *>(updateable_key),
         updateable_key_len);
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
  /* A PK in BlitzDB is the 'actual' key in the data dictionary.
     Therefore we do a direct lookup on the data dictionary. All
     other indexes are clustered btree. */
  if (key_num == table->s->primary_key) {
    char *pk, *fetched_row;
    int fetched_len, blitz_key_len;

    pk = native_to_blitz_key(key, key_num, &blitz_key_len);
    fetched_row = share->dict.get_row((const char *)pk, blitz_key_len,
                                      &fetched_len);

    if (fetched_row == NULL)
      return HA_ERR_KEY_NOT_FOUND;

    /* Found the row. Unpack it into Drizzle's buffer */
    unpack_row(buf, fetched_row, fetched_len);
    free(fetched_row);

    /* Now keep track of the key. This is because another function that
       needs this information such as delete_row() might be called before
       BlitzDB reaches index_end(). */
    memcpy(key_buffer, key, key_len);
    updateable_key = key_buffer;
    updateable_key_len = key_len;
    return 0;
  }

  return HA_ERR_UNSUPPORTED;
}

int ha_blitz::index_end(void) {
  updateable_key = NULL;
  updateable_key_len = 0;

  if (thread_locked)
    critical_section_exit();

  return 0;
}

int ha_blitz::write_row(unsigned char *drizzle_row) {
  size_t row_len = max_row_length();
  unsigned char *row_buf = get_pack_buffer(row_len);
  int rv;
  
  ha_statistic_increment(&SSV::ha_write_count);

  /* Prepare Auto Increment field if one exists. This logic is borrowed
     from Archive until we hack on multiple index support. */
  if (table->next_number_field && drizzle_row == table->record[0]) {
    if ((rv = update_auto_increment()) != 0)
      return rv;

    KEY *key = &table->s->key_info[0];
    uint64_t next_val = table->next_number_field->val_int();
    
    if (next_val <= share->auto_increment_value && key->flags & HA_NOSAME)
      return HA_ERR_FOUND_DUPP_KEY;

    if (next_val > share->auto_increment_value) {
      share->auto_increment_value = next_val;
      stats.auto_increment_value = share->auto_increment_value + 1;
    }
  }

  /* Serialize a primary key for this row. If a PK doesn't exist,
     an internal hidden ID will be generated. We obtain the PK here
     and pack it to this function's local buffer instead of the
     thread's own 'key_buffer' because the PK value needs to be
     remembered when writing non-PK keys AND because the 'key_buffer'
     will be used to generate these non-PK keys. */
  char temp_pkbuf[BLITZ_MAX_KEY_LEN];
  size_t pk_len = make_primary_key(temp_pkbuf, drizzle_row);

  row_len = pack_row(row_buf, drizzle_row);

  /* Loop over the keys and write them to it's exclusive tree.
     If writing to a certain index fails, we delete the entries in
     the reverse order that we wrote to the tree. */
  uint32_t curr_key = 0;

  /* We isolate this condition outside the key loop to avoid the CPU
     from going through unnecessary conditional branching on heavy
     insertion load. */
  if (share->primary_key_exists) {
    rv = share->btrees[curr_key].write_unique(temp_pkbuf, pk_len, "", 0);

    if (rv == HA_ERR_FOUND_DUPP_KEY) {
      this->errkey_id = curr_key;
      return rv;
    }
    curr_key = 1;
  }

  while (curr_key < share->nkeys) {
    /* TODO: write to tree(s) and check for error. */
    curr_key++;
  }

  /* Write the 'real' row to the Data Dictionary. */
  rv = share->dict.write_row(temp_pkbuf, pk_len, row_buf, row_len);

  /* TODO: Check rv for error. If writing to the dictionary had failed,
           we must delete all index entries for this row. What we need
           to do is: rewind_index(const int key_num_to_rewind_from). */
  return rv;
}

int ha_blitz::update_row(const unsigned char *old_row,
                         unsigned char *new_row) {
  size_t row_len = max_row_length();
  unsigned char *row_buf = get_pack_buffer(row_len);
  int rv = 0;

  ha_statistic_increment(&SSV::ha_update_count);

  if (thread_locked) {
    /* This is a really simple case where an UPDATE statement is
       requested to be processed in middle of a table scan. */
    if (table_scan) {
      row_len = pack_row(row_buf, new_row);
      rv = share->dict.write_row(updateable_key, updateable_key_len,
                                 row_buf, row_len); 
      return rv;
    }

    /* This is for: 'UPDATE t1 SET id = id+1 ORDER BY id LIMIT 2' */
    if (current_key) {
      row_len = pack_row(row_buf, new_row);
      rv = share->dict.write_row(current_key, current_key_len, row_buf, row_len);

      /* Don't free this pointer because drizzled will free the original */
      current_key = NULL;
      current_key_len = 0;
      return rv;
    }

    /* Check for Unique Constraint Violations. For now it only checks PK. */
    if ((rv = compare_rows_for_unique_violation(old_row, new_row)) != 0)
      return rv;

    /* Check if the old key is in the database. If it is then delete
       it before writing the row with a new key. TODO: delete_keys(); */
    char *key, *fetched;
    int klen, fetched_len;

    key = key_buffer;
    klen = make_index_key(key, table->s->primary_key, old_row);
    fetched = share->dict.get_row(key, klen, &fetched_len);

    if (fetched != NULL) {
      share->dict.delete_row(key, klen);
      free(fetched);
    }

    /* It is now safe to write the new row. */
    row_len = pack_row(row_buf, new_row);
    klen = make_index_key(key, table->s->primary_key, new_row);
    rv = share->dict.write_row(key, klen, row_buf, row_len);
  }

  return rv;
}

int ha_blitz::delete_row(const unsigned char *) {
  bool rv = false;

  ha_statistic_increment(&SSV::ha_delete_count);

  if (thread_locked)
    rv = share->dict.delete_row(updateable_key, updateable_key_len);

  return (rv) ? 0 : -1;
}

void ha_blitz::get_auto_increment(uint64_t, uint64_t,
                                  uint64_t, uint64_t *first_value,
                                  uint64_t *nb_reserved_values) {
  *first_value = share->auto_increment_value + 1;
  *nb_reserved_values = UINT64_MAX;
}

int ha_blitz::reset_auto_increment(uint64_t value) {
  share->auto_increment_value = (value == 0) ? 1 : value;
  return 0;
}

int ha_blitz::delete_all_rows(void) {
  for (uint32_t i = 0; i < share->nkeys; i++) {
    if (share->btrees[i].delete_all() != 0) {
      errkey = i;
      return HA_ERR_CRASHED_ON_USAGE;
    }
  }
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

size_t ha_blitz::make_primary_key(char *pack_to, const unsigned char *row) {
  if (!share->primary_key_exists) {
    uint64_t next_id = share->dict.next_hidden_row_id();
    int8store(pack_to, next_id);
    return sizeof(next_id);
  }

  /* Getting here means that there is a PK in this table. Get the
     binary representation of the PK, pack it to BlitzDB's key buffer
     and return the size of it. */
  return make_index_key(pack_to, table->s->primary_key, row);
}

size_t ha_blitz::make_index_key(char *pack_to, int key_num,
                                const unsigned char *row) {
  KEY *key = &table->key_info[key_num];
  KEY_PART_INFO *key_part = key->key_part;
  KEY_PART_INFO *key_part_end = key_part + key->key_parts;

  unsigned char *pos = (unsigned char *)pack_to;

  memset(pack_to, 0, BLITZ_MAX_KEY_LEN);

  /* Loop through key part(s) and pack them as we go. */
  for (; key_part != key_part_end; key_part++) {
    if (key_part->null_bit) {
      /* This means that the current key part is NULL. We therefore
         pack 0 to it and move on to the next key part. */
      if (row[key_part->null_offset] & key_part->null_bit) {
        *pos++ = 0;
        continue;
      }
      *pos++ = 1;
    }

    key_part->field->pack(pos, row + key_part->offset);
    pos += key_part->field->pack_length();
  }

  return ((char *)pos - pack_to);
}

/* Converts a native Drizzle index key to BlitzDB's format. */
char *ha_blitz::native_to_blitz_key(const unsigned char *native_key,
                                    const int key_num, int *return_key_len) {
  KEY *key = &table->key_info[key_num];
  KEY_PART_INFO *key_part = key->key_part;
  KEY_PART_INFO *key_part_end = key_part + key->key_parts;
  int total_key_len = 0;

  unsigned char *key_pos = (unsigned char *)native_key;
  unsigned char *keybuf_pos = (unsigned char *)this->key_buffer;

  memset(key_buffer, 0, BLITZ_MAX_KEY_LEN);

  for (; key_part != key_part_end; key_part++) {
    key_part->field->pack(keybuf_pos, key_pos);

    key_pos += key_part->field->key_length();
    keybuf_pos += key_part->field->pack_length();
    total_key_len += key_part->field->pack_length();
  }

  *return_key_len = total_key_len;
  return this->key_buffer;
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

  /* Unpack all fields in the provided row. */
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

BlitzShare *ha_blitz::get_share(const char *path) {
  int len, vlen;
  BlitzShare *share_ptr;

  pthread_mutex_lock(&blitz_utility_mutex);
  len = (int)strlen(path);

  /* Look up the table cache to see if the table resource is available */
  const void *cached = tcmapget(blitz_table_cache, path, len, &vlen);
  
  /* Check before dereferencing */
  if (cached) {
    share_ptr = *(BlitzShare **)cached;
    share_ptr->use_count++;
    pthread_mutex_unlock(&blitz_utility_mutex);
    return share_ptr;
  }

  /* Table wasn't cached so create a new table handler */
  share_ptr = new BlitzShare();

  /* Prepare the Data Dictionary */
  if (share_ptr->dict.startup(path) != 0) {
    delete share_ptr;
    pthread_mutex_unlock(&blitz_utility_mutex);
    return NULL;
  }

  /* Prepare Index Structure(s) */
  KEY *curr = table->s->key_info;
  share_ptr->btrees = new BlitzTree[table->s->keys];

  for (uint32_t i = 0; i < table->s->keys; i++, curr++) {
    share_ptr->btrees[i].open(path, i, BDBOWRITER);
    share_ptr->btrees[i].parts = new BlitzKeyPart[curr->key_parts];

    if (table->key_info[i].flags & HA_NOSAME)
      share_ptr->btrees[i].unique = true;

    share_ptr->btrees[i].nparts = curr->key_parts;

    /* Record Meta Data of the Key Segments */
    for (uint32_t j = 0; j < curr->key_parts; j++) {
      Field *f = curr->key_part[j].field;

      if (f->null_ptr) {
        share_ptr->btrees[i].parts[j].null_bitmask = f->null_bit;
        share_ptr->btrees[i].parts[j].null_pos
          = (uint32_t)(f->null_ptr - (unsigned char *)table->record[0]);
      }

      share_ptr->btrees[i].parts[j].flag = curr->key_part[j].key_part_flag;

      if (f->type() == DRIZZLE_TYPE_BLOB) {
        share_ptr->btrees[i].parts[j].flag |= HA_BLOB_PART;
      }

      share_ptr->btrees[i].parts[j].type = f->type();
      share_ptr->btrees[i].parts[j].offset = curr->key_part[j].offset;
      share_ptr->btrees[i].parts[j].length = curr->key_part[j].length;
    }
  }

  /* Set Meta Data */
  share_ptr->auto_increment_value = share_ptr->dict.read_meta_autoinc();
  share_ptr->table_name.append(path);
  share_ptr->nkeys = table->s->keys;
  share_ptr->use_count = 1;

  share_ptr->fixed_length_table = !(table->s->db_create_options
                                    & HA_OPTION_PACK_RECORD);

  if (table->s->primary_key >= MAX_KEY)
    share_ptr->primary_key_exists = false;
  else
    share_ptr->primary_key_exists = true;

  /* Cache the memory address of the object */
  tcmapput(blitz_table_cache, path, len, &share_ptr, sizeof(share_ptr));
  pthread_mutex_unlock(&blitz_utility_mutex);
  return share_ptr;
}

static int free_share(BlitzShare *share) {
  pthread_mutex_lock(&blitz_utility_mutex);

  /* BlitzShare could still be used by another thread. Check the
     reference counter to see if it's safe to free it */
  if (--share->use_count == 0) {
    share->dict.write_meta_autoinc(share->auto_increment_value);

    if (share->dict.shutdown() != 0) {
      pthread_mutex_unlock(&blitz_utility_mutex);
      return HA_ERR_CRASHED_ON_USAGE;
    }

    for (uint32_t i = 0; i < share->nkeys; i++) {
      delete[] share->btrees[i].parts;
      share->btrees[i].close();
    }

    tcmapout2(blitz_table_cache, share->table_name.c_str());
    delete[] share->btrees;
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
  /**
   * If MY_MUTEX_INIT_FAST becomes exposed in the API, then turn this
   * back on.
   */
  //pthread_mutex_init(&blitz_utility_mutex, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&blitz_utility_mutex, NULL);
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

DRIZZLE_PLUGIN(blitz_init, blitz_deinit, NULL, NULL);
