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

#include "config.h"
#include "ha_blitz.h"

using namespace std;
using namespace drizzled;
namespace po= boost::program_options;

static pthread_mutex_t blitz_utility_mutex;

static const char *ha_blitz_exts[] = {
  BLITZ_DATA_EXT,
  BLITZ_INDEX_EXT,
  BLITZ_SYSTEM_EXT,
  NULL
};

/* Global Variables for Startup Options */
uint64_t blitz_estimated_rows;

class BlitzEngine : public drizzled::plugin::StorageEngine {
private:
  TCMAP *blitz_table_cache;

public:
  BlitzEngine(const std::string &name_arg) :
    drizzled::plugin::StorageEngine(name_arg,
                                    drizzled::HTON_NULL_IN_KEY |
                                    drizzled::HTON_PRIMARY_KEY_IN_READ_INDEX |
                                    drizzled::HTON_STATS_RECORDS_IS_EXACT |
                                    drizzled::HTON_SKIP_STORE_LOCK) {
    table_definition_ext = BLITZ_SYSTEM_EXT;
  }

  virtual ~BlitzEngine() {
    pthread_mutex_destroy(&blitz_utility_mutex);
    tcmapdel(blitz_table_cache);
  }

  virtual drizzled::Cursor *create(drizzled::Table &table) {
    return new ha_blitz(*this, table);
  }

  const char **bas_ext() const {
    return ha_blitz_exts;
  }

  int doCreateTable(drizzled::Session &session,
                    drizzled::Table &table_arg,
                    const drizzled::identifier::Table &identifier,
                    drizzled::message::Table &table_proto);

  int doRenameTable(drizzled::Session &session,
                    const drizzled::identifier::Table &from_identifier,
                    const drizzled::identifier::Table &to_identifier);

  int doDropTable(drizzled::Session &session,
                  const drizzled::identifier::Table &identifier);

  int doGetTableDefinition(drizzled::Session &session,
                           const drizzled::identifier::Table &identifier,
                           drizzled::message::Table &table_proto);

  void doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                             const drizzled::identifier::Schema &schema_identifier,
                             drizzled::identifier::Table::vector &set_of_identifiers);

  bool doDoesTableExist(drizzled::Session &session,
                        const drizzled::identifier::Table &identifier);

  bool validateCreateTableOption(const std::string &key,
                                 const std::string &state);

  bool doCreateTableCache(void);

  BlitzShare *getTableShare(const std::string &name);
  void cacheTableShare(const std::string &name, BlitzShare *share);
  void deleteTableShare(const std::string &name);

  uint32_t max_supported_keys() const { return BLITZ_MAX_INDEX; }
  uint32_t max_supported_key_length() const { return BLITZ_MAX_KEY_LEN; }
  uint32_t max_supported_key_part_length() const { return BLITZ_MAX_KEY_LEN; }

  uint32_t index_flags(enum drizzled::ha_key_alg) const {
    return (HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER |
            HA_READ_RANGE | HA_ONLY_WHOLE_INDEX | HA_KEYREAD_ONLY);
  }
};

/* A key stored in BlitzDB's B+Tree is a byte array that also includes
   a key to that row in the data dictionary. Two keys are merged and
   stored as a key because we want to avoid reading the leaf node and
   thus save disk IO and some computation in the tree. Note that the
   comparison function of BlitzDB's btree only takes into accound the
   actual index key. See blitzcmp.cc for details.

   With the above in mind, this helper function returns a pointer to
   the dictionary key by calculating the offset. */
static char *skip_btree_key(const char *key, const size_t skip_len,
                            int *return_klen);

static bool str_is_numeric(const std::string &str);

int BlitzEngine::doCreateTable(drizzled::Session &,
                               drizzled::Table &table,
                               const drizzled::identifier::Table &identifier,
                               drizzled::message::Table &proto) {
  BlitzData dict;
  BlitzTree btree;
  int ecode;

  /* Temporary fix for blocking composite keys. We need to add this
     check because version 1 doesn't handle composite indexes. */
  for (uint32_t i = 0; i < table.getShare()->keys; i++) {
    if (table.key_info[i].key_parts > 1)
      return HA_ERR_UNSUPPORTED;
  }

  /* Create relevant files for a new table and close them immediately.
     All we want to do here is somewhat like UNIX touch(1). */
  if ((ecode = dict.create_data_table(proto, table, identifier)) != 0)
    return ecode;

  if ((ecode = dict.create_system_table(identifier.getPath())) != 0)
    return ecode;

  /* Create b+tree index(es) for this table. */
  for (uint32_t i = 0; i < table.getShare()->keys; i++) {
    if ((ecode = btree.create(identifier.getPath().c_str(), i)) != 0)
      return ecode;
  }

  /* Write the table definition to system table. */
  if ((ecode = dict.open_system_table(identifier.getPath(), HDBOWRITER)) != 0)
    return ecode;

  if (!dict.write_table_definition(proto)) {
    dict.close_system_table();
    return HA_ERR_CRASHED_ON_USAGE;
  }

  dict.close_system_table();
  return 0;
}

