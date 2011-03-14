/*
  Copyright (C) 2010 Stewart Smith

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#pragma once

#include <drizzled/cursor.h>
#include <drizzled/atomics.h>

class HailDBTableShare
{
public:
  HailDBTableShare(const char* name, bool hidden_primary_key);

  drizzled::THR_LOCK lock;
  int use_count;
  std::string table_name;

  drizzled::atomic<uint64_t> auto_increment_value;
  drizzled::atomic<uint64_t> hidden_pkey_auto_increment_value;
  bool has_hidden_primary_key;
};

class HailDBCursor: public drizzled::Cursor
{
public:
  HailDBCursor(drizzled::plugin::StorageEngine &engine, drizzled::Table &table_arg);
  ~HailDBCursor()
  {}

  /*
    The name of the index type that will be used for display
    don't implement this method unless you really have indexes
  */
  const char *index_type(uint32_t key_number);
  uint32_t index_flags(uint32_t inx) const;
  int open(const char *name, int mode, uint32_t test_if_locked);
  int close(void);
  int external_lock(drizzled::Session* session, int lock_type);
  int doInsertRecord(unsigned char * buf);
  int doStartTableScan(bool scan);
  int rnd_next(unsigned char *buf);
  int doEndTableScan();
  int rnd_pos(unsigned char * buf, unsigned char *pos);

  int doStartIndexScan(uint32_t, bool);
  int index_read(unsigned char *buf, const unsigned char *key_ptr,
                 uint32_t key_len, drizzled::ha_rkey_function find_flag);

  int haildb_index_read(unsigned char *buf,
                        const unsigned char *key_ptr,
                        uint32_t key_len,
                        drizzled::ha_rkey_function find_flag,
                        bool allocate_blobs);

  uint32_t calculate_key_len(uint32_t key_position,
                             drizzled::key_part_map keypart_map_arg);
  int haildb_index_read_map(unsigned char * buf,
                            const unsigned char *key,
                            drizzled::key_part_map keypart_map,
                            drizzled::ha_rkey_function find_flag,
                            bool allocate_blobs);
  int index_read_idx_map(unsigned char * buf,
                         uint32_t index,
                         const unsigned char * key,
                         drizzled::key_part_map keypart_map,
                         drizzled::ha_rkey_function find_flag);

  int index_next(unsigned char * buf);
  int doEndIndexScan();
  int index_prev(unsigned char * buf);
  int index_first(unsigned char * buf);
  int index_last(unsigned char * buf);
  void position(const unsigned char *record);
  int info(uint32_t flag);
  double scan_time();
  int doDeleteRecord(const unsigned char *);
  int delete_all_rows(void);
  int doUpdateRecord(const unsigned char * old_data, unsigned char * new_data);
  int extra(drizzled::ha_extra_function operation);

  HailDBTableShare *get_share(const char *table_name,
                              bool has_hidden_primary_key,
                              int *rc);
  int free_share();

  HailDBTableShare *share;
  drizzled::THR_LOCK_DATA lock;  /* lock for store_lock. this is ass. */
  drizzled::THR_LOCK_DATA **store_lock(drizzled::Session *,
                                       drizzled::THR_LOCK_DATA **to,
                                       drizzled::thr_lock_type);

  uint64_t getInitialAutoIncrementValue();
  uint64_t getHiddenPrimaryKeyInitialAutoIncrementValue();

  void get_auto_increment(uint64_t ,
                          uint64_t ,
                          uint64_t ,
                          uint64_t *first_value,
                          uint64_t *nb_reserved_values);

  int reset();
  int analyze(drizzled::Session* session);

private:
  ib_id_t table_id;
  ib_crsr_t cursor;
  ib_tpl_t tuple;
  bool advance_cursor;
  ib_lck_mode_t ib_lock_mode;
  bool cursor_is_sec_index;

  bool write_can_replace;
  uint64_t hidden_autoinc_pkey_position;
  drizzled::memory::Root *blobroot;

  bool in_table_scan;
};

int get_haildb_system_table_message(const char* table_name, drizzled::message::Table *table_message);

