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


#ifndef STORAGE_MYISAM_HA_MYISAM_H
#define STORAGE_MYISAM_HA_MYISAM_H

#include <drizzled/handler.h>
#include <mysys/thr_lock.h>

/* class for the the myisam handler */

#include <storage/myisam/myisam.h>

#define HA_RECOVER_NONE		0	/* No automatic recover */
#define HA_RECOVER_DEFAULT	1	/* Automatic recover active */
#define HA_RECOVER_BACKUP	2	/* Make a backupfile on recover */
#define HA_RECOVER_FORCE	4	/* Recover even if we loose rows */
#define HA_RECOVER_QUICK	8	/* Don't check rows in data file */

extern TYPELIB myisam_recover_typelib;
extern ulong myisam_recover_options;

#ifdef __cplusplus
extern "C" {
#endif

bool index_cond_func_myisam(void *arg);

#ifdef __cplusplus
}
#endif

class ha_myisam: public handler
{
  MI_INFO *file;
  uint64_t int_table_flags;
  char    *data_file_name, *index_file_name;
  bool can_enable_indexes;
  int repair(Session *session, MI_CHECK &param, bool optimize);

 public:
  ha_myisam(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_myisam() {}
  handler *clone(MEM_ROOT *mem_root);
  const char *table_type() const { return "MyISAM"; }
  const char *index_type(uint32_t key_number);
  const char **bas_ext() const;
  uint64_t table_flags() const { return int_table_flags; }
  int index_init(uint32_t idx, bool sorted);
  int index_end();
  uint32_t index_flags(uint32_t inx, uint32_t part, bool all_parts) const;
  uint32_t max_supported_keys()          const { return MI_MAX_KEY; }
  uint32_t max_supported_key_length()    const { return MI_MAX_KEY_LENGTH; }
  uint32_t max_supported_key_part_length() const { return MI_MAX_KEY_LENGTH; }
  uint32_t checksum() const;

  int open(const char *name, int mode, uint32_t test_if_locked);
  int close(void);
  int write_row(unsigned char * buf);
  int update_row(const unsigned char * old_data, unsigned char * new_data);
  int delete_row(const unsigned char * buf);
  int index_read_map(unsigned char *buf, const unsigned char *key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);
  int index_read_idx_map(unsigned char *buf, uint32_t index, const unsigned char *key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag);
  int index_read_last_map(unsigned char *buf, const unsigned char *key, key_part_map keypart_map);
  int index_next(unsigned char * buf);
  int index_prev(unsigned char * buf);
  int index_first(unsigned char * buf);
  int index_last(unsigned char * buf);
  int index_next_same(unsigned char *buf, const unsigned char *key, uint32_t keylen);
  int rnd_init(bool scan);
  int rnd_next(unsigned char *buf);
  int rnd_pos(unsigned char * buf, unsigned char *pos);
  int restart_rnd_next(unsigned char *buf, unsigned char *pos);
  void position(const unsigned char *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, uint32_t cache_size);
  int reset(void);
  int external_lock(Session *session, int lock_type);
  int delete_all_rows(void);
  int disable_indexes(uint32_t mode);
  int enable_indexes(uint32_t mode);
  int indexes_are_disabled(void);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();
  ha_rows records_in_range(uint32_t inx, key_range *min_key, key_range *max_key);
  void update_create_info(HA_CREATE_INFO *create_info);
  int create(const char *name, Table *form, HA_CREATE_INFO *create_info);
  THR_LOCK_DATA **store_lock(Session *session, THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);
  virtual void get_auto_increment(uint64_t offset, uint64_t increment,
                                  uint64_t nb_desired_values,
                                  uint64_t *first_value,
                                  uint64_t *nb_reserved_values);
  int rename_table(const char * from, const char * to);
  int delete_table(const char *name);
  int check(Session* session, HA_CHECK_OPT* check_opt);
  int analyze(Session* session,HA_CHECK_OPT* check_opt);
  int repair(Session* session, HA_CHECK_OPT* check_opt);
  bool check_and_repair(Session *session);
  bool is_crashed() const;
  bool auto_repair() const { return myisam_recover_options != 0; }
  int optimize(Session* session, HA_CHECK_OPT* check_opt);
  int assign_to_keycache(Session* session, HA_CHECK_OPT* check_opt);
  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint32_t table_changes);
#ifdef HAVE_QUERY_CACHE
  bool register_query_cache_table(Session *session, char *table_key,
                                     uint32_t key_length,
                                     qc_engine_callback
                                     *engine_callback,
                                     uint64_t *engine_data);
#endif
  MI_INFO *file_ptr(void)
  {
    return file;
  }
  int read_range_first(const key_range *start_key, const key_range *end_key,
                       bool eq_range_arg, bool sorted);
  int read_range_next();
public:
  /**
   * Multi Range Read interface
   */
  int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                            uint32_t n_ranges, uint32_t mode, HANDLER_BUFFER *buf);
  int multi_range_read_next(char **range_info);
  ha_rows multi_range_read_info_const(uint32_t keyno, RANGE_SEQ_IF *seq,
                                      void *seq_init_param,
                                      uint32_t n_ranges, uint32_t *bufsz,
                                      uint32_t *flags, COST_VECT *cost);
  int multi_range_read_info(uint32_t keyno, uint32_t n_ranges, uint32_t keys,
                            uint32_t *bufsz, uint32_t *flags, COST_VECT *cost);

  /* Index condition pushdown implementation */
  Item *idx_cond_push(uint32_t keyno, Item* idx_cond);
  bool check_if_supported_virtual_columns(void) { return true; }
private:
  DsMrr_impl ds_mrr;
  key_map keys_with_parts;
  friend bool index_cond_func_myisam(void *arg);
};

#endif /* STORAGE_MYISAM_HA_MYISAM_H */