int BlitzEngine::doRenameTable(drizzled::Session &,
                               const drizzled::identifier::Table &from,
                               const drizzled::identifier::Table &to) {
  int rv = 0;

  BlitzData blitz_table;
  uint32_t nkeys;

  BlitzData dict;
  int ecode;
  /* Write the table definition to system table. */
  if ((ecode = dict.open_system_table(from.getPath(), HDBOWRITER)) != 0)
    return ecode;

  drizzled::message::Table proto;
  char *proto_string;
  int proto_string_len;

  proto_string = dict.get_system_entry(BLITZ_TABLE_PROTO_KEY.c_str(),
                                       BLITZ_TABLE_PROTO_KEY.length(),
                                       &proto_string_len);

  if (proto_string == NULL) {
    return ENOMEM;
  }

  if (!proto.ParseFromArray(proto_string, proto_string_len)) {
    free(proto_string);
    return HA_ERR_CRASHED_ON_USAGE;
  }

  free(proto_string);

  proto.set_name(to.getTableName());
  proto.set_schema(to.getSchemaName());
  proto.set_catalog(to.getCatalogName());

  if (!dict.write_table_definition(proto)) {
    dict.close_system_table();
    return HA_ERR_CRASHED_ON_USAGE;
  }

  dict.close_system_table();

  /* Find out the number of indexes in this table. This information
     is required because BlitzDB creates a file for each indexes.*/
  if (blitz_table.open_data_table(from.getPath().c_str(), HDBOREADER) != 0)
    return HA_ERR_CRASHED_ON_USAGE;

  nkeys = blitz_table.read_meta_keycount();

  if (blitz_table.close_data_table() != 0)
    return HA_ERR_CRASHED_ON_USAGE;

  /* We're now ready to rename the file(s) for this table. Start by
     attempting to rename the data and system files. */
  if (rename_file_ext(from.getPath().c_str(),
                      to.getPath().c_str(), BLITZ_DATA_EXT)) {
    if ((rv = errno) != ENOENT)
      return rv;
  }

  if (rename_file_ext(from.getPath().c_str(),
                      to.getPath().c_str(), BLITZ_SYSTEM_EXT)) {
    if ((rv = errno) != ENOENT)
      return rv;
  }

  /* So far so good. Rename the index file(s) and we're done. */
  BlitzTree btree;

  for (uint32_t i = 0; i < nkeys; i++) {
    if (btree.rename(from.getPath().c_str(), to.getPath().c_str(), i) != 0)
      return HA_ERR_CRASHED_ON_USAGE;
  }

  return rv;
}

int BlitzEngine::doDropTable(drizzled::Session &,
                             const drizzled::identifier::Table &identifier) {
  BlitzData dict;
  BlitzTree btree;
  char buf[FN_REFLEN];
  uint32_t nkeys;
  int err;

  /* We open the dictionary to extract meta data from it */
  if ((err = dict.open_data_table(identifier.getPath().c_str(),
                                  HDBOREADER)) != 0) {
    return err;
  }

  nkeys = dict.read_meta_keycount();

  /* We no longer need the dictionary to be open */
  dict.close_data_table();

  /* Drop the Data Dictionary */
  snprintf(buf, FN_REFLEN, "%s%s", identifier.getPath().c_str(), BLITZ_DATA_EXT);
  if ((err = unlink(buf)) == -1) {
    return err;
  }

  /* Drop the System Table */
  snprintf(buf, FN_REFLEN, "%s%s", identifier.getPath().c_str(), BLITZ_SYSTEM_EXT);
  if ((err = unlink(buf)) == -1) {
    return err;
  }

  /* Drop Index file(s) */
  for (uint32_t i = 0; i < nkeys; i++) {
    if ((err = btree.drop(identifier.getPath().c_str(), i)) != 0) {
      return err;
    }
  }

  return 0;
}

int BlitzEngine::doGetTableDefinition(drizzled::Session &,
                                      const drizzled::identifier::Table &identifier,
                                      drizzled::message::Table &proto) {
  struct stat stat_info;
  std::string path(identifier.getPath());

  path.append(BLITZ_SYSTEM_EXT);

  if (stat(path.c_str(), &stat_info)) {
    return errno;
  }

  BlitzData db;
  char *proto_string;
  int proto_string_len;

  if (db.open_system_table(identifier.getPath(), HDBOREADER) != 0) {
    return HA_ERR_CRASHED_ON_USAGE;
  }

  proto_string = db.get_system_entry(BLITZ_TABLE_PROTO_KEY.c_str(),
                                     BLITZ_TABLE_PROTO_KEY.length(),
                                     &proto_string_len);

  if (db.close_system_table() != 0) {
    return HA_ERR_CRASHED_ON_USAGE;
  }

  if (proto_string == NULL) {
    return ENOMEM;
  }

  if (!proto.ParseFromArray(proto_string, proto_string_len)) {
    free(proto_string);
    return HA_ERR_CRASHED_ON_USAGE;
  }

  free(proto_string);

  return EEXIST;
}

void BlitzEngine::doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                                        const drizzled::identifier::Schema &schema_id,
                                        drizzled::identifier::Table::vector &ids) {
  drizzled::CachedDirectory::Entries entries = directory.getEntries();

  for (drizzled::CachedDirectory::Entries::iterator entry_iter = entries.begin();
       entry_iter != entries.end(); ++entry_iter) {

    drizzled::CachedDirectory::Entry *entry = *entry_iter;
    const std::string *filename = &entry->filename;

    assert(filename->size());

    const char *ext = strchr(filename->c_str(), '.');

    if (ext == NULL || my_strcasecmp(system_charset_info, ext, BLITZ_SYSTEM_EXT) ||
        (filename->compare(0, strlen(TMP_FILE_PREFIX), TMP_FILE_PREFIX) == 0)) {
    } else {
      char uname[NAME_LEN + 1];
      uint32_t file_name_len;

      file_name_len = identifier::Table::filename_to_tablename(filename->c_str(),
                                                             uname,
                                                             sizeof(uname));

      uname[file_name_len - sizeof(BLITZ_DATA_EXT) + 1]= '\0';
      ids.push_back(identifier::Table(schema_id, uname));
    }
  }
}

bool BlitzEngine::doDoesTableExist(drizzled::Session &,
                                   const drizzled::identifier::Table &identifier) {
  std::string proto_path(identifier.getPath());
  proto_path.append(BLITZ_DATA_EXT);

  return (access(proto_path.c_str(), F_OK)) ? false : true;
}

