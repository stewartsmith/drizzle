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

#ifndef STORAGE_BLITZ_HA_BLITZ_H
#define STORAGE_BLITZ_HA_BLITZ_H

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/cursor.h>
#include <drizzled/table.h>
#include <drizzled/field.h>
#include <drizzled/field/blob.h>
#include <drizzled/atomics.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <mysys/thr_lock.h>
#include <tchdb.h>

#include <string>

#define BLITZ_DATAFILE_EXT    ".bzd"
#define BLITZ_INDEX_FILE_EXT  ".bzx"
#define BLITZ_SYSTEM_EXT      ".bzs"
#define BLITZ_MAX_ROW_STACK   2048
#define BLITZ_MAX_KEY_LENGTH  128

using namespace std;

const string BLITZ_TABLE_PROTO_KEY = "table_definition";
const string BLITZ_TABLE_PROTO_COMMENT_KEY = "table_definition_comment";

static const char *ha_blitz_exts[] = {
  BLITZ_DATAFILE_EXT,
  BLITZ_INDEX_FILE_EXT,
  BLITZ_SYSTEM_EXT,
  NULL
};

/* Multi Reader-Writer lock responsible for controlling concurrency
   at the handler level. This class is implemented in blitzlock.cc */
class BlitzLock {
private:
  int scanner_count;
  int updater_count;
  pthread_mutex_t mutex;
  pthread_cond_t condition;

public:
  BlitzLock();
  ~BlitzLock();

  void update_begin();
  void update_end();
  void scan_begin();
  void scan_end();
  void scan_update_begin();
  void scan_update_end();
};

/* Handler that takes care of all I/O to the the data dictionary
   that holds actual rows. */
class BlitzData {
private:
  TCHDB *data_table;          /* Where the actual row data lives */
  TCHDB *system_table;        /* Keeps track of system info */
  char *tc_meta_buffer;       /* Tokyo Cabinet's Persistent Meta Buffer */
  drizzled::atomic<uint64_t> current_hidden_row_id;

public:
  BlitzData() { current_hidden_row_id = 0; }
  ~BlitzData() {}
  bool startup(const char *table_path);
  bool shutdown(void);

  /* DATA DICTIONARY CREATION RELATED */
  TCHDB *open_table(const char *path, const char *ext, int mode);
  int create_table(const char *table_path, const char *ext);
  bool close_table(TCHDB *table);
  bool rename_table(const char *from, const char *to);
  bool write_table_definition(TCHDB *system_table,
                              drizzled::message::Table &proto);

  /* DATA DICTIONARY META INFO RELATED */
  uint64_t nrecords(void);

  /* DATA DICTIONARY READ RELATED*/
  char *get_row(const char *key, const size_t klen, int *value_len);
  char *next_key_and_row(const char *key, const size_t klen,
                         int *next_key_len, const char **value,
                         int *value_length);

  /* DATA DICTIONARY WRITE RELATED */
  uint64_t next_hidden_row_id(void);
  size_t generate_table_key(char *key_buffer);
  bool overwrite_row(const char *key, const size_t klen,
                     const unsigned char *row, const size_t rlen);
  bool delete_row(const char *key, const size_t klen);
  bool delete_all_rows(void);
};

/* Object shared among all worker threads. Try to only add
   data that will not be updated at runtime or those that
   do not require locking. */
class BlitzShare {
public:
  BlitzShare() : blitz_lock(), use_count(0) {}
  ~BlitzShare() {}

  THR_LOCK lock;           /* Shared Drizzle Lock */
  BlitzLock blitz_lock;    /* Handler level lock for BlitzDB */
  BlitzData dict;          /* Utility class of BlitzDB */
  std::string table_name;  /* Name and Length of the table */
  uint32_t use_count;      /* Reference counter of this object */
  bool fixed_length_table; /* Whether the table is fixed length */
  bool primary_key_exists; /* Whether a PK exists in this table */
};

