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


#include <inttypes.h>
#include <zlib.h>
#include "azio.h"
#include <mysys/thr_lock.h>
#include <mysys/hash.h>
#include <drizzled/handler.h>

/*
  Please read ha_archive.cc first. If you are looking for more general
  answers on how storage engines work, look at ha_example.cc and
  ha_example.h.
*/

typedef struct st_archive_record_buffer {
  unsigned char *buffer;
  uint32_t length;
} archive_record_buffer;


typedef struct st_archive_share {
  char *table_name;
  char data_file_name[FN_REFLEN];
  uint32_t table_name_length,use_count;
  pthread_mutex_t mutex;
  THR_LOCK lock;
  azio_stream archive_write;     /* Archive file we are working with */
  bool archive_write_open;
  bool dirty;               /* Flag for if a flush should occur */
  bool crashed;             /* Meta file is crashed */
  ha_rows rows_recorded;    /* Number of rows in tables */
  uint64_t mean_rec_length;
  char real_path[FN_REFLEN];
  unsigned int  version;
  ha_rows version_rows;
} ARCHIVE_SHARE;

/*
  Version for file format.
  1 - Initial Version (Never Released)
  2 - Stream Compression, seperate blobs, no packing
  3 - One steam (row and blobs), with packing
*/
#define ARCHIVE_VERSION 3

class ha_archive: public handler
{
  THR_LOCK_DATA lock;        /* MySQL lock */
  ARCHIVE_SHARE *share;      /* Shared lock info */
  
  azio_stream archive;            /* Archive file we are working with */
  my_off_t current_position;  /* The position of the row we just read */
  unsigned char byte_buffer[IO_SIZE]; /* Initial buffer for our string */
  String buffer;             /* Buffer used for blob storage */
  ha_rows scan_rows;         /* Number of rows left in scan */
  bool delayed_insert;       /* If the insert is delayed */
  bool bulk_insert;          /* If we are performing a bulk insert */
  const unsigned char *current_key;
  uint32_t current_key_len;
  uint32_t current_k_offset;
  archive_record_buffer *record_buffer;
  bool archive_reader_open;

  archive_record_buffer *create_record_buffer(unsigned int length);
  void destroy_record_buffer(archive_record_buffer *r);

public:
  ha_archive(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_archive()
  {
  }
  const char *table_type() const { return "ARCHIVE"; }
  const char *index_type(uint32_t inx __attribute__((unused)))
  { return "NONE"; }
  const char **bas_ext() const;
  uint64_t table_flags() const
  {
    return (HA_NO_TRANSACTIONS | HA_REC_NOT_IN_SEQ |
            HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE |
            HA_STATS_RECORDS_IS_EXACT |
            HA_HAS_RECORDS |
            HA_FILE_BASED);
  }
  uint32_t index_flags(uint32_t idx __attribute__((unused)),
                       uint32_t part __attribute__((unused)),
                       bool all_parts __attribute__((unused))) const
  {
    return HA_ONLY_WHOLE_INDEX;
  }
  virtual void get_auto_increment(uint64_t offset, uint64_t increment,
                                  uint64_t nb_desired_values,
                                  uint64_t *first_value,
                                  uint64_t *nb_reserved_values);
  uint32_t max_supported_keys()          const { return 1; }
  uint32_t max_supported_key_length()    const { return sizeof(uint64_t); }
  uint32_t max_supported_key_part_length() const { return sizeof(uint64_t); }
  ha_rows records() { return share->rows_recorded; }
  int index_init(uint32_t keynr, bool sorted);
  virtual int index_read(unsigned char * buf, const unsigned char * key,
			 uint32_t key_len, enum ha_rkey_function find_flag);
  virtual int index_read_idx(unsigned char * buf, uint32_t index, const unsigned char * key,
			     uint32_t key_len, enum ha_rkey_function find_flag);
  int index_next(unsigned char * buf);
  int open(const char *name, int mode, uint32_t test_if_locked);
  int close(void);
  int write_row(unsigned char * buf);
  int real_write_row(unsigned char *buf, azio_stream *writer);
  int delete_all_rows();
  int rnd_init(bool scan=1);
  int rnd_next(unsigned char *buf);
  int rnd_pos(unsigned char * buf, unsigned char *pos);
  int get_row(azio_stream *file_to_read, unsigned char *buf);
  int get_row_version2(azio_stream *file_to_read, unsigned char *buf);
  int get_row_version3(azio_stream *file_to_read, unsigned char *buf);
  ARCHIVE_SHARE *get_share(const char *table_name, int *rc);
  int free_share();
  int init_archive_writer();
  int init_archive_reader();
  bool auto_repair() const { return 1; } // For the moment we just do this
  int read_data_header(azio_stream *file_to_read);
  void position(const unsigned char *record);
  int info(uint);
  void update_create_info(HA_CREATE_INFO *create_info);
  int create(const char *name, Table *form, HA_CREATE_INFO *create_info);
  int optimize(Session* session, HA_CHECK_OPT* check_opt);
  int repair(Session* session, HA_CHECK_OPT* check_opt);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();
  enum row_type get_row_type() const 
  { 
    return ROW_TYPE_COMPRESSED;
  }
  THR_LOCK_DATA **store_lock(Session *session, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
  bool is_crashed() const;
  int check(Session* session, HA_CHECK_OPT* check_opt);
  bool check_and_repair(Session *session);
  uint32_t max_row_length(const unsigned char *buf);
  bool fix_rec_buff(unsigned int length);
  int unpack_row(azio_stream *file_to_read, unsigned char *record);
  unsigned int pack_row(unsigned char *record);
};

