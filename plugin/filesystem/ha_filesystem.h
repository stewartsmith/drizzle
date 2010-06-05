/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 ziminq
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PLUGIN_FILESYSTEM_HA_FILESYSTEM_H
#define PLUGIN_FILESYSTEM_HA_FILESYSTEM_H

#include <drizzled/cursor.h>
#include <drizzled/thr_lock.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
using namespace std;

class FilesystemShare
{
  FilesystemShare();
  FilesystemShare(const FilesystemShare &);
  FilesystemShare& operator=(const FilesystemShare &);
public:
  explicit FilesystemShare(const std::string name);
  ~FilesystemShare();

  uint32_t use_count;
  const std::string table_name;
  pthread_mutex_t mutex;
  drizzled::THR_LOCK lock;
};

class ha_filesystem : public drizzled::Cursor
{
  drizzled::THR_LOCK_DATA lock;      /* MySQL lock */
  FilesystemShare *share;       /* Shared lock info */
  std::ifstream fd;
  std::string real_file_name;
  std::string sep;

public:
  ha_filesystem(drizzled::plugin::StorageEngine &engine, drizzled::TableShare &table_arg);
  ~ha_filesystem()
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

  int open(const char *name, int mode, uint32_t open_options);
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
                                  uint64_t *nb_reserved_values) { (void)offset; (void)increment; (void)nb_desired_values; (void)first_value; (void)nb_reserved_values; };
  FilesystemShare *get_share(const char *table_name);
};

#endif /* PLUGIN_FILESYSTEM_HA_FILESYSTEM_H */