bool BlitzEngine::validateCreateTableOption(const std::string &key,
                                            const std::string &state) {
  if (key == "ESTIMATED_ROWS" || key == "estimated_rows") {
    if (str_is_numeric(state))
      return true;
  }
  return false;
}

bool BlitzEngine::doCreateTableCache(void) {
  return ((blitz_table_cache = tcmapnew()) == NULL) ? false : true;
}

BlitzShare *BlitzEngine::getTableShare(const std::string &table_name) {
  int vlen;
  const void *fetched;
  BlitzShare *rv = NULL;

  fetched = tcmapget(blitz_table_cache, table_name.c_str(),
                     table_name.length(), &vlen);

  /* dereference the object */
  if (fetched)
    rv = *(BlitzShare **)fetched;

  return rv;
}

void BlitzEngine::cacheTableShare(const std::string &table_name,
                                  BlitzShare *share) {
  /* Cache the memory address of the share object */
  tcmapput(blitz_table_cache, table_name.c_str(), table_name.length(),
           &share, sizeof(share));
}

void BlitzEngine::deleteTableShare(const std::string &table_name) {
  tcmapout2(blitz_table_cache, table_name.c_str());
}

ha_blitz::ha_blitz(drizzled::plugin::StorageEngine &engine_arg,
                   Table &table_arg) : Cursor(engine_arg, table_arg),
                                            btree_cursor(NULL),
                                            table_scan(false),
                                            table_based(false),
                                            thread_locked(false),
                                            held_key(NULL),
                                            held_key_len(0),
                                            current_key(NULL),
                                            current_key_len(0),
                                            key_buffer(NULL),
                                            errkey_id(0) {}

int ha_blitz::open(const char *table_name, int, uint32_t) {
  if ((share = get_share(table_name)) == NULL)
    return HA_ERR_CRASHED_ON_USAGE;

  pthread_mutex_lock(&blitz_utility_mutex);

  btree_cursor = new BlitzCursor[share->nkeys];

  for (uint32_t i = 0; i < share->nkeys; i++) {
    if (!share->btrees[i].create_cursor(&btree_cursor[i])) {
      free_share();
      pthread_mutex_unlock(&blitz_utility_mutex);
      return HA_ERR_OUT_OF_MEM;
    }
  }

  if ((key_buffer = (char *)malloc(BLITZ_MAX_KEY_LEN)) == NULL) {
    free_share();
    pthread_mutex_unlock(&blitz_utility_mutex);
    return HA_ERR_OUT_OF_MEM;
  }

  if ((key_merge_buffer = (char *)malloc(BLITZ_MAX_KEY_LEN)) == NULL) {
    free_share();
    pthread_mutex_unlock(&blitz_utility_mutex);
    return HA_ERR_OUT_OF_MEM;
  }

  if ((held_key_buf = (char *)malloc(BLITZ_MAX_KEY_LEN)) == NULL) {
    free_share();
    free(key_buffer);
    free(key_merge_buffer);
    pthread_mutex_unlock(&blitz_utility_mutex);
    return HA_ERR_OUT_OF_MEM;
  }

  secondary_row_buffer = NULL;
  secondary_row_buffer_size = 0;
  key_merge_buffer_len = BLITZ_MAX_KEY_LEN;

  /* 'ref_length' determines the size of the buffer that the kernel
     will use to uniquely identify a row. The actual allocation is
     done by the kernel so all we do here is specify the size of it.*/
  if (share->primary_key_exists) {
    ref_length = getTable()->key_info[getTable()->getShare()->getPrimaryKey()].key_length;
  } else {
    ref_length = sizeof(held_key_len) + sizeof(uint64_t);
  }

  pthread_mutex_unlock(&blitz_utility_mutex);
  return 0;
}

int ha_blitz::close(void) {
  for (uint32_t i = 0; i < share->nkeys; i++) {
    share->btrees[i].destroy_cursor(&btree_cursor[i]);
  }
  delete [] btree_cursor;

  free(key_buffer);
  free(key_merge_buffer);
  free(held_key_buf);
  free(secondary_row_buffer);
  return free_share();
}

int ha_blitz::info(uint32_t flag) {
  if (flag & HA_STATUS_VARIABLE) {
    stats.records = share->dict.nrecords();
    stats.data_file_length = share->dict.table_size();
  }

  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value = share->auto_increment_value + 1;

  if (flag & HA_STATUS_ERRKEY)
    errkey = errkey_id;

  return 0;
}

int ha_blitz::doStartTableScan(bool scan) {
  /* Obtain the query type for this scan */
  sql_command_type = getTable()->getSession()->get_sql_command();
  table_scan = scan;
  table_based = true;

  /* Obtain the most suitable lock for the given statement type. */
  blitz_optimal_lock();

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

  free(held_key);
  held_key = NULL;

  if (current_key == NULL) {
    getTable()->status = STATUS_NOT_FOUND;
    return HA_ERR_END_OF_FILE;
  }

  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);

  /* Unpack and copy the current row to Drizzle's result buffer. */
  unpack_row(drizzle_buf, current_row, current_row_len);

  /* Retrieve both key and row of the next record with one allocation. */
  next_key = share->dict.next_key_and_row(current_key, current_key_len,
                                          &next_key_len, &next_row,
                                          &next_row_len);

  /* Memory region for "current_row" will be freed as "held key" on
     the next iteration. This is because "current_key" points to the
     region of memory that contains "current_row" and "held_key" points
     to it. If there isn't another iteration then it is freed in doEndTableScan(). */
  current_row = next_row;
  current_row_len = next_row_len;

  /* Remember the current row because delete, update or replace
     function could be called after this function. This pointer is
     also used to free the previous key and row, which resides on
     the same buffer. */
  held_key = current_key;
  held_key_len = current_key_len;

  /* It is now memory-leak-safe to point current_key to next_key. */
  current_key = next_key;
  current_key_len = next_key_len;
  getTable()->status = 0;
  return 0;
}

