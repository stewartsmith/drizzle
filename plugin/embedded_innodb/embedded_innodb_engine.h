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

#ifndef PLUGIN_EMBEDDED_INNODB_EMBEDDED_INNODB_ENGINE_H
#define PLUGIN_EMBEDDED_INNODB_EMBEDDED_INNODB_ENGINE_H

#include <drizzled/cursor.h>
#include <drizzled/atomics.h>

class EmbeddedInnoDBTableShare
{
public:
  EmbeddedInnoDBTableShare(const char* name, uint64_t intial_auto_increment_value);

  drizzled::THR_LOCK lock;
  int use_count;
  std::string table_name;

  drizzled::atomic<uint64_t> auto_increment_value;
};

class EmbeddedInnoDBCursor: public drizzled::Cursor
{
public:
  EmbeddedInnoDBCursor(drizzled::plugin::StorageEngine &engine, drizzled::TableShare &table_arg);
  ~EmbeddedInnoDBCursor()
  {}

  /*
    The name of the index type that will be used for display
    don't implement this method unless you really have indexes
  */
  const char *index_type(uint32_t key_number);
  uint32_t index_flags(uint32_t inx) const;
  int open(const char *name, int mode, uint32_t test_if_locked);
  int close(void);
  int write_row(unsigned char * buf);
  int rnd_init(bool scan);
  int rnd_next(unsigned char *buf);
  int rnd_end();
  int rnd_pos(unsigned char * buf, unsigned char *pos);

  int index_init(uint32_t, bool);
  int index_read(unsigned char *buf, const unsigned char *key_ptr,
                 uint32_t key_len, drizzled::ha_rkey_function find_flag);

  int index_next(unsigned char * buf);
  int index_end();
  int index_prev(unsigned char * buf);
  int index_first(unsigned char * buf);
  int index_last(unsigned char * buf);
  void position(const unsigned char *record);
  int info(uint32_t flag);
  double scan_time();
  int delete_row(const unsigned char *);
  int delete_all_rows(void);
  int update_row(const unsigned char * old_data, unsigned char * new_data);

  EmbeddedInnoDBTableShare *get_share(const char *table_name, int *rc);
  int free_share();

  EmbeddedInnoDBTableShare *share;
  drizzled::THR_LOCK_DATA lock;  /* lock for store_lock. this is ass. */
  drizzled::THR_LOCK_DATA **store_lock(drizzled::Session *,
                                       drizzled::THR_LOCK_DATA **to,
                                       drizzled::thr_lock_type);

  uint64_t getInitialAutoIncrementValue();
  void get_auto_increment(uint64_t ,
                          uint64_t ,
                          uint64_t ,
                          uint64_t *first_value,
                          uint64_t *nb_reserved_values);

private:
  ib_crsr_t cursor;
  ib_tpl_t tuple;

  ib_err_t next_innodb_error;
};

#endif /* PLUGIN_EMBEDDED_INNODB_EMBEDDED_INNODB_ENGINE_H */
