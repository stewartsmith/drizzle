/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

namespace drizzled {

/**
  CacheField and JoinCache is used on full join to cache records in outer
  table
*/
class CacheField {
  /*
    Where source data is located (i.e. this points to somewhere in
    tableX->getInsertRecord())
  */
public:
  unsigned char *str;
  uint32_t length; /* Length of data at *str, in bytes */
  uint32_t blob_length; /* Valid IFF blob_field != 0 */
  Field_blob *blob_field;
  bool strip; /* true <=> Strip endspaces ?? */
  Table *get_rowid; /* _ != NULL <=> */

  CacheField():
    str(NULL),
    length(0),
    blob_length(0),
    blob_field(NULL),
    strip(false),
    get_rowid(NULL)
  {}

};

class JoinCache
{
public:
  unsigned char *buff;
  unsigned char *pos;    /* Start of free space in the buffer */
  unsigned char *end;
  uint32_t records;  /* # of row cominations currently stored in the cache */
  uint32_t record_nr;
  uint32_t ptr_record;
  /*
    Number of fields (i.e. cache_field objects). Those correspond to table
    columns, and there are also special fields for
     - table's column null bits
     - table's null-complementation byte
     - [new] table's rowid.
  */
  uint32_t fields;
  uint32_t length;
  uint32_t blobs;
  CacheField *field;
  CacheField **blob_ptr;
  optimizer::SqlSelect *select;

  JoinCache():
    buff(NULL),
    pos(NULL),
    end(NULL),
    records(0),
    record_nr(0),
    ptr_record(0),
    fields(0),
    length(0),
    blobs(0),
    field(NULL),
    blob_ptr(NULL),
    select(NULL)
  {}

  void reset_cache_read();
  void reset_cache_write();
  bool store_record_in_cache();
};

int join_init_cache(Session *session, JoinTable *tables, uint32_t table_count);

} /* namespace drizzled */

