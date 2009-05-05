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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */



/* class for the the heap handler */

#ifndef STORAGE_HEAP_HA_HEAP_H
#define STORAGE_HEAP_HA_HEAP_H

#include <drizzled/handler.h>
#include <mysys/thr_lock.h>

typedef struct st_heap_info HP_INFO;
typedef struct st_heap_share HEAP_SHARE;
typedef unsigned char *HEAP_PTR;


class ha_heap: public handler
{
  HP_INFO *file;
  HP_SHARE *internal_share;
  key_map btree_keys;
  /* number of records changed since last statistics update */
  uint32_t    records_changed;
  uint32_t    key_stat_version;
  bool internal_table;
public:
  ha_heap(StorageEngine *engine, TableShare *table);
  ~ha_heap() {}
  handler *clone(MEM_ROOT *mem_root);
  const char *table_type() const
  {
    return "MEMORY";
  }
  const char *index_type(uint32_t inx);
  enum row_type get_row_type() const;
  const char **bas_ext() const;
  uint64_t table_flags() const
  {
    return (HA_FAST_KEY_READ | HA_NO_BLOBS | HA_NULL_IN_KEY |
            HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE |
            HA_REC_NOT_IN_SEQ | HA_NO_TRANSACTIONS |
            HA_HAS_RECORDS | HA_STATS_RECORDS_IS_EXACT);
  }
  uint32_t index_flags(uint32_t inx, uint32_t part, bool all_parts) const;
  const key_map *keys_to_use_for_scanning() { return &btree_keys; }
  uint32_t max_supported_keys()          const { return MAX_KEY; }
  uint32_t max_supported_key_part_length() const { return MAX_KEY_LENGTH; }
  double scan_time()
  { return (double) (stats.records+stats.deleted) / 20.0+10; }
  double read_time(uint32_t, uint32_t,
                   ha_rows rows)
  { return (double) rows /  20.0+1; }

  int open(const char *name, int mode, uint32_t test_if_locked);
  int close(void);
  void set_keys_for_scanning(void);
  int write_row(unsigned char * buf);
  int update_row(const unsigned char * old_data, unsigned char * new_data);
  int delete_row(const unsigned char * buf);
  virtual void get_auto_increment(uint64_t offset, uint64_t increment,
                                  uint64_t nb_desired_values,
                                  uint64_t *first_value,
                                  uint64_t *nb_reserved_values);
  int index_read_map(unsigned char * buf, const unsigned char * key,
                     key_part_map keypart_map,
                     enum ha_rkey_function find_flag);
  int index_read_last_map(unsigned char *buf, const unsigned char *key,
                          key_part_map keypart_map);
  int index_read_idx_map(unsigned char * buf, uint32_t index,
                         const unsigned char * key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag);
  int index_next(unsigned char * buf);
  int index_prev(unsigned char * buf);
  int index_first(unsigned char * buf);
  int index_last(unsigned char * buf);
  int rnd_init(bool scan);
  int rnd_next(unsigned char *buf);
  int rnd_pos(unsigned char * buf, unsigned char *pos);
  void position(const unsigned char *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int reset();
  int external_lock(Session *session, int lock_type);
  int delete_all_rows(void);
  int disable_indexes(uint32_t mode);
  int enable_indexes(uint32_t mode);
  int indexes_are_disabled(void);
  ha_rows records_in_range(uint32_t inx, key_range *min_key, key_range *max_key);
  int delete_table(const char *from);
  void drop_table(const char *name);
  int rename_table(const char * from, const char * to);
  int create(const char *name, Table *form, HA_CREATE_INFO *create_info);
  void update_create_info(HA_CREATE_INFO *create_info);

  THR_LOCK_DATA **store_lock(Session *session, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
  int cmp_ref(const unsigned char *ref1, const unsigned char *ref2);
  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint32_t table_changes);
private:
  void update_key_stats();
};

#endif /* STORAGE_HEAP_HA_HEAP_H */
