/* Copyright (C) 2003 MySQL AB

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

#ifndef PLUGIN_CSV_HA_TINA_H
#define PLUGIN_CSV_HA_TINA_H

#include <drizzled/cursor.h>
#include <drizzled/thr_lock.h>

#include <sys/types.h>
#include <sys/stat.h>
#include "transparent_file.h"

#define DEFAULT_CHAIN_LENGTH 512
/*
  Version for file format.
  1 - Initial Version. That is, the version when the metafile was introduced.
*/

#define TINA_VERSION 1

class TinaShare
{
  TinaShare();
  TinaShare(const TinaShare &);
  TinaShare& operator=(const TinaShare &);
public:
  explicit TinaShare(const char *name);
  ~TinaShare();

  std::string table_name;
  char data_file_name[FN_REFLEN];
  uint32_t use_count;
  /*
    Here we save the length of the file for readers. This is updated by
    inserts, updates and deletes. The var is initialized along with the
    share initialization.
  */
  off_t saved_data_file_length;
  pthread_mutex_t mutex;
  drizzled::THR_LOCK lock;
  bool update_file_opened;
  bool tina_write_opened;
  int meta_file;           /* Meta file we use */
  int tina_write_filedes;  /* File Cursor for readers */
  bool crashed;             /* Meta file is crashed */
  drizzled::ha_rows rows_recorded;    /* Number of rows in tables */
  uint32_t data_file_version;   /* Version of the data file used */
};

struct tina_set {
  off_t begin;
  off_t end;
};

class ha_tina: public drizzled::Cursor
{
  drizzled::THR_LOCK_DATA lock;      /* MySQL lock */
  TinaShare *share;       /* Shared lock info */
  off_t current_position;  /* Current position in the file during a file scan */
  off_t next_position;     /* Next position in the file scan */
  off_t local_saved_data_file_length; /* save position for reads */
  off_t temp_file_length;
  unsigned char byte_buffer[IO_SIZE];
  Transparent_file *file_buff;
  int data_file;                   /* File Cursor for readers */
  int update_temp_file;
  drizzled::String buffer;
  /*
    The chain contains "holes" in the file, occured because of
    deletes/updates. It is used in rnd_end() to get rid of them
    in the end of the query.
  */
  tina_set chain_buffer[DEFAULT_CHAIN_LENGTH];
  tina_set *chain;
  tina_set *chain_ptr;
  unsigned char chain_alloced;
  uint32_t chain_size;
  uint32_t local_data_file_version;  /* Saved version of the data file used */
  bool records_is_known;
  drizzled::memory::Root blobroot;

  bool get_write_pos(off_t *end_pos, tina_set *closest_hole);
  int open_update_temp_file_if_needed();
  int init_tina_writer();
  int init_data_file();

public:
  ha_tina(drizzled::plugin::StorageEngine &engine, drizzled::TableShare &table_arg);
  ~ha_tina()
  {
    if (chain_alloced)
      free(chain);
    if (file_buff)
      delete file_buff;
  }
  const char *table_type(void) const { return "CSV"; }
  const char *index_type(uint32_t)
  { return "NONE"; }

  /*
     Called in test_quick_select to determine if indexes should be used.
   */
  virtual double scan_time() { return (double) (stats.records+stats.deleted) / 20.0+10; }

  /* The next method will never be called */
  virtual bool fast_key_read() { return 1;}
  /*
    TODO: return actual upper bound of number of records in the table.
    (e.g. save number of records seen on full table scan and/or use file size
    as upper bound)
  */
  drizzled::ha_rows estimate_rows_upper_bound() { return HA_POS_ERROR; }

  int open(const char *name, int mode, uint32_t open_options);
  int close(void);
  int doInsertRecord(unsigned char * buf);
  int doUpdateRecord(const unsigned char * old_data, unsigned char * new_data);
  int doDeleteRecord(const unsigned char * buf);
  int rnd_init(bool scan=1);
  int rnd_next(unsigned char *buf);
  int rnd_pos(unsigned char * buf, unsigned char *pos);
  int rnd_end();
  TinaShare *get_share(const char *table_name);
  int free_share();
  int repair(drizzled::Session* session, drizzled::HA_CHECK_OPT* check_opt);
  /* This is required for SQL layer to know that we support autorepair */
  void position(const unsigned char *record);
  int info(uint);
  int delete_all_rows(void);
  void get_auto_increment(uint64_t, uint64_t,
                          uint64_t,
                          uint64_t *,
                          uint64_t *)
  {}

  /*
    These functions used to get/update status of the Cursor.
    Needed to enable concurrent inserts.
  */
  void get_status();
  void update_status();

  /* The following methods were added just for TINA */
  int encode_quote(unsigned char *buf);
  int find_current_row(unsigned char *buf);
  int chain_append();
};

#endif /* PLUGIN_CSV_HA_TINA_H */
