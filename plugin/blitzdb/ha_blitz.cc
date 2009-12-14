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

  if ((ecode = dict.create_table(table_path, BLITZ_DATAFILE_EXT)) != 0) {
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
           BLITZ_DATAFILE_EXT);

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

  /* Check if the system table exists */
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
      uname[file_name_len - sizeof(BLITZ_DATAFILE_EXT) + 1]= '\0';  
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
                                            updateable_key_length(0) {}

int ha_blitz::open(const char *table_name, int, uint32_t) {
  if ((share = get_share(table_name)) == NULL)
    return HA_ERR_CRASHED_ON_USAGE;

  pthread_mutex_lock(&blitz_utility_mutex);

  secondary_row_buffer = NULL;
  secondary_row_buffer_size = 0;

  share->fixed_length_table = !(table->s->db_create_options
                                & HA_OPTION_PACK_RECORD);

  if (table->s->primary_key >= MAX_KEY) {
    share->primary_key_exists = false;
    ref_length = sizeof(updateable_key_length) + sizeof(uint64_t);
  } else {
    share->primary_key_exists = true;
    /* TODO: Set ref_length to something safe and appropriate
             when BlitzDB supports indexes. */
  }

  thr_lock_data_init(&share->lock, &lock, NULL);
  pthread_mutex_unlock(&blitz_utility_mutex);
  return 0;
}

int ha_blitz::close(void) {
  free(secondary_row_buffer);
  return free_share(share);
}

int ha_blitz::info(uint32_t flag) {
  if (flag & HA_STATUS_VARIABLE)
    stats.records = share->dict.nrecords(); 

  return 0;
}

int ha_blitz::rnd_init(bool scan) {
  /* Obtain the most suitable lock for the given statement type */
  critical_section_enter();

  /* Store this information in the thread for later use */
  table_scan = scan;

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

  /* Unpack and copy the current row to Drizzle's result buffer */
  unpack_row(drizzle_buf, current_row);

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

  unpack_row(copy_to, row);

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

int ha_blitz::write_row(unsigned char *drizzle_row) {
  unsigned char *buffer_pos = get_pack_buffer();
  uint32_t row_length = max_row_length();
  bool rv;
  
  ha_statistic_increment(&SSV::ha_write_count);
  generated_key_length = share->dict.generate_table_key(key_buffer);
  row_length = pack_row(buffer_pos, drizzle_row);

  rv = share->dict.overwrite_row(key_buffer, generated_key_length,
                                 buffer_pos, row_length);
  return (rv) ? 0 : -1;
}

int ha_blitz::update_row(const unsigned char *,
                         unsigned char *new_row) {
  uint32_t row_length = max_row_length();
  unsigned char *buffer_pos = get_pack_buffer();
  bool rv = false;

  ha_statistic_increment(&SSV::ha_update_count);

  if (thread_locked) {
    /* This is a really simple case where an UPDATE statement is
       requested to be processed in middle of a table scan. */
    if (table_scan) {
      row_length = pack_row(buffer_pos, new_row);
      rv = share->dict.overwrite_row(updateable_key, updateable_key_length,
                                     buffer_pos, row_length); 
      return (rv) ? 0 : -1;
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

    row_length = pack_row(buffer_pos, new_row);
    rv = share->dict.overwrite_row(current_key, current_key_length,
                                   buffer_pos, row_length); 

    /* Don't free this pointer because drizzled will */
    if (current_key) {
      current_key = NULL;
      current_key_length = 0;
    }
  }
  return (rv) ? 0 : -1;
}

int ha_blitz::delete_row(const unsigned char *) {
  bool rv = false;

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

bool ha_blitz::unpack_row(unsigned char *to, const char *from) {
  const unsigned char *pos;

  /* Nothing special to do */
  if (share->fixed_length_table) {
    memcpy(to, from, table->s->reclength);
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

unsigned char *ha_blitz::get_pack_buffer(void) {
  uint32_t row_length = max_row_length();
  unsigned char *buf = pack_buffer;

  /* This is a shitty case where the row size is larger than 2KB. */
  if (row_length > BLITZ_MAX_ROW_STACK) {
    if (row_length > secondary_row_buffer_size) {
      void *new_ptr = realloc(secondary_row_buffer, row_length);

      if (new_ptr == NULL) {
        errno = HA_ERR_OUT_OF_MEM;
        return NULL;
      }

      secondary_row_buffer_size = row_length;
      secondary_row_buffer = (unsigned char *)new_ptr;
      buf = secondary_row_buffer;
    }
  }
  return buf;
}

THR_LOCK_DATA **ha_blitz::store_lock(Session *session,
                                     THR_LOCK_DATA **to,
                                     enum thr_lock_type lock_type) {

  sql_command_type = session_sql_command(session);

  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE)
        && !session_tablespace_op(session)) {
      lock_type = TL_WRITE_ALLOW_WRITE;
    } else if (lock_type == TL_READ_NO_INSERT) {
      lock_type = TL_READ_NO_INSERT;
    }

    lock.type = lock_type;
  }

  *to++ = &lock;
  return to;
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
  thr_lock_init(&share->lock);
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

__attribute__ ((visibility("default")))
DRIZZLE_PLUGIN(blitz_init, blitz_deinit, NULL, NULL);