class ha_blitz: public Cursor {
private:
  BlitzShare *share;                     /* Shared object among all threads */
  THR_LOCK_DATA lock;                    /* Drizzle Lock */

  /* THREAD STATE */
  bool table_scan;                       /* Whether a table scan is occuring */
  bool thread_locked;                    /* Whether the thread is locked */
  uint32_t sql_command_type;             /* Type of SQL command to process */

  /* KEY GENERATION SPECIFIC VARIABLES */
  char key_buffer[BLITZ_MAX_KEY_LENGTH]; /* Buffer for key generation */
  size_t generated_key_length;           /* Length of the generated key */

  /* TABLE SCANNER SPECIFIC VARIABLES */
  char *current_key;                     /* Current key in table scan */
  const char *current_row;               /* Current row in table scan */
  int current_key_length;                /* Length of the current key */
  int current_row_length;                /* Length of the current row */
  char *updateable_key;                  /* Used in table scan */
  int updateable_key_length;             /* Length of updateable key */

  /* ROW PROCESSING SPECIFIC VARIABLES */
  unsigned char pack_buffer[BLITZ_MAX_ROW_STACK]; /* Pack Buffer */
  unsigned char *secondary_row_buffer;            /* For big rows */
  size_t secondary_row_buffer_size;               /* Reserved buffer size */

public:
  ha_blitz(drizzled::plugin::StorageEngine &engine_arg, TableShare &table_arg);
  ~ha_blitz() {}

  const char *index_type(uint32_t) { return "NONE"; };
  const char **bas_ext() const;

  int open(const char *name, int mode, uint32_t open_options);
  int close(void);
  int info(uint32_t flag);

  int rnd_init(bool scan);
  int rnd_next(unsigned char *buf);
  int rnd_end();
  int rnd_pos(unsigned char *buf, unsigned char *pos);

  void position(const unsigned char *record);

  int write_row(unsigned char *buf);
  int update_row(const unsigned char *old_data, unsigned char *new_data);
  int delete_row(const unsigned char *buf);
  int delete_all_rows(void);

  int critical_section_enter();
  int critical_section_exit();

  /* BLITZDB SPECIFIC THREAD SPECIFIC FUNCTIONS */
  uint32_t max_row_length(void);
  size_t pack_row(unsigned char *row_buffer, unsigned char *row_to_pack);
  bool unpack_row(unsigned char *to, const char *from, const size_t from_len);
  unsigned char *get_pack_buffer(const size_t size);
};

class BlitzEngine : public drizzled::plugin::StorageEngine {
public:
  BlitzEngine(const string &name_arg)
    : drizzled::plugin::StorageEngine(name_arg,
                                      HTON_FILE_BASED |
                                      HTON_SKIP_STORE_LOCK) {
    table_definition_ext = BLITZ_SYSTEM_EXT;
  }

  virtual Cursor *create(TableShare &table, MEM_ROOT *mem_root) {
    return new (mem_root) ha_blitz(*this, table);
  }

  const char **bas_ext() const {
    return ha_blitz_exts;
  }

  int doCreateTable(Session *session, const char *table_name,
                    Table &table_arg, drizzled::message::Table&);

  int doRenameTable(Session *session, const char *from, const char *to);

  int doDropTable(Session&, const string table_name); 

  int doGetTableDefinition(Session& session, const char *path, const char *db,
                           const char *table_name, const bool is_tmp,
                           drizzled::message::Table *table_proto);

  void doGetTableNames(CachedDirectory &directory, string&,
                       set<string>& set_of_names);

  uint32_t max_supported_keys() const { return 0; }
  uint32_t max_supported_key_length() const { return 0; }
  uint32_t max_supported_key_parts() const { return 0; }
  uint32_t max_supported_key_part_length() const { return 0; }

  uint32_t index_flags(enum ha_key_alg) const {
    return HA_ONLY_WHOLE_INDEX;
  }
};

#endif /* STORAGE_BLITZ_HA_BLITZ_H */