int ha_blitz::doEndTableScan() {
  if (table_scan && current_key)
    free(current_key);
  if (table_scan && held_key)
    free(held_key);

  current_key = NULL;
  held_key = NULL;
  current_key_len = 0;
  held_key_len = 0;
  table_scan = false;
  table_based = false;

  if (thread_locked)
    blitz_optimal_unlock();

  return 0;
}

int ha_blitz::rnd_pos(unsigned char *copy_to, unsigned char *pos) {
  char *row;
  char *key = NULL;
  int key_len, row_len;

  memcpy(&key_len, pos, sizeof(key_len));
  key = (char *)(pos + sizeof(key_len));

  /* TODO: Find a better error type. */
  if (key == NULL)
    return HA_ERR_KEY_NOT_FOUND;

  row = share->dict.get_row(key, key_len, &row_len);

  if (row == NULL)
    return HA_ERR_KEY_NOT_FOUND;

  unpack_row(copy_to, row, row_len);

  /* Remember the key location on memory if the thread is not doing
     a table scan. This is because either update_row() or delete_row()
     might be called after this function. */
  if (!table_scan) {
    held_key = key;
    held_key_len = key_len;
  }

  free(row);
  return 0;
}

void ha_blitz::position(const unsigned char *) {
  int length = sizeof(held_key_len);
  memcpy(ref, &held_key_len, length);
  memcpy(ref + length, (unsigned char *)held_key, held_key_len);
}

const char *ha_blitz::index_type(uint32_t /*key_num*/) {
  return "BTREE";
}

int ha_blitz::doStartIndexScan(uint32_t key_num, bool) {
  active_index = key_num;
  sql_command_type = getTable()->getSession()->get_sql_command();

  /* This is unlikely to happen but just for assurance, re-obtain
     the lock if this thread already has a certain lock. This makes
     sure that this thread will get the most appropriate lock for
     the current statement. */
  if (thread_locked)
    blitz_optimal_unlock();

  blitz_optimal_lock();
  return 0;
}

int ha_blitz::index_first(unsigned char *buf) {
  char *dict_key, *bt_key, *row;
  int dict_klen, bt_klen, prefix_len, rlen;

  bt_key = btree_cursor[active_index].first_key(&bt_klen);

  if (bt_key == NULL)
    return HA_ERR_END_OF_FILE;

  prefix_len = btree_key_length(bt_key, active_index);
  dict_key = skip_btree_key(bt_key, prefix_len, &dict_klen);

  if ((row = share->dict.get_row(dict_key, dict_klen, &rlen)) == NULL) {
    free(bt_key);
    return HA_ERR_KEY_NOT_FOUND;
  }

  unpack_row(buf, row, rlen);
  keep_track_of_key(bt_key, bt_klen);

  free(bt_key);
  free(row);
  return 0;
}

int ha_blitz::index_next(unsigned char *buf) {
  char *dict_key, *bt_key, *row;
  int dict_klen, bt_klen, prefix_len, rlen;

  bt_key = btree_cursor[active_index].next_key(&bt_klen);

  if (bt_key == NULL) {
    getTable()->status = STATUS_NOT_FOUND;
    return HA_ERR_END_OF_FILE;
  }

  prefix_len = btree_key_length(bt_key, active_index);
  dict_key = skip_btree_key(bt_key, prefix_len, &dict_klen);

  if ((row = share->dict.get_row(dict_key, dict_klen, &rlen)) == NULL) {
    free(bt_key);
    getTable()->status = STATUS_NOT_FOUND;
    return HA_ERR_KEY_NOT_FOUND;
  }

  unpack_row(buf, row, rlen);
  keep_track_of_key(bt_key, bt_klen);

  free(bt_key);
  free(row);
  return 0;
}

int ha_blitz::index_prev(unsigned char *buf) {
  char *dict_key, *bt_key, *row;
  int dict_klen, bt_klen, prefix_len, rlen;

  bt_key = btree_cursor[active_index].prev_key(&bt_klen);

  if (bt_key == NULL)
    return HA_ERR_END_OF_FILE;

  prefix_len = btree_key_length(bt_key, active_index);
  dict_key = skip_btree_key(bt_key, prefix_len, &dict_klen);

  if ((row = share->dict.get_row(dict_key, dict_klen, &rlen)) == NULL) {
    free(bt_key);
    return HA_ERR_KEY_NOT_FOUND;
  }

  unpack_row(buf, row, rlen);
  keep_track_of_key(bt_key, bt_klen);

  free(bt_key);
  free(row);
  return 0;
}

int ha_blitz::index_last(unsigned char *buf) {
  char *dict_key, *bt_key, *row;
  int dict_klen, bt_klen, prefix_len, rlen;

  bt_key = btree_cursor[active_index].final_key(&bt_klen);

  if (bt_key == NULL)
    return HA_ERR_KEY_NOT_FOUND;

  prefix_len = btree_key_length(bt_key, active_index);
  dict_key = skip_btree_key(bt_key, prefix_len, &dict_klen);

  if ((row = share->dict.get_row(dict_key, dict_klen, &rlen)) == NULL) {
    free(bt_key);
    errkey_id = active_index;
    return HA_ERR_KEY_NOT_FOUND;
  }

  unpack_row(buf, row, rlen);
  keep_track_of_key(bt_key, bt_klen);

  free(bt_key);
  free(row);
  return 0;
}

int ha_blitz::index_read(unsigned char *buf, const unsigned char *key,
                         uint32_t key_len, enum ha_rkey_function find_flag) {
  return index_read_idx(buf, active_index, key, key_len, find_flag);
}

/* This is where the read related index logic lives. It is used by both
   BlitzDB and the Database Kernel (specifically, by the optimizer). */
