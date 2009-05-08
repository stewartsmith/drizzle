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

#ifndef STORAGE_CSV_HA_TINA_H
#define STORAGE_CSV_HA_TINA_H

#include <drizzled/handler.h>
#include <mysys/thr_lock.h>

#include <sys/types.h>
#include <sys/stat.h>
#include "transparent_file.h"

#define DEFAULT_CHAIN_LENGTH 512
/*
  Version for file format.
  1 - Initial Version. That is, the version when the metafile was introduced.
*/

#define TINA_VERSION 1

typedef struct st_tina_share {
  char *table_name;
  char data_file_name[FN_REFLEN];
  uint32_t table_name_length, use_count;
  /*
    Here we save the length of the file for readers. This is updated by
    inserts, updates and deletes. The var is initialized along with the
    share initialization.
  */
  off_t saved_data_file_length;
  pthread_mutex_t mutex;
  THR_LOCK lock;
  bool update_file_opened;
  bool tina_write_opened;
  File meta_file;           /* Meta file we use */
  File tina_write_filedes;  /* File handler for readers */
  bool crashed;             /* Meta file is crashed */
  ha_rows rows_recorded;    /* Number of rows in tables */
  uint32_t data_file_version;   /* Version of the data file used */
} TINA_SHARE;

struct tina_set {
  off_t begin;
  off_t end;
};

class ha_tina: public handler
{
  THR_LOCK_DATA lock;      /* MySQL lock */
  TINA_SHARE *share;       /* Shared lock info */
  off_t current_position;  /* Current position in the file during a file scan */
  off_t next_position;     /* Next position in the file scan */
  off_t local_saved_data_file_length; /* save position for reads */
  off_t temp_file_length;
  unsigned char byte_buffer[IO_SIZE];
  Transparent_file *file_buff;
  File data_file;                   /* File handler for readers */
  File update_temp_file;
  String buffer;
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
  MEM_ROOT blobroot;

private:
  bool get_write_pos(off_t *end_pos, tina_set *closest_hole);
  int open_update_temp_file_if_needed();
  int init_tina_writer();
  int init_data_file();

public:
  ha_tina(StorageEngine *engine, TableShare *table_arg);
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
  const char **bas_ext() const;
  uint64_t table_flags() const
  {
    return (HA_NO_TRANSACTIONS | HA_REC_NOT_IN_SEQ | HA_NO_AUTO_INCREMENT);
  }
  uint32_t index_flags(uint32_t, uint32_t, bool) const
  {
    /*
      We will never have indexes so this will never be called(AKA we return
      zero)
    */
    return 0;
  }
  uint32_t max_record_length() const { return HA_MAX_REC_LENGTH; }
  uint32_t max_keys()          const { return 0; }
  uint32_t max_key_parts()     const { return 0; }
  uint32_t max_key_length()    const { return 0; }
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
  ha_rows estimate_rows_upper_bound() { return HA_POS_ERROR; }

  int open(const char *name, int mode, uint32_t open_options);
  int close(void);
  int write_row(unsigned char * buf);
  int update_row(const unsigned char * old_data, unsigned char * new_data);
  int delete_row(const unsigned char * buf);
  int rnd_init(bool scan=1);
  int rnd_next(unsigned char *buf);
  int rnd_pos(unsigned char * buf, unsigned char *pos);
  bool check_and_repair(Session *session);
  int check(Session* session, HA_CHECK_OPT* check_opt);
  bool is_crashed() const;
  int rnd_end();
  int repair(Session* session, HA_CHECK_OPT* check_opt);
  /* This is required for SQL layer to know that we support autorepair */
  bool auto_repair() const { return 1; }
  void position(const unsigned char *record);
  int info(uint);
  int delete_all_rows(void);
  int create(const char *name, Table *form, HA_CREATE_INFO *create_info);

  THR_LOCK_DATA **store_lock(Session *session, THR_LOCK_DATA **to,
      enum thr_lock_type lock_type);

  /*
    These functions used to get/update status of the handler.
    Needed to enable concurrent inserts.
  */
  void get_status();
  void update_status();

  /* The following methods were added just for TINA */
  int encode_quote(unsigned char *buf);
  int find_current_row(unsigned char *buf);
  int chain_append();
};

#endif /* STORAGE_CSV_HA_TINA_H */
