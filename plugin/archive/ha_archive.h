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

#ifndef PLUGIN_ARCHIVE_HA_ARCHIVE_H
#define PLUGIN_ARCHIVE_HA_ARCHIVE_H

/*
  Please read ha_archive.cc first. If you are looking for more general
  answers on how storage engines work, look at ha_example.cc and
  ha_example.h.
*/

class ArchiveShare {
public:
  ArchiveShare();
  ArchiveShare(const char *name);
  ~ArchiveShare();

  bool prime(uint64_t *auto_increment);

  std::string table_name;
  std::string data_file_name;
  uint32_t use_count;
  pthread_mutex_t _mutex;
  drizzled::THR_LOCK _lock;
  azio_stream archive_write;     /* Archive file we are working with */
  bool archive_write_open;
  bool dirty;               /* Flag for if a flush should occur */
  bool crashed;             /* Meta file is crashed */
  uint64_t mean_rec_length;
  char real_path[FN_REFLEN];
  uint64_t  version;
  drizzled::ha_rows rows_recorded;    /* Number of rows in tables */
  drizzled::ha_rows version_rows;

  pthread_mutex_t &mutex()
  {
    return _mutex;
  }
};

/*
  Version for file format.
  1 - Initial Version (Never Released)
  2 - Stream Compression, seperate blobs, no packing
  3 - One steam (row and blobs), with packing
*/
#define ARCHIVE_VERSION 3

class ha_archive: public drizzled::Cursor
{
  drizzled::THR_LOCK_DATA lock;        /* MySQL lock */
  ArchiveShare *share;      /* Shared lock info */

  azio_stream archive;            /* Archive file we are working with */
  drizzled::internal::my_off_t current_position;  /* The position of the row we just read */
  unsigned char byte_buffer[IO_SIZE]; /* Initial buffer for our string */
  drizzled::String buffer;             /* Buffer used for blob storage */
  drizzled::ha_rows scan_rows;         /* Number of rows left in scan */
  bool delayed_insert;       /* If the insert is delayed */
  bool bulk_insert;          /* If we are performing a bulk insert */
  const unsigned char *current_key;
  uint32_t current_key_len;
  uint32_t current_k_offset;
  std::vector <unsigned char> record_buffer;
  bool archive_reader_open;

public:
  ha_archive(drizzled::plugin::StorageEngine &engine_arg,
             drizzled::Table &table_arg);
  ~ha_archive()
  { }

  const char *index_type(uint32_t)
  { return "NONE"; }
  void get_auto_increment(uint64_t, uint64_t, uint64_t,
                          uint64_t *first_value, uint64_t *nb_reserved_values);
  drizzled::ha_rows records() { return share->rows_recorded; }
  int doStartIndexScan(uint32_t keynr, bool sorted);
  virtual int index_read(unsigned char * buf, const unsigned char * key,
			 uint32_t key_len,
                         drizzled::ha_rkey_function find_flag);
  int index_next(unsigned char * buf);
  int doOpen(const drizzled::identifier::Table &identifier, int mode, uint32_t test_if_locked);
  int open(const char *name, int mode, uint32_t test_if_locked);
  int close(void);
  int doInsertRecord(unsigned char * buf);
  int real_write_row(unsigned char *buf, azio_stream *writer);
  int delete_all_rows();
  int doStartTableScan(bool scan=1);
  int rnd_next(unsigned char *buf);
  int rnd_pos(unsigned char * buf, unsigned char *pos);
  ArchiveShare *get_share(const char *table_name, int *rc);
  int free_share();
  int init_archive_writer();
  int init_archive_reader();
  void position(const unsigned char *record);
  int info(uint);
private:
  int get_row(azio_stream *file_to_read, unsigned char *buf);
  int get_row_version2(azio_stream *file_to_read, unsigned char *buf);
  int get_row_version3(azio_stream *file_to_read, unsigned char *buf);
  int read_data_header(azio_stream *file_to_read);
  int optimize();
  int repair();
public:
  void start_bulk_insert(drizzled::ha_rows rows);
  int end_bulk_insert();

  drizzled::THR_LOCK_DATA **store_lock(drizzled::Session *session,
                                       drizzled::THR_LOCK_DATA **to,
                                       drizzled::thr_lock_type lock_type);
  int check(drizzled::Session* session);
  bool check_and_repair(drizzled::Session *session);
  uint32_t max_row_length(const unsigned char *buf);
  bool fix_rec_buff(unsigned int length);
  int unpack_row(azio_stream *file_to_read, unsigned char *record);
  unsigned int pack_row(unsigned char *record);
};

#endif /* PLUGIN_ARCHIVE_HA_ARCHIVE_H */