int ha_blitz::index_read_idx(unsigned char *buf, uint32_t key_num,
                             const unsigned char *key, uint32_t,
                             enum ha_rkey_function search_mode) {

  /* If the provided key is NULL, we are required to return the first
     row in the active_index. */
  if (key == NULL)
    return this->index_first(buf);

  /* Otherwise we search for it. Prepare the key to look up the tree. */
  int packed_klen;
  char *packed_key = native_to_blitz_key(key, key_num, &packed_klen);

  /* Lookup the tree and get the master key. */
  int unique_klen;
  char *unique_key;

  unique_key = btree_cursor[key_num].find_key(search_mode, packed_key,
                                              packed_klen, &unique_klen);

  if (unique_key == NULL) {
    errkey_id = key_num;
    return HA_ERR_KEY_NOT_FOUND;
  }

  /* Got the master key. Prepare it to lookup the data dictionary. */
  int dict_klen;
  int skip_len = btree_key_length(unique_key, key_num);
  char *dict_key = skip_btree_key(unique_key, skip_len, &dict_klen);

  /* Fetch the packed row from the data dictionary. */
  int row_len;
  char *fetched_row = share->dict.get_row(dict_key, dict_klen, &row_len);

  if (fetched_row == NULL) {
    errkey_id = key_num;
    free(unique_key);
    return HA_ERR_KEY_NOT_FOUND;
  }

  /* Unpack it into Drizzle's return buffer and keep track of the
     master key for future use (before index_end() is called). */
  unpack_row(buf, fetched_row, row_len);
  keep_track_of_key(unique_key, unique_klen);

  free(unique_key);
  free(fetched_row);
  return 0;
}

int ha_blitz::doEndIndexScan(void) {
  held_key = NULL;
  held_key_len = 0;

  btree_cursor[active_index].moved = false;

  if (thread_locked)
    blitz_optimal_unlock();

  return 0;
}

int ha_blitz::enable_indexes(uint32_t) {
  return HA_ERR_UNSUPPORTED;
}

int ha_blitz::disable_indexes(uint32_t) {
  return HA_ERR_UNSUPPORTED;
}

/* Find the estimated number of rows between min_key and max_key.
   Leave the proper implementation of this for now since there are
   too many exceptions to cover. */
ha_rows ha_blitz::records_in_range(uint32_t /*key_num*/,
                                   drizzled::key_range * /*min_key*/,
                                   drizzled::key_range * /*max_key*/) {
  return BLITZ_WORST_CASE_RANGE;
}

int ha_blitz::doInsertRecord(unsigned char *drizzle_row) {
  int rv;

  ha_statistic_increment(&system_status_var::ha_write_count);

  /* Prepare Auto Increment field if one exists. */
  if (getTable()->next_number_field && drizzle_row == getTable()->getInsertRecord()) {
    pthread_mutex_lock(&blitz_utility_mutex);
    if ((rv = update_auto_increment()) != 0) {
      pthread_mutex_unlock(&blitz_utility_mutex);
      return rv;
    }

    uint64_t next_val = getTable()->next_number_field->val_int();

    if (next_val > share->auto_increment_value) {
      share->auto_increment_value = next_val;
      stats.auto_increment_value = share->auto_increment_value + 1;
    }
    pthread_mutex_unlock(&blitz_utility_mutex);
  }

  /* Serialize a primary key for this row. If a PK doesn't exist,
     an internal hidden ID will be generated. We obtain the PK here
     and pack it to this function's local buffer instead of the
     thread's own 'key_buffer' because the PK value needs to be
     remembered when writing non-PK keys AND because the 'key_buffer'
     will be used to generate these non-PK keys. */
  char temp_pkbuf[BLITZ_MAX_KEY_LEN];
  size_t pk_len = make_primary_key(temp_pkbuf, drizzle_row);

  /* Obtain a buffer that can accommodate this row. We then pack
     the provided row into it. Note that this code works most
     efficiently for rows smaller than BLITZ_MAX_ROW_STACK */
  unsigned char *row_buf = get_pack_buffer(max_row_length());
  size_t row_len = pack_row(row_buf, drizzle_row);

  uint32_t curr_key = 0;
  uint32_t lock_id = 0;

  if (share->nkeys > 0) {
    lock_id = share->blitz_lock.slot_id(temp_pkbuf, pk_len);
    share->blitz_lock.slotted_lock(lock_id);
  }

  /* We isolate this condition outside the key loop to avoid the CPU
     from going through unnecessary conditional branching on heavy
     insertion load. TODO: Optimize this block. PK should not need
     to go through merge_key() since this information is redundant. */
  if (share->primary_key_exists) {
    char *key = NULL;
    size_t klen = 0;

    key = merge_key(temp_pkbuf, pk_len, temp_pkbuf, pk_len, &klen);

    rv = share->btrees[curr_key].write_unique(key, klen);

    if (rv == HA_ERR_FOUND_DUPP_KEY) {
      errkey_id = curr_key;
      share->blitz_lock.slotted_unlock(lock_id);
      return rv;
    }
    curr_key = 1;
  }

  /* Loop over the keys and write them to it's exclusive tree. */
  while (curr_key < share->nkeys) {
    char *key = NULL;
    size_t prefix_len = 0;
    size_t klen = 0;

    prefix_len = make_index_key(key_buffer, curr_key, drizzle_row);
    key = merge_key(key_buffer, prefix_len, temp_pkbuf, pk_len, &klen);

    if (share->btrees[curr_key].unique) {
      rv = share->btrees[curr_key].write_unique(key, klen);
    } else {
      rv = share->btrees[curr_key].write(key, klen);
    }

    if (rv != 0) {
      errkey_id = curr_key;
      share->blitz_lock.slotted_unlock(lock_id);
      return rv;
    }

    curr_key++;
  }

  /* Write the row to the Data Dictionary. */
  rv = share->dict.write_row(temp_pkbuf, pk_len, row_buf, row_len);

  if (share->nkeys > 0)
    share->blitz_lock.slotted_unlock(lock_id);

  return rv;
}

