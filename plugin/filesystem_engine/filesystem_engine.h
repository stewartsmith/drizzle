/*
  Copyright (C) 2010 Zimin

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

#ifndef PLUGIN_FILESYSTEM_ENGINE_FILESYSTEM_ENGINE_H
#define PLUGIN_FILESYSTEM_ENGINE_FILESYSTEM_ENGINE_H

#include <drizzled/cursor.h>
#include <drizzled/thr_lock.h>

#include <boost/scoped_ptr.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>

#include "transparent_file.h"
#include "formatinfo.h"
#include "filesystemlock.h"

class FilesystemTableShare
{
  FilesystemTableShare();
  FilesystemTableShare(const FilesystemTableShare &);
  FilesystemTableShare& operator=(const FilesystemTableShare &);
public:
  explicit FilesystemTableShare(const std::string name);
  ~FilesystemTableShare();

  uint32_t use_count;
  const std::string table_name;
  bool update_file_opened;
  bool needs_reopen;
  pthread_mutex_t mutex;
  FormatInfo format;
  std::vector< std::map<std::string, std::string> > vm;
  FilesystemLock filesystem_lock;
};

class FilesystemCursor : public drizzled::Cursor
{
  FilesystemTableShare *share;       /* Shared lock info */
  boost::scoped_ptr<TransparentFile> file_buff;
  int file_desc;
  std::string update_file_name;
  int update_file_desc;
  size_t tag_depth;
  off_t current_position;
  off_t next_position;
  bool thread_locked;
  uint32_t sql_command_type; /* Type of SQL command to process */
  /* each slot means an interval in a file which will be deleted later */
  std::vector< std::pair<off_t, off_t> > slots;

public:
  FilesystemCursor(drizzled::plugin::StorageEngine &engine, drizzled::Table &table_arg);
  ~FilesystemCursor()
  {
  }

  /** @brief
    The name that will be used for display purposes.
   */
  const char *table_type(void) const { return "FILESYSTEM"; }

  /*
     Called in test_quick_select to determine if indexes should be used.
   */
  virtual double scan_time() { return (double) (stats.records+stats.deleted) / 20.0+10; }

  /* The next method will never be called */
  virtual bool fast_key_read() { return 1;}
  drizzled::ha_rows estimate_rows_upper_bound() { return HA_POS_ERROR; }

  int doOpen(const drizzled::TableIdentifier &, int, uint32_t);
  int close(void);
  int doStartTableScan(bool scan=1);
  int rnd_next(unsigned char *);
  int rnd_pos(unsigned char * , unsigned char *);
  void position(const unsigned char *);
  int info(uint);
  int doEndTableScan();
  int doInsertRecord(unsigned char * buf);
  int doUpdateRecord(const unsigned char *, unsigned char *);
  int doDeleteRecord(const unsigned char *);

  virtual void get_auto_increment(uint64_t offset, uint64_t increment,
                                  uint64_t nb_desired_values,
                                  uint64_t *first_value,
                                  uint64_t *nb_reserved_values) { (void)offset; (void)increment; (void)nb_desired_values; (void)first_value; (void)nb_reserved_values; }
  FilesystemTableShare *get_share(const char *table_name);
  void free_share();
  void critical_section_enter();
  void critical_section_exit();
private:
  void recordToString(std::string& output);
  void addSlot();
  int openUpdateFile();
  int find_current_row(unsigned char *buf);
};

#endif /* PLUGIN_FILESYSTEM_ENGINE_FILESYSTEM_ENGINE_H */
