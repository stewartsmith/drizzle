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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef PLUGIN_CSV_HA_TINA_H
#define PLUGIN_CSV_HA_TINA_H

#include <drizzled/cursor.h>
#include <drizzled/thr_lock.h>

#include <sys/types.h>
#include <sys/stat.h>
#include "transparent_file.h"

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
  explicit TinaShare(const std::string &name);
  ~TinaShare();

  std::string table_name;
  std::string data_file_name;
  uint32_t use_count;
  /*
    Here we save the length of the file for readers. This is updated by
    inserts, updates and deletes. The var is initialized along with the
    share initialization.
  */
  off_t saved_data_file_length;
  pthread_mutex_t mutex;
  bool update_file_opened;
  bool tina_write_opened;
  int meta_file;           /* Meta file we use */
  int tina_write_filedes;  /* File Cursor for readers */
  bool crashed;             /* Meta file is crashed */
  drizzled::ha_rows rows_recorded;    /* Number of rows in tables */
  uint32_t data_file_version;   /* Version of the data file used */
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
    deletes/updates. It is used in doEndTableScan() to get rid of them
    in the end of the query.
  */
  std::vector< std::pair<off_t, off_t> > chain;
  uint32_t local_data_file_version;  /* Saved version of the data file used */
  bool records_is_known;
  drizzled::memory::Root blobroot;

  bool get_write_pos(off_t *end_pos,
                     std::vector< std::pair<off_t, off_t> >::iterator &closest_hole);
  int open_update_temp_file_if_needed();
  int init_tina_writer();
  int init_data_file();

public:
  ha_tina(drizzled::plugin::StorageEngine &engine, drizzled::Table &table_arg);
  ~ha_tina()
  {
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
    @TODO return actual upper bound of number of records in the table.
    (e.g. save number of records seen on full table scan and/or use file size
    as upper bound)
  */
  drizzled::ha_rows estimate_rows_upper_bound() { return HA_POS_ERROR; }

  int doOpen(const drizzled::TableIdentifier &identifier, int mode, uint32_t test_if_locked);
  int open(const char *, int , uint32_t ) { assert(0); return -1; }
  int close(void);
  int doInsertRecord(unsigned char * buf);
  int doUpdateRecord(const unsigned char * old_data, unsigned char * new_data);
  int doDeleteRecord(const unsigned char * buf);
  int doStartTableScan(bool scan=1);
  int rnd_next(unsigned char *buf);
  int rnd_pos(unsigned char * buf, unsigned char *pos);
  int doEndTableScan();
  TinaShare *get_share(const std::string &table_name);
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

  /* The following methods were added just for TINA */
  int encode_quote(unsigned char *buf);
  int find_current_row(unsigned char *buf);
  int chain_append();
};

#endif /* PLUGIN_CSV_HA_TINA_H */