int ha_blitz::doUpdateRecord(const unsigned char *old_row,
                             unsigned char *new_row) {
  int rv;
  uint32_t lock_id = 0;

  ha_statistic_increment(&system_status_var::ha_update_count);

  /* Handle Indexes */
  if (share->nkeys > 0) {
    /* BlitzDB cannot update an indexed row on table scan. */
    if (table_scan)
      return HA_ERR_UNSUPPORTED;

    if ((rv = compare_rows_for_unique_violation(old_row, new_row)) != 0)
      return rv;

    lock_id = share->blitz_lock.slot_id(held_key, held_key_len);
    share->blitz_lock.slotted_lock(lock_id);

    /* Update all relevant index entries. Start by deleting the
       the existing key then write the new key. Something we should
       consider in the future is to take a diff of the keys and only
       update changed keys. */
    int skip = btree_key_length(held_key, active_index);
    char *suffix = held_key + skip;
    uint16_t suffix_len = uint2korr(suffix);

    suffix += sizeof(suffix_len);

    for (uint32_t i = 0; i < share->nkeys; i++) {
      char *key;
      size_t prefix_len, klen;

      klen = 0;
      prefix_len = make_index_key(key_buffer, i, old_row);
      key = merge_key(key_buffer, prefix_len, suffix, suffix_len, &klen);

      if (share->btrees[i].delete_key(key, klen) != 0) {
        errkey_id = i;
        share->blitz_lock.slotted_unlock(lock_id);
        return HA_ERR_KEY_NOT_FOUND;
      }

      /* Now write the new key. */
      prefix_len = make_index_key(key_buffer, i, new_row);

      if (i == getTable()->getShare()->getPrimaryKey()) {
        key = merge_key(key_buffer, prefix_len, key_buffer, prefix_len, &klen);
        rv = share->btrees[i].write(key, klen);
      } else {
        key = merge_key(key_buffer, prefix_len, suffix, suffix_len, &klen);
        rv = share->btrees[i].write(key, klen);
      }

      if (rv != 0) {
        errkey_id = i;
        share->blitz_lock.slotted_unlock(lock_id);
        return rv;
      }
    }
  }

  /* Getting this far means that the index has been successfully
     updated. We now update the Data Dictionary. This implementation
     is admittedly far from optimial and will be revisited. */
  size_t row_len = max_row_length();
  unsigned char *row_buf = get_pack_buffer(row_len);
  row_len = pack_row(row_buf, new_row);

  /* This is a basic case where we can simply overwrite the key. */
  if (table_based) {
    rv = share->dict.write_row(held_key, held_key_len, row_buf, row_len);
  } else {
    int klen = make_index_key(key_buffer, getTable()->getShare()->getPrimaryKey(), old_row);

    /* Delete with the old key. */
    share->dict.delete_row(key_buffer, klen);

    /* Write with the new key. */
    klen = make_index_key(key_buffer, getTable()->getShare()->getPrimaryKey(), new_row);
    rv = share->dict.write_row(key_buffer, klen, row_buf, row_len);
  }

  if (share->nkeys > 0)
    share->blitz_lock.slotted_unlock(lock_id);

  return rv;
}

