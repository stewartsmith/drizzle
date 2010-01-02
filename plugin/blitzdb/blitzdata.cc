/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Toru Maesaka
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

#include "ha_blitz.h"

#define BLITZ_TC_EXTRA_MMAP_SIZE (1024 * 1024 * 256)
#define BLITZ_TC_BUCKET_NUM 1000000

bool BlitzData::startup(const char *table_name) {
  data_table = open_table(table_name, BLITZ_DATA_EXT, HDBOWRITER);

  if (data_table == NULL)
    return false;

  system_table = open_table(table_name, BLITZ_SYSTEM_EXT, HDBOWRITER);

  if (system_table == NULL) {
    close_table(data_table);
    return false;
  }

  tc_meta_buffer = tchdbopaque(data_table);
  current_hidden_row_id = (uint64_t)uint8korr(tc_meta_buffer);
  return true;
}

bool BlitzData::shutdown() {
  /* Copy the latest autogenerated ID back to TC's metadata buffer.
     This data will be sync'd by TC. */
  int8store(tc_meta_buffer, current_hidden_row_id);

  if (!close_table(data_table))
    return false;

  if (!close_table(system_table))
    return false;

  return true;
}

/* Similar to UNIX touch(1) but generates a TCHDB file. */
int BlitzData::create_table(const char *table_path, const char *ext) {
  TCHDB *table;
  int mode = (HDBOWRITER | HDBOCREAT);

  if ((table = open_table(table_path, ext, mode)) == NULL)
    return HA_ERR_CRASHED_ON_USAGE;

  if (!close_table(table))
    return HA_ERR_CRASHED_ON_USAGE;

  return 0;
}

TCHDB *BlitzData::open_table(const char *path, const char *ext, int mode) {
  TCHDB *table;
  char name_buffer[FN_REFLEN];

  if ((table = tchdbnew()) == NULL) {
    return NULL;
  }

  if (!tchdbsetmutex(table)) {
    tchdbdel(table);
    return NULL;
  }

  /* Allow the data table to use more resource than default. */
  if (strcmp(ext, BLITZ_DATA_EXT) == 0) {
    if (!tchdbtune(table, BLITZ_TC_BUCKET_NUM, -1, -1, 0)) {
      tchdbdel(table);
      return NULL;
    }

    if (!tchdbsetxmsiz(table, BLITZ_TC_EXTRA_MMAP_SIZE)) {
      tchdbdel(table);
      return NULL;
    }
  }

  snprintf(name_buffer, FN_REFLEN, "%s%s", path, ext);

  if (!tchdbopen(table, name_buffer, mode)) {
    tchdbdel(table);
    return NULL;
  }

  return table;
}

bool BlitzData::rename_table(const char *from, const char *to) {
  char from_buf[FN_REFLEN];
  char to_buf[FN_REFLEN];

  snprintf(from_buf, FN_REFLEN, "%s%s", from, BLITZ_DATA_EXT);
  snprintf(to_buf, FN_REFLEN, "%s%s", to, BLITZ_DATA_EXT);

  if (rename(from_buf, to_buf) != 0)
    return false;

  snprintf(from_buf, FN_REFLEN, "%s%s", from, BLITZ_SYSTEM_EXT);
  snprintf(to_buf, FN_REFLEN, "%s%s", to, BLITZ_SYSTEM_EXT);

  if (rename(from_buf, to_buf) != 0)
    return false;

  return true;
}

bool BlitzData::close_table(TCHDB *table) {
  assert(table);

  if (!tchdbclose(table)) {
    tchdbdel(table);
    return false ;
  }

  tchdbdel(table);
  return true;
}

bool BlitzData::write_table_definition(TCHDB *table,
                                       drizzled::message::Table &proto) {
  assert(table);
  string serialized_proto;

  proto.SerializeToString(&serialized_proto);

  if (!tchdbput(table, BLITZ_TABLE_PROTO_KEY.c_str(),
                BLITZ_TABLE_PROTO_KEY.length(), serialized_proto.c_str(),
                serialized_proto.length())) {
    return false;
  }

  if (proto.options().has_comment()) {
    if (!tchdbput(table, BLITZ_TABLE_PROTO_COMMENT_KEY.c_str(),
                  BLITZ_TABLE_PROTO_COMMENT_KEY.length(),
                  proto.options().comment().c_str(),
                  proto.options().comment().length())) {
      return false;
    }
  }
  return true;
}

uint64_t BlitzData::nrecords(void) {
  return tchdbrnum(data_table);
}

uint64_t BlitzData::table_size(void) {
  return tchdbfsiz(data_table);
}

char *BlitzData::get_row(const char *key, const size_t klen, int *vlen) {
  return (char *)tchdbget(data_table, key, klen, vlen);
}

/* Fastest way to fetch both key and value from TCHDB since it only
   involves one allocation. That is, both key and value are living
   on the same block of memory. The return value is a pointer to the
   next key. Technically it is a pointer to the region of memory that
   holds both key and value. */
char *BlitzData::next_key_and_row(const char *key, const size_t klen,
                                  int *next_key_len, const char **value,
                                  int *value_len) {
  return tchdbgetnext3(data_table, key, klen, next_key_len,
                       value, value_len);
}

char *BlitzData::first_row(int *row_len) {
  return (char *)tchdbgetnext(data_table, NULL, 0, row_len);
}

uint64_t BlitzData::next_hidden_row_id(void) {
  /* current_hidden_row_id is an atomic type */
  uint64_t rv = current_hidden_row_id++;
  return rv;
}

int BlitzData::write_row(const char *key, const size_t klen,
                         const unsigned char *row, const size_t rlen) {
  bool success = tchdbput(data_table, key, klen, row, rlen);
  return (success) ? 0 : 1; 
}

int BlitzData::write_unique_row(const char *key, const size_t klen,
                                const unsigned char *row, const size_t rlen) {
  int rv = 0;

  if (!tchdbputkeep(data_table, key, klen, row, rlen)) {
    if (tchdbecode(data_table) == TCEKEEP) {
      my_errno = HA_ERR_FOUND_DUPP_KEY;
      rv = HA_ERR_FOUND_DUPP_KEY;
    }
  }
  return rv;
}

bool BlitzData::delete_row(const char *key, const size_t klen) {
  return tchdbout(data_table, key, klen);
}

bool BlitzData::delete_all_rows() {
  return tchdbvanish(data_table);
}