int ha_blitz::doDeleteRecord(const unsigned char *row_to_delete) {
  int rv;

  ha_statistic_increment(&system_status_var::ha_delete_count);

  char *dict_key = held_key;
  int dict_klen = held_key_len;
  uint32_t lock_id = 0;

  if (share->nkeys > 0) {
    lock_id = share->blitz_lock.slot_id(held_key, held_key_len);
    share->blitz_lock.slotted_lock(lock_id);

    /* Loop over the indexes and delete all relevant entries for
       this row. We do this by reproducing the key in BlitzDB's
       unique key format. The procedure is simple.

       (1): Compute the key value for this index from the row then
            pack it into key_buffer (not unique at this point).

       (2): Append the suffix of the held_key to the key generated
            in step 1. The key is then guaranteed to be unique. */
    for (uint32_t i = 0; i < share->nkeys; i++) {
      /* In this case, we don't need to search for the key because
         TC's cursor is already pointing at the key that we want
         to delete. We wouldn't be here otherwise. */
      if (i == active_index) {
        btree_cursor[active_index].delete_position();
        continue;
      }

      int klen = make_index_key(key_buffer, i, row_to_delete);
      int skip_len = btree_key_length(held_key, active_index);
      uint16_t suffix_len = uint2korr(held_key + skip_len);

      /* Append the suffix to the key */
      memcpy(key_buffer + klen, held_key + skip_len,
             sizeof(suffix_len) + suffix_len);

      /* Update the key length to cover the generated key. */
      klen = klen + sizeof(suffix_len) + suffix_len;

      if (share->btrees[i].delete_key(key_buffer, klen) != 0)
        return HA_ERR_KEY_NOT_FOUND;
    }

    /* Skip to the data dictionary key. */
    int dict_key_offset = btree_key_length(dict_key, active_index);
    dict_key = skip_btree_key(dict_key, dict_key_offset, &dict_klen);
  }

  rv = share->dict.delete_row(dict_key, dict_klen);

  if (share->nkeys > 0)
    share->blitz_lock.slotted_unlock(lock_id);

  return rv;
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
  uint32_t length = (getTable()->getRecordLength() + getTable()->sizeFields() * 2);
  uint32_t *pos = getTable()->getBlobField();
  uint32_t *end = pos + getTable()->sizeBlobFields();

  while (pos != end) {
    length += 2 + ((Field_blob *)getTable()->getField(*pos))->get_length();
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
  return make_index_key(pack_to, getTable()->getShare()->getPrimaryKey(), row);
}

size_t ha_blitz::make_index_key(char *pack_to, int key_num,
                                const unsigned char *row) {
  KeyInfo *key = &getTable()->key_info[key_num];
  KeyPartInfo *key_part = key->key_part;
  KeyPartInfo *key_part_end = key_part + key->key_parts;

  unsigned char *pos = (unsigned char *)pack_to;
  unsigned char *end;
  int offset = 0;

  memset(pack_to, 0, BLITZ_MAX_KEY_LEN);

  /* Loop through key part(s) and pack them as we go. */
  for (; key_part != key_part_end; key_part++) {
    if (key_part->null_bit) {
      if (row[key_part->null_offset] & key_part->null_bit) {
        *pos++ = 0;
        continue;
      }
      *pos++ = 1;
    }

    /* Here we normalize VARTEXT1 to VARTEXT2 for simplicity. */
    if (key_part->type == HA_KEYTYPE_VARTEXT1) {
      /* Extract the length of the string from the row. */
      uint16_t data_len = *(uint8_t *)(row + key_part->offset);

      /* Copy the length of the string. Use 2 bytes. */
      int2store(pos, data_len);
      pos += sizeof(data_len);

      /* Copy the string data */
      memcpy(pos, row + key_part->offset + sizeof(uint8_t), data_len);
      pos += data_len;
    } else {
      end = key_part->field->pack(pos, row + key_part->offset);
      offset = end - pos;
      pos += offset;
    }
  }

  return ((char *)pos - pack_to);
}

char *ha_blitz::merge_key(const char *a, const size_t a_len, const char *b,
                          const size_t b_len, size_t *merged_len) {

  size_t total = a_len + sizeof(uint16_t) + b_len;

  if (total > key_merge_buffer_len) {
    key_merge_buffer = (char *)realloc(key_merge_buffer, total);

    if (key_merge_buffer == NULL) {
      errno = HA_ERR_OUT_OF_MEM;
      return NULL;
    }
    key_merge_buffer_len = total;
  }

  char *pos = key_merge_buffer;

  /* Copy the prefix. */
  memcpy(pos, a, a_len);
  pos += a_len;

  /* Copy the length of b. */
  int2store(pos, (uint16_t)b_len);
  pos += sizeof(uint16_t);

  /* Copy the suffix and we're done. */
  memcpy(pos, b, b_len);

  *merged_len = total;
  return key_merge_buffer;
}

size_t ha_blitz::btree_key_length(const char *key, const int key_num) {
  KeyInfo *key_info = &getTable()->key_info[key_num];
  KeyPartInfo *key_part = key_info->key_part;
  KeyPartInfo *key_part_end = key_part + key_info->key_parts;
  char *pos = (char *)key;
  uint64_t len = 0;
  size_t rv = 0;

  for (; key_part != key_part_end; key_part++) {
    if (key_part->null_bit) {
      pos++;
      rv++;
      if (*key == 0)
        continue;
    }

    if (key_part->type == HA_KEYTYPE_VARTEXT1 ||
        key_part->type == HA_KEYTYPE_VARTEXT2) {
      len = uint2korr(pos);
      rv += len + sizeof(uint16_t);
    } else {
      len = key_part->field->key_length();
      rv += len;
    }
    pos += len;
    len = 0;
  }

  return rv;
}

void ha_blitz::keep_track_of_key(const char *key, const int klen) {
  memcpy(held_key_buf, key, klen);
  held_key = held_key_buf;
  held_key_len = klen;
}

/* Converts a native Drizzle index key to BlitzDB's format. */
char *ha_blitz::native_to_blitz_key(const unsigned char *native_key,
                                    const int key_num, int *return_key_len) {
  KeyInfo *key = &getTable()->key_info[key_num];
  KeyPartInfo *key_part = key->key_part;
  KeyPartInfo *key_part_end = key_part + key->key_parts;

  unsigned char *key_pos = (unsigned char *)native_key;
  unsigned char *keybuf_pos = (unsigned char *)key_buffer;
  unsigned char *end;
  int key_size = 0;
  int offset = 0;

  memset(key_buffer, 0, BLITZ_MAX_KEY_LEN);

  for (; key_part != key_part_end; key_part++) {
    if (key_part->null_bit) {
      key_size++;

      /* This key is NULL */
      if (!(*keybuf_pos++ = (*key_pos++ == 0)))
        continue;
    }

    /* Normalize a VARTEXT1 key to VARTEXT2. */
    if (key_part->type == HA_KEYTYPE_VARTEXT1) {
      uint16_t str_len = *(uint16_t *)key_pos;

      /* Copy the length of the string over to key buffer. */
      int2store(keybuf_pos, str_len);
      keybuf_pos += sizeof(str_len);

      /* Copy the actual value over to the key buffer. */
      memcpy(keybuf_pos, key_pos + sizeof(str_len), str_len);
      keybuf_pos += str_len;

      /* NULL byte + Length of str (2 byte) + Actual String. */
      offset = 1 + sizeof(str_len) + str_len;
    } else {
      end = key_part->field->pack(keybuf_pos, key_pos);
      offset = end - keybuf_pos;
      keybuf_pos += offset;
    }

    key_size += offset;
    key_pos += key_part->field->key_length();
  }

  *return_key_len = key_size;
  return key_buffer;
}

size_t ha_blitz::pack_row(unsigned char *row_buffer,
                          unsigned char *row_to_pack) {
  unsigned char *pos;

  /* Nothing special to do if the table is fixed length */
  if (share->fixed_length_table) {
    memcpy(row_buffer, row_to_pack, getTable()->getShare()->getRecordLength());
    return (size_t)getTable()->getShare()->getRecordLength();
  }

  /* Copy NULL bits */
  memcpy(row_buffer, row_to_pack, getTable()->getShare()->null_bytes);
  pos = row_buffer + getTable()->getShare()->null_bytes;

  /* Pack each field into the buffer */
  for (Field **field = getTable()->getFields(); *field; field++) {
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
  memcpy(to, pos, getTable()->getShare()->null_bytes);
  pos += getTable()->getShare()->null_bytes;

  /* Unpack all fields in the provided row. */
  for (Field **field = getTable()->getFields(); *field; field++) {
    if (!((*field)->is_null())) {
      pos = (*field)->unpack(to + (*field)->offset(getTable()->getInsertRecord()), pos);
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
    }
    buf = secondary_row_buffer;
  }
  return buf;
}

static BlitzEngine *blitz_engine = NULL;

BlitzShare *ha_blitz::get_share(const char *name) {
  BlitzShare *share_ptr;
  BlitzEngine *bz_engine = (BlitzEngine *)getEngine();
  std::string table_path(name);

  pthread_mutex_lock(&blitz_utility_mutex);

  /* Look up the table cache to see if the table resource is available */
  share_ptr = bz_engine->getTableShare(table_path);

  if (share_ptr) {
    share_ptr->use_count++;
    pthread_mutex_unlock(&blitz_utility_mutex);
    return share_ptr;
  }

  /* Table wasn't cached so create a new table handler */
  share_ptr = new BlitzShare();

  /* Prepare the Data Dictionary */
  if (share_ptr->dict.startup(table_path.c_str()) != 0) {
    delete share_ptr;
    pthread_mutex_unlock(&blitz_utility_mutex);
    return NULL;
  }

  /* Prepare Index Structure(s) */
  KeyInfo *curr = &getTable()->getMutableShare()->getKeyInfo(0);
  share_ptr->btrees = new BlitzTree[getTable()->getShare()->keys];

  for (uint32_t i = 0; i < getTable()->getShare()->keys; i++, curr++) {
    share_ptr->btrees[i].open(table_path.c_str(), i, BDBOWRITER);
    share_ptr->btrees[i].parts = new BlitzKeyPart[curr->key_parts];

    if (getTable()->key_info[i].flags & HA_NOSAME)
      share_ptr->btrees[i].unique = true;

    share_ptr->btrees[i].length = curr->key_length;
    share_ptr->btrees[i].nparts = curr->key_parts;

    /* Record Meta Data of the Key Segments */
    for (uint32_t j = 0; j < curr->key_parts; j++) {
      Field *f = curr->key_part[j].field;

      if (f->null_ptr) {
        share_ptr->btrees[i].parts[j].null_bitmask = f->null_bit;
        share_ptr->btrees[i].parts[j].null_pos
          = (uint32_t)(f->null_ptr - (unsigned char *)getTable()->getInsertRecord());
      }

      share_ptr->btrees[i].parts[j].flag = curr->key_part[j].key_part_flag;

      if (f->type() == DRIZZLE_TYPE_BLOB) {
        share_ptr->btrees[i].parts[j].flag |= HA_BLOB_PART;
      }

      share_ptr->btrees[i].parts[j].type = curr->key_part[j].type;
      share_ptr->btrees[i].parts[j].offset = curr->key_part[j].offset;
      share_ptr->btrees[i].parts[j].length = curr->key_part[j].length;
    }
  }

  /* Set Meta Data */
  share_ptr->auto_increment_value = share_ptr->dict.read_meta_autoinc();
  share_ptr->table_name = table_path;
  share_ptr->nkeys = getTable()->getShare()->keys;
  share_ptr->use_count = 1;

  share_ptr->fixed_length_table = !(getTable()->getShare()->db_create_options
                                    & HA_OPTION_PACK_RECORD);

  if (getTable()->getShare()->getPrimaryKey() >= MAX_KEY)
    share_ptr->primary_key_exists = false;
  else
    share_ptr->primary_key_exists = true;

  /* Done creating the share object. Cache it for later
     use by another cursor object.*/
  bz_engine->cacheTableShare(table_path, share_ptr);

  pthread_mutex_unlock(&blitz_utility_mutex);
  return share_ptr;
}

int ha_blitz::free_share(void) {
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

    BlitzEngine *bz_engine = (BlitzEngine *)getEngine();
    bz_engine->deleteTableShare(share->table_name);

    delete[] share->btrees;
    delete share;
  }

  pthread_mutex_unlock(&blitz_utility_mutex);
  return 0;
}

static int blitz_init(drizzled::module::Context &context) {
  blitz_engine = new BlitzEngine("BLITZDB");

  if (!blitz_engine->doCreateTableCache()) {
    delete blitz_engine;
    return HA_ERR_OUT_OF_MEM;
  }

  pthread_mutex_init(&blitz_utility_mutex, NULL);
  context.add(blitz_engine);
  context.registerVariable(new sys_var_uint64_t_ptr("estimated-rows",
                                                    &blitz_estimated_rows));
  return 0;
}

/* Read the prototype of this function for details. */
static char *skip_btree_key(const char *key, const size_t skip_len,
                            int *return_klen) {
  char *pos = (char *)key;
  *return_klen = uint2korr(pos + skip_len);
  return pos + skip_len + sizeof(uint16_t);
}

static bool str_is_numeric(const std::string &str) {
  for (uint32_t i = 0; i < str.length(); i++) {
    if (!std::isdigit(str[i]))
      return false;
  }
  return true;
}

static void blitz_init_options(drizzled::module::option_context &context)
{
  context("estimated-rows",
          po::value<uint64_t>(&blitz_estimated_rows)->default_value(0),
          N_("Estimated number of rows that a BlitzDB table will store."));
}

DRIZZLE_PLUGIN(blitz_init, NULL, blitz_init_options);
